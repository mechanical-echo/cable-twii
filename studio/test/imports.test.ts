import { mkdir, mkdtemp, realpath, rm, stat, symlink, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { afterEach, beforeEach, expect, test } from "vitest";
import type { FastifyInstance, InjectOptions } from "fastify";
import { buildApp, type AppOptions } from "../src/server/app.js";
import { ContainmentError } from "../src/server/contain.js";
import { isAllowedSource, listImports } from "../src/server/imports.js";
import { ConversionQueue } from "../src/server/queue.js";
import { schedBinPath } from "../src/server/sched.js";
import { initServerLibrary, uploadsDir } from "../src/server/serverSettings.js";
import { readSources } from "../src/server/sources.js";
import type { Job } from "../src/server/types.js";
import { fakeConvert, fakeProbe, waitFor } from "./helpers.js";

const LAN_HOST = "192.168.1.20:4747";
const OUTSIDE = "Videos must come from the imports folder or an upload.";

const sleep = (ms: number) => new Promise((r) => setTimeout(r, ms));

let dir: string;
let imports: string;
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
    server: { library: dir, password: "correct horse", imports, name: "Home PC" },
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

function list(rel: string) {
  return send({ method: "GET", url: "/api/imports", query: { dir: rel } });
}

function scan(rel: string) {
  return send({ method: "POST", url: "/api/import/scan", payload: { dir: rel } });
}

function importShow(...sources: string[]) {
  const episodes = sources.map((source, i) => ({ source, season: 1, episode: i + 1, title: "Pilot", description: "", aspect: "4:3" }));
  return send({ method: "POST", url: "/api/import/show", payload: { channelFolder: "Cartoon Network", showFolder: "Gumball", episodes } });
}

function importAds(...sources: string[]) {
  return send({ method: "POST", url: "/api/import/ads", payload: { channelFolder: "Cartoon Network", files: sources.map((source) => ({ source, aspect: "4:3" })) } });
}

async function jobs(): Promise<Job[]> {
  return (await send({ method: "GET", url: "/api/jobs" })).json() as Job[];
}

async function converted(): Promise<void> {
  await waitFor(async () => (await jobs()).every((j) => j.state === "done"));
  await waitFor(async () => (await readSources(root)).length > 0);
}

async function put(base: string, rel: string, data = "video"): Promise<string> {
  const file = path.join(base, ...rel.split("/"));
  await mkdir(path.dirname(file), { recursive: true });
  await writeFile(file, data);
  return file;
}

beforeEach(async () => {
  dir = await realpath(await mkdtemp(path.join(os.tmpdir(), "studio-imports-")));
  imports = path.join(dir, "imports");
  await initServerLibrary({ library: dir, password: "correct horse", imports, port: 4747, name: "Home PC" });
  root = path.join(dir, "cable-twii");
  app = build();
  cookie = await signIn(app);
});

afterEach(async () => {
  await app.close();
  await rm(dir, { recursive: true, force: true });
});

test("imports listing hides dot files and non-videos", async () => {
  await mkdir(path.join(imports, "a"));
  await put(imports, "b.mp4");
  await put(imports, ".hidden.mp4");
  await put(imports, "c.txt");
  const res = await list("");
  expect(res.statusCode).toBe(200);
  expect(res.json()).toEqual({
    dir: "",
    entries: [
      { name: "a", kind: "folder" },
      { name: "b.mp4", kind: "video" },
    ],
  });
});

test("imports listing puts folders first, then videos, each sorted by name", async () => {
  await mkdir(path.join(imports, "season 10"));
  await mkdir(path.join(imports, "season 2"));
  await mkdir(path.join(imports, ".hidden"));
  await put(imports, "b.mkv");
  await put(imports, "a.mp4");
  await put(imports, "ep 10.mp4");
  await put(imports, "ep 9.mp4");
  const res = await list("");
  expect(res.json().entries).toEqual([
    { name: "season 2", kind: "folder" },
    { name: "season 10", kind: "folder" },
    { name: "a.mp4", kind: "video" },
    { name: "b.mkv", kind: "video" },
    { name: "ep 9.mp4", kind: "video" },
    { name: "ep 10.mp4", kind: "video" },
  ]);
});

test("imports listing lists a folder inside", async () => {
  await put(imports, "Gumball/S01/ep1.mp4");
  await put(imports, "Gumball/S01/notes.txt");
  await put(imports, "Gumball/trailer.mp4");
  const top = await list("Gumball");
  expect(top.json()).toEqual({
    dir: "Gumball",
    entries: [
      { name: "S01", kind: "folder" },
      { name: "trailer.mp4", kind: "video" },
    ],
  });
  const nested = await list("Gumball/S01");
  expect(nested.json()).toEqual({ dir: "Gumball/S01", entries: [{ name: "ep1.mp4", kind: "video" }] });
});

test("imports listing leaves out links", async () => {
  await mkdir(path.join(dir, "elsewhere"));
  await put(dir, "elsewhere/x.mp4");
  await put(imports, "real.mp4");
  await symlink(path.join(dir, "elsewhere", "x.mp4"), path.join(imports, "link.mp4"));
  await symlink(path.join(dir, "elsewhere"), path.join(imports, "escape"));
  const res = await list("");
  expect(res.json().entries).toEqual([{ name: "real.mp4", kind: "video" }]);
});

test("imports listing is empty for an empty folder", async () => {
  expect((await list("")).json()).toEqual({ dir: "", entries: [] });
  await mkdir(path.join(imports, "empty"));
  expect((await list("empty")).json()).toEqual({ dir: "empty", entries: [] });
});

test("imports listing refuses ..", async () => {
  const res = await list("..");
  expect(res.statusCode).toBe(400);
});

test.each([
  ["a parent", ".."],
  ["a parent after a folder", "a/.."],
  ["a parent before a folder", "../imports"],
  ["a backslash", "a\\.."],
  ["a backslash on its own", "\\"],
  ["an absolute path", "/etc"],
  ["an empty segment", "a//b"],
  ["a trailing slash", "a/"],
  ["a dot folder", ".hidden"],
  ["a dot segment", "a/./b"],
  ["a NUL byte", "a\0b"],
])("imports listing refuses %s", async (_name, rel) => {
  await mkdir(path.join(imports, "a", "b"), { recursive: true });
  await mkdir(path.join(imports, ".hidden"));
  const res = await list(rel);
  expect(res.statusCode).toBe(400);
  expect(res.json().error).toEqual(expect.any(String));
});

test("imports listing refuses a percent-encoded ..", async () => {
  const res = await send({ method: "GET", url: "/api/imports?dir=%2e%2e" });
  expect(res.statusCode).toBe(400);
});

test("imports listing refuses a folder that links out", async () => {
  await mkdir(path.join(dir, "elsewhere"));
  await put(dir, "elsewhere/x.mp4");
  await symlink(path.join(dir, "elsewhere"), path.join(imports, "escape"));
  const res = await list("escape");
  expect(res.statusCode).toBe(400);
  expect(res.body).not.toContain("x.mp4");
});

test("imports listing refuses a folder that links to the imports folder itself", async () => {
  await symlink(imports, path.join(imports, "again"));
  expect((await list("again")).statusCode).toBe(400);
});

test("imports listing refuses a link to a dot folder inside", async () => {
  await put(imports, ".hidden/x.mp4");
  await symlink(path.join(imports, ".hidden"), path.join(imports, "alias"));
  const res = await list("alias");
  expect(res.statusCode).toBe(400);
  expect(res.body).not.toContain("x.mp4");
});

test("imports listing says a missing folder is missing", async () => {
  const res = await list("nope");
  expect(res.statusCode).toBe(404);
  expect(res.json()).toEqual({ error: "That folder does not exist." });
});

test("imports listing says a file is not a folder", async () => {
  await put(imports, "b.mp4");
  const res = await list("b.mp4");
  expect(res.statusCode).toBe(404);
});

test("imports listing without dir lists the imports folder", async () => {
  await put(imports, "b.mp4");
  const res = await send({ method: "GET", url: "/api/imports" });
  expect(res.statusCode).toBe(200);
  expect(res.json()).toEqual({ dir: "", entries: [{ name: "b.mp4", kind: "video" }] });
});

test("imports listing refuses a dir that is not a string", async () => {
  const res = await send({ method: "GET", url: "/api/imports?dir=a&dir=b" });
  expect(res.statusCode).toBe(400);
  expect(res.json()).toEqual({ error: "dir must be a string." });
});

test("listImports", async () => {
  await mkdir(path.join(imports, "a"));
  await put(imports, "b.mp4");
  await put(imports, "c.txt");
  expect(await listImports(imports, "")).toEqual({
    dir: "",
    entries: [
      { name: "a", kind: "folder" },
      { name: "b.mp4", kind: "video" },
    ],
  });
  await expect(listImports(imports, "..")).rejects.toBeInstanceOf(ContainmentError);
  await expect(listImports(imports, "a\\b")).rejects.toBeInstanceOf(ContainmentError);
  await expect(listImports(imports, "nope")).rejects.toMatchObject({ code: "ENOENT" });
});

test("imports routes need a session", async () => {
  const listed = await anonymous({ method: "GET", url: "/api/imports" });
  expect(listed.statusCode).toBe(401);
  expect(listed.json()).toEqual({ error: "Sign in first." });
  const scanned = await anonymous({ method: "POST", url: "/api/import/scan", payload: { dir: "" } });
  expect(scanned.statusCode).toBe(401);
  expect(scanned.json()).toEqual({ error: "Sign in first." });
});

test("imports routes do not exist in drive mode", async () => {
  const drive = build({ server: undefined });
  try {
    const headers = { host: "127.0.0.1:4747", "x-cable-twii": "1" };
    expect((await drive.inject({ method: "GET", url: "/api/imports", headers })).statusCode).toBe(404);
    expect((await drive.inject({ method: "POST", url: "/api/import/scan", headers, payload: { dir: "" } })).statusCode).toBe(404);
  } finally {
    await drive.close();
  }
});

test("import scan returns files", async () => {
  await put(imports, "b.mp4");
  await put(imports, "a/S01E02 Second.mp4");
  await put(imports, "a/S01E01 First.mkv");
  await put(imports, ".hidden.mp4");
  await put(imports, "a/c.txt");
  const res = await scan("");
  expect(res.statusCode).toBe(200);
  const body = res.json();
  expect(body.dir).toBe(imports);
  expect(body.files.map((f: { source: string }) => f.source)).toEqual([path.join(imports, "a", "S01E01 First.mkv"), path.join(imports, "a", "S01E02 Second.mp4"), path.join(imports, "b.mp4")]);
  expect(body.files[0]).toMatchObject({ durationMs: 60000, aspect: "4:3", tooShort: false, guess: { season: 1, episode: 1, title: "First" } });
});

test("scan marks files converted before", async () => {
  const first = await put(imports, "a/S01E01 First.mp4");
  expect((await importShow(first)).statusCode).toBe(200);
  await converted();
  await put(imports, "a/S01E02 Second.mp4");
  const files = (await scan("a")).json().files as { source: string; added?: boolean }[];
  expect(files.map((f) => [path.basename(f.source), f.added])).toEqual([
    ["S01E01 First.mp4", true],
    ["S01E02 Second.mp4", undefined],
  ]);
});

function preview(file: string, headers: Record<string, string> = {}) {
  return send({ method: "GET", url: "/api/import/file", query: { path: file }, headers });
}

test("serves a scanned file with ranges", async () => {
  const file = await put(imports, "a/S01E01 First.mp4", "0123456789abcdef");
  await put(imports, "a/S01E02 Second.mkv", "matroska");
  expect((await scan("a")).statusCode).toBe(200);
  const part = await preview(file, { range: "bytes=0-9" });
  expect(part.statusCode).toBe(206);
  expect(part.body).toBe("0123456789");
  expect(part.headers["content-range"]).toBe("bytes 0-9/16");
  expect(part.headers["accept-ranges"]).toBe("bytes");
  expect(part.headers["content-type"]).toBe("video/mp4");
  const whole = await preview(file);
  expect(whole.statusCode).toBe(200);
  expect(whole.body).toBe("0123456789abcdef");
  expect((await preview(path.join(imports, "a", "S01E02 Second.mkv"))).headers["content-type"]).toBe("video/x-matroska");
  expect((await preview(file, { range: "bytes=99-" })).statusCode).toBe(416);
});

test("refuses a file outside the scanned folders", async () => {
  await put(imports, "a/x.mp4");
  const other = await put(imports, "b/y.mp4");
  expect((await scan("a")).statusCode).toBe(200);
  expect((await preview(other)).statusCode).toBe(404);
  expect((await preview("/etc/hosts")).statusCode).toBe(404);
  expect((await preview(path.join(imports, "a", "..", "b", "y.mp4"))).statusCode).toBe(404);
  expect((await preview("a/x.mp4")).statusCode).toBe(404);
});

test("refuses a symlink out of the folder", async () => {
  await put(imports, "a/x.mp4");
  const outside = await put(dir, "outside/secret.mp4");
  await symlink(outside, path.join(imports, "a", "link.mp4"));
  expect((await scan("a")).statusCode).toBe(200);
  expect((await preview(path.join(imports, "a", "link.mp4"))).statusCode).toBe(404);
});

test("refuses a non-video file", async () => {
  await put(imports, "a/x.mp4");
  const notes = await put(imports, "a/notes.txt");
  expect((await scan("a")).statusCode).toBe(200);
  expect((await preview(notes)).statusCode).toBe(404);
});

test("source preview needs a session", async () => {
  const file = await put(imports, "a/x.mp4");
  expect((await scan("a")).statusCode).toBe(200);
  expect((await anonymous({ method: "GET", url: "/api/import/file", query: { path: file } })).statusCode).toBe(401);
});

test("play needs a session", async () => {
  expect((await anonymous({ method: "GET", url: "/api/media/play", query: { path: "ads/a.mpg" } })).statusCode).toBe(401);
});

test("import scan takes one folder", async () => {
  await put(imports, "b.mp4");
  await put(imports, "a/x.mp4");
  await put(imports, "a/deeper/y.mp4");
  const res = await scan("a");
  expect(res.statusCode).toBe(200);
  expect(res.json().dir).toBe(path.join(imports, "a"));
  expect(res.json().files.map((f: { source: string }) => f.source).sort()).toEqual([path.join(imports, "a", "deeper", "y.mp4"), path.join(imports, "a", "x.mp4")]);
});

test("import scan reports a broken video and leaves the file", async () => {
  const file = await put(imports, "broken.mp4");
  const res = await scan("");
  expect(res.json().files).toHaveLength(1);
  expect(res.json().files[0]).toMatchObject({ source: file, error: "no video stream" });
  expect((await stat(file)).isFile()).toBe(true);
});

test("import scan ignores links", async () => {
  await mkdir(path.join(dir, "elsewhere"));
  await put(dir, "elsewhere/x.mp4");
  await put(imports, "real.mp4");
  await symlink(path.join(dir, "elsewhere", "x.mp4"), path.join(imports, "link.mp4"));
  await symlink(path.join(dir, "elsewhere"), path.join(imports, "escape"));
  const res = await scan("");
  expect(res.json().files.map((f: { source: string }) => f.source)).toEqual([path.join(imports, "real.mp4")]);
});

test.each([
  ["a parent", ".."],
  ["a backslash", "a\\.."],
  ["an absolute path", "/etc"],
  ["a dot folder", ".hidden"],
])("import scan refuses %s", async (_name, rel) => {
  await mkdir(path.join(imports, "a"), { recursive: true });
  await mkdir(path.join(imports, ".hidden"));
  await put(dir, "outside.mp4");
  const res = await scan(rel);
  expect(res.statusCode).toBe(400);
  expect(res.body).not.toContain("outside.mp4");
});

test("import scan refuses a folder that links out", async () => {
  await mkdir(path.join(dir, "elsewhere"));
  await put(dir, "elsewhere/x.mp4");
  await symlink(path.join(dir, "elsewhere"), path.join(imports, "escape"));
  const res = await scan("escape");
  expect(res.statusCode).toBe(400);
  expect(res.body).not.toContain("x.mp4");
});

test("import scan says a missing folder is missing", async () => {
  const res = await scan("nope");
  expect(res.statusCode).toBe(404);
  expect(res.json()).toEqual({ error: "That folder does not exist." });
});

test("import scan needs a dir", async () => {
  const res = await send({ method: "POST", url: "/api/import/scan", payload: {} });
  expect(res.statusCode).toBe(400);
  expect(res.json()).toEqual({ error: "dir must be a string." });
});

test("import refuses a source outside", async () => {
  const res = await importShow("/etc/hosts");
  expect(res.statusCode).toBe(400);
  expect(res.json()).toEqual({ error: OUTSIDE });
  expect(await jobs()).toEqual([]);
});

test("ads import refuses a source outside", async () => {
  const res = await importAds("/etc/hosts");
  expect(res.statusCode).toBe(400);
  expect(res.json()).toEqual({ error: OUTSIDE });
  expect(await jobs()).toEqual([]);
});

test.each([
  ["the imports folder itself", () => imports],
  ["a sibling that starts with the same name", () => `${imports}-other/ep.mp4`],
  ["a parent step out of the imports folder", () => path.join(imports, "..", "ep.mp4")],
  ["a raw parent step out of the imports folder", () => `${imports}/../ep.mp4`],
  ["a backslash parent step", () => `${imports}\\..\\ep.mp4`],
  ["the library", () => path.join(dir, "ep.mp4")],
  ["the cable-twii folder", () => path.join(root, "ads", "ep.mp4")],
  ["the uploads folder itself", () => uploadsDir(dir)],
  ["a parent step out of the uploads folder", () => path.join(uploadsDir(dir), "..", "ep.mp4")],
])("import refuses a source in %s", async (_name, source) => {
  const res = await importShow(source());
  expect(res.statusCode).toBe(400);
  expect(res.json()).toEqual({ error: OUTSIDE });
  expect((await importAds(source())).statusCode).toBe(400);
  expect(await jobs()).toEqual([]);
});

test("import refuses a source that is a link out of the imports folder", async () => {
  const outside = await put(dir, "outside.mp4");
  const link = path.join(imports, "link.mp4");
  await symlink(outside, link);
  const res = await importShow(link);
  expect(res.statusCode).toBe(400);
  expect(res.json()).toEqual({ error: OUTSIDE });
  expect((await importAds(link)).statusCode).toBe(400);
  expect(await jobs()).toEqual([]);
});

test("import refuses a source under a folder that links out of the imports folder", async () => {
  await put(dir, "elsewhere/x.mp4");
  await symlink(path.join(dir, "elsewhere"), path.join(imports, "escape"));
  const res = await importShow(path.join(imports, "escape", "x.mp4"));
  expect(res.statusCode).toBe(400);
  expect(res.json()).toEqual({ error: OUTSIDE });
});

test("import refuses a link that points nowhere", async () => {
  const link = path.join(imports, "dangling.mp4");
  await symlink(path.join(dir, "not-there.mp4"), link);
  expect((await importShow(link)).statusCode).toBe(400);
});

test("import refuses the whole batch when one source is outside", async () => {
  const inside = await put(imports, "ep.mp4");
  const res = await importShow(inside, "/etc/hosts");
  expect(res.statusCode).toBe(400);
  expect(await jobs()).toEqual([]);
});

test("import takes a missing file in the imports folder as a rejected one", async () => {
  const missing = path.join(imports, "broken-gone.mp4");
  const res = await importShow(missing);
  expect(res.statusCode).toBe(200);
  expect(res.json().jobs).toEqual([]);
  expect(res.json().rejected).toEqual([{ source: missing, reason: expect.stringContaining("could not read the video") }]);
});

test("ads import takes a file from the imports folder", async () => {
  const file = await put(imports, "ad.mp4");
  const res = await importAds(file);
  expect(res.statusCode).toBe(200);
  expect(res.json().jobs).toHaveLength(1);
  await converted();
});

test("import takes a file from an upload", async () => {
  const file = await put(uploadsDir(dir), "batch-1/ep.mp4");
  const res = await importShow(file);
  expect(res.statusCode).toBe(200);
  expect(res.json().jobs).toHaveLength(1);
  await converted();
  await waitFor(async () => !(await stat(file).then(() => true, () => false)));
});

test("isAllowedSource takes folders given through a link", async () => {
  const linkDir = await realpath(await mkdtemp(path.join(os.tmpdir(), "studio-imports-link-")));
  const importsLink = path.join(linkDir, "imports");
  const uploadsLink = path.join(linkDir, "uploads");
  await symlink(imports, importsLink);
  await symlink(uploadsDir(dir), uploadsLink);
  try {
    await put(imports, "x.mp4");
    await put(uploadsDir(dir), "batch-1/x.mp4");
    const outside = await put(dir, "outside.mp4");
    await symlink(outside, path.join(imports, "escape.mp4"));
    expect(await isAllowedSource(importsLink, uploadsLink, path.join(importsLink, "x.mp4"))).toBe(true);
    expect(await isAllowedSource(importsLink, uploadsLink, path.join(uploadsLink, "batch-1", "x.mp4"))).toBe(true);
    expect(await isAllowedSource(importsLink, uploadsLink, path.join(importsLink, "escape.mp4"))).toBe(false);
    expect(await isAllowedSource(importsLink, uploadsLink, outside)).toBe(false);
  } finally {
    await rm(linkDir, { recursive: true, force: true });
  }
});

test("isAllowedSource still takes imports when the uploads folder is gone", async () => {
  const file = await put(imports, "x.mp4");
  expect(await isAllowedSource(imports, path.join(dir, "no-uploads"), file)).toBe(true);
});

test("isAllowedSource refuses a relative path", async () => {
  expect(await isAllowedSource(imports, uploadsDir(dir), path.relative(process.cwd(), path.join(imports, "x.mp4")))).toBe(false);
});

test("a library given through a link scans and imports its own files", async () => {
  const linkDir = await realpath(await mkdtemp(path.join(os.tmpdir(), "studio-imports-link-")));
  const link = path.join(linkDir, "tv");
  await symlink(dir, link);
  await put(imports, "Gumball/ep.mp4");
  const linked = build({ server: { library: link, password: "correct horse", imports: path.join(link, "imports"), name: "Home PC" } });
  try {
    const headers = { host: LAN_HOST, "x-cable-twii": "1", cookie: await signIn(linked) };
    const scanned = await linked.inject({ method: "POST", url: "/api/import/scan", headers, payload: { dir: "" } });
    expect(scanned.statusCode).toBe(200);
    expect(scanned.json().dir).toBe(imports);
    const episodes = scanned.json().files.map((f: { source: string }, i: number) => ({ source: f.source, season: 1, episode: i + 1, title: "Pilot", description: "", aspect: "4:3" }));
    expect(episodes).toHaveLength(1);
    const imported = await linked.inject({ method: "POST", url: "/api/import/show", headers, payload: { channelFolder: "Cartoon Network", showFolder: "Gumball", episodes } });
    expect(imported.statusCode).toBe(200);
    expect(imported.json().jobs).toHaveLength(1);
    await waitFor(async () => ((await linked.inject({ method: "GET", url: "/api/jobs", headers })).json() as Job[]).every((j) => j.state === "done"));
    await waitFor(async () => (await readSources(root)).length > 0);
  } finally {
    await linked.close();
    await rm(linkDir, { recursive: true, force: true });
  }
});

test("imports file is kept after conversion", async () => {
  const file = await put(imports, "Gumball/ep.mp4", "original");
  const imported = await importShow(file);
  expect(imported.statusCode).toBe(200);
  expect(imported.json().jobs).toHaveLength(1);
  await converted();
  await sleep(100);
  expect((await stat(file)).isFile()).toBe(true);
  expect((await readSources(root))[0]).toMatchObject({ source: file, size: 8 });
  expect((await stat(path.dirname(file))).isDirectory()).toBe(true);
});

test("imports files are kept after an ads conversion", async () => {
  const file = await put(imports, "ad.mp4", "original");
  expect((await importAds(file)).statusCode).toBe(200);
  await converted();
  await sleep(100);
  expect((await stat(file)).isFile()).toBe(true);
});
