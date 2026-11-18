import { stat } from "node:fs/promises";
import path from "node:path";
import type { Library } from "./types.js";

export const SHRINK_ABOVE_KBPS = 1300;

export function needsShrink(bitRateKbps: number | null): boolean {
  return bitRateKbps !== null && bitRateKbps > SHRINK_ABOVE_KBPS;
}

export async function shrinkCandidates(root: string, lib: Library, busy: Set<string>): Promise<string[]> {
  const entries = [...lib.ad, ...lib.show.flatMap((s) => s.episode)];
  const found: string[] = [];
  for (const { path: target, duration_ms } of entries) {
    if (busy.has(target) || duration_ms <= 0) continue;
    try {
      const { size } = await stat(path.join(root, target));
      if (needsShrink((size * 8) / duration_ms)) found.push(target);
    } catch {
      continue;
    }
  }
  return found;
}
