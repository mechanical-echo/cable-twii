import { spawnSync } from "node:child_process";
import { existsSync } from "node:fs";
import { mkdtemp, rm, writeFile, readFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { afterEach, beforeEach, expect, test } from "vitest";
import { aspectFor, conversionArgs, convert, displayAspectOf, isTooShort, probe } from "../src/server/ffmpeg.js";
import { MIN_DURATION_MS } from "../src/server/videoSettings.js";

let dir: string;

beforeEach(async () => {
  dir = await mkdtemp(path.join(os.tmpdir(), "studio-ffmpeg-"));
});

afterEach(async () => {
  await rm(dir, { recursive: true, force: true });
});

function makeVideo(name: string, size: string, seconds: number, extra: string[] = []): string {
  const out = path.join(dir, name);
  const r = spawnSync("ffmpeg", ["-y", "-v", "error", "-f", "lavfi", "-i", `testsrc2=s=${size}:d=${seconds}`, "-f", "lavfi", "-i", `sine=d=${seconds}`, "-shortest", ...extra, out]);
  if (r.status !== 0) throw new Error(String(r.stderr));
  return out;
}

function makeAudioOnly(name: string): string {
  const out = path.join(dir, name);
  const r = spawnSync("ffmpeg", ["-y", "-v", "error", "-f", "lavfi", "-i", "sine=d=1", out]);
  if (r.status !== 0) throw new Error(String(r.stderr));
  return out;
}

test("args 16:9", () => {
  const a = conversionArgs("in.mkv", "out.mpg.part", "16:9", 0);
  expect(a).toEqual(expect.arrayContaining(["-c:v", "mpeg1video", "-b:v", "1500k", "-maxrate", "2000k", "-bufsize", "1000k", "-g", "15", "-c:a", "mp2", "-ar", "48000", "-ac", "2", "-b:a", "192k", "-f", "mpeg"]));
  expect(a.join(" ")).toContain("scale=640:360");
  expect(a.join(" ")).toContain("pad=640:360:(ow-iw)/2:(oh-ih)/2,fps=30");
  expect(a.join(" ")).toContain("loudnorm=I=-16:TP=-1.5:LRA=11");
  expect(a.slice(0, 5)).toEqual(["-y", "-nostdin", "-progress", "pipe:1", "-nostats"]);
  expect(a.at(-1)).toBe("file:out.mpg.part");
  expect(a).toEqual(expect.arrayContaining(["-i", "file:in.mkv", "-map", "0:0", "0:a:0?", "-sn", "-dn"]));
});
test("args 4:3", () => expect(conversionArgs("a", "b", "4:3", 0).join(" ")).toContain("scale=480:360:force_original_aspect_ratio=decrease,pad=480:360"));
test("aspectFor", () => {
  expect(aspectFor(16 / 9)).toBe("16:9");
  expect(aspectFor(4 / 3)).toBe("4:3");
  expect(aspectFor(1.5)).toBe("16:9");
});
test("isTooShort", () => {
  expect(MIN_DURATION_MS).toBe(1000);
  expect(isTooShort(999)).toBe(true);
  expect(isTooShort(1000)).toBe(false);
});
test("probe reads duration and aspect", async () => {
  const src = makeVideo("a.mkv", "320x240", 1.5);
  const p = await probe(src);
  expect(p.durationMs).toBeGreaterThanOrEqual(1400);
  expect(p.durationMs).toBeLessThanOrEqual(1700);
  expect(Number.isInteger(p.durationMs)).toBe(true);
  expect(p.width).toBe(320);
  expect(p.height).toBe(240);
  expect(p.displayAspect).toBeCloseTo(4 / 3, 3);
});
test("displayAspectOf prefers declared ratio and falls back to SAR", () => {
  expect(displayAspectOf({ width: 320, height: 240, display_aspect_ratio: "16:9" })).toBeCloseTo(16 / 9, 5);
  expect(displayAspectOf({ width: 320, height: 240, display_aspect_ratio: "0:1", sample_aspect_ratio: "2:1" })).toBeCloseTo(8 / 3, 5);
  expect(displayAspectOf({ width: 320, height: 240, sample_aspect_ratio: "0:1" })).toBeCloseTo(4 / 3, 5);
  expect(displayAspectOf({ width: 320, height: 240 })).toBeCloseTo(4 / 3, 5);
});
function makeCover(): string {
  const png = path.join(dir, "cover.png");
  const r = spawnSync("ffmpeg", ["-y", "-v", "error", "-f", "lavfi", "-i", "color=c=red:s=64x64", "-frames:v", "1", png]);
  if (r.status !== 0) throw new Error(String(r.stderr));
  return png;
}
test("rejects mp3 with only cover art", async () => {
  const mp3 = path.join(dir, "a.mp3");
  const r = spawnSync("ffmpeg", ["-y", "-v", "error", "-f", "lavfi", "-i", "sine=d=1", "-i", makeCover(), "-map", "0", "-map", "1", "-c:a", "libmp3lame", "-c:v", "copy", "-disposition:v", "attached_pic", mp3]);
  if (r.status !== 0) throw new Error(String(r.stderr));
  await expect(probe(mp3)).rejects.toThrow(/no video stream/i);
});
test("video with attached cover probes and converts the real video (mp4)", async () => {
  const base = makeVideo("base.mkv", "320x240", 1.2);
  const withPic = path.join(dir, "pic.mp4");
  const r = spawnSync("ffmpeg", ["-y", "-v", "error", "-i", makeCover(), "-i", base, "-map", "0", "-map", "1:v", "-map", "1:a", "-c:v:0", "copy", "-c:v:1", "mpeg4", "-c:a", "aac", "-disposition:v:0", "attached_pic", withPic]);
  if (r.status !== 0) throw new Error(String(r.stderr));
  const p = await probe(withPic);
  expect([p.width, p.height]).toEqual([320, 240]);
  const dst = path.join(dir, "out.mpg");
  await convert(withPic, dst, "4:3", () => {});
  const q = await probe(dst);
  expect([q.width, q.height]).toEqual([480, 360]);
});
test("dash and odd names work for source and destination", async () => {
  const src = makeVideo("-dash é 'q\".mkv", "320x240", 1.2);
  expect((await probe(src)).width).toBe(320);
  const dst = path.join(dir, "-out é.mpg");
  await convert(src, dst, "4:3", () => {});
  expect(existsSync(dst)).toBe(true);
});
test("relative dash destination works", async () => {
  const src = makeVideo("a.mkv", "320x240", 1.2);
  const cwd = process.cwd();
  process.chdir(dir);
  try {
    await convert(src, "-rel.mpg", "4:3", () => {});
  } finally {
    process.chdir(cwd);
  }
  expect(existsSync(path.join(dir, "-rel.mpg"))).toBe(true);
});
test("probe rejects missing file", async () => {
  await expect(probe(path.join(dir, "nope.mkv"))).rejects.toThrow(/ffprobe/);
});
test("rejects source with no video stream", async () => {
  await expect(probe(makeAudioOnly("a.wav"))).rejects.toThrow(/no video stream/i);
});
test("convert and probe a 1 s file", async () => {
  const src = makeVideo("a.mkv", "320x240", 1.5);
  const dst = path.join(dir, "out.mpg");
  const ratios: number[] = [];
  await convert(src, dst, "4:3", (r) => ratios.push(r));
  expect(existsSync(dst)).toBe(true);
  expect(existsSync(dst + ".part")).toBe(false);
  const p = await probe(dst);
  expect(p.durationMs).toBeGreaterThanOrEqual(900);
  expect(p.durationMs).toBeLessThanOrEqual(2100);
  expect(p.width).toBe(480);
  expect(p.height).toBe(360);
});
test("progress callback reaches 1", async () => {
  const src = makeVideo("a.mkv", "320x240", 1.5);
  const ratios: number[] = [];
  await convert(src, path.join(dir, "out.mpg"), "4:3", (r) => ratios.push(r));
  expect(ratios.length).toBeGreaterThan(0);
  expect(Math.max(...ratios)).toBe(1);
  expect(ratios.every((r) => r >= 0 && r <= 1)).toBe(true);
});
test("4:3 source in 16:9 mode is pillarboxed to 640x360", async () => {
  const src = makeVideo("a.mkv", "320x240", 1.2);
  const dst = path.join(dir, "out.mpg");
  await convert(src, dst, "16:9", () => {});
  const p = await probe(dst);
  expect([p.width, p.height]).toEqual([640, 360]);
});
test("16:9 source in 4:3 mode is letterboxed to 480x360", async () => {
  const src = makeVideo("a.mkv", "640x360", 1.2);
  const dst = path.join(dir, "out.mpg");
  await convert(src, dst, "4:3", () => {});
  const p = await probe(dst);
  expect([p.width, p.height]).toEqual([480, 360]);
});
test("convert_leaves_part_on_failure", async () => {
  const dst = path.join(dir, "out.mpg");
  await expect(convert(path.join(dir, "missing.mkv"), dst, "16:9", () => {})).rejects.toThrow();
  expect(existsSync(dst)).toBe(false);
  expect(existsSync(dst + ".part")).toBe(false);
});
test("existing dst survives failed conversion", async () => {
  const dst = path.join(dir, "out.mpg");
  await writeFile(dst, "old");
  const bad = path.join(dir, "bad.mkv");
  await writeFile(bad, "not a video");
  await expect(convert(bad, dst, "16:9", () => {})).rejects.toThrow();
  expect(await readFile(dst, "utf8")).toBe("old");
  expect(existsSync(dst + ".part")).toBe(false);
});
test("existing dst is replaced on success", async () => {
  const dst = path.join(dir, "out.mpg");
  await writeFile(dst, "old");
  await convert(makeVideo("a.mkv", "320x240", 1.2), dst, "4:3", () => {});
  expect((await readFile(dst)).length).toBeGreaterThan(100);
});
test("abort removes part file", async () => {
  const src = makeVideo("long.mkv", "640x360", 20);
  const dst = path.join(dir, "out.mpg");
  const ctl = new AbortController();
  const p = convert(src, dst, "16:9", () => {}, ctl.signal);
  const assertion = expect(p).rejects.toThrow(/abort/i);
  await new Promise((r) => setTimeout(r, 400));
  ctl.abort();
  await assertion;
  expect(existsSync(dst)).toBe(false);
  expect(existsSync(dst + ".part")).toBe(false);
});
test("already aborted signal rejects without output", async () => {
  const ctl = new AbortController();
  ctl.abort();
  const dst = path.join(dir, "out.mpg");
  await expect(convert(makeVideo("a.mkv", "320x240", 1.2), dst, "4:3", () => {}, ctl.signal)).rejects.toThrow(/abort/i);
  expect(existsSync(dst + ".part")).toBe(false);
});
