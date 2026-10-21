import type { Channel, Config, Mode } from "../server/types.js";
import type { WebLibrary } from "./tree.js";

export interface ChannelDraft {
  number: string;
  name: string;
  category: string;
  folder: string;
  ads: boolean;
  mode: Mode;
  slot: NonNullable<Channel["slot"]>;
  loop: NonNullable<Channel["loop"]>;
  icon?: string;
}

export const MIN_NUMBER = 1;
export const MAX_NUMBER = 999;

export function seedSlot(): NonNullable<Channel["slot"]> {
  return [{ at: "00:00", items: [] }];
}

export function seedLoop(): NonNullable<Channel["loop"]> {
  return [{ type: "ads", count: 1 }];
}

export function sortChannels(channels: Channel[]): Channel[] {
  return [...channels].sort((a, b) => a.number - b.number);
}

export function effectiveFolder(channel: Pick<Channel, "name" | "folder">): string {
  return channel.folder !== undefined && channel.folder !== "" ? channel.folder : channel.name;
}

export function iconPath(channel: Pick<Channel, "name" | "folder" | "icon">): string {
  return channel.icon ?? `${effectiveFolder(channel)}/channel.png`;
}

export function nextFreeNumber(channels: Channel[]): number {
  const used = new Set(channels.map((c) => c.number));
  for (let n = MIN_NUMBER; n <= MAX_NUMBER; n++) if (!used.has(n)) return n;
  return MAX_NUMBER;
}

export function newDraft(channels: Channel[]): ChannelDraft {
  return {
    number: String(nextFreeNumber(channels)),
    name: "",
    category: "",
    folder: "",
    ads: true,
    mode: "timed",
    slot: seedSlot(),
    loop: seedLoop()
  };
}

export function draftFromChannel(channel: Channel): ChannelDraft {
  const draft: ChannelDraft = {
    number: String(channel.number),
    name: channel.name,
    category: channel.category,
    folder: channel.folder ?? "",
    ads: channel.ads,
    mode: channel.mode,
    slot: channel.slot !== undefined && channel.slot.length > 0 ? channel.slot : seedSlot(),
    loop: channel.loop !== undefined && channel.loop.length > 0 ? channel.loop : seedLoop()
  };
  if (channel.icon !== undefined) draft.icon = channel.icon;
  return draft;
}

export function draftFolder(draft: Pick<ChannelDraft, "name" | "folder">): string {
  const folder = draft.folder.trim();
  return folder !== "" ? folder : draft.name.trim();
}

export function setMode(draft: ChannelDraft, mode: Mode): ChannelDraft {
  const next = { ...draft, mode };
  if (mode === "timed" && next.slot.length === 0) next.slot = seedSlot();
  if (mode === "looping" && next.loop.length === 0) next.loop = seedLoop();
  return next;
}

export function validateDraft(draft: ChannelDraft, channels: Channel[], editing: number | null): string[] {
  const errors: string[] = [];
  const raw = draft.number.trim();
  const number = Number(raw);
  if (raw === "" || !Number.isInteger(number) || number < MIN_NUMBER || number > MAX_NUMBER) {
    errors.push(`Number must be a whole number from ${MIN_NUMBER} to ${MAX_NUMBER}.`);
  } else if (channels.some((c) => c.number === number && c.number !== editing)) {
    errors.push(`Channel number ${number} is already used.`);
  }
  if (draft.name.trim() === "") errors.push("Name is required.");
  if (draft.category.trim() === "") errors.push("Category is required.");
  const folder = draftFolder(draft);
  if (folder !== "" && /[\\/]|^\.\.?$/.test(folder)) errors.push("Folder must be a single folder name.");
  return errors;
}

export function toChannel(draft: ChannelDraft): Channel {
  const name = draft.name.trim();
  const folder = draftFolder(draft);
  const channel: Channel = {
    number: Number(draft.number.trim()),
    name,
    category: draft.category.trim(),
    ads: draft.ads,
    mode: draft.mode
  };
  if (folder !== name) channel.folder = folder;
  if (draft.icon !== undefined) channel.icon = draft.icon;
  if (draft.mode === "timed") channel.slot = draft.slot;
  else channel.loop = draft.loop;
  return channel;
}

export function applyEdit(config: Config, editing: number | null, draft: ChannelDraft): Config {
  const channel = toChannel(draft);
  const rest = config.channel.filter((c) => c.number !== editing);
  return { ...config, channel: sortChannels([...rest, channel]) };
}

export function applyDelete(config: Config, number: number): Config {
  return { ...config, channel: config.channel.filter((c) => c.number !== number) };
}

export function folderSuggestions(library: Pick<WebLibrary, "show" | "ad">): string[] {
  const found = new Set<string>();
  for (const show of library.show) if (show.channel !== "") found.add(show.channel);
  for (const ad of library.ad) if (ad.channel !== "") found.add(ad.channel);
  return [...found].sort((a, b) => a.localeCompare(b));
}
