import type { ScanResult } from "../server/drive.js";
import { cleanTitle, episodeMatch, fatSafe, guessEpisode, isVideoFile, stripExtension } from "../server/names.js";
import type { Config, Job } from "../server/types.js";
import { effectiveFolder } from "./channels.js";
import { describeTarget } from "./episodes.js";
import type { WebLibrary } from "./tree.js";
import { storedUploadName } from "./uploading.js";

export type RowNote = "added" | "on-air" | "in-batch" | "same-name" | "too-short" | "unreadable" | null;

export interface AddRow {
  id: string;
  name: string;
  size: number | null;
  durationMs: number | null;
  aspect: "16:9" | "4:3" | null;
  season: number | null;
  episode: number | null;
  title: string;
  description: string;
  show: string;
  ticked: boolean;
  held: boolean;
  note: RowNote;
  replaces: string | null;
  source: string | null;
  file: File | null;
}

export interface Destination {
  channel: string;
  show: string;
  isNewShow: boolean;
}

export interface AddGroup {
  show: string;
  rows: AddRow[];
  dest: Destination;
}

export function guessShow(fileName: string): string {
  const base = stripExtension(fileName);
  const match = episodeMatch(base);
  if (match === null) return "";
  return cleanTitle(base.slice(0, match.index)).replace(/(?:\s*-)+$/, "").trim();
}

function baseName(source: string): string {
  return source.slice(Math.max(source.lastIndexOf("/"), source.lastIndexOf("\\")) + 1);
}

export function videoFiles(files: File[]): { videos: File[]; skipped: number } {
  const videos = files.filter((f) => isVideoFile(f.name));
  return { videos, skipped: files.length - videos.length };
}

export function rowsFromFiles(files: File[], added: string[]): AddRow[] {
  const known = new Set(added);
  const stored = new Set<string>();
  return files.map((file, i) => {
    const guess = guessEpisode(file.name);
    const key = storedUploadName(file.name).toLowerCase();
    const note: RowNote = stored.has(key) ? "same-name" : known.has(file.name) ? "added" : null;
    stored.add(key);
    return {
      id: `file:${i}:${file.name}`,
      name: file.name,
      size: file.size,
      durationMs: null,
      aspect: null,
      season: guess.season,
      episode: guess.episode,
      title: guess.title,
      description: "",
      show: guessShow(file.name),
      ticked: note === null,
      held: false,
      note,
      replaces: null,
      source: null,
      file,
    };
  });
}

export function rowsFromScan(scan: ScanResult[]): AddRow[] {
  return scan.map((entry) => {
    const name = baseName(entry.source);
    const note: RowNote = entry.error !== undefined ? "unreadable" : entry.tooShort ? "too-short" : entry.added ? "added" : null;
    return {
      id: `scan:${entry.source}`,
      name,
      size: null,
      durationMs: entry.durationMs,
      aspect: entry.aspect,
      season: entry.guess.season,
      episode: entry.guess.episode,
      title: entry.guess.title,
      description: "",
      show: guessShow(name),
      ticked: note === null,
      held: false,
      note,
      replaces: null,
      source: entry.source,
      file: null,
    };
  });
}

export function commonShow(rows: AddRow[]): string {
  const counts = new Map<string, number>();
  for (const row of rows) if (row.show !== "") counts.set(row.show, (counts.get(row.show) ?? 0) + 1);
  let best = "";
  let most = 0;
  for (const [show, count] of counts) {
    if (count > most) {
      best = show;
      most = count;
    }
  }
  return best;
}

function destination(show: string, library: WebLibrary, config: Config, channel: number | null): Destination {
  const wanted = show.toLowerCase();
  const preferred = channel === null ? undefined : config.channel.find((c) => c.number === channel);
  const matches = library.show.filter((s) => s.name.toLowerCase() === wanted || s.folder.toLowerCase() === wanted);
  const match = matches.find((s) => preferred !== undefined && s.channel === effectiveFolder(preferred)) ?? matches[0];
  if (match !== undefined) return { channel: match.channel, show: match.folder, isNewShow: false };
  const first = [...config.channel].sort((a, b) => a.number - b.number)[0];
  const target = preferred ?? first;
  return { channel: target === undefined ? "" : effectiveFolder(target), show, isNewShow: true };
}

export function groupRows(rows: AddRow[], fallbackShow: string, library: WebLibrary, config: Config, channel: number | null): AddGroup[] {
  const groups = new Map<string, { show: string; rows: AddRow[] }>();
  for (const row of rows) {
    const show = row.show !== "" ? row.show : fallbackShow;
    const key = show.toLowerCase();
    let group = groups.get(key);
    if (group === undefined) {
      group = { show, rows: [] };
      groups.set(key, group);
    }
    group.rows.push({ ...row, show: group.show });
  }
  return [...groups.values()].map((g) => ({ ...g, dest: destination(g.show, library, config, channel) }));
}

const OWN_NOTES: RowNote[] = [null, "on-air", "in-batch"];

export function canTick(row: Pick<AddRow, "note" | "replaces">): boolean {
  return row.note === null || (row.note === "on-air" && row.replaces !== null);
}

function destKey(dest: Destination): string {
  return `${dest.channel}\0${dest.show}`.toLowerCase();
}

export function markAll(groups: AddGroup[], library: WebLibrary, jobs: Job[]): AddGroup[] {
  const seenByDest = new Map<string, Set<string>>();
  return groups.map((group) => {
    const { channel, show } = group.dest;
    const onAir = library.show.find((s) => s.channel === channel && s.folder === show)?.episode ?? [];
    const busy = jobs
      .filter((j) => j.state === "queued" || j.state === "running")
      .map((j) => describeTarget(j.target))
      .filter((t) => t.channel === channel && t.show === show && t.episode !== null);
    const seen = seenByDest.get(destKey(group.dest)) ?? new Set<string>();
    seenByDest.set(destKey(group.dest), seen);
    const rows = group.rows.map((row): AddRow => {
      if (!OWN_NOTES.includes(row.note)) return row;
      const key = row.episode === null ? null : `${row.season ?? 0}x${row.episode}`;
      let note: RowNote = null;
      let replaces: string | null = null;
      if (key !== null) {
        const season = row.season ?? 0;
        const existing = row.season !== null && row.season >= 1 ? onAir.find((e) => e.season === season && e.episode === row.episode) : undefined;
        if (seen.has(key)) note = "in-batch";
        else if (busy.some((t) => (t.season ?? 0) === season && t.episode === row.episode)) note = "on-air";
        else if (existing !== undefined) {
          note = "on-air";
          replaces = existing.path;
        }
      }
      let { ticked, held } = row;
      if (note === null) {
        if (held) ticked = true;
        held = false;
      } else if (note !== row.note || replaces !== row.replaces) {
        held = held || ticked;
        ticked = false;
      }
      if (!canTick({ note, replaces })) ticked = false;
      if (ticked && key !== null) seen.add(key);
      return { ...row, note, replaces, ticked, held };
    });
    return { ...group, rows };
  });
}

export function markExisting(group: AddGroup, library: WebLibrary, jobs: Job[]): AddGroup {
  return markAll([group], library, jobs)[0]!;
}

export function folderProblem(name: string): string | null {
  const trimmed = name.trim();
  if (trimmed === "") return "Give it a name.";
  if (/[/\\]/.test(name)) return "A name can't contain / or \\.";
  const safe = fatSafe(trimmed);
  if (safe === "" || safe.startsWith(".")) return "A name can't start with a dot.";
  if (safe.toLowerCase() === "ads") return "“Ads” is kept for ads. Pick another name.";
  return null;
}

export function showNameFor(dest: Destination, library: WebLibrary): string {
  if (!dest.isNewShow) return "";
  const wanted = dest.show.trim().toLowerCase();
  const existing = library.show.some((s) => s.channel === dest.channel && s.folder.toLowerCase() === wanted);
  return existing ? "" : dest.show.trim();
}

export function summary(rows: AddRow[]): { ticked: number; total: number; bytes: number } {
  const ticked = rows.filter((r) => r.ticked);
  return { ticked: ticked.length, total: rows.length, bytes: ticked.reduce((sum, r) => sum + (r.size ?? 0), 0) };
}
