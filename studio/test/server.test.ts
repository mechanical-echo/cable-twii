import { mkdir, mkdtemp, realpath, rm, symlink, writeFile } from "node:fs/promises";
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

function login(target: FastifyInstance, password: string) {
  return target.inject({ method: "POST", url: "/api/login", headers: { host: LAN_HOST, "x-cable-twii": "1" }, payload: { password } });
}

async function signIn(target: FastifyInstance): Promise<string> {
  const res = await login(target, "correct horse");
  expect(res.statusCode).toBe(200);
  return String(res.headers["set-cookie"]).split(";")[0] as string;
}

beforeEach(async () => {
  dir = await realpath(await mkdtemp(path.join(os.tmpdir(), "studio-server-")));
  imports = path.join(dir, "imports");
  await initServerLibrary({ library: dir, password: "correct horse", imports, port: 4747, name: "Home PC" });
  app = build();
  cookie = await signIn(app);
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
  expect(res.json()).toEqual({ mode: "server", name: "Home PC", signedIn: true });
});

test("GET /api/mode works without a session and says so", async () => {
  const res = await anonymous({ method: "GET", url: "/api/mode" });
  expect(res.statusCode).toBe(200);
  expect(res.json()).toEqual({ mode: "server", name: "Home PC", signedIn: false });
});

test("a drive-mode app reports no server name and needs no sign-in", async () => {
  const drive = build({ server: undefined });
  try {
    const res = await drive.inject({ method: "GET", url: "/api/mode", headers: { host: "127.0.0.1:4747" } });
    expect(res.json()).toEqual({ mode: "drive", name: null, signedIn: true });
    const login = await drive.inject({ method: "POST", url: "/api/login", headers: { host: "127.0.0.1:4747", "x-cable-twii": "1" }, payload: { password: "correct horse" } });
    expect(login.statusCode).toBe(404);
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

test("a library given through a symlink is resolved once", async () => {
  const linkDir = await realpath(await mkdtemp(path.join(os.tmpdir(), "studio-server-link-")));
  const link = path.join(linkDir, "tv");
  await symlink(dir, link);
  const linked = build({ server: { library: link, password: "correct horse", imports: path.join(link, "imports"), name: "Home PC" } });
  try {
    const linkedCookie = await signIn(linked);
    const drive = await linked.inject({ method: "GET", url: "/api/drive", headers: { host: LAN_HOST, cookie: linkedCookie } });
    expect(drive.statusCode).toBe(200);
    expect(drive.json().usable).toBe(true);
    expect(drive.json().path).toBe(dir);
    expect(drive.json().rootPath).toBe(path.join(dir, "cable-twii"));
    const config = await linked.inject({ method: "GET", url: "/api/config", headers: { host: LAN_HOST, cookie: linkedCookie } });
    expect(config.statusCode).toBe(200);
  } finally {
    await linked.close();
    await rm(linkDir, { recursive: true, force: true });
  }
});

test("the API needs a session", async () => {
  const res = await anonymous({ method: "GET", url: "/api/config" });
  expect(res.statusCode).toBe(401);
  expect(res.json()).toEqual({ error: "Sign in first." });
});

test("an unknown session is refused", async () => {
  const res = await anonymous({ method: "GET", url: "/api/config", headers: { cookie: "ctw_session=made-up" } });
  expect(res.statusCode).toBe(401);
});

test("media needs a session", async () => {
  const res = await anonymous({ method: "GET", url: "/media/channel.png" });
  expect(res.statusCode).toBe(401);
  expect(res.json()).toEqual({ error: "Sign in first." });
});

test("a percent-encoded path does not get around the session", async () => {
  const res = await anonymous({ method: "GET", url: "/%61pi/config" });
  expect(res.statusCode).toBe(401);
});

test("a wrong password is refused", async () => {
  const res = await anonymous({ method: "POST", url: "/api/login", payload: { password: "wrong horse" } });
  expect(res.statusCode).toBe(401);
  expect(res.json()).toEqual({ error: "Wrong password." });
  expect(res.headers["set-cookie"]).toBeUndefined();
});

test("a login without a password is refused", async () => {
  const res = await anonymous({ method: "POST", url: "/api/login", payload: {} });
  expect(res.statusCode).toBe(400);
  expect(res.json()).toEqual({ error: "password must be a string." });
});

test("logging in sets a host-only session cookie", async () => {
  const res = await anonymous({ method: "POST", url: "/api/login", payload: { password: "correct horse" } });
  expect(res.statusCode).toBe(200);
  expect(res.json()).toEqual({ ok: true });
  expect(res.headers["set-cookie"]).toMatch(/^ctw_session=[A-Za-z0-9_-]{43}; Path=\/; HttpOnly; SameSite=Strict; Max-Age=2592000$/);
});

test("a signed-in cookie opens the API", async () => {
  const mode = await anonymous({ method: "GET", url: "/api/mode", headers: { cookie } });
  expect(mode.json().signedIn).toBe(true);
  const res = await anonymous({ method: "GET", url: "/api/config", headers: { cookie } });
  expect(res.statusCode).toBe(200);
  expect(res.json().config.version).toBe(1);
});

test("signing out ends the session", async () => {
  const out = await send({ method: "POST", url: "/api/logout" });
  expect(out.statusCode).toBe(200);
  expect(out.json()).toEqual({ ok: true });
  expect(out.headers["set-cookie"]).toBe("ctw_session=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
  const res = await anonymous({ method: "GET", url: "/api/config", headers: { cookie } });
  expect(res.statusCode).toBe(401);
  const mode = await anonymous({ method: "GET", url: "/api/mode", headers: { cookie } });
  expect(mode.json().signedIn).toBe(false);
});

test("a rebound host with a matching Origin still needs a session", async () => {
  const res = await anonymous({
    method: "PUT",
    url: "/api/config",
    headers: { host: "evil.example:4747", origin: "http://evil.example:4747" },
    payload: {},
  });
  expect(res.statusCode).toBe(401);
  expect(res.json()).toEqual({ error: "Sign in first." });
});

test("the web files and the Wii interface stay open", async () => {
  const webDir = path.join(dir, "web");
  await mkdir(webDir);
  await writeFile(path.join(webDir, "index.html"), "<h1>Studio</h1>");
  const web = build({ webDir });
  try {
    const page = await web.inject({ method: "GET", url: "/", headers: { host: LAN_HOST } });
    expect(page.statusCode).toBe(200);
    expect(page.body).toBe("<h1>Studio</h1>");
    const wii = await web.inject({ method: "GET", url: "/wii/v1/info", headers: { host: LAN_HOST } });
    expect(wii.statusCode).not.toBe(401);
  } finally {
    await web.close();
  }
});

test.each([
  ["no cookie", {}],
  ["an unknown token", { cookie: "ctw_session=made-up" }],
])("signing out with %s still clears the cookie", async (_name, headers) => {
  const res = await anonymous({ method: "POST", url: "/api/logout", headers });
  expect(res.statusCode).toBe(200);
  expect(res.json()).toEqual({ ok: true });
  expect(res.headers["set-cookie"]).toBe("ctw_session=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
});

test("five wrong passwords lock sign-in for a minute", async () => {
  let now = 1_000_000;
  const timed = build({ now: () => now });
  try {
    for (let i = 0; i < 5; i++) expect((await login(timed, "wrong horse")).statusCode).toBe(401);
    const locked = await login(timed, "correct horse");
    expect(locked.statusCode).toBe(429);
    expect(locked.json()).toEqual({ error: "Too many tries. Wait a minute and try again." });
    expect(locked.headers["set-cookie"]).toBeUndefined();
    now += 60_001;
    const open = await login(timed, "correct horse");
    expect(open.statusCode).toBe(200);
    expect(open.headers["set-cookie"]).toMatch(/^ctw_session=/);
  } finally {
    await timed.close();
  }
});

test("the lock covers every client, not one address", async () => {
  for (let i = 0; i < 5; i++) {
    const res = await anonymous({ method: "POST", url: "/api/login", remoteAddress: `192.168.1.${30 + i}`, payload: { password: "wrong horse" } });
    expect(res.statusCode).toBe(401);
  }
  const res = await anonymous({ method: "POST", url: "/api/login", remoteAddress: "192.168.1.99", payload: { password: "correct horse" } });
  expect(res.statusCode).toBe(429);
});
