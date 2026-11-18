import path from "node:path";
import type { probe } from "./ffmpeg.js";
import { FRAME_SIZE, type Aspect } from "./videoSettings.js";
import type { Library } from "./types.js";

export const SHRINK_ABOVE_KBPS = 1300;

export function needsShrink(bitRateKbps: number | null): boolean {
  return bitRateKbps !== null && bitRateKbps > SHRINK_ABOVE_KBPS;
}

export async function shrinkCandidates(
  root: string,
  lib: Library,
  busy: Set<string>,
  probeFn: typeof probe,
): Promise<{ target: string; aspect: Aspect }[]> {
  const targets = [...lib.ad.map((a) => a.path), ...lib.show.flatMap((s) => s.episode.map((e) => e.path))];
  const found: { target: string; aspect: Aspect }[] = [];
  for (const target of targets) {
    if (busy.has(target)) continue;
    try {
      const info = await probeFn(path.join(root, target));
      if (needsShrink(info.bitRateKbps)) found.push({ target, aspect: info.width >= FRAME_SIZE["16:9"].width ? "16:9" : "4:3" });
    } catch {
      continue;
    }
  }
  return found;
}
