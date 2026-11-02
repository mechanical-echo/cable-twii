import { createReadStream } from "node:fs";
import { readFile, realpath, stat } from "node:fs/promises";
import path from "node:path";
import { gzipSync } from "node:zlib";
import type { FastifyInstance } from "fastify";
import { stringify } from "smol-toml";
import { configMtime } from "./config.js";
import { contained } from "./contain.js";

export interface ByteRange {
  start: number;
  end: number;
}

const WII_API_VERSION = 1;
const LIBRARY_FILE = "library.toml";
const RANGE = /^bytes=(\d*)-(\d*)$/;
const CONTENT_TYPES: Record<string, string> = {
  ".mpg": "video/mpeg",
  ".png": "image/png",
  ".toml": "application/toml",
};

export function parseRange(header: string | undefined, size: number): ByteRange | "unsatisfiable" | null {
  const match = header === undefined ? null : RANGE.exec(header);
  if (!match) return null;
  const [, first = "", last = ""] = match;
  if (first === "") {
    if (last === "") return null;
    const length = Number(last);
    if (length === 0 || size === 0) return "unsatisfiable";
    return { start: Math.max(0, size - length), end: size - 1 };
  }
  const start = Number(first);
  if (start >= size) return "unsatisfiable";
  if (last === "") return { start, end: size - 1 };
  const end = Number(last);
  if (end < start) return null;
  return { start, end: Math.min(end, size - 1) };
}

export function wiiFilePath(rel: string): string | null {
  if (rel === "" || rel.includes("\\") || rel.includes("\0")) return null;
  for (const segment of rel.split("/")) {
    if (segment === "" || segment.startsWith(".")) return null;
  }
  return rel;
}

async function libraryMtime(root: string): Promise<number> {
  try {
    return Math.trunc((await stat(path.join(root, LIBRARY_FILE))).mtimeMs);
  } catch (err) {
    if ((err as NodeJS.ErrnoException).code === "ENOENT") return 0;
    throw err;
  }
}

export async function libraryRevision(root: string): Promise<string> {
  return `${await configMtime(root)}-${await libraryMtime(root)}`;
}

export function infoToml(name: string, revision: string): string {
  return stringify({ version: WII_API_VERSION, name, revision });
}

async function regularFile(root: string, rel: string): Promise<{ real: string; size: number } | null> {
  try {
    const realRoot = await realpath(root);
    const real = await realpath(path.join(realRoot, ...rel.split("/")));
    if (!contained(realRoot, real)) return null;
    const info = await stat(real);
    return info.isFile() ? { real, size: info.size } : null;
  } catch {
    return null;
  }
}

export function registerWiiApi(app: FastifyInstance, root: string, name: string): void {
  app.get("/wii/v1/info", async (_request, reply) => reply.type("application/toml").header("cache-control", "no-cache").send(infoToml(name, await libraryRevision(root))));

  app.get("/wii/v1/files/*", async (request, reply) => {
    const notFound = () => reply.code(404).send({ error: "Not found" });
    const rel = wiiFilePath((request.params as { "*"?: string })["*"] ?? "");
    if (rel === null) return notFound();
    const file = await regularFile(root, rel);
    if (file === null) return notFound();
    const extension = path.extname(rel).toLowerCase();
    const type = CONTENT_TYPES[extension] ?? "application/octet-stream";
    reply.header("accept-ranges", "bytes");
    if (extension === ".toml" && request.headers["accept-encoding"]?.includes("gzip")) {
      return reply.type(type).header("content-encoding", "gzip").send(gzipSync(await readFile(file.real)));
    }
    const range = parseRange(request.headers.range, file.size);
    if (range === "unsatisfiable") return reply.code(416).header("content-range", `bytes */${file.size}`).send({ error: "Range not satisfiable" });
    reply.type(type);
    if (range === null) return reply.header("content-length", file.size).send(createReadStream(file.real));
    return reply
      .code(206)
      .header("content-range", `bytes ${range.start}-${range.end}/${file.size}`)
      .header("content-length", range.end - range.start + 1)
      .send(createReadStream(file.real, range));
  });
}
