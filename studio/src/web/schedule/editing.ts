import type { Channel, Config, Item, Slot } from "../../server/types.js";

export type Schedule = { mode: "timed"; slots: Slot[] } | { mode: "looping"; loop: Item[] };

export interface ItemRef {
  slot: number;
  item: number;
}

export interface DropTarget {
  slot: number;
  index: number;
}

export type SlotResult = { ok: true; slots: Slot[]; index: number } | { ok: false; error: string };

export const UNSAVED_PROMPT = "You have unsaved schedule changes. Leave without saving?";
export const MAX_COUNT = 50;
export const MAX_ADS_BETWEEN = 5;

const AT_PATTERN = /^([01]\d|2[0-3]):[0-5]\d$/;

export function sortSlots(slots: Slot[]): Slot[] {
  return [...slots].sort((a, b) => (a.at < b.at ? -1 : a.at > b.at ? 1 : 0));
}

export function scheduleOf(channel: Channel): Schedule {
  if (channel.mode === "looping") return { mode: "looping", loop: channel.loop ?? [] };
  return { mode: "timed", slots: sortSlots(channel.slot ?? []) };
}

export function withSchedule(channel: Channel, schedule: Schedule): Channel {
  const { slot: _slot, loop: _loop, ...rest } = channel;
  return schedule.mode === "timed" ? { ...rest, slot: schedule.slots } : { ...rest, loop: schedule.loop };
}

export function applySchedule(config: Config, number: number, schedule: Schedule): Config {
  return { ...config, channel: config.channel.map((c) => (c.number === number ? withSchedule(c, schedule) : c)) };
}

export function sameSchedule(a: Schedule, b: Schedule): boolean {
  return a === b || JSON.stringify(a) === JSON.stringify(b);
}

export function itemLists(schedule: Schedule): Item[][] {
  return schedule.mode === "timed" ? schedule.slots.map((s) => s.items) : [schedule.loop];
}

function withLists(schedule: Schedule, lists: Item[][]): Schedule {
  if (schedule.mode === "looping") return { mode: "looping", loop: lists[0] ?? [] };
  return { mode: "timed", slots: schedule.slots.map((slot, i) => (lists[i] === slot.items ? slot : { ...slot, items: lists[i] ?? [] })) };
}

export function itemAt(schedule: Schedule, ref: ItemRef): Item | undefined {
  return itemLists(schedule)[ref.slot]?.[ref.item];
}

export function canHold(schedule: Schedule, item: Item): boolean {
  return !(schedule.mode === "looping" && item.type === "fill");
}

export function insertItem(schedule: Schedule, target: DropTarget, item: Item): Schedule {
  const lists = itemLists(schedule);
  const list = lists[target.slot];
  if (list === undefined || !canHold(schedule, item)) return schedule;
  const index = Math.max(0, Math.min(target.index, list.length));
  const next = [...lists];
  next[target.slot] = [...list.slice(0, index), item, ...list.slice(index)];
  return withLists(schedule, next);
}

export function moveItem(schedule: Schedule, from: ItemRef, target: DropTarget): { schedule: Schedule; ref: ItemRef } {
  const lists = itemLists(schedule);
  const source = lists[from.slot];
  const item = source?.[from.item];
  const dest = lists[target.slot];
  if (source === undefined || item === undefined || dest === undefined) return { schedule, ref: from };
  let index = Math.max(0, Math.min(target.index, dest.length));
  if (target.slot === from.slot) {
    if (index > from.item) index -= 1;
    if (index === from.item) return { schedule, ref: from };
  }
  const next = [...lists];
  next[from.slot] = source.filter((_, i) => i !== from.item);
  const destList = next[target.slot]!;
  next[target.slot] = [...destList.slice(0, index), item, ...destList.slice(index)];
  return { schedule: withLists(schedule, next), ref: { slot: target.slot, item: index } };
}

export function deleteItem(schedule: Schedule, ref: ItemRef): Schedule {
  const lists = itemLists(schedule);
  const list = lists[ref.slot];
  if (list === undefined || ref.item < 0 || ref.item >= list.length) return schedule;
  const next = [...lists];
  next[ref.slot] = list.filter((_, i) => i !== ref.item);
  return withLists(schedule, next);
}

export function replaceItem(schedule: Schedule, ref: ItemRef, item: Item): Schedule {
  const lists = itemLists(schedule);
  const list = lists[ref.slot];
  if (list === undefined || ref.item < 0 || ref.item >= list.length || list[ref.item] === item) return schedule;
  const next = [...lists];
  next[ref.slot] = list.map((current, i) => (i === ref.item ? item : current));
  return withLists(schedule, next);
}

export function resolveDrop(lists: Item[][], over: ItemRef | { slot: number }, before: boolean): DropTarget | null {
  const list = lists[over.slot];
  if (list === undefined) return null;
  if (!("item" in over)) return { slot: over.slot, index: list.length };
  return { slot: over.slot, index: over.item + (before ? 0 : 1) };
}

export function validAt(at: string): boolean {
  return AT_PATTERN.test(at);
}

function checkAt(slots: Slot[], at: string, ignore: number): string | null {
  if (!validAt(at)) return "Use a time from 00:00 to 23:59 (HH:MM).";
  if (slots.some((s, i) => i !== ignore && s.at === at)) return `There is already a slot at ${at}.`;
  return null;
}

function placed(slots: Slot[], slot: Slot): SlotResult {
  const sorted = sortSlots(slots);
  return { ok: true, slots: sorted, index: sorted.indexOf(slot) };
}

export function addSlot(slots: Slot[], raw: string): SlotResult {
  const at = raw.trim();
  const error = checkAt(slots, at, -1);
  if (error !== null) return { ok: false, error };
  const slot: Slot = { at, items: [] };
  return placed([...slots, slot], slot);
}

export function renameSlot(slots: Slot[], index: number, raw: string): SlotResult {
  const current = slots[index];
  if (current === undefined) return { ok: false, error: "That slot no longer exists." };
  const at = raw.trim();
  if (at === current.at) return { ok: true, slots, index };
  const error = checkAt(slots, at, index);
  if (error !== null) return { ok: false, error };
  const slot: Slot = { ...current, at };
  return placed(slots.map((s, i) => (i === index ? slot : s)), slot);
}

export function removeSlot(slots: Slot[], index: number): SlotResult {
  if (slots[index] === undefined) return { ok: false, error: "That slot no longer exists." };
  if (slots.length === 1) return { ok: false, error: "A timed channel needs at least one slot." };
  return { ok: true, slots: slots.filter((_, i) => i !== index), index: -1 };
}

function clamp(value: number, min: number, max: number): number | null {
  if (!Number.isFinite(value)) return null;
  return Math.min(max, Math.max(min, Math.round(value)));
}

export function setCount(item: Item, value: number): Item {
  if (item.type !== "random" && item.type !== "random-run" && item.type !== "ads") return item;
  const count = clamp(value, 1, MAX_COUNT);
  return count === null || count === item.count ? item : { ...item, count };
}

export function setSeason(item: Item, value: number): Item {
  if (item.type !== "random" && item.type !== "random-run" && item.type !== "fill") return item;
  const season = clamp(value, 0, 65535);
  if (season === null || season === (item.season ?? 0)) return item;
  const { season: _season, ...rest } = item;
  return season === 0 ? rest : { ...rest, season };
}

export function setPickType(item: Item, type: "random" | "random-run"): Item {
  if ((item.type !== "random" && item.type !== "random-run") || item.type === type) return item;
  return { ...item, type };
}

export function setAdsBetween(item: Item, value: number): Item {
  if (item.type !== "fill") return item;
  const ads = clamp(value, 0, MAX_ADS_BETWEEN);
  if (ads === null || ads === (item.ads_between ?? 0)) return item;
  const { ads_between: _ads, ...rest } = item;
  return ads === 0 ? rest : { ...rest, ads_between: ads };
}

const ids = new WeakMap<Item, string>();
let nextId = 0;

export function itemId(item: Item): string {
  let id = ids.get(item);
  if (id === undefined) {
    nextId += 1;
    id = `item-${nextId}`;
    ids.set(item, id);
  }
  return id;
}
