import { mkdir, mkdtemp, rm, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { afterEach, beforeEach, expect, test } from "vitest";
import { SHRINK_ABOVE_KBPS, needsShrink, shrinkCandidates } from "../src/server/shrink.js";
import type { Library } from "../src/server/types.js";

let dir: string;

// 60 s files: 1300 kbit/s is 9,750,000 bytes
const SIZES: Record<string, number> = { big: 12_750_000, small: 8_250_000, busy: 12_750_000, narrow: 12_750_000 };

function ep(name: string, n: number) {
  return { season: 1, episode: n, title: name, description: "", path: `CN/Gumball/${name}.mpg`, duration_ms: 60000 };
}

const lib: Library = {
  version: 1,
  generated: new Date(0),
  ad: [{ path: "Ads/spot.mpg", channel: "Ads", duration_ms: 30000 }],
  show: [{ channel: "Cartoon Network", folder: "Gumball", name: "Gumball", icon: "", episode: [ep("big", 1), ep("small", 2), ep("busy", 3), ep("missing", 4), ep("narrow", 5)] }],
};

beforeEach(async () => {
  dir = await mkdtemp(path.join(os.tmpdir(), "studio-shrink-"));
  await mkdir(path.join(dir, "CN", "Gumball"), { recursive: true });
  await mkdir(path.join(dir, "Ads"), { recursive: true });
  for (const [name, size] of Object.entries(SIZES)) await writeFile(path.join(dir, "CN", "Gumball", `${name}.mpg`), Buffer.alloc(size));
  await writeFile(path.join(dir, "Ads", "spot.mpg"), Buffer.alloc(6_000_000));
});

afterEach(async () => {
  await rm(dir, { recursive: true, force: true });
});

test("needsShrink is true only above the threshold", () => {
  expect(SHRINK_ABOVE_KBPS).toBe(1300);
  expect(needsShrink(1700)).toBe(true);
  expect(needsShrink(1100)).toBe(false);
  expect(needsShrink(1300)).toBe(false);
  expect(needsShrink(null)).toBe(false);
});

test("shrinkCandidates estimates from file size and library duration, skipping busy and missing files", async () => {
  const found = await shrinkCandidates(dir, lib, new Set(["CN/Gumball/busy.mpg"]));
  expect(found).toEqual(["Ads/spot.mpg", "CN/Gumball/big.mpg", "CN/Gumball/narrow.mpg"]);
});
