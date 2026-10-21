import { describe, expect, it } from "vitest";
import type { Airing, LibShow } from "../src/server/types.js";
import {
  adBreakLabel,
  findShow,
  formatClock,
  formatEpisodeCode,
  groupAirings,
  localDate,
  localMs,
  nowPosition
} from "../src/web/preview.js";

function airing(kind: Airing["kind"], start_ms: number, duration_ms: number, extra: Partial<Airing> = {}): Airing {
  return { start_ms, duration_ms, kind, path: null, show: null, season: null, episode: null, title: null, slot: 0, item: 0, ...extra };
}

function show(channel: string, folder: string, name: string): LibShow {
  return { channel, folder, name, icon: `${channel}/${folder}/show.png`, episode: [] };
}

describe("formatClock", () => {
  it("formats HH:MM and drops seconds", () => {
    expect(formatClock(0)).toBe("00:00");
    expect(formatClock(7 * 3600_000 + 5 * 60_000 + 59_000)).toBe("07:05");
    expect(formatClock(86_400_000 - 1)).toBe("23:59");
  });

  it("wraps past midnight with +1", () => {
    expect(formatClock(86_400_000)).toBe("00:00 +1");
    expect(formatClock(86_400_000 + 90 * 60_000)).toBe("01:30 +1");
  });
});

describe("formatEpisodeCode", () => {
  it("pads season and episode", () => {
    expect(formatEpisodeCode(1, 2)).toBe("S01E02");
    expect(formatEpisodeCode(12, 103)).toBe("S12E103");
  });

  it("omits the season for season 0", () => {
    expect(formatEpisodeCode(0, 5)).toBe("E05");
    expect(formatEpisodeCode(null, 5)).toBe("E05");
  });

  it("is empty without an episode", () => {
    expect(formatEpisodeCode(null, null)).toBe("");
    expect(formatEpisodeCode(1, null)).toBe("");
  });
});

describe("groupAirings", () => {
  it("collapses consecutive ads into one row", () => {
    const rows = groupAirings([
      airing("episode", 0, 600_000, { show: "A", season: 1, episode: 1 }),
      airing("ad", 600_000, 30_000),
      airing("ad", 630_000, 15_000),
      airing("ad", 645_000, 30_000),
      airing("episode", 675_000, 600_000),
      airing("ad", 1_275_000, 30_000),
      airing("upnext", 1_305_000, 100_000)
    ]);
    expect(rows.map((r) => r.kind)).toEqual(["episode", "ad", "episode", "ad", "upnext"]);
    expect(rows[1]).toMatchObject({ kind: "ad", startMs: 600_000, endMs: 675_000, count: 3, totalMs: 75_000 });
    expect(rows[3]).toMatchObject({ count: 1, startMs: 1_275_000, endMs: 1_305_000 });
  });

  it("keeps row keys unique and handles empty input", () => {
    expect(groupAirings([])).toEqual([]);
    const rows = groupAirings([airing("episode", 0, 10), airing("ad", 10, 10), airing("episode", 20, 10)]);
    expect(new Set(rows.map((r) => r.key)).size).toBe(3);
  });
});

describe("adBreakLabel", () => {
  it("names the count and duration", () => {
    expect(adBreakLabel({ count: 3 }, "1:15")).toBe("Ad break (3 ads, 1:15)");
    expect(adBreakLabel({ count: 1 }, "0:30")).toBe("Ad break (1 ad, 0:30)");
  });
});

describe("findShow", () => {
  const shows = [show("Cartoon Network", "Gumball", "The Amazing World of Gumball"), show("MTV", "Gumball", "Other Gumball"), show("MTV", "Daria", "Daria")];

  it("prefers the channel folder match", () => {
    expect(findShow(shows, "MTV", "Gumball")?.name).toBe("Other Gumball");
  });

  it("falls back to any show with that folder", () => {
    expect(findShow(shows, "Comedy", "Daria")?.name).toBe("Daria");
  });

  it("returns null when unknown or no folder", () => {
    expect(findShow(shows, "MTV", "Nope")).toBeNull();
    expect(findShow(shows, "MTV", null)).toBeNull();
  });
});

describe("nowPosition", () => {
  const rows = groupAirings([airing("episode", 1000, 1000), airing("ad", 2000, 500), airing("episode", 2500, 1000)]);

  it("marks the airing in progress as current", () => {
    expect(nowPosition(rows, 1000)).toEqual({ index: 0, current: true });
    expect(nowPosition(rows, 2100)).toEqual({ index: 1, current: true });
    expect(nowPosition(rows, 2500)).toEqual({ index: 2, current: true });
  });

  it("places the marker before the first row when early and after the last when late", () => {
    expect(nowPosition(rows, 500)).toEqual({ index: 0, current: false });
    expect(nowPosition(rows, 3500)).toEqual({ index: 3, current: false });
  });

  it("is null without rows", () => {
    expect(nowPosition([], 0)).toBeNull();
  });
});

describe("local time", () => {
  it("reads the local date and ms since midnight", () => {
    const now = new Date(2026, 0, 5, 13, 45, 30, 250);
    expect(localDate(now)).toBe("2026-01-05");
    expect(localMs(now)).toBe(((13 * 60 + 45) * 60 + 30) * 1000 + 250);
  });
});
