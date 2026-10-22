import { execFile } from "node:child_process";
import { access, constants, copyFile, mkdtemp, rm, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { serializeConfig } from "./config.js";
import { serializeLibrary } from "./library.js";
import type { Config, Issue, LineupResult } from "./types.js";

export const STUDIO_DIR = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "../..");
export const ENGINE_DIR = path.resolve(STUDIO_DIR, "../engine");

const TIMEOUT_MS = 30_000;
const MAX_BUFFER = 64 * 1024 * 1024;
const MAKE_TIMEOUT_MS = 300_000;

export interface ValidateResult {
  ok: boolean;
  issues: Issue[];
}

export interface SchedFailure {
  ok: false;
  issues: Issue[];
}

export class SchedUsageError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "SchedUsageError";
  }
}

const isWindows = process.platform === "win32";

export function schedBinPath(): string {
  if (process.env.CTW_SCHED_BIN) return process.env.CTW_SCHED_BIN;
  const name = isWindows ? "cable-twii-sched.exe" : "cable-twii-sched";
  return path.join(ENGINE_DIR, "build", name);
}

async function isExecutable(file: string): Promise<boolean> {
  try {
    await access(file, constants.X_OK);
    return true;
  } catch {
    return false;
  }
}

export async function ensureSchedBin(bin: string = schedBinPath()): Promise<string> {
  if (await isExecutable(bin)) return bin;
  const winTmpVars = isWindows ? [`TMP=${process.env.TMP ?? ""}`, `TEMP=${process.env.TEMP ?? ""}`] : [];
  await new Promise<void>((resolve, reject) => {
    execFile(
      "make",
      [...winTmpVars, "-C", ENGINE_DIR, "cli"],
      { timeout: MAKE_TIMEOUT_MS, maxBuffer: MAX_BUFFER },
      (error, _stdout, stderr) => {
        if (error) reject(new Error(`could not build ${bin}: ${String(stderr).trim() || error.message}`));
        else resolve();
      },
    );
  });
  if (!(await isExecutable(bin))) throw new Error(`${bin} is missing after building the engine`);
  return bin;
}

type Outcome<T> = { ok: true; value: T } | SchedFailure;

function parseJson(text: string, bin: string): unknown {
  try {
    return JSON.parse(text);
  } catch {
    throw new Error(`${bin} printed invalid JSON: ${text.slice(0, 200)}`);
  }
}

function runSched<T>(bin: string, args: string[]): Promise<Outcome<T>> {
  return new Promise((resolve, reject) => {
    execFile(bin, args, { timeout: TIMEOUT_MS, maxBuffer: MAX_BUFFER, encoding: "utf8" }, (error, stdout, stderr) => {
      try {
        if (!error) {
          resolve({ ok: true, value: parseJson(stdout, bin) as T });
          return;
        }
        const code: unknown = (error as { code?: unknown }).code;
        if (code === 1) {
          const doc = parseJson(stdout, bin) as { issues?: Issue[] };
          resolve({ ok: false, issues: Array.isArray(doc.issues) ? doc.issues : [] });
          return;
        }
        if (code === 2) {
          reject(new SchedUsageError(stderr.trim() || "invalid arguments"));
          return;
        }
        if (error.killed) {
          reject(new Error(`${bin} timed out after ${TIMEOUT_MS} ms`));
          return;
        }
        reject(new Error(`${bin} failed: ${stderr.trim() || error.message}`));
      } catch (err) {
        reject(err);
      }
    });
  });
}

export async function validateRoot(root: string, checkFiles = false, bin: string = schedBinPath()): Promise<ValidateResult> {
  const args = ["validate", root];
  if (checkFiles) args.push("--check-files");
  const result = await runSched<ValidateResult>(bin, args);
  if (!result.ok) return { ok: false, issues: result.issues };
  return { ok: result.value.ok, issues: result.value.issues ?? [] };
}

export async function lineup(root: string, channel: number, date: string, bin: string = schedBinPath()): Promise<LineupResult | SchedFailure> {
  const result = await runSched<LineupResult>(bin, ["lineup", root, "--channel", String(channel), "--date", date]);
  return result.ok ? result.value : result;
}

export async function validateCandidate(root: string, cfg: Config, bin: string = schedBinPath()): Promise<ValidateResult> {
  const text = serializeConfig(cfg);
  const dir = await mkdtemp(path.join(os.tmpdir(), "ctw-candidate-"));
  try {
    await writeFile(path.join(dir, "config.toml"), text);
    try {
      await copyFile(path.join(root, "library.toml"), path.join(dir, "library.toml"));
    } catch (err) {
      if ((err as NodeJS.ErrnoException).code !== "ENOENT") throw err;
      await writeFile(path.join(dir, "library.toml"), serializeLibrary({ version: 1, generated: new Date(), ad: [], show: [] }));
    }
    return await validateRoot(dir, false, bin);
  } finally {
    await rm(dir, { recursive: true, force: true });
  }
}
