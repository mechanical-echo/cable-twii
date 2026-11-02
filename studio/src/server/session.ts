import { createHash, randomBytes, timingSafeEqual } from "node:crypto";

export const SESSION_COOKIE = "ctw_session";

const SESSION_TTL_MS = 30 * 24 * 60 * 60 * 1000;
const COOKIE_ATTRIBUTES = "Path=/; HttpOnly; SameSite=Strict";

function digest(value: string): Buffer {
  return createHash("sha256").update(value).digest();
}

export class Sessions {
  private readonly expected: Buffer;
  private readonly expiries = new Map<string, number>();

  constructor(password: string, private readonly ttlMs: number = SESSION_TTL_MS, private readonly now: () => number = Date.now) {
    this.expected = digest(password);
  }

  signIn(password: string): string | null {
    if (!timingSafeEqual(digest(password), this.expected)) return null;
    const token = randomBytes(32).toString("base64url");
    this.expiries.set(token, this.now() + this.ttlMs);
    return token;
  }

  isValid(token: string | undefined): boolean {
    if (token === undefined) return false;
    const expiry = this.expiries.get(token);
    if (expiry === undefined) return false;
    if (this.now() >= expiry) {
      this.expiries.delete(token);
      return false;
    }
    return true;
  }

  signOut(token: string | undefined): void {
    if (token !== undefined) this.expiries.delete(token);
  }
}

export function sessionToken(cookieHeader: string | undefined): string | undefined {
  if (cookieHeader === undefined) return undefined;
  for (const pair of cookieHeader.split(";")) {
    const eq = pair.indexOf("=");
    if (eq < 0 || pair.slice(0, eq).trim() !== SESSION_COOKIE) continue;
    const value = pair.slice(eq + 1).trim();
    return value === "" ? undefined : value;
  }
  return undefined;
}

export function sessionCookie(token: string): string {
  return `${SESSION_COOKIE}=${token}; ${COOKIE_ATTRIBUTES}; Max-Age=${SESSION_TTL_MS / 1000}`;
}

export function clearedSessionCookie(): string {
  return `${SESSION_COOKIE}=; ${COOKIE_ATTRIBUTES}; Max-Age=0`;
}
