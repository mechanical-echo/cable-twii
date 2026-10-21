import { mkdir, mkdtemp, readFile, readdir, rm, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { afterEach, beforeEach, expect, test } from "vitest";
import { needsConversion, readSources, upsertSource, writeSources } from "../src/server/sources.js";
import type { SourceRecord } from "../src/server/types.js";

let dir: string;

beforeEach(async () => {
  dir = await mkdtemp(path.join(os.tmpdir(), "studio-sources-"));
});

afterEach(async () => {
  await rm(dir, { recursive: true, force: true });
});

const rec = (target: string, size = 1): SourceRecord => ({ target, source: `/m/${target}`, size, mtimeMs: 1760000000000, durationMs: 5000 });

test("needsConversion", () => {
  expect(needsConversion(undefined, { size: 1, mtimeMs: 1 }, false)).toBe(true);
  expect(needsConversion({ target: "t", source: "s", size: 1, mtimeMs: 1, durationMs: 1 }, { size: 1, mtimeMs: 1 }, true)).toBe(false);
  expect(needsConversion({ target: "t", source: "s", size: 1, mtimeMs: 1, durationMs: 1 }, { size: 2, mtimeMs: 1 }, true)).toBe(true);
  expect(needsConversion({ target: "t", source: "s", size: 1, mtimeMs: 1, durationMs: 1 }, { size: 1, mtimeMs: 1 }, false)).toBe(true);
});

test("needsConversion compares truncated mtime", () => {
  expect(needsConversion({ target: "t", source: "s", size: 1, mtimeMs: 5, durationMs: 1 }, { size: 1, mtimeMs: 5.75 }, true)).toBe(false);
});

test("sources round trip", async () => {
  const recs = [rec("b.mpg"), rec("a.mpg", 2)];
  await writeSources(dir, recs);
  expect(await readSources(dir)).toEqual([rec("a.mpg", 2), rec("b.mpg")]);
  const text = await readFile(path.join(dir, ".studio", "sources.toml"), "utf8");
  expect(text.startsWith("version = 1")).toBe(true);
  expect(await readdir(path.join(dir, ".studio"))).toEqual(["sources.toml"]);
});

test("readSources returns empty when missing", async () => {
  expect(await readSources(dir)).toEqual([]);
});

test("damaged sources.toml throws", async () => {
  await mkdir(path.join(dir, ".studio"));
  const file = path.join(dir, ".studio", "sources.toml");
  await writeFile(file, "version = [[[");
  await expect(readSources(dir)).rejects.toThrow(/damaged/);
  await writeFile(file, "version = 2\n");
  await expect(readSources(dir)).rejects.toThrow(/damaged/);
  expect(await readFile(file, "utf8")).toBe("version = 2\n");
});

test("upsertSource replaces by target and survives parallel calls", async () => {
  await Promise.all([rec("a.mpg"), rec("b.mpg"), rec("c.mpg"), rec("a.mpg", 9)].map((r) => upsertSource(dir, r)));
  const recs = await readSources(dir);
  expect(recs.map((r) => r.target)).toEqual(["a.mpg", "b.mpg", "c.mpg"]);
});

test("non-integer numbers make sources.toml damaged", async () => {
  await mkdir(path.join(dir, ".studio"));
  await writeFile(
    path.join(dir, ".studio", "sources.toml"),
    'version = 1\n[[source]]\ntarget = "a.mpg"\nsource = "/s"\nsize = 1.5\nmtimeMs = 1\ndurationMs = 1\n',
  );
  await expect(readSources(dir)).rejects.toThrow(/damaged/);
});
