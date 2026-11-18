import { expect, test } from "vitest";
import type { probe } from "../src/server/ffmpeg.js";
import { SHRINK_ABOVE_KBPS, needsShrink, shrinkCandidates } from "../src/server/shrink.js";
import type { Library } from "../src/server/types.js";

const lib: Library = {
  version: 1,
  generated: new Date(0),
  ad: [{ path: "Ads/spot.mpg", channel: "Ads", duration_ms: 30000 }],
  show: [
    {
      channel: "Cartoon Network",
      folder: "Gumball",
      name: "Gumball",
      icon: "",
      episode: [
        { season: 1, episode: 1, title: "Big", description: "", path: "CN/Gumball/big.mpg", duration_ms: 60000 },
        { season: 1, episode: 2, title: "Small", description: "", path: "CN/Gumball/small.mpg", duration_ms: 60000 },
        { season: 1, episode: 3, title: "Busy", description: "", path: "CN/Gumball/busy.mpg", duration_ms: 60000 },
        { season: 1, episode: 4, title: "Broken", description: "", path: "CN/Gumball/broken.mpg", duration_ms: 60000 },
        { season: 1, episode: 5, title: "Narrow", description: "", path: "CN/Gumball/narrow.mpg", duration_ms: 60000 },
      ],
    },
  ],
};

const fake: typeof probe = async (file) => {
  const name = file.split("/").pop() ?? "";
  if (name.startsWith("broken")) throw new Error("no video stream");
  const wide = !name.startsWith("narrow");
  const bitRateKbps = name.startsWith("small") ? 1100 : 1700;
  return { durationMs: 60000, width: wide ? 640 : 480, height: 360, displayAspect: wide ? 16 / 9 : 4 / 3, videoStreamIndex: 0, bitRateKbps };
};

test("needsShrink is true only above the threshold", () => {
  expect(SHRINK_ABOVE_KBPS).toBe(1300);
  expect(needsShrink(1700)).toBe(true);
  expect(needsShrink(1100)).toBe(false);
  expect(needsShrink(1300)).toBe(false);
  expect(needsShrink(null)).toBe(false);
});

test("shrinkCandidates returns the heavy files, skipping busy and unreadable ones", async () => {
  const found = await shrinkCandidates("/root", lib, new Set(["CN/Gumball/busy.mpg"]), fake);
  expect(found).toEqual([
    { target: "Ads/spot.mpg", aspect: "16:9" },
    { target: "CN/Gumball/big.mpg", aspect: "16:9" },
    { target: "CN/Gumball/narrow.mpg", aspect: "4:3" },
  ]);
});
