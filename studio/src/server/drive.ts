import { execFile } from "node:child_process";
import { realpath as realpathCallback } from "node:fs";
import { lstat, mkdir, readdir, realpath, stat, statfs } from "node:fs/promises";
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

export type RunCommand = (cmd: string, args: string[], input?: string, env?: Record<string, string>) => Promise<string>;

export type Env = Record<string, string | undefined>;

export interface DriveDeps {
  findMountPoint?: (p: string) => Promise<MountInfo>;
  statDev?: StatDev;
  bootDevices?: () => Promise<number[]>;
  runDiskutil?: (p: string) => Promise<DiskutilInfo>;
  run?: (cmd: string, args: string[]) => Promise<unknown>;
  exec?: RunCommand;
  platform?: NodeJS.Platform;
  env?: Env;
  homedir?: () => string;
  realpathNative?: (p: string) => Promise<string>;
}

export interface ScanResult {
  source: string;
  guess: EpisodeGuess;
  durationMs: number;
  aspect: "16:9" | "4:3";
  tooShort: boolean;
  error?: string;
}

const MISSING_REASON = "That folder doesn't exist.";
const NOT_ROOT_REASON = "Pick the drive itself, not a folder inside it.";
const SYSTEM_REASON = "Choose your media drive or a test folder, not a system folder.";
const WBFS_REASON = "This looks like your games drive (it has a wbfs folder). cable-twii will never write to it.";
const PROBE_CONCURRENCY = 4;
const VOLUME_PATH_VAR = "CTW_VOLUME_PATH";
const VOLUME_SCRIPT = [
  `$name = $env:${VOLUME_PATH_VAR}`,
  "$volumes = @(Get-CimInstance -ClassName Win32_Volume | Where-Object { $_.Name -eq $name })",
  "if ($volumes.Count -ne 1) { exit 3 }",
  "$volumes[0] | Select-Object FileSystem | ConvertTo-Json -Compress",
].join("; ");
const WINDOWS_DRIVE_FOLDERS = ["$Recycle.Bin", "System Volume Information", "Recovery", "Boot"];

export const execCommand: RunCommand = (cmd, args, input, env) =>
  new Promise((resolve, reject) => {
    const options = { maxBuffer: 16 * 1024 * 1024, env: env === undefined ? process.env : { ...process.env, ...env } };
    const child = execFile(cmd, args, options, (error, stdout) => {
      if (error) {
        reject(error);
        return;
      }
      resolve(stdout);
    });
    if (input !== undefined) child.stdin?.end(input);
  });

export async function defaultRunDiskutil(p: string, exec: RunCommand = execCommand): Promise<DiskutilInfo> {
  const plist = await exec("diskutil", ["info", "-plist", p]);
  const json = await exec("plutil", ["-convert", "json", "-o", "-", "-"], plist);
  return JSON.parse(json) as DiskutilInfo;
}

export async function defaultRunVolumeInfo(p: string, exec: RunCommand = execCommand): Promise<DiskutilInfo> {
  const letter = /^([A-Za-z]):\\/.exec(p)?.[1];
  if (letter === undefined || !/^[A-Za-z]$/.test(letter)) throw new Error(`Not a drive letter path: ${p}`);
  const name = p.endsWith("\\") ? p : `${p}\\`;
  const json = await exec("powershell.exe", ["-NoProfile", "-NonInteractive", "-Command", VOLUME_SCRIPT], undefined, { [VOLUME_PATH_VAR]: name });
  const raw: unknown = JSON.parse(json);
  const fsName = typeof raw === "object" && raw !== null && !Array.isArray(raw) ? (raw as { FileSystem?: unknown }).FileSystem : undefined;
  if (typeof fsName !== "string" || fsName.trim() === "") return {};
  const fsType = fsName.toLowerCase() === "fat32" ? "msdos" : fsName.toLowerCase();
  return { FilesystemType: fsType, FilesystemName: fsName };
}

function platformOf(deps: DriveDeps): NodeJS.Platform {
  return deps.platform ?? process.platform;
}

function runFsInfo(deps: DriveDeps): (p: string) => Promise<DiskutilInfo> {
  if (deps.runDiskutil !== undefined) return deps.runDiskutil;
  return platformOf(deps) === "win32" ? (p) => defaultRunVolumeInfo(p, deps.exec) : (p) => defaultRunDiskutil(p, deps.exec);
}

export interface MountInfo {
  mountPoint: string;
  external: boolean;
}

export type StatDev = (p: string) => Promise<number>;

const defaultStatDev: StatDev = async (p) => (await stat(p)).dev;

const defaultRealpathNative = (p: string): Promise<string> =>
  new Promise((resolve, reject) => {
    realpathCallback.native(p, (error, resolved) => {
      if (error) reject(error);
      else resolve(resolved);
    });
  });

function windowsSystemDrive(env: Env): string {
  const value = (env.SystemDrive ?? "").replace(/\\$/, "");
  return /^[A-Za-z]:$/.test(value) ? value : "C:";
}

export async function defaultBootDevices(
  statDev: StatDev = defaultStatDev,
  platform: NodeJS.Platform = process.platform,
  env: Env = process.env,
): Promise<number[]> {
  if (platform === "win32") return [await statDev(`${windowsSystemDrive(env)}\\`)];
  const devs: number[] = [await statDev("/")];
  try {
    devs.push(await statDev("/System/Volumes/Data"));
  } catch {
    devs.length = 1;
  }
  return devs;
}

export async function defaultFindMountPoint(
  p: string,
  statDev: StatDev = defaultStatDev,
  bootDevices: () => Promise<number[]> = () => defaultBootDevices(statDev, platform),
  platform: NodeJS.Platform = process.platform,
): Promise<MountInfo> {
  const isWindows = platform === "win32";
  const paths = isWindows ? path.win32 : path.posix;
  let current = p;
  const dev = await statDev(p);
  for (;;) {
    const parent = paths.dirname(current);
    if (parent === current) break;
    if ((await statDev(parent)) !== dev) break;
    current = parent;
  }
  const boot = await bootDevices();
  const external = isWindows ? !boot.includes(dev) : current.startsWith("/Volumes/") || !boot.includes(dev);
  return { mountPoint: current, external };
}

const BLOCKED_PREFIXES = ["/System", "/Library", "/usr", "/bin", "/sbin", "/Applications", "/private"];
const ALLOWED_PRIVATE = ["/private/tmp", "/private/var/folders"];

function under(p: string, prefix: string): boolean {
  return p === prefix || p.startsWith(prefix + "/");
}

function windowsKey(p: string): string {
  const normalized = path.win32.normalize(p);
  const trimmed = path.win32.parse(normalized).root === normalized ? normalized : normalized.replace(/\\+$/, "");
  return trimmed.toLowerCase();
}

function windowsUnder(key: string, prefix: string): boolean {
  return key === prefix || key.startsWith(prefix.endsWith("\\") ? prefix : `${prefix}\\`);
}

async function windowsKeys(value: string, realpathNative: (p: string) => Promise<string>): Promise<string[]> {
  const keys = [windowsKey(value)];
  try {
    keys.push(windowsKey(await realpathNative(value)));
  } catch {
    return keys;
  }
  return keys;
}

async function isWindowsSystemPath(p: string, deps: DriveDeps): Promise<boolean> {
  const env = deps.env ?? process.env;
  const resolveNative = deps.realpathNative ?? defaultRealpathNative;
  const drive = windowsSystemDrive(env);
  const key = windowsKey(p);
  const exact = [`${drive}\\`, (deps.homedir ?? os.homedir)()];
  const prefixes = [
    env.SystemRoot || `${drive}\\Windows`,
    env.ProgramFiles || `${drive}\\Program Files`,
    env["ProgramFiles(x86)"] || `${drive}\\Program Files (x86)`,
    env.ProgramData || `${drive}\\ProgramData`,
    ...WINDOWS_DRIVE_FOLDERS.map((name) => `${drive}\\${name}`),
  ];
  for (const value of exact) {
    if ((await windowsKeys(value, resolveNative)).includes(key)) return true;
  }
  for (const value of prefixes) {
    if ((await windowsKeys(value, resolveNative)).some((prefix) => windowsUnder(key, prefix))) return true;
  }
  const users = await windowsKeys(`${drive}\\Users`, resolveNative);
  return users.some((usersKey) => key === usersKey || path.win32.dirname(key) === usersKey);
}

export async function isSystemPath(p: string, deps: DriveDeps = {}): Promise<boolean> {
  if (platformOf(deps) === "win32") return isWindowsSystemPath(p, deps);
  if (p === "/" || p === "/Volumes" || p === "/dev" || p === "/Users") return true;
  const homedir = deps.homedir ?? os.homedir;
  let home = path.resolve(homedir());
  try {
    home = await realpath(home);
  } catch {
    home = path.resolve(homedir());
  }
  if (p === home) return true;
  if (ALLOWED_PRIVATE.some((a) => under(p, a))) return false;
  return BLOCKED_PREFIXES.some((b) => under(p, b));
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

export async function inspectDrive(inputPath: string, deps: DriveDeps = {}): Promise<DriveInfo> {
  let p: string;
  try {
    p = await realpath(path.resolve(inputPath));
  } catch {
    const missing = path.resolve(inputPath);
    return { path: missing, rootPath: path.join(missing, "cable-twii"), isVolumeRoot: false, fsType: null, freeBytes: 0, hasRoot: false, hasWbfs: false, usable: false, reason: MISSING_REASON };
  }
  const rootPath = path.join(p, "cable-twii");
  const hasRoot = await dirExists(rootPath);
  const freeBytes = await freeBytesOf(p);
  const refuse = (reason: string, extra: Partial<DriveInfo> = {}): DriveInfo => ({
    path: p, rootPath, isVolumeRoot: false, fsType: null, freeBytes, hasRoot, hasWbfs: false, usable: false, reason, ...extra,
  });

  let mount: MountInfo;
  try {
    const platform = platformOf(deps);
    const bootDevices = deps.bootDevices ?? (() => defaultBootDevices(deps.statDev, platform, deps.env));
    mount = await (deps.findMountPoint ?? ((x: string) => defaultFindMountPoint(x, deps.statDev, bootDevices, platform)))(p);
  } catch {
    return refuse(MISSING_REASON);
  }
  const isVolumeRoot = mount.mountPoint === p;

  if (!mount.external) {
    if (await isSystemPath(p, deps)) return refuse(SYSTEM_REASON);
    if (await looksLikeWbfs([...new Set([mount.mountPoint, p])])) return refuse(WBFS_REASON, { hasWbfs: true, isVolumeRoot });
    const nested = await nestedMountLabel(p, deps.statDev ?? defaultStatDev);
    if (nested !== null) return refuse(`${nested} is on a different drive`, { isVolumeRoot });
    return { path: p, rootPath, isVolumeRoot, fsType: null, freeBytes, hasRoot, hasWbfs: false, usable: true };
  }

  let fsType: string | null = null;
  let fsName: string | null = null;
  try {
    const info = await runFsInfo(deps)(mount.mountPoint);
    fsType = info.FilesystemType ?? null;
    fsName = info.FilesystemName ?? null;
  } catch {
    fsType = null;
  }
  const fsSaysWbfs = [fsType, fsName].some((v) => v !== null && v.toLowerCase().includes("wbfs"));
  const hasWbfs = fsSaysWbfs || (await looksLikeWbfs([...new Set([mount.mountPoint, p])]));

  let reason: string | undefined;
  if (hasWbfs) reason = WBFS_REASON;
  else if (fsType !== "msdos") reason = `This drive is ${fsType ?? "unknown"}. cable-twii needs a FAT32 (MS-DOS) drive.`;
  else if (!isVolumeRoot) reason = NOT_ROOT_REASON;

  if (reason === undefined) {
    const nested = await nestedMountLabel(p, deps.statDev ?? defaultStatDev);
    if (nested !== null) reason = `${nested} is on a different drive`;
  }

  const info: DriveInfo = { path: p, rootPath, isVolumeRoot, fsType, freeBytes, hasRoot, hasWbfs, usable: reason === undefined };
  if (reason !== undefined) info.reason = reason;
  return info;
}

const NESTED_LEVELS: [string[], string][] = [
  [["cable-twii"], "cable-twii"],
  [["cable-twii", "ads"], "cable-twii/ads"],
  [["cable-twii", ".studio"], "cable-twii/.studio"],
];

async function nestedMountLabel(p: string, statDev: StatDev): Promise<string | null> {
  const rootDev = await statDev(p);
  for (const [parts, label] of NESTED_LEVELS) {
    let dev: number;
    try {
      dev = await statDev(path.join(p, ...parts));
    } catch {
      continue;
    }
    if (dev !== rootDev) return label;
  }
  return null;
}

async function ensureNormalDir(full: string, label: string, rootDev: number, statDev: StatDev): Promise<void> {
  let st = await lstat(full).catch(() => null);
  if (st === null) {
    await mkdir(full);
    st = await lstat(full);
  }
  if (!st.isDirectory() || (await realpath(full)) !== full) throw new Error(`${label} is not a normal folder`);
  if ((await statDev(full)) !== rootDev) throw new Error(`${label} is on a different drive`);
}

export async function initRoot(info: DriveInfo, deps: DriveDeps = {}): Promise<void> {
  const fresh = await inspectDrive(info.path, deps);
  if (!fresh.usable) throw new Error(fresh.reason ?? "This drive cannot be used.");
  const base = fresh.path;
  const steps: [string, string][] = [
    [path.join(base, "cable-twii"), "cable-twii"],
    [path.join(base, "cable-twii", "ads"), "cable-twii/ads"],
    [path.join(base, "cable-twii", ".studio"), "cable-twii/.studio"],
  ];
  const statDev = deps.statDev ?? defaultStatDev;
  const rootDev = await statDev(base);
  for (const [full, label] of steps) {
    await ensureNormalDir(full, label, rootDev, statDev);
  }
  const resolved = await realpath(path.join(base, "cable-twii"));
  if (resolved !== path.join(base, "cable-twii")) throw new Error("cable-twii is not a normal folder");
}

export async function dotClean(volumePath: string, deps: DriveDeps = {}): Promise<void> {
  const info = await inspectDrive(volumePath, deps);
  if (!info.usable) throw new Error(info.reason ?? "This drive cannot be used.");
  if (!info.isVolumeRoot || info.fsType !== "msdos") throw new Error("dot_clean only runs on a FAT32 drive root.");
  if (platformOf(deps) === "win32") return;
  const run = deps.run ?? ((cmd: string, args: string[]) => execCommand(cmd, args));
  await run("dot_clean", ["-m", info.path]);
}

async function collectVideos(dir: string, out: string[], isTop: boolean): Promise<void> {
  let entries;
  try {
    entries = await readdir(dir, { withFileTypes: true });
  } catch (error) {
    if (isTop) throw error;
    return;
  }
  for (const entry of entries) {
    if (entry.name.startsWith(".") || entry.isSymbolicLink()) continue;
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      await collectVideos(full, out, false);
    } else if (entry.isFile() && isVideoFile(entry.name)) {
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

export async function scanSource(dir: string): Promise<ScanResult[]> {
  const files: string[] = [];
  await collectVideos(dir, files, true);
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
