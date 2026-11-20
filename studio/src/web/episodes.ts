import { HISTORY_MS } from "../server/history.js";
import type { Job } from "../server/types.js";
import { formatEpisodeCode } from "./preview.js";

export interface TargetInfo {
  channel: string;
  show: string | null;
  season: number | null;
  episode: number | null;
  ad: boolean;
  name: string;
}

export type RowState = "uploading" | "queued" | "running" | "done" | "failed" | "missing" | "rejected";

export interface JobRow {
  key: string;
  state: RowState;
  title: string;
  channel: string;
  target: string | null;
  detail: string;
  progress: number | null;
  count: number;
  shrink: boolean;
  error: string | null;
  finishedAt: number | null;
}

export interface UploadTask {
  id: string;
  name: string;
  size: number;
  loaded: number;
  state: "waiting" | "uploading" | "importing" | "done" | "rejected";
  reason: string | null;
}

const SEASON_DIR = /^Season (\d+)$/;
const EPISODE_NAME = /^S(\d+)E(\d+)/;

export function describeTarget(target: string): TargetInfo {
  const parts = target.split("/");
  const file = parts[parts.length - 1] ?? "";
  const name = file.replace(/\.mpg$/i, "");
  if (parts[0] === "ads" && parts.length === 2) return { channel: "", show: null, season: null, episode: null, ad: true, name };
  if (parts[1] === "ads" && parts.length === 3) return { channel: parts[0]!, show: null, season: null, episode: null, ad: true, name };
  const show = parts.length >= 3 ? parts[1]! : null;
  const seasonDir = parts.length >= 4 ? SEASON_DIR.exec(parts[2]!) : null;
  const code = EPISODE_NAME.exec(name);
  return {
    channel: parts[0] ?? "",
    show,
    season: seasonDir ? Number(seasonDir[1]) : code ? Number(code[1]) : null,
    episode: code ? Number(code[2]) : null,
    ad: false,
    name,
  };
}

function groupKey(job: Job, info: TargetInfo): string {
  return [job.kind ?? "convert", info.channel, info.ad ? "\0ads" : (info.show ?? ""), info.season ?? ""].join("\0");
}

function singleTitle(info: TargetInfo): string {
  if (info.ad) return `Ad · ${info.name}`;
  if (info.show === null) return info.name;
  if (info.episode !== null) return `${info.show} ${formatEpisodeCode(info.season, info.episode)}`;
  return `${info.show} · ${info.name}`;
}

function groupTitle(infos: TargetInfo[]): string {
  const first = infos[0]!;
  if (infos.length === 1) return singleTitle(first);
  if (first.ad) return "Ads";
  const last = infos[infos.length - 1]!;
  if (first.show !== null && first.episode !== null && last.episode !== null) {
    return `${first.show} ${formatEpisodeCode(first.season, first.episode)}–E${String(last.episode).padStart(2, "0")}`;
  }
  return first.show ?? first.channel;
}

function percent(ratio: number): string {
  return `${Math.round(ratio * 100)}%`;
}

function collapse(jobs: Job[], state: "queued" | "done"): JobRow[] {
  const sorted = [...jobs].sort((a, b) => a.target.localeCompare(b.target));
  const runs: { key: string; jobs: Job[]; infos: TargetInfo[] }[] = [];
  for (const job of sorted) {
    const info = describeTarget(job.target);
    const key = groupKey(job, info);
    const last = runs[runs.length - 1];
    const previous = last?.infos[last.infos.length - 1];
    const follows = previous === undefined || previous.episode === null || info.episode === null || info.episode === previous.episode + 1;
    if (last !== undefined && last.key === key && follows) {
      last.jobs.push(job);
      last.infos.push(info);
    } else {
      runs.push({ key, jobs: [job], infos: [info] });
    }
  }
  return runs.map((run) => {
    const count = run.jobs.length;
    const shrink = run.jobs[0]!.kind === "shrink";
    const finished = run.jobs.map((j) => j.finishedAt).filter((t): t is number => t !== undefined);
    let detail = state === "queued" ? `${count} waiting` : `${count} on air`;
    if (shrink) detail = state === "queued" ? "Making smaller" : "Made smaller";
    return {
      key: count === 1 ? run.jobs[0]!.id : `group:${run.jobs[0]!.id}`,
      state,
      title: groupTitle(run.infos),
      channel: run.infos[0]!.channel,
      target: count === 1 ? run.jobs[0]!.target : null,
      detail,
      progress: null,
      count,
      shrink,
      error: null,
      finishedAt: finished.length > 0 ? Math.max(...finished) : null,
    };
  });
}

function uploadRow(task: UploadTask): JobRow {
  const progress = task.size > 0 ? task.loaded / task.size : 0;
  const detail = task.state === "waiting" ? "Waiting to upload" : task.state === "importing" ? "Adding" : percent(progress);
  return { key: `upload:${task.id}`, state: "uploading", title: task.name, channel: "", target: null, detail, progress, count: 1, shrink: false, error: null, finishedAt: null };
}

function supersedes(other: Job, failed: Job): boolean {
  if (other.id === failed.id || other.target !== failed.target) return false;
  if (other.state === "queued" || other.state === "running") return true;
  return other.state === "done" && (other.finishedAt ?? 0) >= (failed.finishedAt ?? 0);
}

export function homeRows(jobs: Job[], uploads: UploadTask[], now: number): { active: JobRow[]; recent: JobRow[] } {
  const since = now - HISTORY_MS;
  const running = jobs.filter((j) => j.state === "running").sort((a, b) => a.target.localeCompare(b.target));
  const active: JobRow[] = [
    ...uploads.filter((u) => u.state === "waiting" || u.state === "uploading" || u.state === "importing").map(uploadRow),
    ...running.map((job) => {
      const shrink = job.kind === "shrink";
      const info = describeTarget(job.target);
      return {
        key: job.id,
        state: "running" as const,
        title: singleTitle(info),
        channel: info.channel,
        target: job.target,
        detail: shrink ? "Making smaller" : percent(job.progress),
        progress: job.progress,
        count: 1,
        shrink,
        error: null,
        finishedAt: null,
      };
    }),
    ...collapse(jobs.filter((j) => j.state === "queued"), "queued"),
  ];

  const recentJobs = jobs.filter((j) => (j.state === "done" || j.state === "failed") && j.finishedAt !== undefined && j.finishedAt >= since);
  const newestDone = new Map<string, Job>();
  for (const job of recentJobs) {
    if (job.state !== "done") continue;
    const seen = newestDone.get(job.target);
    if (seen === undefined || (seen.finishedAt ?? 0) < (job.finishedAt ?? 0)) newestDone.set(job.target, job);
  }
  const failedRows: JobRow[] = recentJobs
    .filter((job) => job.state === "failed" && !jobs.some((other) => supersedes(other, job)))
    .map((job) => ({
      key: job.id,
      state: job.missing ? "missing" : "failed",
      title: singleTitle(describeTarget(job.target)),
      channel: describeTarget(job.target).channel,
      target: job.target,
      detail: job.missing ? "Add the file again" : "Failed",
      progress: null,
      count: 1,
      shrink: job.kind === "shrink",
      error: job.error ?? null,
      finishedAt: job.finishedAt ?? null,
    }));
  const rejected: JobRow[] = uploads
    .filter((u) => u.state === "rejected")
    .map((u) => ({ key: `upload:${u.id}`, state: "rejected", title: u.name, channel: "", target: null, detail: "Not added", progress: null, count: 1, shrink: false, error: u.reason, finishedAt: null }));
  const recent = [...collapse([...newestDone.values()], "done"), ...failedRows].sort((a, b) => (b.finishedAt ?? 0) - (a.finishedAt ?? 0));
  return { active, recent: [...rejected, ...recent] };
}
