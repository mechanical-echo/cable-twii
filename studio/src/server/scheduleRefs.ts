import type { Channel, Config, Item, Library } from "./types.js";

export interface ShowRef {
  channel: string;
  show: string;
}

export interface ItemRef {
  channel: number;
  slot: number;
  item: number;
}

interface Located {
  ref: ItemRef;
  item: Item;
  home: string;
}

export function channelFolder(ch: Pick<Channel, "name" | "folder">): string {
  const folder = ch.folder?.trim() ?? "";
  return folder !== "" ? folder : ch.name.trim();
}

export function itemShow(item: Item, home: string): ShowRef | null {
  if (item.type === "ads" || item.type === "ad") return null;
  return { channel: item.from ?? home, show: item.show };
}

function allItems(config: Config): Located[] {
  const found: Located[] = [];
  for (const ch of config.channel) {
    const home = channelFolder(ch);
    (ch.loop ?? []).forEach((item, i) => found.push({ ref: { channel: ch.number, slot: -1, item: i }, item, home }));
    (ch.slot ?? []).forEach((slot, s) => slot.items.forEach((item, i) => found.push({ ref: { channel: ch.number, slot: s, item: i }, item, home })));
  }
  return found;
}

function sameShow(a: ShowRef | null, b: ShowRef): boolean {
  return a !== null && a.channel === b.channel && a.show === b.show;
}

function unique(numbers: number[]): number[] {
  return [...new Set(numbers)].sort((a, b) => a - b);
}

export function channelsAiring(config: Config, ref: ShowRef): number[] {
  return unique(allItems(config).filter((l) => sameShow(itemShow(l.item, l.home), ref)).map((l) => l.ref.channel));
}

export function episodeChannels(config: Config, ref: ShowRef, season: number, episode: number): number[] {
  return unique(
    allItems(config)
      .filter(({ item, home }) => {
        if (!sameShow(itemShow(item, home), ref)) return false;
        if (item.type === "episode") return item.season === season && item.episode === episode;
        if (item.type === "random" || item.type === "random-run" || item.type === "fill") return item.season === undefined || item.season === season;
        return false;
      })
      .map((l) => l.ref.channel),
  );
}

function adFolder(path: string): string {
  const parts = path.split("/");
  return parts.length === 2 ? "" : (parts[0] ?? "");
}

export function adChannels(config: Config, library: Pick<Library, "ad">, path: string): number[] {
  const inLibrary = library.ad.some((a) => a.path === path);
  const folder = adFolder(path);
  const channels: number[] = [];
  for (const ch of config.channel) {
    if (!inLibrary || !ch.ads) continue;
    const own = channelFolder(ch);
    const source = library.ad.some((a) => a.channel === own) ? own : "";
    if (source === folder) channels.push(ch.number);
  }
  for (const { item, ref } of allItems(config)) if (item.type === "ad" && item.path === path) channels.push(ref.channel);
  return unique(channels);
}

export function itemsNaming(config: Config, target: { show: ShowRef; season?: number; episode?: number } | { ad: string }): ItemRef[] {
  return allItems(config)
    .filter(({ item, home }) => {
      if ("ad" in target) return item.type === "ad" && item.path === target.ad;
      if (!sameShow(itemShow(item, home), target.show)) return false;
      if (target.season === undefined || target.episode === undefined) return true;
      return item.type === "episode" && item.season === target.season && item.episode === target.episode;
    })
    .map((l) => l.ref);
}

function mapItems(config: Config, change: (item: Item, ref: ItemRef, home: string) => Item | null): Config {
  return {
    ...config,
    channel: config.channel.map((ch) => {
      const home = channelFolder(ch);
      const next: Channel = { ...ch };
      if (ch.loop !== undefined) {
        next.loop = ch.loop.flatMap((item, i) => {
          const out = change(item, { channel: ch.number, slot: -1, item: i }, home);
          return out === null ? [] : [out];
        });
      }
      if (ch.slot !== undefined) {
        next.slot = ch.slot.map((slot, s) => ({
          ...slot,
          items: slot.items.flatMap((item, i) => {
            const out = change(item, { channel: ch.number, slot: s, item: i }, home);
            return out === null ? [] : [out];
          }),
        }));
      }
      return next;
    }),
  };
}

export function removeItems(config: Config, refs: ItemRef[]): Config {
  const gone = new Set(refs.map((r) => `${r.channel}:${r.slot}:${r.item}`));
  return mapItems(config, (item, ref) => (gone.has(`${ref.channel}:${ref.slot}:${ref.item}`) ? null : item));
}

export function retargetEpisodes(config: Config, ref: ShowRef, moves: { from: [number, number]; to: [number, number] }[]): Config {
  const map = new Map(moves.map((m) => [`${m.from[0]}x${m.from[1]}`, m.to]));
  return mapItems(config, (item, _ref, home) => {
    if (item.type !== "episode" || !sameShow(itemShow(item, home), ref)) return item;
    const to = map.get(`${item.season}x${item.episode}`);
    return to === undefined ? item : { ...item, season: to[0], episode: to[1] };
  });
}

export function renameAd(config: Config, from: string, to: string): Config {
  return mapItems(config, (item) => (item.type === "ad" && item.path === from ? { ...item, path: to } : item));
}

export function appendItems(config: Config, channel: number, slot: number | null, items: Item[]): Config {
  return {
    ...config,
    channel: config.channel.map((ch) => {
      if (ch.number !== channel) return ch;
      if (slot === null) return { ...ch, loop: [...(ch.loop ?? []), ...items] };
      return { ...ch, slot: (ch.slot ?? []).map((s, i) => (i === slot ? { ...s, items: [...s.items, ...items] } : s)) };
    }),
  };
}

function code(season: number, episode: number): string {
  const e = `E${String(episode).padStart(2, "0")}`;
  return season >= 1 ? `S${String(season).padStart(2, "0")}${e}` : e;
}

export function describeItem(item: Item, home: string, showName: (ref: ShowRef) => string): string {
  switch (item.type) {
    case "ads":
      return `Ad break ×${item.count}`;
    case "ad":
      return `Ad · ${item.path.slice(item.path.lastIndexOf("/") + 1).replace(/\.mpg$/i, "")}`;
    case "episode":
      return `${code(item.season, item.episode)} · ${showName({ channel: item.from ?? home, show: item.show })}`;
    case "random":
      return `Random episode ×${item.count} · ${showName({ channel: item.from ?? home, show: item.show })}`;
    case "random-run":
      return `Random run ×${item.count} · ${showName({ channel: item.from ?? home, show: item.show })}`;
    case "fill":
      return `Fill · ${showName({ channel: item.from ?? home, show: item.show })}`;
  }
}
