import type { Config, Issue, Job, LineupResult } from "../server/types.js";
import { effectiveFolder, iconPath, sortChannels } from "./channels.js";
import { describeTarget } from "./episodes.js";
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

export interface PendingEpisode {
  show: string;
  season: number;
  episode: number | null;
  title: string;
  path: string;
}

export function pendingEpisodes(folder: string, library: WebLibrary, jobs: Job[]): PendingEpisode[] {
  const known = new Set(library.show.flatMap((s) => s.episode.map((e) => e.path)));
  const seen = new Set<string>();
  const pending: PendingEpisode[] = [];
  for (const job of jobs) {
    if (job.kind === "shrink" || known.has(job.target) || seen.has(job.target)) continue;
    const info = describeTarget(job.target);
    if (info.ad || info.show === null || info.channel !== folder) continue;
    if (episodeStatus(job.target, jobs).state === "on-air") continue;
    seen.add(job.target);
    pending.push({ show: info.show, season: info.season ?? 0, episode: info.episode, title: info.name.replace(/^S\d+E\d+ - /, ""), path: job.target });
  }
  return pending.sort((a, b) => a.show.localeCompare(b.show) || a.path.localeCompare(b.path));
}
