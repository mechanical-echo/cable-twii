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
