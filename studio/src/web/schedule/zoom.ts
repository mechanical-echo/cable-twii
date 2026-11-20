import type { Mode } from "../../server/types.js";

export function defaultZoom(mode: Mode): number {
  return mode === "timed" ? 30 : 120;
}
