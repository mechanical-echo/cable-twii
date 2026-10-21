import { randomInt } from "node:crypto";
import { readFile, stat } from "node:fs/promises";
import path from "node:path";
import { parse } from "smol-toml";
import { atomicWriteFile } from "./sources.js";
import { tomlInt, tomlString } from "./toml.js";
import type { Channel, Config, Item, Mode, Slot } from "./types.js";

const CONFIG_FILE = "config.toml";
const BACKUP_FILE = "config.bak.toml";
const SEED_LIMIT = 2 ** 31;
const ITEM_TYPES = new Set(["episode", "random", "random-run", "ads", "ad", "fill"]);

export class ConfigConflictError extends Error {
  constructor() {
    super("config.toml was changed somewhere else. Reload it and try again.");
    this.name = "ConfigConflictError";
  }
}

export class ConfigShapeError extends Error {
  constructor(message: string) {
    super(message);
    this.name = "ConfigShapeError";
  }
}

type Raw = Record<string, unknown>;

function fail(where: string, message: string): never {
  throw new ConfigShapeError(`config.toml: ${where}${message}`);
}

function table(value: unknown, where: string): Raw {
  if (typeof value !== "object" || value === null || Array.isArray(value)) fail(where, "must be a table");
  return value as Raw;
}

function list(value: unknown, where: string): unknown[] {
  if (!Array.isArray(value)) fail(where, "must be an array");
  return value;
}

function int(t: Raw, key: string, where: string): number {
  const v = t[key];
  if (typeof v !== "number" || !Number.isSafeInteger(v)) fail(where, `${key} must be an integer`);
  return v;
}

function optInt(t: Raw, key: string, where: string): number | undefined {
  return t[key] === undefined ? undefined : int(t, key, where);
}

function str(t: Raw, key: string, where: string): string {
  const v = t[key];
  if (typeof v !== "string") fail(where, `${key} must be a string`);
  return v;
}

function optStr(t: Raw, key: string, where: string): string | undefined {
  return t[key] === undefined ? undefined : str(t, key, where);
}

function withOptional<T extends object>(base: T, extra: Record<string, unknown>): T {
  const out: Record<string, unknown> = { ...(base as Record<string, unknown>) };
  for (const [k, v] of Object.entries(extra)) if (v !== undefined) out[k] = v;
  return out as T;
}

function toItem(raw: unknown, where: string): Item {
  const t = table(raw, where);
  const type = str(t, "type", where);
  if (!ITEM_TYPES.has(type)) fail(where, `unknown item type "${type}"`);
  switch (type) {
    case "episode":
      return withOptional(
        { type, show: str(t, "show", where), season: int(t, "season", where), episode: int(t, "episode", where) },
        { from: optStr(t, "from", where) },
      );
    case "random":
    case "random-run":
      return withOptional(
        { type, show: str(t, "show", where), count: optInt(t, "count", where) ?? 1 } as Item,
        { season: optInt(t, "season", where), from: optStr(t, "from", where) },
      );
    case "ads":
      return { type, count: optInt(t, "count", where) ?? 1 };
    case "ad":
      return { type, path: str(t, "path", where) };
    default:
      return withOptional(
        { type: "fill", show: str(t, "show", where) } as Item,
        { season: optInt(t, "season", where), ads_between: optInt(t, "ads_between", where), from: optStr(t, "from", where) },
      );
  }
}

function toItems(raw: unknown, where: string): Item[] {
  return list(raw, where).map((item, i) => toItem(item, `${where}item ${i + 1}: `));
}

function toSlot(raw: unknown, where: string): Slot {
  const t = table(raw, where);
  return { at: str(t, "at", where), items: toItems(t.items, where) };
}

function toChannel(raw: unknown, where: string): Channel {
  const t = table(raw, where);
  const number = int(t, "number", where);
  const name = str(t, "name", where);
  const category = str(t, "category", where);
  const mode = str(t, "mode", where);
  if (mode !== "timed" && mode !== "looping") fail(where, `unknown mode "${mode}"`);
  const ads = t.ads;
  if (typeof ads !== "boolean") fail(where, "ads must be true or false");
  const channel = withOptional<Channel>(
    { number, name, category, ads, mode: mode as Mode },
    { folder: optStr(t, "folder", where), icon: optStr(t, "icon", where) },
  );
  if (t.slot !== undefined) channel.slot = list(t.slot, where).map((s, i) => toSlot(s, `${where}slot ${i + 1}: `));
  if (t.loop !== undefined) channel.loop = toItems(t.loop, `${where}loop `);
  return channel;
}

export function toConfig(raw: unknown): Config {
  const t = table(raw, "");
  if (t.version !== 1) fail("", "unsupported version (expected 1)");
  const seed = int(t, "seed", "");
  const channels = t.channel === undefined ? [] : list(t.channel, "channel ");
  return { version: 1, seed, channel: channels.map((c, i) => toChannel(c, `channel ${i + 1}: `)) };
}

export function parseConfig(text: string): Config {
  let doc: Raw;
  try {
    doc = parse(text);
  } catch (err) {
    throw new ConfigShapeError(`config.toml is not valid TOML: ${(err as Error).message}`);
  }
  return toConfig(doc);
}

function itemLine(item: Item): string {
  const parts: string[] = [`type = ${tomlString(item.type)}`];
  const add = (key: string, value: string | number | undefined, kind: "s" | "i") => {
    if (value === undefined) return;
    parts.push(`${key} = ${kind === "s" ? tomlString(value as string) : tomlInt(value as number, key)}`);
  };
  switch (item.type) {
    case "episode":
      add("show", item.show, "s");
      add("season", item.season, "i");
      add("episode", item.episode, "i");
      add("from", item.from, "s");
      break;
    case "random":
    case "random-run":
      add("show", item.show, "s");
      add("season", item.season, "i");
      add("count", item.count, "i");
      add("from", item.from, "s");
      break;
    case "ads":
      add("count", item.count, "i");
      break;
    case "ad":
      add("path", item.path, "s");
      break;
    case "fill":
      add("show", item.show, "s");
      add("season", item.season, "i");
      add("ads_between", item.ads_between, "i");
      add("from", item.from, "s");
      break;
  }
  return `{ ${parts.join(", ")} }`;
}

function itemArray(key: string, items: Item[], indent: string): string[] {
  if (items.length === 0) return [`${indent}${key} = []`];
  return [`${indent}${key} = [`, ...items.map((item) => `${indent}  ${itemLine(item)},`), `${indent}]`];
}

export function serializeConfig(cfg: Config): string {
  const out: string[] = ["version = 1", `seed = ${tomlInt(cfg.seed, "seed")}`];
  const channels = [...cfg.channel].sort((a, b) => a.number - b.number);
  for (const ch of channels) {
    out.push("", "[[channel]]", `number = ${tomlInt(ch.number, "number")}`, `name = ${tomlString(ch.name)}`, `category = ${tomlString(ch.category)}`);
    if (ch.folder !== undefined) out.push(`folder = ${tomlString(ch.folder)}`);
    if (ch.icon !== undefined) out.push(`icon = ${tomlString(ch.icon)}`);
    out.push(`ads = ${ch.ads ? "true" : "false"}`, `mode = ${tomlString(ch.mode)}`);
    if (ch.mode === "timed") {
      const slots = [...(ch.slot ?? [])].sort((a, b) => (a.at < b.at ? -1 : a.at > b.at ? 1 : 0));
      for (const slot of slots) {
        out.push("", "  [[channel.slot]]", `  at = ${tomlString(slot.at)}`, ...itemArray("items", slot.items, "  "));
      }
    } else {
      out.push(...itemArray("loop", ch.loop ?? [], ""));
    }
  }
  return `${out.join("\n")}\n`;
}

export function randomSeed(): number {
  return randomInt(0, SEED_LIMIT);
}

function configPath(root: string): string {
  return path.join(root, CONFIG_FILE);
}

export async function readConfig(root: string): Promise<{ config: Config; mtimeMs: number }> {
  const file = configPath(root);
  let text: string;
  let mtimeMs: number;
  try {
    mtimeMs = Math.trunc((await stat(file)).mtimeMs);
    text = await readFile(file, "utf8");
  } catch (err) {
    if ((err as NodeJS.ErrnoException).code === "ENOENT") return { config: { version: 1, seed: randomSeed(), channel: [] }, mtimeMs: 0 };
    throw err;
  }
  return { config: parseConfig(text), mtimeMs };
}

export async function writeConfig(root: string, cfg: Config, expectedMtimeMs: number): Promise<number> {
  const file = configPath(root);
  const text = serializeConfig(cfg);
  let current = 0;
  let previous: string | null = null;
  try {
    current = Math.trunc((await stat(file)).mtimeMs);
    previous = await readFile(file, "utf8");
  } catch (err) {
    if ((err as NodeJS.ErrnoException).code !== "ENOENT") throw err;
  }
  if (current !== Math.trunc(expectedMtimeMs)) throw new ConfigConflictError();
  if (previous !== null) await atomicWriteFile(path.join(root, BACKUP_FILE), previous);
  await atomicWriteFile(file, text);
  return Math.trunc((await stat(file)).mtimeMs);
}
