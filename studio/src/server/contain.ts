import { lstatSync, mkdirSync, realpathSync, statSync, unlinkSync } from "node:fs";
import path from "node:path";
import { atomicWriteFile } from "./fsutil.js";

export class ContainmentError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "ContainmentError";
  }
}

function within(base: string, full: string): boolean {
  const rel = path.relative(base, full);
  return rel === "" || (rel !== ".." && !rel.startsWith(`..${path.sep}`) && !path.isAbsolute(rel));
}

function exists(full: string): boolean {
  try {
    lstatSync(full);
    return true;
  } catch {
    return false;
  }
}

function nearestExisting(full: string): string {
  let current = full;
  while (!exists(current)) {
    const parent = path.dirname(current);
    if (parent === current) return current;
    current = parent;
  }
  return current;
}

export function assertContained(root: string, dest: string): void {
  const realRoot = realpathSync(root);
  const rootDev = statSync(realRoot).dev;
  const target = path.resolve(dest);
  if (!within(path.resolve(root), target)) throw new ContainmentError("That path is outside the cable-twii folder.");
  let real: string;
  try {
    real = realpathSync(nearestExisting(target));
  } catch {
    throw new ContainmentError("That path leads outside the cable-twii folder.");
  }
  if (!within(realRoot, real)) throw new ContainmentError("That path leads outside the cable-twii folder.");
  if (statSync(real).dev !== rootDev) throw new ContainmentError("That path is on a different drive.");
}

export function mkdirContained(root: string, dir: string): void {
  const base = path.resolve(root);
  const target = path.resolve(dir);
  assertContained(root, target);
  const parts = path.relative(base, target).split(path.sep).filter((p) => p !== "");
  let current = base;
  for (const part of parts) {
    current = path.join(current, part);
    assertContained(root, current);
    if (exists(current)) {
      if (!statSync(current).isDirectory()) throw new ContainmentError(`${path.basename(current)} is not a folder.`);
      continue;
    }
    try {
      mkdirSync(current);
    } catch (err) {
      if ((err as NodeJS.ErrnoException).code !== "EEXIST") throw err;
    }
    assertContained(root, current);
    if (!statSync(current).isDirectory()) throw new ContainmentError(`${path.basename(current)} is not a folder.`);
  }
}

function regularOrMissing(file: string): boolean {
  try {
    return lstatSync(file).isFile();
  } catch (err) {
    if ((err as NodeJS.ErrnoException).code === "ENOENT") return true;
    throw err;
  }
}

export function prepareScratchFile(root: string, file: string): void {
  assertContained(root, path.dirname(file));
  if (!regularOrMissing(file)) throw new ContainmentError(`${path.basename(file)} is not a regular file.`);
  if (exists(file)) unlinkSync(file);
}

export function checkRename(root: string, from: string, to: string): void {
  assertContained(root, path.dirname(to));
  assertContained(root, path.dirname(from));
  if (!exists(from) || !lstatSync(from).isFile()) throw new ContainmentError(`${path.basename(from)} is not a regular file.`);
  if (!regularOrMissing(to)) throw new ContainmentError(`${path.basename(to)} is not a regular file.`);
}

export async function atomicWriteContained(root: string, file: string, data: string | Uint8Array): Promise<void> {
  mkdirContained(root, path.dirname(file));
  assertContained(root, file);
  await atomicWriteFile(file, data);
}
