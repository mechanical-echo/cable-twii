import { copyFile, mkdir } from "node:fs/promises";
import path from "node:path";
import { fileURLToPath } from "node:url";

const FILES = [
  ["data", "font_medium.ttf"],
  ["data", "font_bold.ttf"],
  ["", "OFL.txt"],
];

export async function copyFonts(studio) {
  const wii = path.join(studio, "..", "wii");
  const dir = path.join(studio, "src", "web", "public", "fonts");
  await mkdir(dir, { recursive: true });
  const copied = [];
  for (const [sub, name] of FILES) {
    const dst = path.join(dir, name);
    await copyFile(path.join(wii, sub, name), dst);
    copied.push(dst);
  }
  return copied;
}

if (process.argv[1] === fileURLToPath(import.meta.url)) {
  await copyFonts(path.resolve(path.dirname(fileURLToPath(import.meta.url)), ".."));
}
