import { describe, expect, it } from "vitest";
import type { ScanResult } from "../src/server/drive.js";
import type { Config, Job } from "../src/server/types.js";
import { type AddRow, commonShow, folderProblem, groupRows, guessShow, markAll, markExisting, rowsFromFiles, rowsFromScan, showNameFor, summary, videoFiles } from "../src/web/adding.js";
import type { WebLibrary } from "../src/web/tree.js";

const config: Config = {
  version: 1,
  seed: 1,
  channel: [
    { number: 2, name: "MTV", category: "Music", folder: "mtv-00s", ads: true, mode: "looping" },
    { number: 1, name: "Cartoon Network", category: "Kids", ads: true, mode: "timed" },
  ],
};

const library: WebLibrary = {
  version: 1,
  generated: "2026-11-20T00:00:00Z",
  ad: [],
  show: [
    {
      channel: "Cartoon Network",
      folder: "Gumball",
      name: "The Amazing World of Gumball",
      icon: "",
      episode: [{ season: 1, episode: 1, title: "Pilot", description: "", path: "Cartoon Network/Gumball/Season 1/S01E01 - Pilot.mpg", duration_ms: 60000 }],
    },
    {
      channel: "Cartoon Network",
      folder: "Shorts",
      name: "Shorts",
      icon: "",
      episode: [
        { season: 0, episode: 1, title: "Alpha", description: "", path: "Cartoon Network/Shorts/Alpha.mpg", duration_ms: 60000 },
        { season: 0, episode: 2, title: "Beta", description: "", path: "Cartoon Network/Shorts/Beta.mpg", duration_ms: 60000 },
      ],
    },
  ],
};

function file(name: string, size = 1): File {
  return new File([new Uint8Array(size)], name);
}

function scanned(source: string, extra: Partial<ScanResult> = {}): ScanResult {
  return { source, guess: { season: null, episode: null, title: "" }, durationMs: 60000, aspect: "4:3", tooShort: false, ...extra };
}

function row(name: string, extra: Partial<AddRow> = {}): AddRow {
  return { ...rowsFromFiles([file(name)], [])[0]!, ...extra };
}

describe("guessShow", () => {
  it("guesses the show", () => {
    expect(guessShow("Gumball.S03E19.The.Pest.mkv")).toBe("Gumball");
    expect(guessShow("The Amazing World of Gumball - 3x19 - The Pest.mp4")).toBe("The Amazing World of Gumball");
    expect(guessShow("Pilot.mp4")).toBe("");
    expect(guessShow("S01E01 First.mp4")).toBe("");
  });
});

describe("rows", () => {
  it("files already added start unticked", () => {
    const rows = rowsFromFiles([file("a.mp4"), file("Gumball.S03E19.The.Pest.mkv", 7)], ["a.mp4"]);
    expect(rows.map((r) => [r.name, r.ticked, r.note])).toEqual([
      ["a.mp4", false, "added"],
      ["Gumball.S03E19.The.Pest.mkv", true, null],
    ]);
    expect(rows[1]).toMatchObject({ season: 3, episode: 19, title: "The Pest", show: "Gumball", size: 7, durationMs: null, source: null });
    expect(rows[0]!.id).not.toBe(rows[1]!.id);
  });

  it("scan problems start unticked", () => {
    const rows = rowsFromScan([
      scanned("/imports/a/short.mp4", { tooShort: true }),
      scanned("/imports/a/broken.mp4", { error: "no video stream" }),
      scanned("/imports/a/old.mp4", { added: true }),
      scanned("/imports/a/Gumball S01E02 Two.mp4", { guess: { season: 1, episode: 2, title: "Two" }, aspect: "16:9" }),
    ]);
    expect(rows.map((r) => [r.name, r.ticked, r.note])).toEqual([
      ["short.mp4", false, "too-short"],
      ["broken.mp4", false, "unreadable"],
      ["old.mp4", false, "added"],
      ["Gumball S01E02 Two.mp4", true, null],
    ]);
    expect(rows[3]).toMatchObject({ season: 1, episode: 2, title: "Two", show: "Gumball", aspect: "16:9", durationMs: 60000, source: "/imports/a/Gumball S01E02 Two.mp4", file: null });
  });
});

describe("groupRows", () => {
  it("matches an existing show", () => {
    const groups = groupRows([row("Gumball.S03E19.The.Pest.mkv")], "", library, config, null);
    expect(groups).toHaveLength(1);
    expect(groups[0]!.dest).toEqual({ channel: "Cartoon Network", show: "Gumball", isNewShow: false });
  });

  it("matches a show by its display name", () => {
    const groups = groupRows([row("The Amazing World of Gumball - 3x19 - The Pest.mp4")], "", library, config, 2);
    expect(groups[0]!.dest).toEqual({ channel: "Cartoon Network", show: "Gumball", isNewShow: false });
  });

  it("new show goes to the preferred channel", () => {
    const groups = groupRows([row("Daria.S01E01.Esteemsters.mkv")], "", library, config, 2);
    expect(groups[0]!.dest).toEqual({ channel: "mtv-00s", show: "Daria", isNewShow: true });
  });

  it("new show without a preferred channel goes to the first channel", () => {
    const groups = groupRows([row("Daria.S01E01.Esteemsters.mkv")], "", library, config, null);
    expect(groups[0]!.dest.channel).toBe("Cartoon Network");
  });

  it("splits two shows", () => {
    const rows = [row("Gumball.S03E19.The.Pest.mkv"), row("Ben.10.S01E01.And.Then.mkv"), row("Pilot.mp4"), row("Gumball.S03E20.The.Sale.mkv")];
    const groups = groupRows(rows, "Gumball", library, config, null);
    expect(groups.map((g) => [g.show, g.rows.map((r) => r.name)])).toEqual([
      ["Gumball", ["Gumball.S03E19.The.Pest.mkv", "Pilot.mp4", "Gumball.S03E20.The.Sale.mkv"]],
      ["Ben 10", ["Ben.10.S01E01.And.Then.mkv"]],
    ]);
    expect(groups[0]!.rows.every((r) => r.show === "Gumball")).toBe(true);
  });

  it("finds the common show", () => {
    expect(commonShow([row("Gumball.S03E19.x.mkv"), row("Ben.10.S01E01.x.mkv"), row("Gumball.S03E20.x.mkv"), row("Pilot.mp4")])).toBe("Gumball");
    expect(commonShow([row("Pilot.mp4")])).toBe("");
  });
});

describe("markExisting", () => {
  const gumball = (rows: AddRow[]) => groupRows(rows, "", library, config, null)[0]!;

  it("on-air episode offers replace", () => {
    const group = markExisting(gumball([row("Gumball.S01E01.Pilot.mkv")]), library, []);
    expect(group.rows[0]).toMatchObject({ ticked: false, note: "on-air", replaces: "Cartoon Network/Gumball/Season 1/S01E01 - Pilot.mpg" });
  });

  it("queued episode counts as existing", () => {
    const job: Job = { id: "j1", source: "/x.mkv", target: "Cartoon Network/Gumball/Season 3/S03E20 - The Sale.mpg", aspect: "4:3", state: "queued", progress: 0 };
    const done: Job = { ...job, id: "j2", target: "Cartoon Network/Gumball/Season 3/S03E21 - The Done.mpg", state: "done" };
    const group = markExisting(gumball([row("Gumball.S03E20.The.Sale.mkv"), row("Gumball.S03E21.The.Done.mkv")]), library, [job, done]);
    expect(group.rows.map((r) => [r.ticked, r.note, r.replaces])).toEqual([
      [false, "on-air", null],
      [true, null, null],
    ]);
  });

  it("second copy in the batch is unticked", () => {
    const group = markExisting(gumball([row("Gumball.S03E19.The.Pest.mkv"), row("Gumball - 3x19 - The Pest (copy).mkv")]), library, []);
    expect(group.rows.map((r) => [r.ticked, r.note])).toEqual([
      [true, null],
      [false, "in-batch"],
    ]);
  });

  it("keeps notes it did not set", () => {
    const group = markExisting(gumball([row("Gumball.S01E01.Pilot.mkv", { ticked: false, note: "added" })]), library, []);
    expect(group.rows[0]).toMatchObject({ note: "added", replaces: null });
  });
});

describe("summary", () => {
  it("summary counts ticked bytes", () => {
    const rows = [row("a.mp4", { size: 1_000_000 }), row("b.mp4", { size: 2_000_000 }), row("c.mp4", { size: 4_000_000, ticked: false })];
    expect(summary(rows)).toEqual({ ticked: 2, total: 3, bytes: 3_000_000 });
  });
});

describe("ticks the owner chose", () => {
  const gumball = (rows: AddRow[]) => groupRows(rows, "", library, config, null)[0]!;
  const tick = (group: ReturnType<typeof gumball>, i: number, ticked: boolean) => ({ ...group, rows: group.rows.map((r, k) => (k === i ? { ...r, ticked, held: false } : r)) });
  const episode = (group: ReturnType<typeof gumball>, i: number, n: number) => ({ ...group, rows: group.rows.map((r, k) => (k === i ? { ...r, episode: n } : r)) });

  it("a ticked Replace survives another row's edit", () => {
    let group = markExisting(gumball([row("Gumball.S01E01.Pilot.mkv"), row("Gumball.S03E19.The.Pest.mkv")]), library, []);
    group = markExisting(tick(group, 0, true), library, []);
    group = markExisting(episode(group, 1, 20), library, []);
    expect(group.rows[0]).toMatchObject({ ticked: true, note: "on-air", replaces: "Cartoon Network/Gumball/Season 1/S01E01 - Pilot.mpg" });
  });

  it("a row the owner unticked stays unticked through a clash", () => {
    let group = markExisting(gumball([row("Gumball.S01E05.Five.mkv"), row("Gumball.S01E12.Twelve.mkv")]), library, []);
    group = markExisting(tick(group, 1, false), library, []);
    group = markExisting(episode(group, 1, 5), library, []);
    expect(group.rows[1]!.note).toBe("in-batch");
    group = markExisting(episode(group, 1, 13), library, []);
    expect(group.rows[1]).toMatchObject({ ticked: false, note: null });
  });

  it("a row unticked for a clash is ticked again once the clash goes", () => {
    let group = markExisting(gumball([row("Gumball.S01E05.Five.mkv"), row("Gumball.S01E05.Copy.mkv")]), library, []);
    expect(group.rows[1]).toMatchObject({ ticked: false, note: "in-batch" });
    group = markExisting(episode(group, 1, 6), library, []);
    expect(group.rows[1]).toMatchObject({ ticked: true, note: null });
  });

  it("unticking the first copy lets the second through", () => {
    let group = markExisting(gumball([row("Gumball.S01E05.Five.mkv"), row("Gumball.S01E05.Copy.mkv")]), library, []);
    group = markExisting(tick(group, 0, false), library, []);
    expect(group.rows.map((r) => [r.ticked, r.note])).toEqual([
      [false, null],
      [true, null],
    ]);
  });

  it("a queued Replace is not offered again", () => {
    const job = { id: "j1", source: "/x.mkv", target: "Cartoon Network/Gumball/Season 1/S01E01 - Pilot.mpg", aspect: "4:3" as const, state: "queued" as const, progress: 0 };
    const group = markExisting(gumball([row("Gumball.S01E01.Pilot.mkv")]), library, [job]);
    expect(group.rows[0]).toMatchObject({ ticked: false, note: "on-air", replaces: null });
  });
});

describe("season-less shows", () => {
  it("are not matched by episode position", () => {
    const groups = groupRows([row("Shorts - Episode 2 - Gamma.mp4")], "Shorts", library, config, null);
    const group = markExisting({ ...groups[0]!, rows: groups[0]!.rows.map((r) => ({ ...r, season: null })) }, library, []);
    expect(group.rows[0]).toMatchObject({ ticked: true, note: null, replaces: null });
  });
});

describe("markAll", () => {
  it("finds the same episode across groups going to one show", () => {
    const groups = groupRows([row("Gumball.S03E19.The.Pest.mkv"), row("The Amazing World of Gumball - 3x19 - The Pest.mp4")], "", library, config, null);
    expect(groups).toHaveLength(2);
    const marked = markAll(groups, library, []);
    expect(marked.map((g) => g.rows[0]!.note)).toEqual([null, "in-batch"]);
    expect(marked[1]!.rows[0]!.ticked).toBe(false);
  });
});

describe("videoFiles", () => {
  it("keeps only videos", () => {
    const picked = [file("Gumball.S03E19.The.Pest.mkv"), file("Gumball.S03E19.The.Pest.en.srt"), file("poster.jpg"), file("._Gumball.mkv")];
    const { videos, skipped } = videoFiles(picked);
    expect(videos.map((f) => f.name)).toEqual(["Gumball.S03E19.The.Pest.mkv"]);
    expect(skipped).toBe(3);
  });
});

describe("same file name", () => {
  it("a second file with the same stored name starts unticked", () => {
    const rows = rowsFromFiles([file("Episode 1.mp4", 1), file("Episode 1.mp4", 2), file("a:b.mp4"), file("a-b.mp4")], []);
    expect(rows.map((r) => [r.ticked, r.note])).toEqual([
      [true, null],
      [false, "same-name"],
      [true, null],
      [false, "same-name"],
    ]);
  });
});

describe("folderProblem", () => {
  it("names the server would refuse", () => {
    expect(folderProblem("Gumball")).toBeNull();
    expect(folderProblem("AC/DC")).toBe("A name can't contain / or \\.");
    expect(folderProblem("Ads")).toBe("“Ads” is kept for ads. Pick another name.");
    expect(folderProblem(".hidden")).toBe("A name can't start with a dot.");
    expect(folderProblem("  ")).toBe("Give it a name.");
    expect(folderProblem("..")).toBe("A name can't start with a dot.");
  });
});

describe("showNameFor", () => {
  it("only names new shows", () => {
    expect(showNameFor({ channel: "Cartoon Network", show: "Gumball", isNewShow: false }, library)).toBe("");
    expect(showNameFor({ channel: "Cartoon Network", show: "gumball", isNewShow: true }, library)).toBe("");
    expect(showNameFor({ channel: "Cartoon Network", show: " Daria ", isNewShow: true }, library)).toBe("Daria");
  });
});
