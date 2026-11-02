import { mkdir, readFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { parse } from "smol-toml";

export interface ServerSettings {
  library: string;
  password: string;
  imports: string;
  port: number;
  name: string;
}

const DEFAULT_FILE = path.join(os.homedir(), ".cable-twii-server.toml");
const DEFAULT_PORT = 4747;
const MIN_PASSWORD = 8;
const MAX_PORT = 65535;

function fail(file: string, key: string, problem: string): never {
  throw new Error(`${file}: ${key} ${problem}`);
}

function absolutePath(doc: Record<string, unknown>, key: string, file: string): string | undefined {
  const v = doc[key];
  if (v === undefined) return undefined;
  if (typeof v !== "string" || !path.isAbsolute(v)) fail(file, key, "must be an absolute path");
  return v;
}

export function parseServerSettings(text: string, file: string): ServerSettings {
  let doc: Record<string, unknown>;
  try {
    doc = parse(text);
  } catch (err) {
    throw new Error(`${file} is not valid TOML: ${(err as Error).message}`);
  }
  const library = absolutePath(doc, "library", file);
  if (library === undefined) fail(file, "library", "is required");
  const password = doc.password;
  if (typeof password !== "string" || password.length < MIN_PASSWORD) fail(file, "password", `is required and must be at least ${MIN_PASSWORD} characters`);
  const port = doc.port ?? DEFAULT_PORT;
  if (typeof port !== "number" || !Number.isInteger(port) || port < 1 || port > MAX_PORT) fail(file, "port", `must be a whole number from 1 to ${MAX_PORT}`);
  const name = doc.name ?? os.hostname();
  if (typeof name !== "string" || name.trim() === "") fail(file, "name", "must not be empty");
  return { library, password, imports: absolutePath(doc, "imports", file) ?? path.join(library, "imports"), port, name };
}

export function serverSettingsPath(env: NodeJS.ProcessEnv = process.env): string {
  const file = env.CTW_SERVER_SETTINGS;
  return file !== undefined && file !== "" ? file : DEFAULT_FILE;
}

export async function readServerSettings(file: string): Promise<ServerSettings> {
  let text: string;
  try {
    text = await readFile(file, "utf8");
  } catch (err) {
    if ((err as NodeJS.ErrnoException).code === "ENOENT") throw new Error(`${file} was not found. Create it with a library and a password, or point CTW_SERVER_SETTINGS at it.`);
    throw err;
  }
  return parseServerSettings(text, file);
}

export function uploadsDir(library: string): string {
  return path.join(library, ".uploads");
}

export async function initServerLibrary(s: ServerSettings): Promise<void> {
  const root = path.join(s.library, "cable-twii");
  for (const dir of [root, path.join(root, "ads"), path.join(root, ".studio"), s.imports, uploadsDir(s.library)]) {
    await mkdir(dir, { recursive: true });
  }
}
