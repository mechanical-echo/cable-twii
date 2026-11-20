import { existsSync } from "node:fs";
import { cp, mkdir, mkdtemp, readFile, readdir, realpath, rename, rm, stat, symlink, utimes, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { Readable } from "node:stream";
import sharp from "sharp";
import { afterEach, beforeEach, expect, test } from "vitest";
import type { FastifyInstance, InjectOptions } from "fastify";
import { buildApp, type AppOptions } from "../src/server/app.js";
import { parseConfig } from "../src/server/config.js";
import type { convert } from "../src/server/ffmpeg.js";
import { ConversionQueue } from "../src/server/queue.js";
import { schedBinPath } from "../src/server/sched.js";
import { writeOff } from "../src/server/off.js";
import { readShowMeta } from "../src/server/showMeta.js";
import type { WebLibrary } from "../src/web/tree.js";
import { readSources, writeSources } from "../src/server/sources.js";
import type { Issue, Job, LineupResult } from "../src/server/types.js";
import { DRIVE_PROFILE, type VideoProfile } from "../src/server/videoSettings.js";
import { fakeConvert, fakeProbe, multipart, waitFor } from "./helpers.js";

const FIXTURE = path.resolve(import.meta.dirname, "../../engine/tests/fixtures/root_basic");

let dir: string;
let drive: string;
let root: string;
let app: FastifyInstance;

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

function bare(extra: Partial<AppOptions> = {}): FastifyInstance {
  return buildApp({
    schedBin: schedBinPath(),
    settingsPath: path.join(dir, "settings.json"),
    queueFactory: (r, profile) => new ConversionQueue(r, { convertFn: fakeConvert, probeFn: fakeProbe, concurrency: 1, profile }),
    probeFn: fakeProbe,
    rebuildDelayMs: 20,
    webDir: path.join(dir, "no-web"),
    ...extra,
  });
}

function make(extra: Partial<AppOptions> = {}): FastifyInstance {
  const built = bare(extra);
  const inject = built.inject.bind(built);
  const trusted = (options: InjectOptions) => {
    const address = built.server.address();
    const port = typeof address === "object" && address !== null ? address.port : 4747;
    return inject({ ...options, headers: { host: `127.0.0.1:${port}`, "x-cable-twii": "1", ...options.headers } });
  };
  built.inject = trusted as typeof built.inject;
  return built;
}

beforeEach(async () => {
  dir = await realpath(await mkdtemp(path.join(os.tmpdir(), "studio-app-")));
  drive = path.join(dir, "drive");
  root = path.join(drive, "cable-twii");
  await mkdir(drive);
  app = make();
});

afterEach(async () => {
  await app.close();
  await rm(dir, { recursive: true, force: true });
});

async function useFixture(): Promise<void> {
  await cp(FIXTURE, root, { recursive: true });
  const res = await app.inject({ method: "POST", url: "/api/drive", payload: { path: drive } });
  expect(res.statusCode).toBe(200);
}

async function getConfig(): Promise<{ config: ReturnType<typeof parseConfig>; mtimeMs: number }> {
  const res = await app.inject({ method: "GET", url: "/api/config" });
  expect(res.statusCode).toBe(200);
  return res.json();
}

async function getShowMeta(showFolder: string): Promise<{ meta: { name?: string; episodes: Record<string, string> }; mtimeMs: number }> {
  const res = await app.inject({ method: "GET", url: "/api/show-meta", query: { channelFolder: "Cartoon Network", showFolder } });
  expect(res.statusCode).toBe(200);
  return res.json();
}

async function png(width: number, height: number): Promise<Buffer> {
  return sharp({ create: { width, height, channels: 4, background: { r: 0, g: 128, b: 255, alpha: 1 } } }).png().toBuffer();
}

test("requests with a foreign Host are refused", async () => {
  const guarded = bare();
  try {
    const res = await guarded.inject({ method: "GET", url: "/api/drive", headers: { host: "evil.example:4747" } });
    expect(res.statusCode).toBe(403);
    expect(typeof res.json().error).toBe("string");
    const post = await guarded.inject({ method: "POST", url: "/api/drive/init", headers: { host: "evil.example", "x-cable-twii": "1" } });
    expect(post.statusCode).toBe(403);
  } finally {
    await guarded.close();
  }
});

test("writes with a foreign Origin are refused", async () => {
  const guarded = bare();
  try {
    await mkdir(root);
    const res = await guarded.inject({
      method: "POST",
      url: "/api/drive",
      headers: { host: "127.0.0.1:4747", "x-cable-twii": "1", origin: "http://evil.example" },
      payload: { path: drive },
    });
    expect(res.statusCode).toBe(403);
    expect((await guarded.inject({ method: "GET", url: "/api/drive", headers: { host: "127.0.0.1:4747" } })).body).toBe("null");
  } finally {
    await guarded.close();
  }
});

test("writes without the studio header are refused", async () => {
  const guarded = bare();
  try {
    const res = await guarded.inject({ method: "POST", url: "/api/drive/init", headers: { host: "127.0.0.1:4747" } });
    expect(res.statusCode).toBe(403);
    expect(typeof res.json().error).toBe("string");
  } finally {
    await guarded.close();
  }
});

test("allowed hosts, origins and the dev proxy get through", async () => {
  const guarded = bare();
  try {
    await mkdir(root);
    expect((await guarded.inject({ method: "GET", url: "/api/drive", headers: { host: "localhost:4747" } })).statusCode).toBe(200);
    const direct = await guarded.inject({
      method: "POST",
      url: "/api/drive",
      headers: { host: "127.0.0.1:4747", "x-cable-twii": "1", origin: "http://127.0.0.1:4747" },
      payload: { path: drive },
    });
    expect(direct.statusCode).toBe(200);
    const proxied = await guarded.inject({
      method: "POST",
      url: "/api/drive",
      headers: { host: "localhost:5173", "x-cable-twii": "1", origin: "http://localhost:5173" },
      payload: { path: drive },
    });
    expect(proxied.statusCode).toBe(200);
    const noOrigin = await guarded.inject({ method: "POST", url: "/api/drive", headers: { host: "localhost:4747", "x-cable-twii": "1" }, payload: { path: drive } });
    expect(noOrigin.statusCode).toBe(200);
  } finally {
    await guarded.close();
  }
});

test("the allowed host follows the server port", async () => {
  const guarded = bare({ port: 5000 });
  try {
    expect((await guarded.inject({ method: "GET", url: "/api/drive", headers: { host: "127.0.0.1:5000" } })).statusCode).toBe(200);
    expect((await guarded.inject({ method: "GET", url: "/api/drive", headers: { host: "127.0.0.1:4747" } })).statusCode).toBe(403);
  } finally {
    await guarded.close();
  }
});

test("GET /api/config", async () => {
  await useFixture();
  const body = await getConfig();
  expect(body.config.version).toBe(1);
  expect(body.config.channel.map((c) => c.number)).toEqual([1, 2]);
  expect(body.mtimeMs).toBe(Math.trunc((await stat(path.join(root, "config.toml"))).mtimeMs));
});

test("GET /api/config on a root without config returns a fresh one", async () => {
  await mkdir(root);
  await app.inject({ method: "POST", url: "/api/drive", payload: { path: drive } });
  const body = await getConfig();
  expect(body).toMatchObject({ mtimeMs: 0, config: { version: 1, channel: [] } });
});

test("put_config_rejects_invalid", async () => {
  await useFixture();
  const before = await readFile(path.join(root, "config.toml"), "utf8");
  const { config, mtimeMs } = await getConfig();
  config.channel[0]!.slot![0]!.items.push({ type: "random", show: "Nope", count: 1 });
  const res = await app.inject({ method: "PUT", url: "/api/config", payload: { config, mtimeMs } });
  expect(res.statusCode).toBe(422);
  const body = res.json() as { error: string; issues: Issue[] };
  expect(body.error).toBe("invalid");
  expect(body.issues[0]!.code).toBe("unknown_show");
  expect(await readFile(path.join(root, "config.toml"), "utf8")).toBe(before);
});

test("PUT /api/config sees a show imported moments ago", async () => {
  await app.close();
  app = make({ rebuildDelayMs: 60000 });
  await useFixture();
  const source = path.join(dir, "fresh.mkv");
  await writeFile(source, "video");
  const imported = await app.inject({
    method: "POST",
    url: "/api/import/show",
    payload: { channelFolder: "Cartoon Network", showFolder: "Fresh Show", episodes: [{ source, season: 1, episode: 1, title: "One", description: "", aspect: "4:3" }] },
  });
  expect(imported.statusCode).toBe(200);
  await waitFor(async () => ((await app.inject({ method: "GET", url: "/api/jobs" })).json() as Job[]).every((j) => j.state === "done"));
  const { config, mtimeMs } = await getConfig();
  config.channel[0]!.slot![0]!.items.push({ type: "random", show: "Fresh Show", count: 1 });
  const res = await app.inject({ method: "PUT", url: "/api/config", payload: { config, mtimeMs } });
  expect(res.statusCode).toBe(200);
});

test("PUT /api/config conflict", async () => {
  await useFixture();
  const { config, mtimeMs } = await getConfig();
  const later = new Date(mtimeMs + 5000);
  await utimes(path.join(root, "config.toml"), later, later);
  const res = await app.inject({ method: "PUT", url: "/api/config", payload: { config, mtimeMs } });
  expect(res.statusCode).toBe(409);
  expect(typeof res.json().error).toBe("string");
});

test("PUT /api/config saves, keeps a backup and returns warnings only", async () => {
  await useFixture();
  const before = await readFile(path.join(root, "config.toml"), "utf8");
  const { config, mtimeMs } = await getConfig();
  config.channel[0]!.category = 'Kids "fun"\tzone \u{1F4FA}';
  const res = await app.inject({ method: "PUT", url: "/api/config", payload: { config, mtimeMs } });
  expect(res.statusCode).toBe(200);
  const body = res.json() as { mtimeMs: number; issues: Issue[] };
  expect(Number.isInteger(body.mtimeMs)).toBe(true);
  expect(body.issues.every((i) => i.level === "warning")).toBe(true);
  expect(await readFile(path.join(root, "config.bak.toml"), "utf8")).toBe(before);
  expect((await getConfig()).config.channel[0]!.category).toBe('Kids "fun"\tzone \u{1F4FA}');
});

test("PUT /api/config with a malformed body is 400", async () => {
  await useFixture();
  const { mtimeMs } = await getConfig();
  const res = await app.inject({ method: "PUT", url: "/api/config", payload: { config: { version: 1, seed: "x", channel: [] }, mtimeMs } });
  expect(res.statusCode).toBe(400);
  expect(res.json().error).toMatch(/seed/);
  const noMtime = await app.inject({ method: "PUT", url: "/api/config", payload: { config: { version: 1, seed: 1, channel: [] } } });
  expect(noMtime.statusCode).toBe(400);
});

test("POST /api/config/reshuffle sets a new seed", async () => {
  await useFixture();
  const { config, mtimeMs } = await getConfig();
  const res = await app.inject({ method: "POST", url: "/api/config/reshuffle", payload: { mtimeMs } });
  expect(res.statusCode).toBe(200);
  const body = res.json() as { mtimeMs: number; seed: number };
  expect(body.seed).not.toBe(config.seed);
  expect((await getConfig()).config.seed).toBe(body.seed);
  const stale = await app.inject({ method: "POST", url: "/api/config/reshuffle", payload: { mtimeMs: mtimeMs - 10000 } });
  expect(stale.statusCode).toBe(409);
});

test("GET /api/drive is null before a drive is chosen", async () => {
  const res = await app.inject({ method: "GET", url: "/api/drive" });
  expect(res.statusCode).toBe(200);
  expect(res.json()).toBeNull();
});

test("POST /api/drive remembers a usable folder and init creates the root", async () => {
  const res = await app.inject({ method: "POST", url: "/api/drive", payload: { path: drive } });
  expect(res.statusCode).toBe(200);
  expect(res.json()).toMatchObject({ path: drive, usable: true, hasRoot: false });
  expect(JSON.parse(await readFile(path.join(dir, "settings.json"), "utf8"))).toEqual({ drive });
  const again = make();
  const remembered = await again.inject({ method: "GET", url: "/api/drive" });
  await again.close();
  expect(remembered.json()).toMatchObject({ path: drive });
  const init = await app.inject({ method: "POST", url: "/api/drive/init" });
  expect(init.statusCode).toBe(200);
  expect(init.json()).toMatchObject({ hasRoot: true, usable: true });
  expect((await stat(path.join(root, ".studio"))).isDirectory()).toBe(true);
  const done = await app.inject({ method: "POST", url: "/api/drive/done" });
  expect(done.json()).toEqual({ ok: true, skipped: true });
});

test("done refuses while conversions are running and does not run dot_clean", async () => {
  await app.close();
  const calls: string[][] = [];
  let release!: () => void;
  const gate = new Promise<void>((resolve) => {
    release = resolve;
  });
  const gated: typeof convert = async (src, dst, aspect, onProgress, signal) => {
    await gate;
    await fakeConvert(src, dst, aspect, onProgress, signal);
  };
  app = make({
    queueFactory: (r) => new ConversionQueue(r, { convertFn: gated, probeFn: fakeProbe, concurrency: 1 }),
    inspectDeps: {
      findMountPoint: async () => ({ mountPoint: drive, external: true }),
      runDiskutil: async () => ({ FilesystemType: "msdos", FilesystemName: "MS-DOS FAT32" }),
      run: async (cmd, args) => void calls.push([cmd, ...args]),
    },
  });
  await useFixture();
  const files = ["one.mov", "two.mov"].map((name) => ({ source: path.join(dir, name), aspect: "4:3" }));
  for (const file of files) await writeFile(file.source, "video");
  expect((await app.inject({ method: "POST", url: "/api/import/ads", payload: { channelFolder: "", files } })).statusCode).toBe(200);
  const busy = await app.inject({ method: "POST", url: "/api/drive/done" });
  expect(busy.statusCode).toBe(409);
  expect(busy.json()).toEqual({ error: "2 conversions still running" });
  expect(calls).toEqual([]);
  release();
  const jobs = async () => (await app.inject({ method: "GET", url: "/api/jobs" })).json() as Job[];
  await waitFor(async () => (await jobs()).every((j) => j.state === "done"));
  const done = await app.inject({ method: "POST", url: "/api/drive/done" });
  expect(done.json()).toEqual({ ok: true });
  expect(calls).toEqual([["dot_clean", "-m", drive]]);
});

test("POST /api/drive with a missing path is not remembered", async () => {
  const res = await app.inject({ method: "POST", url: "/api/drive", payload: { path: path.join(dir, "missing") } });
  expect(res.statusCode).toBe(200);
  expect(res.json()).toMatchObject({ usable: false });
  expect((await app.inject({ method: "GET", url: "/api/drive" })).json()).toBeNull();
  const bad = await app.inject({ method: "POST", url: "/api/drive", payload: { path: 5 } });
  expect(bad.statusCode).toBe(400);
});

test("drive refuses games disk via API", async () => {
  await mkdir(path.join(drive, "wbfs"));
  const res = await app.inject({ method: "POST", url: "/api/drive", payload: { path: drive } });
  expect(res.statusCode).toBe(200);
  expect(res.json()).toMatchObject({ usable: false, hasWbfs: true });
  expect((await app.inject({ method: "GET", url: "/api/drive" })).json()).toMatchObject({ usable: false });
  const init = await app.inject({ method: "POST", url: "/api/drive/init" });
  expect(init.statusCode).toBe(400);
  expect(init.json().error).toMatch(/games drive/);
  await expect(stat(root)).rejects.toThrow();
});

test("drive init without a drive is 409", async () => {
  const res = await app.inject({ method: "POST", url: "/api/drive/init" });
  expect(res.statusCode).toBe(409);
  expect(typeof res.json().error).toBe("string");
});

test("routes return 409 with no drive", async () => {
  const routes: [string, string][] = [
    ["GET", "/api/config"],
    ["GET", "/api/library"],
    ["POST", "/api/library/rebuild"],
    ["GET", "/api/jobs"],
    ["POST", "/api/validate"],
    ["GET", "/api/preview?channel=1&date=2026-10-20"],
    ["GET", "/media/x.png"],
    ["POST", "/api/drive/done"],
  ];
  for (const [method, url] of routes) {
    const res = await app.inject({ method: method as "GET", url });
    expect(res.statusCode, url).toBe(409);
    expect(typeof res.json().error, url).toBe("string");
  }
});

test("routes return 409 when the drive has no cable-twii folder", async () => {
  await app.inject({ method: "POST", url: "/api/drive", payload: { path: drive } });
  const res = await app.inject({ method: "GET", url: "/api/config" });
  expect(res.statusCode).toBe(409);
});

test("drive pick uses the picker", async () => {
  const picked = make({ pickFolderFn: async () => drive });
  const res = await picked.inject({ method: "POST", url: "/api/drive/pick" });
  expect(res.json()).toMatchObject({ path: drive });
  const cancelled = make({ pickFolderFn: async () => null, settingsPath: path.join(dir, "other.json") });
  const none = await cancelled.inject({ method: "POST", url: "/api/drive/pick" });
  expect(none.statusCode).toBe(200);
  expect(none.json()).toBeNull();
  const source = await cancelled.inject({ method: "POST", url: "/api/import/pick-source" });
  expect(source.json()).toBeNull();
  await picked.close();
  await cancelled.close();
});

test("pick-source scans the picked folder", async () => {
  const src = path.join(dir, "src");
  await mkdir(src);
  const picked = make({ pickFolderFn: async () => src });
  const res = await picked.inject({ method: "POST", url: "/api/import/pick-source" });
  await picked.close();
  expect(res.json()).toEqual({ dir: src, files: [] });
});

test("picked folder marks files converted before", async () => {
  await useFixture();
  const src = path.join(dir, "src");
  await mkdir(src);
  await writeFile(path.join(src, "S01E01 Old.mp4"), "video");
  const picked = make({ pickFolderFn: async () => src });
  const episodes = [{ source: path.join(src, "S01E01 Old.mp4"), season: 1, episode: 1, title: "Old", description: "", aspect: "4:3" }];
  const imported = await picked.inject({ method: "POST", url: "/api/import/show", payload: { channelFolder: "Cartoon Network", showFolder: "Picked", episodes } });
  expect(imported.statusCode).toBe(200);
  await waitFor(async () => ((await picked.inject({ method: "GET", url: "/api/jobs" })).json() as Job[]).every((j) => j.state === "done"));
  await waitFor(async () => (await readSources(root)).some((r) => r.source === path.join(src, "S01E01 Old.mp4")));
  await writeFile(path.join(src, "S01E02 New.mp4"), "video");
  const res = await picked.inject({ method: "POST", url: "/api/import/pick-source" });
  await picked.close();
  const files = res.json().files as { source: string; added?: boolean }[];
  expect(files.map((f) => [path.basename(f.source), f.added])).toEqual([
    ["S01E01 Old.mp4", true],
    ["S01E02 New.mp4", undefined],
  ]);
});

test("serves a file from a picked folder", async () => {
  const src = path.join(dir, "src");
  await mkdir(src);
  await writeFile(path.join(src, "x.mp4"), "video");
  const picked = make({ pickFolderFn: async () => src });
  const before = await picked.inject({ method: "GET", url: "/api/import/file", query: { path: path.join(src, "x.mp4") } });
  expect(before.statusCode).toBe(404);
  await picked.inject({ method: "POST", url: "/api/import/pick-source" });
  const res = await picked.inject({ method: "GET", url: "/api/import/file", query: { path: path.join(src, "x.mp4") } });
  await picked.close();
  expect(res.statusCode).toBe(200);
  expect(res.body).toBe("video");
});

const OLD_EPISODE = "Cartoon Network/Adventure Time/Season 5/S05E02 - Time 2.mpg";

function replaceBody(source: string, replaces: string) {
  return { channelFolder: "Cartoon Network", showFolder: "Adventure Time", episodes: [{ source, season: 5, episode: 2, title: "Time 2", description: "", aspect: "4:3", replaces }] };
}

function withConvert(convertFn: typeof convert): FastifyInstance {
  return make({ queueFactory: (r, profile) => new ConversionQueue(r, { convertFn, probeFn: fakeProbe, concurrency: 1, profile }) });
}

async function jobsOf(target: FastifyInstance): Promise<Job[]> {
  return (await target.inject({ method: "GET", url: "/api/jobs" })).json() as Job[];
}

test("replace converts onto the existing file", async () => {
  await useFixture();
  const oldFile = path.join(root, ...OLD_EPISODE.split("/"));
  const old = await readFile(oldFile);
  let release = () => {};
  const gate = new Promise<void>((resolve) => (release = resolve));
  const replacing = withConvert(async (_s, dst) => {
    await gate;
    await writeFile(dst, "new bytes");
  });
  try {
    const src = path.join(dir, "new.mkv");
    await writeFile(src, "video");
    const res = await replacing.inject({ method: "POST", url: "/api/import/show", payload: replaceBody(src, OLD_EPISODE) });
    expect(res.statusCode).toBe(200);
    expect((res.json() as { jobs: Job[] }).jobs.map((j) => j.target)).toEqual([OLD_EPISODE]);
    await waitFor(async () => (await jobsOf(replacing)).some((j) => j.state === "running"));
    expect(await readFile(oldFile)).toEqual(old);
    release();
    await waitFor(async () => (await jobsOf(replacing)).every((j) => j.state === "done"));
    expect(await readFile(oldFile, "utf8")).toBe("new bytes");
    expect(await readdir(path.dirname(oldFile))).not.toContain("S05E02 - Time 2 (2).mpg");
  } finally {
    release();
    await replacing.close();
  }
});

test("replace outside the show is refused", async () => {
  await useFixture();
  const src = path.join(dir, "new.mkv");
  await writeFile(src, "video");
  for (const replaces of [
    "mtv-00s/Music Videos/Artist 1 - Song 1.mpg",
    "../x.mpg",
    "Cartoon Network/Adventure Time/../../mtv-00s/Music Videos/Artist 1 - Song 1.mpg",
    "Cartoon Network/Adventure Time/Season 5/S05E99 - Missing.mpg",
    "Cartoon Network/Adventure Time/Season 5",
  ]) {
    const res = await app.inject({ method: "POST", url: "/api/import/show", payload: replaceBody(src, replaces) });
    expect(res.statusCode, replaces).toBe(400);
    expect(res.json().error).toBe("replaces must be an episode of this show.");
  }
  expect(await jobsOf(app)).toEqual([]);
});

test("failed replace keeps the old file", async () => {
  await useFixture();
  const oldFile = path.join(root, ...OLD_EPISODE.split("/"));
  const old = await readFile(oldFile);
  const replacing = withConvert(async (_s, dst) => {
    await writeFile(`${dst}.part`, "half");
    throw new Error("boom");
  });
  try {
    const src = path.join(dir, "new.mkv");
    await writeFile(src, "video");
    expect((await replacing.inject({ method: "POST", url: "/api/import/show", payload: replaceBody(src, OLD_EPISODE) })).statusCode).toBe(200);
    await waitFor(async () => (await jobsOf(replacing)).every((j) => j.state === "failed"));
    expect(await readFile(oldFile)).toEqual(old);
  } finally {
    await replacing.close();
  }
});

test("no rebuild runs after close", async () => {
  await useFixture();
  let release = () => {};
  const gate = new Promise<void>((resolve) => (release = resolve));
  const closing = withConvert(async (_s, dst) => {
    await gate;
    await writeFile(dst, "converted");
  });
  const src = path.join(dir, "late.mkv");
  await writeFile(src, "video");
  const episodes = [{ source: src, season: 1, episode: 9, title: "Late", description: "", aspect: "4:3" }];
  expect((await closing.inject({ method: "POST", url: "/api/import/show", payload: { channelFolder: "Cartoon Network", showFolder: "Late Show", episodes } })).statusCode).toBe(200);
  await waitFor(async () => (await jobsOf(closing)).some((j) => j.state === "running"));
  const closed = closing.close();
  release();
  await closed;
  await rm(path.join(root, "library.toml"), { force: true });
  await sleep(150);
  expect(existsSync(path.join(root, "library.toml"))).toBe(false);
});

test("notes start empty", async () => {
  await useFixture();
  const res = await app.inject({ method: "GET", url: "/api/library/notes" });
  expect(res.statusCode).toBe(200);
  expect(res.json()).toEqual({ at: null, skipped: [], warnings: [] });
});

test("notes keep the last automatic rebuild", async () => {
  await useFixture();
  await writeFile(path.join(root, "ads", "short-x.mpg"), "tiny");
  const src = path.join(dir, "new.mkv");
  await writeFile(src, "video");
  const episodes = [{ source: src, season: 1, episode: 1, title: "New", description: "", aspect: "4:3" }];
  expect((await app.inject({ method: "POST", url: "/api/import/show", payload: { channelFolder: "Cartoon Network", showFolder: "Noted", episodes } })).statusCode).toBe(200);
  let notes: { at: number | null; skipped: { path: string; reason: string }[] } = { at: null, skipped: [] };
  await waitFor(async () => {
    notes = (await app.inject({ method: "GET", url: "/api/library/notes" })).json();
    return notes.at !== null;
  });
  expect(typeof notes.at).toBe("number");
  expect(notes.skipped).toContainEqual({ path: "ads/short-x.mpg", reason: "duration 500 ms is under 1000 ms" });
});

const EPISODE = "Cartoon Network/Adventure Time/Season 5/S05E02 - Time 2.mpg";

function fakeRemux() {
  const calls: { file: string; start: number; stopped: boolean }[] = [];
  const remuxFn = (file: string, start: number) => {
    const call = { file, start, stopped: false };
    calls.push(call);
    return { stream: Readable.from([Buffer.from("ts")]), stop: () => (call.stopped = true) };
  };
  return { calls, remuxFn };
}

test("play streams the remux of a library file", async () => {
  await useFixture();
  const fake = fakeRemux();
  const playing = make({ remuxFn: fake.remuxFn });
  try {
    const res = await playing.inject({ method: "GET", url: "/api/media/play", query: { path: EPISODE } });
    expect(res.statusCode).toBe(200);
    expect(res.body).toBe("ts");
    expect(res.headers["content-type"]).toBe("video/mp2t");
    expect(fake.calls).toHaveLength(1);
    expect(fake.calls[0]).toMatchObject({ file: await realpath(path.join(root, ...EPISODE.split("/"))), start: 0 });
  } finally {
    await playing.close();
  }
});

test("play passes the start time", async () => {
  await useFixture();
  const fake = fakeRemux();
  const playing = make({ remuxFn: fake.remuxFn });
  try {
    expect((await playing.inject({ method: "GET", url: "/api/media/play", query: { path: EPISODE, start: "95" } })).statusCode).toBe(200);
    expect(fake.calls[0]!.start).toBe(95);
    for (const start of ["-1", "x", "1.5"]) {
      expect((await playing.inject({ method: "GET", url: "/api/media/play", query: { path: EPISODE, start } })).statusCode, start).toBe(400);
    }
  } finally {
    await playing.close();
  }
});

test("play refuses paths outside the library", async () => {
  await useFixture();
  const fake = fakeRemux();
  const playing = make({ remuxFn: fake.remuxFn });
  const outside = path.join(dir, "outside.mpg");
  await writeFile(outside, "video");
  await symlink(outside, path.join(root, "ads", "link.mpg"));
  try {
    for (const rel of ["../outside.mpg", "ads/link.mpg", "config.toml", "Cartoon Network/missing.mpg", ""]) {
      expect((await playing.inject({ method: "GET", url: "/api/media/play", query: { path: rel } })).statusCode, rel).toBe(404);
    }
    expect(fake.calls).toEqual([]);
  } finally {
    await playing.close();
  }
});

test("play stops ffmpeg when the response closes", async () => {
  await useFixture();
  const fake = fakeRemux();
  const playing = make({ remuxFn: fake.remuxFn });
  try {
    await playing.inject({ method: "GET", url: "/api/media/play", query: { path: EPISODE } });
    expect(fake.calls[0]!.stopped).toBe(true);
  } finally {
    await playing.close();
  }
});

test("library lists off items after a rebuild", async () => {
  await useFixture();
  await writeOff(root, new Set([EPISODE]));
  expect((await app.inject({ method: "POST", url: "/api/library/rebuild" })).statusCode).toBe(200);
  const library = (await app.inject({ method: "GET", url: "/api/library" })).json() as WebLibrary;
  expect(library.show.flatMap((s) => s.episode).map((e) => e.path)).not.toContain(EPISODE);
  expect(library.off!.show.flatMap((s) => s.episode).map((e) => e.path)).toEqual([EPISODE]);
});

async function edit(target: FastifyInstance, action: unknown, extra: Record<string, unknown> = {}) {
  const { mtimeMs } = (await target.inject({ method: "GET", url: "/api/config" })).json() as { mtimeMs: number };
  return target.inject({ method: "POST", url: "/api/library/edit", payload: { mtimeMs, action, ...extra } });
}

const BEN = "Cartoon Network/Ben 10/Season 1/S01E05 - Ben Ten.mpg";
const SEASON5 = [1, 2, 3, 4, 5].map((n) => `Cartoon Network/Adventure Time/Season 5/S05E0${n} - Time ${n}.mpg`);

test("renames an episode and its picks follow", async () => {
  await useFixture();
  const res = await edit(app, { kind: "episode", path: "mtv-00s/Music Videos/Artist 1 - Song 1.mpg", title: "Zed", season: null, episode: null });
  expect(res.statusCode).toBe(200);
  expect(existsSync(path.join(root, "mtv-00s", "Music Videos", "Zed.mpg"))).toBe(true);
  expect(res.json().moves).toEqual([{ from: "mtv-00s/Music Videos/Artist 1 - Song 1.mpg", to: "mtv-00s/Music Videos/Zed.mpg" }]);
  const config = (await getConfig()).config;
  expect(config.channel[1]!.loop![3]).toEqual({ type: "episode", show: "Music Videos", season: 0, episode: 2 });
});

test("an edit that adds an error is refused and changes nothing", async () => {
  await useFixture();
  const before = await readFile(path.join(root, "config.toml"), "utf8");
  const off = await edit(app, { kind: "off", paths: SEASON5, off: true });
  expect(off.statusCode).toBe(422);
  const issues = (off.json() as { issues: Issue[] }).issues;
  expect(issues.length).toBeGreaterThan(0);
  expect(issues.every((i) => i.level === "error" && i.channel === 1)).toBe(true);
  expect(existsSync(path.join(root, ".studio", "off.toml"))).toBe(false);
  const moved = await edit(app, { kind: "move-season", paths: SEASON5, season: 6 });
  expect(moved.statusCode).toBe(422);
  for (const file of SEASON5) expect(existsSync(path.join(root, ...file.split("/")))).toBe(true);
  expect(existsSync(path.join(root, "Cartoon Network", "Adventure Time", "Season 6"))).toBe(false);
  expect(await readFile(path.join(root, "config.toml"), "utf8")).toBe(before);
});

test("a converting file can't be edited", async () => {
  await useFixture();
  let release = () => {};
  const gate = new Promise<void>((resolve) => (release = resolve));
  const busy = withConvert(async (_s, dst) => {
    await gate;
    await writeFile(dst, "new");
  });
  try {
    const src = path.join(dir, "new.mkv");
    await writeFile(src, "video");
    await busy.inject({ method: "POST", url: "/api/import/show", payload: replaceBody(src, OLD_EPISODE) });
    await waitFor(async () => (await jobsOf(busy)).some((j) => j.state === "running"));
    const res = await edit(busy, { kind: "episode", path: OLD_EPISODE, title: "Renamed", season: 5, episode: 2 });
    expect(res.statusCode).toBe(409);
    expect(res.json().error).toBe("Still converting. Try again when it's done.");
  } finally {
    release();
    await busy.close();
  }
});

test("dry run changes nothing and lists removed items", async () => {
  await useFixture();
  const before = await readFile(path.join(root, "config.toml"), "utf8");
  const res = await edit(app, { kind: "off", paths: [BEN], off: true }, { dryRun: true });
  expect(res.statusCode).toBe(200);
  expect(res.json().removed).toEqual([{ channel: 1, label: "S01E05 · Ben 10" }]);
  expect(existsSync(path.join(root, ".studio", "off.toml"))).toBe(false);
  expect(await readFile(path.join(root, "config.toml"), "utf8")).toBe(before);
});

test("delete removes the file and its direct items", async () => {
  await useFixture();
  const res = await edit(app, { kind: "delete", paths: [BEN] });
  expect(res.statusCode).toBe(200);
  expect(existsSync(path.join(root, ...BEN.split("/")))).toBe(false);
  const items = (await getConfig()).config.channel[0]!.slot![0]!.items;
  expect(items.some((i) => i.type === "episode")).toBe(false);
});

test("switching off keeps the file and hides it from the library", async () => {
  await useFixture();
  expect((await edit(app, { kind: "off", paths: [BEN], off: true })).statusCode).toBe(200);
  expect(existsSync(path.join(root, ...BEN.split("/")))).toBe(true);
  const library = (await app.inject({ method: "GET", url: "/api/library" })).json() as WebLibrary;
  expect(library.show.some((s) => s.folder === "Ben 10")).toBe(false);
  expect(library.off!.show.map((s) => s.folder)).toEqual(["Ben 10"]);
});

test("conflict on stale mtime", async () => {
  await useFixture();
  const res = await app.inject({ method: "POST", url: "/api/library/edit", payload: { mtimeMs: 1, action: { kind: "off", paths: [BEN], off: true } } });
  expect(res.statusCode).toBe(409);
});

test("file facts give size and source", async () => {
  await useFixture();
  await writeSources(root, [{ target: EPISODE, source: "/videos/time2.mkv", size: 5, mtimeMs: 1, durationMs: 60000 }]);
  const res = await app.inject({ method: "GET", url: "/api/library/file", query: { path: EPISODE } });
  expect(res.statusCode).toBe(200);
  const facts = res.json() as { size: number; mtimeMs: number; source: string | null };
  expect(facts.size).toBe((await stat(path.join(root, ...EPISODE.split("/")))).size);
  expect(typeof facts.mtimeMs).toBe("number");
  expect(facts.source).toBe("/videos/time2.mkv");
  const plain = await app.inject({ method: "GET", url: "/api/library/file", query: { path: "ads/ad-a.mpg" } });
  expect(plain.json().source).toBeNull();
});

test("file facts refuse paths outside the library", async () => {
  await useFixture();
  for (const rel of ["../x.mpg", "config.toml", "missing.mpg"]) {
    expect((await app.inject({ method: "GET", url: "/api/library/file", query: { path: rel } })).statusCode, rel).toBe(404);
  }
});

test("sched lineup through GET /api/preview", async () => {
  await useFixture();
  const res = await app.inject({ method: "GET", url: "/api/preview?channel=1&date=2026-10-20" });
  expect(res.statusCode).toBe(200);
  const body = res.json() as LineupResult;
  expect(body.hash).toMatch(/^[0-9a-f]{16}$/);
  expect(body.airings.length).toBeGreaterThan(0);
});

test("preview with a bad query is 400", async () => {
  await useFixture();
  for (const query of ["", "?channel=1", "?date=2026-10-20", "?channel=0&date=2026-10-20", "?channel=01&date=2026-10-20", "?channel=x&date=2026-10-20", "?channel=1&date=2026-1-20", "?channel=1&date=2026-10-20T00"]) {
    const res = await app.inject({ method: "GET", url: `/api/preview${query}` });
    expect(res.statusCode, query).toBe(400);
    expect(typeof res.json().error).toBe("string");
  }
});

test("preview of a channel that is not in the config is 400", async () => {
  await useFixture();
  const res = await app.inject({ method: "GET", url: "/api/preview?channel=42&date=2026-10-20" });
  expect(res.statusCode).toBe(400);
});

test("preview on a broken config is 422", async () => {
  await useFixture();
  await writeFile(path.join(root, "config.toml"), "version = 1\nseed = ");
  const res = await app.inject({ method: "GET", url: "/api/preview?channel=1&date=2026-10-20" });
  expect(res.statusCode).toBe(422);
  expect((res.json() as { issues: Issue[] }).issues[0]!.code).toBe("parse");
});

test("GET /api/config on a malformed file is 422", async () => {
  await useFixture();
  await writeFile(path.join(root, "config.toml"), "version = 1\nseed = ");
  const res = await app.inject({ method: "GET", url: "/api/config" });
  expect(res.statusCode).toBe(422);
  expect(res.json().error).toMatch(/config.toml/);
});

test("POST /api/validate checks files", async () => {
  await useFixture();
  const ok = await app.inject({ method: "POST", url: "/api/validate" });
  expect(ok.json()).toMatchObject({ ok: true });
  await rm(path.join(root, "ads", "ad-a.mpg"));
  const missing = await app.inject({ method: "POST", url: "/api/validate" });
  expect(missing.statusCode).toBe(200);
  expect(missing.json().ok).toBe(false);
  expect((missing.json().issues as Issue[]).map((i) => i.code)).toContain("missing_file");
});

test("icon upload writes a 128x128 channel icon", async () => {
  await useFixture();
  const { payload, headers } = multipart({ kind: "channel", channelFolder: "Cartoon Network" }, { name: "logo.png", data: await png(500, 300) });
  const res = await app.inject({ method: "POST", url: "/api/icon", payload, headers });
  expect(res.statusCode).toBe(200);
  expect(res.json()).toEqual({ path: "Cartoon Network/channel.png" });
  const meta = await sharp(await readFile(path.join(root, "Cartoon Network", "channel.png"))).metadata();
  expect([meta.width, meta.height, meta.format]).toEqual([128, 128, "png"]);
});

test("show icon upload lands in the show folder and appears in the library", async () => {
  await useFixture();
  const { payload, headers } = multipart({ kind: "show", channelFolder: "Cartoon Network", showFolder: "Ben 10" }, { name: "b.png", data: await png(64, 64) });
  const res = await app.inject({ method: "POST", url: "/api/icon", payload, headers });
  expect(res.statusCode).toBe(200);
  expect(res.json()).toEqual({ path: "Cartoon Network/Ben 10/show.png" });
  await waitFor(async () => (await readFile(path.join(root, "library.toml"), "utf8")).includes('icon = "Cartoon Network/Ben 10/show.png"'));
});

test("icon upload with a non-image is 400", async () => {
  await useFixture();
  const { payload, headers } = multipart({ kind: "channel", channelFolder: "Cartoon Network" }, { name: "x.png", data: Buffer.from("not an image") });
  const res = await app.inject({ method: "POST", url: "/api/icon", payload, headers });
  expect(res.statusCode).toBe(400);
  expect(res.json().error).toMatch(/not an image/);
});

test("icon path traversal is 400", async () => {
  await useFixture();
  const image = await png(10, 10);
  const cases: Record<string, string>[] = [
    { kind: "channel", channelFolder: "../x" },
    { kind: "channel", channelFolder: ".." },
    { kind: "channel", channelFolder: "" },
    { kind: "show", channelFolder: "Cartoon Network", showFolder: "../../x" },
    { kind: "show", channelFolder: "Cartoon Network" },
    { kind: "show", channelFolder: "Cartoon Network", showFolder: ".studio" },
    { kind: "other", channelFolder: "Cartoon Network" },
  ];
  for (const fields of cases) {
    const { payload, headers } = multipart(fields, { name: "a.png", data: image });
    const res = await app.inject({ method: "POST", url: "/api/icon", payload, headers });
    expect(res.statusCode, JSON.stringify(fields)).toBe(400);
  }
  await expect(stat(path.join(drive, "x"))).rejects.toThrow();
  const { payload, headers } = multipart({ kind: "channel", channelFolder: "Cartoon Network" });
  expect((await app.inject({ method: "POST", url: "/api/icon", payload, headers })).statusCode).toBe(400);
});

test("icon upload over 10 MB is refused", async () => {
  await useFixture();
  const { payload, headers } = multipart({ kind: "channel", channelFolder: "Cartoon Network" }, { name: "big.png", data: Buffer.alloc(10 * 1024 * 1024 + 10) });
  const res = await app.inject({ method: "POST", url: "/api/icon", payload, headers });
  expect(res.statusCode).toBe(413);
  expect(typeof res.json().error).toBe("string");
});

test("media serves png files from the root", async () => {
  await useFixture();
  const data = await png(8, 8);
  await writeFile(path.join(root, "Cartoon Network", "channel.png"), data);
  const res = await app.inject({ method: "GET", url: "/media/Cartoon%20Network/channel.png" });
  expect(res.statusCode).toBe(200);
  expect(res.headers["content-type"]).toBe("image/png");
  expect(res.rawPayload.equals(data)).toBe(true);
});

test("media non-png is 404", async () => {
  await useFixture();
  for (const url of ["/media/config.toml", "/media/ads/ad-a.mpg", "/media/Cartoon%20Network", "/media/", "/media/missing.png"]) {
    const res = await app.inject({ method: "GET", url });
    expect(res.statusCode, url).toBe(404);
    expect(typeof res.json().error).toBe("string");
  }
});

test("media symlink escape is 404", async () => {
  await useFixture();
  await writeFile(path.join(dir, "secret.png"), await png(4, 4));
  await symlink(path.join(dir, "secret.png"), path.join(root, "link.png"));
  await symlink(dir, path.join(root, "outside"));
  for (const url of ["/media/link.png", "/media/outside/secret.png", "/media/../secret.png", "/media/%2E%2E/secret.png", "/media/..%2Fsecret.png"]) {
    const res = await app.inject({ method: "GET", url });
    expect(res.statusCode, url).toBe(404);
  }
});

test("import show enqueues episodes, writes show.toml and rejects bad sources", async () => {
  await useFixture();
  const src = path.join(dir, "src");
  await mkdir(src);
  const body = {
    channelFolder: "Cartoon: Network",
    showFolder: "New Show",
    showName: "The New Show",
    episodes: [
      { source: path.join(src, "a.mkv"), season: 1, episode: 1, title: "Pilot", description: "First one", aspect: "16:9" },
      { source: path.join(src, "b.mkv"), season: 1, episode: 1, title: "Pilot", description: "", aspect: "4:3" },
      { source: path.join(src, "short.mkv"), season: 1, episode: 2, title: "Tiny", description: "", aspect: "4:3" },
      { source: path.join(src, "broken.mkv"), season: 1, episode: 3, title: "Bad", description: "", aspect: "4:3" },
      { source: path.join(src, "c.mkv"), season: null, episode: null, title: "Special", description: "Extra", aspect: "4:3" },
    ],
  };
  const res = await app.inject({ method: "POST", url: "/api/import/show", payload: body });
  expect(res.statusCode).toBe(200);
  const out = res.json() as { jobs: Job[]; rejected: { source: string; reason: string }[] };
  expect(out.jobs.map((j) => j.target)).toEqual([
    "Cartoon- Network/New Show/Season 1/S01E01 - Pilot.mpg",
    "Cartoon- Network/New Show/Season 1/S01E01 - Pilot (2).mpg",
    "Cartoon- Network/New Show/Special.mpg",
  ]);
  expect(out.jobs[0]!.aspect).toBe("16:9");
  expect(out.rejected.map((r) => r.source)).toEqual([path.join(src, "short.mkv"), path.join(src, "broken.mkv")]);
  expect(out.rejected[0]!.reason).toMatch(/short/);
  expect(out.rejected[1]!.reason).toMatch(/no video stream/);
  const meta = await readFile(path.join(root, "Cartoon- Network", "New Show", "show.toml"), "utf8");
  expect(meta).toContain('name = "The New Show"');
  expect(meta).toContain('S01E01 = "First one"');
  expect(meta).toContain('Special = "Extra"');
  const jobs = await app.inject({ method: "GET", url: "/api/jobs" });
  expect((jobs.json() as Job[]).length).toBe(3);
  await waitFor(async () => {
    try {
      const lib = await readFile(path.join(root, "library.toml"), "utf8");
      return lib.includes("S01E01 - Pilot (2).mpg") && lib.includes("New Show/Special.mpg");
    } catch {
      return false;
    }
  });
  const library = await app.inject({ method: "GET", url: "/api/library" });
  const show = (library.json() as { show: { folder: string; name: string }[] }).show.find((s) => s.folder === "New Show");
  expect(show?.name).toBe("The New Show");
});

test("import show avoids names already on the drive", async () => {
  await useFixture();
  const res = await app.inject({
    method: "POST",
    url: "/api/import/show",
    payload: {
      channelFolder: "Cartoon Network",
      showFolder: "Gumball",
      showName: "",
      episodes: [{ source: path.join(dir, "x.mkv"), season: 1, episode: 1, title: "gumball 1", description: "", aspect: "4:3" }],
    },
  });
  expect((res.json() as { jobs: Job[] }).jobs[0]!.target).toBe("Cartoon Network/Gumball/Season 1/S01E01 - gumball 1 (2).mpg");
});

test("re-import skips sources already on the drive and reconverts changed ones in place", async () => {
  await useFixture();
  const src = path.join(dir, "src");
  await mkdir(src);
  const pilot = path.join(src, "pilot.mkv");
  const ad = path.join(src, "promo.mov");
  await writeFile(pilot, "pilot");
  await writeFile(ad, "promo");
  const showTarget = "Cartoon Network/Fresh/Season 1/S01E01 - Pilot.mpg";
  const adTarget = "ads/promo.mpg";
  for (const target of [showTarget, adTarget]) {
    await mkdir(path.dirname(path.join(root, target)), { recursive: true });
    await writeFile(path.join(root, target), "converted");
  }
  const record = async (source: string, target: string) => {
    const st = await stat(source);
    return { target, source, size: st.size, mtimeMs: Math.trunc(st.mtimeMs), durationMs: 60000 };
  };
  await writeSources(root, [await record(pilot, showTarget), await record(ad, adTarget)]);
  const showBody = {
    channelFolder: "Cartoon Network",
    showFolder: "Fresh",
    episodes: [{ source: pilot, season: 1, episode: 1, title: "Pilot", description: "", aspect: "4:3" }],
  };
  const adsBody = { channelFolder: "", files: [{ source: ad, aspect: "4:3" }] };
  const show = (await app.inject({ method: "POST", url: "/api/import/show", payload: showBody })).json() as { jobs: Job[]; rejected: { source: string; reason: string }[] };
  expect(show).toEqual({ jobs: [], rejected: [{ source: pilot, reason: "already on the drive" }] });
  const ads = (await app.inject({ method: "POST", url: "/api/import/ads", payload: adsBody })).json() as { jobs: Job[]; rejected: { source: string; reason: string }[] };
  expect(ads).toEqual({ jobs: [], rejected: [{ source: ad, reason: "already on the drive" }] });
  await writeFile(pilot, "pilot, recut");
  await writeFile(ad, "promo, recut");
  const changedShow = (await app.inject({ method: "POST", url: "/api/import/show", payload: showBody })).json() as { jobs: Job[]; rejected: unknown[] };
  expect(changedShow.rejected).toEqual([]);
  expect(changedShow.jobs.map((j) => j.target)).toEqual([showTarget]);
  const changedAds = (await app.inject({ method: "POST", url: "/api/import/ads", payload: adsBody })).json() as { jobs: Job[]; rejected: unknown[] };
  expect(changedAds.rejected).toEqual([]);
  expect(changedAds.jobs.map((j) => j.target)).toEqual([adTarget]);
});

test("import rejects a too-short source", async () => {
  await useFixture();
  const res = await app.inject({
    method: "POST",
    url: "/api/import/ads",
    payload: { channelFolder: "", files: [{ source: path.join(dir, "short-ad.mov"), aspect: "4:3" }, { source: path.join(dir, "Soda: Ad.mov"), aspect: "16:9" }] },
  });
  expect(res.statusCode).toBe(200);
  const out = res.json() as { jobs: Job[]; rejected: { source: string; reason: string }[] };
  expect(out.rejected).toEqual([{ source: path.join(dir, "short-ad.mov"), reason: expect.stringMatching(/short/) }]);
  expect(out.jobs.map((j) => j.target)).toEqual(["ads/Soda- Ad.mpg"]);
  const own = await app.inject({
    method: "POST",
    url: "/api/import/ads",
    payload: { channelFolder: "Cartoon Network", files: [{ source: path.join(dir, "ad-a.mov"), aspect: "4:3" }] },
  });
  expect((own.json() as { jobs: Job[] }).jobs.map((j) => j.target)).toEqual(["Cartoon Network/ads/ad-a.mpg"]);
});

test("import validates the body", async () => {
  await useFixture();
  const good = { source: path.join(dir, "a.mkv"), season: 1, episode: 1, title: "A", description: "", aspect: "4:3" };
  const bad: unknown[] = [
    { channelFolder: "", showFolder: "S", showName: "", episodes: [good] },
    { channelFolder: "C", showFolder: "..", showName: "", episodes: [good] },
    { channelFolder: "C", showFolder: "a/b", showName: "", episodes: [good] },
    { channelFolder: "C", showFolder: "S", showName: "", episodes: "x" },
    { channelFolder: "C", showFolder: "S", showName: "", episodes: [{ ...good, aspect: "1:1" }] },
    { channelFolder: "C", showFolder: "S", showName: "", episodes: [{ ...good, season: 1.5 }] },
    { channelFolder: "C", showFolder: "S", showName: "", episodes: [{ ...good, source: "relative.mkv" }] },
  ];
  for (const payload of bad) {
    const res = await app.inject({ method: "POST", url: "/api/import/show", payload: payload as object });
    expect(res.statusCode, JSON.stringify(payload)).toBe(400);
  }
  const ads = await app.inject({ method: "POST", url: "/api/import/ads", payload: { channelFolder: "..", files: [] } });
  expect(ads.statusCode).toBe(400);
});

test("POST /api/library/rebuild returns the build result", async () => {
  await useFixture();
  const res = await app.inject({ method: "POST", url: "/api/library/rebuild" });
  expect(res.statusCode).toBe(200);
  const body = res.json() as { library: { show: { folder: string }[] }; skipped: unknown[]; warnings: unknown[] };
  expect(body.library.show.map((s) => s.folder)).toContain("Gumball");
  expect(Array.isArray(body.skipped)).toBe(true);
  expect(Array.isArray(body.warnings)).toBe(true);
});

test("GET /api/show-meta returns meta and mtime", async () => {
  await useFixture();
  expect(await getShowMeta("Ben 10")).toEqual({ meta: { episodes: {} }, mtimeMs: 0 });
  const { mtimeMs } = await getShowMeta("Ben 10");
  const put = await app.inject({
    method: "PUT",
    url: "/api/show-meta",
    payload: { channelFolder: "Cartoon Network", showFolder: "Ben 10", name: "Ben Ten!", episodes: { S01E05: "Aliens" }, mtimeMs },
  });
  expect(put.statusCode).toBe(200);
  const saved = await getShowMeta("Ben 10");
  expect(saved.meta).toEqual({ name: "Ben Ten!", episodes: { S01E05: "Aliens" } });
  expect(saved.mtimeMs).toBe(Math.trunc((await stat(path.join(root, "Cartoon Network", "Ben 10", "show.toml"))).mtimeMs));
  const missing = await app.inject({ method: "GET", url: "/api/show-meta", query: { channelFolder: "Cartoon Network", showFolder: "Nope" } });
  expect(missing.statusCode).toBe(404);
  const noFolder = await app.inject({ method: "GET", url: "/api/show-meta", query: { channelFolder: "Cartoon Network" } });
  expect(noFolder.statusCode).toBe(400);
});

test("PUT /api/show-meta writes show.toml and returns the rebuilt library", async () => {
  await useFixture();
  const { mtimeMs } = await getShowMeta("Ben 10");
  const res = await app.inject({
    method: "PUT",
    url: "/api/show-meta",
    payload: { channelFolder: "Cartoon Network", showFolder: "Ben 10", name: "Ben Ten!", episodes: { S01E05: "Aliens" }, mtimeMs },
  });
  expect(res.statusCode).toBe(200);
  const show = (res.json() as { show: { folder: string; name: string; episode: { description: string }[] }[] }).show.find((s) => s.folder === "Ben 10");
  expect(show?.name).toBe("Ben Ten!");
  expect(show?.episode[0]!.description).toBe("Aliens");
  const missing = await app.inject({ method: "PUT", url: "/api/show-meta", payload: { channelFolder: "Cartoon Network", showFolder: "Nope", name: "", episodes: {}, mtimeMs: 0 } });
  expect(missing.statusCode).toBe(404);
  const bad = await app.inject({ method: "PUT", url: "/api/show-meta", payload: { channelFolder: "Cartoon Network", showFolder: "Ben 10", name: "", episodes: { S01E05: 3 }, mtimeMs: 0 } });
  expect(bad.statusCode).toBe(400);
  const noMtime = await app.inject({ method: "PUT", url: "/api/show-meta", payload: { channelFolder: "Cartoon Network", showFolder: "Ben 10", name: "", episodes: {} } });
  expect(noMtime.statusCode).toBe(400);
});

test("a damaged show.toml opens as empty meta and a save rewrites it", async () => {
  await useFixture();
  const file = path.join(root, "Cartoon Network", "Ben 10", "show.toml");
  await writeFile(file, "name = [[[");
  const loaded = await getShowMeta("Ben 10");
  expect(loaded).toEqual({ meta: { episodes: {} }, mtimeMs: Math.trunc((await stat(file)).mtimeMs) });
  const res = await app.inject({
    method: "PUT",
    url: "/api/show-meta",
    payload: { channelFolder: "Cartoon Network", showFolder: "Ben 10", name: "Ben Ten!", episodes: { S01E05: "Aliens" }, mtimeMs: loaded.mtimeMs },
  });
  expect(res.statusCode).toBe(200);
  expect(await readShowMeta(path.dirname(file))).toEqual({ name: "Ben Ten!", episodes: { S01E05: "Aliens" } });
});

test("show-meta conflict keeps the first save", async () => {
  await useFixture();
  const first = await getShowMeta("Ben 10");
  const second = await getShowMeta("Ben 10");
  const target = { channelFolder: "Cartoon Network", showFolder: "Ben 10" };
  const a = await app.inject({ method: "PUT", url: "/api/show-meta", payload: { ...target, name: "Name A", episodes: {}, mtimeMs: first.mtimeMs } });
  expect(a.statusCode).toBe(200);
  const b = await app.inject({ method: "PUT", url: "/api/show-meta", payload: { ...target, name: "Name B", episodes: {}, mtimeMs: second.mtimeMs } });
  expect(b.statusCode).toBe(409);
  expect(b.json()).toEqual({ error: "show.toml was changed somewhere else. Reload it and try again." });
  expect((await getShowMeta("Ben 10")).meta.name).toBe("Name A");
  expect(await readFile(path.join(root, "Cartoon Network", "Ben 10", "show.toml"), "utf8")).toContain('name = "Name A"');
});

test("show-meta conflict also catches an edit made outside Studio", async () => {
  await useFixture();
  const target = { channelFolder: "Cartoon Network", showFolder: "Ben 10" };
  const first = await app.inject({ method: "PUT", url: "/api/show-meta", payload: { ...target, name: "Name A", episodes: {}, mtimeMs: 0 } });
  expect(first.statusCode).toBe(200);
  const { mtimeMs } = await getShowMeta("Ben 10");
  const later = new Date(mtimeMs + 5000);
  await utimes(path.join(root, "Cartoon Network", "Ben 10", "show.toml"), later, later);
  const stale = await app.inject({ method: "PUT", url: "/api/show-meta", payload: { ...target, name: "Name B", episodes: {}, mtimeMs } });
  expect(stale.statusCode).toBe(409);
  const fresh = await app.inject({ method: "PUT", url: "/api/show-meta", payload: { ...target, name: "Name B", episodes: {}, mtimeMs: (await getShowMeta("Ben 10")).mtimeMs } });
  expect(fresh.statusCode).toBe(200);
});

test("POST /api/jobs/retry requeues failed conversions", async () => {
  await app.close();
  let fail = true;
  const flaky: typeof convert = async (src, dst, aspect, onProgress, signal) => {
    if (fail) throw new Error("boom");
    await fakeConvert(src, dst, aspect, onProgress, signal);
  };
  app = make({ queueFactory: (r) => new ConversionQueue(r, { convertFn: flaky, probeFn: fakeProbe, concurrency: 1 }) });
  await useFixture();
  await writeFile(path.join(dir, "x.mov"), "video");
  const imported = await app.inject({ method: "POST", url: "/api/import/ads", payload: { channelFolder: "", files: [{ source: path.join(dir, "x.mov"), aspect: "4:3" }] } });
  expect(imported.statusCode).toBe(200);
  const jobs = async () => (await app.inject({ method: "GET", url: "/api/jobs" })).json() as Job[];
  await waitFor(async () => (await jobs())[0]?.state === "failed");
  fail = false;
  const res = await app.inject({ method: "POST", url: "/api/jobs/retry" });
  expect(res.statusCode).toBe(200);
  expect((res.json() as { jobs: Job[] }).jobs.map((j) => [j.target, j.state])).toEqual([["ads/x.mpg", "queued"]]);
  await waitFor(async () => (await jobs())[0]?.state === "done");
});

test("the queue resumes when the drive comes back", async () => {
  await app.close();
  let release!: () => void;
  const gate = new Promise<void>((resolve) => {
    release = resolve;
  });
  let calls = 0;
  const gated: typeof convert = async (src, dst, aspect, onProgress, signal) => {
    calls++;
    if (calls === 1) await gate;
    await fakeConvert(src, dst, aspect, onProgress, signal);
  };
  app = make({ queueFactory: (r) => new ConversionQueue(r, { convertFn: gated, probeFn: fakeProbe, concurrency: 1 }) });
  await useFixture();
  const files = ["one.mov", "two.mov"].map((name) => ({ source: path.join(dir, name), aspect: "4:3" }));
  for (const file of files) await writeFile(file.source, "video");
  expect((await app.inject({ method: "POST", url: "/api/import/ads", payload: { channelFolder: "", files } })).statusCode).toBe(200);
  const jobs = async () => (await app.inject({ method: "GET", url: "/api/jobs" })).json() as Job[];
  await waitFor(async () => (await jobs())[0]?.state === "running");
  await sleep(30);
  const away = path.join(dir, "away");
  await rename(root, away);
  release();
  await sleep(50);
  expect((await app.inject({ method: "GET", url: "/api/jobs" })).statusCode).toBe(409);
  await rename(away, root);
  await waitFor(async () => (await jobs()).every((j) => j.state === "done"));
  expect((await readdir(path.join(root, "ads"))).filter((n) => n === "one.mpg" || n === "two.mpg").sort()).toEqual(["one.mpg", "two.mpg"]);
});

async function pausedQueue(holdAfterResume = false): Promise<{ queue: () => ConversionQueue; away: string; release: () => void }> {
  await app.close();
  let first!: () => void;
  const firstGate = new Promise<void>((resolve) => {
    first = resolve;
  });
  let later!: () => void;
  const laterGate = new Promise<void>((resolve) => {
    later = resolve;
  });
  let calls = 0;
  const gated: typeof convert = async (src, dst, aspect, onProgress, signal) => {
    calls++;
    if (calls === 1) await firstGate;
    else if (holdAfterResume) await laterGate;
    await fakeConvert(src, dst, aspect, onProgress, signal);
  };
  let created: ConversionQueue | undefined;
  app = make({
    queueFactory: (r) => {
      created = new ConversionQueue(r, { convertFn: gated, probeFn: fakeProbe, concurrency: 1 });
      return created;
    },
  });
  await useFixture();
  const files = ["one.mov", "two.mov"].map((name) => ({ source: path.join(dir, name), aspect: "4:3" }));
  for (const file of files) await writeFile(file.source, "video");
  expect((await app.inject({ method: "POST", url: "/api/import/ads", payload: { channelFolder: "", files } })).statusCode).toBe(200);
  const queue = () => created!;
  await waitFor(async () => queue().list()[0]?.state === "running");
  await sleep(30);
  const away = path.join(dir, "away");
  await rename(root, away);
  first();
  await waitFor(async () => queue().isPaused() && queue().list().every((j) => j.state === "queued"));
  return { queue, away, release: later };
}

test("replug with a symlinked cable-twii folder does not resume the queue", async () => {
  const { queue, away } = await pausedQueue();
  const before = await readFile(path.join(away, ".studio", "queue.toml"), "utf8");
  await symlink(away, root);
  const res = await app.inject({ method: "POST", url: "/api/drive", payload: { path: drive } });
  expect(res.statusCode).toBe(200);
  await sleep(50);
  expect(queue().isPaused()).toBe(true);
  expect(queue().list().map((j) => j.state)).toEqual(["queued", "queued"]);
  const ads = await readdir(path.join(away, "ads"));
  expect(ads.filter((n) => n === "one.mpg" || n === "two.mpg" || n.endsWith(".part"))).toEqual([]);
  expect(await readFile(path.join(away, ".studio", "queue.toml"), "utf8")).toBe(before);
});

test("done resumes a paused queue once the drive is back", async () => {
  const { queue, away } = await pausedQueue();
  await rename(away, root);
  const busy = await app.inject({ method: "POST", url: "/api/drive/done" });
  expect(busy.statusCode).toBe(409);
  expect(queue().isPaused()).toBe(false);
  await waitFor(async () => queue().list().every((j) => j.state === "done"));
  const done = await app.inject({ method: "POST", url: "/api/drive/done" });
  expect(done.json()).toEqual({ ok: true, skipped: true });
});

test("done after a resume still refuses while a job runs", async () => {
  const { queue, away, release } = await pausedQueue(true);
  await rename(away, root);
  const res = await app.inject({ method: "POST", url: "/api/drive/done" });
  expect(res.statusCode).toBe(409);
  expect(res.json()).toEqual({ error: "2 conversions still running" });
  expect(queue().list()[0]?.state).toBe("running");
  release();
  await waitFor(async () => queue().list().every((j) => j.state === "done"));
});

test("SSE emits a job change", async () => {
  await useFixture();
  await app.listen({ host: "127.0.0.1", port: 0 });
  const address = app.server.address() as { port: number };
  const controller = new AbortController();
  const res = await fetch(`http://127.0.0.1:${address.port}/api/jobs/stream`, { signal: controller.signal });
  expect(res.status).toBe(200);
  expect(res.headers.get("content-type")).toMatch(/^text\/event-stream/);
  const reader = res.body!.getReader();
  const decoder = new TextDecoder();
  let text = "";
  const imported = app.inject({
    method: "POST",
    url: "/api/import/ads",
    payload: { channelFolder: "", files: [{ source: path.join(dir, "stream.mov"), aspect: "4:3" }] },
  });
  while (!/^data: /m.test(text)) {
    const { value, done } = await reader.read();
    if (done) break;
    text += decoder.decode(value, { stream: true });
  }
  controller.abort();
  await imported;
  const line = text.split("\n").find((l) => l.startsWith("data: "))!;
  const job = JSON.parse(line.slice("data: ".length)) as Job;
  expect(job.target).toBe("ads/stream.mpg");
  expect(text).toMatch(/\n\n/);
});

test("unknown routes are 404 JSON", async () => {
  const res = await app.inject({ method: "GET", url: "/api/nope" });
  expect(res.statusCode).toBe(404);
  expect(res.json()).toEqual({ error: expect.any(String) });
});

async function symlinkedOutside(link: string): Promise<string> {
  const outside = path.join(dir, "outside");
  await mkdir(outside);
  await symlink(outside, link);
  return outside;
}

test("icon upload into a symlinked channel folder is 400 and writes nothing outside", async () => {
  await useFixture();
  const outside = await symlinkedOutside(path.join(root, "Linked"));
  const cases: Record<string, string>[] = [{ kind: "channel", channelFolder: "Linked" }, { kind: "show", channelFolder: "Linked", showFolder: "Show" }];
  for (const fields of cases) {
    const { payload, headers } = multipart(fields, { name: "logo.png", data: await png(20, 20) });
    const res = await app.inject({ method: "POST", url: "/api/icon", payload, headers });
    expect(res.statusCode).toBe(400);
    expect(res.json().error).toContain("outside the cable-twii folder");
  }
  expect(await readdir(outside)).toEqual([]);
});

test("import show into a symlinked channel or show folder is refused and writes nothing outside", async () => {
  await useFixture();
  const src = path.join(dir, "src");
  await mkdir(src);
  const outside = await symlinkedOutside(path.join(root, "Linked"));
  await mkdir(path.join(dir, "outside2"));
  await symlink(path.join(dir, "outside2"), path.join(root, "Cartoon Network", "LinkedShow"));
  for (const [channelFolder, showFolder] of [["Linked", "New Show"], ["Cartoon Network", "LinkedShow"]]) {
    const body = { channelFolder, showFolder, showName: "X", episodes: [{ source: path.join(src, "a.mkv"), season: 1, episode: 1, title: "Pilot", description: "D", aspect: "4:3" }] };
    const res = await app.inject({ method: "POST", url: "/api/import/show", payload: body });
    expect(res.statusCode).toBe(400);
  }
  await sleep(100);
  expect(await readdir(outside)).toEqual([]);
  expect(await readdir(path.join(dir, "outside2"))).toEqual([]);
  expect(((await app.inject({ method: "GET", url: "/api/jobs" })).json() as Job[]).length).toBe(0);
});

test("PUT /api/show-meta on a symlinked show folder is 400 and writes nothing outside", async () => {
  await useFixture();
  const outside = await symlinkedOutside(path.join(root, "Cartoon Network", "LinkedShow"));
  const res = await app.inject({ method: "PUT", url: "/api/show-meta", payload: { channelFolder: "Cartoon Network", showFolder: "LinkedShow", name: "N", episodes: { S01E01: "d" }, mtimeMs: 0 } });
  expect(res.statusCode).toBe(400);
  expect(await readdir(outside)).toEqual([]);
});

test("icon upload where a channel folder should be a file is 400", async () => {
  await useFixture();
  await writeFile(path.join(root, "Blocked"), "x");
  const { payload, headers } = multipart({ kind: "show", channelFolder: "Blocked", showFolder: "Show" }, { name: "logo.png", data: await png(20, 20) });
  const res = await app.inject({ method: "POST", url: "/api/icon", payload, headers });
  expect(res.statusCode).toBe(400);
  expect((await stat(path.join(root, "Blocked"))).isFile()).toBe(true);
});

test("drive mode converts with the drive profile", async () => {
  const seen: (VideoProfile | undefined)[] = [];
  await app.close();
  app = make({
    queueFactory: (r, profile) => {
      const convertFn: typeof fakeConvert = (src, dst, aspect, onProgress, signal, guard, used) => {
        seen.push(used);
        return fakeConvert(src, dst, aspect, onProgress, signal, guard, used);
      };
      return new ConversionQueue(r, { convertFn, probeFn: fakeProbe, concurrency: 1, profile });
    },
  });
  await useFixture();
  const src = path.join(dir, "src");
  await mkdir(src);
  const body = {
    channelFolder: "Cartoons",
    showFolder: "Profile Show",
    showName: "Profile Show",
    episodes: [{ source: path.join(src, "a.mkv"), season: 1, episode: 1, title: "Pilot", description: "", aspect: "4:3" }],
  };
  const res = await app.inject({ method: "POST", url: "/api/import/show", payload: body });
  expect(res.statusCode).toBe(200);
  await waitFor(async () => seen.length === 1);
  expect(seen).toEqual([DRIVE_PROFILE]);
});
