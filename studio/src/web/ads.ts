import type { Config } from "../server/types.js";
import { effectiveFolder, sortChannels } from "./channels.js";
import type { WebLibrary } from "./tree.js";

export interface AdGroup {
  channel: string;
  label: string;
  ads: { path: string; name: string; durationMs: number }[];
}

const AD_PATH = /^(?:[^/]+\/)?ads\/[^/]+$/;

function fileName(path: string): string {
  return path.slice(path.lastIndexOf("/") + 1).replace(/\.mpg$/i, "");
}

export function adGroups(library: WebLibrary, config: Config): AdGroup[] {
  const byChannel = new Map<string, AdGroup["ads"]>();
  for (const ad of library.ad) {
    const list = byChannel.get(ad.channel) ?? [];
    list.push({ path: ad.path, name: fileName(ad.path), durationMs: ad.duration_ms });
    byChannel.set(ad.channel, list);
  }
  for (const list of byChannel.values()) list.sort((a, b) => a.name.localeCompare(b.name));
  const groups: AdGroup[] = [];
  const take = (channel: string, label: string) => {
    const ads = byChannel.get(channel);
    if (ads === undefined) return;
    groups.push({ channel, label, ads });
    byChannel.delete(channel);
  };
  take("", "All channels");
  for (const channel of sortChannels(config.channel)) take(effectiveFolder(channel), channel.name);
  for (const folder of [...byChannel.keys()].sort((a, b) => a.localeCompare(b))) take(folder, folder);
  return groups;
}

export function skippedAds(notes: { skipped: { path: string; reason: string }[] }): { path: string; reason: string }[] {
  return notes.skipped.filter((s) => AD_PATH.test(s.path));
}
