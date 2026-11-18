import { lstat, readdir, realpath } from "node:fs/promises";
import path from "node:path";
import { ContainmentError, contained } from "./contain.js";
import { isVideoFile } from "./names.js";
import { isUploadPath } from "./uploads.js";
import { wiiFilePath } from "./wiiApi.js";

export interface ImportEntry {
  name: string;
  kind: "folder" | "video";
}

const OUTSIDE = "That path is outside the imports folder.";

function byName(a: ImportEntry, b: ImportEntry): number {
  return a.name.localeCompare(b.name, undefined, { numeric: true });
}

export async function resolveImportsDir(imports: string, rel: string): Promise<string> {
  const realRoot = await realpath(imports);
  if (rel === "") return realRoot;
  if (wiiFilePath(rel) === null) throw new ContainmentError(OUTSIDE);
  const real = await realpath(path.join(realRoot, ...rel.split("/")));
  // aliases such as Windows 8.3 short names and symlinks get past the string check, so the dot rule runs again on the real path
  if (!contained(realRoot, real) || wiiFilePath(path.relative(realRoot, real).split(path.sep).join("/")) === null) throw new ContainmentError(OUTSIDE);
  return real;
}

export async function listImports(imports: string, rel: string): Promise<{ dir: string; entries: ImportEntry[] }> {
  const folders: ImportEntry[] = [];
  const videos: ImportEntry[] = [];
  for (const entry of await readdir(await resolveImportsDir(imports, rel), { withFileTypes: true })) {
    if (entry.name.startsWith(".") || entry.isSymbolicLink()) continue;
    if (entry.isDirectory()) folders.push({ name: entry.name, kind: "folder" });
    else if (entry.isFile() && isVideoFile(entry.name)) videos.push({ name: entry.name, kind: "video" });
  }
  return { dir: rel, entries: [...folders.sort(byName), ...videos.sort(byName)] };
}

function allowedIn(imports: string, uploads: string, file: string): boolean {
  return path.isAbsolute(file) && (contained(imports, file) || isUploadPath(uploads, file));
}

async function resolved(dir: string): Promise<string> {
  try {
    return await realpath(dir);
  } catch {
    return dir;
  }
}

export async function isAllowedSource(imports: string, uploads: string, source: string): Promise<boolean> {
  if (!allowedIn(imports, uploads, source)) return false;
  try {
    await lstat(source);
  } catch {
    return true;
  }
  try {
    const [realImports, realUploads] = await Promise.all([resolved(imports), resolved(uploads)]);
    return allowedIn(realImports, realUploads, await realpath(source));
  } catch {
    return false;
  }
}
