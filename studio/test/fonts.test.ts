import { mkdir, mkdtemp, readFile, readdir, rm, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { afterEach, expect, test } from "vitest";
import { copyFonts } from "../scripts/copy-fonts.mjs";

let repo: string;

afterEach(async () => {
  await rm(repo, { recursive: true, force: true });
});

test("copies the Wii fonts and licence", async () => {
  repo = await mkdtemp(path.join(os.tmpdir(), "studio-fonts-"));
  const studio = path.join(repo, "studio");
  await mkdir(path.join(repo, "wii", "data"), { recursive: true });
  await mkdir(studio);
  await writeFile(path.join(repo, "wii", "data", "font_medium.ttf"), "medium");
  await writeFile(path.join(repo, "wii", "data", "font_bold.ttf"), "bold");
  await writeFile(path.join(repo, "wii", "OFL.txt"), "licence");

  const copied = await copyFonts(studio);

  const dir = path.join(studio, "src", "web", "public", "fonts");
  expect((await readdir(dir)).sort()).toEqual(["OFL.txt", "font_bold.ttf", "font_medium.ttf"]);
  expect(copied.map((file) => path.basename(file)).sort()).toEqual(["OFL.txt", "font_bold.ttf", "font_medium.ttf"]);
  expect(await readFile(path.join(dir, "font_medium.ttf"), "utf8")).toBe("medium");
  expect(await readFile(path.join(dir, "font_bold.ttf"), "utf8")).toBe("bold");
  expect(await readFile(path.join(dir, "OFL.txt"), "utf8")).toBe("licence");
});
