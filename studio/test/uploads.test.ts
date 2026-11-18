import { randomUUID } from "node:crypto";
import { lstat, mkdir, mkdtemp, readFile, readdir, realpath, rm, stat, symlink, utimes, writeFile } from "node:fs/promises";
import { type AddressInfo, createServer } from "node:net";
import os from "node:os";
import path from "node:path";
import { PassThrough } from "node:stream";
import { afterEach, beforeEach, expect, test, vi } from "vitest";
import type { FastifyInstance, InjectOptions } from "fastify";
import { stringify } from "smol-toml";
import { buildApp, type AppOptions } from "../src/server/app.js";
import { ContainmentError } from "../src/server/contain.js";
import { MIN_DURATION_MS, type convert } from "../src/server/ffmpeg.js";
import { ConversionQueue } from "../src/server/queue.js";
import { schedBinPath } from "../src/server/sched.js";
import { initServerLibrary, uploadsDir } from "../src/server/serverSettings.js";
import { readSources } from "../src/server/sources.js";
import type { Job } from "../src/server/types.js";
import { UPLOAD_LIMIT, isUploadPath, saveUpload, sweepUploads, uploadName, uploadSource } from "../src/server/uploads.js";
import { fakeConvert, fakeProbe, multipart, waitFor } from "./helpers.js";

vi.mock("node:crypto", async (importOriginal) => {
  const actual = await importOriginal<typeof import("node:crypto")>();
  return { ...actual, randomUUID: vi.fn(actual.randomUUID) };
});

const LAN_HOST = "192.168.1.20:4747";
const FIXED_BATCH = "aaaaaaaa-bbbb-cccc-dddd-eeeeeeeeeeee";
const DAY_MS = 24 * 60 * 60 * 1000;
const T0 = Date.UTC(2026, 10, 1);

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

let dir: string;
let uploads: string;
let root: string;
let app: FastifyInstance;
let cookie: string;

function build(extra: Partial<AppOptions> = {}): FastifyInstance {
  return buildApp({
    schedBin: schedBinPath(),
    settingsPath: path.join(dir, "settings.json"),
    queueFactory: (r) => new ConversionQueue(r, { convertFn: fakeConvert, probeFn: fakeProbe, concurrency: 1 }),
    probeFn: fakeProbe,
    rebuildDelayMs: 20,
    webDir: path.join(dir, "no-web"),
    server: { library: dir, password: "correct horse", imports: path.join(dir, "imports"), name: "Home PC" },
    ...extra,
  });
}

function anonymous(options: InjectOptions) {
  return app.inject({ ...options, headers: { host: LAN_HOST, "x-cable-twii": "1", ...options.headers } });
}

function send(options: InjectOptions) {
  return anonymous({ ...options, headers: { cookie, ...options.headers } });
}

async function signIn(target: FastifyInstance): Promise<string> {
  const res = await target.inject({ method: "POST", url: "/api/login", headers: { host: LAN_HOST, "x-cable-twii": "1" }, payload: { password: "correct horse" } });
  expect(res.statusCode).toBe(200);
  return String(res.headers["set-cookie"]).split(";")[0] as string;
}

function multipartFiles(files: { name: string; data: Buffer }[]) {
  const boundary = "----ctwboundary" + Math.random().toString(16).slice(2);
  const chunks: Buffer[] = [];
  for (const file of files) {
    chunks.push(Buffer.from(`--${boundary}\r\nContent-Disposition: form-data; name="file"; filename="${file.name}"\r\nContent-Type: application/octet-stream\r\n\r\n`), file.data, Buffer.from("\r\n"));
  }
  chunks.push(Buffer.from(`--${boundary}--\r\n`));
  return { payload: Buffer.concat(chunks), headers: { "content-type": `multipart/form-data; boundary=${boundary}` } };
}

function upload(file: { name: string; data: Buffer }) {
  const { payload, headers } = multipart({}, file);
  return send({ method: "POST", url: "/api/upload", payload, headers });
}

function uploadMany(files: { name: string; data: Buffer }[]) {
  const { payload, headers } = multipartFiles(files);
  return send({ method: "POST", url: "/api/upload", payload, headers });
}

function importShow(...sources: string[]) {
  const episodes = sources.map((source, i) => ({ source, season: 1, episode: i + 1, title: "Pilot", description: "", aspect: "4:3" }));
  return send({ method: "POST", url: "/api/import/show", payload: { channelFolder: "Cartoon Network", showFolder: "Gumball", episodes } });
}

async function jobs(): Promise<Job[]> {
  return (await send({ method: "GET", url: "/api/jobs" })).json() as Job[];
}

async function uploadsEmpty(): Promise<boolean> {
  return (await readdir(uploads)).length === 0;
}

async function stored(rel: string, mtimeMs: number, base = uploads): Promise<string> {
  const file = path.join(base, ...rel.split("/"));
  await mkdir(path.dirname(file), { recursive: true });
  await writeFile(file, "whole");
  await utimes(file, new Date(mtimeMs), new Date(mtimeMs));
  return file;
}

beforeEach(async () => {
  dir = await realpath(await mkdtemp(path.join(os.tmpdir(), "studio-uploads-")));
  await initServerLibrary({ library: dir, password: "correct horse", imports: path.join(dir, "imports"), port: 4747, name: "Home PC" });
  uploads = uploadsDir(dir);
  root = path.join(dir, "cable-twii");
  app = build();
  cookie = await signIn(app);
});

afterEach(async () => {
  await app.close();
  await rm(dir, { recursive: true, force: true });
});

test("upload stores the file and returns a scan", async () => {
  const res = await upload({ name: "ep.mp4", data: Buffer.from("video") });
  expect(res.statusCode).toBe(200);
  const body = res.json();
  expect(body.files).toHaveLength(1);
  expect(body.rejected).toEqual([]);
  const source = body.files[0].source as string;
  expect(path.dirname(path.dirname(source))).toBe(uploads);
  expect(body.dir).toBe(path.dirname(source));
  expect(await readFile(source, "utf8")).toBe("video");
  expect(await readdir(body.dir)).toEqual(["ep.mp4"]);
  expect(body.files[0].durationMs).toBe(60000);
});

test("upload keeps an accented name as it was sent", async () => {
  const res = await upload({ name: "Café Épisode 1.mp4", data: Buffer.from("video") });
  expect(res.statusCode).toBe(200);
  expect(await readdir(res.json().dir)).toEqual(["Café Épisode 1.mp4"]);
});

test("upload takes many files at once", async () => {
  const res = await uploadMany([
    { name: "one.mp4", data: Buffer.from("1") },
    { name: "notes.txt", data: Buffer.from("n") },
    { name: "two.mkv", data: Buffer.from("22") },
  ]);
  expect(res.statusCode).toBe(200);
  const body = res.json();
  expect(body.files.map((f: { source: string }) => path.basename(f.source))).toEqual(["one.mp4", "two.mkv"]);
  expect(body.rejected).toEqual([{ name: "notes.txt", reason: "not a video" }]);
  expect((await readdir(body.dir)).sort()).toEqual(["one.mp4", "two.mkv"]);
});

test("upload rejects non-video names", async () => {
  const res = await upload({ name: "notes.txt", data: Buffer.from("text") });
  expect(res.statusCode).toBe(200);
  expect(res.json().files).toEqual([]);
  expect(res.json().rejected).toEqual([{ name: "notes.txt", reason: "not a video" }]);
  expect(await uploadsEmpty()).toBe(true);
});

test("upload rejects a hidden name", async () => {
  const res = await upload({ name: ".secret.mp4", data: Buffer.from("video") });
  expect(res.statusCode).toBe(200);
  expect(res.json().files).toEqual([]);
  expect(res.json().rejected).toEqual([{ name: ".secret.mp4", reason: "not a usable file name" }]);
  expect(await uploadsEmpty()).toBe(true);
});

test("upload keeps one of two files with the same name", async () => {
  const res = await uploadMany([
    { name: "ep.mp4", data: Buffer.from("first") },
    { name: "ep.mp4", data: Buffer.from("second") },
  ]);
  expect(res.statusCode).toBe(200);
  const body = res.json();
  expect(body.files).toHaveLength(1);
  expect(body.rejected).toEqual([{ name: "ep.mp4", reason: "already in this upload" }]);
  expect(await readFile(body.files[0].source, "utf8")).toBe("first");
});

test("upload stores an unsafe name under a safe one", async () => {
  const res = await upload({ name: "Ep 1: Pilot?.mp4", data: Buffer.from("video") });
  expect(res.statusCode).toBe(200);
  const body = res.json();
  expect(body.rejected).toEqual([]);
  expect(body.files).toHaveLength(1);
  expect(path.basename(body.files[0].source)).toBe("Ep 1- Pilot.mp4");
  expect(await readdir(body.dir)).toEqual(["Ep 1- Pilot.mp4"]);
});

test.each([
  ["con.mp4", "con_.mp4"],
  ["NUL.mkv", "NUL_.mkv"],
  ["COM1.mp4", "COM1_.mp4"],
])("upload makes the device name %s safe", async (sent, stored) => {
  const res = await upload({ name: sent, data: Buffer.from("video") });
  expect(res.statusCode).toBe(200);
  expect(res.json().rejected).toEqual([]);
  expect(await readdir(res.json().dir)).toEqual([stored]);
});

test("upload rejects a name that is too long and keeps the rest of the batch", async () => {
  const long = `${"a".repeat(296)}.mp4`;
  const res = await uploadMany([
    { name: long, data: Buffer.from("video") },
    { name: "one.mp4", data: Buffer.from("video") },
  ]);
  expect(res.statusCode).toBe(200);
  const body = res.json();
  expect(body.rejected).toEqual([{ name: long, reason: "not a usable file name" }]);
  expect(body.files.map((f: { source: string }) => path.basename(f.source))).toEqual(["one.mp4"]);
  expect(await readdir(body.dir)).toEqual(["one.mp4"]);
});

test("upload rejects a name with nothing usable left", async () => {
  const res = await upload({ name: "???.mp4", data: Buffer.from("video") });
  expect(res.statusCode).toBe(200);
  expect(res.json().files).toEqual([]);
  expect(res.json().rejected).toEqual([{ name: "???.mp4", reason: "not a usable file name" }]);
  expect(await uploadsEmpty()).toBe(true);
});

test("upload keeps one of two names that become the same", async () => {
  const res = await uploadMany([
    { name: "a:b.mp4", data: Buffer.from("first") },
    { name: "a-b.mp4", data: Buffer.from("second") },
  ]);
  expect(res.statusCode).toBe(200);
  const body = res.json();
  expect(body.files).toHaveLength(1);
  expect(body.rejected).toEqual([{ name: "a-b.mp4", reason: "already in this upload" }]);
  expect(await readFile(body.files[0].source, "utf8")).toBe("first");
});

test("a file that cannot be written is rejected and the rest of the batch is kept", async () => {
  await mkdir(path.join(uploads, FIXED_BATCH, "ep.mp4.part"), { recursive: true });
  vi.mocked(randomUUID).mockReturnValueOnce(FIXED_BATCH);
  const res = await uploadMany([
    { name: "ep.mp4", data: Buffer.from("first") },
    { name: "two.mp4", data: Buffer.from("second") },
  ]);
  expect(res.statusCode).toBe(200);
  const body = res.json();
  expect(body.rejected).toEqual([{ name: "ep.mp4", reason: "could not be saved" }]);
  expect(body.files.map((f: { source: string }) => path.basename(f.source))).toEqual(["two.mp4"]);
  expect(await readFile(path.join(uploads, FIXED_BATCH, "two.mp4"), "utf8")).toBe("second");
});

test("upload deletes files that cannot be read or are too short", async () => {
  const res = await uploadMany([
    { name: "broken.mp4", data: Buffer.from("video") },
    { name: "one.mp4", data: Buffer.from("video") },
    { name: "short.mp4", data: Buffer.from("video") },
  ]);
  expect(res.statusCode).toBe(200);
  const body = res.json();
  expect(body.files.map((f: { source: string }) => path.basename(f.source))).toEqual(["one.mp4"]);
  expect(body.rejected).toEqual([
    { name: "broken.mp4", reason: "could not read the video: no video stream" },
    { name: "short.mp4", reason: `too short (500 ms, needs at least ${MIN_DURATION_MS} ms)` },
  ]);
  expect(await readdir(body.dir)).toEqual(["one.mp4"]);
});

test("upload leaves no folder when every file is refused after the scan", async () => {
  const res = await uploadMany([
    { name: "broken.mp4", data: Buffer.from("video") },
    { name: "short.mp4", data: Buffer.from("video") },
  ]);
  expect(res.statusCode).toBe(200);
  expect(res.json().files).toEqual([]);
  expect(res.json().rejected.map((r: { name: string }) => r.name)).toEqual(["broken.mp4", "short.mp4"]);
  expect(await uploadsEmpty()).toBe(true);
});

test("upload needs a session", async () => {
  const { payload, headers } = multipart({}, { name: "ep.mp4", data: Buffer.from("video") });
  const res = await anonymous({ method: "POST", url: "/api/upload", payload, headers });
  expect(res.statusCode).toBe(401);
  expect(res.json()).toEqual({ error: "Sign in first." });
  expect(await uploadsEmpty()).toBe(true);
});

test("upload does not exist in drive mode", async () => {
  const drive = build({ server: undefined });
  try {
    const { payload, headers } = multipart({}, { name: "ep.mp4", data: Buffer.from("video") });
    const res = await drive.inject({ method: "POST", url: "/api/upload", payload, headers: { host: "127.0.0.1:4747", "x-cable-twii": "1", ...headers } });
    expect(res.statusCode).toBe(404);
  } finally {
    await drive.close();
  }
});

function discard(payload: unknown) {
  return send({ method: "POST", url: "/api/upload/discard", payload: payload as object });
}

test("discard removes the named files and the emptied batch folder", async () => {
  const res = await uploadMany([
    { name: "a.mp4", data: Buffer.from("one") },
    { name: "b.mp4", data: Buffer.from("two") },
  ]);
  const batch = res.json().dir as string;
  const first = await discard({ dir: batch, names: ["a.mp4"] });
  expect(first.statusCode).toBe(200);
  expect(first.json()).toEqual({ removed: 1, kept: 0 });
  expect(await readdir(batch)).toEqual(["b.mp4"]);
  const second = await discard({ dir: batch, names: ["b.mp4"] });
  expect(second.json()).toEqual({ removed: 1, kept: 0 });
  expect(await uploadsEmpty()).toBe(true);
});

test("discard refuses a folder outside the uploads and a bad name", async () => {
  const batch = (await upload({ name: "a.mp4", data: Buffer.from("one") })).json().dir as string;
  const outside = path.join(dir, "imports");
  await mkdir(outside, { recursive: true });
  await writeFile(path.join(outside, "keep.mp4"), "keep");
  const attempts = [
    { dir: outside, names: ["keep.mp4"] },
    { dir: uploads, names: ["a.mp4"] },
    { dir: path.join(batch, ".."), names: [path.basename(batch)] },
    { dir: batch, names: ["../a.mp4"] },
    { dir: batch, names: "a.mp4" },
  ];
  for (const attempt of attempts) expect((await discard(attempt)).statusCode).toBe(400);
  expect(await readdir(batch)).toEqual(["a.mp4"]);
  expect(await readFile(path.join(outside, "keep.mp4"), "utf8")).toBe("keep");
});

test("discard skips a missing file and keeps one with a queued job", async () => {
  const batch = (await uploadMany([
    { name: "a.mp4", data: Buffer.from("one") },
    { name: "b.mp4", data: Buffer.from("two") },
  ])).json().dir as string;
  const queue = new ConversionQueue(root, { convertFn: fakeConvert, probeFn: fakeProbe, concurrency: 1 });
  vi.spyOn(queue, "list").mockReturnValue([{ id: "1", source: path.join(batch, "a.mp4"), target: "x", aspect: "4:3", state: "queued", progress: 0 }]);
  const kept = build({ queueFactory: () => queue });
  try {
    const key = await signIn(kept);
    const res = await kept.inject({ method: "POST", url: "/api/upload/discard", payload: { dir: batch, names: ["a.mp4", "b.mp4", "gone.mp4"] }, headers: { host: LAN_HOST, "x-cable-twii": "1", cookie: key } });
    expect(res.statusCode).toBe(200);
    expect(res.json()).toEqual({ removed: 1, kept: 1 });
    expect(await readdir(batch)).toEqual(["a.mp4"]);
  } finally {
    await kept.close();
  }
});

test("discard does not exist in drive mode", async () => {
  const drive = build({ server: undefined });
  try {
    const res = await drive.inject({ method: "POST", url: "/api/upload/discard", payload: { dir: uploads, names: [] }, headers: { host: "127.0.0.1:4747", "x-cable-twii": "1" } });
    expect(res.statusCode).toBe(404);
  } finally {
    await drive.close();
  }
});

test("converted upload is deleted and recorded", async () => {
  const res = await upload({ name: "ep.mp4", data: Buffer.from("video") });
  const source = res.json().files[0].source as string;
  const imported = await importShow(source);
  expect(imported.statusCode).toBe(200);
  expect(imported.json().jobs).toHaveLength(1);
  await waitFor(async () => (await jobs()).every((j) => j.state === "done"));
  await waitFor(uploadsEmpty);
  await expect(stat(source)).rejects.toMatchObject({ code: "ENOENT" });
  const recorded = await readSources(root);
  expect(recorded).toHaveLength(1);
  expect(recorded[0]).toMatchObject({ source: "upload:ep.mp4", size: 5 });
  expect((await stat(path.join(root, ...recorded[0]!.target.split("/")))).isFile()).toBe(true);
});

test("an upload imported twice stays until its last conversion", async () => {
  let release!: () => void;
  const gate = new Promise<void>((resolve) => {
    release = resolve;
  });
  let started = 0;
  const convertFn: typeof convert = async (src, dst) => {
    if (++started === 2) await gate;
    await readFile(src);
    await writeFile(dst, "converted");
  };
  await app.close();
  app = build({ queueFactory: (r) => new ConversionQueue(r, { convertFn, probeFn: fakeProbe, concurrency: 1 }) });
  cookie = await signIn(app);
  try {
    const res = await upload({ name: "ep.mp4", data: Buffer.from("video") });
    const source = res.json().files[0].source as string;
    const imported = await importShow(source, source);
    expect(imported.json().jobs).toHaveLength(2);
    await waitFor(async () => (await readSources(root)).some((r) => r.source === "upload:ep.mp4"));
    await sleep(50);
    expect((await stat(source)).isFile()).toBe(true);
    release();
    await waitFor(async () => (await jobs()).every((j) => j.state === "done"));
    await waitFor(uploadsEmpty);
    expect((await readSources(root)).map((r) => r.source)).toEqual(["upload:ep.mp4", "upload:ep.mp4"]);
  } finally {
    release();
  }
});

test("a converted upload is recorded and recognised under its stored name", async () => {
  const first = await upload({ name: "Ep 1: Pilot?.mp4", data: Buffer.from("video") });
  expect((await importShow(first.json().files[0].source)).statusCode).toBe(200);
  await waitFor(uploadsEmpty);
  expect((await readSources(root)).map((r) => r.source)).toEqual(["upload:Ep 1- Pilot.mp4"]);
  const again = await upload({ name: "Ep 1: Pilot?.mp4", data: Buffer.from("video") });
  expect(again.json().files).toEqual([]);
  expect(again.json().rejected).toEqual([{ name: "Ep 1: Pilot?.mp4", reason: "already on the server" }]);
  expect(await uploadsEmpty()).toBe(true);
});

test("same upload again is rejected", async () => {
  const first = await upload({ name: "ep.mp4", data: Buffer.from("video") });
  expect((await importShow(first.json().files[0].source)).statusCode).toBe(200);
  await waitFor(uploadsEmpty);
  const again = await upload({ name: "ep.mp4", data: Buffer.from("video") });
  expect(again.statusCode).toBe(200);
  expect(again.json().files).toEqual([]);
  expect(again.json().rejected).toEqual([{ name: "ep.mp4", reason: "already on the server" }]);
  expect(await uploadsEmpty()).toBe(true);
});

test("the same name with another size is a new upload", async () => {
  const first = await upload({ name: "ep.mp4", data: Buffer.from("video") });
  expect((await importShow(first.json().files[0].source)).statusCode).toBe(200);
  await waitFor(uploadsEmpty);
  const other = await upload({ name: "ep.mp4", data: Buffer.from("a longer video") });
  expect(other.json().rejected).toEqual([]);
  expect(other.json().files).toHaveLength(1);
});

test("upload over the limit leaves nothing", async () => {
  await app.close();
  app = build({ uploadLimit: 10 });
  cookie = await signIn(app);
  const res = await upload({ name: "ep.mp4", data: Buffer.alloc(100) });
  expect(res.statusCode).toBe(413);
  expect(await readdir(uploads)).toEqual([]);
  expect((await send({ method: "GET", url: "/api/jobs" })).json()).toEqual([]);
});

test("a file over the limit takes the whole batch with it", async () => {
  await app.close();
  app = build({ uploadLimit: 10 });
  cookie = await signIn(app);
  const res = await uploadMany([
    { name: "small.mp4", data: Buffer.from("tiny") },
    { name: "big.mp4", data: Buffer.alloc(100) },
  ]);
  expect(res.statusCode).toBe(413);
  expect(await readdir(uploads)).toEqual([]);
  expect((await send({ method: "GET", url: "/api/jobs" })).json()).toEqual([]);
});

test("the upload limit is 20 GiB", () => {
  expect(UPLOAD_LIMIT).toBe(21474836480);
});

test("uploadName makes a name safe for the file system", () => {
  expect(uploadName("Ep 1: Pilot?.mp4")).toBe("Ep 1- Pilot.mp4");
  expect(uploadName('a*b"c<d>e|f.mkv')).toBe("abcde-f.mkv");
  expect(uploadName("con.mp4")).toBe("con_.mp4");
  expect(uploadName("NUL.mkv")).toBe("NUL_.mkv");
  expect(uploadName("lpt1.mp4")).toBe("lpt1_.mp4");
  expect(uploadName("a\u0001b.mp4")).toBe("ab.mp4");
  expect(uploadName("ep .mp4")).toBe("ep.mp4");
  expect(uploadName("  ep  1.mp4")).toBe("ep 1.mp4");
});

test("uploadName refuses what is still unusable", () => {
  expect(uploadName("???.mp4")).toBeNull();
  expect(uploadName("?.hidden.mp4")).toBeNull();
  expect(uploadName(`${"a".repeat(246)}.mp4`)).not.toBeNull();
  expect(uploadName(`${"a".repeat(247)}.mp4`)).toBeNull();
  expect(uploadName(`${"é".repeat(123)}.mp4`)).not.toBeNull();
  expect(uploadName(`${"é".repeat(124)}.mp4`)).toBeNull();
});

test("uploadName", () => {
  expect(uploadName("ep.mp4")).toBe("ep.mp4");
  expect(uploadName("Café Épisode 1.mp4")).toBe("Café Épisode 1.mp4");
  expect(uploadName("../x.mp4")).toBeNull();
  expect(uploadName("")).toBeNull();
  expect(uploadName(".hidden.mp4")).toBeNull();
  expect(uploadName("a/b.mp4")).toBeNull();
  expect(uploadName("a\\b.mp4")).toBeNull();
  expect(uploadName("a\0b.mp4")).toBeNull();
});

test("uploadSource", () => {
  expect(uploadSource("ep.mp4")).toBe("upload:ep.mp4");
});

test("isUploadPath", () => {
  expect(isUploadPath(uploads, path.join(uploads, "batch", "ep.mp4"))).toBe(true);
  expect(isUploadPath(uploads, uploads)).toBe(false);
  expect(isUploadPath(uploads, `${uploads}-other/batch/ep.mp4`)).toBe(false);
  expect(isUploadPath(uploads, path.join(uploads, "..", "cable-twii", "ep.mp4"))).toBe(false);
  expect(isUploadPath(uploads, path.join(uploads, "batch", "..", "..", "ep.mp4"))).toBe(false);
  expect(isUploadPath(uploads, path.join("batch", "ep.mp4"))).toBe(false);
  expect(isUploadPath(uploads, "upload:ep.mp4")).toBe(false);
});

test("saveUpload writes a part file and renames it when the stream ends", async () => {
  const batch = path.join(uploads, "b1");
  const stream = new PassThrough();
  const saving = saveUpload(uploads, "b1", "ep.mp4", stream);
  stream.write("abc");
  await waitFor(async () => (await readdir(batch).catch((): string[] => [])).includes("ep.mp4.part"));
  expect(await readdir(batch)).toEqual(["ep.mp4.part"]);
  stream.end("def");
  expect(await saving).toBe(path.join(batch, "ep.mp4"));
  expect(await readdir(batch)).toEqual(["ep.mp4"]);
  expect(await readFile(path.join(batch, "ep.mp4"), "utf8")).toBe("abcdef");
});

test("saveUpload leaves no part file when the stream fails", async () => {
  const stream = new PassThrough();
  const saving = saveUpload(uploads, "b1", "ep.mp4", stream);
  stream.write("abc");
  await waitFor(async () => (await readdir(path.join(uploads, "b1")).catch((): string[] => [])).includes("ep.mp4.part"));
  stream.destroy(new Error("connection lost"));
  await expect(saving).rejects.toThrow("connection lost");
  expect(await readdir(path.join(uploads, "b1"))).toEqual([]);
});

test.each([
  ["b1", "../ep.mp4"],
  ["b1", "sub/ep.mp4"],
  ["b1", ".hidden.mp4"],
  ["b1", "ep?.mp4"],
  ["b1", "a:b.mp4"],
  ["b1", "con.mp4"],
  ["b1", ""],
  ["..", "ep.mp4"],
  ["", "ep.mp4"],
  ["a/b", "ep.mp4"],
])("saveUpload refuses batch %j with name %j", async (batch, name) => {
  const stream = new PassThrough();
  await expect(saveUpload(uploads, batch, name, stream)).rejects.toBeInstanceOf(ContainmentError);
  expect(await readdir(uploads)).toEqual([]);
  expect(await readdir(dir)).not.toContain("ep.mp4");
});

test("the sweep removes part files and empty batch folders but keeps fresh uploads", async () => {
  await stored("b1/x.mp4.part", T0);
  await mkdir(path.join(uploads, "b2"));
  await stored("b3/y.mp4", T0);
  await stored("b4/z.mp4.part", T0);
  await stored("b4/w.mp4", T0);
  await sweepUploads(uploads, new Set(), T0 + 60_000);
  expect((await readdir(uploads)).sort()).toEqual(["b3", "b4"]);
  expect(await readdir(path.join(uploads, "b3"))).toEqual(["y.mp4"]);
  expect(await readdir(path.join(uploads, "b4"))).toEqual(["w.mp4"]);
  expect(await readFile(path.join(uploads, "b3", "y.mp4"), "utf8")).toBe("whole");
});

test("the sweep removes an upload older than a day that no job needs", async () => {
  await stored("b1/old.mp4", T0);
  await sweepUploads(uploads, new Set(), T0 + DAY_MS + 1000);
  expect(await readdir(uploads)).toEqual([]);
});

test("the sweep keeps an upload newer than a day", async () => {
  const fresh = await stored("b1/fresh.mp4", T0);
  await sweepUploads(uploads, new Set(), T0 + DAY_MS - 60_000);
  expect(await readFile(fresh, "utf8")).toBe("whole");
});

test("the sweep keeps an old upload that a queued job needs", async () => {
  const needed = await stored("b1/needed.mp4", T0);
  await stored("b1/orphan.mp4", T0);
  await sweepUploads(uploads, new Set([needed]), T0 + 2 * DAY_MS);
  expect(await readdir(path.join(uploads, "b1"))).toEqual(["needed.mp4"]);
});

test("the sweep does not follow links out of the uploads folder", async () => {
  const outside = path.join(dir, "outside");
  const mine = await stored("keep.mp4", T0, outside);
  await stored("keep.mp4.part", T0, outside);
  await symlink(outside, path.join(uploads, "linked-batch"));
  await mkdir(path.join(uploads, "b1"));
  await symlink(mine, path.join(uploads, "b1", "linked.mp4"));
  await sweepUploads(uploads, new Set(), T0 + 2 * DAY_MS);
  expect((await readdir(outside)).sort()).toEqual(["keep.mp4", "keep.mp4.part"]);
  expect((await lstat(path.join(uploads, "b1", "linked.mp4"))).isSymbolicLink()).toBe(true);
});

test("the sweep leaves an uploads folder that is a link alone", async () => {
  const elsewhere = path.join(dir, "elsewhere");
  await stored("b1/old.mp4", T0, elsewhere);
  await stored("b1/x.mp4.part", T0, elsewhere);
  await rm(uploads, { recursive: true });
  await symlink(elsewhere, uploads);
  await sweepUploads(uploads, new Set(), T0 + 2 * DAY_MS);
  expect((await readdir(path.join(elsewhere, "b1"))).sort()).toEqual(["old.mp4", "x.mp4.part"]);
});

test("the server sweeps uploads only once it is listening, keeping what the queue needs", async () => {
  const old = Date.now() - 2 * DAY_MS;
  await stored("b1/x.mp4.part", Date.now());
  await stored("b2/orphan.mp4", old);
  const needed = await stored("b3/needed.mp4", old);
  const job = { id: "j-0001", source: needed, target: "Cartoon Network/Gumball/Season 1/S01E01 - Pilot.mpg", aspect: "4:3" };
  await writeFile(path.join(root, ".studio", "queue.toml"), stringify({ version: 1, job: [job] }));
  let release!: () => void;
  const gate = new Promise<void>((resolve) => {
    release = resolve;
  });
  const convertFn: typeof convert = async (_src, dst) => {
    await gate;
    await writeFile(dst, "converted");
  };
  await app.close();
  try {
    await initServerLibrary({ library: dir, password: "correct horse", imports: path.join(dir, "imports"), port: 4747, name: "Home PC" });
    app = build({ queueFactory: (r) => new ConversionQueue(r, { convertFn, probeFn: fakeProbe, concurrency: 1 }) });
    await app.ready();
    await sleep(50);
    expect((await readdir(uploads)).sort()).toEqual(["b1", "b2", "b3"]);
    await app.listen({ host: "127.0.0.1", port: 0 });
    await waitFor(async () => (await readdir(uploads)).length === 1);
    expect(await readdir(uploads)).toEqual(["b3"]);
    expect(await readdir(path.join(uploads, "b3"))).toEqual(["needed.mp4"]);
  } finally {
    release();
  }
});

test("a server that cannot listen leaves uploads alone", async () => {
  await stored("b1/x.mp4.part", Date.now());
  await stored("b2/orphan.mp4", Date.now() - 2 * DAY_MS);
  const blocker = createServer();
  await new Promise<void>((resolve) => blocker.listen(0, "127.0.0.1", resolve));
  const { port } = blocker.address() as AddressInfo;
  const second = build();
  try {
    await expect(second.listen({ host: "127.0.0.1", port })).rejects.toThrow(/EADDRINUSE/);
    await sleep(50);
    expect((await readdir(uploads)).sort()).toEqual(["b1", "b2"]);
    expect(await readdir(path.join(uploads, "b1"))).toEqual(["x.mp4.part"]);
  } finally {
    await second.close();
    await new Promise((resolve) => blocker.close(resolve));
  }
});
