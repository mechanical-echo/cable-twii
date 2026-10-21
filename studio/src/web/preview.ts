import type { Airing, LibShow } from "../server/types.js";

export const DAY_MS = 86_400_000;

export interface EpisodeRow {
  kind: "episode" | "upnext";
  key: string;
  startMs: number;
  endMs: number;
  airing: Airing;
}

export interface AdRow {
  kind: "ad";
  key: string;
  startMs: number;
  endMs: number;
  count: number;
  totalMs: number;
}

export type GuideRow = EpisodeRow | AdRow;

export function formatClock(ms: number): string {
  const day = Math.floor(ms / DAY_MS);
  const minutes = Math.floor((ms - day * DAY_MS) / 60_000);
  const hh = String(Math.floor(minutes / 60)).padStart(2, "0");
  const mm = String(minutes % 60).padStart(2, "0");
  return day > 0 ? `${hh}:${mm} +${day}` : `${hh}:${mm}`;
}

export function formatEpisodeCode(season: number | null, episode: number | null): string {
  if (episode === null) return "";
  const e = `E${String(episode).padStart(2, "0")}`;
  if (season === null || season === 0) return e;
  return `S${String(season).padStart(2, "0")}${e}`;
}

export function groupAirings(airings: Airing[]): GuideRow[] {
  const rows: GuideRow[] = [];
  for (const airing of airings) {
    const endMs = airing.start_ms + airing.duration_ms;
    const last = rows[rows.length - 1];
    if (airing.kind === "ad") {
      if (last?.kind === "ad") {
        last.endMs = endMs;
        last.count += 1;
        last.totalMs += airing.duration_ms;
        continue;
      }
      rows.push({ kind: "ad", key: `ad-${airing.start_ms}`, startMs: airing.start_ms, endMs, count: 1, totalMs: airing.duration_ms });
      continue;
    }
    rows.push({ kind: airing.kind, key: `${airing.kind}-${airing.start_ms}`, startMs: airing.start_ms, endMs, airing });
  }
  return rows;
}

export function adBreakLabel(row: Pick<AdRow, "count">, duration: string): string {
  return `Ad break (${row.count} ${row.count === 1 ? "ad" : "ads"}, ${duration})`;
}

export function findShow(shows: LibShow[], channelFolder: string, folder: string | null): LibShow | null {
  if (folder === null) return null;
  return shows.find((s) => s.channel === channelFolder && s.folder === folder) ?? shows.find((s) => s.folder === folder) ?? null;
}

export function localDate(now: Date): string {
  const y = String(now.getFullYear()).padStart(4, "0");
  const m = String(now.getMonth() + 1).padStart(2, "0");
  const d = String(now.getDate()).padStart(2, "0");
  return `${y}-${m}-${d}`;
}

export function localMs(now: Date): number {
  return ((now.getHours() * 60 + now.getMinutes()) * 60 + now.getSeconds()) * 1000 + now.getMilliseconds();
}

export function nowPosition(rows: GuideRow[], ms: number): { index: number; current: boolean } | null {
  if (rows.length === 0) return null;
  const index = rows.findIndex((row) => ms < row.endMs);
  if (index < 0) return { index: rows.length, current: false };
  return { index, current: rows[index].startMs <= ms };
}
