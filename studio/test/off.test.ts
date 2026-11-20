import { mkdir, mkdtemp, readFile, rm, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { afterEach, beforeEach, expect, test } from "vitest";
import { readOff, writeOff } from "../src/server/off.js";

let root: string;

beforeEach(async () => {
  root = await mkdtemp(path.join(os.tmpdir(), "studio-off-"));
});

afterEach(async () => {
  await rm(root, { recursive: true, force: true });
});

test("reads and writes the off list", async () => {
  await writeOff(root, new Set(["b/x.mpg", "a/y.mpg"]));
  expect([...(await readOff(root))]).toEqual(["a/y.mpg", "b/x.mpg"]);
  expect(await readFile(path.join(root, ".studio", "off.toml"), "utf8")).toContain("version = 1");
});

test("missing file is empty", async () => {
  expect((await readOff(root)).size).toBe(0);
});

test("damaged file is an error", async () => {
  await mkdir(path.join(root, ".studio"));
  await writeFile(path.join(root, ".studio", "off.toml"), "version = [");
  await expect(readOff(root)).rejects.toThrow("off.toml");
});
