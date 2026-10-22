import { expect, test } from "vitest";
import { episodeFileName, fatSafe, fileStem, guessEpisode, isVideoFile, uniqueName } from "../src/server/names.js";

test("guess SxxEyy with title", () => expect(guessEpisode("Gumball S01E02 The Responsible.mkv")).toEqual({ season: 1, episode: 2, title: "The Responsible" }));
test("guess lowercase and dots", () => expect(guessEpisode("adventure.time.s05e01.finn.the.human.mp4")).toEqual({ season: 5, episode: 1, title: "Finn The Human" }));
test("guess 1x05", () => expect(guessEpisode("Ben 10 - 1x05 - Title.avi")).toEqual({ season: 1, episode: 5, title: "Title" }));
test("guess Episode N", () => expect(guessEpisode("Episode 7.mp4")).toEqual({ season: null, episode: 7, title: "" }));
test("guess plain name", () => expect(guessEpisode("Artist - Song.mp4")).toEqual({ season: null, episode: null, title: "Artist - Song" }));
test("fatSafe replaces and strips", () => expect(fatSafe('AC/DC: Live? <1> "x" |y|.')).toBe("AC-DC- Live 1 x -y-"));
test("fatSafe keeps unicode", () => expect(fatSafe("Pokémon ✨")).toBe("Pokémon ✨"));
test("fatSafe suffixes Windows device names in any case, with or without extension", () => {
  const cases: [string, string][] = [
    ["CON", "CON_"],
    ["prn", "prn_"],
    ["Aux", "Aux_"],
    ["NUL", "NUL_"],
    ["nul.txt", "nul_.txt"],
    ["COM1", "COM1_"],
    ["com9.mpg", "com9_.mpg"],
    ["LPT1", "LPT1_"],
    ["lpt9.tar.gz", "lpt9_.tar.gz"],
    ["NUL ", "NUL_"],
    ["CON .txt", "CON_ .txt"],
  ];
  for (const [input, expected] of cases) expect(fatSafe(input), input).toBe(expected);
});
test("fatSafe leaves names that only start like device names", () => {
  for (const name of ["CONAN", "Console", "NULL", "COM10", "LPT0", "com", "Auxiliary.mpg", "My NUL"]) expect(fatSafe(name), name).toBe(name);
});
test("fileStem suffixes a device name revealed by stripping leading dots", () => {
  expect(fileStem(".nul")).toBe("nul_");
  expect(fileStem(" .CON.mpg")).toBe("CON_.mpg");
});
test("episodeFileName never produces a device name", () => {
  expect(episodeFileName({ season: null, episode: null, title: "NUL" })).toBe("NUL_.mpg");
});
test("fatSafe_collisions", () => {
  const taken = new Set(["s01e01 - a.mpg"]);
  expect(uniqueName("S01E01 - A.mpg", taken)).toBe("S01E01 - A (2).mpg");
});
test("episodeFileName variants", () => {
  expect(episodeFileName({ season: 1, episode: 2, title: "The Responsible" })).toBe("S01E02 - The Responsible.mpg");
  expect(episodeFileName({ season: 1, episode: 2, title: "" })).toBe("S01E02.mpg");
  expect(episodeFileName({ season: null, episode: null, title: "Artist - Song" })).toBe("Artist - Song.mpg");
});
test("isVideoFile", () => {
  expect(isVideoFile("a.MKV")).toBe(true);
  expect(isVideoFile("._a.mkv")).toBe(false);
  expect(isVideoFile("a.srt")).toBe(false);
});
test("episodeFileName never starts with a dot", () => {
  expect(episodeFileName({ season: null, episode: null, title: ".hidden" })).toBe("hidden.mpg");
  expect(episodeFileName({ season: null, episode: null, title: ". . .Dots" })).toBe("Dots.mpg");
  expect(episodeFileName({ season: null, episode: null, title: "..." })).toBe("Untitled.mpg");
  expect(episodeFileName({ season: null, episode: 3, title: ".x" })).toBe("E03 - x.mpg");
});
