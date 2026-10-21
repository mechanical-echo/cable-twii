import { EventEmitter } from "node:events";
import { createReadStream, existsSync } from "node:fs";
import { readFile, readdir, realpath, stat } from "node:fs/promises";
import type { ServerResponse } from "node:http";
import os from "node:os";
import path from "node:path";
import multipart from "@fastify/multipart";
import fastifyStatic from "@fastify/static";
import Fastify, { type FastifyInstance, type FastifyReply } from "fastify";
import { ConfigConflictError, ConfigShapeError, configMtime, randomSeed, readConfig, toConfig, writeConfig } from "./config.js";
import { type DriveDeps, type DriveInfo, dotClean, initRoot, inspectDrive, scanSource } from "./drive.js";
import { isTooShort, MIN_DURATION_MS, probe } from "./ffmpeg.js";
import { makeIcon, NotImageError } from "./icons.js";
import { type BuildResult, buildLibrary, readLibrary, writeLibrary } from "./library.js";
import { episodeFileName, fatSafe, uniqueName } from "./names.js";
import { pickFolder } from "./picker.js";
import { ConversionQueue } from "./queue.js";
import { SchedUsageError, STUDIO_DIR, lineup, validateCandidate, validateRoot } from "./sched.js";
import { readShowMeta, writeShowMeta } from "./showMeta.js";
import { atomicWriteFile, readSources } from "./sources.js";
import type { Config, Issue, Job } from "./types.js";

export interface AppOptions {
  schedBin: string;
  queueFactory?: (root: string) => ConversionQueue;
  settingsPath?: string;
  inspectDeps?: DriveDeps;
  probeFn?: typeof probe;
  pickFolderFn?: (prompt: string) => Promise<string | null>;
  rebuildDelayMs?: number;
  webDir?: string;
  logger?: boolean;
}

type Aspect = "16:9" | "4:3";

interface Rejected {
  source: string;
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

function contained(root: string, full: string): boolean {
  const rel = path.relative(root, full);
  return rel !== "" && rel !== ".." && !rel.startsWith(`..${path.sep}`) && !path.isAbsolute(rel);
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

function sendNull(reply: FastifyReply): FastifyReply {
  return reply.type("application/json; charset=utf-8").send("null");
}

export function buildApp(opts: AppOptions): FastifyInstance {
  const app = Fastify({ logger: opts.logger ?? false, bodyLimit: BODY_LIMIT });
  const settingsPath = opts.settingsPath ?? DEFAULT_SETTINGS;
  const deps = opts.inspectDeps ?? {};
  const probeFn = opts.probeFn ?? probe;
  const pick = opts.pickFolderFn ?? ((prompt: string) => pickFolder(prompt));
  const rebuildDelay = opts.rebuildDelayMs ?? REBUILD_DELAY_MS;
  const webDir = opts.webDir ?? DEFAULT_WEB_DIR;
  const bin = opts.schedBin;

  const jobEvents = new EventEmitter();
  jobEvents.setMaxListeners(0);
  const queues = new Map<string, ConversionQueue>();
  const rebuildTimers = new Map<string, NodeJS.Timeout>();
  const rebuildChains = new Map<string, Promise<unknown>>();
  const streams = new Set<{ raw: ServerResponse; cleanup: () => void }>();
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
    const info = await requireDrive();
    if (!info.usable) throw new HttpError(409, info.reason ?? "This drive cannot be used.");
    if (!info.hasRoot) throw new HttpError(409, "This drive has no cable-twii folder yet. Set it up first.");
    let real: string;
    try {
      real = await realpath(info.rootPath);
    } catch {
      throw new HttpError(409, "The cable-twii folder is missing.");
    }
    if (real !== info.rootPath) throw new HttpError(409, "cable-twii is not a normal folder.");
    return info.rootPath;
  }

  function rebuildNow(root: string): Promise<BuildResult> {
    const previous = rebuildChains.get(root) ?? Promise.resolve();
    const run = async (): Promise<BuildResult> => {
      const result = await buildLibrary(root, await readSources(root), probeFn);
      await writeLibrary(root, result.library);
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

  function queueFor(root: string): ConversionQueue {
    let queue = queues.get(root);
    if (queue) return queue;
    queue = opts.queueFactory ? opts.queueFactory(root) : new ConversionQueue(root);
    queue.on("change", (job: Job) => jobEvents.emit("change", job));
    queue.on("converted", () => scheduleRebuild(root));
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
    const result = await validateCandidate(root, cfg, bin);
    if (!result.ok || result.issues.some((i) => i.level === "error")) throw new HttpError(422, "invalid", result.issues);
    const saved = await writeConfig(root, cfg, mtimeMs);
    return { mtimeMs: saved, issues: result.issues.filter((i) => i.level === "warning") };
  }

  async function checkSource(source: string): Promise<string | null> {
    try {
      const { durationMs } = await probeFn(source);
      if (isTooShort(durationMs)) return `too short (${durationMs} ms, needs at least ${MIN_DURATION_MS} ms)`;
      return null;
    } catch (err) {
      return `could not read the video: ${errorText(err)}`;
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

  async function rememberDrive(input: string): Promise<DriveInfo> {
    const info = await inspectDrive(input, deps);
    if (await pathExists(info.path)) await saveDrive(info.path);
    return info;
  }

  app.setErrorHandler((error: unknown, request, reply) => {
    if (error instanceof ConfigConflictError) return reply.code(409).send({ error: error.message });
    if (error instanceof ConfigShapeError) return reply.code(422).send({ error: error.message });
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

  app.register(multipart, { limits: { fileSize: ICON_LIMIT, files: 1, fields: 10 } });

  if (existsSync(webDir)) app.register(fastifyStatic, { root: webDir, prefix: "/" });

  app.addHook("preClose", async () => {
    for (const stream of streams) {
      stream.cleanup();
      stream.raw.end();
    }
    streams.clear();
  });

  app.addHook("onClose", async () => {
    for (const timer of rebuildTimers.values()) clearTimeout(timer);
    rebuildTimers.clear();
    await Promise.allSettled([...queues.values()].map((q) => q.stop()));
    await Promise.allSettled([...rebuildChains.values()]);
  });

  app.get("/api/drive", async (_request, reply) => {
    const info = await currentDrive();
    return info ?? sendNull(reply);
  });

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
    if (info.usable && info.hasRoot) await flushRebuild(info.rootPath);
    if (!(info.usable && info.isVolumeRoot && info.fsType !== null)) return { ok: true, skipped: true };
    await dotClean(info.path, deps);
    return { ok: true };
  });

  app.post("/api/import/pick-source", async (_request, reply) => {
    const dir = await pick("Choose a folder with videos");
    if (dir === null) return sendNull(reply);
    return { dir, files: await scanSource(dir) };
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
      };
    });
    const root = await requireRoot();
    const queue = queueFor(root);
    const taken = takenNames(root, queue);
    const jobs: Job[] = [];
    const rejected: Rejected[] = [];
    const descriptions: Record<string, string> = {};
    const showRel = `${channelFolder}/${showFolder}`;
    for (const ep of episodes) {
      const problem = await checkSource(ep.source);
      if (problem !== null) {
        rejected.push({ source: ep.source, reason: problem });
        continue;
      }
      const seasoned = ep.season !== null && ep.season >= 1;
      const dirRel = seasoned ? `${showRel}/Season ${ep.season}` : showRel;
      const names = await taken(dirRel);
      const name = uniqueName(episodeFileName({ season: seasoned ? ep.season : null, episode: ep.episode, title: ep.title }), names);
      names.add(name);
      jobs.push(queue.enqueue(ep.source, `${dirRel}/${name}`, ep.aspect));
      if (ep.description === "") continue;
      if (!seasoned) descriptions[name.slice(0, -".mpg".length)] = ep.description;
      else if (ep.episode !== null) descriptions[`S${two(ep.season!)}E${two(ep.episode)}`] ??= ep.description;
    }
    if (jobs.length > 0) {
      const showDir = inside(root, channelFolder, showFolder);
      const meta = await readShowMeta(showDir);
      const name = showName.trim() !== "" ? showName.trim() : meta.name;
      await writeShowMeta(showDir, { ...(name !== undefined ? { name } : {}), episodes: { ...meta.episodes, ...descriptions } });
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
    const root = await requireRoot();
    const queue = queueFor(root);
    const dirRel = channelFolder === "" ? "ads" : `${channelFolder}/ads`;
    const names = await takenNames(root, queue)(dirRel);
    const jobs: Job[] = [];
    const rejected: Rejected[] = [];
    for (const file of files) {
      const problem = await checkSource(file.source);
      if (problem !== null) {
        rejected.push({ source: file.source, reason: problem });
        continue;
      }
      const base = path.basename(file.source);
      const stem = base.lastIndexOf(".") > 0 ? base.slice(0, base.lastIndexOf(".")) : base;
      const name = uniqueName(`${fatSafe(stem) || "Ad"}.mpg`, names);
      names.add(name);
      jobs.push(queue.enqueue(file.source, `${dirRel}/${name}`, file.aspect));
    }
    return { jobs, rejected };
  });

  app.get("/api/jobs", async () => queueFor(await requireRoot()).list());

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
      if (library) return library;
    } catch (err) {
      app.log.warn(err);
    }
    return (await rebuildNow(root)).library;
  });

  app.post("/api/library/rebuild", async () => rebuildNow(await requireRoot()));

  app.put("/api/show-meta", async (request) => {
    const body = objectBody(request.body);
    const channelFolder = folderName(body.channelFolder, "channelFolder");
    const showFolder = folderName(body.showFolder, "showFolder");
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
    await writeShowMeta(showDir, { ...(name.trim() !== "" ? { name: name.trim() } : {}), episodes });
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
    await makeIcon(file, dst);
    scheduleRebuild(root);
    return { path: parts.join("/") };
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

  return app;
}
