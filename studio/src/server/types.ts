export type Mode = "timed" | "looping";
export type Item =
  | { type: "episode"; show: string; season: number; episode: number; from?: string }
  | { type: "random"; show: string; season?: number; count: number; from?: string }
  | { type: "random-run"; show: string; season?: number; count: number; from?: string }
  | { type: "ads"; count: number }
  | { type: "ad"; path: string }
  | { type: "fill"; show: string; season?: number; ads_between?: number; from?: string };
export interface Slot { at: string; items: Item[] }
export interface Channel { number: number; name: string; category: string; folder?: string; icon?: string; ads: boolean; mode: Mode; slot?: Slot[]; loop?: Item[] }
export interface Config { version: 1; seed: number; channel: Channel[] }

export interface LibEpisode { season: number; episode: number; title: string; description: string; path: string; duration_ms: number }
export interface LibShow { channel: string; folder: string; name: string; icon: string; episode: LibEpisode[] }
export interface LibAd { path: string; channel: string; duration_ms: number }
export interface Library { version: 1; generated: Date; ad: LibAd[]; show: LibShow[] }

export interface SourceRecord { target: string; source: string; size: number; mtimeMs: number; durationMs: number }
export interface EpisodeGuess { season: number | null; episode: number | null; title: string }

export interface Issue { level: "error" | "warning"; code: string; channel: number; slot: number; item: number; line: number; file: "config" | "library" | "lineup"; message: string }
export interface Airing { start_ms: number; duration_ms: number; kind: "episode" | "ad" | "upnext"; path: string | null; show: string | null; season: number | null; episode: number | null; title: string | null; slot: number; item: number }
export interface LineupResult { channel: number; date: string; hash: string; airings: Airing[]; warnings: Issue[] }

export type JobState = "queued" | "running" | "done" | "failed";
export interface Job { id: string; source: string; target: string; aspect: "16:9" | "4:3"; state: JobState; progress: number; error?: string }
