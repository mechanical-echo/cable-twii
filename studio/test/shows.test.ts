import { describe, expect, it } from "vitest";
import type { Config, LibShow } from "../src/server/types.js";
import { findAd, findEpisode, pickedFrom, seasonTargets, showGroups } from "../src/web/shows.js";
import type { WebLibrary } from "../src/web/tree.js";

function show(channel: string, folder: string, name: string, seasons: number[], path = (s: number, e: number) => `${channel}/${folder}/Season ${s}/S0${s}E0${e} - E.mpg`): LibShow {
  return { channel, folder, name, icon: "", episode: seasons.flatMap((s) => [1, 2].map((e) => ({ season: s, episode: e, title: `T${s}${e}`, description: "", path: path(s, e), duration_ms: 60000 }))) };
}

const config: Config = {
  version: 1,
  seed: 1,
  channel: [
    { number: 2, name: "Nick", category: "Kids", ads: true, mode: "looping", loop: [{ type: "random", show: "SpongeBob", count: 1 }] },
    { number: 1, name: "CN", category: "Kids", folder: "Cartoon Network", ads: true, mode: "looping", loop: [{ type: "random", show: "Gumball", count: 1 }, { type: "random", show: "SpongeBob", count: 1, from: "Nick" }] },
  ],
};

const library: WebLibrary = {
  version: 1,
  generated: "",
  ad: [{ path: "ads/cola.mpg", channel: "", duration_ms: 5000 }],
  show: [show("Cartoon Network", "Gumball", "Gumball", [1, 2]), show("Nick", "SpongeBob", "SpongeBob", [1]), show("Old", "Retro", "Retro", [1])],
  off: { ad: [{ path: "ads/old.mpg", channel: "", duration_ms: 5000 }], show: [{ ...show("Cartoon Network", "Gumball", "Gumball", [3]) }, show("Cartoon Network", "Hidden", "Hidden", [1])] },
};

describe("showGroups", () => {
  it("groups shows by the folder that holds them", () => {
    const groups = showGroups(library, config);
    expect(groups.map((g) => [g.folder, g.label, g.shows.map((s) => s.name)])).toEqual([
      ["Cartoon Network", "CN", ["Gumball", "Hidden"]],
      ["Nick", "Nick", ["SpongeBob"]],
      ["Old", "Old · not on a channel", ["Retro"]],
    ]);
  });

  it("counts seasons and episodes including off-air ones", () => {
    const gumball = showGroups(library, config)[0]!.shows[0]!;
    expect(gumball).toMatchObject({ seasons: 3, episodes: 6, off: 2 });
  });

  it("airs on uses the schedules", () => {
    const groups = showGroups(library, config);
    expect(groups[1]!.shows[0]!.airsOn).toEqual([1, 2]);
    expect(groups[2]!.shows[0]!.airsOn).toEqual([]);
  });
});

describe("finding", () => {
  it("finds an episode by path, on or off air", () => {
    expect(findEpisode(library, "Cartoon Network/Gumball/Season 2/S02E01 - E.mpg")).toMatchObject({ show: { folder: "Gumball" }, episode: { season: 2, episode: 1 }, off: false });
    expect(findEpisode(library, "Cartoon Network/Gumball/Season 3/S03E02 - E.mpg")).toMatchObject({ episode: { season: 3, episode: 2 }, off: true });
    expect(findEpisode(library, "nope.mpg")).toBeNull();
  });

  it("finds an ad", () => {
    expect(findAd(library, "ads/cola.mpg")).toMatchObject({ off: false });
    expect(findAd(library, "ads/old.mpg")).toMatchObject({ off: true });
  });
});

describe("management helpers", () => {
  it("season targets", () => {
    expect(seasonTargets(show("CN", "Gumball", "Gumball", [1, 2]))).toEqual([1, 2, 3]);
    expect(seasonTargets(show("CN", "Videos", "Videos", [0], () => "CN/Videos/x.mpg"))).toEqual([1]);
    expect(seasonTargets(show("CN", "Mixed", "Mixed", [0, 3]))).toEqual([3, 4]);
  });

  it("picked from lists channels with picks", () => {
    const picks: Config = {
      ...config,
      channel: [
        ...config.channel,
        { number: 7, name: "Xmas", category: "Fun", ads: false, mode: "looping", loop: [{ type: "episode", show: "SpongeBob", season: 1, episode: 2, from: "Nick" }] },
      ],
    };
    expect(pickedFrom(picks, { channel: "Nick", show: "SpongeBob" })).toEqual([7]);
    expect(pickedFrom(picks, { channel: "Cartoon Network", show: "Gumball" })).toEqual([]);
  });
});
