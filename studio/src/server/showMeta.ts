import { readFile } from "node:fs/promises";
import path from "node:path";
import { parse, stringify } from "smol-toml";
import { atomicWriteContained } from "./contain.js";

export interface ShowMeta {
  name?: string;
  episodes: Record<string, string>;
}

const SHOW_FILE = "show.toml";

export async function readShowMeta(showDir: string): Promise<ShowMeta> {
  const file = path.join(showDir, SHOW_FILE);
  let text: string;
  try {
    text = await readFile(file, "utf8");
  } catch (err) {
    if ((err as NodeJS.ErrnoException).code === "ENOENT") return { episodes: {} };
    throw err;
  }
  let doc: Record<string, unknown>;
  try {
    doc = parse(text);
  } catch (err) {
    throw new Error(`${file} is damaged: ${(err as Error).message}`);
  }
  const meta: ShowMeta = { episodes: {} };
  if (typeof doc.name === "string" && doc.name !== "") meta.name = doc.name;
  const episodes = doc.episodes;
  if (episodes !== undefined) {
    if (typeof episodes !== "object" || episodes === null || Array.isArray(episodes)) {
      throw new Error(`${file} is damaged: invalid [episodes] table`);
    }
    for (const [key, value] of Object.entries(episodes)) {
      if (typeof value !== "string") throw new Error(`${file} is damaged: description for "${key}" is not a string`);
      meta.episodes[key] = value;
    }
  }
  return meta;
}

export async function writeShowMeta(root: string, showDir: string, meta: ShowMeta): Promise<void> {
  const doc: Record<string, unknown> = {};
  if (meta.name !== undefined && meta.name !== "") doc.name = meta.name;
  doc.episodes = meta.episodes;
  await atomicWriteContained(root, path.join(showDir, SHOW_FILE), stringify(doc));
}
