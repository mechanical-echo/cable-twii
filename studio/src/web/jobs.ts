import type { Job } from "../server/types.js";

export interface ResyncResult {
  jobs: Map<string, Job>;
  finished: boolean;
}

function terminal(job: Job): boolean {
  return job.state === "done" || job.state === "failed";
}

export function mergeResync(current: Map<string, Job>, fetched: Job[], seenSinceFetch: Set<string>): ResyncResult {
  const jobs = new Map<string, Job>();
  let finished = false;
  for (const job of fetched) {
    const live = current.get(job.id);
    if (seenSinceFetch.has(job.id) && live !== undefined) {
      jobs.set(job.id, live);
      continue;
    }
    jobs.set(job.id, job);
    if (terminal(job) && live !== undefined && !terminal(live)) finished = true;
  }
  for (const [id, job] of current) {
    if (!jobs.has(id) && seenSinceFetch.has(id)) jobs.set(id, job);
  }
  return { jobs, finished };
}

export const STREAM_CLOSED = 2;

export function streamLost(readyState: number, checkSession: () => Promise<unknown>): boolean {
  const closed = readyState === STREAM_CLOSED;
  if (closed) checkSession().catch(() => undefined);
  return closed;
}

export function streamProblem(closed: boolean): string {
  return closed ? "Lost connection to job updates. Reload the page to reconnect." : "Lost connection to job updates. Reconnecting…";
}
