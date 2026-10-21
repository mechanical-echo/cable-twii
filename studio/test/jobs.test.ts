import { describe, expect, it } from "vitest";
import { mergeResync } from "../src/web/jobs.js";
import type { Job } from "../src/server/types.js";

const job = (id: string, state: Job["state"], progress = 0): Job => ({ id, source: `/s/${id}`, target: `t/${id}`, aspect: "16:9", state, progress });
const map = (...jobs: Job[]) => new Map(jobs.map((j) => [j.id, j] as const));

describe("mergeResync", () => {
  it("keeps a newer streamed state over a stale snapshot", () => {
    const result = mergeResync(map(job("a", "done", 1)), [job("a", "running", 0.4)], new Set(["a"]));
    expect(result.jobs.get("a")?.state).toBe("done");
    expect(result.finished).toBe(false);
  });

  it("takes the fetched state for jobs with no streamed event", () => {
    const result = mergeResync(map(job("a", "running", 0.2)), [job("a", "running", 0.6)], new Set());
    expect(result.jobs.get("a")?.progress).toBe(0.6);
  });

  it("drops jobs the server no longer lists unless the stream delivered them", () => {
    const current = map(job("old", "done", 1), job("fresh", "queued"));
    const result = mergeResync(current, [], new Set(["fresh"]));
    expect([...result.jobs.keys()]).toEqual(["fresh"]);
  });

  it("adds jobs only the server knows", () => {
    const result = mergeResync(new Map(), [job("n", "queued")], new Set());
    expect(result.jobs.has("n")).toBe(true);
    expect(result.finished).toBe(false);
  });

  it("reports finished when a known unfinished job turned done or failed unseen", () => {
    expect(mergeResync(map(job("a", "running")), [job("a", "done", 1)], new Set()).finished).toBe(true);
    expect(mergeResync(map(job("a", "queued")), [job("a", "failed")], new Set()).finished).toBe(true);
  });

  it("does not report finished for jobs already finished or first seen finished", () => {
    expect(mergeResync(map(job("a", "done", 1)), [job("a", "done", 1)], new Set()).finished).toBe(false);
    expect(mergeResync(new Map(), [job("a", "done", 1)], new Set()).finished).toBe(false);
  });
});
