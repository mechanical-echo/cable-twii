import { describe, expect, it } from "vitest";
import type { Issue } from "../src/server/types.js";
import type { TimelineBlock } from "../src/web/schedule/layout.js";
import { blockKey, describeLength, errorBlocks, formatClock, hintMs, hourLabels, issueWhere, overlapLabel } from "../src/web/schedule/marks.js";

function issue(over: Partial<Issue>): Issue {
  return { level: "error", code: "unknown_show", channel: 1, slot: 0, item: 0, line: 0, file: "config", message: "m", ...over };
}

function block(slotIndex: number, itemIndex: number | null, startMs: number, durationMs: number): TimelineBlock {
  return { slotIndex, itemIndex, kind: "episode", startMs, durationMs, exact: true, overlapMs: 0, label: "" };
}

describe("errorBlocks", () => {
  it("maps error issues of the channel to block keys", () => {
    const keys = errorBlocks(
      [issue({ slot: 1, item: 2 }), issue({ level: "warning", item: 3 }), issue({ channel: 2, item: 4 }), issue({ item: -1 })],
      1,
      "timed"
    );
    expect([...keys]).toEqual([blockKey(1, 2)]);
  });

  it("maps loop issues (slot -1) to the single stack", () => {
    expect([...errorBlocks([issue({ channel: 2, slot: -1, item: 1 })], 2, "looping")]).toEqual(["0:1"]);
  });
});

describe("issueWhere", () => {
  it("names the channel, slot time and item", () => {
    expect(issueWhere(issue({ slot: 1, item: 2 }), "timed", (i) => ["07:00", "19:00"][i])).toBe("CH1 · slot 19:00 · item 3");
    expect(issueWhere(issue({ slot: -1, item: 0 }), "looping", () => undefined)).toBe("CH1 · item 1");
    expect(issueWhere(issue({ channel: -1 }), null, () => undefined)).toBe("");
  });
});

describe("labels", () => {
  it("formats overlap in whole minutes, at least 1", () => {
    expect(overlapLabel(19 * 60000)).toBe("⚠ 19 min overlap");
    expect(overlapLabel(5000)).toBe("⚠ 1 min overlap");
  });

  it("formats exact and estimated lengths", () => {
    expect(formatClock(665000)).toBe("11:05");
    expect(formatClock(4200000)).toBe("1:10:00");
    expect(describeLength(665000, true)).toBe("11:05");
    expect(describeLength(1980000, false)).toBe("about 33 min");
    expect(describeLength(30000, false)).toBe("about 30 s");
    expect(describeLength(0, false)).toBe("length unknown");
    expect(describeLength(Infinity, false)).toBe("fills the rest of the slot");
  });

  it("labels 24 hours from the first slot", () => {
    const labels = hourLabels("07:30");
    expect(labels.length).toBe(24);
    expect(labels[0]).toBe("07:30");
    expect(labels[17]).toBe("00:30");
    expect(labels[23]).toBe("06:30");
  });
});

describe("hintMs", () => {
  const blocks = [block(0, 0, 0, 1000), block(0, 1, 1000, 2000), block(0, null, 3000, 5000), block(1, 0, 8000, Infinity)];

  it("points at the start of the item at the index", () => {
    expect(hintMs(blocks, 0, 1, 0)).toBe(1000);
  });

  it("points after the last item when appending", () => {
    expect(hintMs(blocks, 0, 2, 0)).toBe(3000);
    expect(hintMs(blocks, 1, 1, 8000)).toBe(8000);
  });

  it("points at the slot start when the slot is empty", () => {
    expect(hintMs(blocks, 2, 0, 9000)).toBe(9000);
  });
});
