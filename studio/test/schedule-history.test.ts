import { describe, expect, it } from "vitest";
import { HISTORY_LIMIT, canRedo, canUndo, createHistory, pushHistory, redo, undo } from "../src/web/schedule/history.js";

describe("history", () => {
  it("starts with nothing to undo or redo", () => {
    const h = createHistory("a");
    expect(canUndo(h)).toBe(false);
    expect(canRedo(h)).toBe(false);
    expect(undo(h)).toBe(h);
    expect(redo(h)).toBe(h);
  });

  it("undoes and redoes in order", () => {
    let h = pushHistory(pushHistory(createHistory("a"), "b"), "c");
    h = undo(h);
    expect(h.present).toBe("b");
    h = undo(h);
    expect(h.present).toBe("a");
    expect(canUndo(h)).toBe(false);
    h = redo(h);
    expect(h.present).toBe("b");
    expect(h.future).toEqual(["c"]);
  });

  it("drops the redo branch on a new edit", () => {
    const h = pushHistory(undo(pushHistory(createHistory("a"), "b")), "x");
    expect(h.present).toBe("x");
    expect(canRedo(h)).toBe(false);
    expect(h.past).toEqual(["a"]);
  });

  it("ignores pushing the same state", () => {
    const h = createHistory("a");
    expect(pushHistory(h, "a")).toBe(h);
  });

  it("keeps at most 50 undo steps", () => {
    let h = createHistory(0);
    for (let i = 1; i <= 60; i++) h = pushHistory(h, i);
    expect(HISTORY_LIMIT).toBe(50);
    expect(h.past.length).toBe(50);
    expect(h.past[0]).toBe(10);
    while (canUndo(h)) h = undo(h);
    expect(h.present).toBe(10);
  });
});
