import { randomUUID } from "node:crypto";
import { EventEmitter } from "node:events";
import { createReadStream, existsSync, realpathSync } from "node:fs";
import { type FileHandle, lstat, open, readFile, readdir, realpath, rm, rmdir, stat, statfs } from "node:fs/promises";
import type { ServerResponse } from "node:http";
import os from "node:os";
import path from "node:path";
import multipart from "@fastify/multipart";
import fastifyStatic from "@fastify/static";
import Fastify, { type FastifyInstance, type FastifyReply } from "fastify";
import { ConfigConflictError, ConfigShapeError, configMtime, randomSeed, readConfig, toConfig, writeConfig } from "./config.js";
import { type DriveDeps, type DriveInfo, type ScanResult, dotClean, initRoot, inspectDrive, scanSource } from "./drive.js";
import { isTooShort, MIN_DURATION_MS, probe, remux } from "./ffmpeg.js";
import { ContainmentError, assertContained, contained } from "./contain.js";
import { makeIcon, NotImageError } from "./icons.js";
import { isAllowedSource, listImports, resolveImportsDir } from "./imports.js";
import { type BuildResult, OFF_LIBRARY_FILE, type SkippedFile, buildLibrary, readLibrary, writeLibrary, writeOffLibrary } from "./library.js";
import { readOff } from "./off.js";
import { episodeFileName, fatSafe, fileStem, isVideoFile, uniqueName } from "./names.js";
import { pickFolderNative } from "./picker.js";
import { ConversionQueue } from "./queue.js";
import { DRIVE_PROFILE, SERVER_PROFILE, type VideoProfile } from "./videoSettings.js";
import { SchedUsageError, STUDIO_DIR, lineup, newErrors, validateCandidate, validateRoot } from "./sched.js";
import { type EditAction, EditRefused, applyPlan, planEdit } from "./libraryEdit.js";
import { uploadsDir } from "./serverSettings.js";
import { clearedSessionCookie, sessionToken, Sessions } from "./session.js";
import { type ShowMeta, ShowMetaConflictError, ShowMetaDamagedError, readShowMeta, showMetaMtime, writeShowMeta } from "./showMeta.js";
import { atomicWriteFile, needsConversion, readSources, upsertSource, writeSources } from "./sources.js";
import type { Config, Issue, Job, Library, SourceRecord } from "./types.js";
import { shrinkCandidates } from "./shrink.js";
import { UPLOAD_LIMIT, convertedUpload, discardUploads, isUploadPath, saveUpload, sweepUploads, uploadName, uploadSource } from "./uploads.js";
import { parseRange, registerWiiApi } from "./wiiApi.js";

export interface ServerModeOptions {
  library: string;
  password: string;
  imports: string;
  name: string;
}

export interface AppOptions {
  schedBin: string;
  queueFactory?: (root: string, profile: VideoProfile) => ConversionQueue;
  settingsPath?: string;
  inspectDeps?: DriveDeps;
  probeFn?: typeof probe;
  remuxFn?: typeof remux;
  pickFolderFn?: (prompt: string) => Promise<string | null>;
  rebuildDelayMs?: number;
  webDir?: string;
  logger?: boolean;
  port?: number;
  server?: ServerModeOptions;
  uploadLimit?: number;
  now?: () => number;
}

type Aspect = "16:9" | "4:3";

interface Rejected {
  source: string;
  reason: string;
}

interface UploadRejected {
  name: string;
  reason: string;
}

interface Settings {
  drive?: string;
}

const DEFAULT_SETTINGS = path.join(os.homedir(), ".cable-twii-studio.json");
const DEFAULT_WEB_DIR = path.join(STUDIO_DIR, "dist", "web");
const REBUILD_DELAY_MS = 2000;
const HEARTBEAT_MS = 15000;
const ICON_LIMIT = 10 * 1024 * 1024;
const BODY_LIMIT = 16 * 1024 * 1024;
const MAX_U16 = 65535;
const CHANNEL_QUERY = /^[1-9][0-9]*$/;
const DATE_QUERY = /^\d{4}-\d{2}-\d{2}$/;
const RESERVED = new Set(["ads"]);
const ALREADY_CONVERTED = "already on the drive";
const ALREADY_UPLOADED = "already on the server";
const OUTSIDE_SOURCES = "Videos must come from the imports folder or an upload.";
const BAD_REPLACES = "replaces must be an episode of this show.";
const UPLOAD_FILES = 50;
const DEFAULT_PORT = 4747;
const DEV_HOST = "localhost:5173";
const API_HEADER = "x-cable-twii";
const SAFE_METHODS = new Set(["GET", "HEAD"]);
const SESSION_GUARDED = /^\/(?:api|media)\//;
const SESSION_OPEN = new Set(["/api/mode", "/api/login", "/api/logout"]);
const SOURCE_TYPES: Record<string, string> = {
  ".mp4": "video/mp4",
  ".m4v": "video/mp4",
  ".mkv": "video/x-matroska",
  ".webm": "video/webm",
  ".mov": "video/quicktime",
};

class HttpError extends Error {
  constructor(readonly statusCode: number, message: string, readonly issues?: Issue[]) {
    super(message);
    this.name = "HttpError";
  }
}

function bad(message: string): HttpError {
  return new HttpError(400, message);
}

function objectBody(body: unknown): Record<string, unknown> {
  if (typeof body !== "object" || body === null || Array.isArray(body)) throw bad("Expected a JSON object.");
  return body as Record<string, unknown>;
}

function stringField(o: Record<string, unknown>, key: string): string {
  const v = o[key];
  if (typeof v !== "string") throw bad(`${key} must be a string.`);
  return v;
}

function mtimeField(o: Record<string, unknown>): number {
  const v = o.mtimeMs;
  if (typeof v !== "number" || !Number.isSafeInteger(v) || v < 0) throw bad("mtimeMs must be a whole number.");
  return v;
}

function aspectField(o: Record<string, unknown>): Aspect {
  const v = o.aspect;
  if (v !== "16:9" && v !== "4:3") throw bad('aspect must be "16:9" or "4:3".');
  return v;
}

function sourceField(o: Record<string, unknown>): string {
  const v = stringField(o, "source");
  if (!path.isAbsolute(v)) throw bad("source must be an absolute path.");
  return v;
}

function nullableInt(o: Record<string, unknown>, key: string, min: number): number | null {
  const v = o[key];
  if (v === null || v === undefined) return null;
  if (typeof v !== "number" || !Number.isInteger(v) || v < min || v > MAX_U16) throw bad(`${key} must be a whole number from ${min} to ${MAX_U16}, or null.`);
  return v;
}

function folderName(raw: unknown, label: string): string {
  if (typeof raw !== "string") throw bad(`${label} is required.`);
  const trimmed = raw.trim();
  if (raw.includes("/") || raw.includes("\\") || trimmed === "." || trimmed === "..") throw bad(`${label} must be a single folder name.`);
  const safe = fatSafe(raw);
  if (safe === "" || safe.startsWith(".")) throw bad(`${label} is not a usable folder name.`);
  if (RESERVED.has(safe.toLowerCase())) throw bad(`${label} cannot be "${safe}".`);
  return safe;
}

function inside(root: string, ...parts: string[]): string {
  const full = path.resolve(root, ...parts);
  const rel = path.relative(root, full);
  if (rel === "" || rel === ".." || rel.startsWith(`..${path.sep}`) || path.isAbsolute(rel)) throw bad("That path is outside the cable-twii folder.");
  return full;
}

async function inImports<T>(task: () => Promise<T>): Promise<T> {
  try {
    return await task();
  } catch (err) {
    const code = (err as NodeJS.ErrnoException).code;
    if (code === "ENOENT" || code === "ENOTDIR") throw new HttpError(404, "That folder does not exist.");
    throw err;
  }
}

async function editableShowMeta(showDir: string): Promise<ShowMeta> {
  try {
    return await readShowMeta(showDir);
  } catch (err) {
    if (err instanceof ShowMetaDamagedError) return { episodes: {} };
    throw err;
  }
}

function uploadPlan(sent: string, kept: ReadonlyMap<string, string>): { name: string } | { reason: string } {
  if (!isVideoFile(sent)) return { reason: "not a video" };
  const name = uploadName(sent);
  if (name === null) return { reason: "not a usable file name" };
  if (kept.has(name.toLowerCase())) return { reason: "already in this upload" };
  return { name };
}

function tooShortReason(durationMs: number): string {
  return `too short (${durationMs} ms, needs at least ${MIN_DURATION_MS} ms)`;
}

function unreadableReason(message: string): string {
  return `could not read the video: ${message}`;
}

function scanProblem(entry: ScanResult): string | null {
  if (entry.error !== undefined) return unreadableReason(entry.error);
  return entry.tooShort ? tooShortReason(entry.durationMs) : null;
}

function two(n: number): string {
  return String(n).padStart(2, "0");
}

function errorText(err: unknown): string {
  return err instanceof Error ? err.message : String(err);
}

async function pathExists(p: string): Promise<boolean> {
  try {
    await stat(p);
    return true;
  } catch {
    return false;
  }
}

async function alreadyConverted(root: string, records: SourceRecord[], source: string): Promise<boolean> {
  for (const rec of records) {
    if (rec.source !== source) continue;
    try {
      if (!(await lstat(path.join(root, ...rec.target.split("/")))).isFile()) continue;
      if (!needsConversion(rec, await stat(source), true)) return true;
    } catch {
      continue;
    }
  }
  return false;
}

async function markAdded(root: string, records: SourceRecord[], files: ScanResult[]): Promise<ScanResult[]> {
  for (const file of files) {
    if (await alreadyConverted(root, records, file.source)) file.added = true;
  }
  return files;
}

async function openPreviewFile(dirs: ReadonlySet<string>, file: string): Promise<{ handle: FileHandle; size: number; type: string } | null> {
  if (!path.isAbsolute(file) || !isVideoFile(path.basename(file))) return null;
  let handle: FileHandle | undefined;
  try {
    const real = await realpath(file);
    if (!isVideoFile(path.basename(real)) || ![...dirs].some((dir) => contained(dir, real))) return null;
    if (!(await stat(real)).isFile()) return null;
    handle = await open(real, "r");
    const info = await handle.stat();
    if (!info.isFile()) {
      await handle.close();
      return null;
    }
    return { handle, size: info.size, type: SOURCE_TYPES[path.extname(real).toLowerCase()] ?? "application/octet-stream" };
  } catch {
    await handle?.close().catch(() => undefined);
    return null;
  }
}

async function isShowEpisode(root: string, showRel: string, rel: string): Promise<boolean> {
  if (!rel.startsWith(`${showRel}/`) || !rel.toLowerCase().endsWith(".mpg") || path.posix.normalize(rel) !== rel || rel.split("/").includes("..")) return false;
  const file = path.join(root, ...rel.split("/"));
  try {
    assertContained(root, file);
    return (await lstat(file)).isFile();
  } catch {
    return false;
  }
}

async function libraryFile(root: string, rel: unknown, extension: string): Promise<string | null> {
  if (typeof rel !== "string" || rel === "" || !rel.toLowerCase().endsWith(extension)) return null;
  try {
    const realRoot = await realpath(root);
    const real = await realpath(path.resolve(realRoot, rel));
    if (!contained(realRoot, real) || !real.toLowerCase().endsWith(extension)) return null;
    return (await stat(real)).isFile() ? real : null;
  } catch {
    return null;
  }
}

function mergeLibraries(library: Library, off: Library | null): Library {
  if (off === null) return library;
  const shows = new Map(library.show.map((s) => [`${s.channel}\0${s.folder}`, { ...s, episode: [...s.episode] }]));
  for (const s of off.show) {
    const key = `${s.channel}\0${s.folder}`;
    const found = shows.get(key);
    if (found) found.episode.push(...s.episode);
    else shows.set(key, { ...s, episode: [...s.episode] });
  }
  return { ...library, ad: [...library.ad, ...off.ad], show: [...shows.values()] };
}

async function topFiles(dir: string): Promise<string[]> {
  try {
    return (await readdir(dir, { withFileTypes: true })).filter((e) => e.isFile()).map((e) => e.name);
  } catch {
    return [];
  }
}

async function isDirectory(p: string): Promise<boolean> {
  try {
    return (await stat(p)).isDirectory();
  } catch {
    return false;
  }
}

async function listNames(dir: string): Promise<string[]> {
  try {
    return await readdir(dir);
  } catch {
    return [];
  }
}

function originHost(origin: string): string | null {
  try {
    const url = new URL(origin);
    return url.protocol === "http:" ? url.host.toLowerCase() : null;
  } catch {
    return null;
  }
}

function sendNull(reply: FastifyReply): FastifyReply {
  return reply.type("application/json; charset=utf-8").send("null");
}

async function freeBytesIn(dir: string): Promise<number> {
  try {
    const s = await statfs(dir);
    return Number(s.bavail) * Number(s.bsize);
  } catch {
    return 0;
  }
}

async function serverDrive(library: string): Promise<DriveInfo> {
  return {
    path: library,
    rootPath: path.join(library, "cable-twii"),
    isVolumeRoot: false,
    fsType: null,
    freeBytes: await freeBytesIn(library),
    hasRoot: true,
    hasWbfs: false,
    usable: true,
  };
}

export function buildApp(opts: AppOptions): FastifyInstance {
  const app = Fastify({ logger: opts.logger ?? false, bodyLimit: BODY_LIMIT });
  const settingsPath = opts.settingsPath ?? DEFAULT_SETTINGS;
  const deps = opts.inspectDeps ?? {};
  const probeFn = opts.probeFn ?? probe;
  const remuxFn = opts.remuxFn ?? remux;
  const pick = opts.pickFolderFn ?? ((prompt: string) => pickFolderNative(prompt));
  const rebuildDelay = opts.rebuildDelayMs ?? REBUILD_DELAY_MS;
  const webDir = opts.webDir ?? DEFAULT_WEB_DIR;
  const bin = opts.schedBin;
  const server = opts.server && { ...opts.server, library: realpathSync.native(opts.server.library), imports: realpathSync.native(opts.server.imports) };
  const sessions = server && new Sessions(server.password, undefined, opts.now);
  const uploads = server && uploadsDir(server.library);
  const uploadLimit = opts.uploadLimit ?? UPLOAD_LIMIT;

  const jobEvents = new EventEmitter();
  jobEvents.setMaxListeners(0);
  const queues = new Map<string, ConversionQueue>();
  const previewDirs = new Set<string>();
  const lastBuild = new Map<string, { at: number; skipped: SkippedFile[]; warnings: string[] }>();
  const rebuildTimers = new Map<string, NodeJS.Timeout>();
  const rebuildChains = new Map<string, Promise<unknown>>();
  const streams = new Set<{ raw: ServerResponse; cleanup: () => void }>();
  const retiring = new Set<Promise<void>>();
  let configChain: Promise<unknown> = Promise.resolve();

  async function readSettings(): Promise<Settings> {
    try {
      const doc = JSON.parse(await readFile(settingsPath, "utf8")) as unknown;
      if (typeof doc === "object" && doc !== null && typeof (doc as Settings).drive === "string") return { drive: (doc as Settings).drive };
    } catch {
      return {};
    }
    return {};
  }

  async function saveDrive(drivePath: string): Promise<void> {
    await atomicWriteFile(settingsPath, `${JSON.stringify({ drive: drivePath }, null, 2)}\n`);
  }

  async function currentDrive(): Promise<DriveInfo | null> {
    if (server) return serverDrive(server.library);
    const { drive } = await readSettings();
    if (drive === undefined) return null;
    return inspectDrive(drive, deps);
  }

  async function requireDrive(): Promise<DriveInfo> {
    const info = await currentDrive();
    if (!info) throw new HttpError(409, "Choose a drive first.");
    return info;
  }

  async function requireRoot(): Promise<string> {
    return checkedRoot(await requireDrive());
  }

  async function checkedRoot(info: DriveInfo): Promise<string> {
    if (!info.usable) throw new HttpError(409, info.reason ?? "This drive cannot be used.");
    if (!info.hasRoot) throw new HttpError(409, "This drive has no cable-twii folder yet. Set it up first.");
    let real: string;
    try {
      real = await realpath(info.rootPath);
    } catch {
      throw new HttpError(409, "The cable-twii folder is missing.");
    }
    if (real !== info.rootPath) throw new HttpError(409, "cable-twii is not a normal folder.");
    queues.get(info.rootPath)?.resume();
    return info.rootPath;
  }

  function rebuildNow(root: string): Promise<BuildResult> {
    const previous = rebuildChains.get(root) ?? Promise.resolve();
    const run = async (): Promise<BuildResult> => {
      const result = await buildLibrary(root, await readSources(root), probeFn, await readOff(root));
      await writeLibrary(root, result.library);
      await writeOffLibrary(root, result.off);
      lastBuild.set(root, { at: Date.now(), skipped: result.skipped, warnings: result.warnings });
      return result;
    };
    const next = previous.then(run, run);
    rebuildChains.set(root, next.catch(() => undefined));
    return next;
  }

  function scheduleRebuild(root: string): void {
    clearTimeout(rebuildTimers.get(root));
    rebuildTimers.set(
      root,
      setTimeout(() => {
        rebuildTimers.delete(root);
        rebuildNow(root).catch((err) => app.log.error(err));
      }, rebuildDelay),
    );
  }

  async function flushRebuild(root: string): Promise<void> {
    const timer = rebuildTimers.get(root);
    if (timer !== undefined) {
      clearTimeout(timer);
      rebuildTimers.delete(root);
      await rebuildNow(root);
    }
    await rebuildChains.get(root);
  }

  function retireUpload(root: string, rec: SourceRecord): void {
    if (!uploads || !isUploadPath(uploads, rec.source)) return;
    const stillNeeded = queues.get(root)?.list().some((j) => j.source === rec.source && (j.state === "queued" || j.state === "running"));
    const task: Promise<void> = (async () => {
      assertContained(uploads, rec.source);
      await upsertSource(root, { ...rec, source: uploadSource(path.basename(rec.source)) });
      if (stillNeeded) return;
      await rm(rec.source, { force: true });
      await rmdir(path.dirname(rec.source)).catch(() => undefined);
    })()
      .catch((err: unknown) => app.log.warn(err))
      .finally(() => retiring.delete(task));
    retiring.add(task);
  }

  function queueFor(root: string): ConversionQueue {
    let queue = queues.get(root);
    if (queue) return queue;
    const profile = server ? SERVER_PROFILE : DRIVE_PROFILE;
    queue = opts.queueFactory ? opts.queueFactory(root, profile) : new ConversionQueue(root, { profile });
    queue.on("change", (job: Job) => jobEvents.emit("change", job));
    queue.on("converted", (rec: SourceRecord) => {
      scheduleRebuild(root);
      retireUpload(root, rec);
    });
    queue.on("error", (err: unknown) => app.log.warn(err));
    queue.start();
    queues.set(root, queue);
    return queue;
  }

  function withConfigLock<T>(task: () => Promise<T>): Promise<T> {
    const next = configChain.then(task, task);
    configChain = next.catch(() => undefined);
    return next;
  }

  async function saveConfig(root: string, cfg: Config, mtimeMs: number): Promise<{ mtimeMs: number; issues: Issue[] }> {
    if ((await configMtime(root)) !== Math.trunc(mtimeMs)) throw new ConfigConflictError();
    await flushRebuild(root).catch((err: unknown) => app.log.warn(err));
    const result = await validateCandidate(root, cfg, bin);
    if (!result.ok || result.issues.some((i) => i.level === "error")) throw new HttpError(422, "invalid", result.issues);
    const saved = await writeConfig(root, cfg, mtimeMs);
    return { mtimeMs: saved, issues: result.issues.filter((i) => i.level === "warning") };
  }

  async function checkSource(source: string): Promise<string | null> {
    try {
      const { durationMs } = await probeFn(source);
      return isTooShort(durationMs) ? tooShortReason(durationMs) : null;
    } catch (err) {
      return unreadableReason(errorText(err));
    }
  }

  async function requireImportable(sources: string[]): Promise<void> {
    if (!server || !uploads) return;
    for (const source of sources) {
      if (!(await isAllowedSource(server.imports, uploads, source))) throw bad(OUTSIDE_SOURCES);
    }
  }

  async function sourceRecords(root: string): Promise<SourceRecord[]> {
    try {
      return await readSources(root);
    } catch (err) {
      app.log.warn(err);
      return [];
    }
  }

  async function earlierConversion(root: string, records: SourceRecord[], source: string, dirRel: string): Promise<{ current: boolean; target: string } | null> {
    const rec = records.find((r) => r.source === source && path.posix.dirname(r.target) === dirRel);
    if (!rec) return null;
    try {
      if (!(await lstat(path.join(root, ...rec.target.split("/")))).isFile()) return null;
      return { current: !needsConversion(rec, await stat(source), true), target: rec.target };
    } catch {
      return null;
    }
  }

  function takenNames(root: string, queue: ConversionQueue) {
    const cache = new Map<string, Set<string>>();
    return async (dirRel: string): Promise<Set<string>> => {
      const hit = cache.get(dirRel);
      if (hit) return hit;
      const names = new Set(await listNames(path.join(root, ...dirRel.split("/"))));
      for (const job of queue.list()) {
        if (job.state !== "queued" && job.state !== "running") continue;
        if (path.posix.dirname(job.target) === dirRel) names.add(path.posix.basename(job.target));
      }
      cache.set(dirRel, names);
      return names;
    };
  }

  async function resumeIfReady(info: DriveInfo): Promise<void> {
    if (!queues.has(info.rootPath)) return;
    try {
      await checkedRoot(info);
    } catch (err) {
      if (!(err instanceof HttpError)) throw err;
    }
  }

  async function rememberDrive(input: string): Promise<DriveInfo> {
    const info = await inspectDrive(input, deps);
    if (await pathExists(info.path)) await saveDrive(info.path);
    await resumeIfReady(info);
    return info;
  }

  app.setErrorHandler((error: unknown, request, reply) => {
    if (error instanceof ConfigConflictError) return reply.code(409).send({ error: error.message });
    if (error instanceof ShowMetaConflictError) return reply.code(409).send({ error: error.message });
    if (error instanceof ConfigShapeError) return reply.code(422).send({ error: error.message });
    if (error instanceof ContainmentError) return reply.code(400).send({ error: error.message });
    if (error instanceof NotImageError) return reply.code(400).send({ error: error.message });
    if (error instanceof SchedUsageError) return reply.code(400).send({ error: error.message });
    if (error instanceof HttpError) {
      const body: { error: string; issues?: Issue[] } = { error: error.message };
      if (error.issues) body.issues = error.issues;
      return reply.code(error.statusCode).send(body);
    }
    const status = (error as { statusCode?: unknown }).statusCode;
    const code = typeof status === "number" && status >= 400 && status < 600 ? status : 500;
    if (code >= 500) request.log.error(error);
    return reply.code(code).send({ error: errorText(error) });
  });

  app.setNotFoundHandler((_request, reply) => reply.code(404).send({ error: "Not found" }));

  function allowedHosts(): Set<string> {
    const address = app.server.address();
    const port = typeof address === "object" && address !== null ? address.port : opts.port ?? DEFAULT_PORT;
    return new Set([`127.0.0.1:${port}`, `localhost:${port}`, DEV_HOST]);
  }

  app.addHook("onRequest", async (request, reply) => {
    const host = request.headers.host?.toLowerCase();
    if (!server && (host === undefined || !allowedHosts().has(host))) return reply.code(403).send({ error: "Open cable-twii Studio at http://127.0.0.1:4747/." });
    if (SAFE_METHODS.has(request.method)) return;
    if (request.headers[API_HEADER] !== "1") return reply.code(403).send({ error: "This request did not come from cable-twii Studio." });
    const origin = request.headers.origin;
    if (origin === undefined) return;
    const from = originHost(origin);
    const sameSite = server ? from === host : from !== null && allowedHosts().has(from);
    if (!sameSite) return reply.code(403).send({ error: "This request came from another site." });
  });

  if (sessions) {
    app.addHook("onRequest", async (request, reply) => {
      const route = request.routeOptions.url;
      if (route === undefined || !SESSION_GUARDED.test(route) || SESSION_OPEN.has(route)) return;
      if (!sessions.isValid(sessionToken(request.headers.cookie))) return reply.code(401).send({ error: "Sign in first." });
    });
  }

  app.register(multipart, { limits: { fileSize: ICON_LIMIT, files: 1, fields: 10 } });

  if (existsSync(webDir)) app.register(fastifyStatic, { root: webDir, prefix: "/" });

  app.addHook("preClose", async () => {
    for (const stream of streams) {
      stream.cleanup();
      stream.raw.end();
    }
    streams.clear();
  });

  if (server) {
    app.addHook("onReady", async () => {
      try {
        queueFor(await requireRoot());
      } catch (err) {
        app.log.warn(err);
      }
    });
  }

  if (uploads) {
    app.addHook("onListen", async () => {
      const jobs = queueFor(await requireRoot()).list();
      const queued = new Set(jobs.filter((j) => j.state === "queued" || j.state === "running").map((j) => path.resolve(j.source)));
      await sweepUploads(uploads, queued, (opts.now ?? Date.now)());
    });
  }

  app.addHook("onClose", async () => {
    await Promise.allSettled([...queues.values()].map((q) => q.stop()));
    await Promise.allSettled([...retiring]);
    if (!server) {
      for (const timer of rebuildTimers.values()) clearTimeout(timer);
      rebuildTimers.clear();
    }
    if (server) await Promise.all([...rebuildTimers.keys()].map((root) => flushRebuild(root).catch((err: unknown) => app.log.warn(err))));
    await Promise.allSettled([...rebuildChains.values()]);
  });

  app.get("/api/mode", async (request) => ({
    mode: server ? "server" : "drive",
    name: server?.name ?? null,
    signedIn: sessions?.isValid(sessionToken(request.headers.cookie)) ?? true,
  }));

  if (sessions) {
    app.post("/api/login", async (request, reply) => {
      if (sessions.isLocked()) throw new HttpError(429, "Too many tries. Wait a minute and try again.");
      const token = sessions.signIn(stringField(objectBody(request.body), "password"));
      if (token === null) throw new HttpError(401, "Wrong password.");
      reply.header("set-cookie", sessions.cookieFor(token));
      return { ok: true };
    });

    app.post("/api/logout", async (request, reply) => {
      sessions.signOut(sessionToken(request.headers.cookie));
      reply.header("set-cookie", clearedSessionCookie());
      return { ok: true };
    });
  }

  app.get("/api/drive", async (_request, reply) => {
    const info = await currentDrive();
    return info ?? sendNull(reply);
  });

  if (!server) {
    app.post("/api/drive", async (request) => {
      const input = stringField(objectBody(request.body), "path");
      if (input === "" || !path.isAbsolute(input)) throw bad("path must be an absolute path.");
      return rememberDrive(input);
    });

    app.post("/api/drive/pick", async (_request, reply) => {
      const picked = await pick("Choose your cable-twii drive");
      if (picked === null) return sendNull(reply);
      return rememberDrive(picked);
    });

    app.post("/api/drive/init", async () => {
      const info = await requireDrive();
      if (!info.usable) throw bad(info.reason ?? "This drive cannot be used.");
      try {
        await initRoot(info, deps);
      } catch (err) {
        throw bad(errorText(err));
      }
      return inspectDrive(info.path, deps);
    });

    app.post("/api/drive/done", async () => {
      const info = await requireDrive();
      await resumeIfReady(info);
      const pending = (queues.get(info.rootPath)?.list() ?? []).filter((j) => j.state === "queued" || j.state === "running").length;
      if (pending > 0) throw new HttpError(409, `${pending} ${pending === 1 ? "conversion" : "conversions"} still running`);
      if (info.usable && info.hasRoot) await flushRebuild(info.rootPath);
      if (!(info.usable && info.isVolumeRoot && info.fsType !== null)) return { ok: true, skipped: true };
      await dotClean(info.path, deps);
      return { ok: true };
    });

    app.post("/api/import/pick-source", async (_request, reply) => {
      const dir = await pick("Choose a folder with videos");
      if (dir === null) return sendNull(reply);
      const files = await scanSource(dir, probeFn);
      previewDirs.add(await realpath(dir));
      const root = await requireRoot().catch(() => null);
      return { dir, files: root === null ? files : await markAdded(root, await sourceRecords(root), files) };
    });
  }

  if (uploads) {
    app.post("/api/upload", async (request) => {
      const root = await requireRoot();
      const batch = randomUUID();
      const dir = path.join(uploads, batch);
      const records = await sourceRecords(root);
      const kept = new Map<string, string>();
      const rejected: UploadRejected[] = [];
      const files: ScanResult[] = [];
      try {
        for await (const part of request.parts({ limits: { fileSize: uploadLimit, files: UPLOAD_FILES } })) {
          if (part.type !== "file") continue;
          const sent = part.filename;
          const plan = uploadPlan(sent, kept);
          if ("reason" in plan) {
            rejected.push({ name: sent, reason: plan.reason });
            part.file.resume();
            continue;
          }
          const { name } = plan;
          let file: string;
          try {
            file = await saveUpload(uploads, batch, name, part.file);
          } catch (err) {
            // a destroyed part stream ends the whole multipart parse, so only an untouched or finished one can be skipped
            if (part.file.destroyed && !part.file.readableEnded) throw err;
            request.log.warn(err);
            rejected.push({ name: sent, reason: "could not be saved" });
            part.file.resume();
            continue;
          }
          if (part.file.truncated) throw new HttpError(413, `${sent} is larger than the upload limit.`);
          if (await convertedUpload(root, records, name, (await stat(file)).size)) {
            await rm(file);
            rejected.push({ name: sent, reason: ALREADY_UPLOADED });
            continue;
          }
          kept.set(name.toLowerCase(), sent);
        }
        for (const entry of kept.size === 0 ? [] : await scanSource(dir, probeFn)) {
          const problem = scanProblem(entry);
          if (problem === null) {
            files.push(entry);
            continue;
          }
          await rm(entry.source);
          rejected.push({ name: kept.get(path.basename(entry.source).toLowerCase()) ?? path.basename(entry.source), reason: problem });
        }
        if (files.length === 0) await rm(dir, { recursive: true, force: true });
      } catch (err) {
        await rm(dir, { recursive: true, force: true }).catch(() => undefined);
        throw err;
      }
      return { dir, files, rejected };
    });
  }

  if (uploads) {
    app.post("/api/upload/check", async (request) => {
      const files = objectBody(request.body).files;
      if (!Array.isArray(files)) throw bad("files must be a list.");
      const wanted = files.map((raw) => {
        const f = objectBody(raw);
        if (typeof f.name !== "string" || typeof f.size !== "number") throw bad("Each file needs a name and a size.");
        return { name: f.name, size: f.size };
      });
      const root = await requireRoot();
      const records = await sourceRecords(root);
      const added: string[] = [];
      for (const { name, size } of wanted) {
        const stored = uploadName(name);
        if (stored !== null && (await convertedUpload(root, records, stored, size))) added.push(name);
      }
      return { added };
    });

    app.post("/api/upload/discard", async (request) => {
      const body = objectBody(request.body);
      const names = body.names;
      if (!Array.isArray(names) || names.some((n) => typeof n !== "string")) throw bad("names must be a list of strings.");
      const jobs = queueFor(await requireRoot()).list();
      const queued = new Set(jobs.filter((j) => j.state === "queued" || j.state === "running").map((j) => path.resolve(j.source)));
      return discardUploads(uploads, stringField(body, "dir"), names as string[], queued);
    });
  }

  if (server) {
    async function candidates(root: string, queue: ConversionQueue): Promise<string[]> {
      const library = (await readLibrary(root)) ?? (await rebuildNow(root)).library;
      const busy = new Set(queue.list().filter((j) => j.state === "queued" || j.state === "running").map((j) => j.target));
      return shrinkCandidates(root, library, busy);
    }

    app.get("/api/library/shrink", async () => {
      const root = await requireRoot();
      return { candidates: (await candidates(root, queueFor(root))).length };
    });

    app.post("/api/library/shrink", async () => {
      const root = await requireRoot();
      const queue = queueFor(root);
      const known = new Set(queue.list().map((j) => j.id));
      const found = await candidates(root, queue);
      const created = found.map((target) => queue.enqueueShrink(target)).filter((job) => !known.has(job.id));
      return { queued: created.length };
    });

    app.get("/api/imports", async (request) => {
      const dir = (request.query as Record<string, unknown>).dir ?? "";
      if (typeof dir !== "string") throw bad("dir must be a string.");
      return inImports(() => listImports(server.imports, dir));
    });

    app.post("/api/import/scan", async (request) => {
      const rel = stringField(objectBody(request.body), "dir");
      const root = await requireRoot();
      return inImports(async () => {
        const dir = await resolveImportsDir(server.imports, rel);
        const files = await scanSource(dir, probeFn);
        previewDirs.add(await realpath(dir));
        return { dir, files: await markAdded(root, await sourceRecords(root), files) };
      });
    });
  }

  app.get("/api/import/file", async (request, reply) => {
    const raw = (request.query as Record<string, unknown>).path;
    const file = typeof raw === "string" ? await openPreviewFile(previewDirs, raw) : null;
    if (file === null) return reply.code(404).send({ error: "Not found" });
    const { handle, size, type } = file;
    reply.header("accept-ranges", "bytes");
    const range = parseRange(request.headers.range, size);
    if (range === "unsatisfiable") {
      await handle.close();
      return reply.code(416).header("content-range", `bytes */${size}`).send({ error: "Range not satisfiable" });
    }
    reply.type(type);
    if (range === null) return reply.header("content-length", size).send(handle.createReadStream());
    return reply
      .code(206)
      .header("content-range", `bytes ${range.start}-${range.end}/${size}`)
      .header("content-length", range.end - range.start + 1)
      .send(handle.createReadStream(range));
  });

  app.post("/api/import/show", async (request) => {
    const body = objectBody(request.body);
    const channelFolder = folderName(body.channelFolder, "channelFolder");
    const showFolder = folderName(body.showFolder, "showFolder");
    const showName = body.showName === undefined ? "" : stringField(body, "showName");
    if (!Array.isArray(body.episodes)) throw bad("episodes must be an array.");
    const episodes = body.episodes.map((raw) => {
      const e = objectBody(raw);
      return {
        source: sourceField(e),
        season: nullableInt(e, "season", 0),
        episode: nullableInt(e, "episode", 1),
        title: stringField(e, "title"),
        description: e.description === undefined ? "" : stringField(e, "description"),
        aspect: aspectField(e),
        replaces: e.replaces === undefined ? null : stringField(e, "replaces"),
      };
    });
    await requireImportable(episodes.map((e) => e.source));
    const root = await requireRoot();
    for (const ep of episodes) {
      if (ep.replaces !== null && !(await isShowEpisode(root, `${channelFolder}/${showFolder}`, ep.replaces))) throw bad(BAD_REPLACES);
    }
    const queue = queueFor(root);
    const taken = takenNames(root, queue);
    const jobs: Job[] = [];
    const rejected: Rejected[] = [];
    const descriptions: Record<string, string> = {};
    const showRel = `${channelFolder}/${showFolder}`;
    assertContained(root, inside(root, channelFolder, showFolder));
    const records = await sourceRecords(root);
    for (const ep of episodes) {
      const seasoned = ep.season !== null && ep.season >= 1;
      const dirRel = ep.replaces !== null ? path.posix.dirname(ep.replaces) : seasoned ? `${showRel}/Season ${ep.season}` : showRel;
      const earlier = await earlierConversion(root, records, ep.source, dirRel);
      if (earlier?.current) {
        rejected.push({ source: ep.source, reason: ALREADY_CONVERTED });
        continue;
      }
      const problem = await checkSource(ep.source);
      if (problem !== null) {
        rejected.push({ source: ep.source, reason: problem });
        continue;
      }
      const names = await taken(dirRel);
      const name = ep.replaces !== null ? path.posix.basename(ep.replaces) : earlier ? path.posix.basename(earlier.target) : uniqueName(episodeFileName({ season: seasoned ? ep.season : null, episode: ep.episode, title: ep.title }), names);
      names.add(name);
      jobs.push(queue.enqueue(ep.source, `${dirRel}/${name}`, ep.aspect));
      if (ep.description === "") continue;
      if (!seasoned) descriptions[name.slice(0, -".mpg".length)] = ep.description;
      else if (ep.episode !== null) descriptions[`S${two(ep.season!)}E${two(ep.episode)}`] ??= ep.description;
    }
    if (jobs.length > 0) {
      const showDir = inside(root, channelFolder, showFolder);
      await withConfigLock(async () => {
        const meta = await readShowMeta(showDir);
        const name = showName.trim() !== "" ? showName.trim() : meta.name;
        await writeShowMeta(root, showDir, { ...(name !== undefined ? { name } : {}), episodes: { ...meta.episodes, ...descriptions } });
      });
      scheduleRebuild(root);
    }
    return { jobs, rejected };
  });

  app.post("/api/import/ads", async (request) => {
    const body = objectBody(request.body);
    const rawChannel = body.channelFolder;
    const channelFolder = rawChannel === "" || rawChannel === undefined ? "" : folderName(rawChannel, "channelFolder");
    if (!Array.isArray(body.files)) throw bad("files must be an array.");
    const files = body.files.map((raw) => {
      const f = objectBody(raw);
      return { source: sourceField(f), aspect: aspectField(f) };
    });
    await requireImportable(files.map((f) => f.source));
    const root = await requireRoot();
    const queue = queueFor(root);
    const dirRel = channelFolder === "" ? "ads" : `${channelFolder}/ads`;
    const names = await takenNames(root, queue)(dirRel);
    const records = await sourceRecords(root);
    const jobs: Job[] = [];
    const rejected: Rejected[] = [];
    for (const file of files) {
      const earlier = await earlierConversion(root, records, file.source, dirRel);
      if (earlier?.current) {
        rejected.push({ source: file.source, reason: ALREADY_CONVERTED });
        continue;
      }
      const problem = await checkSource(file.source);
      if (problem !== null) {
        rejected.push({ source: file.source, reason: problem });
        continue;
      }
      const base = path.basename(file.source);
      const stem = base.lastIndexOf(".") > 0 ? base.slice(0, base.lastIndexOf(".")) : base;
      const name = earlier ? path.posix.basename(earlier.target) : uniqueName(`${fileStem(stem) || "Ad"}.mpg`, names);
      names.add(name);
      jobs.push(queue.enqueue(file.source, `${dirRel}/${name}`, file.aspect));
    }
    return { jobs, rejected };
  });

  app.get("/api/jobs", async () => queueFor(await requireRoot()).list());

  app.post("/api/jobs/retry", async () => ({ jobs: queueFor(await requireRoot()).retryFailed() }));

  app.get("/api/jobs/stream", (_request, reply) => {
    reply.hijack();
    const raw = reply.raw;
    raw.writeHead(200, { "content-type": "text/event-stream; charset=utf-8", "cache-control": "no-cache", connection: "keep-alive" });
    raw.write(": connected\n\n");
    const send = (job: Job) => {
      raw.write(`data: ${JSON.stringify(job)}\n\n`);
    };
    const heartbeat = setInterval(() => raw.write(": heartbeat\n\n"), HEARTBEAT_MS);
    const entry = {
      raw,
      cleanup: () => {
        clearInterval(heartbeat);
        jobEvents.off("change", send);
        streams.delete(entry);
      },
    };
    jobEvents.on("change", send);
    streams.add(entry);
    raw.on("close", entry.cleanup);
  });

  app.get("/api/library", async () => {
    const root = await requireRoot();
    try {
      const library = await readLibrary(root);
      const off = await readLibrary(root, OFF_LIBRARY_FILE);
      if (library) return { ...library, off: { ad: off?.ad ?? [], show: off?.show ?? [] } };
    } catch (err) {
      app.log.warn(err);
    }
    const built = await rebuildNow(root);
    return { ...built.library, off: built.off };
  });

  app.post("/api/library/rebuild", async () => rebuildNow(await requireRoot()));

  app.post("/api/library/edit", async (request) => {
    const body = objectBody(request.body);
    const mtimeMs = mtimeField(body);
    const dryRun = body.dryRun === true;
    const action = objectBody(body.action) as unknown as EditAction;
    if (typeof action.kind !== "string") throw bad("action must say what to change.");
    const root = await requireRoot();
    return withConfigLock(async () => {
      if ((await configMtime(root)) !== Math.trunc(mtimeMs)) throw new ConfigConflictError();
      await flushRebuild(root);
      const busy = new Set(queueFor(root).list().filter((j) => j.state === "queued" || j.state === "running").map((j) => j.target));
      const { config } = await readConfig(root);
      const off = await readOff(root);
      const library = mergeLibraries((await readLibrary(root)) ?? (await rebuildNow(root)).library, await readLibrary(root, OFF_LIBRARY_FILE));
      const files = new Map<string, string[]>();
      for (const s of library.show) files.set(`${s.channel}\0${s.folder}`, await topFiles(path.join(root, s.channel, s.folder)));
      let plan;
      try {
        plan = planEdit(action, { config, library, off, busy, showFiles: (ref) => files.get(`${ref.channel}\0${ref.show}`) ?? [] });
      } catch (err) {
        if (err instanceof EditRefused) throw new HttpError(err.status, err.message);
        throw err;
      }
      const before = (await validateRoot(root, false, bin)).issues;
      const check = async () => {
        const candidate = await buildLibrary(root, await readSources(root), probeFn, plan.exclude);
        return newErrors(before, (await validateCandidate(root, plan.config, bin, candidate.library)).issues);
      };
      if (dryRun) {
        if (plan.moves.length > 0) throw bad("This change can't be previewed.");
        return { mtimeMs, removed: plan.removed, issues: await check(), moves: plan.moves };
      }
      const applied = await applyPlan(root, plan);
      let saved = mtimeMs;
      try {
        const added = await check();
        if (added.length > 0) throw new HttpError(422, "This change would break a schedule.", added);
        if (JSON.stringify(plan.config) !== JSON.stringify(config)) saved = await writeConfig(root, plan.config, mtimeMs);
      } catch (err) {
        await applied.undo();
        throw err;
      }
      if (plan.deletes.length > 0 || plan.deleteDirs.length > 0) {
        const gone = new Set(plan.deletes);
        await applied.finish().catch((err: unknown) => app.log.warn(err));
        const records = await readSources(root);
        if (records.some((r) => gone.has(r.target))) await writeSources(root, records.filter((r) => !gone.has(r.target)));
      }
      await rebuildNow(root);
      return { mtimeMs: saved, removed: plan.removed, issues: [], moves: plan.moves };
    });
  });

  app.get("/api/library/notes", async () => lastBuild.get(await requireRoot()) ?? { at: null, skipped: [], warnings: [] });

  app.get("/api/show-meta", async (request) => {
    const query = request.query as Record<string, unknown>;
    const channelFolder = folderName(query.channelFolder, "channelFolder");
    const showFolder = folderName(query.showFolder, "showFolder");
    const root = await requireRoot();
    const showDir = inside(root, channelFolder, showFolder);
    if (!(await isDirectory(showDir))) throw new HttpError(404, "That show folder does not exist.");
    const mtimeMs = await showMetaMtime(showDir);
    return { meta: await editableShowMeta(showDir), mtimeMs };
  });

  app.put("/api/show-meta", async (request) => {
    const body = objectBody(request.body);
    const channelFolder = folderName(body.channelFolder, "channelFolder");
    const showFolder = folderName(body.showFolder, "showFolder");
    const mtimeMs = mtimeField(body);
    const name = body.name === undefined ? "" : stringField(body, "name");
    const rawEpisodes = body.episodes === undefined ? {} : objectBody(body.episodes);
    const episodes: Record<string, string> = {};
    for (const [key, value] of Object.entries(rawEpisodes)) {
      if (typeof value !== "string") throw bad(`episodes.${key} must be a string.`);
      episodes[key] = value;
    }
    const root = await requireRoot();
    const showDir = inside(root, channelFolder, showFolder);
    if (!(await isDirectory(showDir))) throw new HttpError(404, "That show folder does not exist.");
    await withConfigLock(async () => {
      if ((await showMetaMtime(showDir)) !== mtimeMs) throw new ShowMetaConflictError();
      await writeShowMeta(root, showDir, { ...(name.trim() !== "" ? { name: name.trim() } : {}), episodes });
    });
    return (await rebuildNow(root)).library;
  });

  app.get("/api/config", async () => readConfig(await requireRoot()));

  app.put("/api/config", async (request) => {
    const body = objectBody(request.body);
    let cfg: Config;
    try {
      cfg = toConfig(body.config);
    } catch (err) {
      if (err instanceof ConfigShapeError) throw bad(err.message.replace(/^config\.toml: /, "config: "));
      throw err;
    }
    const mtimeMs = mtimeField(body);
    const root = await requireRoot();
    return withConfigLock(() => saveConfig(root, cfg, mtimeMs));
  });

  app.post("/api/config/reshuffle", async (request) => {
    const mtimeMs = mtimeField(objectBody(request.body));
    const root = await requireRoot();
    return withConfigLock(async () => {
      const current = await readConfig(root);
      if (current.mtimeMs !== mtimeMs) throw new ConfigConflictError();
      let seed = randomSeed();
      while (seed === current.config.seed) seed = randomSeed();
      const saved = await saveConfig(root, { ...current.config, seed }, mtimeMs);
      return { mtimeMs: saved.mtimeMs, seed };
    });
  });

  app.post("/api/validate", async () => validateRoot(await requireRoot(), true, bin));

  app.get("/api/preview", async (request) => {
    const query = request.query as Record<string, unknown>;
    const channel = query.channel;
    const date = query.date;
    if (typeof channel !== "string" || !CHANNEL_QUERY.test(channel)) throw bad("channel must be a channel number.");
    if (typeof date !== "string" || !DATE_QUERY.test(date)) throw bad("date must look like YYYY-MM-DD.");
    const root = await requireRoot();
    const result = await lineup(root, Number(channel), date, bin);
    if ("ok" in result) throw new HttpError(422, "invalid", result.issues);
    return result;
  });

  app.post("/api/icon", async (request) => {
    const root = await requireRoot();
    const fields: Record<string, string> = {};
    let file: Buffer | null = null;
    for await (const part of request.parts()) {
      if (part.type === "file") {
        const data = await part.toBuffer();
        if (part.fieldname === "file" && file === null) file = data;
      } else {
        fields[part.fieldname] = String(part.value);
      }
    }
    const kind = fields.kind;
    if (kind !== "channel" && kind !== "show") throw bad('kind must be "channel" or "show".');
    const channelFolder = folderName(fields.channelFolder, "channelFolder");
    const parts = kind === "channel" ? [channelFolder, "channel.png"] : [channelFolder, folderName(fields.showFolder, "showFolder"), "show.png"];
    const dst = inside(root, ...parts);
    if (file === null) throw bad("file is required.");
    await makeIcon(root, file, dst);
    scheduleRebuild(root);
    return { path: parts.join("/") };
  });

  app.get("/api/library/file", async (request, reply) => {
    const root = await requireRoot();
    const rel = (request.query as Record<string, unknown>).path;
    const file = await libraryFile(root, rel, ".mpg");
    if (file === null || typeof rel !== "string") return reply.code(404).send({ error: "Not found" });
    const info = await stat(file);
    const record = (await sourceRecords(root)).find((r) => r.target === rel);
    return { size: info.size, mtimeMs: Math.trunc(info.mtimeMs), source: record?.source ?? null };
  });

  app.get("/api/media/play", async (request, reply) => {
    const root = await requireRoot();
    const query = request.query as Record<string, unknown>;
    const start = query.start ?? "0";
    if (typeof start !== "string" || !/^\d+$/.test(start)) throw bad("start must be a whole number of seconds.");
    const file = await libraryFile(root, query.path, ".mpg");
    if (file === null) return reply.code(404).send({ error: "Not found" });
    const { stream, stop } = remuxFn(file, Number(start));
    reply.raw.on("close", stop);
    return reply.type("video/mp2t").header("cache-control", "no-cache").send(stream);
  });

  app.get("/media/*", async (request, reply) => {
    const root = await requireRoot();
    const notFound = () => reply.code(404).send({ error: "Not found" });
    const rel = (request.params as { "*"?: string })["*"] ?? "";
    if (!rel.toLowerCase().endsWith(".png")) return notFound();
    const realRoot = await realpath(root);
    let real: string;
    try {
      real = await realpath(path.resolve(realRoot, rel));
    } catch {
      return notFound();
    }
    if (!contained(realRoot, real) || !real.toLowerCase().endsWith(".png")) return notFound();
    try {
      if (!(await stat(real)).isFile()) return notFound();
    } catch {
      return notFound();
    }
    return reply.type("image/png").header("cache-control", "no-cache").send(createReadStream(real));
  });

  if (server) registerWiiApi(app, path.join(server.library, "cable-twii"), server.name);

  return app;
}
