import { lstat, mkdir, open, readdir, rename, rm, rmdir } from "node:fs/promises";
import path from "node:path";
import type { Readable } from "node:stream";
import { pipeline } from "node:stream/promises";
import { ContainmentError, assertContained, contained, mkdirContained } from "./contain.js";
import { fatSafe } from "./names.js";
import type { SourceRecord } from "./types.js";

export const UPLOAD_LIMIT = 20 * 1024 ** 3;

const PART_SUFFIX = ".part";
const MAX_NAME_BYTES = 250;
const ORPHAN_AGE_MS = 24 * 60 * 60 * 1000;

export function uploadSource(name: string): string {
  return `upload:${name}`;
}

export function isUploadPath(uploads: string, file: string): boolean {
  return path.isAbsolute(file) && contained(uploads, file);
}

export function uploadName(name: string): string | null {
  if (name.startsWith(".") || /[/\\\0]/.test(name) || Buffer.byteLength(name) > MAX_NAME_BYTES) return null;
  const dot = name.lastIndexOf(".");
  const safe = dot > 0 ? `${fatSafe(name.slice(0, dot))}${fatSafe(name.slice(dot))}` : fatSafe(name);
  if (safe === "" || safe.startsWith(".") || Buffer.byteLength(safe) > MAX_NAME_BYTES) return null;
  return safe;
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
  if (path.dirname(dir) !== base) throw new ContainmentError("That path is outside the uploads folder.");
  if (uploadName(name) !== name) throw new ContainmentError(`${name} is not a usable upload name.`);
  const final = path.join(dir, name);
  const part = `${final}${PART_SUFFIX}`;
  await mkdir(base, { recursive: true });
  mkdirContained(base, dir);
  assertContained(base, part);
  const handle = await open(part, "wx");
  try {
    await pipeline(stream, handle.createWriteStream());
    await rename(part, final);
  } catch (err) {
    await rm(part, { force: true }).catch(() => undefined);
    throw err;
  }
  return final;
}

async function entriesOf(dir: string) {
  try {
    return await readdir(dir, { withFileTypes: true });
  } catch {
    return [];
  }
}

async function isFolder(dir: string): Promise<boolean> {
  try {
    return (await lstat(dir)).isDirectory();
  } catch {
    return false;
  }
}

async function modifiedBefore(file: string, cutoff: number): Promise<boolean> {
  try {
    return (await lstat(file)).mtimeMs < cutoff;
  } catch {
    return false;
  }
}

export async function sweepUploads(uploads: string, queued: ReadonlySet<string>, now: number): Promise<void> {
  if (!(await isFolder(uploads))) return;
  const cutoff = now - ORPHAN_AGE_MS;
  for (const batch of await entriesOf(uploads)) {
    if (!batch.isDirectory()) continue;
    const dir = path.join(uploads, batch.name);
    for (const entry of await entriesOf(dir)) {
      if (!entry.isFile()) continue;
      const file = path.join(dir, entry.name);
      if (entry.name.endsWith(PART_SUFFIX) || (!queued.has(file) && (await modifiedBefore(file, cutoff)))) await rm(file, { force: true }).catch(() => undefined);
    }
    await rmdir(dir).catch(() => undefined);
  }
}

export async function discardUploads(uploads: string, dir: string, names: string[]): Promise<number> {
  const base = path.resolve(uploads);
  const batch = path.resolve(dir);
  if (path.dirname(batch) !== base) throw new ContainmentError("That path is outside the uploads folder.");
  assertContained(base, batch);
  const files = names.map((name) => {
    if (uploadName(name) !== name) throw new ContainmentError(`${name} is not a usable upload name.`);
    return path.join(batch, name);
  });
  for (const file of files) {
    let stat;
    try {
      stat = await lstat(file);
    } catch {
      throw new ContainmentError(`${path.basename(file)} is not in this upload.`);
    }
    if (!stat.isFile()) throw new ContainmentError(`${path.basename(file)} is not in this upload.`);
  }
  for (const file of files) await rm(file, { force: true });
  await rmdir(batch).catch(() => undefined);
  return files.length;
}
