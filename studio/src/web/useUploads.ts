import { useCallback, useEffect, useRef, useState } from "react";
import { type ImportEpisode, type ImportResult, type ScanResult, discardUploaded, importAds, importShow, uploadIcon, uploadVideos } from "./api";
import type { AddRow } from "./adding";
import type { UploadTask } from "./episodes";
import { UPLOAD_BATCH_FILES, batches, spreadLoaded, storedUploadName } from "./uploading";

export type AddPlan =
  | { kind: "show"; channelFolder: string; showFolder: string; showName: string; icon: File | null; rows: AddRow[] }
  | { kind: "ads"; channelFolder: string; rows: AddRow[] };

export type AddStart = { files: File[] } | { from: "imports" | "pick" };

let handedOff: AddStart | null = null;

export function handOff(start: AddStart): void {
  handedOff = start;
}

export function takeHandOff(): AddStart | null {
  const start = handedOff;
  handedOff = null;
  return start;
}

export interface Uploads {
  tasks: UploadTask[];
  start: (plans: AddPlan[]) => void;
  dismiss: (id: string) => void;
}

const NOT_ADDED = "could not be added";
let nextTask = 0;

function baseName(source: string): string {
  return source.slice(Math.max(source.lastIndexOf("/"), source.lastIndexOf("\\")) + 1);
}

function episodeOf(row: AddRow, source: string, aspect: "16:9" | "4:3"): ImportEpisode {
  return {
    source,
    season: row.season,
    episode: row.episode,
    title: row.title.trim(),
    description: row.description.trim(),
    aspect,
    ...(row.replaces !== null ? { replaces: row.replaces } : {}),
  };
}

function importPlan(plan: AddPlan, pairs: { row: AddRow; source: string; aspect: "16:9" | "4:3" }[]): Promise<ImportResult> {
  if (plan.kind === "ads") return importAds({ channelFolder: plan.channelFolder, files: pairs.map((p) => ({ source: p.source, aspect: p.aspect })) });
  return importShow({ channelFolder: plan.channelFolder, showFolder: plan.showFolder, showName: plan.showName, episodes: pairs.map((p) => episodeOf(p.row, p.source, p.aspect)) });
}

export function useUploads(): Uploads {
  const [tasks, setTasks] = useState<UploadTask[]>([]);
  const chain = useRef<Promise<void>>(Promise.resolve());

  const patch = useCallback((ids: string[], change: (task: UploadTask) => Partial<UploadTask> | null) => {
    const wanted = new Set(ids);
    setTasks((prev) =>
      prev.flatMap((task) => {
        if (!wanted.has(task.id)) return [task];
        const next = change(task);
        return next === null ? [] : [{ ...task, ...next }];
      }),
    );
  }, []);

  const busy = tasks.some((t) => t.state === "waiting" || t.state === "uploading" || t.state === "importing");
  useEffect(() => {
    if (!busy) return;
    const warn = (event: BeforeUnloadEvent) => event.preventDefault();
    window.addEventListener("beforeunload", warn);
    return () => window.removeEventListener("beforeunload", warn);
  }, [busy]);

  const run = useCallback(
    async (plan: AddPlan, entries: { id: string; row: AddRow }[]) => {
      let iconSent = plan.kind !== "show" || plan.icon === null;
      const finish = async (pairs: { id: string; row: AddRow; source: string; aspect: "16:9" | "4:3" }[], uploaded: boolean) => {
        if (pairs.length === 0) return;
        patch(pairs.map((p) => p.id), () => ({ state: "importing" }));
        let result: ImportResult;
        try {
          result = await importPlan(plan, pairs);
        } catch (err) {
          patch(pairs.map((p) => p.id), () => ({ state: "rejected", reason: (err as Error).message }));
          if (uploaded) await discardUploaded(pairs.map((p) => p.source)).catch(() => undefined);
          return;
        }
        const refused = new Map(result.rejected.map((r) => [r.source, r.reason]));
        for (const pair of pairs) {
          const reason = refused.get(pair.source);
          patch([pair.id], () => (reason === undefined ? null : { state: "rejected", reason }));
        }
        if (uploaded && refused.size > 0) await discardUploaded([...refused.keys()]).catch(() => undefined);
        if (!iconSent && plan.kind === "show" && plan.icon !== null && result.jobs.length > 0) {
          iconSent = true;
          await uploadIcon("show", plan.channelFolder, plan.showFolder, plan.icon).catch(() => undefined);
        }
      };

      await finish(
        entries.filter((e) => e.row.source !== null).map((e) => ({ ...e, source: e.row.source!, aspect: e.row.aspect ?? "16:9" })),
        false,
      );

      for (const batch of batches(entries.filter((e) => e.row.file !== null), UPLOAD_BATCH_FILES)) {
        const ids = batch.map((e) => e.id);
        const sizes = batch.map((e) => e.row.file!.size);
        const total = sizes.reduce((sum, size) => sum + size, 0);
        patch(ids, () => ({ state: "uploading", loaded: 0 }));
        let files: ScanResult[];
        let rejected: { name: string; reason: string }[];
        try {
          const result = await uploadVideos(
            batch.map((e) => e.row.file!),
            (ratio) => {
              const loaded = spreadLoaded(sizes, ratio * total);
              setTasks((prev) => prev.map((task) => (ids.includes(task.id) ? { ...task, loaded: loaded[ids.indexOf(task.id)] ?? task.loaded } : task)));
            },
          );
          files = result.files;
          rejected = result.rejected;
        } catch (err) {
          patch(ids, () => ({ state: "rejected", reason: (err as Error).message }));
          continue;
        }
        const byName = new Map(files.map((f) => [baseName(f.source).toLowerCase(), f]));
        const refused = new Map(rejected.map((r) => [r.name, r.reason]));
        const pairs: { id: string; row: AddRow; source: string; aspect: "16:9" | "4:3" }[] = [];
        for (const entry of batch) {
          const scan = byName.get(storedUploadName(entry.row.name).toLowerCase());
          if (scan !== undefined) {
            pairs.push({ ...entry, source: scan.source, aspect: scan.aspect });
            continue;
          }
          patch([entry.id], (task) => ({ state: "rejected", reason: refused.get(entry.row.name) ?? NOT_ADDED, loaded: task.size }));
        }
        await finish(pairs, true);
      }
    },
    [patch],
  );

  const start = useCallback(
    (plans: AddPlan[]) => {
      const queued = plans.map((plan) => {
        const entries = plan.rows
          .filter((row) => row.ticked)
          .map((row) => {
            nextTask += 1;
            return { id: `t${nextTask}`, row };
          });
        return { plan, entries };
      });
      const fresh: UploadTask[] = queued.flatMap(({ entries }) =>
        entries.map(({ id, row }) => ({ id, name: row.name, size: row.size ?? 0, loaded: 0, state: row.file !== null ? "waiting" : "importing", reason: null })),
      );
      setTasks((prev) => [...prev, ...fresh]);
      for (const { plan, entries } of queued) {
        chain.current = chain.current.then(() => run(plan, entries)).catch(() => undefined);
      }
    },
    [run],
  );

  const dismiss = useCallback((id: string) => setTasks((prev) => prev.filter((t) => t.id !== id)), []);

  return { tasks, start, dismiss };
}
