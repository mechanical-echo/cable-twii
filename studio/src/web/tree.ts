import type { LibAd, LibEpisode, LibShow, Library } from "../server/types.js";

export type WebLibrary = Omit<Library, "generated"> & { generated: string };

export interface SeasonNode {
  season: number;
  episodes: LibEpisode[];
}

export interface ShowNode {
  show: LibShow;
  seasons: SeasonNode[];
}

export interface ChannelNode {
  channel: string;
  shows: ShowNode[];
  ads: LibAd[];
}

export function buildTree(library: WebLibrary): ChannelNode[] {
  const channels = new Map<string, ChannelNode>();
  const node = (name: string): ChannelNode => {
    let found = channels.get(name);
    if (!found) {
      found = { channel: name, shows: [], ads: [] };
      channels.set(name, found);
    }
    return found;
  };
  for (const show of library.show) {
    const seasons = new Map<number, LibEpisode[]>();
    for (const ep of show.episode) {
      const list = seasons.get(ep.season) ?? [];
      list.push(ep);
      seasons.set(ep.season, list);
    }
    const ordered = [...seasons.entries()]
      .sort((a, b) => a[0] - b[0])
      .map(([season, episodes]) => ({ season, episodes: [...episodes].sort((a, b) => a.episode - b.episode) }));
    node(show.channel).shows.push({ show, seasons: ordered });
  }
  for (const ad of library.ad) node(ad.channel).ads.push(ad);
  return [...channels.values()];
}

export function seasonLabel(season: number): string {
  return season === 0 ? "Episodes" : `Season ${season}`;
}

export function descriptionKey(ep: Pick<LibEpisode, "season" | "episode" | "path">): string {
  if (ep.season >= 1) return `S${String(ep.season).padStart(2, "0")}E${String(ep.episode).padStart(2, "0")}`;
  const file = ep.path.slice(ep.path.lastIndexOf("/") + 1);
  return file.endsWith(".mpg") ? file.slice(0, -".mpg".length) : file;
}
