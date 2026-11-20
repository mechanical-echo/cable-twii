import { lstat, rename, rm, rmdir } from "node:fs/promises";
import path from "node:path";
import { checkRename, mkdirContained } from "./contain.js";
import { seasonlessOrder } from "./library.js";
import { readOff, writeOff } from "./off.js";
import { episodeFileName, fatSafe } from "./names.js";
import { type ShowMeta, readShowMeta, writeShowMeta } from "./showMeta.js";
import { readSources, writeSources } from "./sources.js";
import { type ItemRef, type ShowRef, appendItems, channelFolder, channelsAiring, describeItem, itemsNaming, removeItems, renameAd, retargetEpisodes } from "./scheduleRefs.js";
import type { Config, Item, LibEpisode, LibShow, Library } from "./types.js";

export type EditAction =
  | { kind: "episode"; path: string; title: string; season: number | null; episode: number | null }
  | { kind: "ad"; path: string; name: string; channel: string }
  | { kind: "move-season"; paths: string[]; season: number }
  | { kind: "off"; paths: string[]; off: boolean }
  | { kind: "delete"; paths: string[] }
  | { kind: "delete-show"; channel: string; show: string }
  | { kind: "airs-on"; channel: string; show: string; number: number; on: boolean; slot: number | null }
  | { kind: "append"; number: number; slot: number | null; items: Item[] };

export interface EditPlan {
  moves: { from: string; to: string }[];
  deletes: string[];
  deleteDirs: string[];
  off: Set<string>;
  exclude: Set<string>;
  config: Config;
  removed: { channel: number; label: string }[];
  describeKeys: { showDir: string; from: string; to: string }[];
}

export interface EditContext {
  config: Config;
  library: Library;
  off: Set<string>;
  busy: Set<string>;
  showFiles: (ref: ShowRef) => string[];
}

export class EditRefused extends Error {
  constructor(
    readonly status: 400 | 409,
    message: string,
  ) {
    super(message);
  }
}

const BUSY = "Still converting. Try again when it's done.";
type Move = { from: [number, number]; to: [number, number] };

function code(season: number, episode: number): string {
  const e = `E${String(episode).padStart(2, "0")}`;
  return season >= 1 ? `S${String(season).padStart(2, "0")}${e}` : e;
}

function findEpisode(library: Library, file: string): { show: LibShow; episode: LibEpisode } | null {
  for (const show of library.show) {
    const episode = show.episode.find((e) => e.path === file);
    if (episode) return { show, episode };
  }
  return null;
}

function refOf(show: LibShow): ShowRef {
  return { channel: show.channel, show: show.folder };
}

function seasonDir(show: LibShow, season: number): string {
  const existing = show.episode.find((e) => e.season === season);
  return existing ? path.posix.dirname(existing.path) : `${show.channel}/${show.folder}/Season ${season}`;
}

function checkBusy(paths: string[], busy: Set<string>): void {
  if (paths.some((p) => busy.has(p))) throw new EditRefused(409, BUSY);
}

function wholeAtLeast(value: unknown, min: number): value is number {
  return typeof value === "number" && Number.isInteger(value) && value >= min;
}

function seasonlessMoves(before: string[], after: string[], renamed: Map<string, string>): Move[] {
  const moves: Move[] = [];
  before.forEach((file, i) => {
    const j = after.indexOf(renamed.get(file) ?? file);
    if (j >= 0 && j !== i) moves.push({ from: [0, i + 1], to: [0, j + 1] });
  });
  return moves;
}

function labels(ctx: EditContext, refs: ItemRef[]): { channel: number; label: string }[] {
  const name = (r: ShowRef) => ctx.library.show.find((s) => s.channel === r.channel && s.folder === r.show)?.name ?? r.show;
  return refs.flatMap((ref) => {
    const ch = ctx.config.channel.find((c) => c.number === ref.channel);
    const item = ref.slot < 0 ? ch?.loop?.[ref.item] : ch?.slot?.[ref.slot]?.items[ref.item];
    return ch && item ? [{ channel: ref.channel, label: describeItem(item, channelFolder(ch), name) }] : [];
  });
}

function directRefs(ctx: EditContext, paths: string[]): ItemRef[] {
  return paths.flatMap((p) => {
    const found = findEpisode(ctx.library, p);
    if (found) return itemsNaming(ctx.config, { show: refOf(found.show), season: found.episode.season, episode: found.episode.episode });
    return itemsNaming(ctx.config, { ad: p });
  });
}

function requireKnown(ctx: EditContext, paths: string[]): void {
  if (paths.length === 0) throw new EditRefused(400, "Pick at least one file.");
  for (const p of paths) {
    if (!findEpisode(ctx.library, p) && !ctx.library.ad.some((a) => a.path === p)) throw new EditRefused(400, `${p} is not in the library.`);
  }
}

function channelOf(ctx: EditContext, number: number) {
  const ch = ctx.config.channel.find((c) => c.number === number);
  if (!ch) throw new EditRefused(400, `No channel ${number}.`);
  return ch;
}

function checkSlot(ch: Config["channel"][number], slot: number | null): void {
  if (ch.mode === "looping") return;
  if (slot === null || !Number.isInteger(slot) || slot < 0 || slot >= (ch.slot?.length ?? 0)) throw new EditRefused(400, "Pick a slot.");
}

function withRemoved(ctx: EditContext, plan: EditPlan, refs: ItemRef[]): EditPlan {
  return { ...plan, config: removeItems(plan.config, refs), removed: [...plan.removed, ...labels(ctx, refs)] };
}

function planEpisode(action: Extract<EditAction, { kind: "episode" }>, ctx: EditContext, plan: EditPlan): EditPlan {
  const found = findEpisode(ctx.library, action.path);
  if (!found) throw new EditRefused(400, "That episode is not in the library.");
  checkBusy([action.path], ctx.busy);
  const { show, episode } = found;
  const ref = refOf(show);
  const showDir = `${show.channel}/${show.folder}`;
  const title = action.title.trim();
  if (title === "" || fatSafe(title) === "") throw new EditRefused(400, "Give it a title.");
  if (episode.season >= 1) {
    const season = action.season ?? episode.season;
    const number = action.episode ?? episode.episode;
    if (!wholeAtLeast(season, 1) || !wholeAtLeast(number, 1)) throw new EditRefused(400, "Season and episode must be whole numbers from 1.");
    const renumbered = season !== episode.season || number !== episode.episode;
    if (renumbered) {
      const taken = show.episode.find((e) => e.season === season && e.episode === number);
      if (taken) throw new EditRefused(409, `${code(season, number)} already exists: ${taken.title}`);
    }
    const to = `${seasonDir(show, season)}/${episodeFileName({ season, episode: number, title })}`;
    return {
      ...plan,
      moves: to === action.path ? [] : [{ from: action.path, to }],
      config: renumbered ? retargetEpisodes(plan.config, ref, [{ from: [episode.season, episode.episode], to: [season, number] }]) : plan.config,
      describeKeys: renumbered ? [{ showDir, from: code(episode.season, episode.episode), to: code(season, number) }] : [],
    };
  }
  const oldName = path.posix.basename(action.path);
  const stem = oldName.replace(/\.mpg$/i, "");
  const prefix = /^E\d+ - /i.exec(stem)?.[0] ?? "";
  const newName = `${prefix}${fatSafe(title)}.mpg`;
  if (newName === oldName) return plan;
  const files = ctx.showFiles(ref);
  if (files.some((f) => f !== oldName && f.toLowerCase() === newName.toLowerCase())) throw new EditRefused(409, `${newName} already exists.`);
  const before = seasonlessOrder(files);
  const after = seasonlessOrder(files.map((f) => (f === oldName ? newName : f)));
  return {
    ...plan,
    moves: [{ from: action.path, to: `${showDir}/${newName}` }],
    config: retargetEpisodes(plan.config, ref, seasonlessMoves(before, after, new Map([[oldName, newName]]))),
    describeKeys: [{ showDir, from: stem, to: newName.replace(/\.mpg$/i, "") }],
  };
}

function planAd(action: Extract<EditAction, { kind: "ad" }>, ctx: EditContext, plan: EditPlan): EditPlan {
  if (!ctx.library.ad.some((a) => a.path === action.path)) throw new EditRefused(400, "That ad is not in the library.");
  checkBusy([action.path], ctx.busy);
  const name = fatSafe(action.name.trim());
  if (name === "") throw new EditRefused(400, "Give it a name.");
  if (/[/\\]/.test(action.channel) || action.channel === "." || action.channel === "..") throw new EditRefused(400, "Pick a channel.");
  const to = `${action.channel === "" ? "ads" : `${action.channel}/ads`}/${name}.mpg`;
  if (to === action.path) return plan;
  if (ctx.library.ad.some((a) => a.path !== action.path && a.path.toLowerCase() === to.toLowerCase())) throw new EditRefused(409, `${name}.mpg already exists there.`);
  return { ...plan, moves: [{ from: action.path, to }], config: renameAd(plan.config, action.path, to) };
}

function planMoveSeason(action: Extract<EditAction, { kind: "move-season" }>, ctx: EditContext, plan: EditPlan): EditPlan {
  if (!wholeAtLeast(action.season, 1)) throw new EditRefused(400, "Pick a season from 1.");
  requireKnown(ctx, action.paths);
  const found = action.paths.map((p) => findEpisode(ctx.library, p));
  if (found.some((f) => f === null)) throw new EditRefused(400, "Only episodes can move to a season.");
  const show = found[0]!.show;
  if (found.some((f) => f!.show !== show)) throw new EditRefused(400, "Pick episodes of one show.");
  checkBusy(action.paths, ctx.busy);
  const ref = refOf(show);
  const showDir = `${show.channel}/${show.folder}`;
  const moving = new Set(action.paths);
  let last = Math.max(0, ...show.episode.filter((e) => e.season === action.season && !moving.has(e.path)).map((e) => e.episode));
  const dir = seasonDir(show, action.season);
  const moves: { from: string; to: string }[] = [];
  const numbers: Move[] = [];
  const keys: EditPlan["describeKeys"] = [];
  for (const f of found) {
    const ep = f!.episode;
    last += 1;
    moves.push({ from: ep.path, to: `${dir}/${episodeFileName({ season: action.season, episode: last, title: ep.title })}` });
    numbers.push({ from: [ep.season, ep.episode], to: [action.season, last] });
    keys.push({ showDir, from: ep.season >= 1 ? code(ep.season, ep.episode) : path.posix.basename(ep.path).replace(/\.mpg$/i, ""), to: code(action.season, last) });
  }
  const movedNames = new Set(found.filter((f) => f!.episode.season === 0).map((f) => path.posix.basename(f!.episode.path)));
  if (movedNames.size > 0) {
    const before = seasonlessOrder(ctx.showFiles(ref));
    const after = before.filter((f) => !movedNames.has(f));
    numbers.push(...seasonlessMoves(before, after, new Map()));
  }
  return { ...plan, moves, config: retargetEpisodes(plan.config, ref, numbers), describeKeys: keys };
}

export function planEdit(action: EditAction, ctx: EditContext): EditPlan {
  let plan: EditPlan = { moves: [], deletes: [], deleteDirs: [], off: new Set(ctx.off), exclude: new Set(ctx.off), config: ctx.config, removed: [], describeKeys: [] };
  switch (action.kind) {
    case "episode":
      plan = planEpisode(action, ctx, plan);
      break;
    case "ad":
      plan = planAd(action, ctx, plan);
      break;
    case "move-season":
      plan = planMoveSeason(action, ctx, plan);
      break;
    case "off": {
      requireKnown(ctx, action.paths);
      checkBusy(action.paths, ctx.busy);
      for (const p of action.paths) {
        if (action.off) plan.off.add(p);
        else plan.off.delete(p);
      }
      plan.exclude = new Set(plan.off);
      if (action.off) plan = withRemoved(ctx, plan, directRefs(ctx, action.paths));
      break;
    }
    case "delete": {
      requireKnown(ctx, action.paths);
      checkBusy(action.paths, ctx.busy);
      plan = withRemoved(ctx, { ...plan, deletes: [...action.paths] }, directRefs(ctx, action.paths));
      for (const p of action.paths) plan.off.delete(p);
      plan.exclude = new Set([...plan.off, ...action.paths]);
      break;
    }
    case "delete-show": {
      const show = ctx.library.show.find((s) => s.channel === action.channel && s.folder === action.show);
      if (!show) throw new EditRefused(400, "That show is not in the library.");
      const paths = show.episode.map((e) => e.path);
      checkBusy(paths, ctx.busy);
      plan = withRemoved(ctx, { ...plan, deletes: paths, deleteDirs: [`${show.channel}/${show.folder}`] }, itemsNaming(ctx.config, { show: refOf(show) }));
      for (const p of paths) plan.off.delete(p);
      plan.exclude = new Set([...plan.off, ...paths]);
      break;
    }
    case "airs-on": {
      const ch = channelOf(ctx, action.number);
      const ref = { channel: action.channel, show: action.show };
      if (action.on) {
        if (channelsAiring(ctx.config, ref).includes(action.number)) break;
        checkSlot(ch, action.slot);
        const item: Item = { type: "random", show: action.show, count: 1, ...(action.channel !== channelFolder(ch) ? { from: action.channel } : {}) };
        plan.config = appendItems(plan.config, action.number, ch.mode === "looping" ? null : action.slot, [item]);
      } else {
        plan = withRemoved(ctx, plan, itemsNaming(ctx.config, { show: ref }).filter((r) => r.channel === action.number));
      }
      break;
    }
    case "append": {
      const ch = channelOf(ctx, action.number);
      checkSlot(ch, action.slot);
      plan.config = appendItems(plan.config, action.number, ch.mode === "looping" ? null : action.slot, action.items);
      break;
    }
  }
  if (plan.moves.length > 0) {
    const moved = new Map(plan.moves.map((m) => [m.from, m.to]));
    plan.off = new Set([...plan.off].map((p) => moved.get(p) ?? p));
    plan.exclude = new Set([...plan.exclude].map((p) => moved.get(p) ?? p));
  }
  return plan;
}

async function exists(file: string): Promise<boolean> {
  try {
    await lstat(file);
    return true;
  } catch {
    return false;
  }
}

function abs(root: string, rel: string): string {
  return path.join(root, ...rel.split("/"));
}

export async function applyPlan(root: string, plan: EditPlan): Promise<() => Promise<void>> {
  const undo: (() => Promise<void>)[] = [];
  const rollback = async () => {
    for (const step of undo.reverse()) await step();
    undo.length = 0;
  };
  try {
    for (const move of plan.moves) {
      const from = abs(root, move.from);
      const to = abs(root, move.to);
      if (await exists(to)) throw new Error(`${move.to} already exists.`);
      const dir = path.dirname(to);
      const madeDir = !(await exists(dir));
      mkdirContained(root, dir);
      checkRename(root, from, to);
      await rename(from, to);
      undo.push(async () => {
        await rename(to, from);
        if (madeDir) await rmdir(dir).catch(() => undefined);
      });
    }
    if (plan.moves.length > 0) {
      const moved = new Map(plan.moves.map((m) => [m.from, m.to]));
      const records = await readSources(root);
      if (records.some((r) => moved.has(r.target))) {
        await writeSources(root, records.map((r) => ({ ...r, target: moved.get(r.target) ?? r.target })));
        undo.push(() => writeSources(root, records));
      }
    }
    const off = await readOff(root);
    if ([...off].sort().join("\n") !== [...plan.off].sort().join("\n")) {
      const offFile = abs(root, ".studio/off.toml");
      const hadFile = await exists(offFile);
      await writeOff(root, plan.off);
      undo.push(async () => (hadFile ? writeOff(root, off) : rm(offFile, { force: true })));
    }
    const metas = new Map<string, ShowMeta>();
    for (const key of plan.describeKeys) {
      const showDir = abs(root, key.showDir);
      const meta = await readShowMeta(showDir);
      if (!metas.has(key.showDir)) {
        metas.set(key.showDir, meta);
        undo.push(() => writeShowMeta(root, showDir, metas.get(key.showDir)!));
      }
      const text = meta.episodes[key.from];
      if (text === undefined) continue;
      const episodes = { ...meta.episodes, [key.to]: text };
      delete episodes[key.from];
      await writeShowMeta(root, showDir, { ...meta, episodes });
    }
  } catch (err) {
    await rollback();
    throw err;
  }
  return rollback;
}
