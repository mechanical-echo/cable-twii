import type { DriveInfo, ScanResult } from "../server/drive.js";
import type { BuildResult } from "../server/library.js";
import type { ValidateResult } from "../server/sched.js";
import type { Config, Issue, Job, LineupResult } from "../server/types.js";
import type { WebLibrary } from "./tree.js";

export type { DriveInfo, ScanResult, Config, Issue, Job, LineupResult, ValidateResult };

export interface WebBuildResult extends Omit<BuildResult, "library"> {
  library: WebLibrary;
}

export interface Rejected {
  source: string;
  reason: string;
}

export interface ImportEpisode {
  source: string;
  season: number | null;
  episode: number | null;
  title: string;
  description: string;
  aspect: "16:9" | "4:3";
}

export interface ImportShowRequest {
  channelFolder: string;
  showFolder: string;
  showName: string;
  episodes: ImportEpisode[];
}

export interface ImportAdsRequest {
  channelFolder: string;
  files: { source: string; aspect: "16:9" | "4:3" }[];
}

export interface ImportResult {
  jobs: Job[];
  rejected: Rejected[];
}

export interface ShowMetaRequest {
  channelFolder: string;
  showFolder: string;
  name: string;
  episodes: Record<string, string>;
}

export class ApiError extends Error {
  constructor(message: string, readonly status: number, readonly issues?: Issue[]) {
    super(message);
    this.name = "ApiError";
  }
}

async function parse<T>(res: Response): Promise<T> {
  const text = await res.text();
  let body: unknown = null;
  if (text !== "") {
    try {
      body = JSON.parse(text);
    } catch {
      body = null;
    }
  }
  if (!res.ok) {
    const failure = (body ?? {}) as { error?: unknown; issues?: Issue[] };
    const message = typeof failure.error === "string" ? failure.error : `Request failed (${res.status})`;
    throw new ApiError(message, res.status, failure.issues);
  }
  return body as T;
}

const API_HEADERS = { "x-cable-twii": "1" };

function send<T>(method: string, url: string, body?: unknown): Promise<T> {
  const init: RequestInit = { method, headers: API_HEADERS };
  if (body !== undefined) {
    init.headers = { ...API_HEADERS, "content-type": "application/json" };
    init.body = JSON.stringify(body);
  }
  return fetch(url, init).then((res) => parse<T>(res));
}

export const getDrive = () => send<DriveInfo | null>("GET", "/api/drive");
export const setDrive = (path: string) => send<DriveInfo>("POST", "/api/drive", { path });
export const pickDrive = () => send<DriveInfo | null>("POST", "/api/drive/pick");
export const initDrive = () => send<DriveInfo>("POST", "/api/drive/init");
export const driveDone = () => send<{ ok: boolean; skipped?: boolean }>("POST", "/api/drive/done");
export const pickSource = () => send<{ dir: string; files: ScanResult[] } | null>("POST", "/api/import/pick-source");
export const importShow = (req: ImportShowRequest) => send<ImportResult>("POST", "/api/import/show", req);
export const importAds = (req: ImportAdsRequest) => send<ImportResult>("POST", "/api/import/ads", req);
export const getJobs = () => send<Job[]>("GET", "/api/jobs");
export const getLibrary = () => send<WebLibrary>("GET", "/api/library");
export const rebuildLibrary = () => send<WebBuildResult>("POST", "/api/library/rebuild");
export const getConfig = () => send<{ config: Config; mtimeMs: number }>("GET", "/api/config");
export const putConfig = (config: Config, mtimeMs: number) =>
  send<{ mtimeMs: number; issues: Issue[] }>("PUT", "/api/config", { config, mtimeMs });
export const reshuffle = (mtimeMs: number) => send<{ mtimeMs: number; seed: number }>("POST", "/api/config/reshuffle", { mtimeMs });
export const validate = () => send<ValidateResult>("POST", "/api/validate");
export const getPreview = (channel: number, date: string) =>
  send<LineupResult>("GET", `/api/preview?channel=${channel}&date=${encodeURIComponent(date)}`);
export const putShowMeta = (req: ShowMetaRequest) => send<WebLibrary>("PUT", "/api/show-meta", req);

export function uploadIcon(kind: "channel" | "show", channelFolder: string, showFolder: string | null, file: Blob): Promise<{ path: string }> {
  const form = new FormData();
  form.append("kind", kind);
  form.append("channelFolder", channelFolder);
  if (showFolder !== null) form.append("showFolder", showFolder);
  form.append("file", file);
  return fetch("/api/icon", { method: "POST", headers: API_HEADERS, body: form }).then((res) => parse<{ path: string }>(res));
}

export interface JobStreamHandlers {
  onOpen?: () => void;
  onError?: () => void;
}

export function subscribeJobs(cb: (job: Job) => void, handlers: JobStreamHandlers = {}): () => void {
  const source = new EventSource("/api/jobs/stream");
  source.onopen = () => handlers.onOpen?.();
  source.onerror = () => handlers.onError?.();
  source.onmessage = (event) => {
    try {
      cb(JSON.parse(event.data as string) as Job);
    } catch {
      return;
    }
  };
  return () => source.close();
}

export function mediaUrl(rel: string): string {
  return `/media/${rel.split("/").map(encodeURIComponent).join("/")}`;
}
