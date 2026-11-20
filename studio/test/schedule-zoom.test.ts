import { expect, test } from "vitest";
import { defaultZoom } from "../src/web/schedule/zoom.js";

test("timed opens at 30 px per hour", () => {
  expect(defaultZoom("timed")).toBe(30);
});

test("looping opens at 120 px per hour", () => {
  expect(defaultZoom("looping")).toBe(120);
});
