import { describe, expect, it } from "vitest";
import { HISTORY_MS } from "../src/server/history.js";
import type { Job } from "../src/server/types.js";
import { describeTarget, homeRows, type UploadTask } from "../src/web/episodes.js";

const NOW = Date.UTC(2026, 10, 20, 12);
let next = 0;

function job(target: string, state: Job["state"], extra: Partial<Job> = {}): Job {
  next += 1;
  return { id: `j${next}`, source: `/imports/${next}.mkv`, target, aspect: "4:3", state, progress: state === "done" ? 1 : 0, ...extra };
}

const gumball = (e: number) => `Cartoon Network/Gumball/Season 3/S03E${String(e).padStart(2, "0")} - Ep ${e}.mpg`;

describe("describeTarget", () => {
  it("reads episode targets", () => {
    expect(describeTarget("Cartoon Network/Gumball/Season 3/S03E19 - The Pest.mpg")).toEqual({ channel: "Cartoon Network", show: "Gumball", season: 3, episode: 19, ad: false, name: "S03E19 - The Pest" });
    expect(describeTarget("ads/a.mpg")).toEqual({ channel: "", show: null, season: null, episode: null, ad: true, name: "a" });
    expect(describeTarget("MTV/ads/a.mpg")).toEqual({ channel: "MTV", show: null, season: null, episode: null, ad: true, name: "a" });
    expect(describeTarget("mtv-00s/Music Videos/Song.mpg")).toEqual({ channel: "mtv-00s", show: "Music Videos", season: null, episode: null, ad: false, name: "Song" });
  });
});

describe("homeRows", () => {
  it("collapses queued runs", () => {
    const jobs = Array.from({ length: 21 }, (_, i) => job(gumball(40 - i), "queued"));
    const { active, recent } = homeRows(jobs, [], NOW);
    expect(recent).toEqual([]);
    expect(active).toHaveLength(1);
    expect(active[0]).toMatchObject({ state: "queued", title: "Gumball S03E20–E40", count: 21, detail: "21 waiting", progress: null });
  });

  it("running rows stand alone", () => {
    const jobs = [job(gumball(19), "running", { progress: 0.62 }), job(gumball(20), "queued")];
    const { active } = homeRows(jobs, [], NOW);
    expect(active.map((r) => [r.state, r.title, r.detail, r.progress])).toEqual([
      ["running", "Gumball S03E19", "62%", 0.62],
      ["queued", "Gumball S03E20", "1 waiting", null],
    ]);
    expect(active.map((r) => r.channel)).toEqual(["Cartoon Network", "Cartoon Network"]);
    expect(active.map((r) => r.target)).toEqual([gumball(19), gumball(20)]);
  });

  it("recent keeps seven days", () => {
    const jobs = [job(gumball(1), "done", { finishedAt: NOW - HISTORY_MS - 1 }), job(gumball(2), "done", { finishedAt: NOW - HISTORY_MS })];
    const { recent } = homeRows(jobs, [], NOW);
    expect(recent.map((r) => [r.title, r.detail])).toEqual([["Gumball S03E02", "1 on air"]]);
  });

  it("collapses done runs and sorts recent newest first", () => {
    const jobs = [
      ...Array.from({ length: 18 }, (_, i) => job(gumball(i + 1), "done", { finishedAt: NOW - 5000 + i })),
      job("Cartoon Network/Adventure Time/Season 2/S02E07 - Seven.mpg", "done", { finishedAt: NOW - 1000 }),
    ];
    const { recent } = homeRows(jobs, [], NOW);
    expect(recent.map((r) => [r.state, r.title, r.count, r.detail, r.finishedAt])).toEqual([
      ["done", "Adventure Time S02E07", 1, "1 on air", NOW - 1000],
      ["done", "Gumball S03E01–E18", 18, "18 on air", NOW - 5000 + 17],
    ]);
    expect(recent.map((r) => r.target)).toEqual(["Cartoon Network/Adventure Time/Season 2/S02E07 - Seven.mpg", null]);
  });

  it("a gap in the episodes starts a new row", () => {
    const jobs = [job(gumball(19), "done", { finishedAt: NOW - 2 }), job(gumball(21), "done", { finishedAt: NOW - 1 })];
    const { recent } = homeRows(jobs, [], NOW);
    expect(recent.map((r) => [r.title, r.count])).toEqual([
      ["Gumball S03E21", 1],
      ["Gumball S03E19", 1],
    ]);
  });

  it("failed rows never collapse", () => {
    const jobs = [job(gumball(1), "failed", { error: "boom 1", finishedAt: NOW - 2 }), job(gumball(2), "failed", { error: "boom 2", finishedAt: NOW - 1 })];
    const { recent } = homeRows(jobs, [], NOW);
    expect(recent.map((r) => [r.state, r.title, r.error, r.count])).toEqual([
      ["failed", "Gumball S03E02", "boom 2", 1],
      ["failed", "Gumball S03E01", "boom 1", 1],
    ]);
  });

  it("missing source", () => {
    const { recent } = homeRows([job(gumball(1), "failed", { error: "boom", missing: true, finishedAt: NOW })], [], NOW);
    expect(recent[0]).toMatchObject({ state: "missing", detail: "Add the file again", error: "boom" });
  });

  it("a failed row is dropped once the same episode converts again", () => {
    const jobs = [job(gumball(1), "failed", { error: "boom", finishedAt: NOW - 10 }), job(gumball(1), "done", { finishedAt: NOW })];
    const { recent } = homeRows(jobs, [], NOW);
    expect(recent.map((r) => r.state)).toEqual(["done"]);
  });

  it("shrink jobs say so", () => {
    const { active } = homeRows([job(gumball(1), "running", { kind: "shrink", progress: 0.5 })], [], NOW);
    expect(active[0]).toMatchObject({ shrink: true, detail: "Making smaller", progress: 0.5 });
  });

  it("names ads and videos", () => {
    const jobs = [job("ads/Cola.mpg", "queued"), job("mtv-00s/Music Videos/Artist 1 - Song 1.mpg", "queued")];
    expect(homeRows(jobs, [], NOW).active.map((r) => r.title)).toEqual(["Ad · Cola", "Music Videos · Artist 1 - Song 1"]);
  });

  it("uploads come first", () => {
    const uploads: UploadTask[] = [
      { id: "u1", name: "Gumball.S03E21.mkv", size: 200, loaded: 100, state: "uploading", reason: null },
      { id: "u2", name: "bad.mkv", size: 10, loaded: 10, state: "rejected", reason: "too short" },
      { id: "u3", name: "ok.mkv", size: 10, loaded: 10, state: "done", reason: null },
    ];
    const { active, recent } = homeRows([job(gumball(19), "running", { progress: 0.1 })], uploads, NOW);
    expect(active[0]).toMatchObject({ key: "upload:u1", state: "uploading", title: "Gumball.S03E21.mkv", progress: 0.5 });
    expect(active[1]!.state).toBe("running");
    expect(active).toHaveLength(2);
    expect(recent[0]).toMatchObject({ key: "upload:u2", state: "rejected", title: "bad.mkv", error: "too short" });
  });
});
