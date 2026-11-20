import { readFile } from "node:fs/promises";
import path from "node:path";
import { parse, stringify } from "smol-toml";
import { atomicWriteContained } from "./contain.js";

function offPath(root: string): string {
  return path.join(root, ".studio", "off.toml");
}

export async function readOff(root: string): Promise<Set<string>> {
  const file = offPath(root);
  let text: string;
  try {
    text = await readFile(file, "utf8");
  } catch (err) {
    if ((err as NodeJS.ErrnoException).code === "ENOENT") return new Set();
    throw err;
  }
  let doc: Record<string, unknown>;
  try {
    doc = parse(text);
  } catch (err) {
    throw new Error(`${file} is damaged: ${(err as Error).message}`);
  }
  if (doc.version !== 1 || (doc.paths !== undefined && (!Array.isArray(doc.paths) || doc.paths.some((p) => typeof p !== "string")))) {
    throw new Error(`${file} is damaged: expected version = 1 and a list of paths`);
  }
  return new Set((doc.paths ?? []) as string[]);
}

export async function writeOff(root: string, paths: Set<string>): Promise<void> {
  await atomicWriteContained(root, offPath(root), stringify({ version: 1, paths: [...paths].sort() }));
}
