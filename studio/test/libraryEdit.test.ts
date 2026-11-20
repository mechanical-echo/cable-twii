import { describe, expect, it } from "vitest";
import { EditRefused, planEdit } from "../src/server/libraryEdit.js";
import type { Config, LibEpisode, Library } from "../src/server/types.js";

const ep = (show: string, season: number, episode: number, title: string, file?: string): LibEpisode => ({
  season,
  episode,
  title,
  description: "",
  path: file ?? `Cartoon Network/${show}/Season ${season}/S0${season}E0${episode} - ${title}.mpg`,
  duration_ms: 60000,
});

const library: Library = {
  version: 1,
  generated: new Date(0),
  ad: [
    { path: "ads/cola.mpg", channel: "", duration_ms: 5000 },
    { path: "Cartoon Network/ads/toy.mpg", channel: "Cartoon Network", duration_ms: 5000 },
  ],
  show: [
    {
      channel: "Cartoon Network",
      folder: "Gumball",
      name: "Gumball",
      icon: "",
      episode: [ep("Gumball", 1, 1, "One"), ep("Gumball", 1, 2, "Two"), ep("Gumball", 1, 3, "Three"), ep("Gumball", 2, 1, "Next")],
    },
    {
      channel: "Cartoon Network",
      folder: "Videos",
      name: "Videos",
      icon: "",
      episode: [
        { ...ep("Videos", 0, 1, "A", "Cartoon Network/Videos/A.mpg") },
        { ...ep("Videos", 0, 2, "C", "Cartoon Network/Videos/C.mpg") },
        { ...ep("Videos", 0, 3, "D", "Cartoon Network/Videos/D.mpg") },
      ],
    },
  ],
};

const config: Config = {
  version: 1,
  seed: 1,
  channel: [
    {
      number: 1,
      name: "Cartoon Network",
      category: "Kids",
      ads: true,
      mode: "timed",
      slot: [{ at: "06:00", items: [{ type: "random", show: "Gumball", count: 1 }, { type: "episode", show: "Gumball", season: 1, episode: 2 }, { type: "ad", path: "ads/cola.mpg" }] }],
    },
    {
      number: 5,
      name: "Christmas",
      category: "Fun",
      folder: "xmas",
      ads: false,
      mode: "looping",
      loop: [{ type: "episode", show: "Videos", season: 0, episode: 3, from: "Cartoon Network" }, { type: "episode", show: "Gumball", season: 1, episode: 3, from: "Cartoon Network" }],
    },
  ],
};

const files: Record<string, string[]> = { Gumball: ["Season 1", "Season 2", "show.toml"], Videos: ["A.mpg", "C.mpg", "D.mpg"] };

function ctx(extra: Partial<Parameters<typeof planEdit>[1]> = {}): Parameters<typeof planEdit>[1] {
  return { config, library, off: new Set(), busy: new Set(), showFiles: (ref) => files[ref.show] ?? [], ...extra };
}

function refused(run: () => unknown): EditRefused {
  try {
    run();
  } catch (err) {
    if (err instanceof EditRefused) return err;
    throw err;
  }
  throw new Error("expected a refusal");
}

describe("episode edits", () => {
  it("renames a title in place", () => {
    const plan = planEdit({ kind: "episode", path: "Cartoon Network/Gumball/Season 1/S01E02 - Two.mpg", title: "Second", season: 1, episode: 2 }, ctx());
    expect(plan.moves).toEqual([{ from: "Cartoon Network/Gumball/Season 1/S01E02 - Two.mpg", to: "Cartoon Network/Gumball/Season 1/S01E02 - Second.mpg" }]);
    expect(plan.config).toEqual(config);
  });

  it("renumbering moves the file, its picks and its description", () => {
    const plan = planEdit({ kind: "episode", path: "Cartoon Network/Gumball/Season 1/S01E02 - Two.mpg", title: "Two", season: 2, episode: 5 }, ctx());
    expect(plan.moves).toEqual([{ from: "Cartoon Network/Gumball/Season 1/S01E02 - Two.mpg", to: "Cartoon Network/Gumball/Season 2/S02E05 - Two.mpg" }]);
    expect(plan.config.channel[0]!.slot![0]!.items[1]).toEqual({ type: "episode", show: "Gumball", season: 2, episode: 5 });
    expect(plan.describeKeys).toEqual([{ showDir: "Cartoon Network/Gumball", from: "S01E02", to: "S02E05" }]);
  });

  it("a taken number is refused", () => {
    const err = refused(() => planEdit({ kind: "episode", path: "Cartoon Network/Gumball/Season 1/S01E02 - Two.mpg", title: "Two", season: 1, episode: 3 }, ctx()));
    expect(err.status).toBe(409);
    expect(err.message).toBe("S01E03 already exists: Three");
  });

  it("seasonless rename keeps picks on the same videos", () => {
    const plan = planEdit({ kind: "episode", path: "Cartoon Network/Videos/A.mpg", title: "E", season: null, episode: null }, ctx());
    expect(plan.moves).toEqual([{ from: "Cartoon Network/Videos/A.mpg", to: "Cartoon Network/Videos/E.mpg" }]);
    expect(plan.config.channel[1]!.loop![0]).toEqual({ type: "episode", show: "Videos", season: 0, episode: 2, from: "Cartoon Network" });
    expect(plan.describeKeys).toEqual([{ showDir: "Cartoon Network/Videos", from: "A", to: "E" }]);
  });

  it("a converting file can't be planned", () => {
    const err = refused(() =>
      planEdit({ kind: "episode", path: "Cartoon Network/Gumball/Season 1/S01E02 - Two.mpg", title: "X", season: 1, episode: 2 }, ctx({ busy: new Set(["Cartoon Network/Gumball/Season 1/S01E02 - Two.mpg"]) })),
    );
    expect(err.status).toBe(409);
    expect(err.message).toBe("Still converting. Try again when it's done.");
  });
});

describe("ad edits", () => {
  it("renames and moves an ad and its items", () => {
    const plan = planEdit({ kind: "ad", path: "ads/cola.mpg", name: "Cola Zero", channel: "Cartoon Network" }, ctx());
    expect(plan.moves).toEqual([{ from: "ads/cola.mpg", to: "Cartoon Network/ads/Cola Zero.mpg" }]);
    expect(plan.config.channel[0]!.slot![0]!.items[2]).toEqual({ type: "ad", path: "Cartoon Network/ads/Cola Zero.mpg" });
  });

  it("a taken ad name is refused", () => {
    expect(refused(() => planEdit({ kind: "ad", path: "ads/cola.mpg", name: "toy", channel: "Cartoon Network" }, ctx())).status).toBe(409);
  });
});

describe("move to season", () => {
  it("move to season renumbers after the last episode", () => {
    const plan = planEdit({ kind: "move-season", paths: ["Cartoon Network/Gumball/Season 1/S01E03 - Three.mpg", "Cartoon Network/Gumball/Season 1/S01E01 - One.mpg"], season: 2 }, ctx());
    expect(plan.moves.map((m) => m.to)).toEqual(["Cartoon Network/Gumball/Season 2/S02E02 - Three.mpg", "Cartoon Network/Gumball/Season 2/S02E03 - One.mpg"]);
    expect(plan.config.channel[1]!.loop![1]).toEqual({ type: "episode", show: "Gumball", season: 2, episode: 2, from: "Cartoon Network" });
  });

  it("moving out of a show without seasons renumbers the files left behind", () => {
    const plan = planEdit({ kind: "move-season", paths: ["Cartoon Network/Videos/A.mpg"], season: 1 }, ctx());
    expect(plan.moves).toEqual([{ from: "Cartoon Network/Videos/A.mpg", to: "Cartoon Network/Videos/Season 1/S01E01 - A.mpg" }]);
    expect(plan.config.channel[1]!.loop![0]).toEqual({ type: "episode", show: "Videos", season: 0, episode: 2, from: "Cartoon Network" });
  });
});

describe("off and delete", () => {
  it("switching off removes direct items only", () => {
    const plan = planEdit({ kind: "off", paths: ["Cartoon Network/Gumball/Season 1/S01E02 - Two.mpg", "ads/cola.mpg"], off: true }, ctx());
    expect([...plan.off]).toEqual(["Cartoon Network/Gumball/Season 1/S01E02 - Two.mpg", "ads/cola.mpg"]);
    expect([...plan.exclude]).toEqual([...plan.off]);
    expect(plan.config.channel[0]!.slot![0]!.items).toEqual([{ type: "random", show: "Gumball", count: 1 }]);
    expect(plan.removed).toEqual([
      { channel: 1, label: "S01E02 · Gumball" },
      { channel: 1, label: "Ad · cola" },
    ]);
  });

  it("switching on changes no schedule", () => {
    const plan = planEdit({ kind: "off", paths: ["ads/cola.mpg"], off: false }, ctx({ off: new Set(["ads/cola.mpg"]) }));
    expect(plan.off.size).toBe(0);
    expect(plan.config).toEqual(config);
  });

  it("delete removes the file and its direct items", () => {
    const plan = planEdit({ kind: "delete", paths: ["Cartoon Network/Gumball/Season 1/S01E03 - Three.mpg"] }, ctx());
    expect(plan.deletes).toEqual(["Cartoon Network/Gumball/Season 1/S01E03 - Three.mpg"]);
    expect(plan.exclude.has("Cartoon Network/Gumball/Season 1/S01E03 - Three.mpg")).toBe(true);
    expect(plan.config.channel[1]!.loop).toEqual([config.channel[1]!.loop![0]]);
  });

  it("delete show takes every file and every item naming it", () => {
    const plan = planEdit({ kind: "delete-show", channel: "Cartoon Network", show: "Gumball" }, ctx({ off: new Set(["Cartoon Network/Gumball/Season 2/S02E01 - Next.mpg"]) }));
    expect(plan.deletes).toHaveLength(4);
    expect(plan.deleteDirs).toEqual(["Cartoon Network/Gumball"]);
    expect(plan.config.channel[0]!.slot![0]!.items).toEqual([{ type: "ad", path: "ads/cola.mpg" }]);
    expect(plan.off.size).toBe(0);
  });
});

describe("channels", () => {
  it("airs-on adds one rule, with from when needed", () => {
    const quiet: Config = { ...config, channel: [config.channel[0]!, { ...config.channel[1]!, loop: [config.channel[1]!.loop![0]!] }] };
    const plan = planEdit({ kind: "airs-on", channel: "Cartoon Network", show: "Gumball", number: 5, on: true, slot: null }, ctx({ config: quiet }));
    expect(plan.config.channel[1]!.loop!.at(-1)).toEqual({ type: "random", show: "Gumball", count: 1, from: "Cartoon Network" });
  });

  it("ticking a channel that already airs the show changes nothing", () => {
    const plan = planEdit({ kind: "airs-on", channel: "Cartoon Network", show: "Gumball", number: 5, on: true, slot: null }, ctx());
    expect(plan.config).toEqual(config);
  });

  it("airs-on needs a slot on a timed channel", () => {
    const err = refused(() => planEdit({ kind: "airs-on", channel: "Cartoon Network", show: "Videos", number: 1, on: true, slot: null }, ctx()));
    expect(err.status).toBe(400);
    const plan = planEdit({ kind: "airs-on", channel: "Cartoon Network", show: "Videos", number: 1, on: true, slot: 0 }, ctx());
    expect(plan.config.channel[0]!.slot![0]!.items.at(-1)).toEqual({ type: "random", show: "Videos", count: 1 });
  });

  it("airs-on off removes picks and rules on that channel only", () => {
    const plan = planEdit({ kind: "airs-on", channel: "Cartoon Network", show: "Gumball", number: 1, on: false, slot: null }, ctx());
    expect(plan.config.channel[0]!.slot![0]!.items).toEqual([{ type: "ad", path: "ads/cola.mpg" }]);
    expect(plan.config.channel[1]).toEqual(config.channel[1]);
    expect(plan.removed.map((r) => r.label)).toEqual(["Random episode ×1 · Gumball", "S01E02 · Gumball"]);
  });

  it("append adds items to a loop", () => {
    const plan = planEdit({ kind: "append", number: 5, slot: null, items: [{ type: "episode", show: "Gumball", season: 2, episode: 1, from: "Cartoon Network" }] }, ctx());
    expect(plan.config.channel[1]!.loop).toHaveLength(3);
  });
});
