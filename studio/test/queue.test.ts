import { chmod, lstat, mkdir, mkdtemp, readFile, readdir, rename, rm, stat, symlink, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { afterEach, beforeEach, expect, test } from "vitest";
import { spawnSync } from "node:child_process";
import { ConversionQueue } from "../src/server/queue.js";
import { readSources, writeSources } from "../src/server/sources.js";
import { convert as realConvert } from "../src/server/ffmpeg.js";
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

test("duplicate enqueue returns existing job", async () => {
  const q = new ConversionQueue(dir, { convertFn: quick, probeFn });
  const first = q.enqueue(srcFile, "a.mpg", "4:3");
  const second = q.enqueue(srcFile, "a.mpg", "4:3");
  expect(second.id).toBe(first.id);
  expect(q.list()).toHaveLength(1);
  await q.stop();
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
  await new Promise<void>((resolve) => {
    const check = () => {
      if (q.list().every((j) => j.state === "running")) resolve();
    };
    q.on("change", check);
    check();
  });
  await q.stop();
  expect(q.list().map((j) => j.state)).toEqual(["queued", "queued"]);
  const again = new ConversionQueue(dir, { convertFn: quick, probeFn });
  expect(again.list().map((j) => j.target)).toEqual(["a.mpg", "b.mpg"]);
});

function until(q: ConversionQueue, check: (jobs: Job[]) => boolean): Promise<void> {
  return new Promise((resolve) => {
    const run = () => {
      if (check(q.list())) resolve();
    };
    q.on("change", run);
    run();
  });
}

test("queue pauses while the root is gone and resumes when it is back", async () => {
  const root = path.join(dir, "root");
  const away = path.join(dir, "away");
  await mkdir(root);
  let release!: () => void;
  const gate = new Promise<void>((resolve) => {
    release = resolve;
  });
  let calls = 0;
  const convertFn: typeof convert = async (_s, dst) => {
    calls++;
    if (calls === 1) await gate;
    await writeFile(dst, "converted");
  };
  const q = new ConversionQueue(root, { concurrency: 1, convertFn, probeFn });
  const states: string[] = [];
  q.on("change", (j: Job) => states.push(j.state));
  q.start();
  for (const n of ["a", "b", "c"]) q.enqueue(srcFile, `Show/${n}.mpg`, "4:3");
  await until(q, (jobs) => jobs[0]?.state === "running");
  await sleep(30);
  const stored = await readFile(path.join(root, ".studio", "queue.toml"), "utf8");
  expect(stored.match(/\[\[job\]\]/g)).toHaveLength(3);
  await rename(root, away);
  release();
  await until(q, (jobs) => jobs[0]?.state === "queued");
  await sleep(30);
  expect(q.list().map((j) => j.state)).toEqual(["queued", "queued", "queued"]);
  expect(states).not.toContain("failed");
  expect(q.isPaused()).toBe(true);
  q.resume();
  expect(q.isPaused()).toBe(true);
  await rename(away, root);
  expect(await readFile(path.join(root, ".studio", "queue.toml"), "utf8")).toBe(stored);
  q.resume();
  expect(q.isPaused()).toBe(false);
  await idle(q, 3);
  expect(q.list().map((j) => j.state)).toEqual(["done", "done", "done"]);
  expect((await readdir(path.join(root, "Show"))).sort()).toEqual(["a.mpg", "b.mpg", "c.mpg"]);
  await q.stop();
});

test("retryFailed requeues failed jobs", async () => {
  let fail = true;
  const convertFn: typeof convert = async () => {
    if (fail) throw new Error("boom");
  };
  const q = new ConversionQueue(dir, { concurrency: 1, convertFn, probeFn });
  q.start();
  q.enqueue(srcFile, "a.mpg", "4:3");
  q.enqueue(srcFile, "b.mpg", "4:3");
  await idle(q, 2);
  expect(q.list().map((j) => j.state)).toEqual(["failed", "failed"]);
  fail = false;
  const retried = q.retryFailed();
  expect(retried.map((j) => [j.target, j.state])).toEqual([
    ["a.mpg", "queued"],
    ["b.mpg", "queued"],
  ]);
  await idle(q, 2);
  expect(q.list().map((j) => j.state)).toEqual(["done", "done"]);
  expect(q.retryFailed()).toEqual([]);
  await q.stop();
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

test.each(["a\\..\\..\\b.mpg", ".", "", ".studio/x.mpg", ".STUDIO/x.mpg", "Movie.mkv", "x/..", "./"])("rejects invalid target %j", (target) => {
  const q = new ConversionQueue(dir, { convertFn: quick, probeFn });
  expect(() => q.enqueue(srcFile, target, "4:3")).toThrow();
});

test("targets are normalised and deduplicated", async () => {
  const q = new ConversionQueue(dir, { convertFn: quick, probeFn });
  const a = q.enqueue(srcFile, "a.mpg", "4:3");
  expect(q.enqueue(srcFile, "./a.mpg", "4:3").id).toBe(a.id);
  expect(q.enqueue(srcFile, "x//a.mpg", "4:3").target).toBe("x/a.mpg");
  expect(q.enqueue(srcFile, "x\\a.mpg", "4:3").target).toBe("x/a.mpg");
  expect(q.list().map((j) => j.target)).toEqual(["a.mpg", "x/a.mpg"]);
  await q.stop();
});

test("stop then start runs each job exactly once", async () => {
  const calls: string[] = [];
  let hang = true;
  const convertFn: typeof convert = (_s, dst, _a, _p, signal) => {
    calls.push(path.basename(dst));
    if (!hang) return Promise.resolve();
    return new Promise((_resolve, reject) => {
      signal?.addEventListener("abort", () => reject(new Error("conversion aborted")));
    });
  };
  const q = new ConversionQueue(dir, { concurrency: 1, convertFn, probeFn });
  const running = new Promise<void>((resolve) => {
    q.on("change", (j: Job) => {
      if (j.state === "running") resolve();
    });
  });
  q.start();
  q.enqueue(srcFile, "a.mpg", "4:3");
  q.enqueue(srcFile, "b.mpg", "4:3");
  await running;
  const stops = [q.stop(), q.stop()];
  await Promise.all(stops);
  hang = false;
  calls.length = 0;
  q.start();
  await idle(q, 2);
  expect(calls.sort()).toEqual(["a.mpg", "b.mpg"]);
  await q.stop();
});

async function writeQueueFile(text: string): Promise<void> {
  await mkdir(path.join(dir, ".studio"), { recursive: true });
  await writeFile(path.join(dir, ".studio", "queue.toml"), text);
}

const jobToml = (id: string, target: string) => `[[job]]\nid = "${id}"\nsource = "/s.mkv"\ntarget = ${JSON.stringify(target)}\naspect = "4:3"\n`;

test("reload with duplicates gives no duplicates", async () => {
  await writeQueueFile(
    "version = 1\n" + jobToml("1", "a.mpg") + jobToml("1", "b.mpg") + jobToml("2", "./a.mpg") + jobToml("3", "c.mpg"),
  );
  const q = new ConversionQueue(dir, { convertFn: quick, probeFn });
  expect(q.list().map((j) => [j.id, j.target])).toEqual([["1", "a.mpg"], ["3", "c.mpg"]]);
  await q.stop();
});

test("loaded invalid job is dropped with error", async () => {
  await writeQueueFile("version = 1\n" + jobToml("1", "../evil.mpg") + jobToml("2", ".studio/x.mpg") + jobToml("3", "ok.mpg") + jobToml("4", "m.mkv"));
  const q = new ConversionQueue(dir, { convertFn: quick, probeFn });
  const errors: Error[] = [];
  q.on("error", (e: Error) => errors.push(e));
  await sleep(5);
  expect(q.list().map((j) => j.target)).toEqual(["ok.mpg"]);
  expect(errors.map((e) => e.message).join("\n")).toMatch(/evil/);
  expect(errors).toHaveLength(3);
  await q.stop();
  expect(await readFile(path.join(dir, ".studio", "queue.toml"), "utf8")).not.toContain("evil");
});

test("queue job whose target folder is a symlink out of root fails and writes nothing outside", async () => {
  const outside = await mkdtemp(path.join(os.tmpdir(), "studio-outside-"));
  try {
    await symlink(outside, path.join(dir, "Show"));
    let converted = false;
    const convertFn: typeof convert = async (_src, dst) => {
      converted = true;
      await writeFile(`${dst}.part`, "x");
    };
    const q = new ConversionQueue(dir, { concurrency: 1, convertFn, probeFn });
    q.start();
    q.enqueue(srcFile, "Show/Season 1/a.mpg", "4:3");
    await idle(q, 1);
    const job = q.list()[0]!;
    expect(job.state).toBe("failed");
    expect(job.error).toContain("outside the cable-twii folder");
    expect(converted).toBe(false);
    expect(await readdir(outside)).toEqual([]);
    await q.stop();
  } finally {
    await rm(outside, { recursive: true, force: true });
  }
});

function makeVideo(): string {
  const out = path.join(dir, "real.mkv");
  const r = spawnSync("ffmpeg", ["-y", "-v", "error", "-f", "lavfi", "-i", "testsrc2=s=320x240:d=1", "-f", "lavfi", "-i", "sine=d=1", "-shortest", out]);
  if (r.status !== 0) throw new Error(String(r.stderr));
  return out;
}

test("queue refuses a pre-planted .part symlink and never writes through it", async () => {
  const outside = await mkdtemp(path.join(os.tmpdir(), "studio-outside-"));
  try {
    const victim = path.join(outside, "victim");
    await writeFile(victim, "keep");
    await mkdir(path.join(dir, "Show"));
    await symlink(victim, path.join(dir, "Show", "a.mpg.part"));
    const q = new ConversionQueue(dir, { concurrency: 1, convertFn: realConvert, probeFn });
    q.start();
    q.enqueue(makeVideo(), "Show/a.mpg", "4:3");
    await idle(q, 1);
    expect(q.list()[0]).toMatchObject({ state: "failed" });
    expect(q.list()[0]!.error).toContain("not a regular file");
    expect(await readFile(victim, "utf8")).toBe("keep");
    expect(await readdir(outside)).toEqual(["victim"]);
    expect((await lstat(path.join(dir, "Show", "a.mpg.part"))).isSymbolicLink()).toBe(true);
    await q.stop();
  } finally {
    await rm(outside, { recursive: true, force: true });
  }
});

test("queue refuses a destination that is a symlink and writes nothing through it", async () => {
  const outside = await mkdtemp(path.join(os.tmpdir(), "studio-outside-"));
  try {
    const victim = path.join(outside, "victim");
    await writeFile(victim, "keep");
    await mkdir(path.join(dir, "Show"));
    await symlink(victim, path.join(dir, "Show", "a.mpg"));
    const q = new ConversionQueue(dir, { concurrency: 1, convertFn: realConvert, probeFn });
    q.start();
    q.enqueue(makeVideo(), "Show/a.mpg", "4:3");
    await idle(q, 1);
    expect(q.list()[0]!.state).toBe("failed");
    expect(q.list()[0]!.error).toContain("not a regular file");
    expect(await readFile(victim, "utf8")).toBe("keep");
    await q.stop();
  } finally {
    await rm(outside, { recursive: true, force: true });
  }
});

test("a symlinked .studio folder gets no queue or sources writes", async () => {
  const outside = await mkdtemp(path.join(os.tmpdir(), "studio-outside-"));
  try {
    await symlink(outside, path.join(dir, ".studio"));
    const q = new ConversionQueue(dir, { concurrency: 1, convertFn: quick, probeFn });
    q.start();
    q.enqueue(srcFile, "Show/a.mpg", "4:3");
    await idle(q, 1);
    await q.stop();
    await expect(writeSources(dir, [])).rejects.toThrow("cable-twii folder");
    expect(await readdir(outside)).toEqual([]);
  } finally {
    await rm(outside, { recursive: true, force: true });
  }
});
