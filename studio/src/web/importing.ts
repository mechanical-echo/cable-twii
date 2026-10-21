import type { ScanResult } from "../server/drive.js";
import { baseName } from "./format.js";

export function isUsable(scan: Pick<ScanResult, "tooShort" | "error">): boolean {
  return !scan.tooShort && scan.error === undefined;
}

export function describeRejected(imported: number, rejected: { source: string; reason: string }[]): string | null {
  if (rejected.length === 0) return null;
  return `Imported ${imported}. Skipped: ${rejected.map((x) => `${baseName(x.source)} (${x.reason})`).join("; ")}`;
}
