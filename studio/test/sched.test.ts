import { chmod, cp, mkdtemp, readdir, readFile, rm, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { afterEach, beforeEach, expect, test } from "vitest";
import { parseConfig } from "../src/server/config.js";
import { SchedUsageError, lineup, schedBinPath, validateCandidate, validateRoot } from "../src/server/sched.js";
import type { LineupResult } from "../src/server/types.js";

const FIXTURE = path.resolve(import.meta.dirname, "../../engine/tests/fixtures/root_basic");

let dir: string;
let root: string;

beforeEach(async () => {
  dir = await mkdtemp(path.join(os.tmpdir(), "studio-sched-"));
  root = path.join(dir, "cable-twii");
  await cp(FIXTURE, root, { recursive: true });
});

afterEach(async () => {
  await rm(dir, { recursive: true, force: true });
});

test("schedBinPath points at the engine build unless overridden", () => {
  const saved = process.env.CTW_SCHED_BIN;
  delete process.env.CTW_SCHED_BIN;
  try {
    expect(schedBinPath()).toBe(path.resolve(import.meta.dirname, "../../engine/build/cable-twii-sched"));
    process.env.CTW_SCHED_BIN = "/x/y";
    expect(schedBinPath()).toBe("/x/y");
  } finally {
    if (saved === undefined) delete process.env.CTW_SCHED_BIN;
    else process.env.CTW_SCHED_BIN = saved;
  }
});

test("sched lineup parses", async () => {
  const result = (await lineup(root, 1, "2026-10-20")) as LineupResult;
  expect(result.channel).toBe(1);
  expect(result.date).toBe("2026-10-20");
  expect(result.hash).toMatch(/^[0-9a-f]{16}$/);
  expect(result.airings.length).toBeGreaterThan(0);
  for (let i = 1; i < result.airings.length; i++) {
    const prev = result.airings[i - 1]!;
    expect(result.airings[i]!.start_ms).toBe(prev.start_ms + prev.duration_ms);
  }
});

test("validateRoot ok on the fixture, with files", async () => {
  expect(await validateRoot(root, true)).toEqual({ ok: true, issues: expect.any(Array) });
});

test("validateRoot reports missing files with --check-files", async () => {
  await rm(path.join(root, "ads", "ad-a.mpg"));
  const result = await validateRoot(root, true);
  expect(result.ok).toBe(false);
  expect(result.issues.map((i) => i.code)).toContain("missing_file");
  expect((await validateRoot(root)).ok).toBe(true);
});

test("exit 1 from lineup is a failed result, not a throw", async () => {
  await writeFile(path.join(root, "config.toml"), "version = 1\nseed = ");
  const result = await lineup(root, 1, "2026-10-20");
  expect(result).toMatchObject({ ok: false });
  expect((result as { issues: { code: string }[] }).issues[0]!.code).toBe("parse");
});

test("exit 2 throws with the stderr text", async () => {
  await expect(lineup(root, 42, "2026-10-20")).rejects.toBeInstanceOf(SchedUsageError);
  await expect(lineup(root, 1, "2026-13-45")).rejects.toThrow(/date/i);
});

test("a missing binary throws a clear error", async () => {
  await expect(validateRoot(root, false, path.join(dir, "nope"))).rejects.toThrow(/nope/);
});

test("a failing binary throws", async () => {
  const bin = path.join(dir, "bad.sh");
  await writeFile(bin, "#!/bin/sh\necho boom >&2\nexit 7\n");
  await chmod(bin, 0o755);
  await expect(validateRoot(root, false, bin)).rejects.toThrow(/boom/);
});

test("validateCandidate checks a config without touching the root", async () => {
  const before = await readFile(path.join(root, "config.toml"), "utf8");
  const cfg = parseConfig(before);
  cfg.channel[0]!.slot![0]!.items.push({ type: "random", show: "Nope", count: 1 });
  const bad = await validateCandidate(root, cfg);
  expect(bad.ok).toBe(false);
  expect(bad.issues[0]!.code).toBe("unknown_show");
  expect(await readFile(path.join(root, "config.toml"), "utf8")).toBe(before);
  expect(await validateCandidate(root, parseConfig(before))).toMatchObject({ ok: true });
  const leftovers = (await readdir(os.tmpdir())).filter((n) => n.startsWith("ctw-candidate-"));
  expect(leftovers).toEqual([]);
});

test("validateCandidate without a library uses an empty one", async () => {
  await rm(path.join(root, "library.toml"));
  expect(await validateCandidate(root, { version: 1, seed: 1, channel: [] })).toMatchObject({ ok: true });
});
