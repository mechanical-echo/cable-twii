import { describe, expect, it } from "vitest";
import { ApiError } from "../src/web/api.js";
import { describeIssue, toFailure } from "../src/web/failure.js";
import type { Issue } from "../src/server/types.js";

const issue: Issue = { level: "error", code: "unknown_show", channel: 1, slot: 0, item: 2, line: 0, file: "config", message: "show not found" };

describe("toFailure", () => {
  it("marks 409 as a conflict", () => {
    expect(toFailure(new ApiError("changed", 409))).toEqual({ message: "changed", issues: [], conflict: true });
  });

  it("keeps 422 issues", () => {
    expect(toFailure(new ApiError("invalid", 422, [issue]))).toEqual({ message: "invalid", issues: [issue], conflict: false });
  });

  it("handles plain errors", () => {
    expect(toFailure(new Error("offline"))).toEqual({ message: "offline", issues: [], conflict: false });
  });

  it("describes issues by level", () => {
    expect(describeIssue(issue)).toBe("Error: show not found");
    expect(describeIssue({ ...issue, level: "warning" })).toBe("Warning: show not found");
  });
});
