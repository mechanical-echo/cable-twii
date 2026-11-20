import type { Config, Issue, Job, LineupResult } from "../server/types.js";
import { effectiveFolder, iconPath, sortChannels } from "./channels.js";
import { describeIssue } from "./failure.js";
import { formatEpisodeCode } from "./preview.js";
import type { WebLibrary } from "./tree.js";

export interface ChannelSummary {
  number: number;
  name: string;
  folder: string;
  icon: string;
  mode: "timed" | "looping";
  shows: number;
  episodes: number;
  now: string | null;
  warnings: string[];
}

export type EpisodeStatus = { state: "on-air" } | { state: "queued" } | { state: "running"; progress: number } | { state: "failed"; error: string };

export function channelSummaries(config: Config, library: WebLibrary, issues: Issue[]): ChannelSummary[] {
  return sortChannels(config.channel).map((channel) => {
    const folder = effectiveFolder(channel);
    const shows = library.show.filter((s) => s.channel === folder);
    const own = issues.filter((i) => i.channel === channel.number);
    const warnings = [
      ...(shows.length === 0 ? ["No shows yet"] : []),
      ...own.filter((i) => i.level === "error").map(describeIssue),
      ...own.filter((i) => i.level === "warning").map(describeIssue),
    ];
    return {
      number: channel.number,
      name: channel.name,
      folder,
      icon: iconPath(channel),
      mode: channel.mode,
      shows: shows.length,
      episodes: shows.reduce((sum, s) => sum + s.episode.length, 0),
      now: null,
      warnings,
    };
  });
}

export function nowLine(lineup: LineupResult, ms: number): string | null {
  const airing = lineup.airings.find((a) => a.start_ms <= ms && ms < a.start_ms + a.duration_ms);
  if (airing === undefined) return null;
  if (airing.kind === "ad") return "Ad break";
  if (airing.kind === "upnext") return "Up next";
  const code = formatEpisodeCode(airing.season, airing.episode);
  const name = [airing.show, code].filter((part) => part !== null && part !== "").join(" ");
  if (airing.title === null || airing.title === "") return name === "" ? null : name;
  return name === "" ? airing.title : `${name} · ${airing.title}`;
}

export function orphanFolders(config: Config, library: WebLibrary): string[] {
  const known = new Set(config.channel.map(effectiveFolder));
  const orphans = new Set(library.show.map((s) => s.channel).filter((c) => c !== "" && !known.has(c)));
  return [...orphans].sort((a, b) => a.localeCompare(b));
}

export function episodeStatus(path: string, jobs: Job[]): EpisodeStatus {
  const own = jobs.filter((j) => j.target === path);
  const busy = own.filter((j) => j.state === "queued" || j.state === "running");
  const latest = busy[busy.length - 1];
  if (latest !== undefined) return latest.state === "running" ? { state: "running", progress: latest.progress } : { state: "queued" };
  const lastDone = Math.max(-Infinity, ...own.filter((j) => j.state === "done").map((j) => j.finishedAt ?? 0));
  const failed = own.filter((j) => j.state === "failed" && (j.finishedAt ?? 0) > lastDone).pop();
  return failed !== undefined ? { state: "failed", error: failed.error ?? "Conversion failed" } : { state: "on-air" };
}
