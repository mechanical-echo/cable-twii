import { chmod, mkdtemp, readFile, rm, stat, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { afterEach, beforeEach, expect, test } from "vitest";
import { ConversionQueue } from "../src/server/queue.js";
import { readSources } from "../src/server/sources.js";
import type { convert, probe } from "../src/server/ffmpeg.js";
import type { Job, SourceRecord } from "../src/server/types.js";

let dir: string;
let srcFile: string;

beforeEach(async () => {
  dir = await mkdtemp(path.join(os.tmpdir(), "studio-queue-"));
  srcFile = path.join(dir, "original.mkv");
  await writeFile(srcFile, "12345");
});

afterEach(async () => {
  await chmod(path.join(dir, ".studio"), 0o700).catch(() => undefined);
  await rm(dir, { recursive: true, force: true });
});

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));
const probeFn: typeof probe = async () => ({ durationMs: 4242, width: 640, height: 480, displayAspect: 4 / 3, videoStreamIndex: 0 });
const quick: typeof convert = async () => {
  await sleep(10);
};

function idle(q: ConversionQueue, count: number): Promise<void> {
  return new Promise((resolve) => {
    const check = () => {
      const jobs = q.list();
      if (jobs.length >= count && jobs.every((j) => j.state === "done" || j.state === "failed")) resolve();
    };
    q.on("change", check);
    check();
  });
}

test("queue runs jobs with concurrency 2", async () => {
  let running = 0;
  let max = 0;
  const convertFn: typeof convert = async () => {
    running++;
    max = Math.max(max, running);
    await sleep(10);
    running--;
  };
  const q = new ConversionQueue(dir, { concurrency: 2, convertFn, probeFn });
  q.start();
  for (const n of ["a", "b", "c"]) q.enqueue(srcFile, `${n}.mpg`, "4:3");
  await idle(q, 3);
  expect(max).toBe(2);
  expect(q.list().map((j) => j.state)).toEqual(["done", "done", "done"]);
  await q.stop();
});

test("queue failure marks job failed", async () => {
  const convertFn: typeof convert = async () => {
    throw new Error("boom");
  };
  const q = new ConversionQueue(dir, { concurrency: 1, convertFn, probeFn });
  q.start();
  q.enqueue(srcFile, "a.mpg", "16:9");
  await idle(q, 1);
  expect(q.list()[0]).toMatchObject({ state: "failed", error: "boom" });
  await q.stop();
});

test("queue_resume_after_restart", async () => {
  const q = new ConversionQueue(dir, { convertFn: quick, probeFn });
  q.enqueue(srcFile, "a.mpg", "16:9");
  q.enqueue(srcFile, "b.mpg", "4:3");
  await q.stop();
  const text = await readFile(path.join(dir, ".studio", "queue.toml"), "utf8");
  expect(text).toContain("version = 1");
  expect(text).toContain("[[job]]");
  const again = new ConversionQueue(dir, { convertFn: quick, probeFn });
  expect(again.list().map((j) => [j.target, j.aspect, j.state])).toEqual([
    ["a.mpg", "16:9", "queued"],
    ["b.mpg", "4:3", "queued"],
  ]);
});

test("queue records source on success", async () => {
  const converted: SourceRecord[] = [];
  const q = new ConversionQueue(dir, { concurrency: 1, convertFn: quick, probeFn });
  q.on("converted", (r: SourceRecord) => converted.push(r));
  q.start();
  q.enqueue(srcFile, "Show/a.mpg", "4:3");
  await idle(q, 1);
  const recs = await readSources(dir);
  const st = await stat(srcFile);
  expect(recs).toEqual([{ target: "Show/a.mpg", source: srcFile, size: 5, mtimeMs: Math.trunc(st.mtimeMs), durationMs: 4242 }]);
  expect(converted).toEqual(recs);
  await q.stop();
});

test("done jobs leave queue.toml", async () => {
  const q = new ConversionQueue(dir, { concurrency: 1, convertFn: quick, probeFn });
  q.start();
  q.enqueue(srcFile, "a.mpg", "4:3");
  await idle(q, 1);
  await q.stop();
  expect(await readFile(path.join(dir, ".studio", "queue.toml"), "utf8")).not.toContain("[[job]]");
});

test("rejects target outside root", () => {
  const q = new ConversionQueue(dir, { convertFn: quick, probeFn });
  expect(() => q.enqueue(srcFile, "/abs/a.mpg", "4:3")).toThrow();
  expect(() => q.enqueue(srcFile, "../a.mpg", "4:3")).toThrow();
  expect(() => q.enqueue(srcFile, "x/../../a.mpg", "4:3")).toThrow();
  expect(q.list()).toEqual([]);
});

test("duplicate enqueue returns existing job", () => {
  const q = new ConversionQueue(dir, { convertFn: quick, probeFn });
  const first = q.enqueue(srcFile, "a.mpg", "4:3");
  const second = q.enqueue(srcFile, "a.mpg", "4:3");
  expect(second.id).toBe(first.id);
  expect(q.list()).toHaveLength(1);
});

test("stop requeues running jobs and persists them", async () => {
  const convertFn: typeof convert = (_s, _d, _a, _p, signal) =>
    new Promise((_resolve, reject) => {
      signal?.addEventListener("abort", () => reject(new Error("conversion aborted")));
    });
  const q = new ConversionQueue(dir, { concurrency: 2, convertFn, probeFn });
  q.start();
  q.enqueue(srcFile, "a.mpg", "4:3");
  q.enqueue(srcFile, "b.mpg", "4:3");
  await sleep(30);
  expect(q.list().map((j) => j.state)).toEqual(["running", "running"]);
  await q.stop();
  expect(q.list().map((j) => j.state)).toEqual(["queued", "queued"]);
  const again = new ConversionQueue(dir, { convertFn: quick, probeFn });
  expect(again.list().map((j) => j.target)).toEqual(["a.mpg", "b.mpg"]);
});

test("parallel successes all recorded in sources.toml", async () => {
  const q = new ConversionQueue(dir, { concurrency: 3, convertFn: quick, probeFn });
  q.start();
  for (let i = 0; i < 5; i++) q.enqueue(srcFile, `t${i}.mpg`, "4:3");
  await idle(q, 5);
  await q.stop();
  expect((await readSources(dir)).map((r) => r.target)).toEqual(["t0.mpg", "t1.mpg", "t2.mpg", "t3.mpg", "t4.mpg"]);
});

test("queue write failure emits error and continues", async () => {
  const q = new ConversionQueue(dir, { concurrency: 1, convertFn: quick, probeFn });
  const errors: Error[] = [];
  q.on("error", (e: Error) => errors.push(e));
  q.enqueue(srcFile, "warm.mpg", "4:3");
  await q.stop();
  await chmod(path.join(dir, ".studio"), 0o500);
  try {
    q.start();
    q.enqueue(srcFile, "b.mpg", "4:3");
    await idle(q, 2);
    expect(errors.length).toBeGreaterThan(0);
    expect(q.list().find((j) => j.target === "b.mpg")?.state).toBe("done");
  } finally {
    await chmod(path.join(dir, ".studio"), 0o700);
    await q.stop();
  }
});

test("queue survives write failure without an error listener", async () => {
  const q = new ConversionQueue(dir, { convertFn: quick, probeFn });
  q.enqueue(srcFile, "warm.mpg", "4:3");
  await q.stop();
  await chmod(path.join(dir, ".studio"), 0o500);
  try {
    expect(() => q.enqueue(srcFile, "b.mpg", "4:3")).not.toThrow();
    await q.stop();
  } finally {
    await chmod(path.join(dir, ".studio"), 0o700);
  }
});

test("progress events throttled", async () => {
  const convertFn: typeof convert = async (_s, _d, _a, onProgress) => {
    for (let i = 1; i <= 100; i++) onProgress(i / 100);
  };
  const q = new ConversionQueue(dir, { concurrency: 1, convertFn, probeFn });
  const events: Job[] = [];
  q.on("change", (j: Job) => {
    if (j.state === "running") events.push(j);
  });
  q.start();
  q.enqueue(srcFile, "a.mpg", "4:3");
  await idle(q, 1);
  await q.stop();
  expect(events.length).toBeLessThanOrEqual(3);
  expect(events.length).toBeGreaterThanOrEqual(1);
});
