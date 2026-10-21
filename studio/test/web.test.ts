import { describe, expect, it } from "vitest";
import { baseName, formatBytes, formatDuration } from "../src/web/format.js";
import { buildTree, descriptionKey, seasonLabel, type WebLibrary } from "../src/web/tree.js";

const ep = (season: number, episode: number, title: string) => ({ season, episode, title, description: "", path: `C/S/${title}.mpg`, duration_ms: 30000 });

describe("formatDuration", () => {
  it("formats m:ss", () => {
    expect(formatDuration(30000)).toBe("0:30");
    expect(formatDuration(665000)).toBe("11:05");
    expect(formatDuration(3600000)).toBe("60:00");
  });

  it("rounds to the nearest second and clamps negatives", () => {
    expect(formatDuration(29600)).toBe("0:30");
    expect(formatDuration(-5)).toBe("0:00");
  });
});

describe("formatBytes", () => {
  it("uses GB from one gigabyte and MB below", () => {
    expect(formatBytes(1.5 * 1024 ** 3)).toBe("1.5 GB");
    expect(formatBytes(300 * 1024 ** 2)).toBe("300 MB");
  });
});

describe("baseName", () => {
  it("returns the last path part", () => {
    expect(baseName("a/b/c.mpg")).toBe("c.mpg");
    expect(baseName("c.mpg")).toBe("c.mpg");
  });
});

describe("buildTree", () => {
  const library: WebLibrary = {
    version: 1,
    generated: "2026-10-20T10:00:00Z",
    ad: [
      { path: "ads/a.mpg", channel: "", duration_ms: 30000 },
      { path: "MTV/ads/b.mpg", channel: "MTV", duration_ms: 15000 },
    ],
    show: [
      { channel: "Cartoon Network", folder: "Gumball", name: "Gumball", icon: "", episode: [ep(2, 1, "c"), ep(1, 2, "b"), ep(1, 1, "a")] },
      { channel: "Cartoon Network", folder: "Ben 10", name: "Ben 10", icon: "", episode: [ep(0, 1, "x")] },
      { channel: "MTV", folder: "Show", name: "Show", icon: "", episode: [] },
    ],
  };

  it("groups shows by channel in library order and attaches ads", () => {
    const tree = buildTree(library);
    expect(tree.map((c) => c.channel)).toEqual(["Cartoon Network", "MTV", ""]);
    const cn = tree.find((c) => c.channel === "Cartoon Network")!;
    expect(cn.shows.map((s) => s.show.folder)).toEqual(["Gumball", "Ben 10"]);
    expect(tree.find((c) => c.channel === "MTV")!.ads.map((a) => a.path)).toEqual(["MTV/ads/b.mpg"]);
    expect(tree.find((c) => c.channel === "")!.ads).toHaveLength(1);
  });

  it("orders seasons and episodes", () => {
    const gumball = buildTree(library).find((c) => c.channel === "Cartoon Network")!.shows[0]!;
    expect(gumball.seasons.map((s) => s.season)).toEqual([1, 2]);
    expect(gumball.seasons[0]!.episodes.map((e) => e.title)).toEqual(["a", "b"]);
  });
});

describe("seasonLabel", () => {
  it("labels season zero as Episodes", () => {
    expect(seasonLabel(0)).toBe("Episodes");
    expect(seasonLabel(3)).toBe("Season 3");
  });
});

describe("descriptionKey", () => {
  it("uses SxxExx for seasoned episodes and the file stem otherwise", () => {
    expect(descriptionKey({ season: 1, episode: 2, path: "x.mpg" })).toBe("S01E02");
    expect(descriptionKey({ season: 0, episode: 1, path: "C/S/E01 - Pilot.mpg" })).toBe("E01 - Pilot");
  });
});
