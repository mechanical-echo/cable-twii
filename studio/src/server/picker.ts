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
      "set AppleScript's text item delimiters to (ASCII character 31)",
      "-e",
      "return paths as text",
      "-e",
      "end run",
      prompt,
    ]);
    return out.replace(/\n$/, "").split("\u001f").filter((line) => line !== "");
  } catch (error) {
    if (isCancel(error)) return [];
    throw error;
  }
}

export type PowershellRunner = (script: string) => Promise<string>;

export const defaultPowershell: PowershellRunner = (script) =>
  new Promise((resolve, reject) => {
    execFile(
      "powershell.exe",
      ["-NoProfile", "-NonInteractive", "-Command", script],
      { maxBuffer: 1024 * 1024 },
      (error: ExecFileException | null, stdout: string, stderr: string) => {
        if (error) {
          reject(Object.assign(error, { stderr }));
          return;
        }
        resolve(stdout);
      },
    );
  });

function psSingleQuote(s: string): string {
  return `'${s.replace(/['‘’‚‛]/g, "$&$&")}'`;
}

const UTF8_OUTPUT = "[Console]::OutputEncoding = [System.Text.Encoding]::UTF8";

const TOPMOST_OWNER = [
  "Add-Type -AssemblyName System.Windows.Forms",
  "Add-Type -AssemblyName System.Drawing",
  "$owner = New-Object System.Windows.Forms.Form",
  "$owner.TopMost = $true",
  "$owner.StartPosition = 'CenterScreen'",
  "$owner.Size = New-Object System.Drawing.Size(0,0)",
  "$owner.ShowInTaskbar = $false",
  "$owner.Show()",
  "$owner.Activate()",
].join("; ");

export async function pickFolderWindows(prompt: string, run: PowershellRunner = defaultPowershell): Promise<string | null> {
  const script = [
    UTF8_OUTPUT,
    TOPMOST_OWNER,
    "$f = New-Object System.Windows.Forms.FolderBrowserDialog",
    `$f.Description = ${psSingleQuote(prompt)}`,
    "$f.ShowNewFolderButton = $true",
    "$result = $f.ShowDialog($owner)",
    "$owner.Close()",
    "if ($result -eq [System.Windows.Forms.DialogResult]::OK) { [Console]::Out.Write($f.SelectedPath) }",
  ].join("; ");
  const out = await run(script);
  const picked = out.trim();
  return picked === "" ? null : picked;
}

export interface PickerDeps {
  platform?: NodeJS.Platform;
  osascript?: OsascriptRunner;
  powershell?: PowershellRunner;
}

export async function pickFolderNative(prompt: string, deps: PickerDeps = {}): Promise<string | null> {
  if ((deps.platform ?? process.platform) === "win32") return pickFolderWindows(prompt, deps.powershell);
  return pickFolder(prompt, deps.osascript);
}
