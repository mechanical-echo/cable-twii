import { cp, lstat, mkdtemp, readFile, rm, stat, symlink, utimes, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { afterEach, beforeEach, expect, test } from "vitest";
import { ContainmentError } from "../src/server/contain.js";
import { ConfigConflictError, parseConfig, readConfig, serializeConfig, writeConfig } from "../src/server/config.js";
import { validateRoot } from "../src/server/sched.js";
import { tomlString } from "../src/server/toml.js";
import type { Config } from "../src/server/types.js";

const FIXTURE = path.resolve(import.meta.dirname, "../../engine/tests/fixtures/root_basic");

let root: string;

beforeEach(async () => {
  root = await mkdtemp(path.join(os.tmpdir(), "studio-config-"));
});

afterEach(async () => {
  await rm(root, { recursive: true, force: true });
});

const sample: Config = {
  version: 1,
  seed: 1234,
  channel: [
    {
      number: 1,
      name: "Cartoon Network",
      category: "Kids",
      folder: "Cartoon Network",
      icon: "channel.png",
      ads: true,
      mode: "timed",
      slot: [
        {
          at: "07:00",
          items: [
            { type: "random-run", show: "Gumball", count: 3 },
            { type: "ads", count: 1 },
            { type: "random", show: "Adventure Time", season: 5, count: 5 },
            { type: "ads", count: 2 },
            { type: "episode", show: "Ben 10", season: 1, episode: 5 },
            { type: "random", show: "Gumball", count: 2, from: "Cartoon Network" },
            { type: "ad", path: "ads/ad-b.mpg" },
          ],
        },
        { at: "19:00", items: [{ type: "fill", show: "Adventure Time", season: 5, ads_between: 2, from: "Cartoon Network" }] },
      ],
    },
    {
      number: 2,
      name: "MTV",
      category: "Music",
      folder: "mtv-00s",
      ads: false,
      mode: "looping",
      loop: [
        { type: "random", show: "Music Videos", count: 3 },
        { type: "random-run", show: "Music Videos", season: 0, count: 2, from: "mtv-00s" },
        { type: "episode", show: "Music Videos", season: 0, episode: 3 },
      ],
    },
  ],
};

async function fixtureRoot(): Promise<string> {
  const dst = path.join(root, "cable-twii");
  await cp(FIXTURE, dst, { recursive: true });
  return dst;
}

test("config round trip", () => {
  expect(parseConfig(serializeConfig(sample))).toEqual(sample);
});

test("serializeConfig sorts channels and slots and emits the formats shape", () => {
  const unsorted: Config = {
    version: 1,
    seed: 7,
    channel: [
      { ...sample.channel[1]!, number: 9 },
      { ...sample.channel[0]!, slot: [...sample.channel[0]!.slot!].reverse() },
    ],
  };
  const text = serializeConfig(unsorted);
  expect(text.startsWith("version = 1\nseed = 7\n")).toBe(true);
  expect(text.indexOf("number = 1")).toBeLessThan(text.indexOf("number = 9"));
  expect(text.indexOf('at = "07:00"')).toBeLessThan(text.indexOf('at = "19:00"'));
  expect(text).toContain("[[channel.slot]]");
  expect(text).toContain('{ type = "episode", show = "Ben 10", season = 1, episode = 5 }');
  expect(text).toContain('{ type = "ads", count = 1 }');
  expect(text).toContain('{ type = "ad", path = "ads/ad-b.mpg" }');
  expect(text).toContain("loop = [");
  expect(text).not.toMatch(/\d\.\d/);
  expect(text).not.toMatch(/[ \t]+$/m);
});

test("serializeConfig drops keys that do not belong to the item type", () => {
  const cfg = {
    version: 1,
    seed: 1,
    channel: [
      {
        number: 1, name: "A", category: "B", ads: false, mode: "looping",
        loop: [{ type: "ads", count: 2, show: "X", path: "p" }],
      },
    ],
  } as unknown as Config;
  expect(serializeConfig(cfg)).toContain('loop = [\n  { type = "ads", count = 2 },\n]');
});

test("serializeConfig refuses non-integers", () => {
  expect(() => serializeConfig({ ...sample, seed: 1.5 })).toThrow(/integer/);
});

test("parseConfig fills defaults and rejects malformed input", () => {
  const cfg = parseConfig('version = 1\nseed = 3\n\n[[channel]]\nnumber = 1\nname = "A"\ncategory = "B"\nads = true\nmode = "looping"\nloop = [ { type = "random", show = "S" }, { type = "ads" } ]\n');
  expect(cfg.channel[0]!.loop).toEqual([{ type: "random", show: "S", count: 1 }, { type: "ads", count: 1 }]);
  expect(parseConfig("version = 1\nseed = 3\n")).toEqual({ version: 1, seed: 3, channel: [] });
  expect(() => parseConfig("version = 1\nseed = ")).toThrow(/config.toml/);
  expect(() => parseConfig("version = 2\nseed = 1\n")).toThrow(/version/);
  expect(() => parseConfig('version = 1\nseed = 1\n[[channel]]\nnumber = "x"\n')).toThrow(/number/);
  expect(() => parseConfig('version = 1\nseed = 1\n[[channel]]\nnumber = 1\nname = "A"\ncategory = "B"\nads = true\nmode = "looping"\nloop = [ { type = "nope" } ]\n')).toThrow(/type/);
});

test("tomlString escapes quotes, backslashes, controls and keeps emoji", () => {
  expect(tomlString('a"b\\c\td\u{1F600}')).toBe('"a\\"b\\\\c\\u0009d\u{1F600}"');
});

test("serialized config is valid for the engine", async () => {
  const dst = await fixtureRoot();
  await writeFile(path.join(dst, "config.toml"), serializeConfig(sample));
  const result = await validateRoot(dst);
  expect(result.issues.filter((i) => i.level === "error")).toEqual([]);
  expect(result.ok).toBe(true);
});

test("serialized config with tricky strings validates in the engine", async () => {
  const dst = await fixtureRoot();
  const cfg: Config = {
    version: 1,
    seed: 99,
    channel: [
      { number: 3, name: 'Tab\there "quoted" \u{1F4FA}', category: "Back\\slash", folder: "mtv-00s", ads: false, mode: "looping", loop: [{ type: "random", show: "Music Videos", count: 1 }] },
    ],
  };
  const text = serializeConfig(cfg);
  await writeFile(path.join(dst, "config.toml"), text);
  const result = await validateRoot(dst);
  expect(result.ok).toBe(true);
  expect(parseConfig(text)).toEqual(cfg);
});

test("readConfig on a missing file returns a fresh config", async () => {
  const { config, mtimeMs } = await readConfig(root);
  expect(mtimeMs).toBe(0);
  expect(config.version).toBe(1);
  expect(config.channel).toEqual([]);
  expect(Number.isInteger(config.seed)).toBe(true);
  expect(config.seed).toBeGreaterThanOrEqual(0);
  expect(config.seed).toBeLessThan(2 ** 31);
});

test("readConfig reads an existing file with an integer mtime", async () => {
  const dst = await fixtureRoot();
  const { config, mtimeMs } = await readConfig(dst);
  expect(config.channel.map((c) => c.number)).toEqual([1, 2]);
  expect(mtimeMs).toBe(Math.trunc((await stat(path.join(dst, "config.toml"))).mtimeMs));
});

test("writeConfig on a missing file with mtime 0 writes it", async () => {
  const mtimeMs = await writeConfig(root, sample, 0);
  expect(Number.isInteger(mtimeMs)).toBe(true);
  expect(await readFile(path.join(root, "config.toml"), "utf8")).toBe(serializeConfig(sample));
  await expect(stat(path.join(root, "config.bak.toml"))).rejects.toThrow();
  expect((await readConfig(root)).mtimeMs).toBe(mtimeMs);
});

test("config_save_conflict", async () => {
  const dst = await fixtureRoot();
  const { config, mtimeMs } = await readConfig(dst);
  const later = new Date(mtimeMs + 5000);
  await utimes(path.join(dst, "config.toml"), later, later);
  await expect(writeConfig(dst, config, mtimeMs)).rejects.toBeInstanceOf(ConfigConflictError);
});

test("writeConfig keeps backup", async () => {
  const dst = await fixtureRoot();
  const before = await readFile(path.join(dst, "config.toml"), "utf8");
  const { config, mtimeMs } = await readConfig(dst);
  await writeConfig(dst, { ...config, seed: 5 }, mtimeMs);
  expect(await readFile(path.join(dst, "config.bak.toml"), "utf8")).toBe(before);
  expect((await readConfig(dst)).config.seed).toBe(5);
});

test("writeConfig refuses a symlinked config.toml and reads nothing through it", async () => {
  const outside = await mkdtemp(path.join(os.tmpdir(), "studio-config-outside-"));
  try {
    const secret = path.join(outside, "secret.txt");
    await writeFile(secret, "secret");
    await symlink(secret, path.join(root, "config.toml"));
    const mtimeMs = Math.trunc((await stat(secret)).mtimeMs);
    await expect(writeConfig(root, sample, mtimeMs)).rejects.toBeInstanceOf(ContainmentError);
    expect(await readFile(secret, "utf8")).toBe("secret");
    expect((await lstat(path.join(root, "config.toml"))).isSymbolicLink()).toBe(true);
    await expect(stat(path.join(root, "config.bak.toml"))).rejects.toThrow();
  } finally {
    await rm(outside, { recursive: true, force: true });
  }
});

test("writeConfig refuses a symlinked config.bak.toml", async () => {
  const outside = await mkdtemp(path.join(os.tmpdir(), "studio-config-outside-"));
  try {
    const dst = await fixtureRoot();
    const secret = path.join(outside, "secret.txt");
    await writeFile(secret, "secret");
    await symlink(outside, path.join(dst, "linked"));
    await symlink(path.join(dst, "linked", "secret.txt"), path.join(dst, "config.bak.toml"));
    const { config, mtimeMs } = await readConfig(dst);
    await expect(writeConfig(dst, { ...config, seed: 9 }, mtimeMs)).rejects.toBeInstanceOf(ContainmentError);
    expect(await readFile(secret, "utf8")).toBe("secret");
  } finally {
    await rm(outside, { recursive: true, force: true });
  }
});
