import { type ShowRef, channelFolder, channelsAiring } from "../server/scheduleRefs.js";
import type { Config, LibAd, LibEpisode, LibShow } from "../server/types.js";
import type { WebLibrary } from "./tree.js";

export interface ShowEntry {
  ref: ShowRef;
  name: string;
  icon: string;
  seasons: number;
  episodes: number;
  off: number;
  airsOn: number[];
}

export interface ShowGroup {
  folder: string;
  label: string;
  shows: ShowEntry[];
}

export function allShows(library: WebLibrary): LibShow[] {
  const shows = new Map<string, LibShow>();
  for (const show of [...library.show, ...(library.off?.show ?? [])]) {
    const key = `${show.channel}\0${show.folder}`;
    const found = shows.get(key);
    if (found) shows.set(key, { ...found, episode: [...found.episode, ...show.episode].sort((a, b) => a.season - b.season || a.episode - b.episode) });
    else shows.set(key, { ...show, episode: [...show.episode] });
  }
  return [...shows.values()];
}

export function offPaths(library: WebLibrary): Set<string> {
  return new Set([...(library.off?.show ?? []).flatMap((s) => s.episode.map((e) => e.path)), ...(library.off?.ad ?? []).map((a) => a.path)]);
}

export function showGroups(library: WebLibrary, config: Config): ShowGroup[] {
  const off = offPaths(library);
  const byFolder = new Map<string, ShowEntry[]>();
  for (const show of allShows(library)) {
    const ref = { channel: show.channel, show: show.folder };
    const entry: ShowEntry = {
      ref,
      name: show.name,
      icon: show.icon,
      seasons: new Set(show.episode.map((e) => e.season)).size,
      episodes: show.episode.length,
      off: show.episode.filter((e) => off.has(e.path)).length,
      airsOn: channelsAiring(config, ref),
    };
    byFolder.set(show.channel, [...(byFolder.get(show.channel) ?? []), entry]);
  }
  const groups: ShowGroup[] = [];
  const take = (folder: string, label: string) => {
    const shows = byFolder.get(folder);
    if (shows === undefined) return;
    groups.push({ folder, label, shows: shows.sort((a, b) => a.name.localeCompare(b.name)) });
    byFolder.delete(folder);
  };
  for (const ch of [...config.channel].sort((a, b) => a.number - b.number)) take(channelFolder(ch), ch.name);
  for (const folder of [...byFolder.keys()].sort((a, b) => a.localeCompare(b))) take(folder, `${folder} · not on a channel`);
  return groups;
}

export function findEpisode(library: WebLibrary, path: string): { show: LibShow; episode: LibEpisode; off: boolean } | null {
  for (const [shows, off] of [[library.show, false], [library.off?.show ?? [], true]] as const) {
    for (const show of shows) {
      const episode = show.episode.find((e) => e.path === path);
      if (episode) return { show: allShows(library).find((s) => s.channel === show.channel && s.folder === show.folder) ?? show, episode, off };
    }
  }
  return null;
}

export function findAd(library: WebLibrary, path: string): { ad: LibAd; off: boolean } | null {
  const on = library.ad.find((a) => a.path === path);
  if (on) return { ad: on, off: false };
  const off = library.off?.ad.find((a) => a.path === path);
  return off ? { ad: off, off: true } : null;
}
