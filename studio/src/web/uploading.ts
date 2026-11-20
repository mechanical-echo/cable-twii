import { fatSafe } from "../server/names.js";

export const UPLOAD_BATCH_FILES = 50;

export function uploadRatio(loaded: number, total: number): number {
  if (!(total > 0)) return 0;
  return Math.min(1, Math.max(0, loaded / total));
}

export function joinRel(rel: string, name: string): string {
  return rel === "" ? name : `${rel}/${name}`;
}

export function crumbs(rel: string): { name: string; rel: string }[] {
  const out = [{ name: "imports", rel: "" }];
  for (const part of rel.split("/")) {
    if (part !== "") out.push({ name: part, rel: joinRel(out[out.length - 1]!.rel, part) });
  }
  return out;
}

export function batches<T>(items: readonly T[], size: number): T[][] {
  const out: T[][] = [];
  for (let i = 0; i < items.length; i += size) out.push(items.slice(i, i + size));
  return out;
}

export function rejectedLine(rejected: { name: string; reason: string }): string {
  return `${rejected.name}: ${rejected.reason.split("\n")[0]}`;
}

export function tickedFiles<T>(files: T[], ticked: boolean[]): T[] {
  return files.filter((_, i) => ticked[i] === true);
}

export function uploadGroups(sources: string[]): { dir: string; names: string[] }[] {
  const groups = new Map<string, string[]>();
  for (const source of sources) {
    const cut = Math.max(source.lastIndexOf("/"), source.lastIndexOf("\\"));
    const dir = source.slice(0, cut);
    groups.set(dir, [...(groups.get(dir) ?? []), source.slice(cut + 1)]);
  }
  return [...groups].map(([dir, names]) => ({ dir, names }));
}

export function storedUploadName(name: string): string {
  const dot = name.lastIndexOf(".");
  return dot > 0 ? `${fatSafe(name.slice(0, dot))}${fatSafe(name.slice(dot))}` : fatSafe(name);
}

export function spreadLoaded(sizes: number[], loaded: number): number[] {
  let left = Math.max(0, loaded);
  return sizes.map((size) => {
    const part = Math.min(size, left);
    left -= part;
    return part;
  });
}
