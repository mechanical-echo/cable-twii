import type { Item, LibAd, Mode } from "../../server/types.js";
import type { WebLibrary } from "../tree.js";

export type RuleKind = "random" | "random-run" | "fill";

export type LibraryEntry =
  | { kind: "episode"; channel: string; show: string; season: number; episode: number }
  | { kind: "rule"; rule: RuleKind; channel: string; show: string; season: number }
  | { kind: "ads" }
  | { kind: "ad"; path: string };

const RULE_LABELS: Record<RuleKind, string> = {
  random: "Random episode",
  "random-run": "Random sequential run",
  fill: "Fill slot"
};

export function ruleLabel(rule: RuleKind): string {
  return RULE_LABELS[rule];
}

export function rulesFor(mode: Mode): RuleKind[] {
  return mode === "looping" ? ["random", "random-run"] : ["random", "random-run", "fill"];
}

function withFrom<T extends Item & { show: string }>(item: T, channel: string, ownFolder: string): T {
  return channel === ownFolder ? item : { ...item, from: channel };
}

export function itemFromEntry(entry: LibraryEntry, ownFolder: string): Item {
  switch (entry.kind) {
    case "episode":
      return withFrom({ type: "episode", show: entry.show, season: entry.season, episode: entry.episode }, entry.channel, ownFolder);
    case "rule": {
      const season = entry.season > 0 ? { season: entry.season } : {};
      const item: Item =
        entry.rule === "fill" ? { type: "fill", show: entry.show, ...season } : { type: entry.rule, show: entry.show, ...season, count: 1 };
      return withFrom(item as Item & { show: string }, entry.channel, ownFolder);
    }
    case "ads":
      return { type: "ads", count: 1 };
    case "ad":
      return { type: "ad", path: entry.path };
  }
}

export function entryKey(entry: LibraryEntry): string {
  switch (entry.kind) {
    case "episode":
      return `ep|${entry.channel}|${entry.show}|${entry.season}|${entry.episode}`;
    case "rule":
      return `rule|${entry.rule}|${entry.channel}|${entry.show}|${entry.season}`;
    case "ads":
      return "ads";
    case "ad":
      return `ad|${entry.path}`;
  }
}

function pad(value: number): string {
  return String(value).padStart(2, "0");
}

export function entryLabel(entry: LibraryEntry): string {
  switch (entry.kind) {
    case "episode":
      return `${entry.show} S${pad(entry.season)}E${pad(entry.episode)}`;
    case "rule":
      return `${ruleLabel(entry.rule)} · ${entry.show}${entry.season > 0 ? ` S${entry.season}` : ""}`;
    case "ads":
      return "Random ad";
    case "ad":
      return entry.path.slice(entry.path.lastIndexOf("/") + 1);
  }
}

export function adsFor(library: Pick<WebLibrary, "ad">, folder: string): LibAd[] {
  const own = library.ad.filter((a) => a.channel === folder);
  const shared = library.ad.filter((a) => a.channel === "");
  return [...own, ...shared];
}

export function seasonsOf(library: Pick<WebLibrary, "show">, folder: string, show: string): number[] {
  const found = library.show.find((s) => s.channel === folder && s.folder === show);
  if (!found) return [];
  return [...new Set(found.episode.map((e) => e.season))].sort((a, b) => a - b);
}
