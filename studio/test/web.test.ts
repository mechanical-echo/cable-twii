import { describe, expect, it } from "vitest";
import { baseName, formatBytes, formatDuration } from "../src/web/format.js";
import { describeRejected, isUsable } from "../src/web/importing.js";
import { buildTree, descriptionKey, seasonLabel, type WebLibrary } from "../src/web/tree.js";
import { channelChoices } from "../src/web/channels.js";
import { batches, crumbs, joinRel, rejectedLine, spreadLoaded, storedUploadName, tickedFiles, uploadGroups, uploadRatio } from "../src/web/uploading.js";
import type { Config } from "../src/server/types.js";

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

describe("isUsable", () => {
  it("rejects too short files and files with errors", () => {
    expect(isUsable({ tooShort: false })).toBe(true);
    expect(isUsable({ tooShort: true })).toBe(false);
    expect(isUsable({ tooShort: false, error: "bad" })).toBe(false);
  });
});

describe("describeRejected", () => {
  it("returns null when nothing was rejected", () => {
    expect(describeRejected(3, [])).toBeNull();
  });

  it("lists skipped files with reasons", () => {
    expect(describeRejected(2, [{ source: "/a/b.mp4", reason: "too short" }, { source: "/c.mp4", reason: "bad" }])).toBe(
      "Imported 2. Skipped: b.mp4 (too short); c.mp4 (bad)",
    );
  });
});

describe("uploadRatio", () => {
  it("is the sent share of the total", () => {
    expect(uploadRatio(50, 200)).toBe(0.25);
    expect(uploadRatio(200, 200)).toBe(1);
  });

  it("is 0 when the total is unknown", () => {
    expect(uploadRatio(0, 0)).toBe(0);
    expect(uploadRatio(10, 0)).toBe(0);
  });

  it("stays within 0 and 1 when multipart framing overshoots", () => {
    expect(uploadRatio(210, 200)).toBe(1);
    expect(uploadRatio(-5, 200)).toBe(0);
  });
});

describe("crumbs", () => {
  it("starts at the imports root", () => {
    expect(crumbs("")).toEqual([{ name: "imports", rel: "" }]);
  });

  it("adds one crumb per folder with its path from the root", () => {
    expect(crumbs("a/b")).toEqual([
      { name: "imports", rel: "" },
      { name: "a", rel: "a" },
      { name: "b", rel: "a/b" },
    ]);
  });
});

describe("joinRel", () => {
  it("adds a folder name to a path from the imports root", () => {
    expect(joinRel("", "a")).toBe("a");
    expect(joinRel("a", "b c")).toBe("a/b c");
  });
});

describe("batches", () => {
  it("splits a list into runs of at most the given size, keeping order", () => {
    expect(batches([1, 2, 3, 4, 5], 2)).toEqual([[1, 2], [3, 4], [5]]);
    expect(batches([1, 2], 2)).toEqual([[1, 2]]);
  });

  it("gives no batches for an empty list", () => {
    expect(batches([], 50)).toEqual([]);
  });
});

describe("rejectedLine", () => {
  it("shows the file name and the first line of the reason", () => {
    expect(rejectedLine({ name: "a.mp4", reason: "not a video" })).toBe("a.mp4: not a video");
    expect(rejectedLine({ name: "b.mp4", reason: "could not read the video: moov atom not found\nfile:/srv/library/.uploads/x/b.mp4: Invalid data" })).toBe(
      "b.mp4: could not read the video: moov atom not found",
    );
  });
});

describe("tickedFiles", () => {
  it("keeps the ticked files in order", () => {
    expect(tickedFiles(["a", "b", "c"], [true, false, true])).toEqual(["a", "c"]);
  });
});

describe("channelChoices", () => {
  const config = { channel: [{ name: "Cartoon Network" }, { name: "MTV", folder: "mtv-00s" }] } as Config;

  it("merges config folders and library channels, de-duplicated and sorted", () => {
    expect(channelChoices(config, ["Cartoon Network", "Nickelodeon"])).toEqual(["Cartoon Network", "mtv-00s", "Nickelodeon"]);
  });

  it("is empty without a config or channels", () => {
    expect(channelChoices(null, [])).toEqual([]);
  });
});

describe("uploadGroups", () => {
  it("groups files by their batch folder", () => {
    expect(uploadGroups(["/lib/.uploads/a/x.mp4", "/lib/.uploads/b/y.mp4", "/lib/.uploads/a/z.mp4"])).toEqual([
      { dir: "/lib/.uploads/a", names: ["x.mp4", "z.mp4"] },
      { dir: "/lib/.uploads/b", names: ["y.mp4"] },
    ]);
  });
});

describe("storedUploadName", () => {
  it("matches the name the server stores an upload under", () => {
    expect(storedUploadName("Ep 1: Pilot?.mp4")).toBe("Ep 1- Pilot.mp4");
    expect(storedUploadName("Gumball.S03E19.The.Pest.mkv")).toBe("Gumball.S03E19.The.Pest.mkv");
  });
});

describe("spreadLoaded", () => {
  it("fills files in the order they are sent", () => {
    expect(spreadLoaded([100, 50, 10], 0)).toEqual([0, 0, 0]);
    expect(spreadLoaded([100, 50, 10], 120)).toEqual([100, 20, 0]);
    expect(spreadLoaded([100, 50, 10], 500)).toEqual([100, 50, 10]);
  });
});
