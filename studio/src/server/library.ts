import { readdir, stat } from "node:fs/promises";
import path from "node:path";
import { MIN_DURATION_MS, probe } from "./ffmpeg.js";
import { readShowMeta } from "./showMeta.js";
import { atomicWriteFile } from "./sources.js";
import type { LibAd, LibEpisode, LibShow, Library, SourceRecord } from "./types.js";

export interface SkippedFile {
  path: string;
  reason: string;
}

export interface BuildResult {
  library: Library;
  skipped: SkippedFile[];
  warnings: string[];
}

const LIBRARY_FILE = "library.toml";
const ADS_DIR = "ads";
const ICON_FILE = "show.png";
const SEASON_DIR = /^Season (\d+)$/i;
const SEASON_EPISODE = /^S(\d+)E(\d+)(?: - (.*))?$/i;
const EPISODE_ONLY = /^E(\d+)(?: - (.*))?$/i;

interface Named {
  name: string;
}

interface Scan {
  sources: Map<string, SourceRecord>;
  probeFn: typeof probe;
  skipped: SkippedFile[];
  warnings: string[];
}

function natural(a: string, b: string): number {
  return a.localeCompare(b, undefined, { numeric: true });
}

function byName(a: Named, b: Named): number {
  return natural(a.name, b.name);
}

function posix(...parts: string[]): string {
  return parts.join("/");
}

function key(rel: string): string {
  return rel.normalize("NFC");
}

function isVisible(name: string): boolean {
  return !name.startsWith(".");
}

async function listEntries(dir: string): Promise<{ dirs: string[]; files: string[] }> {
  let entries;
  try {
    entries = await readdir(dir, { withFileTypes: true });
  } catch (err) {
    if ((err as NodeJS.ErrnoException).code === "ENOENT") return { dirs: [], files: [] };
    throw err;
  }
  const dirs: string[] = [];
  const files: string[] = [];
  for (const entry of entries) {
    if (!isVisible(entry.name)) continue;
    if (entry.isDirectory()) dirs.push(entry.name);
    else if (entry.isFile()) files.push(entry.name);
  }
  return { dirs: dirs.sort(natural), files: files.sort(natural) };
}

function mpgFiles(files: string[]): string[] {
  return files.filter((name) => name.toLowerCase().endsWith(".mpg"));
}

async function durationOf(scan: Scan, root: string, rel: string): Promise<number | null> {
  let durationMs: number;
  const known = scan.sources.get(key(rel));
  if (known) {
    durationMs = known.durationMs;
  } else {
    try {
      durationMs = (await scan.probeFn(path.join(root, ...rel.split("/")))).durationMs;
    } catch (err) {
      scan.skipped.push({ path: rel, reason: `probe failed: ${(err as Error).message}` });
      return null;
    }
  }
  if (!Number.isInteger(durationMs) || durationMs < MIN_DURATION_MS) {
    scan.skipped.push({ path: rel, reason: `duration ${durationMs} ms is under ${MIN_DURATION_MS} ms` });
    return null;
  }
  return durationMs;
}

async function scanAds(scan: Scan, root: string, channel: string): Promise<LibAd[]> {
  const rel = channel === "" ? ADS_DIR : posix(channel, ADS_DIR);
  const { files } = await listEntries(path.join(root, ...rel.split("/")));
  const ads: LibAd[] = [];
  for (const file of mpgFiles(files)) {
    const adPath = posix(rel, file);
    const durationMs = await durationOf(scan, root, adPath);
    if (durationMs !== null) ads.push({ path: adPath, channel, duration_ms: durationMs });
  }
  return ads;
}

function stripMpg(file: string): string {
  return file.slice(0, file.length - ".mpg".length);
}

function parseName(stem: string): { season: number | null; episode: number | null; title: string } {
  const full = SEASON_EPISODE.exec(stem);
  if (full) return { season: Number(full[1]), episode: Number(full[2]), title: full[3] ?? "" };
  const only = EPISODE_ONLY.exec(stem);
  if (only) return { season: null, episode: Number(only[1]), title: only[2] ?? "" };
  return { season: null, episode: null, title: stem };
}

async function scanSeason(
  scan: Scan,
  root: string,
  showRel: string,
  dirName: string,
  season: number,
  descriptions: Record<string, string>,
): Promise<LibEpisode[]> {
  const { files } = await listEntries(path.join(root, ...showRel.split("/"), dirName));
  const parsed = mpgFiles(files).map((file) => ({ file, ...parseName(stripMpg(file)) }));
  let highest = 0;
  for (const p of parsed) if (p.episode !== null && p.episode > highest) highest = p.episode;
  const episodes: LibEpisode[] = [];
  for (const p of parsed) {
    const rel = posix(showRel, dirName, p.file);
    let episode = p.episode;
    if (episode === null) {
      highest += 1;
      episode = highest;
      scan.warnings.push(`${rel} has no episode number, numbered ${episode}`);
    }
    const durationMs = await durationOf(scan, root, rel);
    if (durationMs === null) continue;
    const code = `S${String(season).padStart(2, "0")}E${String(episode).padStart(2, "0")}`;
    episodes.push({
      season,
      episode,
      title: p.title,
      description: descriptions[code] ?? "",
      path: rel,
      duration_ms: durationMs,
    });
  }
  return episodes;
}

async function scanSeasonless(
  scan: Scan,
  root: string,
  showRel: string,
  files: string[],
  descriptions: Record<string, string>,
): Promise<LibEpisode[]> {
  const episodes: LibEpisode[] = [];
  const ordered = mpgFiles(files);
  for (let index = 0; index < ordered.length; index++) {
    const file = ordered[index]!;
    const stem = stripMpg(file);
    const rel = posix(showRel, file);
    const parsed = parseName(stem);
    const prefixed = SEASON_EPISODE.test(stem) || EPISODE_ONLY.test(stem);
    const durationMs = await durationOf(scan, root, rel);
    if (durationMs === null) continue;
    episodes.push({
      season: 0,
      episode: index + 1,
      title: prefixed ? parsed.title : stem,
      description: descriptions[stem] ?? "",
      path: rel,
      duration_ms: durationMs,
    });
  }
  return episodes;
}

async function exists(file: string): Promise<boolean> {
  try {
    return (await stat(file)).isFile();
  } catch {
    return false;
  }
}

async function scanShow(scan: Scan, root: string, channel: string, folder: string): Promise<LibShow | null> {
  const showRel = posix(channel, folder);
  const showDir = path.join(root, channel, folder);
  const { dirs, files } = await listEntries(showDir);
  const meta = await readShowMeta(showDir);
  const seasons = dirs
    .map((dir) => ({ dir, match: SEASON_DIR.exec(dir) }))
    .filter((s): s is { dir: string; match: RegExpExecArray } => s.match !== null)
    .map((s) => ({ dir: s.dir, season: Number(s.match[1]) }))
    .filter((s) => s.season >= 1)
    .sort((a, b) => a.season - b.season || natural(a.dir, b.dir));
  const episodes: LibEpisode[] = [];
  for (const s of seasons) {
    episodes.push(...(await scanSeason(scan, root, showRel, s.dir, s.season, meta.episodes)));
  }
  episodes.push(...(await scanSeasonless(scan, root, showRel, files, meta.episodes)));
  if (seasons.length === 0 && mpgFiles(files).length === 0) return null;
  episodes.sort((a, b) => a.season - b.season || a.episode - b.episode);
  const icon = (await exists(path.join(showDir, ICON_FILE))) ? posix(channel, folder, ICON_FILE) : "";
  return { channel, folder, name: meta.name ?? folder, icon, episode: episodes };
}

export async function buildLibrary(
  root: string,
  sources: SourceRecord[],
  probeFn: typeof probe = probe,
): Promise<BuildResult> {
  const scan: Scan = {
    sources: new Map(sources.map((s) => [key(s.target), s])),
    probeFn,
    skipped: [],
    warnings: [],
  };
  const ads: LibAd[] = [];
  const shows: LibShow[] = [];
  ads.push(...(await scanAds(scan, root, "")));
  const top = await listEntries(root);
  for (const channel of top.dirs) {
    if (channel === ADS_DIR) continue;
    ads.push(...(await scanAds(scan, root, channel)));
    const inner = await listEntries(path.join(root, channel));
    for (const folder of inner.dirs) {
      if (folder === ADS_DIR) continue;
      const show = await scanShow(scan, root, channel, folder);
      if (show) shows.push(show);
    }
  }
  ads.sort((a, b) => (a.path < b.path ? -1 : a.path > b.path ? 1 : 0));
  shows.sort((a, b) => natural(a.channel, b.channel) || natural(a.folder, b.folder));
  return {
    library: { version: 1, generated: new Date(), ad: ads, show: shows },
    skipped: scan.skipped,
    warnings: scan.warnings,
  };
}

function str(value: string): string {
  return JSON.stringify(value);
}

function two(value: number): string {
  return String(value).padStart(2, "0");
}

function localDateTime(d: Date): string {
  const date = `${String(d.getFullYear()).padStart(4, "0")}-${two(d.getMonth() + 1)}-${two(d.getDate())}`;
  return `${date}T${two(d.getHours())}:${two(d.getMinutes())}:${two(d.getSeconds())}`;
}

export function serializeLibrary(lib: Library): string {
  const out: string[] = ["version = 1", `generated = ${localDateTime(lib.generated)}`];
  for (const ad of lib.ad) {
    out.push("", "[[ad]]", `path = ${str(ad.path)}`, `channel = ${str(ad.channel)}`, `duration_ms = ${Math.trunc(ad.duration_ms)}`);
  }
  for (const show of lib.show) {
    out.push(
      "",
      "[[show]]",
      `channel = ${str(show.channel)}`,
      `folder = ${str(show.folder)}`,
      `name = ${str(show.name)}`,
      `icon = ${str(show.icon)}`,
    );
    for (const ep of show.episode) {
      out.push(
        "",
        "[[show.episode]]",
        `season = ${Math.trunc(ep.season)}`,
        `episode = ${Math.trunc(ep.episode)}`,
        `title = ${str(ep.title)}`,
        `description = ${str(ep.description)}`,
        `path = ${str(ep.path)}`,
        `duration_ms = ${Math.trunc(ep.duration_ms)}`,
      );
    }
  }
  return `${out.join("\n")}\n`;
}

export async function writeLibrary(root: string, lib: Library): Promise<void> {
  await atomicWriteFile(path.join(root, LIBRARY_FILE), serializeLibrary(lib));
}
