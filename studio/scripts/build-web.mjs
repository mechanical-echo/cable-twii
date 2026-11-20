import { spawnSync } from "node:child_process";
import { existsSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";
import { copyFonts } from "./copy-fonts.mjs";

const studio = path.resolve(path.dirname(fileURLToPath(import.meta.url)), "..");

if (!existsSync(path.join(studio, "src", "web", "index.html"))) {
  console.log("No web UI entry yet, starting the server only.");
  process.exit(0);
}

await copyFonts(studio);

const result = spawnSync("npx", ["vite", "build"], { cwd: studio, stdio: "inherit", shell: process.platform === "win32" });
process.exit(result.status ?? 1);
