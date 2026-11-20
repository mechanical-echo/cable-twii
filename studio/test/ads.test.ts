import { describe, expect, it } from "vitest";
import type { Config } from "../src/server/types.js";
import { adGroups, skippedAds } from "../src/web/ads.js";
import type { WebLibrary } from "../src/web/tree.js";

const config: Config = {
  version: 1,
  seed: 1,
  channel: [
    { number: 2, name: "MTV", category: "Music", folder: "mtv-00s", ads: true, mode: "looping" },
    { number: 1, name: "Cartoon Network", category: "Kids", ads: true, mode: "timed" },
  ],
};

function library(ads: { path: string; channel: string; duration_ms: number }[]): WebLibrary {
  return { version: 1, generated: "2026-11-20T00:00:00Z", ad: ads, show: [] };
}

describe("adGroups", () => {
  it("groups ads by channel", () => {
    const groups = adGroups(library([{ path: "ads/ad-c.mpg", channel: "", duration_ms: 3000 }, { path: "ads/ad-a.mpg", channel: "", duration_ms: 1000 }]), config);
    expect(groups).toEqual([
      {
        channel: "",
        label: "All channels",
        ads: [
          { path: "ads/ad-a.mpg", name: "ad-a", durationMs: 1000 },
          { path: "ads/ad-c.mpg", name: "ad-c", durationMs: 3000 },
        ],
      },
    ]);
  });

  it("channel ads use the channel name", () => {
    const groups = adGroups(
      library([
        { path: "mtv-00s/ads/m.mpg", channel: "mtv-00s", duration_ms: 1000 },
        { path: "Gone/ads/g.mpg", channel: "Gone", duration_ms: 1000 },
        { path: "Cartoon Network/ads/c.mpg", channel: "Cartoon Network", duration_ms: 2000 },
        { path: "ads/a.mpg", channel: "", duration_ms: 1000 },
      ]),
      config,
    );
    expect(groups.map((g) => [g.channel, g.label])).toEqual([
      ["", "All channels"],
      ["Cartoon Network", "Cartoon Network"],
      ["mtv-00s", "MTV"],
      ["Gone", "Gone"],
    ]);
  });

  it("has no groups without ads", () => {
    expect(adGroups(library([]), config)).toEqual([]);
  });
});

describe("skippedAds", () => {
  it("skipped ads only", () => {
    const notes = { skipped: [{ path: "ads/ad-b.mpg", reason: "too short" }, { path: "Cartoon Network/Gumball/x.mpg", reason: "bad" }, { path: "MTV/ads/m.mpg", reason: "bad" }] };
    expect(skippedAds(notes)).toEqual([
      { path: "ads/ad-b.mpg", reason: "too short" },
      { path: "MTV/ads/m.mpg", reason: "bad" },
    ]);
  });
});
