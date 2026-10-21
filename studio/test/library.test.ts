import { execFile } from "node:child_process";
import { mkdir, mkdtemp, readFile, rm, symlink, writeFile } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { promisify } from "node:util";
import { parse } from "smol-toml";
import { afterEach, beforeEach, expect, test, vi } from "vitest";
import type { probe } from "../src/server/ffmpeg.js";
import { buildLibrary, serializeLibrary, writeLibrary } from "../src/server/library.js";
import { readShowMeta, writeShowMeta } from "../src/server/showMeta.js";
import type { Library, SourceRecord } from "../src/server/types.js";

const run = promisify(execFile);
const SCHED = path.resolve(import.meta.dirname, "../../engine/build/cable-twii-sched");

let root: string;

beforeEach(async () => {
  root = await mkdtemp(path.join(os.tmpdir(), "studio-library-"));
});

afterEach(async () => {
  await rm(root, { recursive: true, force: true });
});

async function put(rel: string, content = "x"): Promise<void> {
  const file = path.join(root, ...rel.split("/"));
  await mkdir(path.dirname(file), { recursive: true });
  await writeFile(file, content);
}

const fakeProbe = (durationMs = 5000): typeof probe =>
  vi.fn(async () => ({ durationMs, width: 640, height: 480, displayAspect: 4 / 3, videoStreamIndex: 0 })) as unknown as typeof probe;

async function sampleTree(): Promise<void> {
  for (let n = 1; n <= 3; n++) {
    await put(`Cartoon Network/Gumball/Season 1/S01E0${n} - Ep ${n}.mpg`);
    await put(`Cartoon Network/Gumball/Season 2/S02E0${n} - Two ${n}.mpg`);
  }
  await put("Cartoon Network/Gumball/show.png");
  await put("Cartoon Network/ads/own.mpg");
  await put("mtv-00s/Music Videos/Artist 1 - Song 1.mpg");
  await put("mtv-00s/Music Videos/Artist 2 - Song 2.mpg");
  await put("mtv-00s/Music Videos/Artist 3 - Song 3.mpg");
  await put("mtv-00s/Other/E05 - Solo.mpg");
  await put("mtv-00s/Third/Season 1/S01E01.mpg");
  await put("ads/a.mpg");
  await put("ads/b.mpg");
  await put("ads/c.mpg");
  await put(".studio/sources.toml", "version = 1\n");
}

test("indexes shows, seasons, ads", async () => {
  await sampleTree();
  const { library, skipped, warnings } = await buildLibrary(root, [], fakeProbe());
  expect(skipped).toEqual([]);
  expect(warnings).toEqual([]);
  expect(library.version).toBe(1);
  expect(library.show.map((s) => `${s.channel}/${s.folder}`)).toEqual([
    "Cartoon Network/Gumball",
    "mtv-00s/Music Videos",
    "mtv-00s/Other",
    "mtv-00s/Third",
  ]);
  const gumball = library.show[0]!;
  expect(gumball.icon).toBe("Cartoon Network/Gumball/show.png");
  expect(gumball.episode.map((e) => [e.season, e.episode])).toEqual([[1, 1], [1, 2], [1, 3], [2, 1], [2, 2], [2, 3]]);
  expect(gumball.episode[0]).toEqual({
    season: 1,
    episode: 1,
    title: "Ep 1",
    description: "",
    path: "Cartoon Network/Gumball/Season 1/S01E01 - Ep 1.mpg",
    duration_ms: 5000,
  });
  expect(library.show[1]!.icon).toBe("");
  expect(library.show[2]!.episode).toEqual([
    { season: 0, episode: 1, title: "Solo", description: "", path: "mtv-00s/Other/E05 - Solo.mpg", duration_ms: 5000 },
  ]);
  expect(library.show[3]!.episode[0]!.title).toBe("");
  expect(library.ad.map((a) => a.path)).toEqual(["Cartoon Network/ads/own.mpg", "ads/a.mpg", "ads/b.mpg", "ads/c.mpg"]);
});

test("seasonless show numbering", async () => {
  await sampleTree();
  const { library } = await buildLibrary(root, [], fakeProbe());
  const music = library.show.find((s) => s.folder === "Music Videos")!;
  expect(music.episode.map((e) => [e.season, e.episode, e.title])).toEqual([
    [0, 1, "Artist 1 - Song 1"],
    [0, 2, "Artist 2 - Song 2"],
    [0, 3, "Artist 3 - Song 3"],
  ]);
});

test("natural order seasonless (Song 2 before Song 10)", async () => {
  await put("ch/Show/Song 10.mpg");
  await put("ch/Show/Song 2.mpg");
  await put("ch/Show/Song 1.mpg");
  const { library } = await buildLibrary(root, [], fakeProbe());
  expect(library.show[0]!.episode.map((e) => e.title)).toEqual(["Song 1", "Song 2", "Song 10"]);
});

test("descriptions from show.toml", async () => {
  await sampleTree();
  await writeShowMeta(path.join(root, "Cartoon Network", "Gumball"), { episodes: { S01E01: "The first one." } });
  await writeShowMeta(path.join(root, "mtv-00s", "Music Videos"), { episodes: { "Artist 2 - Song 2": "The cat." } });
  const { library } = await buildLibrary(root, [], fakeProbe());
  expect(library.show[0]!.episode[0]!.description).toBe("The first one.");
  expect(library.show[0]!.episode[1]!.description).toBe("");
  expect(library.show[1]!.episode[1]!.description).toBe("The cat.");
});

test("show name override", async () => {
  await sampleTree();
  await writeShowMeta(path.join(root, "Cartoon Network", "Gumball"), { name: "The Amazing World of Gumball", episodes: {} });
  const { library } = await buildLibrary(root, [], fakeProbe());
  expect(library.show[0]!.name).toBe("The Amazing World of Gumball");
  expect(library.show[0]!.folder).toBe("Gumball");
  expect(library.show[1]!.name).toBe("Music Videos");
});

test("show.toml name override and descriptions", async () => {
  await put("ch/Show/Season 1/S01E01 - A.mpg");
  const dir = path.join(root, "ch", "Show");
  await writeShowMeta(dir, { name: "Nice Name", episodes: { S01E01: "Desc" } });
  const meta = await readShowMeta(dir);
  expect(meta).toEqual({ name: "Nice Name", episodes: { S01E01: "Desc" } });
  const text = await readFile(path.join(dir, "show.toml"), "utf8");
  expect(parse(text)).toEqual({ name: "Nice Name", episodes: { S01E01: "Desc" } });
  const { library } = await buildLibrary(root, [], fakeProbe());
  expect(library.show[0]!.name).toBe("Nice Name");
  expect(library.show[0]!.episode[0]!.description).toBe("Desc");
});

test("readShowMeta missing and damaged", async () => {
  const dir = path.join(root, "ch", "Show");
  await mkdir(dir, { recursive: true });
  expect(await readShowMeta(dir)).toEqual({ episodes: {} });
  await writeFile(path.join(dir, "show.toml"), "name = [[[");
  await expect(readShowMeta(dir)).rejects.toThrow();
});

test("skips part and dot files", async () => {
  await put("ch/Show/Season 1/S01E01 - A.mpg");
  await put("ch/Show/Season 1/S01E02 - B.mpg.part");
  await put("ch/Show/Season 1/._S01E01 - A.mpg");
  await put("ch/Show/Season 1/.hidden.mpg");
  await put("ch/Show/Season 1/.DS_Store");
  await put("ch/Show/Season 1/notes.txt");
  await put("ch/ads/._x.mpg");
  await put("ch/ads/x.mpg.part");
  const { library, skipped } = await buildLibrary(root, [], fakeProbe());
  expect(library.show[0]!.episode.map((e) => e.path)).toEqual(["ch/Show/Season 1/S01E01 - A.mpg"]);
  expect(library.ad).toEqual([]);
  expect(skipped).toEqual([]);
});

test("durations prefer sources", async () => {
  await put("ch/Show/Season 1/S01E01 - A.mpg");
  await put("ch/Show/Season 1/S01E02 - B.mpg");
  const sources: SourceRecord[] = [
    { target: "ch/Show/Season 1/S01E01 - A.mpg", source: "/m/a.mkv", size: 1, mtimeMs: 1, durationMs: 123456 },
  ];
  const fake = fakeProbe(7000);
  const { library } = await buildLibrary(root, sources, fake);
  expect(library.show[0]!.episode.map((e) => e.duration_ms)).toEqual([123456, 7000]);
  expect(fake).toHaveBeenCalledTimes(1);
});

test("short file skipped", async () => {
  await put("ch/Show/Season 1/S01E01 - A.mpg");
  await put("ch/Show/Season 1/S01E02 - B.mpg");
  await put("ads/short.mpg");
  const sources: SourceRecord[] = [
    { target: "ch/Show/Season 1/S01E02 - B.mpg", source: "/m/b", size: 1, mtimeMs: 1, durationMs: 999 },
    { target: "ads/short.mpg", source: "/m/s", size: 1, mtimeMs: 1, durationMs: 500 },
  ];
  const { library, skipped } = await buildLibrary(root, sources, fakeProbe(5000));
  expect(library.show[0]!.episode.map((e) => e.episode)).toEqual([1]);
  expect(library.ad).toEqual([]);
  expect(skipped.map((s) => s.path).sort()).toEqual(["ads/short.mpg", "ch/Show/Season 1/S01E02 - B.mpg"]);
  expect(skipped.every((s) => s.reason.length > 0)).toBe(true);
});

test("probe failure skipped", async () => {
  await put("ch/Show/Season 1/S01E01 - A.mpg");
  await put("ch/Show/Season 1/S01E02 - B.mpg");
  const failing = vi.fn(async (file: string) => {
    if (file.includes("S01E02")) throw new Error("broken file");
    return { durationMs: 5000, width: 1, height: 1, displayAspect: 1, videoStreamIndex: 0 };
  }) as unknown as typeof probe;
  const { library, skipped } = await buildLibrary(root, [], failing);
  expect(library.show[0]!.episode.map((e) => e.episode)).toEqual([1]);
  expect(skipped).toEqual([{ path: "ch/Show/Season 1/S01E02 - B.mpg", reason: expect.stringContaining("broken file") }]);
});

test("episode without number in season folder gets next number with warning", async () => {
  await put("ch/Show/Season 1/S01E01 - A.mpg");
  await put("ch/Show/Season 1/S01E04 - D.mpg");
  await put("ch/Show/Season 1/Bonus.mpg");
  await put("ch/Show/Season 1/Extra.mpg");
  const { library, warnings } = await buildLibrary(root, [], fakeProbe());
  const eps = library.show[0]!.episode;
  expect(eps.map((e) => [e.episode, e.title])).toEqual([[1, "A"], [4, "D"], [5, "Bonus"], [6, "Extra"]]);
  expect(warnings).toHaveLength(2);
  expect(warnings[0]).toContain("Bonus.mpg");
});

test("symlinked show folder ignored", async () => {
  await put("ch/Real/Season 1/S01E01 - A.mpg");
  await put("elsewhere/Season 1/S01E01 - B.mpg");
  await symlink(path.join(root, "elsewhere"), path.join(root, "ch", "Linked"));
  await symlink(path.join(root, "ch", "Real", "Season 1", "S01E01 - A.mpg"), path.join(root, "ch", "Real", "Season 1", "S01E02 - L.mpg"));
  const { library } = await buildLibrary(root, [], fakeProbe());
  const keys = library.show.map((s) => `${s.channel}/${s.folder}`);
  expect(keys).toContain("ch/Real");
  expect(keys).not.toContain("ch/Linked");
  const real = library.show.find((s) => s.folder === "Real")!;
  expect(real.episode).toHaveLength(1);
});

test("channel ads vs shared ads", async () => {
  await put("ads/shared.mpg");
  await put("one/ads/own.mpg");
  await put("one/Show/Season 1/S01E01.mpg");
  await put("two/ads/other.mpg");
  const { library } = await buildLibrary(root, [], fakeProbe());
  expect(library.ad).toEqual([
    { path: "ads/shared.mpg", channel: "", duration_ms: 5000 },
    { path: "one/ads/own.mpg", channel: "one", duration_ms: 5000 },
    { path: "two/ads/other.mpg", channel: "two", duration_ms: 5000 },
  ]);
  expect(library.show.map((s) => s.folder)).toEqual(["Show"]);
});

test("unicode folder names", async () => {
  await put("Канал/Шоу/Season 1/S01E01 - Привет.mpg");
  await put("Канал/Шоу/show.png");
  const sources: SourceRecord[] = [
    { target: "Канал/Шоу/Season 1/S01E01 - Привет.mpg".normalize("NFC"), source: "/m/a", size: 1, mtimeMs: 1, durationMs: 4242 },
  ];
  const fake = fakeProbe();
  const { library } = await buildLibrary(root, sources, fake);
  expect(library.show[0]!.channel).toBe("Канал");
  expect(library.show[0]!.name).toBe("Шоу");
  expect(library.show[0]!.icon).toBe("Канал/Шоу/show.png");
  expect(library.show[0]!.episode[0]!.title).toBe("Привет");
  expect(library.show[0]!.episode[0]!.duration_ms).toBe(4242);
  const text = serializeLibrary(library);
  expect((parse(text).show as { channel: string }[])[0]!.channel).toBe("Канал");
});

test("serializeLibrary emits the frozen shape", () => {
  const lib: Library = {
    version: 1,
    generated: new Date(2026, 9, 20, 14, 0, 0),
    ad: [{ path: "ads/soda.mpg", channel: "", duration_ms: 30030 }],
    show: [
      {
        channel: "Cartoon \"Net\"",
        folder: "Gumball",
        name: "The Gumball\\Show",
        icon: "",
        episode: [{ season: 1, episode: 1, title: "The DVD", description: "Line\nbreak", path: "a/b.mpg", duration_ms: 665120 }],
      },
    ],
  };
  const text = serializeLibrary(lib);
  expect(text.startsWith("version = 1\ngenerated = 2026-10-20T14:00:00\n")).toBe(true);
  expect(text).toContain("[[ad]]");
  expect(text).toContain("[[show.episode]]");
  expect(text).toContain("duration_ms = 665120");
  expect(text).not.toMatch(/duration_ms = \d+\.\d/);
  expect(text).not.toMatch(/(season|episode) = \d+\.\d/);
  expect(/ +\n/.test(text)).toBe(false);
  const doc = parse(text) as { show: { channel: string; name: string; episode: { description: string }[] }[] };
  expect(doc.show[0]!.channel).toBe("Cartoon \"Net\"");
  expect(doc.show[0]!.name).toBe("The Gumball\\Show");
  expect(doc.show[0]!.episode[0]!.description).toBe("Line\nbreak");
});

test("duplicate episode in one folder keeps first", async () => {
  await put("ch/Show/Season 1/S01E01 - A.mpg");
  await put("ch/Show/Season 1/S01E01 - B.mpg");
  await put("ch/Show/Season 1/S01E02 - C.mpg");
  const { library, skipped } = await buildLibrary(root, [], fakeProbe());
  expect(library.show[0]!.episode.map((e) => e.title)).toEqual(["A", "C"]);
  expect(skipped).toEqual([
    { path: "ch/Show/Season 1/S01E01 - B.mpg", reason: "duplicate S01E01 (kept ch/Show/Season 1/S01E01 - A.mpg)" },
  ]);
});

test("duplicate episode across Season 1 and season 01", async () => {
  await put("ch/Show/Season 1/S01E01 - A.mpg");
  await put("ch/Show/season 01/S01E01 - B.mpg");
  await put("ch/Show/season 01/S01E02 - C.mpg");
  const { library, skipped } = await buildLibrary(root, [], fakeProbe());
  const eps = library.show[0]!.episode;
  expect(eps.map((e) => [e.season, e.episode])).toEqual([[1, 1], [1, 2]]);
  expect(skipped).toHaveLength(1);
  expect(skipped[0]!.reason).toMatch(/^duplicate S01E01 \(kept ch\/Show\/(Season 1|season 01)\/S01E01 - [AB]\.mpg\)$/);
});

test("Season 0 folder files skipped with reason", async () => {
  await put("ch/Show/Season 1/S01E01 - A.mpg");
  await put("ch/Show/Season 0/Special.mpg");
  const { library, skipped } = await buildLibrary(root, [], fakeProbe());
  expect(library.show[0]!.episode.map((e) => e.path)).toEqual(["ch/Show/Season 1/S01E01 - A.mpg"]);
  expect(skipped).toEqual([
    {
      path: "ch/Show/Season 0/Special.mpg",
      reason: "Season 0 folder is not supported; put these files directly in the show folder",
    },
  ]);
});

test("serializeLibrary escapes control characters and emoji", async () => {
  await put("ch/Show/Season 1/S01E01 - A.mpg");
  const { library } = await buildLibrary(root, [], fakeProbe(60000));
  library.show[0]!.episode[0]!.title = "Tab\there \u0001 \u{1F600} \ud800 end";
  library.show[0]!.name = "Del\u007f";
  const text = serializeLibrary(library);
  expect(text).toContain("\\u0009");
  expect(text).toContain("\\u0001");
  expect(text).toContain("\\uFFFD");
  expect(text).toContain("\u{1F600}");
  const doc = parse(text) as { show: { name: string; episode: { title: string }[] }[] };
  expect(doc.show[0]!.episode[0]!.title).toBe("Tab\there \u0001 \u{1F600} \uFFFD end");
  expect(doc.show[0]!.name).toBe("Del\u007f");
  await writeLibrary(root, library);
  await writeFile(
    path.join(root, "config.toml"),
    'version = 1\nseed = 1\n\n[[channel]]\nnumber = 1\nname = "C"\ncategory = "K"\nfolder = "ch"\nads = false\nmode = "looping"\nloop = [ { type = "random-run", show = "Show", count = 1 } ]\n',
  );
  const validated = await run(SCHED, ["validate", root]);
  expect((JSON.parse(validated.stdout) as { ok: boolean }).ok).toBe(true);
});

test("library round trip through engine", async () => {
  await sampleTree();
  await writeShowMeta(path.join(root, "Cartoon Network", "Gumball"), { name: "The Amazing World of Gumball", episodes: { S01E01: "Desc" } });
  const { library } = await buildLibrary(root, [], fakeProbe(60000));
  await writeLibrary(root, library);
  await writeFile(
    path.join(root, "config.toml"),
    [
      "version = 1",
      "seed = 1",
      "",
      "[[channel]]",
      "number = 1",
      'name = "Cartoon Network"',
      'category = "Kids"',
      'folder = "Cartoon Network"',
      "ads = false",
      'mode = "looping"',
      'loop = [ { type = "random-run", show = "Gumball", count = 2 } ]',
      "",
    ].join("\n"),
  );
  if (!(await run("test", ["-x", SCHED]).then(() => true, () => false))) {
    await run("make", ["-C", path.resolve(import.meta.dirname, "../../engine"), "cli"]);
  }
  const validated = await run(SCHED, ["validate", root]);
  expect((JSON.parse(validated.stdout) as { ok: boolean }).ok).toBe(true);
  const lineup = await run(SCHED, ["lineup", root, "--channel", "1", "--date", "2026-10-20"]);
  const result = JSON.parse(lineup.stdout) as { airings: { start_ms: number; duration_ms: number }[] };
  expect(result.airings.length).toBeGreaterThan(0);
  expect(result.airings[0]!.start_ms).toBe(0);
  for (let i = 1; i < result.airings.length; i++) {
    const prev = result.airings[i - 1]!;
    expect(result.airings[i]!.start_ms).toBe(prev.start_ms + prev.duration_ms);
  }
  const last = result.airings[result.airings.length - 1]!;
  expect(last.start_ms + last.duration_ms).toBe(86400000);
}, 120000);
