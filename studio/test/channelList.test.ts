import { describe, expect, it } from "vitest";
import type { Airing, Channel, Config, Issue, Job, LibShow, LineupResult } from "../src/server/types.js";
import { channelSummaries, episodeStatus, nowLine, orphanFolders, pendingEpisodes } from "../src/web/channelList.js";
import type { WebLibrary } from "../src/web/tree.js";

const cn: Channel = { number: 1, name: "Cartoon Network", category: "Kids", ads: true, mode: "looping", loop: [] };
const mtv: Channel = { number: 2, name: "MTV", category: "Music", folder: "mtv-00s", ads: true, mode: "looping", loop: [] };
const nick: Channel = { number: 3, name: "Nickelodeon", category: "Kids", ads: true, mode: "timed", slot: [] };
const config: Config = { version: 1, seed: 1, channel: [nick, mtv, cn] };

function show(channel: string, folder: string, episodes: number): LibShow {
  return {
    channel,
    folder,
    name: folder,
    icon: "",
    episode: Array.from({ length: episodes }, (_, i) => ({ season: 1, episode: i + 1, title: `E${i + 1}`, description: "", path: `${channel}/${folder}/Season 1/S01E0${i + 1} - E.mpg`, duration_ms: 60000 })),
  };
}

const library: WebLibrary = {
  version: 1,
  generated: "2026-11-20T00:00:00Z",
  ad: [],
  show: [show("Cartoon Network", "Gumball", 6), show("Cartoon Network", "Ben 10", 4), show("Cartoon Network", "Adventure Time", 2), show("mtv-00s", "Music Videos", 6), show("Old Channel", "Retro", 1)],
};

function issue(channel: number, level: Issue["level"], message: string): Issue {
  return { level, code: "x", channel, slot: 0, item: 0, line: 1, file: "config", message };
}

function airing(start: number, duration: number, extra: Partial<Airing>): Airing {
  return { start_ms: start, duration_ms: duration, kind: "episode", path: null, show: null, season: null, episode: null, title: null, slot: 0, item: 0, ...extra };
}

describe("channelSummaries", () => {
  it("summaries count shows and flag empty channels", () => {
    const summaries = channelSummaries(config, library, []);
    expect(summaries.map((s) => [s.number, s.name, s.folder, s.shows, s.episodes, s.mode])).toEqual([
      [1, "Cartoon Network", "Cartoon Network", 3, 12, "looping"],
      [2, "MTV", "mtv-00s", 1, 6, "looping"],
      [3, "Nickelodeon", "Nickelodeon", 0, 0, "timed"],
    ]);
    expect(summaries[0]!.icon).toBe("Cartoon Network/channel.png");
    expect(summaries[0]!.warnings).toEqual([]);
    expect(summaries[2]!.warnings[0]).toBe("No shows yet");
    expect(summaries[0]!.now).toBeNull();
  });

  it("summaries carry issues", () => {
    const summaries = channelSummaries(config, library, [issue(1, "warning", "slot is long"), issue(1, "error", "missing show"), issue(2, "warning", "other")]);
    expect(summaries[0]!.warnings).toEqual(["Error: missing show", "Warning: slot is long"]);
  });
});

describe("nowLine", () => {
  const lineup: LineupResult = {
    channel: 1,
    date: "2026-11-20",
    hash: "x",
    warnings: [],
    airings: [
      airing(0, 1000, { show: "Gumball", season: 3, episode: 4, title: "The Hero", path: "a.mpg" }),
      airing(1000, 500, { kind: "ad", path: "ads/a.mpg" }),
      airing(1500, 500, { show: "Music Videos", title: "Song 3", path: "b.mpg" }),
    ],
  };

  it("now line", () => {
    expect(nowLine(lineup, 500)).toBe("Gumball S03E04 · The Hero");
    expect(nowLine(lineup, 1200)).toBe("Ad break");
    expect(nowLine(lineup, 1600)).toBe("Music Videos · Song 3");
    expect(nowLine(lineup, 2500)).toBeNull();
  });
});

describe("orphanFolders", () => {
  it("orphan folders", () => {
    expect(orphanFolders(config, library)).toEqual(["Old Channel"]);
  });
});

describe("episodeStatus", () => {
  const path = "Cartoon Network/Gumball/Season 1/S01E01 - E.mpg";
  const job = (state: Job["state"], extra: Partial<Job> = {}): Job => ({ id: `${state}${extra.finishedAt ?? ""}`, source: "/x.mkv", target: path, aspect: "4:3", state, progress: 0, ...extra });

  it("episode status", () => {
    expect(episodeStatus(path, [job("queued")])).toEqual({ state: "queued" });
    expect(episodeStatus(path, [job("running", { progress: 0.4 })])).toEqual({ state: "running", progress: 0.4 });
    expect(episodeStatus(path, [])).toEqual({ state: "on-air" });
    expect(episodeStatus(path, [job("done", { finishedAt: 1 }), job("failed", { error: "boom", finishedAt: 2 })])).toEqual({ state: "failed", error: "boom" });
    expect(episodeStatus(path, [job("failed", { error: "boom", finishedAt: 1 }), job("done", { finishedAt: 2 })])).toEqual({ state: "on-air" });
    expect(episodeStatus("Other/x.mpg", [job("queued")])).toEqual({ state: "on-air" });
  });
});

describe("pendingEpisodes", () => {
  const job = (id: string, target: string, state: Job["state"], extra: Partial<Job> = {}): Job => ({ id, source: "/x.mkv", target, aspect: "4:3", state, progress: 0, ...extra });

  it("lists episodes not in the library yet", () => {
    const jobs = [
      job("a", "Cartoon Network/Gumball/Season 3/S03E20 - The Sale.mpg", "queued"),
      job("b", "Cartoon Network/Daria/Season 1/S01E01 - Esteemsters.mpg", "running", { progress: 0.5 }),
      job("c", "Cartoon Network/Gumball/Season 1/S01E01 - E.mpg", "queued"),
      job("d", "Cartoon Network/Gumball/Season 3/S03E21 - Done.mpg", "done", { finishedAt: 1 }),
      job("e", "Cartoon Network/Gumball/Season 3/S03E22 - Broke.mpg", "failed", { error: "boom", finishedAt: 2 }),
      job("f", "mtv-00s/Music Videos/New Song.mpg", "queued"),
      job("g", "Cartoon Network/ads/Cola.mpg", "queued"),
      job("h", "Cartoon Network/Shorts/Gamma.mpg", "queued"),
    ];
    expect(pendingEpisodes("Cartoon Network", library, jobs)).toEqual([
      { show: "Daria", season: 1, episode: 1, title: "Esteemsters", path: "Cartoon Network/Daria/Season 1/S01E01 - Esteemsters.mpg" },
      { show: "Gumball", season: 3, episode: 20, title: "The Sale", path: "Cartoon Network/Gumball/Season 3/S03E20 - The Sale.mpg" },
      { show: "Gumball", season: 3, episode: 22, title: "Broke", path: "Cartoon Network/Gumball/Season 3/S03E22 - Broke.mpg" },
      { show: "Shorts", season: 0, episode: null, title: "Gamma", path: "Cartoon Network/Shorts/Gamma.mpg" },
    ]);
  });
});
