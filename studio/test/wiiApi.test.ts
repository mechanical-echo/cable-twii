import { execFile } from "node:child_process";
import { readdirSync } from "node:fs";
import { get as httpGet } from "node:http";
import type { AddressInfo } from "node:net";
import { cp, mkdir, mkdtemp, readFile, realpath, rename, rm, stat, symlink, utimes, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { promisify } from "node:util";
import { gunzipSync } from "node:zlib";
import { parse } from "smol-toml";
import { afterEach, beforeEach, expect, test } from "vitest";
import type { FastifyInstance, InjectOptions } from "fastify";
import { buildApp, type AppOptions } from "../src/server/app.js";
import { ConversionQueue } from "../src/server/queue.js";
import { schedBinPath } from "../src/server/sched.js";
import { initServerLibrary } from "../src/server/serverSettings.js";
import { infoToml, libraryRevision, openWiiFile, parseRange, wiiFilePath } from "../src/server/wiiApi.js";
import { fakeConvert, fakeProbe, waitFor } from "./helpers.js";

const run = promisify(execFile);

const FIXTURE = path.resolve(import.meta.dirname, "../../engine/tests/fixtures/root_basic");
const FIXTURE_UNICODE = path.resolve(import.meta.dirname, "../../engine/tests/fixtures/root_unicode");
const LAN_HOST = "192.168.1.20:4747";
const EPISODE = "Cartoon Network/Gumball/Season 1/S01E01 - Gumball 1.mpg";
const ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

let dir: string;
let root: string;
let app: FastifyInstance;

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

function wii(options: InjectOptions) {
  return app.inject({ ...options, headers: { host: LAN_HOST, ...options.headers } });
}

function fileUrl(rel: string): string {
  return `/wii/v1/files/${rel.split("/").map(encodeURIComponent).join("/")}`;
}

async function put(rel: string, content: string | Buffer): Promise<void> {
  const file = path.join(root, ...rel.split("/"));
  await mkdir(path.dirname(file), { recursive: true });
  await writeFile(file, content);
}

function fetchRaw(url: string, headers: Record<string, string> = {}): Promise<{ status: number; headers: Record<string, unknown>; length: number }> {
  return new Promise((resolve, reject) => {
    httpGet(url, { agent: false, headers }, (res) => {
      let length = 0;
      res.on("data", (chunk: Buffer) => {
        length += chunk.length;
      });
      res.on("end", () => resolve({ status: res.statusCode ?? 0, headers: res.headers, length }));
      res.on("error", reject);
    }).on("error", reject);
  });
}

beforeEach(async () => {
  dir = await realpath(await mkdtemp(path.join(os.tmpdir(), "studio-wii-")));
  await initServerLibrary({ library: dir, password: "correct horse", imports: path.join(dir, "imports"), port: 4747, name: "Home PC" });
  root = path.join(dir, "cable-twii");
  await cp(FIXTURE, root, { recursive: true });
  app = build();
});

afterEach(async () => {
  await app.close();
  await rm(dir, { recursive: true, force: true });
});

test.each([
  ["bytes=0-99", { start: 0, end: 99 }],
  ["bytes=900-", { start: 900, end: 999 }],
  ["bytes=-100", { start: 900, end: 999 }],
  ["bytes=999-999", { start: 999, end: 999 }],
  ["bytes=0-5000", { start: 0, end: 999 }],
  ["bytes=-5000", { start: 0, end: 999 }],
  ["bytes=1000-", "unsatisfiable"],
  ["bytes=1000-1200", "unsatisfiable"],
  ["bytes=-0", "unsatisfiable"],
  ["bytes=0-1,5-6", null],
  ["bytes=5-2", null],
  ["bytes=-", null],
  ["bytes=a-b", null],
  ["items=0-5", null],
  ["", null],
  [undefined, null],
])("parseRange %s", (header, expected) => {
  expect(parseRange(header, 1000)).toEqual(expected);
});

test("parseRange on an empty file is unsatisfiable", () => {
  expect(parseRange("bytes=0-", 0)).toBe("unsatisfiable");
  expect(parseRange("bytes=-5", 0)).toBe("unsatisfiable");
  expect(parseRange(undefined, 0)).toBeNull();
});

test.each([
  ["config.toml"],
  ["Cartoon Network/Gumball/Season 1/S01E01 - Gumball 1.mpg"],
  ["Cartoon Network/Pokémon ✨/Season 1/S01E02 - Café über alles.mpg"],
  ["a.b/c.d"],
])("wiiFilePath keeps %s", (rel) => {
  expect(wiiFilePath(rel)).toBe(rel);
});

test.each([
  [""],
  ["/config.toml"],
  ["config.toml/"],
  [".studio/queue.toml"],
  [".DS_Store"],
  ["a/../b"],
  [".."],
  ["a/."],
  ["Cartoon Network/._x.mpg"],
  ["a//b"],
  ["..\\config.toml"],
  ["a\\b"],
  ["a\0b"],
])("wiiFilePath refuses %j", (rel) => {
  expect(wiiFilePath(rel)).toBeNull();
});

test("libraryRevision joins the whole-number mtimes of config and library", async () => {
  await utimes(path.join(root, "config.toml"), 1_700_000_000, 1_700_000_000);
  await utimes(path.join(root, "library.toml"), 1_800_000_000.0005, 1_800_000_000.0005);
  expect(await libraryRevision(root)).toBe("1700000000000-1800000000000");
});

test("libraryRevision is zero for a file that is missing", async () => {
  expect(await libraryRevision(path.join(dir, "imports"))).toBe("0-0");
  await rm(path.join(root, "library.toml"));
  await utimes(path.join(root, "config.toml"), 1_700_000_000, 1_700_000_000);
  expect(await libraryRevision(root)).toBe("1700000000000-0");
});

test("infoToml is TOML with a version, the name and the revision", () => {
  expect(infoToml("Home PC", "1-2")).toBe('version = 1\nname = "Home PC"\nrevision = "1-2"\n');
  const name = 'Sofia\'s "TV" ✨';
  expect(parse(infoToml(name, "7-9"))).toEqual({ version: 1, name, revision: "7-9" });
});

test("wii info is open and versioned", async () => {
  const res = await wii({ method: "GET", url: "/wii/v1/info" });
  expect(res.statusCode).toBe(200);
  expect(res.headers["content-type"]).toBe("application/toml");
  expect(res.headers["cache-control"]).toBe("no-cache");
  expect(res.headers["content-length"]).toBe(String(Buffer.byteLength(res.body)));
  const doc = parse(res.body);
  expect(doc.version).toBe(1);
  expect(doc.name).toBe("Home PC");
  expect(doc.revision).toMatch(/^\d+-\d+$/);
});

test("wii revision changes after a save", async () => {
  const before = parse((await wii({ method: "GET", url: "/wii/v1/info" })).body).revision;
  const later = new Date(Date.now() + 60_000);
  await utimes(path.join(root, "config.toml"), later, later);
  const after = parse((await wii({ method: "GET", url: "/wii/v1/info" })).body).revision;
  expect(after).not.toBe(before);
});

test("wii files are absent in drive mode", async () => {
  const drive = build({ server: undefined });
  try {
    const res = await drive.inject({ method: "GET", url: "/wii/v1/info", headers: { host: "127.0.0.1:4747" } });
    expect(res.statusCode).toBe(404);
  } finally {
    await drive.close();
  }
});

test("wii files serve a whole file", async () => {
  await put(EPISODE, ALPHABET);
  const res = await wii({ method: "GET", url: fileUrl(EPISODE) });
  expect(res.statusCode).toBe(200);
  expect(res.headers["content-type"]).toBe("video/mpeg");
  expect(res.headers["accept-ranges"]).toBe("bytes");
  expect(res.headers["content-length"]).toBe("26");
  expect(res.headers["content-range"]).toBeUndefined();
  expect(res.rawPayload.toString()).toBe(ALPHABET);
});

test("wii files serve an empty file", async () => {
  const res = await wii({ method: "GET", url: fileUrl(EPISODE) });
  expect(res.statusCode).toBe(200);
  expect(res.headers["content-length"]).toBe("0");
  expect(res.rawPayload.length).toBe(0);
});

test("wii files answer HEAD with the size and no body", async () => {
  await put(EPISODE, ALPHABET);
  const res = await wii({ method: "HEAD", url: fileUrl(EPISODE) });
  expect(res.statusCode).toBe(200);
  expect(res.headers["content-length"]).toBe("26");
  expect(res.rawPayload.length).toBe(0);
});

test("wii files serve a range", async () => {
  await put(EPISODE, ALPHABET);
  const res = await wii({ method: "GET", url: fileUrl(EPISODE), headers: { range: "bytes=0-9" } });
  expect(res.statusCode).toBe(206);
  expect(res.headers["content-range"]).toBe("bytes 0-9/26");
  expect(res.headers["content-length"]).toBe("10");
  expect(res.headers["accept-ranges"]).toBe("bytes");
  expect(res.rawPayload.toString()).toBe("ABCDEFGHIJ");
});

test.each([
  ["bytes=20-", "bytes 20-25/26", "UVWXYZ"],
  ["bytes=-3", "bytes 23-25/26", "XYZ"],
  ["bytes=25-25", "bytes 25-25/26", "Z"],
  ["bytes=24-999", "bytes 24-25/26", "YZ"],
])("wii files serve the range %s", async (range, contentRange, expected) => {
  await put(EPISODE, ALPHABET);
  const res = await wii({ method: "GET", url: fileUrl(EPISODE), headers: { range } });
  expect(res.statusCode).toBe(206);
  expect(res.headers["content-range"]).toBe(contentRange);
  expect(res.headers["content-length"]).toBe(String(expected.length));
  expect(res.rawPayload.toString()).toBe(expected);
});

test("wii files refuse a range past the end", async () => {
  await put(EPISODE, ALPHABET);
  const res = await wii({ method: "GET", url: fileUrl(EPISODE), headers: { range: "bytes=26-" } });
  expect(res.statusCode).toBe(416);
  expect(res.headers["content-range"]).toBe("bytes */26");
  expect(res.headers["content-length"]).toBe(String(Buffer.byteLength(res.body)));
});

test.each([["bytes=0-1,5-6"], ["bytes=5-2"], ["lines=0-1"]])("wii files send everything for the range %s", async (range) => {
  await put(EPISODE, ALPHABET);
  const res = await wii({ method: "GET", url: fileUrl(EPISODE), headers: { range } });
  expect(res.statusCode).toBe(200);
  expect(res.rawPayload.toString()).toBe(ALPHABET);
});

test("wii files serve the usual types", async () => {
  const png = Buffer.from([0x89, 0x50, 0x4e, 0x47]);
  await put("Cartoon Network/channel.png", png);
  const image = await wii({ method: "GET", url: fileUrl("Cartoon Network/channel.png") });
  expect(image.statusCode).toBe(200);
  expect(image.headers["content-type"]).toBe("image/png");
  expect(image.rawPayload.equals(png)).toBe(true);
  const text = await wii({ method: "GET", url: fileUrl("library.toml") });
  expect(text.statusCode).toBe(200);
  expect(text.headers["content-type"]).toBe("application/toml");
  expect(text.headers["content-encoding"]).toBeUndefined();
  expect(text.body).toBe(await readFile(path.join(root, "library.toml"), "utf8"));
});

test("wii files gzip toml", async () => {
  const res = await wii({ method: "GET", url: fileUrl("library.toml"), headers: { "accept-encoding": "gzip" } });
  expect(res.statusCode).toBe(200);
  expect(res.headers["content-type"]).toBe("application/toml");
  expect(res.headers["content-encoding"]).toBe("gzip");
  expect(res.headers["accept-ranges"]).toBe("bytes");
  expect(res.headers["content-length"]).toBe(String(res.rawPayload.length));
  expect(gunzipSync(res.rawPayload).toString()).toBe(await readFile(path.join(root, "library.toml"), "utf8"));
});

test("wii files do not gzip video", async () => {
  await put(EPISODE, ALPHABET);
  const res = await wii({ method: "GET", url: fileUrl(EPISODE), headers: { "accept-encoding": "gzip" } });
  expect(res.headers["content-encoding"]).toBeUndefined();
  expect(res.rawPayload.toString()).toBe(ALPHABET);
});

test("wii files refuse traversal", async () => {
  await writeFile(path.join(dir, "settings"), "secret");
  for (const url of ["/wii/v1/files/%2e%2e%2fsettings", "/wii/v1/files/..%2fsettings", "/wii/v1/files/Cartoon%20Network%2f..%2f..%2fsettings", "/wii/v1/files/%2e%2e%5csettings"]) {
    const res = await wii({ method: "GET", url });
    expect(res.statusCode, url).toBe(404);
    expect(res.body, url).not.toContain("secret");
  }
});

test("wii files refuse backslash", async () => {
  await put("a\\b.mpg", "backslash");
  await put("..\\config.toml", "backslash");
  for (const rel of ["a\\b.mpg", "..\\config.toml"]) {
    const res = await wii({ method: "GET", url: `/wii/v1/files/${encodeURIComponent(rel)}` });
    expect(res.statusCode, rel).toBe(404);
  }
});

test("wii files refuse NUL bytes", async () => {
  const res = await wii({ method: "GET", url: "/wii/v1/files/config.toml%00.mpg" });
  expect(res.statusCode).toBe(404);
});

test("wii files refuse dot folders", async () => {
  await put(".studio/sources.toml", "secret");
  await put(".DS_Store", "secret");
  await put("Cartoon Network/._x.mpg", "secret");
  for (const rel of [".studio/sources.toml", ".DS_Store", "Cartoon Network/._x.mpg"]) {
    const res = await wii({ method: "GET", url: fileUrl(rel) });
    expect(res.statusCode, rel).toBe(404);
    expect(res.body, rel).not.toContain("secret");
  }
});

test("wii files refuse symlink out", async () => {
  await writeFile(path.join(dir, "outside.mpg"), "secret");
  await symlink(path.join(dir, "outside.mpg"), path.join(root, "link.mpg"));
  const res = await wii({ method: "GET", url: fileUrl("link.mpg") });
  expect(res.statusCode).toBe(404);
  expect(res.body).not.toContain("secret");
});

test("wii files refuse a folder that links out", async () => {
  await mkdir(path.join(dir, "elsewhere"));
  await writeFile(path.join(dir, "elsewhere", "x.mpg"), "secret");
  await symlink(path.join(dir, "elsewhere"), path.join(root, "Cartoon Network", "escape"));
  const res = await wii({ method: "GET", url: fileUrl("Cartoon Network/escape/x.mpg") });
  expect(res.statusCode).toBe(404);
  expect(res.body).not.toContain("secret");
});

test.each([
  ["a link to a file in a dot folder", ".studio/sources.toml", "x.mpg", "x.mpg"],
  ["a link to a hidden file", "Cartoon Network/._hidden.mpg", "y.mpg", "y.mpg"],
  ["a link to a dot folder", ".studio", "dotlink", "dotlink/sources.toml"],
])("wii files refuse %s", async (_name, target, link, requested) => {
  await put(".studio/sources.toml", "secret");
  await put("Cartoon Network/._hidden.mpg", "secret");
  await symlink(path.join(root, ...target.split("/")), path.join(root, link));
  const res = await wii({ method: "GET", url: fileUrl(requested) });
  expect(res.statusCode).toBe(404);
  expect(res.body).not.toContain("secret");
});

test("wii files follow a link to a visible file in the root", async () => {
  await put(EPISODE, ALPHABET);
  await symlink(path.join(root, ...EPISODE.split("/")), path.join(root, "alias.mpg"));
  const res = await wii({ method: "GET", url: fileUrl("alias.mpg") });
  expect(res.statusCode).toBe(200);
  expect(res.body).toBe(ALPHABET);
});

test("wii files refuse a link to nothing", async () => {
  await symlink(path.join(root, "gone.mpg"), path.join(root, "dangling.mpg"));
  const res = await wii({ method: "GET", url: fileUrl("dangling.mpg") });
  expect(res.statusCode).toBe(404);
});

test.skipIf(process.platform === "win32")("wii files refuse a pipe without waiting on it", async () => {
  await run("mkfifo", [path.join(root, "pipe.mpg")]);
  const res = await wii({ method: "GET", url: fileUrl("pipe.mpg") });
  expect(res.statusCode).toBe(404);
});

test("openWiiFile reads the size and the bytes from one handle", async () => {
  await put(EPISODE, ALPHABET);
  const file = await openWiiFile(root, EPISODE);
  expect(file).not.toBeNull();
  try {
    await put("replacement.tmp", "0123456789".repeat(10));
    await rename(path.join(root, "replacement.tmp"), path.join(root, ...EPISODE.split("/")));
    expect((await stat(path.join(root, ...EPISODE.split("/")))).size).toBe(100);
    expect(file!.size).toBe(26);
    expect((await file!.handle.readFile()).toString()).toBe(ALPHABET);
  } finally {
    await file!.handle.close();
  }
});

test("openWiiFile finds nothing for a missing file or a folder", async () => {
  expect(await openWiiFile(root, "nothing.mpg")).toBeNull();
  expect(await openWiiFile(root, "Cartoon Network")).toBeNull();
  await rm(root, { recursive: true });
  expect(await openWiiFile(root, "config.toml")).toBeNull();
});

test.skipIf(process.platform === "win32")("wii files close every file they open", async () => {
  await put(EPISODE, ALPHABET);
  const openFiles = () => readdirSync("/dev/fd").length;
  const before = openFiles();
  const requests: [string, Record<string, string>][] = [
    [fileUrl(EPISODE), {}],
    [fileUrl(EPISODE), { range: "bytes=0-9" }],
    [fileUrl(EPISODE), { range: "bytes=26-" }],
    ["/wii/v1/files/library.toml", { "accept-encoding": "gzip" }],
    ["/wii/v1/files/library.toml", {}],
    ["/wii/v1/files/nothing.mpg", {}],
    ["/wii/v1/files/Cartoon%20Network", {}],
  ];
  for (let round = 0; round < 10; round++) {
    for (const [url, headers] of requests) await wii({ method: "GET", url, headers });
  }
  await waitFor(async () => openFiles() <= before);
});

test("wii files refuse folders, empty paths and missing files", async () => {
  for (const url of ["/wii/v1/files/Cartoon%20Network", "/wii/v1/files/", "/wii/v1/files/nothing.mpg", "/wii/v1/files/Cartoon%20Network/Gumball/"]) {
    const res = await wii({ method: "GET", url });
    expect(res.statusCode, url).toBe(404);
    expect(res.headers["content-length"], url).toBe(String(Buffer.byteLength(res.body)));
  }
});

test("wii files decode the path once", async () => {
  await put("a b.mpg", "space");
  await put("a%20b.mpg", "percent");
  const res = await wii({ method: "GET", url: "/wii/v1/files/a%2520b.mpg" });
  expect(res.statusCode).toBe(200);
  expect(res.body).toBe("percent");
});

test("wii files serve unicode names", async () => {
  await cp(FIXTURE_UNICODE, root, { recursive: true });
  const rel = "Cartoon Network/Pokémon ✨/Season 1/S01E02 - Café über alles.mpg";
  const bytes = Buffer.from([0x00, 0xff, 0x10, 0x80, 0x7f, 0xc3, 0x28]);
  await put(rel, bytes);
  const res = await wii({ method: "GET", url: fileUrl(rel) });
  expect(res.statusCode).toBe(200);
  expect(res.rawPayload.equals(bytes)).toBe(true);
  expect(res.headers["content-length"]).toBe(String(bytes.length));
});

test("wii responses always carry a Content-Length and are never chunked", async () => {
  await put(EPISODE, ALPHABET);
  await app.listen({ host: "127.0.0.1", port: 0 });
  const base = `http://127.0.0.1:${(app.server.address() as AddressInfo).port}`;
  const cases: [string, Record<string, string>, number][] = [
    ["/wii/v1/info", {}, 200],
    ["/wii/v1/files/library.toml", {}, 200],
    ["/wii/v1/files/library.toml", { "accept-encoding": "gzip" }, 200],
    [fileUrl(EPISODE), {}, 200],
    [fileUrl(EPISODE), { range: "bytes=2-11" }, 206],
    [fileUrl(EPISODE), { range: "bytes=26-" }, 416],
    ["/wii/v1/files/nothing.mpg", {}, 404],
    ["/wii/v1/files/%2e%2e%2fsettings", {}, 404],
  ];
  for (const [url, headers, status] of cases) {
    const res = await fetchRaw(`${base}${url}`, headers);
    expect(res.status, url).toBe(status);
    expect(res.headers["transfer-encoding"], url).toBeUndefined();
    expect(res.headers["content-length"], url).toBe(String(res.length));
  }
});
