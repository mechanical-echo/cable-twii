import { mkdtemp, realpath, rm } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { afterEach, beforeEach, expect, test } from "vitest";
import type { FastifyInstance, InjectOptions } from "fastify";
import { buildApp, type AppOptions } from "../src/server/app.js";
import { ConversionQueue } from "../src/server/queue.js";
import { schedBinPath } from "../src/server/sched.js";
import { initServerLibrary } from "../src/server/serverSettings.js";
import { fakeConvert, fakeProbe } from "./helpers.js";

const LAN_HOST = "192.168.1.20:4747";

let dir: string;
let imports: string;
let app: FastifyInstance;

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

function send(options: InjectOptions) {
  return app.inject({ ...options, headers: { host: LAN_HOST, "x-cable-twii": "1", ...options.headers } });
}

beforeEach(async () => {
  dir = await realpath(await mkdtemp(path.join(os.tmpdir(), "studio-server-")));
  imports = path.join(dir, "imports");
  await initServerLibrary({ library: dir, password: "correct horse", imports, port: 4747, name: "Home PC" });
  app = build();
});

afterEach(async () => {
  await app.close();
  await rm(dir, { recursive: true, force: true });
});

test("the library is the drive", async () => {
  const res = await send({ method: "GET", url: "/api/drive" });
  expect(res.statusCode).toBe(200);
  const info = res.json();
  expect(info.usable).toBe(true);
  expect(info.hasRoot).toBe(true);
  expect(info.rootPath).toBe(path.join(dir, "cable-twii"));
  expect(info.path).toBe(dir);
  expect(info.freeBytes).toBeGreaterThan(0);
});

test("the library root serves the usual routes", async () => {
  const res = await send({ method: "GET", url: "/api/config" });
  expect(res.statusCode).toBe(200);
  expect(res.json().config.version).toBe(1);
});

test.each([
  ["/api/drive"],
  ["/api/drive/pick"],
  ["/api/drive/init"],
  ["/api/drive/done"],
  ["/api/import/pick-source"],
])("POST %s does not exist in server mode", async (url) => {
  const res = await send({ method: "POST", url, payload: { path: dir } });
  expect(res.statusCode).toBe(404);
});

test("GET /api/mode names the server", async () => {
  const res = await send({ method: "GET", url: "/api/mode" });
  expect(res.statusCode).toBe(200);
  expect(res.json()).toEqual({ mode: "server", name: "Home PC" });
});

test("a drive-mode app reports no server name", async () => {
  const drive = build({ server: undefined });
  try {
    const res = await drive.inject({ method: "GET", url: "/api/mode", headers: { host: "127.0.0.1:4747" } });
    expect(res.json()).toEqual({ mode: "drive", name: null });
  } finally {
    await drive.close();
  }
});

test("writes with a foreign Origin are refused", async () => {
  const res = await send({ method: "PUT", url: "/api/config", headers: { origin: "http://evil.example" }, payload: {} });
  expect(res.statusCode).toBe(403);
});

test("writes whose Origin matches the Host get past the guard", async () => {
  const res = await send({ method: "PUT", url: "/api/config", headers: { origin: `http://${LAN_HOST}` }, payload: {} });
  expect(res.statusCode).not.toBe(403);
});

test("writes without the studio header are refused", async () => {
  const res = await app.inject({ method: "PUT", url: "/api/config", headers: { host: LAN_HOST }, payload: {} });
  expect(res.statusCode).toBe(403);
});
