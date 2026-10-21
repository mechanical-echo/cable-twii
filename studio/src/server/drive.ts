import { execFile } from "node:child_process";
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

export type RunCommand = (cmd: string, args: string[], input?: string) => Promise<string>;

export interface DriveDeps {
  findMountPoint?: (p: string) => Promise<MountInfo>;
  statDev?: StatDev;
  bootDevices?: () => Promise<number[]>;
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

const MISSING_REASON = "That folder doesn't exist.";
const NOT_ROOT_REASON = "Pick the drive itself, not a folder inside it.";
const SYSTEM_REASON = "Choose your media drive or a test folder, not a system folder.";
const WBFS_REASON = "This looks like your games drive (it has a wbfs folder). cable-twii will never write to it.";
const PROBE_CONCURRENCY = 4;

const isWindows = process.platform === "win32";

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

export async function defaultRunDiskutil(p: string, exec: RunCommand = execCommand): Promise<DiskutilInfo> {
  const plist = await exec("diskutil", ["info", "-plist", p]);
  const json = await exec("plutil", ["-convert", "json", "-o", "-", "-"], plist);
  return JSON.parse(json) as DiskutilInfo;
}

export async function defaultRunVolumeInfo(p: string, exec: RunCommand = execCommand): Promise<DiskutilInfo> {
  const driveLetter = path.parse(p).root.replace(/\\$/, "").replace(/:$/, "");
  const json = await exec("powershell.exe", [
    "-NoProfile",
    "-NonInteractive",
    "-Command",
    `Get-Volume -DriveLetter ${driveLetter} | Select-Object FileSystemType, SizeRemaining | ConvertTo-Json -Compress`,
  ]);
  const raw = JSON.parse(json) as { FileSystemType?: string; SizeRemaining?: number };
  const fsName = raw.FileSystemType ?? null;
  const fsType = fsName !== null && fsName.toLowerCase() === "fat32" ? "msdos" : fsName;
  return { FilesystemType: fsType ?? undefined, FilesystemName: fsName ?? undefined, FreeSpace: raw.SizeRemaining };
}

const defaultRunFsInfo: (p: string) => Promise<DiskutilInfo> = isWindows ? defaultRunVolumeInfo : defaultRunDiskutil;

export interface MountInfo {
  mountPoint: string;
  external: boolean;
}

export type StatDev = (p: string) => Promise<number>;

const defaultStatDev: StatDev = async (p) => (await stat(p)).dev;

export async function defaultBootDevices(statDev: StatDev = defaultStatDev): Promise<number[]> {
  if (isWindows) {
    const systemDrive = `${(process.env.SystemDrive ?? "C:").replace(/\\$/, "")}\\`;
    return [await statDev(systemDrive)];
  }
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
  bootDevices: () => Promise<number[]> = () => defaultBootDevices(statDev),
): Promise<MountInfo> {
  let current = p;
  const dev = await statDev(p);
  for (;;) {
    const parent = path.dirname(current);
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
  if (isWindows) return p.toLowerCase() === prefix.toLowerCase() || p.toLowerCase().startsWith(prefix.toLowerCase() + "\\");
  return p === prefix || p.startsWith(prefix + "/");
}

function windowsBlockedPrefixes(): string[] {
  const systemDrive = (process.env.SystemDrive ?? "C:").replace(/\\$/, "");
  const prefixes = [
    process.env.SystemRoot ?? `${systemDrive}\\Windows`,
    process.env.ProgramFiles ?? `${systemDrive}\\Program Files`,
    process.env["ProgramFiles(x86)"] ?? `${systemDrive}\\Program Files (x86)`,
    process.env.ProgramData ?? `${systemDrive}\\ProgramData`,
  ];
  return prefixes;
}

async function isSystemPath(p: string): Promise<boolean> {
  if (isWindows) {
    const systemDriveRoot = `${(process.env.SystemDrive ?? "C:").replace(/\\$/, "")}\\`;
    if (p.toLowerCase() === systemDriveRoot.toLowerCase()) return true;
    if (windowsBlockedPrefixes().some((b) => under(p, b))) return true;
  } else {
    if (p === "/" || p === "/Volumes" || p === "/dev" || p === "/Users") return true;
    if (ALLOWED_PRIVATE.some((a) => under(p, a))) return false;
    if (BLOCKED_PREFIXES.some((b) => under(p, b))) return true;
  }
  let home = path.resolve(os.homedir());
  try {
    home = await realpath(home);
  } catch {
    home = path.resolve(os.homedir());
  }
  return p === home;
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
    mount = await (deps.findMountPoint ?? ((x: string) => defaultFindMountPoint(x, deps.statDev, deps.bootDevices)))(p);
  } catch {
    return refuse(MISSING_REASON);
  }
  const isVolumeRoot = mount.mountPoint === p;

  if (!mount.external) {
    if (await isSystemPath(p)) return refuse(SYSTEM_REASON);
    if (await looksLikeWbfs([...new Set([mount.mountPoint, p])])) return refuse(WBFS_REASON, { hasWbfs: true, isVolumeRoot });
    const nested = await nestedMountLabel(p, deps.statDev ?? defaultStatDev);
    if (nested !== null) return refuse(`${nested} is on a different drive`, { isVolumeRoot });
    return { path: p, rootPath, isVolumeRoot, fsType: null, freeBytes, hasRoot, hasWbfs: false, usable: true };
  }

  let fsType: string | null = null;
  let fsName: string | null = null;
  try {
    const info = await (deps.runDiskutil ?? ((x: string) => defaultRunFsInfo(x)))(mount.mountPoint);
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
  if (isWindows) return; // macOS-only cleanup of its own ._* AppleDouble files; nothing to do here.
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
