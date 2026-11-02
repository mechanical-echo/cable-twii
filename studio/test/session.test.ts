import { expect, test } from "vitest";
import { clearedSessionCookie, sessionCookie, sessionToken, Sessions } from "../src/server/session.js";

const DAY_MS = 24 * 60 * 60 * 1000;

test("a wrong password gives no token", () => {
  expect(new Sessions("correct horse").signIn("wrong horse")).toBeNull();
});

test("the right password gives a valid token", () => {
  const sessions = new Sessions("correct horse");
  const token = sessions.signIn("correct horse");
  expect(token).toMatch(/^[A-Za-z0-9_-]{43}$/);
  expect(sessions.isValid(token ?? undefined)).toBe(true);
});

test("every sign-in gets its own token", () => {
  const sessions = new Sessions("correct horse");
  expect(sessions.signIn("correct horse")).not.toBe(sessions.signIn("correct horse"));
});

test("an unknown or missing token is not valid", () => {
  const sessions = new Sessions("correct horse");
  expect(sessions.isValid("made-up")).toBe(false);
  expect(sessions.isValid("")).toBe(false);
  expect(sessions.isValid(undefined)).toBe(false);
});

test("a token expires after 30 days", () => {
  let now = 1_000_000;
  const sessions = new Sessions("correct horse", undefined, () => now);
  const token = sessions.signIn("correct horse") as string;
  now += 30 * DAY_MS - 1;
  expect(sessions.isValid(token)).toBe(true);
  now += 1;
  expect(sessions.isValid(token)).toBe(false);
});

test("a token expires after the given ttl", () => {
  let now = 0;
  const sessions = new Sessions("correct horse", 1000, () => now);
  const token = sessions.signIn("correct horse") as string;
  now = 999;
  expect(sessions.isValid(token)).toBe(true);
  now = 1000;
  expect(sessions.isValid(token)).toBe(false);
});

test("signOut invalidates the token and leaves the others", () => {
  const sessions = new Sessions("correct horse");
  const first = sessions.signIn("correct horse") as string;
  const second = sessions.signIn("correct horse") as string;
  sessions.signOut(first);
  expect(sessions.isValid(first)).toBe(false);
  expect(sessions.isValid(second)).toBe(true);
  sessions.signOut(undefined);
  expect(sessions.isValid(second)).toBe(true);
});

test("sessionToken finds the session cookie among others", () => {
  expect(sessionToken("a=1; ctw_session=abc; b=2")).toBe("abc");
  expect(sessionToken("ctw_session=abc")).toBe("abc");
});

test("sessionToken ignores other cookies", () => {
  expect(sessionToken(undefined)).toBeUndefined();
  expect(sessionToken("")).toBeUndefined();
  expect(sessionToken("a=1; b=2")).toBeUndefined();
  expect(sessionToken("not_ctw_session=abc")).toBeUndefined();
  expect(sessionToken("ctw_session_2=abc")).toBeUndefined();
  expect(sessionToken("ctw_session=")).toBeUndefined();
});

test("the cookies are host-only, http-only and strict", () => {
  expect(sessionCookie("abc")).toBe("ctw_session=abc; Path=/; HttpOnly; SameSite=Strict; Max-Age=2592000");
  expect(clearedSessionCookie()).toBe("ctw_session=; Path=/; HttpOnly; SameSite=Strict; Max-Age=0");
});
