import type { ModeInfo } from "./api.js";

export type SignInView = "none" | "page" | "overlay";

export function signInView(mode: ModeInfo | null, lost: boolean): SignInView {
  if (mode?.mode !== "server") return "none";
  if (!mode.signedIn) return "page";
  return lost ? "overlay" : "none";
}
