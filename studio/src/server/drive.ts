import { execFile } from "node:child_process";
import { mkdir, readdir, stat, statfs } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import type { EpisodeGuess } from "./types.js";
import { guessEpisode, isVideoFile } from "./names.js";
import { aspectFor, isTooShort, probe } from "./ffmpeg.js";

export interface DriveInfo {
  path: string;
  rootPath: string;
  isVolumeRoot: boolean;
  fsType: string | null;
  freeBytes: number;
  hasRoot: boolean;
  hasWbfs: boolean;
  usable: boolean;
  reason?: string;
}

export interface DiskutilInfo {
  FilesystemType?: string;
  FilesystemName?: string;
  FreeSpace?: number;
  APFSContainerFree?: number;
}

export type RunCommand = (cmd: string, args: string[], input?: string) => Promise<string>;

export interface DriveDeps {
  isMountPoint?: (p: string) => Promise<boolean>;
  runDiskutil?: (p: string) => Promise<DiskutilInfo>;
  run?: (cmd: string, args: string[]) => Promise<unknown>;
}

export interface ScanResult {
  source: string;
  guess: EpisodeGuess;
  durationMs: number;
  aspect: "16:9" | "4:3";
  tooShort: boolean;
  error?: string;
}

const SYSTEM_REASON = "Choose your media drive or a test folder, not a system folder.";
const WBFS_REASON = "This looks like your games drive (it has a wbfs folder). cable-twii will never write to it.";
const PROBE_CONCURRENCY = 4;

export const execCommand: RunCommand = (cmd, args, input) =>
  new Promise((resolve, reject) => {
    const child = execFile(cmd, args, { maxBuffer: 16 * 1024 * 1024 }, (error, stdout) => {
      if (error) {
        reject(error);
        return;
      }
      resolve(stdout);
    });
    if (input !== undefined) child.stdin?.end(input);
  });

/**
 * @throws {Error} when diskutil or plutil fails or returns unreadable output
 */
export async function defaultRunDiskutil(p: string, exec: RunCommand = execCommand): Promise<DiskutilInfo> {
  const plist = await exec("diskutil", ["info", "-plist", p]);
  const json = await exec("plutil", ["-convert", "json", "-o", "-", "-"], plist);
  return JSON.parse(json) as DiskutilInfo;
}

async function defaultIsMountPoint(p: string): Promise<boolean> {
  if (path.dirname(p) === "/Volumes") return true;
  try {
    const [self, parent] = await Promise.all([stat(p), stat(path.join(p, ".."))]);
    return self.dev !== parent.dev;
  } catch {
    return false;
  }
}

function isSystemPath(p: string): boolean {
  return p === "/" || p === "/System" || p.startsWith("/System/") || p === path.resolve(os.homedir());
}

async function dirExists(p: string): Promise<boolean> {
  try {
    return (await stat(p)).isDirectory();
  } catch {
    return false;
  }
}

async function safeReaddir(p: string) {
  try {
    return await readdir(p, { withFileTypes: true });
  } catch {
    return [];
  }
}

async function looksLikeWbfs(roots: string[]): Promise<boolean> {
  for (const root of roots) {
    const top = await safeReaddir(root);
    for (const entry of top) {
      const lower = entry.name.toLowerCase();
      if (entry.isDirectory() && lower === "wbfs") return true;
      if (lower.endsWith(".wbfs")) return true;
    }
    for (const entry of top) {
      if (!entry.isDirectory()) continue;
      const inner = await safeReaddir(path.join(root, entry.name));
      if (inner.some((e) => e.name.toLowerCase().endsWith(".wbfs"))) return true;
    }
  }
  return false;
}

async function freeBytesOf(p: string): Promise<number> {
  try {
    const s = await statfs(p);
    return Number(s.bavail) * Number(s.bsize);
  } catch {
    return 0;
  }
}

function volumeRootOf(p: string): string | null {
  const parts = p.split(path.sep);
  if (parts[1] === "Volumes" && parts.length > 3 && parts[2] !== "") return path.join("/Volumes", parts[2]);
  return null;
}

/**
 * @throws {Error} never for filesystem problems; they are reported through usable and reason
 */
export async function inspectDrive(inputPath: string, deps: DriveDeps = {}): Promise<DriveInfo> {
  const p = path.resolve(inputPath);
  const rootPath = path.join(p, "cable-twii");
  const base = { path: p, rootPath };

  if (isSystemPath(p)) {
    return { ...base, isVolumeRoot: false, fsType: null, freeBytes: await freeBytesOf(p), hasRoot: false, hasWbfs: false, usable: false, reason: SYSTEM_REASON };
  }

  const isVolumeRoot = await (deps.isMountPoint ?? defaultIsMountPoint)(p);
  let fsType: string | null = null;
  let fsName: string | null = null;
  if (isVolumeRoot) {
    try {
      const info = await (deps.runDiskutil ?? ((x: string) => defaultRunDiskutil(x)))(p);
      fsType = info.FilesystemType ?? null;
      fsName = info.FilesystemName ?? null;
    } catch {
      fsType = null;
    }
  }

  const roots = [p];
  const parentVolume = volumeRootOf(p);
  if (parentVolume && parentVolume !== p) roots.push(parentVolume);
  const fsSaysWbfs = [fsType, fsName].some((v) => v !== null && v.toLowerCase().includes("wbfs"));
  const hasWbfs = fsSaysWbfs || (await looksLikeWbfs(roots));
  const hasRoot = await dirExists(rootPath);
  const freeBytes = await freeBytesOf(p);

  let reason: string | undefined;
  if (hasWbfs) reason = WBFS_REASON;
  else if (isVolumeRoot && fsType !== "msdos") reason = `This drive is ${fsType ?? "unknown"}. cable-twii needs a FAT32 (MS-DOS) drive.`;

  const info: DriveInfo = { ...base, isVolumeRoot, fsType, freeBytes, hasRoot, hasWbfs, usable: reason === undefined };
  if (reason !== undefined) info.reason = reason;
  return info;
}

/**
 * @throws {Error} when the drive is not usable
 */
export async function initRoot(info: DriveInfo, deps: DriveDeps = {}): Promise<void> {
  const fresh = await inspectDrive(info.path, deps);
  if (!fresh.usable) throw new Error(fresh.reason ?? "This drive cannot be used.");
  await mkdir(fresh.rootPath, { recursive: true });
  await mkdir(path.join(fresh.rootPath, "ads"), { recursive: true });
  await mkdir(path.join(fresh.rootPath, ".studio"), { recursive: true });
}

/**
 * @throws {Error} when the path is not a usable volume root or dot_clean fails
 */
export async function dotClean(volumePath: string, deps: DriveDeps = {}): Promise<void> {
  const info = await inspectDrive(volumePath, deps);
  if (!info.isVolumeRoot) throw new Error("dot_clean only runs on a drive root.");
  if (!info.usable) throw new Error(info.reason ?? "This drive cannot be used.");
  const run = deps.run ?? ((cmd: string, args: string[]) => execCommand(cmd, args));
  await run("dot_clean", ["-m", info.path]);
}

async function collectVideos(dir: string, out: string[]): Promise<void> {
  const entries = await readdir(dir, { withFileTypes: true });
  for (const entry of entries) {
    if (entry.isSymbolicLink()) continue;
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      await collectVideos(full, out);
    } else if (entry.isFile() && !entry.name.startsWith("._") && isVideoFile(entry.name)) {
      out.push(full);
    }
  }
}

async function scanOne(source: string): Promise<ScanResult> {
  const guess = guessEpisode(path.basename(source));
  try {
    const result = await probe(source);
    return { source, guess, durationMs: result.durationMs, aspect: aspectFor(result.displayAspect), tooShort: isTooShort(result.durationMs) };
  } catch (error) {
    return { source, guess, durationMs: 0, aspect: "4:3", tooShort: isTooShort(0), error: error instanceof Error ? error.message : String(error) };
  }
}

function compareNullsLast(a: number | null, b: number | null): number {
  if (a === null && b === null) return 0;
  if (a === null) return 1;
  if (b === null) return -1;
  return a - b;
}

/**
 * @throws {Error} when the folder cannot be read
 */
export async function scanSource(dir: string): Promise<ScanResult[]> {
  const files: string[] = [];
  await collectVideos(dir, files);
  const results: ScanResult[] = new Array(files.length);
  let next = 0;
  const worker = async () => {
    while (next < files.length) {
      const i = next++;
      results[i] = await scanOne(files[i]);
    }
  };
  await Promise.all(Array.from({ length: Math.min(PROBE_CONCURRENCY, files.length) }, worker));
  return results.sort(
    (a, b) =>
      compareNullsLast(a.guess.season, b.guess.season) ||
      compareNullsLast(a.guess.episode, b.guess.episode) ||
      path.basename(a.source).localeCompare(path.basename(b.source), undefined, { numeric: true }),
  );
}
