import { createWriteStream } from "node:fs";
import { lstat, mkdir, rename, rm } from "node:fs/promises";
import path from "node:path";
import type { Readable } from "node:stream";
import { pipeline } from "node:stream/promises";
import { ContainmentError, assertContained, contained, mkdirContained } from "./contain.js";
import type { SourceRecord } from "./types.js";

export const UPLOAD_LIMIT = 20 * 1024 ** 3;

const PART_SUFFIX = ".part";

export function uploadSource(name: string): string {
  return `upload:${name}`;
}

export function isUploadPath(uploads: string, file: string): boolean {
  return path.isAbsolute(file) && contained(uploads, file);
}

export function uploadName(name: string): string | null {
  if (name === "" || name.startsWith(".") || /[/\\\0]/.test(name)) return null;
  return name;
}

export async function convertedUpload(root: string, records: SourceRecord[], name: string, size: number): Promise<boolean> {
  for (const rec of records) {
    if (rec.source !== uploadSource(name) || rec.size !== size) continue;
    try {
      if ((await lstat(path.join(root, ...rec.target.split("/")))).isFile()) return true;
    } catch {
      continue;
    }
  }
  return false;
}

export async function saveUpload(uploads: string, batch: string, name: string, stream: Readable): Promise<string> {
  const base = path.resolve(uploads);
  const dir = path.join(base, batch);
  if (uploadName(name) === null || path.dirname(dir) !== base) throw new ContainmentError("That path is outside the uploads folder.");
  const final = path.join(dir, name);
  const part = `${final}${PART_SUFFIX}`;
  await mkdir(base, { recursive: true });
  mkdirContained(base, dir);
  assertContained(base, part);
  try {
    await pipeline(stream, createWriteStream(part, { flags: "wx" }));
    await rename(part, final);
  } catch (err) {
    await rm(part, { force: true }).catch(() => undefined);
    throw err;
  }
  return final;
}
