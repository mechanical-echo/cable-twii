import type { EpisodeGuess } from "./types.js";

const VIDEO_EXTENSIONS = new Set([".mp4", ".mkv", ".avi", ".mov", ".m4v", ".mpg", ".mpeg", ".ts", ".webm", ".wmv", ".flv"]);
const MAX_NAME_LENGTH = 120;
const DEVICE_PREFIX = /^(con|prn|aux|nul|com[1-9¹²³]|lpt[1-9¹²³])/i;
const DEVICE_NAME = /^(con|prn|aux|nul|com[1-9¹²³]|lpt[1-9¹²³]) *(\.|$)/i;

const SEASON_EPISODE = /(?<![a-z0-9])s(\d{1,3})[ ._-]?e(\d{1,3})(?![0-9])/i;
const CROSS = /(?<![a-z0-9])(\d{1,2})x(\d{1,3})(?![a-z0-9])/i;
const EPISODE_WORD = /(?<![a-z0-9])(?:episode|ep)[ ._-]*(\d{1,3})(?![0-9])/i;

function extensionOf(name: string): string {
  const dot = name.lastIndexOf(".");
  return dot > 0 ? name.slice(dot).toLowerCase() : "";
}

function stripExtension(name: string): string {
  const dot = name.lastIndexOf(".");
  if (dot > 0 && /^\.[A-Za-z0-9]{1,5}$/.test(name.slice(dot))) return name.slice(0, dot);
  return name;
}

function cleanTitle(raw: string): string {
  let title = raw.replace(/[._]+/g, " ").replace(/\s+/g, " ").trim();
  title = title.replace(/^(?:-\s*)+/, "").trim();
  if (/\p{L}/u.test(title) && title === title.toLowerCase()) {
    title = title.replace(/(^|\s)(\p{L})/gu, (_, space: string, letter: string) => space + letter.toUpperCase());
  }
  return title;
}

export function guessEpisode(fileName: string): EpisodeGuess {
  const base = stripExtension(fileName);

  const full = SEASON_EPISODE.exec(base) ?? CROSS.exec(base);
  if (full) {
    return {
      season: Number(full[1]),
      episode: Number(full[2]),
      title: cleanTitle(base.slice(full.index + full[0].length)),
    };
  }

  const word = EPISODE_WORD.exec(base);
  if (word) {
    return {
      season: null,
      episode: Number(word[1]),
      title: cleanTitle(base.slice(word.index + word[0].length)),
    };
  }

  return { season: null, episode: null, title: cleanTitle(base) };
}

export function fatSafe(name: string): string {
  const cleaned = name
    .replace(/[/\\:|]/g, "-")
    .replace(/[*?"<>]/g, "")
    .replace(/\p{Cc}/gu, "")
    .replace(/ {2,}/g, " ")
    .trim();
  const capped = Array.from(cleaned).slice(0, MAX_NAME_LENGTH).join("");
  return notDevice(capped.replace(/[ .]+$/, "").replace(/^ +/, ""));
}

function notDevice(name: string): string {
  return DEVICE_NAME.test(name) ? name.replace(DEVICE_PREFIX, "$1_") : name;
}

export function fileStem(name: string): string {
  return notDevice(fatSafe(name).replace(/^[ .]+/, ""));
}

export function uniqueName(name: string, taken: Set<string>): string {
  const lowered = new Set(Array.from(taken, (entry) => entry.toLowerCase()));
  if (!lowered.has(name.toLowerCase())) return name;
  const dot = name.lastIndexOf(".");
  const stem = dot > 0 ? name.slice(0, dot) : name;
  const extension = dot > 0 ? name.slice(dot) : "";
  for (let n = 2; ; n++) {
    const candidate = `${stem} (${n})${extension}`;
    if (!lowered.has(candidate.toLowerCase())) return candidate;
  }
}

function pad(value: number): string {
  return String(value).padStart(value >= 100 ? 3 : 2, "0");
}

export function episodeFileName(e: EpisodeGuess): string {
  const title = fileStem(e.title);
  let code = "";
  if (e.season !== null && e.episode !== null) code = `S${pad(e.season)}E${pad(e.episode)}`;
  else if (e.episode !== null) code = `E${pad(e.episode)}`;
  const stem = code && title ? `${code} - ${title}` : code || title || "Untitled";
  return `${stem}.mpg`;
}

export function isVideoFile(name: string): boolean {
  if (name.startsWith("._")) return false;
  return VIDEO_EXTENSIONS.has(extensionOf(name));
}
