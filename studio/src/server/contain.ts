import { lstat, mkdir, realpath, stat } from "node:fs/promises";
import path from "node:path";
import { atomicWriteFile } from "./sources.js";

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

async function nearestExisting(full: string): Promise<string> {
  let current = full;
  for (;;) {
    if (await lstat(current).then(() => true, () => false)) return current;
    const parent = path.dirname(current);
    if (parent === current) return current;
    current = parent;
  }
}

export async function assertContained(root: string, dest: string): Promise<void> {
  const realRoot = await realpath(root);
  const rootDev = (await stat(realRoot)).dev;
  const target = path.resolve(dest);
  if (!within(path.resolve(root), target)) throw new ContainmentError("That path is outside the cable-twii folder.");
  const existing = await nearestExisting(target);
  let real: string;
  try {
    real = await realpath(existing);
  } catch {
    throw new ContainmentError("That path leads outside the cable-twii folder.");
  }
  if (!within(realRoot, real)) throw new ContainmentError("That path leads outside the cable-twii folder.");
  if ((await stat(real)).dev !== rootDev) throw new ContainmentError("That path is on a different drive.");
}

export async function mkdirContained(root: string, dir: string): Promise<void> {
  const base = path.resolve(root);
  const target = path.resolve(dir);
  await assertContained(root, target);
  const parts = path.relative(base, target).split(path.sep).filter((p) => p !== "");
  let current = base;
  for (const part of parts) {
    current = path.join(current, part);
    await assertContained(root, current);
    if (!(await lstat(current).then(() => true, () => false))) {
      await mkdir(current).catch((err: NodeJS.ErrnoException) => {
        if (err.code !== "EEXIST") throw err;
      });
      await assertContained(root, current);
    }
  }
}

export async function atomicWriteContained(root: string, file: string, data: string | Uint8Array): Promise<void> {
  await mkdirContained(root, path.dirname(file));
  await assertContained(root, file);
  await atomicWriteFile(file, data);
}
