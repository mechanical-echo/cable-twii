import { describe, expect, it } from "vitest";
import type { ModeInfo } from "../src/web/api.js";
import { signInView } from "../src/web/signin.js";

const server = (signedIn: boolean): ModeInfo => ({ mode: "server", name: "Home PC", signedIn });

describe("signInView", () => {
  it("shows the sign-in page on a first load without a session", () => {
    expect(signInView(server(false), false)).toBe("page");
  });

  it("shows sign-in over the still-mounted app when a session is lost", () => {
    expect(signInView(server(true), true)).toBe("overlay");
  });

  it("shows no sign-in while the session is good", () => {
    expect(signInView(server(true), false)).toBe("none");
  });

  it("goes back to the page after signing out, even if a session was lost before", () => {
    expect(signInView(server(false), true)).toBe("page");
  });

  it("never asks for a password in drive mode or before the mode is known", () => {
    expect(signInView({ mode: "drive", name: null, signedIn: true }, true)).toBe("none");
    expect(signInView(null, true)).toBe("none");
  });
});
