import type { Issue, Mode } from "../../server/types.js";
import type { TimelineBlock } from "./layout.js";

const MINUTE_MS = 60000;
const HOUR_MS = 3600000;

export function blockKey(slot: number, item: number): string {
  return `${slot}:${item}`;
}

export function errorBlocks(issues: Issue[], channel: number, mode: Mode): Set<string> {
  const keys = new Set<string>();
  for (const issue of issues) {
    if (issue.level !== "error" || issue.channel !== channel || issue.item < 0) continue;
    const slot = mode === "looping" ? (issue.slot === -1 ? 0 : -1) : issue.slot;
    if (slot >= 0) keys.add(blockKey(slot, issue.item));
  }
  return keys;
}

export function issueWhere(issue: Issue, mode: Mode | null, slotAt: (index: number) => string | undefined): string {
  if (issue.channel < 0) return "";
  const parts = [`CH${issue.channel}`];
  if (mode === "timed" && issue.slot >= 0) parts.push(`slot ${slotAt(issue.slot) ?? issue.slot + 1}`);
  if (issue.item >= 0) parts.push(`item ${issue.item + 1}`);
  return parts.join(" · ");
}

export function overlapLabel(ms: number): string {
  return `⚠ ${Math.max(1, Math.round(ms / MINUTE_MS))} min overlap`;
}

function pad(value: number): string {
  return String(value).padStart(2, "0");
}

export function formatClock(ms: number): string {
  const total = Math.max(0, Math.round(ms / 1000));
  const h = Math.floor(total / 3600);
  const m = Math.floor((total % 3600) / 60);
  const s = total % 60;
  return h > 0 ? `${h}:${pad(m)}:${pad(s)}` : `${m}:${pad(s)}`;
}

export function describeLength(ms: number, exact: boolean): string {
  if (!Number.isFinite(ms)) return "fills the rest of the slot";
  if (exact) return formatClock(ms);
  if (ms <= 0) return "length unknown";
  if (ms < MINUTE_MS) return `about ${Math.round(ms / 1000)} s`;
  return `about ${Math.round(ms / MINUTE_MS)} min`;
}

export function atToMs(at: string): number {
  const [h, m] = at.split(":");
  return (Number(h) * 60 + Number(m)) * MINUTE_MS;
}

export function hourLabels(firstAt: string): string[] {
  const start = atToMs(firstAt);
  return Array.from({ length: 24 }, (_, k) => {
    const minutes = Math.round(((start + k * HOUR_MS) % (24 * HOUR_MS)) / MINUTE_MS);
    return `${pad(Math.floor(minutes / 60))}:${pad(minutes % 60)}`;
  });
}

export function hintMs(blocks: TimelineBlock[], slot: number, index: number, slotStartMs: number): number {
  const items = blocks.filter((b) => b.slotIndex === slot && b.itemIndex !== null);
  const at = items.find((b) => b.itemIndex === index);
  if (at) return at.startMs;
  const last = items[items.length - 1];
  if (!last) return slotStartMs;
  return last.startMs + (Number.isFinite(last.durationMs) ? last.durationMs : 0);
}
