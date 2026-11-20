import { expect, test } from "vitest";
import { drivePort } from "../src/server/port.js";

test("port defaults to 4747", () => {
  expect(drivePort({})).toBe(4747);
});

test("port comes from CTW_PORT", () => {
  expect(drivePort({ CTW_PORT: "4791" })).toBe(4791);
});

test("bad CTW_PORT is ignored", () => {
  expect(drivePort({ CTW_PORT: "0" })).toBe(4747);
  expect(drivePort({ CTW_PORT: "x" })).toBe(4747);
  expect(drivePort({ CTW_PORT: "65536" })).toBe(4747);
  expect(drivePort({ CTW_PORT: "47.5" })).toBe(4747);
});
