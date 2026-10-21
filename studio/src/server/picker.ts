import { execFile } from "node:child_process";
import type { ExecFileException } from "node:child_process";

export type OsascriptRunner = (args: string[]) => Promise<string>;

export const defaultOsascript: OsascriptRunner = (args) =>
  new Promise((resolve, reject) => {
    execFile("osascript", args, (error: ExecFileException | null, stdout: string, stderr: string) => {
      if (error) {
        reject(Object.assign(error, { stderr }));
        return;
      }
      resolve(stdout);
    });
  });

function isCancel(error: unknown): boolean {
  if (typeof error !== "object" || error === null) return false;
  const { code, stderr } = error as { code?: unknown; stderr?: unknown };
  const text = typeof stderr === "string" ? stderr : "";
  return code === 1 && (text.includes("User canceled") || text.includes("-128"));
}

function stripTrailingSlash(p: string): string {
  return p.length > 1 && p.endsWith("/") ? p.slice(0, -1) : p;
}

/**
 * @throws {Error} when osascript fails for a reason other than the user cancelling
 */
export async function pickFolder(prompt: string, run: OsascriptRunner = defaultOsascript): Promise<string | null> {
  try {
    const out = await run([
      "-e",
      "on run argv",
      "-e",
      "POSIX path of (choose folder with prompt (item 1 of argv))",
      "-e",
      "end run",
      prompt,
    ]);
    const picked = out.trim();
    return picked === "" ? null : stripTrailingSlash(picked);
  } catch (error) {
    if (isCancel(error)) return null;
    throw error;
  }
}

/**
 * @throws {Error} when osascript fails for a reason other than the user cancelling
 */
export async function pickFiles(prompt: string, run: OsascriptRunner = defaultOsascript): Promise<string[]> {
  try {
    const out = await run([
      "-e",
      "on run argv",
      "-e",
      "set picked to choose file with prompt (item 1 of argv) with multiple selections allowed",
      "-e",
      "set paths to {}",
      "-e",
      "repeat with f in picked",
      "-e",
      "set end of paths to POSIX path of f",
      "-e",
      "end repeat",
      "-e",
      "set AppleScript's text item delimiters to linefeed",
      "-e",
      "return paths as text",
      "-e",
      "end run",
      prompt,
    ]);
    return out.split("\n").map((line) => line.trim()).filter((line) => line !== "");
  } catch (error) {
    if (isCancel(error)) return [];
    throw error;
  }
}
