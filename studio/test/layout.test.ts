import { describe, expect, it } from "vitest";
import { estimateItemMs, layoutLoop, layoutTimed, msToPx } from "../src/web/schedule/layout.js";
import type { Channel, Item, LibEpisode, LibShow } from "../src/server/types.js";
import type { WebLibrary } from "../src/web/tree.js";

function episodes(season: number, count: number, duration: number): LibEpisode[] {
  return Array.from({ length: count }, (_, i) => ({
    season,
    episode: i + 1,
    title: `Episode ${i + 1}`,
    description: "",
    path: `s${season}e${i + 1}.mpg`,
    duration_ms: duration
  }));
}

function show(channel: string, folder: string, name: string, episode: LibEpisode[]): LibShow {
  return { channel, folder, name, icon: "", episode };
}

const lib: WebLibrary = {
  version: 1,
  generated: "2026-10-20T14:00:00",
  ad: [
    { path: "ads/ad-a.mpg", channel: "", duration_ms: 30000 },
    { path: "ads/ad-b.mpg", channel: "", duration_ms: 15000 },
    { path: "ads/ad-c.mpg", channel: "", duration_ms: 60000 }
  ],
  show: [
    show("Cartoon Network", "Gumball", "The Amazing World of Gumball", [...episodes(1, 4, 600000), ...episodes(2, 2, 600000)]),
    show("Cartoon Network", "Adventure Time", "Adventure Time", episodes(5, 5, 660000)),
    show("Cartoon Network", "Ben 10", "Ben 10", [{ ...episodes(1, 1, 1320000)[0]!, episode: 5 }]),
    show("mtv-00s", "Music Videos", "Music Videos", episodes(0, 6, 200000))
  ]
};

const cn: Channel = { number: 1, name: "Cartoon Network", category: "Kids", ads: true, mode: "timed" };
const mtv: Channel = { number: 2, name: "MTV", category: "Music", folder: "mtv-00s", ads: false, mode: "looping" };

function timed(slot: NonNullable<Channel["slot"]>, overrides: Partial<Channel> = {}): Channel {
  return { ...cn, slot, ...overrides };
}

describe("estimateItemMs", () => {
  it("fixed episode exact", () => {
    expect(estimateItemMs({ type: "episode", show: "Ben 10", season: 1, episode: 5 }, cn, lib)).toEqual({ ms: 1320000, exact: true });
  });

  it("random estimate is pool mean x count", () => {
    expect(estimateItemMs({ type: "random", show: "Adventure Time", season: 5, count: 5 }, cn, lib)).toEqual({ ms: 3300000, exact: false });
  });

  it("random run uses the pool mean as well", () => {
    expect(estimateItemMs({ type: "random-run", show: "Gumball", count: 2 }, cn, lib)).toEqual({ ms: 1200000, exact: false });
  });

  it("count defaults to one", () => {
    const item = { type: "random", show: "Gumball" } as unknown as Item;
    expect(estimateItemMs(item, cn, lib)).toEqual({ ms: 600000, exact: false });
  });

  it("season zero means all seasons", () => {
    expect(estimateItemMs({ type: "random", show: "Gumball", season: 0, count: 1 }, cn, lib).ms).toBe(600000);
  });

  it("from overrides the channel folder", () => {
    expect(estimateItemMs({ type: "random", show: "Music Videos", count: 3, from: "mtv-00s" }, cn, lib)).toEqual({ ms: 600000, exact: false });
  });

  it("unknown show is zero and inexact", () => {
    expect(estimateItemMs({ type: "random", show: "Nope", count: 3 }, cn, lib)).toEqual({ ms: 0, exact: false });
    expect(estimateItemMs({ type: "episode", show: "Ben 10", season: 1, episode: 9 }, cn, lib)).toEqual({ ms: 0, exact: false });
  });

  it("empty season pool is zero and inexact", () => {
    expect(estimateItemMs({ type: "random", show: "Gumball", season: 9, count: 1 }, cn, lib)).toEqual({ ms: 0, exact: false });
  });

  it("ads estimate uses the shared pool mean", () => {
    expect(estimateItemMs({ type: "ads", count: 2 }, cn, lib)).toEqual({ ms: 70000, exact: false });
  });

  it("ads prefer the channel's own pool", () => {
    const own = { ...lib, ad: [...lib.ad, { path: "own.mpg", channel: "Cartoon Network", duration_ms: 10000 }] };
    expect(estimateItemMs({ type: "ads", count: 3 }, cn, own).ms).toBe(30000);
  });

  it("ads estimate zero when ads off", () => {
    expect(estimateItemMs({ type: "ads", count: 2 }, { ...cn, ads: false }, lib).ms).toBe(0);
  });

  it("specific ad is exact and airs even when ads are off", () => {
    expect(estimateItemMs({ type: "ad", path: "ads/ad-c.mpg" }, { ...cn, ads: false }, lib)).toEqual({ ms: 60000, exact: true });
    expect(estimateItemMs({ type: "ad", path: "missing.mpg" }, cn, lib)).toEqual({ ms: 0, exact: false });
  });

  it("fill is unbounded", () => {
    expect(estimateItemMs({ type: "fill", show: "Gumball" }, cn, lib)).toEqual({ ms: Infinity, exact: false });
  });
});

describe("layoutTimed", () => {
  const ben: Item = { type: "episode", show: "Ben 10", season: 1, episode: 5 };
  const gumball: Item = { type: "episode", show: "Gumball", season: 1, episode: 1 };

  it("overlap flagged", () => {
    const blocks = layoutTimed(timed([{ at: "07:00", items: [ben] }, { at: "07:15", items: [] }]), lib);
    const first = blocks.find((b) => b.slotIndex === 0 && b.itemIndex === 0)!;
    expect(first.startMs).toBe(0);
    expect(first.durationMs).toBe(1320000);
    expect(first.overlapMs).toBe(420000);
    expect(first.exact).toBe(true);
    expect(first.label).toBe("Ben 10 S01E05");
  });

  it("blocks after the window end are listed with full overlap", () => {
    const slot = [{ at: "07:00", items: [ben, gumball] }, { at: "07:15", items: [] }];
    const blocks = layoutTimed(timed(slot), lib).filter((b) => b.slotIndex === 0);
    expect(blocks).toHaveLength(2);
    expect(blocks[1]!.startMs).toBe(1320000);
    expect(blocks[1]!.overlapMs).toBe(600000);
  });

  it("leftover becomes adfill", () => {
    const blocks = layoutTimed(timed([{ at: "07:00", items: [{ type: "episode", show: "Gumball", season: 1, episode: 1 }] }, { at: "08:00", items: [] }]), lib);
    const rest = blocks.filter((b) => b.slotIndex === 0);
    expect(rest).toHaveLength(2);
    expect(rest[1]).toMatchObject({ kind: "adfill", itemIndex: null, startMs: 600000, durationMs: 3000000, exact: false, overlapMs: 0 });
  });

  it("leftover becomes upnext when ads off", () => {
    const channel = timed([{ at: "07:00", items: [gumball] }, { at: "08:00", items: [] }], { ads: false });
    const rest = layoutTimed(channel, lib).filter((b) => b.slotIndex === 0);
    expect(rest[1]).toMatchObject({ kind: "upnext", durationMs: 3000000 });
  });

  it("fill takes rest of slot", () => {
    const channel = timed([{ at: "07:00", items: [gumball, { type: "fill", show: "Gumball" }] }, { at: "08:00", items: [] }]);
    const rest = layoutTimed(channel, lib).filter((b) => b.slotIndex === 0);
    expect(rest.map((b) => b.kind)).toEqual(["episode", "fill"]);
    expect(rest[1]).toMatchObject({ startMs: 600000, durationMs: 3000000, exact: false, overlapMs: 0 });
  });

  it("fill alone spans the whole window", () => {
    const channel = timed([{ at: "07:00", items: [{ type: "fill", show: "Gumball" }] }, { at: "08:00", items: [] }]);
    const blocks = layoutTimed(channel, lib).filter((b) => b.slotIndex === 0);
    expect(blocks).toHaveLength(1);
    expect(blocks[0]!.durationMs).toBe(3600000);
  });

  it("items after a fill start at the window end and overlap fully", () => {
    const channel = timed([{ at: "07:00", items: [{ type: "fill", show: "Gumball" }, gumball] }, { at: "08:00", items: [] }]);
    const blocks = layoutTimed(channel, lib).filter((b) => b.slotIndex === 0);
    expect(blocks[0]!.durationMs).toBe(3600000);
    expect(blocks[1]).toMatchObject({ startMs: 3600000, overlapMs: 600000 });
  });

  it("fill with nothing left is zero length", () => {
    const channel = timed([{ at: "07:00", items: [ben, { type: "fill", show: "Gumball" }] }, { at: "07:15", items: [] }]);
    const blocks = layoutTimed(channel, lib).filter((b) => b.slotIndex === 0);
    expect(blocks[1]).toMatchObject({ kind: "fill", durationMs: 0 });
  });

  it("last slot wraps to first slot next day", () => {
    const channel = timed([{ at: "07:00", items: [gumball] }, { at: "19:00", items: [gumball] }]);
    const blocks = layoutTimed(channel, lib);
    const second = blocks.filter((b) => b.slotIndex === 1);
    expect(second[0]!.startMs).toBe(12 * 3600000);
    expect(second[1]).toMatchObject({ kind: "adfill", durationMs: 12 * 3600000 - 600000 });
  });

  it("single slot gets a 24 hour window", () => {
    const blocks = layoutTimed(timed([{ at: "07:00", items: [gumball] }]), lib);
    expect(blocks[1]).toMatchObject({ kind: "adfill", durationMs: 24 * 3600000 - 600000 });
  });

  it("slots are sorted by time and slotIndex follows the sorted order", () => {
    const channel = timed([{ at: "19:00", items: [gumball] }, { at: "07:00", items: [ben] }]);
    const blocks = layoutTimed(channel, lib);
    expect(blocks[0]).toMatchObject({ slotIndex: 0, label: "Ben 10 S01E05", startMs: 0 });
    expect(blocks.find((b) => b.slotIndex === 1)!.startMs).toBe(12 * 3600000);
  });

  it("no slots gives no blocks", () => {
    expect(layoutTimed(timed([]), lib)).toEqual([]);
    expect(layoutTimed(cn, lib)).toEqual([]);
  });

  it("labels describe random items", () => {
    const channel = timed([{ at: "07:00", items: [{ type: "random", show: "Adventure Time", season: 5, count: 2 }, { type: "ads", count: 2 }] }]);
    const blocks = layoutTimed(channel, lib);
    expect(blocks[0]!.label).toBe("Random · Adventure Time");
    expect(blocks[1]!.label).toBe("Ads ×2");
  });
});

describe("layoutLoop", () => {
  it("loop length", () => {
    const channel: Channel = { ...mtv, loop: [{ type: "random", show: "Music Videos", count: 10 }] };
    const result = layoutLoop(channel, lib);
    expect(result.loopMs).toBe(2000000);
    expect(result.exact).toBe(false);
    expect(result.blocks).toHaveLength(1);
    expect(result.blocks[0]).toMatchObject({ slotIndex: 0, itemIndex: 0, startMs: 0, durationMs: 2000000, overlapMs: 0 });
  });

  it("stacks blocks and stays exact when every item is exact", () => {
    const channel: Channel = { ...cn, mode: "looping", loop: [{ type: "episode", show: "Ben 10", season: 1, episode: 5 }, { type: "ad", path: "ads/ad-a.mpg" }] };
    const result = layoutLoop(channel, lib);
    expect(result.blocks.map((b) => b.startMs)).toEqual([0, 1320000]);
    expect(result.loopMs).toBe(1350000);
    expect(result.exact).toBe(true);
  });

  it("empty loop", () => {
    expect(layoutLoop(mtv, lib)).toEqual({ blocks: [], loopMs: 0, exact: true });
  });
});

describe("msToPx", () => {
  it("scales by pixels per hour", () => {
    expect(msToPx(1800000, 120)).toBe(60);
  });
});
