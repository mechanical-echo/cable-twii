import { mkdtemp, readFile, rm } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import sharp from "sharp";
import { afterEach, beforeEach, expect, test } from "vitest";
import { NotImageError, makeIcon } from "../src/server/icons.js";

let dir: string;

beforeEach(async () => {
  dir = await mkdtemp(path.join(os.tmpdir(), "studio-icons-"));
});

afterEach(async () => {
  await rm(dir, { recursive: true, force: true });
});

test("icon resized", async () => {
  const input = await sharp({ create: { width: 500, height: 300, channels: 3, background: { r: 255, g: 0, b: 0 } } }).png().toBuffer();
  const dst = path.join(dir, "Channel", "channel.png");
  await makeIcon(input, dst);
  const meta = await sharp(await readFile(dst)).metadata();
  expect(meta.format).toBe("png");
  expect(meta.width).toBe(128);
  expect(meta.height).toBe(128);
  expect(meta.hasAlpha).toBe(true);
  const { data } = await sharp(await readFile(dst)).raw().toBuffer({ resolveWithObject: true });
  expect(data[3]).toBe(0);
});

test("non-image input is refused", async () => {
  await expect(makeIcon(Buffer.from("hello, not an image"), path.join(dir, "x.png"))).rejects.toBeInstanceOf(NotImageError);
});
