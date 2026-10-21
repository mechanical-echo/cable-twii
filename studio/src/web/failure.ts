import { ApiError, type Issue } from "./api.js";

export interface Failure {
  message: string;
  issues: Issue[];
  conflict: boolean;
}

export const CONFLICT_MESSAGE = "config.toml changed on disk — Reload";

export function describeIssue(issue: Issue): string {
  return `${issue.level === "error" ? "Error" : "Warning"}: ${issue.message}`;
}

export function toFailure(err: unknown): Failure {
  if (err instanceof ApiError) {
    return { message: err.message, issues: err.issues ?? [], conflict: err.status === 409 };
  }
  return { message: (err as Error).message, issues: [], conflict: false };
}
