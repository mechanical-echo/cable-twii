import { randomUUID } from "node:crypto";
import { EventEmitter } from "node:events";
import { readFileSync, realpathSync, renameSync, statSync } from "node:fs";
import { stat } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { parse, stringify } from "smol-toml";
import { convert, probe } from "./ffmpeg.js";
import { assertContained, atomicWriteContained, checkRename, mkdirContained, prepareScratchFile } from "./contain.js";
import { upsertSource } from "./sources.js";
import type { Job, SourceRecord } from "./types.js";

const PROGRESS_INTERVAL_MS = 250;

export interface QueueOptions {
  concurrency?: number;
  convertFn?: typeof convert;
  probeFn?: typeof probe;
}

type Aspect = "16:9" | "4:3";

function normaliseTarget(root: string, target: string): string {
  const rel = path.relative(root, path.resolve(root, target.replace(/\\/g, "/"))).split(path.sep).join("/");
  const first = rel.split("/")[0] ?? "";
  if (
    rel === "" || rel === "." || rel === ".." || rel.startsWith("../") || path.isAbsolute(rel) ||
    first.toLowerCase() === ".studio" || !rel.toLowerCase().endsWith(".mpg")
  ) {
    throw new Error(`invalid target: ${target}`);
  }
  return rel;
}

function defaultConcurrency(): number {
  const override = Number(process.env.CTW_CONVERT_CONCURRENCY);
  if (Number.isInteger(override) && override > 0) return override;
  const half = Math.max(1, Math.floor(os.cpus().length / 2));
  // Spawning many ffmpeg/ffprobe processes at once can hit a DLL-init race on Windows
  // (STATUS_DLL_INIT_FAILED), so keep default parallelism modest there.
  return process.platform === "win32" ? Math.min(half, 4) : half;
}

export class ConversionQueue extends EventEmitter {
  private readonly jobs = new Map<string, Job>();
  private readonly controllers = new Map<string, AbortController>();
  private readonly active = new Set<Promise<void>>();
  private readonly lastProgress = new Map<string, number>();
  private readonly concurrency: number;
  private readonly convertFn: typeof convert;
  private readonly probeFn: typeof probe;
  private started = false;
  private stopping = false;
  private paused = false;
  private writeChain: Promise<void> = Promise.resolve();
  private stopPromise: Promise<void> | undefined;
  private loadErrors: Error[] = [];

  constructor(private readonly root: string, opts: QueueOptions = {}) {
    super();
    this.on("error", () => undefined);
    this.concurrency = Math.max(1, opts.concurrency ?? defaultConcurrency());
    this.convertFn = opts.convertFn ?? convert;
    this.probeFn = opts.probeFn ?? probe;
    this.load();
    const pending = this.loadErrors;
    queueMicrotask(() => pending.forEach((e) => this.emit("error", e)));
  }

  private queueFile(): string {
    return path.join(this.root, ".studio", "queue.toml");
  }

  private load(): void {
    const file = this.queueFile();
    const dropped: string[] = [];
    let text: string;
    try {
      text = readFileSync(file, "utf8");
    } catch {
      return;
    }
    try {
      const doc = parse(text);
      if (doc.version !== 1 || !Array.isArray(doc.job)) throw new Error("unsupported queue file");
      const seenTargets = new Set<string>();
      for (const raw of doc.job as Record<string, unknown>[]) {
        const label = `${String(raw.id)} (${String(raw.target)})`;
        if (
          typeof raw.id !== "string" || typeof raw.source !== "string" || typeof raw.target !== "string" ||
          (raw.aspect !== "16:9" && raw.aspect !== "4:3")
        ) {
          dropped.push(`dropped invalid job ${label}`);
          continue;
        }
        let rel: string;
        try {
          rel = normaliseTarget(this.root, raw.target);
        } catch {
          dropped.push(`dropped job ${label}: invalid target`);
          continue;
        }
        if (this.jobs.has(raw.id) || seenTargets.has(rel)) {
          dropped.push(`dropped duplicate job ${label}`);
          continue;
        }
        seenTargets.add(rel);
        this.jobs.set(raw.id, { id: raw.id, source: raw.source, target: rel, aspect: raw.aspect, state: "queued", progress: 0 });
      }
      if (dropped.length > 0) {
        this.loadErrors = dropped.map((m) => new Error(m));
        this.persist();
      }
    } catch {
      this.jobs.clear();
      try {
        assertContained(this.root, path.dirname(file));
        renameSync(file, `${file}.damaged`);
      } catch {
        this.loadErrors = [new Error("queue.toml is damaged and could not be moved aside; its conversions were dropped")];
        return;
      }
      this.loadErrors = [new Error("queue.toml was damaged and was moved to queue.toml.damaged; its conversions were dropped")];
    }
  }

  enqueue(source: string, target: string, aspect: Aspect): Job {
    const rel = normaliseTarget(this.root, target);
    for (const existing of this.jobs.values()) {
      if (existing.target === rel && (existing.state === "queued" || existing.state === "running")) return { ...existing };
    }
    const job: Job = { id: randomUUID(), source, target: rel, aspect, state: "queued", progress: 0 };
    this.jobs.set(job.id, job);
    this.persist();
    this.emitChange(job);
    this.pump();
    return { ...job };
  }

  list(): Job[] {
    return [...this.jobs.values()].map((j) => ({ ...j }));
  }

  isPaused(): boolean {
    return this.paused;
  }

  resume(): void {
    if (!this.paused || !this.reachable()) return;
    this.paused = false;
    this.persist();
    this.pump();
  }

  retryFailed(): Job[] {
    const active = new Set<string>();
    for (const job of this.jobs.values()) if (job.state === "queued" || job.state === "running") active.add(job.target);
    const retried: Job[] = [];
    for (const job of this.jobs.values()) {
      if (job.state !== "failed" || active.has(job.target)) continue;
      job.state = "queued";
      job.progress = 0;
      delete job.error;
      active.add(job.target);
      retried.push(job);
    }
    if (retried.length === 0) return [];
    const copies = retried.map((j) => ({ ...j }));
    this.persist();
    for (const job of retried) this.emitChange(job);
    this.pump();
    return copies;
  }

  start(): void {
    this.started = true;
    this.pump();
  }

  stop(): Promise<void> {
    if (this.stopPromise) return this.stopPromise;
    this.stopping = true;
    this.started = false;
    for (const controller of this.controllers.values()) controller.abort();
    this.stopPromise = (async () => {
      while (this.active.size > 0) await Promise.allSettled([...this.active]);
      await this.writeChain;
      this.stopping = false;
      this.stopPromise = undefined;
    })();
    return this.stopPromise;
  }

  private emitChange(job: Job): void {
    this.emit("change", { ...job });
  }

  private reachable(): boolean {
    try {
      return statSync(realpathSync(this.root)).isDirectory();
    } catch {
      return false;
    }
  }

  private persist(): void {
    if (this.paused) return;
    const stored = [...this.jobs.values()]
      .filter((j) => j.state === "queued" || j.state === "running")
      .map((j) => ({ id: j.id, source: j.source, target: j.target, aspect: j.aspect }));
    const text = stringify({ version: 1, job: stored });
    this.writeChain = this.writeChain.then(() => atomicWriteContained(this.root, this.queueFile(), text)).catch((err) => {
      this.emit("error", err);
    });
  }

  private pump(): void {
    if (!this.started || this.stopping || this.paused) return;
    for (const job of this.jobs.values()) {
      if (this.controllers.size >= this.concurrency) return;
      if (job.state !== "queued" || this.controllers.has(job.id)) continue;
      const controller = new AbortController();
      this.controllers.set(job.id, controller);
      const task = this.run(job, controller).finally(() => {
        this.controllers.delete(job.id);
        this.active.delete(task);
        this.pump();
      });
      this.active.add(task);
    }
  }

  private async run(job: Job, controller: AbortController): Promise<void> {
    job.state = "running";
    job.progress = 0;
    delete job.error;
    let ended = false;
    const dst = path.join(this.root, job.target);
    try {
      this.persist();
      this.emitChange(job);
      mkdirContained(this.root, path.dirname(dst));
      if (controller.signal.aborted) throw new Error("conversion aborted");
      await this.convertFn(
        job.source,
        dst,
        job.aspect,
        (ratio) => {
          if (ended) return;
          job.progress = ratio;
          const now = Date.now();
          const last = this.lastProgress.get(job.id);
          if (last === undefined || now - last >= PROGRESS_INTERVAL_MS) {
            this.lastProgress.set(job.id, now);
            this.emitChange(job);
          }
        },
        controller.signal,
        {
          beforeWrite: (part) => prepareScratchFile(this.root, part),
          beforeRename: (part, final) => checkRename(this.root, part, final),
        },
      );
      const { durationMs } = await this.probeFn(dst);
      const st = await stat(job.source);
      const rec: SourceRecord = { target: job.target, source: job.source, size: st.size, mtimeMs: Math.trunc(st.mtimeMs), durationMs };
      try {
        await upsertSource(this.root, rec);
      } catch (err) {
        this.emit("error", err);
      }
      ended = true;
      job.state = "done";
      job.progress = 1;
      this.persist();
      this.emitChange(job);
      this.emit("converted", rec);
    } catch (err) {
      ended = true;
      if (controller.signal.aborted && this.stopping) {
        job.state = "queued";
        job.progress = 0;
      } else if (!this.reachable()) {
        this.paused = true;
        job.state = "queued";
        job.progress = 0;
      } else {
        job.state = "failed";
        job.error = err instanceof Error ? err.message : String(err);
      }
      this.persist();
      this.emitChange(job);
    } finally {
      this.lastProgress.delete(job.id);
    }
  }
}
