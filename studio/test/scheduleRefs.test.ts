import { describe, expect, it } from "vitest";
import type { Channel, Config, Item } from "../src/server/types.js";
import {
  adChannels,
  appendItems,
  channelFolder,
  channelsAiring,
  describeItem,
  episodeChannels,
  itemShow,
  itemsNaming,
  removeItems,
  renameAd,
  retargetEpisodes,
} from "../src/server/scheduleRefs.js";

const cn: Channel = {
  number: 1,
  name: "Cartoon Network",
  category: "Kids",
  ads: true,
  mode: "timed",
  slot: [
    { at: "06:00", items: [{ type: "random", show: "Gumball", count: 2 }, { type: "ads", count: 1 }] },
    { at: "12:00", items: [{ type: "episode", show: "Gumball", season: 1, episode: 3 }, { type: "ad", path: "ads/cola.mpg" }] },
  ],
};
const xmas: Channel = {
  number: 5,
  name: "Christmas",
  category: "Fun",
  folder: "xmas",
  ads: false,
  mode: "looping",
  loop: [
    { type: "episode", show: "Gumball", season: 2, episode: 1, from: "Cartoon Network" },
    { type: "episode", show: "SpongeBob", season: 1, episode: 4, from: "Nick" },
  ],
};
const nick: Channel = { number: 3, name: "Nick", category: "Kids", ads: true, mode: "looping", loop: [{ type: "fill", show: "SpongeBob", season: 1 }, { type: "ads", count: 2 }] };
const config: Config = { version: 1, seed: 1, channel: [cn, nick, xmas] };
const gumball = { channel: "Cartoon Network", show: "Gumball" };

describe("references", () => {
  it("names the home folder unless from is set", () => {
    expect(channelFolder(xmas)).toBe("xmas");
    expect(channelFolder(cn)).toBe("Cartoon Network");
    expect(itemShow({ type: "random", show: "Gumball", count: 1 }, "Cartoon Network")).toEqual(gumball);
    expect(itemShow({ type: "episode", show: "Gumball", season: 1, episode: 1, from: "Old" }, "xmas")).toEqual({ channel: "Old", show: "Gumball" });
    expect(itemShow({ type: "ads", count: 1 }, "xmas")).toBeNull();
  });

  it("channels airing a show", () => {
    expect(channelsAiring(config, gumball)).toEqual([1, 5]);
    expect(channelsAiring(config, { channel: "Nick", show: "SpongeBob" })).toEqual([3, 5]);
    expect(channelsAiring(config, { channel: "Nick", show: "Rugrats" })).toEqual([]);
  });

  it("episode channels", () => {
    expect(episodeChannels(config, gumball, 1, 3)).toEqual([1]);
    expect(episodeChannels(config, gumball, 2, 1)).toEqual([1, 5]);
    expect(episodeChannels(config, { channel: "Nick", show: "SpongeBob" }, 1, 9)).toEqual([3]);
    expect(episodeChannels(config, { channel: "Nick", show: "SpongeBob" }, 2, 1)).toEqual([]);
  });

  it("ad channels", () => {
    const library = { ad: [{ path: "ads/cola.mpg", channel: "", duration_ms: 1000 }, { path: "Nick/ads/slime.mpg", channel: "Nick", duration_ms: 1000 }] };
    expect(adChannels(config, library, "ads/cola.mpg")).toEqual([1]);
    expect(adChannels(config, library, "Nick/ads/slime.mpg")).toEqual([3]);
    const quiet = { ...config, channel: [{ ...cn, ads: false }, nick, xmas] };
    expect(adChannels(quiet, library, "ads/cola.mpg")).toEqual([1]);
    expect(adChannels(quiet, { ad: [] }, "ads/other.mpg")).toEqual([]);
  });

  it("items naming an episode are episode items only", () => {
    expect(itemsNaming(config, { show: gumball, season: 1, episode: 3 })).toEqual([{ channel: 1, slot: 1, item: 0 }]);
    expect(itemsNaming(config, { show: gumball })).toEqual([
      { channel: 1, slot: 0, item: 0 },
      { channel: 1, slot: 1, item: 0 },
      { channel: 5, slot: -1, item: 0 },
    ]);
    expect(itemsNaming(config, { ad: "ads/cola.mpg" })).toEqual([{ channel: 1, slot: 1, item: 1 }]);
  });
});

describe("rewrites", () => {
  it("remove items keeps the rest in order", () => {
    const next = removeItems(config, [{ channel: 1, slot: 0, item: 0 }, { channel: 5, slot: -1, item: 0 }]);
    expect(next.channel[0]!.slot![0]!.items).toEqual([{ type: "ads", count: 1 }]);
    expect(next.channel[2]!.loop).toEqual([xmas.loop![1]]);
    expect(config.channel[0]!.slot![0]!.items).toHaveLength(2);
  });

  it("retarget follows moved episodes", () => {
    const next = retargetEpisodes(config, gumball, [
      { from: [1, 3], to: [1, 2] },
      { from: [2, 1], to: [1, 3] },
    ]);
    expect(next.channel[0]!.slot![1]!.items[0]).toEqual({ type: "episode", show: "Gumball", season: 1, episode: 2 });
    expect(next.channel[2]!.loop![0]).toEqual({ type: "episode", show: "Gumball", season: 1, episode: 3, from: "Cartoon Network" });
    expect(next.channel[2]!.loop![1]).toEqual(xmas.loop![1]);
  });

  it("renames ads", () => {
    const next = renameAd(config, "ads/cola.mpg", "Cartoon Network/ads/cola.mpg");
    expect(next.channel[0]!.slot![1]!.items[1]).toEqual({ type: "ad", path: "Cartoon Network/ads/cola.mpg" });
  });

  it("append to loop and to a slot", () => {
    const pick: Item = { type: "episode", show: "Gumball", season: 1, episode: 1, from: "Cartoon Network" };
    expect(appendItems(config, 5, null, [pick]).channel[2]!.loop!.at(-1)).toEqual(pick);
    expect(appendItems(config, 1, 0, [{ type: "ads", count: 3 }]).channel[0]!.slot![0]!.items.at(-1)).toEqual({ type: "ads", count: 3 });
  });

  it("describe items", () => {
    const name = (ref: { show: string }) => (ref.show === "Gumball" ? "The Amazing World of Gumball" : ref.show);
    expect(describeItem({ type: "random", show: "Gumball", count: 2 }, "Cartoon Network", name)).toBe("Random episode ×2 · The Amazing World of Gumball");
    expect(describeItem({ type: "random-run", show: "Gumball", count: 3 }, "Cartoon Network", name)).toBe("Random run ×3 · The Amazing World of Gumball");
    expect(describeItem({ type: "fill", show: "SpongeBob" }, "Cartoon Network", name)).toBe("Fill · SpongeBob");
    expect(describeItem({ type: "episode", show: "SpongeBob", season: 1, episode: 4 }, "Cartoon Network", name)).toBe("S01E04 · SpongeBob");
    expect(describeItem({ type: "episode", show: "Videos", season: 0, episode: 7 }, "Cartoon Network", name)).toBe("E07 · Videos");
    expect(describeItem({ type: "ads", count: 2 }, "Cartoon Network", name)).toBe("Ad break ×2");
    expect(describeItem({ type: "ad", path: "ads/cola.mpg" }, "Cartoon Network", name)).toBe("Ad · cola");
  });
});
