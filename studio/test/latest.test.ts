import { describe, expect, it } from "vitest";
import { createLatest } from "../src/web/latest.js";

describe("createLatest", () => {
  it("only the most recent request is current", () => {
    const latest = createLatest();
    const first = latest.begin();
    expect(first()).toBe(true);
    const second = latest.begin();
    expect(first()).toBe(false);
    expect(second()).toBe(true);
  });

  it("cancel invalidates the pending request", () => {
    const latest = createLatest();
    const pending = latest.begin();
    latest.cancel();
    expect(pending()).toBe(false);
  });

  it("a request begun after cancel is current", () => {
    const latest = createLatest();
    latest.cancel();
    expect(latest.begin()()).toBe(true);
  });
});
