import { mkdir, mkdtemp, readFile, readdir, rm, stat, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { afterEach, beforeEach, expect, test } from "vitest";
import { initServerLibrary, parseServerSettings, readServerSettings, serverSettingsPath, uploadsDir } from "../src/server/serverSettings.js";

const FILE = "/etc/cable-twii-server.toml";

let dir: string;

beforeEach(async () => {
  dir = await mkdtemp(path.join(os.tmpdir(), "studio-server-settings-"));
});

afterEach(async () => {
  await rm(dir, { recursive: true, force: true });
});

test("minimal settings get the defaults", () => {
  const s = parseServerSettings('library = "/srv/tv"\npassword = "correct horse"\n', FILE);
  expect(s).toEqual({ library: "/srv/tv", password: "correct horse", imports: path.join("/srv/tv", "imports"), port: 4747, name: os.hostname() });
});

test("every key can be set", () => {
  const text = 'library = "/srv/tv"\npassword = "correct horse"\nimports = "/srv/incoming"\nport = 8080\nname = "Home PC"\n';
  expect(parseServerSettings(text, FILE)).toEqual({ library: "/srv/tv", password: "correct horse", imports: "/srv/incoming", port: 8080, name: "Home PC" });
});

test.each([
  ["library", 'library = "tv"\npassword = "correct horse"\n'],
  ["library", 'password = "correct horse"\n'],
  ["password", 'library = "/srv/tv"\npassword = "1234567"\n'],
  ["password", 'library = "/srv/tv"\n'],
  ["imports", 'library = "/srv/tv"\npassword = "correct horse"\nimports = "incoming"\n'],
  ["port", 'library = "/srv/tv"\npassword = "correct horse"\nport = 0\n'],
  ["port", 'library = "/srv/tv"\npassword = "correct horse"\nport = 65536\n'],
  ["port", 'library = "/srv/tv"\npassword = "correct horse"\nport = "4747"\n'],
  ["name", 'library = "/srv/tv"\npassword = "correct horse"\nname = "  "\n'],
])("a bad %s is refused naming the file and the key", (key, text) => {
  expect(() => parseServerSettings(text, FILE)).toThrow(new RegExp(`${FILE}.*${key}`));
});

test("text that is not TOML is refused naming the file", () => {
  expect(() => parseServerSettings("library = ", FILE)).toThrow(new RegExp(`${FILE} is not valid TOML`));
});

test("the settings path follows CTW_SERVER_SETTINGS, else the home folder", () => {
  expect(serverSettingsPath({ CTW_SERVER_SETTINGS: "/etc/ctw.toml" })).toBe("/etc/ctw.toml");
  expect(serverSettingsPath({})).toBe(path.join(os.homedir(), ".cable-twii-server.toml"));
  expect(serverSettingsPath({ CTW_SERVER_SETTINGS: "" })).toBe(path.join(os.homedir(), ".cable-twii-server.toml"));
});

test("readServerSettings reads the file and names it when it is missing", async () => {
  const file = path.join(dir, "server.toml");
  await expect(readServerSettings(file)).rejects.toThrow(file);
  await writeFile(file, `library = ${JSON.stringify(dir)}\npassword = "correct horse"\n`);
  expect((await readServerSettings(file)).library).toBe(dir);
});

test("uploads live in a hidden folder of the library, outside the root", () => {
  expect(uploadsDir("/srv/tv")).toBe(path.join("/srv/tv", ".uploads"));
});

test("initServerLibrary creates the folders and leaves existing files alone", async () => {
  const settings = { library: path.join(dir, "tv"), password: "correct horse", imports: path.join(dir, "incoming"), port: 4747, name: "Home PC" };
  await mkdir(settings.library);
  await initServerLibrary(settings);
  for (const folder of ["cable-twii", "cable-twii/ads", "cable-twii/.studio", ".uploads"]) {
    expect((await stat(path.join(settings.library, ...folder.split("/")))).isDirectory()).toBe(true);
  }
  expect((await stat(settings.imports)).isDirectory()).toBe(true);
  await writeFile(path.join(settings.library, "cable-twii", "config.toml"), "kept");
  await initServerLibrary(settings);
  expect(await readFile(path.join(settings.library, "cable-twii", "config.toml"), "utf8")).toBe("kept");
});

test("initServerLibrary refuses a library that does not exist and creates nothing", async () => {
  const settings = { library: path.join(dir, "missing"), password: "correct horse", imports: path.join(dir, "incoming"), port: 4747, name: "Home PC" };
  await expect(initServerLibrary(settings)).rejects.toThrow(`library ${settings.library}`);
  expect(await readdir(dir)).toEqual([]);
});

test("initServerLibrary refuses a library that is a file", async () => {
  const library = path.join(dir, "tv");
  await writeFile(library, "not a folder");
  await expect(initServerLibrary({ library, password: "correct horse", imports: path.join(dir, "incoming"), port: 4747, name: "Home PC" })).rejects.toThrow(`library ${library}`);
  expect(await readdir(dir)).toEqual(["tv"]);
});
