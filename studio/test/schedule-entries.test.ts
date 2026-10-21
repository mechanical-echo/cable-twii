import { describe, expect, it } from "vitest";
import { adsFor, entryKey, entryLabel, itemFromEntry, rulesFor, seasonsOf } from "../src/web/schedule/entries.js";
import type { WebLibrary } from "../src/web/tree.js";

const lib: WebLibrary = {
  version: 1,
  generated: "",
  ad: [
    { path: "ads/a.mpg", channel: "", duration_ms: 30000 },
    { path: "Cartoon Network/ads/cn.mpg", channel: "Cartoon Network", duration_ms: 15000 },
    { path: "mtv-00s/ads/m.mpg", channel: "mtv-00s", duration_ms: 15000 }
  ],
  show: [
    {
      channel: "Cartoon Network",
      folder: "Gumball",
      name: "Gumball",
      icon: "",
      episode: [
        { season: 2, episode: 1, title: "", description: "", path: "x", duration_ms: 1 },
        { season: 1, episode: 1, title: "", description: "", path: "y", duration_ms: 1 },
        { season: 1, episode: 2, title: "", description: "", path: "z", duration_ms: 1 }
      ]
    }
  ]
};

describe("itemFromEntry", () => {
  it("builds a specific episode", () => {
    expect(itemFromEntry({ kind: "episode", channel: "Cartoon Network", show: "Ben 10", season: 1, episode: 5 }, "Cartoon Network")).toEqual({
      type: "episode",
      show: "Ben 10",
      season: 1,
      episode: 5
    });
  });

  it("adds from for shows of other channels", () => {
    expect(itemFromEntry({ kind: "episode", channel: "mtv-00s", show: "Videos", season: 0, episode: 3 }, "Cartoon Network")).toEqual({
      type: "episode",
      show: "Videos",
      season: 0,
      episode: 3,
      from: "mtv-00s"
    });
    expect(itemFromEntry({ kind: "rule", rule: "fill", channel: "mtv-00s", show: "Videos", season: 0 }, "Cartoon Network")).toEqual({
      type: "fill",
      show: "Videos",
      from: "mtv-00s"
    });
  });

  it("builds rule items with count 1 and the season of the chip", () => {
    expect(itemFromEntry({ kind: "rule", rule: "random", channel: "CN", show: "Gumball", season: 0 }, "CN")).toEqual({ type: "random", show: "Gumball", count: 1 });
    expect(itemFromEntry({ kind: "rule", rule: "random-run", channel: "CN", show: "Gumball", season: 5 }, "CN")).toEqual({
      type: "random-run",
      show: "Gumball",
      season: 5,
      count: 1
    });
    expect(itemFromEntry({ kind: "rule", rule: "fill", channel: "CN", show: "AT", season: 5 }, "CN")).toEqual({ type: "fill", show: "AT", season: 5 });
  });

  it("builds ads items", () => {
    expect(itemFromEntry({ kind: "ads" }, "CN")).toEqual({ type: "ads", count: 1 });
    expect(itemFromEntry({ kind: "ad", path: "ads/a.mpg" }, "CN")).toEqual({ type: "ad", path: "ads/a.mpg" });
  });
});

describe("library helpers", () => {
  it("offers fill only on timed channels", () => {
    expect(rulesFor("timed")).toEqual(["random", "random-run", "fill"]);
    expect(rulesFor("looping")).toEqual(["random", "random-run"]);
  });

  it("lists own ads then shared ads", () => {
    expect(adsFor(lib, "Cartoon Network").map((a) => a.path)).toEqual(["Cartoon Network/ads/cn.mpg", "ads/a.mpg"]);
  });

  it("lists the seasons of a show", () => {
    expect(seasonsOf(lib, "Cartoon Network", "Gumball")).toEqual([1, 2]);
    expect(seasonsOf(lib, "mtv-00s", "Gumball")).toEqual([]);
  });

  it("gives unique keys and readable labels", () => {
    const a = entryKey({ kind: "rule", rule: "random", channel: "CN", show: "G", season: 0 });
    const b = entryKey({ kind: "rule", rule: "random", channel: "MTV", show: "G", season: 0 });
    expect(a).not.toBe(b);
    expect(entryLabel({ kind: "episode", channel: "CN", show: "Ben 10", season: 1, episode: 5 })).toBe("Ben 10 S01E05");
    expect(entryLabel({ kind: "rule", rule: "random-run", channel: "CN", show: "Gumball", season: 2 })).toBe("Random sequential run · Gumball S2");
    expect(entryLabel({ kind: "ads" })).toBe("Random ad");
    expect(entryLabel({ kind: "ad", path: "ads/a.mpg" })).toBe("a.mpg");
  });
});
