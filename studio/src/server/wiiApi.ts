import { type FileHandle, open, realpath, stat } from "node:fs/promises";
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

export async function openWiiFile(root: string, rel: string): Promise<{ handle: FileHandle; size: number } | null> {
  let handle: FileHandle | undefined;
  try {
    const realRoot = await realpath(root);
    const real = await realpath(path.join(realRoot, ...rel.split("/")));
    // aliases such as Windows 8.3 short names and symlinks get past the string check, so the dot rule runs again on the real path
    if (!contained(realRoot, real) || wiiFilePath(path.relative(realRoot, real).split(path.sep).join("/")) === null) return null;
    // open() on a FIFO or device would block, so only regular files get that far
    if (!(await stat(real)).isFile()) return null;
    handle = await open(real, "r");
    const info = await handle.stat();
    if (!info.isFile()) {
      await handle.close();
      return null;
    }
    return { handle, size: info.size };
  } catch {
    await handle?.close().catch(() => undefined);
    return null;
  }
}

export function registerWiiApi(app: FastifyInstance, root: string, name: string): void {
  app.get("/wii/v1/info", { logLevel: "warn" }, async (_request, reply) => reply.type("application/toml").header("cache-control", "no-cache").send(infoToml(name, await libraryRevision(root))));

  app.get("/wii/v1/files/*", { logLevel: "warn" }, async (request, reply) => {
    const notFound = () => reply.code(404).send({ error: "Not found" });
    const rel = wiiFilePath((request.params as { "*"?: string })["*"] ?? "");
    if (rel === null) return notFound();
    const file = await openWiiFile(root, rel);
    if (file === null) return notFound();
    const { handle, size } = file;
    const extension = path.extname(rel).toLowerCase();
    const type = CONTENT_TYPES[extension] ?? "application/octet-stream";
    reply.header("accept-ranges", "bytes");
    if (extension === ".toml" && request.headers["accept-encoding"]?.includes("gzip")) {
      const body = await handle.readFile().finally(() => handle.close());
      return reply.type(type).header("content-encoding", "gzip").send(gzipSync(body));
    }
    const range = parseRange(request.headers.range, size);
    if (range === "unsatisfiable") {
      await handle.close();
      return reply.code(416).header("content-range", `bytes */${size}`).send({ error: "Range not satisfiable" });
    }
    reply.type(type);
    if (range === null) return reply.header("content-length", size).send(handle.createReadStream());
    return reply
      .code(206)
      .header("content-range", `bytes ${range.start}-${range.end}/${size}`)
      .header("content-length", range.end - range.start + 1)
      .send(handle.createReadStream(range));
  });
}
