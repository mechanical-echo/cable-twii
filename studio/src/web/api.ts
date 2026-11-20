import type { DriveInfo, ScanResult } from "../server/drive.js";
import type { ImportEntry } from "../server/imports.js";
import type { BuildResult } from "../server/library.js";
import type { ValidateResult } from "../server/sched.js";
import type { ShowMeta } from "../server/showMeta.js";
import type { Config, Issue, Job, LineupResult } from "../server/types.js";
import { streamLost } from "./jobs.js";
import type { WebLibrary } from "./tree.js";
import { UPLOAD_BATCH_FILES, batches, uploadGroups, uploadRatio } from "./uploading.js";

export type { DriveInfo, ScanResult, ImportEntry, Config, Issue, Job, LineupResult, ValidateResult };

export interface ModeInfo {
  mode: "drive" | "server";
  name: string | null;
  signedIn: boolean;
}

export interface UploadRejected {
  name: string;
  reason: string;
}

export interface UploadResult {
  dir: string;
  files: ScanResult[];
  rejected: UploadRejected[];
}

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
  mtimeMs: number;
}

export class ApiError extends Error {
  constructor(message: string, readonly status: number, readonly issues?: Issue[]) {
    super(message);
    this.name = "ApiError";
  }
}

let unauthorized: (() => void) | null = null;

export function onUnauthorized(handler: () => void): () => void {
  unauthorized = handler;
  return () => {
    if (unauthorized === handler) unauthorized = null;
  };
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
    if (res.status === 401) unauthorized?.();
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

export const getMode = () => send<ModeInfo>("GET", "/api/mode");
export const login = (password: string) => send<{ ok: boolean }>("POST", "/api/login", { password });
export const logout = () => send<{ ok: boolean }>("POST", "/api/logout");
export const getDrive = () => send<DriveInfo | null>("GET", "/api/drive");
export const setDrive = (path: string) => send<DriveInfo>("POST", "/api/drive", { path });
export const pickDrive = () => send<DriveInfo | null>("POST", "/api/drive/pick");
export const initDrive = () => send<DriveInfo>("POST", "/api/drive/init");
export const driveDone = () => send<{ ok: boolean; skipped?: boolean }>("POST", "/api/drive/done");
export const pickSource = () => send<{ dir: string; files: ScanResult[] } | null>("POST", "/api/import/pick-source");
export const getImports = (dir: string) => send<{ dir: string; entries: ImportEntry[] }>("GET", `/api/imports?dir=${encodeURIComponent(dir)}`);
export const scanImports = (dir: string) => send<{ dir: string; files: ScanResult[] }>("POST", "/api/import/scan", { dir });
export const importShow = (req: ImportShowRequest) => send<ImportResult>("POST", "/api/import/show", req);
export const importAds = (req: ImportAdsRequest) => send<ImportResult>("POST", "/api/import/ads", req);
export const checkUploads = (files: { name: string; size: number }[]) =>
  send<{ added: string[] }>("POST", "/api/upload/check", { files }).then((r) => r.added);
export const getJobs = () => send<Job[]>("GET", "/api/jobs");
export const retryJobs = () => send<{ jobs: Job[] }>("POST", "/api/jobs/retry");
export const getShrinkCount = () => send<{ candidates: number }>("GET", "/api/library/shrink");
export const startShrink = () => send<{ queued: number }>("POST", "/api/library/shrink");
export const getLibrary = () => send<WebLibrary>("GET", "/api/library");
export const rebuildLibrary = () => send<WebBuildResult>("POST", "/api/library/rebuild");
export const getConfig = () => send<{ config: Config; mtimeMs: number }>("GET", "/api/config");
export const putConfig = (config: Config, mtimeMs: number) =>
  send<{ mtimeMs: number; issues: Issue[] }>("PUT", "/api/config", { config, mtimeMs });
export const reshuffle = (mtimeMs: number) => send<{ mtimeMs: number; seed: number }>("POST", "/api/config/reshuffle", { mtimeMs });
export const validate = () => send<ValidateResult>("POST", "/api/validate");
export const getPreview = (channel: number, date: string) =>
  send<LineupResult>("GET", `/api/preview?channel=${channel}&date=${encodeURIComponent(date)}`);
export const getShowMeta = (channelFolder: string, showFolder: string) =>
  send<{ meta: ShowMeta; mtimeMs: number }>("GET", `/api/show-meta?channelFolder=${encodeURIComponent(channelFolder)}&showFolder=${encodeURIComponent(showFolder)}`);
export const putShowMeta = (req: ShowMetaRequest) => send<WebLibrary>("PUT", "/api/show-meta", req);

export function uploadIcon(kind: "channel" | "show", channelFolder: string, showFolder: string | null, file: Blob): Promise<{ path: string }> {
  const form = new FormData();
  form.append("kind", kind);
  form.append("channelFolder", channelFolder);
  if (showFolder !== null) form.append("showFolder", showFolder);
  form.append("file", file);
  return fetch("/api/icon", { method: "POST", headers: API_HEADERS, body: form }).then((res) => parse<{ path: string }>(res));
}

function uploadBatch(files: File[], onProgress: (ratio: number) => void, signal?: AbortSignal): Promise<UploadResult> {
  return new Promise((resolve, reject) => {
    const form = new FormData();
    for (const file of files) form.append("file", file, file.name);
    const xhr = new XMLHttpRequest();
    const abort = () => xhr.abort();
    signal?.addEventListener("abort", abort, { once: true });
    const settle = () => signal?.removeEventListener("abort", abort);
    xhr.open("POST", "/api/upload");
    xhr.setRequestHeader("x-cable-twii", "1");
    xhr.upload.onprogress = (event) => onProgress(uploadRatio(event.loaded, event.total));
    xhr.onload = () => {
      settle();
      const text = xhr.responseText;
      parse<UploadResult>(new Response(text === "" ? null : text, { status: xhr.status })).then(resolve, reject);
    };
    xhr.onerror = () => {
      settle();
      reject(new Error("The upload was cut off. Check the connection and try again."));
    };
    xhr.onabort = () => {
      settle();
      reject(new DOMException("The upload was cancelled.", "AbortError"));
    };
    xhr.send(form);
  });
}

export async function uploadVideos(files: File[], onProgress: (ratio: number) => void, signal?: AbortSignal): Promise<UploadResult> {
  const total = files.reduce((sum, file) => sum + file.size, 0);
  const merged: UploadResult = { dir: "", files: [], rejected: [] };
  let sent = 0;
  for (const batch of batches(files, UPLOAD_BATCH_FILES)) {
    const size = batch.reduce((sum, file) => sum + file.size, 0);
    const result = await uploadBatch(batch, (ratio) => onProgress(uploadRatio(sent + ratio * size, total)), signal);
    if (merged.dir === "") merged.dir = result.dir;
    merged.files.push(...result.files);
    merged.rejected.push(...result.rejected);
    sent += size;
    onProgress(uploadRatio(sent, total));
  }
  return merged;
}

export async function discardUploaded(sources: string[]): Promise<void> {
  const failed: string[] = [];
  for (const group of uploadGroups(sources)) {
    try {
      await send<{ removed: number; kept: number }>("POST", "/api/upload/discard", group);
    } catch (err) {
      failed.push((err as Error).message);
    }
  }
  if (failed.length > 0) throw new Error(failed[0]);
}

export interface JobStreamHandlers {
  onOpen?: () => void;
  onError?: (closed: boolean) => void;
}

export function subscribeJobs(cb: (job: Job) => void, handlers: JobStreamHandlers = {}): () => void {
  const source = new EventSource("/api/jobs/stream");
  source.onopen = () => handlers.onOpen?.();
  source.onerror = () => handlers.onError?.(streamLost(source.readyState, getJobs));
  source.onmessage = (event) => {
    try {
      cb(JSON.parse(event.data as string) as Job);
    } catch {
      return;
    }
  };
  return () => source.close();
}

export function sourceUrl(path: string): string {
  return `/api/import/file?path=${encodeURIComponent(path)}`;
}

export function mediaUrl(rel: string): string {
  return `/media/${rel.split("/").map(encodeURIComponent).join("/")}`;
}
