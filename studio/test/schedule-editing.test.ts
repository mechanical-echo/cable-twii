import { describe, expect, it } from "vitest";
import type { Channel, Config, Item, Slot } from "../src/server/types.js";
import {
  type Schedule,
  addSlot,
  applySchedule,
  canHold,
  deleteItem,
  dropsBefore,
  insertItem,
  itemAt,
  itemId,
  moveItem,
  pointerY,
  removeSlot,
  renameSlot,
  replaceItem,
  resolveDrop,
  sameSchedule,
  scheduleOf,
  setAdsBetween,
  setCount,
  setPickType,
  setSeason,
  validAt,
  withSchedule
} from "../src/web/schedule/editing.js";

const a: Item = { type: "random", show: "A", count: 1 };
const b: Item = { type: "ads", count: 1 };
const c: Item = { type: "episode", show: "C", season: 1, episode: 5 };
const d: Item = { type: "ad", path: "ads/d.mpg" };
const fill: Item = { type: "fill", show: "F" };

function timed(...slots: Slot[]): Schedule {
  return { mode: "timed", slots };
}

const twoSlots = timed({ at: "07:00", items: [a, b, c] }, { at: "19:00", items: [d] });
const loop: Schedule = { mode: "looping", loop: [a, b] };

const cn: Channel = { number: 1, name: "Cartoon Network", category: "Kids", ads: true, mode: "timed", slot: [{ at: "19:00", items: [d] }, { at: "07:00", items: [a] }] };
const mtv: Channel = { number: 2, name: "MTV", category: "Music", folder: "mtv-00s", ads: false, mode: "looping", loop: [b] };

describe("scheduleOf / withSchedule / applySchedule", () => {
  it("sorts timed slots by at", () => {
    const s = scheduleOf(cn);
    expect(s.mode === "timed" && s.slots.map((x) => x.at)).toEqual(["07:00", "19:00"]);
  });

  it("reads the loop of a looping channel", () => {
    expect(scheduleOf(mtv)).toEqual({ mode: "looping", loop: [b] });
    expect(scheduleOf({ ...mtv, loop: undefined } as Channel)).toEqual({ mode: "looping", loop: [] });
  });

  it("replaces only the slot or loop of the edited channel", () => {
    const config: Config = { version: 1, seed: 7, channel: [cn, mtv] };
    const next = applySchedule(config, 2, { mode: "looping", loop: [c] });
    expect(next.seed).toBe(7);
    expect(next.channel[0]).toBe(cn);
    expect(next.channel[1]).toEqual({ ...mtv, loop: [c] });
    const timedNext = applySchedule(config, 1, twoSlots);
    expect(timedNext.channel[0]).toEqual({ ...cn, slot: twoSlots.mode === "timed" ? twoSlots.slots : [] });
    expect(timedNext.channel[1]).toBe(mtv);
  });

  it("drops a stray loop key on a timed channel", () => {
    expect(withSchedule({ ...cn, loop: [a] }, twoSlots)).not.toHaveProperty("loop");
  });

  it("compares schedules by value", () => {
    expect(sameSchedule(scheduleOf(cn), scheduleOf(cn))).toBe(true);
    expect(sameSchedule(twoSlots, loop)).toBe(false);
  });
});

describe("insertItem", () => {
  it("inserts at the index within the slot", () => {
    const next = insertItem(twoSlots, { slot: 0, index: 1 }, fill);
    expect(next.mode === "timed" && next.slots[0]!.items).toEqual([a, fill, b, c]);
    expect(next.mode === "timed" && next.slots[1]).toBe(twoSlots.mode === "timed" && twoSlots.slots[1]);
  });

  it("clamps the index to the end", () => {
    const next = insertItem(twoSlots, { slot: 1, index: 99 }, a);
    expect(next.mode === "timed" && next.slots[1]!.items).toEqual([d, a]);
  });

  it("does not put fill into a loop", () => {
    expect(canHold(loop, fill)).toBe(false);
    expect(insertItem(loop, { slot: 0, index: 0 }, fill)).toBe(loop);
    expect(insertItem(loop, { slot: 0, index: 0 }, c)).toEqual({ mode: "looping", loop: [c, a, b] });
  });

  it("ignores a missing slot", () => {
    expect(insertItem(twoSlots, { slot: 5, index: 0 }, a)).toBe(twoSlots);
  });
});

describe("moveItem", () => {
  it("moves down within a slot", () => {
    const { schedule, ref } = moveItem(twoSlots, { slot: 0, item: 0 }, { slot: 0, index: 3 });
    expect(schedule.mode === "timed" && schedule.slots[0]!.items).toEqual([b, c, a]);
    expect(ref).toEqual({ slot: 0, item: 2 });
  });

  it("moves up within a slot", () => {
    const { schedule, ref } = moveItem(twoSlots, { slot: 0, item: 2 }, { slot: 0, index: 0 });
    expect(schedule.mode === "timed" && schedule.slots[0]!.items).toEqual([c, a, b]);
    expect(ref).toEqual({ slot: 0, item: 0 });
  });

  it("leaves the schedule alone when dropped in place", () => {
    expect(moveItem(twoSlots, { slot: 0, item: 1 }, { slot: 0, index: 1 }).schedule).toBe(twoSlots);
    expect(moveItem(twoSlots, { slot: 0, item: 1 }, { slot: 0, index: 2 }).schedule).toBe(twoSlots);
  });

  it("moves across slots", () => {
    const { schedule, ref } = moveItem(twoSlots, { slot: 0, item: 1 }, { slot: 1, index: 0 });
    expect(schedule.mode === "timed" && schedule.slots.map((s) => s.items)).toEqual([[a, c], [b, d]]);
    expect(ref).toEqual({ slot: 1, item: 0 });
  });

  it("keeps item identity so ids stay stable", () => {
    const id = itemId(c);
    const { schedule } = moveItem(twoSlots, { slot: 0, item: 2 }, { slot: 1, index: 1 });
    expect(itemId(itemAt(schedule, { slot: 1, item: 1 })!)).toBe(id);
    expect(itemId(a)).not.toBe(id);
  });

  it("ignores a missing source", () => {
    expect(moveItem(twoSlots, { slot: 0, item: 9 }, { slot: 1, index: 0 }).schedule).toBe(twoSlots);
  });
});

describe("deleteItem / replaceItem", () => {
  it("deletes the referenced item", () => {
    const next = deleteItem(twoSlots, { slot: 0, item: 1 });
    expect(next.mode === "timed" && next.slots[0]!.items).toEqual([a, c]);
    expect(deleteItem(loop, { slot: 0, item: 0 })).toEqual({ mode: "looping", loop: [b] });
    expect(deleteItem(loop, { slot: 0, item: 5 })).toBe(loop);
  });

  it("replaces the referenced item", () => {
    const next = replaceItem(loop, { slot: 0, item: 1 }, c);
    expect(next).toEqual({ mode: "looping", loop: [a, c] });
    expect(replaceItem(loop, { slot: 0, item: 1 }, b)).toBe(loop);
  });
});

describe("resolveDrop", () => {
  const lists = [[a, b, c], [d]];

  it("drops before or after a block", () => {
    expect(resolveDrop(lists, { slot: 0, item: 1 }, true)).toEqual({ slot: 0, index: 1 });
    expect(resolveDrop(lists, { slot: 0, item: 1 }, false)).toEqual({ slot: 0, index: 2 });
  });

  it("appends when dropped on the slot", () => {
    expect(resolveDrop(lists, { slot: 1 }, false)).toEqual({ slot: 1, index: 1 });
    expect(resolveDrop(lists, { slot: 3 }, false)).toBeNull();
  });
});

describe("pointer drop position", () => {
  const tall = { top: 100, height: 1440 };

  it("reads the pointer Y from the activator plus the drag delta", () => {
    expect(pointerY({ clientY: 50 } as unknown as Event, 300)).toBe(350);
  });

  it("returns null for activators without a pointer position", () => {
    expect(pointerY(null, 10)).toBeNull();
    expect(pointerY({ key: "Enter" } as unknown as Event, 10)).toBeNull();
  });

  it("drops before when the pointer is in the top half", () => {
    expect(dropsBefore(110, { top: 100, height: 44 })).toBe(true);
    expect(dropsBefore(pointerY({ clientY: 120 } as unknown as Event, 600)!, tall)).toBe(true);
  });

  it("drops after when the pointer is in the bottom half", () => {
    expect(dropsBefore(140, { top: 100, height: 44 })).toBe(false);
    expect(dropsBefore(pointerY({ clientY: 120 } as unknown as Event, 800)!, tall)).toBe(false);
  });

  it("uses the pointer, not the dragged block, on a tall fill block", () => {
    expect(dropsBefore(101, tall)).toBe(true);
    expect(dropsBefore(819, tall)).toBe(true);
    expect(dropsBefore(821, tall)).toBe(false);
  });
});

describe("slots", () => {
  const slots: Slot[] = [{ at: "07:00", items: [a] }, { at: "19:00", items: [] }];

  it("accepts only HH:MM from 00:00 to 23:59", () => {
    expect(validAt("00:00")).toBe(true);
    expect(validAt("23:59")).toBe(true);
    for (const bad of ["24:00", "7:30", "07:60", "07:3", "0730", "", "07:30:00"]) expect(validAt(bad)).toBe(false);
  });

  it("adds a slot in sorted position", () => {
    const result = addSlot(slots, " 07:30 ");
    expect(result).toEqual({ ok: true, slots: [slots[0], { at: "07:30", items: [] }, slots[1]], index: 1 });
  });

  it("rejects bad or duplicate times", () => {
    expect(addSlot(slots, "25:00")).toEqual({ ok: false, error: "Use a time from 00:00 to 23:59 (HH:MM)." });
    expect(addSlot(slots, "19:00")).toEqual({ ok: false, error: "There is already a slot at 19:00." });
  });

  it("renames a slot and re-sorts, keeping its items", () => {
    const result = renameSlot(slots, 0, "20:00");
    expect(result).toEqual({ ok: true, slots: [slots[1], { at: "20:00", items: [a] }], index: 1 });
    expect(renameSlot(slots, 0, "07:00")).toEqual({ ok: true, slots, index: 0 });
    expect(renameSlot(slots, 0, "19:00")).toEqual({ ok: false, error: "There is already a slot at 19:00." });
    expect(renameSlot(slots, 0, "7pm").ok).toBe(false);
    expect(renameSlot(slots, 4, "08:00").ok).toBe(false);
  });

  it("deletes a slot but keeps at least one", () => {
    expect(removeSlot(slots, 0)).toEqual({ ok: true, slots: [slots[1]], index: -1 });
    expect(removeSlot([slots[0]!], 0)).toEqual({ ok: false, error: "A timed channel needs at least one slot." });
    expect(removeSlot(slots, 2).ok).toBe(false);
  });
});

describe("item patches", () => {
  it("clamps count to 1–50 on random, random-run and ads", () => {
    expect(setCount(a, 3)).toEqual({ ...a, count: 3 });
    expect(setCount(a, 0)).toEqual({ ...a, count: 1 });
    expect(setCount(b, 99)).toEqual({ ...b, count: 50 });
    expect(setCount(a, Number.NaN)).toBe(a);
    expect(setCount(c, 3)).toBe(c);
  });

  it("sets or clears the season", () => {
    expect(setSeason(a, 5)).toEqual({ ...a, season: 5 });
    expect(setSeason({ ...a, season: 5 }, 0)).toEqual(a);
    expect(setSeason(fill, 2)).toEqual({ ...fill, season: 2 });
    expect(setSeason(c, 2)).toBe(c);
  });

  it("switches between random and random-run", () => {
    const from: Item = { type: "random", show: "A", season: 2, count: 4, from: "x" };
    expect(setPickType(from, "random-run")).toEqual({ ...from, type: "random-run" });
    expect(setPickType(from, "random")).toBe(from);
    expect(setPickType(b, "random")).toBe(b);
  });

  it("clamps ads_between to 0–5 and omits 0", () => {
    expect(setAdsBetween(fill, 2)).toEqual({ ...fill, ads_between: 2 });
    expect(setAdsBetween(fill, 9)).toEqual({ ...fill, ads_between: 5 });
    expect(setAdsBetween({ ...fill, ads_between: 2 }, 0)).toEqual(fill);
    expect(setAdsBetween(a, 2)).toBe(a);
  });
});
