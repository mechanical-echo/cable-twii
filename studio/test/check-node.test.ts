import { spawnSync } from "node:child_process";
import { fileURLToPath } from "node:url";
import { expect, test } from "vitest";

const script = fileURLToPath(new URL("../scripts/check-node.mjs", import.meta.url));

test("check-node passes on the current Node", () => {
  const result = spawnSync(process.execPath, [script], { encoding: "utf8" });
  expect(result.status).toBe(0);
  expect(result.stderr).toBe("");
});
