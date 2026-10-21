import { randomUUID } from "node:crypto";
import { EventEmitter } from "node:events";
import { mkdirSync, readFileSync, renameSync } from "node:fs";
import { mkdir, stat } from "node:fs/promises";
import os from "node:os";
import path from "node:path";
import { parse, stringify } from "smol-toml";
import { convert, probe } from "./ffmpeg.js";
import { atomicWriteFile, upsertSource } from "./sources.js";
import type { Job, SourceRecord } from "./types.js";

const PROGRESS_INTERVAL_MS = 250;

export interface QueueOptions {
  concurrency?: number;
  convertFn?: typeof convert;
  probeFn?: typeof probe;
}

type Aspect = "16:9" | "4:3";

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
  private writeChain: Promise<void> = Promise.resolve();

  constructor(private readonly root: string, opts: QueueOptions = {}) {
    super();
    this.on("error", () => undefined);
    this.concurrency = Math.max(1, opts.concurrency ?? Math.max(1, Math.floor(os.cpus().length / 2)));
    this.convertFn = opts.convertFn ?? convert;
    this.probeFn = opts.probeFn ?? probe;
    this.load();
  }

  private queueFile(): string {
    return path.join(this.root, ".studio", "queue.toml");
  }

  private load(): void {
    const file = this.queueFile();
    let text: string;
    try {
      text = readFileSync(file, "utf8");
    } catch {
      return;
    }
    try {
      const doc = parse(text);
      if (doc.version !== 1 || !Array.isArray(doc.job)) throw new Error("unsupported queue file");
      for (const raw of doc.job as Record<string, unknown>[]) {
        if (
          typeof raw.id !== "string" || typeof raw.source !== "string" || typeof raw.target !== "string" ||
          (raw.aspect !== "16:9" && raw.aspect !== "4:3")
        ) {
          throw new Error("invalid job entry");
        }
        this.jobs.set(raw.id, { id: raw.id, source: raw.source, target: raw.target, aspect: raw.aspect, state: "queued", progress: 0 });
      }
    } catch {
      this.jobs.clear();
      try {
        mkdirSync(path.dirname(file), { recursive: true });
        renameSync(file, `${file}.damaged`);
      } catch {
        return;
      }
    }
  }

  /**
   * @throws {Error} when the target is absolute or leaves the root
   */
  enqueue(source: string, target: string, aspect: Aspect): Job {
    const parts = target.split(/[\\/]+/);
    if (target === "" || path.isAbsolute(target) || target.startsWith("/") || target.startsWith("\\") || parts.includes("..")) {
      throw new Error(`target must be a relative path inside the cable-twii folder: ${target}`);
    }
    for (const existing of this.jobs.values()) {
      if (existing.target === target && (existing.state === "queued" || existing.state === "running")) return { ...existing };
    }
    const job: Job = { id: randomUUID(), source, target, aspect, state: "queued", progress: 0 };
    this.jobs.set(job.id, job);
    this.persist();
    this.emitChange(job);
    this.pump();
    return { ...job };
  }

  list(): Job[] {
    return [...this.jobs.values()].map((j) => ({ ...j }));
  }

  start(): void {
    this.started = true;
    this.pump();
  }

  async stop(): Promise<void> {
    this.stopping = true;
    this.started = false;
    for (const controller of this.controllers.values()) controller.abort();
    while (this.active.size > 0) await Promise.allSettled([...this.active]);
    await this.writeChain;
    this.stopping = false;
  }

  private emitChange(job: Job): void {
    this.emit("change", { ...job });
  }

  private persist(): void {
    const stored = [...this.jobs.values()]
      .filter((j) => j.state === "queued" || j.state === "running")
      .map((j) => ({ id: j.id, source: j.source, target: j.target, aspect: j.aspect }));
    const text = stringify({ version: 1, job: stored });
    this.writeChain = this.writeChain.then(() => atomicWriteFile(this.queueFile(), text)).catch((err) => {
      this.emit("error", err);
    });
  }

  private pump(): void {
    if (!this.started || this.stopping) return;
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
    this.persist();
    this.emitChange(job);
    const dst = path.join(this.root, job.target);
    try {
      await mkdir(path.dirname(dst), { recursive: true });
      await this.convertFn(
        job.source,
        dst,
        job.aspect,
        (ratio) => {
          job.progress = ratio;
          const now = Date.now();
          const last = this.lastProgress.get(job.id);
          if (last === undefined || now - last >= PROGRESS_INTERVAL_MS) {
            this.lastProgress.set(job.id, now);
            this.emitChange(job);
          }
        },
        controller.signal,
      );
      const { durationMs } = await this.probeFn(dst);
      const st = await stat(job.source);
      const rec: SourceRecord = { target: job.target, source: job.source, size: st.size, mtimeMs: Math.trunc(st.mtimeMs), durationMs };
      try {
        await upsertSource(this.root, rec);
      } catch (err) {
        this.emit("error", err);
      }
      job.state = "done";
      job.progress = 1;
      this.persist();
      this.emitChange(job);
      this.emit("converted", rec);
    } catch (err) {
      if (controller.signal.aborted && this.stopping) {
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
