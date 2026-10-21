import { randomBytes } from "node:crypto";
import { mkdir, open, readFile, rename, rm } from "node:fs/promises";
import path from "node:path";
import { parse, stringify } from "smol-toml";
import type { SourceRecord } from "./types.js";

const STUDIO_DIR = ".studio";
const SOURCES_FILE = "sources.toml";

export async function atomicWriteFile(file: string, text: string): Promise<void> {
  await mkdir(path.dirname(file), { recursive: true });
  const tmp = path.join(path.dirname(file), `.${path.basename(file)}.${randomBytes(6).toString("hex")}.tmp`);
  try {
    const handle = await open(tmp, "w");
    try {
      await handle.writeFile(text, "utf8");
      await handle.sync();
    } finally {
      await handle.close();
    }
    await rename(tmp, file);
  } catch (err) {
    await rm(tmp, { force: true }).catch(() => undefined);
    throw err;
  }
}

function sourcesPath(root: string): string {
  return path.join(root, STUDIO_DIR, SOURCES_FILE);
}

function damaged(file: string, reason: string): Error {
  return new Error(`${file} is damaged: ${reason}`);
}

function toRecord(file: string, raw: unknown): SourceRecord {
  const r = raw as Record<string, unknown>;
  if (
    typeof r !== "object" || r === null ||
    typeof r.target !== "string" ||
    typeof r.source !== "string" ||
    !Number.isInteger(r.size) ||
    !Number.isInteger(r.mtimeMs) ||
    !Number.isInteger(r.durationMs)
  ) {
    throw damaged(file, "invalid [[source]] entry");
  }
  return { target: r.target, source: r.source, size: r.size as number, mtimeMs: r.mtimeMs as number, durationMs: r.durationMs as number };
}

export async function readSources(root: string): Promise<SourceRecord[]> {
  const file = sourcesPath(root);
  let text: string;
  try {
    text = await readFile(file, "utf8");
  } catch (err) {
    if ((err as NodeJS.ErrnoException).code === "ENOENT") return [];
    throw err;
  }
  let doc: Record<string, unknown>;
  try {
    doc = parse(text);
  } catch (err) {
    throw damaged(file, (err as Error).message);
  }
  if (doc.version !== 1) throw damaged(file, "unsupported version");
  const list = doc.source === undefined ? [] : doc.source;
  if (!Array.isArray(list)) throw damaged(file, "invalid source list");
  return list.map((entry) => toRecord(file, entry));
}

export async function writeSources(root: string, recs: SourceRecord[]): Promise<void> {
  const sorted = [...recs].sort((a, b) => (a.target < b.target ? -1 : a.target > b.target ? 1 : 0));
  const text = stringify({
    version: 1,
    source: sorted.map((r) => ({
      target: r.target,
      source: r.source,
      size: r.size,
      mtimeMs: Math.trunc(r.mtimeMs),
      durationMs: Math.trunc(r.durationMs),
    })),
  });
  await atomicWriteFile(sourcesPath(root), text);
}

export function needsConversion(
  rec: SourceRecord | undefined,
  stat: { size: number; mtimeMs: number },
  targetExists: boolean,
): boolean {
  if (!rec || !targetExists) return true;
  return rec.size !== stat.size || rec.mtimeMs !== Math.trunc(stat.mtimeMs);
}

let upsertChain: Promise<unknown> = Promise.resolve();

export function upsertSource(root: string, rec: SourceRecord): Promise<void> {
  const run = async (): Promise<void> => {
    const recs = await readSources(root);
    await writeSources(root, [...recs.filter((r) => r.target !== rec.target), rec]);
  };
  const result = upsertChain.then(run, run);
  upsertChain = result.catch(() => undefined);
  return result;
}
