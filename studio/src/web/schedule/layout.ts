import type { Channel, Item, LibShow, Library } from "../../server/types.js";
import { effectiveFolder } from "../channels.js";

export type LayoutLibrary = Pick<Library, "show" | "ad">;

export interface Estimate {
  ms: number;
  exact: boolean;
}

export type BlockKind = Item["type"] | "adfill" | "upnext";

export interface TimelineBlock {
  slotIndex: number;
  itemIndex: number | null;
  kind: BlockKind;
  startMs: number;
  durationMs: number;
  exact: boolean;
  overlapMs: number;
  label: string;
}

const HOUR_MS = 3600000;
const DAY_MS = 24 * HOUR_MS;
const MINUTE_MS = 60000;

function mean(values: number[]): number {
  return values.reduce((sum, v) => sum + v, 0) / values.length;
}

function countOf(item: { count?: number }): number {
  return item.count ?? 1;
}

function findShow(item: { show: string; from?: string }, channel: Channel, lib: LayoutLibrary): LibShow | undefined {
  const from = item.from ?? effectiveFolder(channel);
  return lib.show.find((s) => s.channel === from && s.folder === item.show);
}

function poolDurations(item: { show: string; season?: number; from?: string }, channel: Channel, lib: LayoutLibrary): number[] {
  const show = findShow(item, channel, lib);
  if (!show) return [];
  const season = item.season ?? 0;
  return show.episode.filter((e) => season === 0 || e.season === season).map((e) => e.duration_ms);
}

function adDurations(channel: Channel, lib: LayoutLibrary): number[] {
  const own = lib.ad.filter((a) => a.channel === effectiveFolder(channel));
  const pool = own.length > 0 ? own : lib.ad.filter((a) => a.channel === "");
  return pool.map((a) => a.duration_ms);
}

export function estimateItemMs(item: Item, channel: Channel, lib: LayoutLibrary): Estimate {
  switch (item.type) {
    case "episode": {
      const episode = findShow(item, channel, lib)?.episode.find((e) => e.season === item.season && e.episode === item.episode);
      return episode ? { ms: episode.duration_ms, exact: true } : { ms: 0, exact: false };
    }
    case "ad": {
      const ad = lib.ad.find((a) => a.path === item.path);
      return ad ? { ms: ad.duration_ms, exact: true } : { ms: 0, exact: false };
    }
    case "random":
    case "random-run": {
      const pool = poolDurations(item, channel, lib);
      return { ms: pool.length > 0 ? countOf(item) * mean(pool) : 0, exact: false };
    }
    case "ads": {
      if (!channel.ads) return { ms: 0, exact: false };
      const pool = adDurations(channel, lib);
      return { ms: pool.length > 0 ? countOf(item) * mean(pool) : 0, exact: false };
    }
    case "fill":
      return { ms: Infinity, exact: false };
  }
}

function pad(value: number): string {
  return String(value).padStart(2, "0");
}

function itemLabel(item: Item, channel: Channel, lib: LayoutLibrary): string {
  switch (item.type) {
    case "episode": {
      const name = findShow(item, channel, lib)?.name ?? item.show;
      return `${name} S${pad(item.season)}E${pad(item.episode)}`;
    }
    case "random":
      return `Random · ${item.show}`;
    case "random-run":
      return `Run · ${item.show}`;
    case "fill":
      return `Fill · ${item.show}`;
    case "ads":
      return `Ads ×${countOf(item)}`;
    case "ad":
      return `Ad · ${item.path.split("/").pop() ?? item.path}`;
  }
}

function parseAt(at: string): number {
  const [h, m] = at.split(":");
  return (Number(h) * 60 + Number(m)) * MINUTE_MS;
}

export function layoutTimed(channel: Channel, lib: LayoutLibrary): TimelineBlock[] {
  const slots = [...(channel.slot ?? [])].sort((a, b) => parseAt(a.at) - parseAt(b.at));
  if (slots.length === 0) return [];
  const origin = parseAt(slots[0]!.at);
  const blocks: TimelineBlock[] = [];
  slots.forEach((slot, slotIndex) => {
    const slotStart = parseAt(slot.at) - origin;
    const next = slots[slotIndex + 1];
    const windowMs = next ? parseAt(next.at) - parseAt(slot.at) : parseAt(slots[0]!.at) + DAY_MS - parseAt(slot.at);
    let cursor = 0;
    slot.items.forEach((item, itemIndex) => {
      const remaining = Math.max(0, windowMs - cursor);
      const estimate = estimateItemMs(item, channel, lib);
      const durationMs = item.type === "fill" ? remaining : estimate.ms;
      const start = item.type === "fill" ? Math.min(cursor, windowMs) : cursor;
      const overlapMs = Math.min(durationMs, Math.max(0, start + durationMs - windowMs));
      blocks.push({
        slotIndex,
        itemIndex,
        kind: item.type,
        startMs: slotStart + start,
        durationMs,
        exact: estimate.exact,
        overlapMs,
        label: itemLabel(item, channel, lib)
      });
      cursor = item.type === "fill" ? windowMs : cursor + durationMs;
    });
    if (cursor < windowMs) {
      blocks.push({
        slotIndex,
        itemIndex: null,
        kind: channel.ads ? "adfill" : "upnext",
        startMs: slotStart + cursor,
        durationMs: windowMs - cursor,
        exact: false,
        overlapMs: 0,
        label: channel.ads ? "Ads" : "Up next"
      });
    }
  });
  return blocks;
}

export function layoutLoop(channel: Channel, lib: LayoutLibrary): { blocks: TimelineBlock[]; loopMs: number; exact: boolean } {
  const blocks: TimelineBlock[] = [];
  let cursor = 0;
  let exact = true;
  (channel.loop ?? []).forEach((item, itemIndex) => {
    const estimate = item.type === "fill" ? { ms: 0, exact: false } : estimateItemMs(item, channel, lib);
    blocks.push({
      slotIndex: 0,
      itemIndex,
      kind: item.type,
      startMs: cursor,
      durationMs: estimate.ms,
      exact: estimate.exact,
      overlapMs: 0,
      label: itemLabel(item, channel, lib)
    });
    cursor += estimate.ms;
    exact = exact && estimate.exact;
  });
  return { blocks, loopMs: cursor, exact };
}

export function msToPx(ms: number, pxPerHour: number): number {
  return (ms / HOUR_MS) * pxPerHour;
}
