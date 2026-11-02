import { mkdtemp, readFile, readdir, realpath, rm, stat, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { PassThrough } from "node:stream";
import { afterEach, beforeEach, expect, test } from "vitest";
import type { FastifyInstance, InjectOptions } from "fastify";
import { buildApp, type AppOptions } from "../src/server/app.js";
import { ContainmentError } from "../src/server/contain.js";
import type { convert } from "../src/server/ffmpeg.js";
import { ConversionQueue } from "../src/server/queue.js";
import { schedBinPath } from "../src/server/sched.js";
import { initServerLibrary, uploadsDir } from "../src/server/serverSettings.js";
import { readSources } from "../src/server/sources.js";
import type { Job } from "../src/server/types.js";
import { UPLOAD_LIMIT, isUploadPath, saveUpload, uploadName, uploadSource } from "../src/server/uploads.js";
import { fakeConvert, fakeProbe, multipart, waitFor } from "./helpers.js";

const LAN_HOST = "192.168.1.20:4747";

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
