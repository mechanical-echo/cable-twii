# cable-twii Studio Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A Mac tool, used in the browser, that prepares the media drive (import, convert, index), edits channels, lets the owner build schedules on a drag-and-drop timeline sized by real video length, and previews the exact lineup the Wii will air.

**Architecture:** One Node.js 22 + TypeScript package in `studio/`. A Fastify server on `127.0.0.1:4747` owns all file access, ffmpeg runs, the conversion queue and calls to the engine CLI. A React + Vite web UI talks to it over a small JSON API. The drive's `/cable-twii/` folder is the only source of truth; Studio keeps its own bookkeeping in `/cable-twii/.studio/`.

**Tech Stack:** Node.js 22, TypeScript 5, Fastify 5 (+ `@fastify/static`, `@fastify/multipart`), `smol-toml`, `sharp`, `open`, React 18, Vite 5, `@dnd-kit/core` + `@dnd-kit/sortable`, Vitest, ffmpeg/ffprobe (Homebrew), `cable-twii-sched` (plan 01).

**Spec:** `docs/superpowers/specs/2026-10-20-cable-twii-design.md` (§4, §5, §8)

## Status (2026-10-22)

- Done: Tasks 1-11, each reviewed, followed by a final whole-branch review, a fix wave and follow-ups. Studio was also ported to Windows (drive detection, folder pickers, ffmpeg concurrency) and those fixes are merged.
- Left: Task 12, the end-to-end check on a real flash drive. The owner formats the drive and runs it.
- Deferred minor review findings are logged in the git-ignored progress ledger and still need triage.

## Global Constraints

- Server binds to `127.0.0.1` only. Port `4747`.
- Never formats, partitions or writes outside the chosen `/cable-twii/` folder. Refuses any volume that has a `wbfs/` folder at its root or a non-FAT32 filesystem (unless the chosen path is a plain folder used for testing, not a volume root).
- Original source videos are never modified or moved.
- File formats exactly as in `docs/formats.md` (`version = 1`); TOML read/written with `smol-toml`.
- Converted files written as `<name>.mpg.part`, renamed on success.
- Video settings and ffmpeg arguments from `docs/decisions/0001-video-format.md` (spec §8.3 as the default).
- Icons: 128×128 PNG, contain-fit on a transparent background.
- Engine CLI path: `../engine/build/cable-twii-sched` relative to `studio/`, overridable with `CTW_SCHED_BIN`.
- Wii UI colour reference for Studio styling: Wii blue `#34BEED`, panel border `#D6DBE0`.
- No comments in source code. No trailing whitespace.
- Commits: `wip | …`, `test | …`, `fix | …` plus the attribution line.

## Review Focus

- The owner unplugs the drive mid-conversion: no half file may remain without the `.part` suffix, and the queue resumes on replug (Task 4 test `queue_resume_after_restart`, Task 4 test `convert_leaves_part_on_failure`).
- Source filenames with emoji, accents, `:`, `?` or trailing dots must produce valid FAT names and never collide (Task 1 test `fatSafe_collisions`).
- A source folder mixing videos with `.srt`, `.nfo`, `.jpg` and macOS `._` files must import only videos (Task 3 test `scan_ignores_non_video`).
- Editing `config.toml` by hand while Studio is open must not be silently overwritten: Studio checks the file's mtime before saving (Task 6 test `config_save_conflict`).
- A schedule saved with engine errors must not be written to the drive; the errors are shown instead (Task 6 test `put_config_rejects_invalid`).

---

## File Structure

```
studio/
├── package.json  tsconfig.json  vite.config.ts  vitest.config.ts
├── src/server/
│   ├── main.ts           start server, open browser
│   ├── app.ts            buildApp(): Fastify instance and routes
│   ├── types.ts          shared types (also imported by web)
│   ├── names.ts          filename guessing, FAT-safe names
│   ├── ffmpeg.ts         probe, conversion args, convert
│   ├── drive.ts          inspect volume, init root, dot_clean
│   ├── sources.ts        .studio/sources.toml
│   ├── queue.ts          conversion queue (persisted)
│   ├── library.ts        scan drive → library.toml
│   ├── showMeta.ts       show.toml read/write
│   ├── config.ts         config.toml read/write/serialize
│   ├── sched.ts          engine CLI wrapper
│   ├── icons.ts          icon resizing
│   └── picker.ts         native macOS folder/file picker via osascript
├── src/web/
│   ├── index.html  main.tsx  App.tsx  api.ts  styles.css
│   ├── pages/DrivePage.tsx  LibraryPage.tsx  ChannelsPage.tsx  SchedulePage.tsx  PreviewPage.tsx
│   ├── library/ImportShowDialog.tsx  ImportAdsDialog.tsx  JobsPanel.tsx  ShowMetaDialog.tsx
│   └── schedule/layout.ts  LibraryPanel.tsx  Timeline.tsx  TimelineBlock.tsx  ItemEditor.tsx  SlotHeader.tsx
└── test/
    ├── names.test.ts  ffmpeg.test.ts  drive.test.ts  sources.test.ts  queue.test.ts
    ├── library.test.ts  config.test.ts  sched.test.ts  app.test.ts  layout.test.ts
    └── fixtures/ (tiny .mpg files generated in a beforeAll with ffmpeg -t 1)
```

## Shared types (`src/server/types.ts`)

```ts
export type Mode = "timed" | "looping";
export type Item =
  | { type: "episode"; show: string; season: number; episode: number; from?: string }
  | { type: "random"; show: string; season?: number; count: number; from?: string }
  | { type: "random-run"; show: string; season?: number; count: number; from?: string }
  | { type: "ads"; count: number }
  | { type: "ad"; path: string }
  | { type: "fill"; show: string; season?: number; ads_between?: number; from?: string };
export interface Slot { at: string; items: Item[] }
export interface Channel { number: number; name: string; category: string; folder?: string; icon?: string; ads: boolean; mode: Mode; slot?: Slot[]; loop?: Item[] }
export interface Config { version: 1; seed: number; channel: Channel[] }

export interface LibEpisode { season: number; episode: number; title: string; description: string; path: string; duration_ms: number }
export interface LibShow { channel: string; folder: string; name: string; icon: string; episode: LibEpisode[] }
export interface LibAd { path: string; channel: string; duration_ms: number }
export interface Library { version: 1; generated: Date; ad: LibAd[]; show: LibShow[] }

export interface SourceRecord { target: string; source: string; size: number; mtimeMs: number; durationMs: number }
export interface EpisodeGuess { season: number | null; episode: number | null; title: string }

export interface Issue { level: "error" | "warning"; code: string; channel: number; slot: number; item: number; line: number; file: string; message: string }
export interface Airing { start_ms: number; duration_ms: number; kind: "episode" | "ad" | "upnext"; path: string | null; show: string | null; season: number | null; episode: number | null; title: string | null; slot: number; item: number }
export interface LineupResult { channel: number; date: string; hash: string; airings: Airing[]; warnings: Issue[] }

export type JobState = "queued" | "running" | "done" | "failed";
export interface Job { id: string; source: string; target: string; aspect: "16:9" | "4:3"; state: JobState; progress: number; error?: string }
```

---

### Task 1: Package skeleton and naming rules

**Files:**
- Create: `studio/package.json`, `studio/tsconfig.json`, `studio/vitest.config.ts`, `studio/src/server/types.ts`, `studio/src/server/names.ts`, `studio/test/names.test.ts`

**Interfaces:**
- Produces:
  - `guessEpisode(fileName: string): EpisodeGuess`
  - `fatSafe(name: string): string`
  - `uniqueName(name: string, taken: Set<string>): string` (appends ` (2)`, ` (3)`… before the extension, case-insensitive)
  - `episodeFileName(e: EpisodeGuess): string` → `S01E02 - Title.mpg`, `S01E02.mpg` with no title, `Title.mpg` with no season/episode; season/episode zero-padded to 2 digits (3 if ≥ 100)
  - `isVideoFile(name: string): boolean` (`.mp4 .mkv .avi .mov .m4v .mpg .mpeg .ts .webm .wmv .flv`, case-insensitive, rejects names starting with `._`)
- npm scripts: `start` (build web + run server), `dev` (Vite + tsx watch), `test` (`vitest run`), `typecheck` (`tsc --noEmit`).

- [x] **Step 1: Ask the owner to approve `npm install`** of the dependencies in the Tech Stack line. Wait for a yes, then install.

- [x] **Step 2: Write the failing tests**

```ts
test("guess SxxEyy with title", () => expect(guessEpisode("Gumball S01E02 The Responsible.mkv")).toEqual({ season: 1, episode: 2, title: "The Responsible" }));
test("guess lowercase and dots", () => expect(guessEpisode("adventure.time.s05e01.finn.the.human.mp4")).toEqual({ season: 5, episode: 1, title: "Finn The Human" }));
test("guess 1x05", () => expect(guessEpisode("Ben 10 - 1x05 - Title.avi")).toEqual({ season: 1, episode: 5, title: "Title" }));
test("guess Episode N", () => expect(guessEpisode("Episode 7.mp4")).toEqual({ season: null, episode: 7, title: "" }));
test("guess plain name", () => expect(guessEpisode("Artist - Song.mp4")).toEqual({ season: null, episode: null, title: "Artist - Song" }));
test("fatSafe replaces and strips", () => expect(fatSafe('AC/DC: Live? <1> "x" |y|.')).toBe("AC-DC- Live 1 x -y-"));
test("fatSafe keeps unicode", () => expect(fatSafe("Pokémon ✨")).toBe("Pokémon ✨"));
test("fatSafe_collisions", () => { const t = new Set(["s01e01 - a.mpg"]); expect(uniqueName("S01E01 - A.mpg", t)).toBe("S01E01 - A (2).mpg"); });
test("episodeFileName variants", () => {
  expect(episodeFileName({ season: 1, episode: 2, title: "The Responsible" })).toBe("S01E02 - The Responsible.mpg");
  expect(episodeFileName({ season: 1, episode: 2, title: "" })).toBe("S01E02.mpg");
  expect(episodeFileName({ season: null, episode: null, title: "Artist - Song" })).toBe("Artist - Song.mpg");
});
test("isVideoFile", () => { expect(isVideoFile("a.MKV")).toBe(true); expect(isVideoFile("._a.mkv")).toBe(false); expect(isVideoFile("a.srt")).toBe(false); });
```

`fatSafe` rule: `/ \ : |` → `-`; `* ? " < >` removed; control characters removed; runs of spaces collapsed; leading/trailing spaces and trailing dots trimmed; result capped at 120 characters.

- [x] **Step 3: Run to verify failure**

Run: `cd studio && npm test -- names`
Expected: FAIL (module not found).

- [x] **Step 4: Implement `names.ts`.** Title clean-up: dots/underscores → spaces, collapse spaces, strip a leading `- `, title-case words only if the whole title was lowercase.

- [x] **Step 5: Run tests**

Run: `cd studio && npm test -- names`
Expected: PASS.

- [x] **Step 6: Commit**

```bash
git add studio && git commit -m "wip | studio skeleton and naming rules"
```

---

### Task 2: ffmpeg wrapper

**Files:**
- Create: `studio/src/server/ffmpeg.ts`, `studio/test/ffmpeg.test.ts`

**Interfaces:**
- Produces:
  - `probe(path: string): Promise<{ durationMs: number; width: number; height: number; displayAspect: number }>` (ffprobe JSON; `displayAspect` from `display_aspect_ratio` if present, else width×SAR/height)
  - `aspectFor(displayAspect: number): "16:9" | "4:3"` (`>= 1.5` → `"16:9"`)
  - `conversionArgs(src: string, dst: string, aspect: "16:9" | "4:3"): string[]`
  - `convert(src: string, dst: string, aspect: "16:9" | "4:3", onProgress: (ratio: number) => void, signal?: AbortSignal): Promise<void>` (writes `dst + ".part"`, renames on exit code 0, deletes `.part` and rejects otherwise; progress from `-progress pipe:1` `out_time_ms` / probed duration)

- [x] **Step 1: Write the failing tests**

```ts
test("args 16:9", () => {
  const a = conversionArgs("in.mkv", "out.mpg.part", "16:9");
  expect(a).toEqual(expect.arrayContaining(["-c:v", "mpeg1video", "-g", "15", "-c:a", "mp2", "-ar", "48000", "-ac", "2", "-f", "mpeg"]));
  expect(a.join(" ")).toContain("scale=640:360");
  expect(a.join(" ")).toContain("loudnorm=I=-16:TP=-1.5:LRA=11");
  expect(a.at(-1)).toBe("out.mpg.part");
});
test("args 4:3", () => expect(conversionArgs("a", "b", "4:3").join(" ")).toContain("scale=480:360"));
test("aspectFor", () => { expect(aspectFor(16 / 9)).toBe("16:9"); expect(aspectFor(4 / 3)).toBe("4:3"); });
test("convert and probe a 1 s file", async () => { /* ffmpeg testsrc 1 s → convert → probe durationMs within 900..1100, no .part left */ });
test("convert_leaves_part_on_failure", async () => { /* src missing → rejects, neither dst nor dst.part exists */ });
```

- [x] **Step 2: Run to verify failure** — `cd studio && npm test -- ffmpeg` → FAIL.

- [x] **Step 3: Implement `ffmpeg.ts`** with `child_process.spawn`, values from the Global Constraints.

- [x] **Step 4: Run tests** — PASS.

- [x] **Step 5: Commit**

```bash
git add studio && git commit -m "wip | studio ffmpeg wrapper"
```

---

### Task 3: Drive inspection, root setup and source scanning

**Files:**
- Create: `studio/src/server/drive.ts`, `studio/src/server/picker.ts`, `studio/test/drive.test.ts`

**Interfaces:**
- Produces:
  - `inspectDrive(path: string): Promise<DriveInfo>` where `DriveInfo = { path: string; rootPath: string; isVolumeRoot: boolean; fsType: string | null; freeBytes: number; hasRoot: boolean; hasWbfs: boolean; usable: boolean; reason?: string }`. `rootPath = path + "/cable-twii"`. `fsType` from `diskutil info -plist <path>` (`FilesystemType`, e.g. `msdos`) when `isVolumeRoot`. `usable` is false when `hasWbfs`, or when `isVolumeRoot` and `fsType !== "msdos"`, with `reason` text shown in the UI.
  - `initRoot(info: DriveInfo): Promise<void>` creates `cable-twii/`, `cable-twii/ads/`, `cable-twii/.studio/` (throws if not `usable`).
  - `dotClean(volumePath: string): Promise<void>` runs `dot_clean -m <volumePath>`.
  - `scanSource(dir: string): Promise<{ source: string; guess: EpisodeGuess; durationMs: number; aspect: "16:9" | "4:3" }[]>` (recursive, `isVideoFile` only, sorted by guess then name)
  - `pickFolder(prompt: string): Promise<string | null>` and `pickFiles(prompt: string): Promise<string[]>` via `osascript -e 'POSIX path of (choose folder with prompt "…")'` (null/[] on cancel).

- [x] **Step 1: Write the failing tests** (temp dirs; `diskutil` is mocked via an injectable `runDiskutil`)

```ts
test("refuses volume with wbfs folder", async () => { /* temp volume-root flagged, wbfs/ present → usable false, reason contains "games drive" */ });
test("refuses non-FAT32 volume", async () => { /* mocked FilesystemType "apfs" → usable false */ });
test("accepts FAT32 volume", async () => { /* mocked "msdos" → usable true */ });
test("plain folder allowed for testing", async () => { /* isVolumeRoot false → usable true */ });
test("initRoot creates structure", async () => { /* cable-twii/, ads/, .studio/ exist */ });
test("initRoot refuses unusable", async () => { /* rejects */ });
test("scan_ignores_non_video", async () => { /* dir with a.mkv, a.srt, a.nfo, cover.jpg, ._a.mkv → 1 result */ });
```

- [x] **Step 2–4: Fail, implement, pass** — `cd studio && npm test -- drive`.

- [x] **Step 5: Commit**

```bash
git add studio && git commit -m "wip | studio drive inspection and source scan"
```

---

### Task 4: Sources record and conversion queue

**Files:**
- Create: `studio/src/server/sources.ts`, `studio/src/server/queue.ts`, `studio/test/sources.test.ts`, `studio/test/queue.test.ts`

**Interfaces:**
- Produces:
  - `readSources(root: string): Promise<SourceRecord[]>` / `writeSources(root: string, recs: SourceRecord[]): Promise<void>` (`.studio/sources.toml`, array of tables `[[source]]`; atomic write via temp + rename)
  - `needsConversion(rec: SourceRecord | undefined, stat: { size: number; mtimeMs: number }, targetExists: boolean): boolean`
  - `class ConversionQueue extends EventEmitter` — `constructor(root: string, opts?: { concurrency?: number; convertFn?: typeof convert; probeFn?: typeof probe })`, `enqueue(source: string, target: string, aspect: "16:9" | "4:3"): Job`, `list(): Job[]`, `start(): void`, `stop(): Promise<void>`; emits `"change"` with `Job`. Persists queued/running jobs to `.studio/queue.toml`; on construction reloads them as `queued`. Default concurrency `Math.max(1, Math.floor(os.cpus().length / 2))`. On success records the `SourceRecord` (with probed `durationMs`) and emits `"converted"`.

- [x] **Step 1: Write the failing tests**

```ts
test("needsConversion", () => {
  expect(needsConversion(undefined, { size: 1, mtimeMs: 1 }, false)).toBe(true);
  expect(needsConversion({ target: "t", source: "s", size: 1, mtimeMs: 1, durationMs: 1 }, { size: 1, mtimeMs: 1 }, true)).toBe(false);
  expect(needsConversion({ target: "t", source: "s", size: 1, mtimeMs: 1, durationMs: 1 }, { size: 2, mtimeMs: 1 }, true)).toBe(true);
  expect(needsConversion({ target: "t", source: "s", size: 1, mtimeMs: 1, durationMs: 1 }, { size: 1, mtimeMs: 1 }, false)).toBe(true);
});
test("sources round trip", async () => { /* write 2 records → read equal */ });
test("queue runs jobs with concurrency 2", async () => { /* fake convertFn resolving after 10 ms; 3 jobs; max concurrent observed == 2; all done */ });
test("queue failure marks job failed", async () => { /* convertFn rejects "boom" → state failed, error "boom" */ });
test("queue_resume_after_restart", async () => { /* enqueue 2, stop before running, new ConversionQueue(root) → 2 queued jobs */ });
test("queue records source on success", async () => { /* readSources contains the target with durationMs from probeFn */ });
```

- [x] **Step 2–4: Fail, implement, pass** — `cd studio && npm test -- sources queue`.

- [x] **Step 5: Commit**

```bash
git add studio && git commit -m "wip | studio conversion queue"
```

---

### Task 5: Library index and show metadata

**Files:**
- Create: `studio/src/server/library.ts`, `studio/src/server/showMeta.ts`, `studio/test/library.test.ts`

**Interfaces:**
- Produces:
  - `readShowMeta(showDir: string): Promise<{ name?: string; episodes: Record<string, string> }>` / `writeShowMeta(showDir, meta)` (`show.toml`, keys `SxxEyy`, or the file title for seasonless shows)
  - `buildLibrary(root: string, sources: SourceRecord[], probeFn?: typeof probe): Promise<Library>` — walks `root`: `ads/*.mpg` (shared), every other top-level folder except `.studio` is a channel folder; inside it `ads/*.mpg` (channel ads) and show folders; inside a show either `Season N/` folders or `.mpg` files directly (season 0, episode = 1-based index in filename order). Episode numbers from `SxxEyy` in the filename. Titles from the filename after ` - `. Durations from `sources` by target path, else `probeFn`. Icons: `show.png` if present. Skips `._*`, `.DS_Store`, `*.part`.
  - `writeLibrary(root: string, lib: Library): Promise<void>` (atomic) and `serializeLibrary(lib: Library): string`

- [x] **Step 1: Write the failing tests** against a temp tree mirroring plan 00 Task 1's sample layout, using tiny files and a fake `probeFn` returning fixed durations

```ts
test("indexes shows, seasons, ads", async () => { /* 4 shows, Gumball 6 episodes sorted, 3 shared ads */ });
test("seasonless show numbering", async () => { /* Music Videos episodes 1..6, season 0, titles "Artist 1 - Song 1"… */ });
test("descriptions from show.toml", async () => { /* S01E01 description present */ });
test("show name override", async () => { /* show.toml name used as LibShow.name */ });
test("skips part and dot files", async () => { /* a.mpg.part and ._a.mpg ignored */ });
test("durations prefer sources", async () => { /* probeFn not called for files present in sources */ });
test("library round trip through engine", async () => { /* writeLibrary + a minimal config, then `cable-twii-sched validate` exits 0 */ });
```

- [x] **Step 2–4: Fail, implement, pass** — `cd studio && npm test -- library`.

- [x] **Step 5: Commit**

```bash
git add studio && git commit -m "wip | studio library index"
```

---

### Task 6: Config store, engine wrapper and API

**Files:**
- Create: `studio/src/server/config.ts`, `studio/src/server/sched.ts`, `studio/src/server/icons.ts`, `studio/src/server/app.ts`, `studio/src/server/main.ts`
- Create: `studio/test/config.test.ts`, `studio/test/sched.test.ts`, `studio/test/app.test.ts`

**Interfaces:**
- Produces:
  - `parseConfig(text: string): Config`, `serializeConfig(cfg: Config): string` (channels sorted by number; `[[channel]]`, `[[channel.slot]]` with inline-table `items`; `loop` as an inline-table array)
  - `readConfig(root: string): Promise<{ config: Config; mtimeMs: number }>` (missing file → `{ version: 1, seed: <random 31-bit>, channel: [] }`, mtimeMs 0)
  - `writeConfig(root: string, cfg: Config, expectedMtimeMs: number): Promise<number>` — throws `ConfigConflictError` if the file's mtime differs from `expectedMtimeMs`; copies the old file to `config.bak.toml`; atomic write; returns new mtime.
  - `validateRoot(root: string, checkFiles?: boolean): Promise<{ ok: boolean; issues: Issue[] }>`, `lineup(root: string, channel: number, date: string): Promise<LineupResult>` (spawn `CTW_SCHED_BIN`)
  - `validateCandidate(root: string, cfg: Config): Promise<{ ok: boolean; issues: Issue[] }>` — writes `cfg` and a copy of `library.toml` to a temp dir and validates there, so invalid configs never touch the drive.
  - `makeIcon(input: Buffer, dst: string): Promise<void>` (sharp, 128×128, `fit: "contain"`, transparent background, PNG)
  - `buildApp(opts: { schedBin: string; queueFactory?: (root: string) => ConversionQueue }): FastifyInstance`
- API (JSON; errors as `{ error: string, issues?: Issue[] }` with 4xx):

| Method | Path | Body / query | Response |
|---|---|---|---|
| GET | `/api/drive` | — | `DriveInfo \| null` |
| POST | `/api/drive` | `{ path }` | `DriveInfo` (remembered in `~/.cable-twii-studio.json`) |
| POST | `/api/drive/pick` | — | `DriveInfo \| null` (native picker) |
| POST | `/api/drive/init` | — | `DriveInfo` |
| POST | `/api/drive/done` | — | `{ ok: true }` after `dotClean` |
| POST | `/api/import/pick-source` | — | `{ dir, files: ScanResult[] } \| null` |
| POST | `/api/import/show` | `{ channelFolder, showFolder, showName, episodes: { source, season, episode, title, description, aspect }[] }` | `{ jobs: Job[] }` |
| POST | `/api/import/ads` | `{ channelFolder: string \| "", files: { source, aspect }[] }` | `{ jobs: Job[] }` |
| GET | `/api/jobs` | — | `Job[]` |
| GET | `/api/jobs/stream` | — | Server-Sent Events, one `Job` per event |
| GET | `/api/library` | — | `Library` |
| POST | `/api/library/rebuild` | — | `Library` |
| GET | `/api/config` | — | `{ config, mtimeMs }` |
| PUT | `/api/config` | `{ config, mtimeMs }` | `{ mtimeMs, issues }`; 422 with issues if errors; 409 on conflict |
| POST | `/api/config/reshuffle` | `{ mtimeMs }` | `{ mtimeMs, seed }` |
| POST | `/api/validate` | — | `{ ok, issues }` (with `--check-files`) |
| GET | `/api/preview` | `?channel=1&date=2026-10-20` | `LineupResult` |
| POST | `/api/icon` | multipart `file`, fields `kind=channel\|show`, `channelFolder`, `showFolder?` | `{ path }` |
| PUT | `/api/show-meta` | `{ channelFolder, showFolder, name, episodes }` | `Library` (rebuilt) |
| GET | `/media/*` | — | icons from the drive for display |

  Every mutating route rebuilds `library.toml` when files change (`"converted"` events trigger a debounced 2 s rebuild).

- [x] **Step 1: Write the failing tests**

```ts
test("config round trip", () => { /* parseConfig(serializeConfig(sample)) deep-equals sample, sample uses every item type */ });
test("serialized config is valid for the engine", async () => { /* write to fixture root → validateRoot ok */ });
test("config_save_conflict", async () => { /* readConfig, touch file, writeConfig with old mtime → ConfigConflictError */ });
test("writeConfig keeps backup", async () => { /* config.bak.toml equals previous text */ });
test("sched lineup parses", async () => { /* lineup(fixtureRoot, 1, "2026-10-20") → airings contiguous, hash 16 hex chars */ });
test("GET /api/config", async () => { /* app.inject → 200, config.version 1 */ });
test("put_config_rejects_invalid", async () => { /* unknown show → 422, issues[0].code "unknown_show", file on disk unchanged */ });
test("PUT /api/config conflict", async () => { /* stale mtime → 409 */ });
test("icon resized", async () => { /* 500x300 png → 128x128 png */ });
test("drive refuses games disk via API", async () => { /* POST /api/drive with wbfs folder → DriveInfo.usable false; POST /api/drive/init → 400 */ });
```

- [x] **Step 2–4: Fail, implement, pass** — `cd studio && npm test`.

- [x] **Step 5: Commit**

```bash
git add studio && git commit -m "wip | studio config store and api"
```

---

### Task 7: Web shell, Drive and Library pages

**Files:**
- Create: `studio/vite.config.ts`, `studio/src/web/index.html`, `main.tsx`, `App.tsx`, `api.ts`, `styles.css`, `pages/DrivePage.tsx`, `pages/LibraryPage.tsx`, `library/ImportShowDialog.tsx`, `library/ImportAdsDialog.tsx`, `library/JobsPanel.tsx`, `library/ShowMetaDialog.tsx`

**Interfaces:**
- Consumes: Task 6 API.
- Produces: `api.ts` exports one typed function per API route (`getDrive`, `setDrive`, `pickDrive`, `initDrive`, `driveDone`, `pickSource`, `importShow`, `importAds`, `getJobs`, `subscribeJobs(cb): () => void`, `getLibrary`, `rebuildLibrary`, `getConfig`, `putConfig`, `reshuffle`, `validate`, `getPreview`, `uploadIcon`, `putShowMeta`).
- UI: left tab bar `Drive · Library · Channels · Schedule · Preview`, light Wii-like styling (white rounded cards, `#34BEED` accents). Drive page shows `DriveInfo` with a red banner and the `reason` when unusable, an "Set up cable-twii on this drive" button when `!hasRoot`, and "Done — safe to eject" running `driveDone`. Library page: tree channel → show → season → episode (title, duration `m:ss`), "Add show…" (pick source → editable table of guesses: season, episode, title, description; channel and show name fields; show icon picker) → Import; "Add ads…"; Jobs panel with progress bars and failed-job errors; "Edit info" opens `ShowMetaDialog`.

- [x] **Step 1: Implement the pages.**

- [x] **Step 2: Manual check with the sample library**

Run: `tools/make-samples.sh /tmp/ctw-src --short`, then `cd studio && npm start`. In the browser: choose a plain test folder as the drive, set it up, import `Gumball` from the sample source folder into channel `Cartoon Network`, import 3 ads.
Expected: jobs reach `done`; library tree lists Gumball with 6 episodes and durations ~0:30; `cable-twii/library.toml` exists; no `.part` files remain.

- [x] **Step 3: Commit**

```bash
git add studio && git commit -m "wip | studio drive and library pages"
```

---

### Task 8: Channels page

**Files:**
- Create: `studio/src/web/pages/ChannelsPage.tsx`

**Interfaces:**
- Consumes: `getConfig`, `putConfig`, `uploadIcon`, `getLibrary`.
- UI: list of channels sorted by number with icon, name, category, ads on/off, mode; add/edit/delete; edit form fields: number (1–999, unique), name, category, folder (dropdown of channel folders on the drive, default = name), icon (upload → `makeIcon` → `<folder>/channel.png`), ads toggle, mode (switching mode keeps the other list in memory until save but saves only the active one). Save calls `putConfig`; 422 issues listed under the form; 409 shows "config.toml changed on disk — Reload".

- [x] **Step 1: Implement.**

- [x] **Step 2: Manual check:** create channels 1 Cartoon Network (Kids, ads on, timed) and 2 MTV (Music, folder `mtv-00s`, ads off, looping), upload icons.
Expected: `config.toml` contains both, icons are 128×128 PNG on the drive, duplicate number shows an error and nothing is written.

- [x] **Step 3: Commit**

```bash
git add studio && git commit -m "wip | studio channels page"
```

---

### Task 9: Timeline layout (pure logic)

**Files:**
- Create: `studio/src/web/schedule/layout.ts`, `studio/test/layout.test.ts`

**Interfaces:**
- Produces:
  - `estimateItemMs(item: Item, channel: Channel, lib: Library): { ms: number; exact: boolean }` — `episode`/`ad`: exact duration; `random`/`random-run`: `count ×` mean duration of the pool (show from `item.from ?? channel folder`, season filter); `ads`: `count ×` mean ad duration of the channel's ad pool (0 when `channel.ads` is false); `fill`: `{ ms: Infinity, exact: false }`.
  - `type TimelineBlock = { slotIndex: number; itemIndex: number | null; kind: Item["type"] | "adfill" | "upnext"; startMs: number; durationMs: number; exact: boolean; overlapMs: number; label: string }`
  - `layoutTimed(channel: Channel, lib: Library): TimelineBlock[]` — per slot (sorted by `at`, window to next slot / first slot + 24 h): blocks stacked from slot start; `fill` takes the rest of the window; a block whose end passes the window end gets `overlapMs = end - windowEnd`; blocks after the window end are still listed (stacked) with full `overlapMs`; leftover time becomes one `adfill` block (ads on) or `upnext` block (ads off).
  - `layoutLoop(channel: Channel, lib: Library): { blocks: TimelineBlock[]; loopMs: number; exact: boolean }`
  - `msToPx(ms: number, pxPerHour: number): number`

- [x] **Step 1: Write the failing tests** (library fixture equal to engine `lib_basic.toml`, as `Library` JSON)

```ts
test("fixed episode exact", () => expect(estimateItemMs({ type: "episode", show: "Ben 10", season: 1, episode: 5 }, cn, lib)).toEqual({ ms: 1320000, exact: true }));
test("random estimate is pool mean × count", () => expect(estimateItemMs({ type: "random", show: "Adventure Time", season: 5, count: 5 }, cn, lib)).toEqual({ ms: 3300000, exact: false }));
test("ads estimate zero when ads off", () => expect(estimateItemMs({ type: "ads", count: 2 }, { ...cn, ads: false }, lib).ms).toBe(0));
test("overlap flagged", () => { /* 07:00 [Ben 10 1320000], 07:15 → block overlapMs 420000 */ });
test("leftover becomes adfill", () => { /* 07:00 [Gumball episode 600000], 08:00 → adfill block durationMs 3000000 */ });
test("leftover becomes upnext when ads off", () => { /* same with ads false → kind "upnext" */ });
test("fill takes rest of slot", () => { /* fill block durationMs == window length, no adfill */ });
test("last slot wraps to first slot next day", () => { /* slots 07:00, 19:00 → second window 12 h */ });
test("loop length", () => { /* MTV loop random count 10 → loopMs 2000000, exact false */ });
test("msToPx", () => expect(msToPx(1800000, 120)).toBe(60));
```

- [x] **Step 2–4: Fail, implement, pass** — `cd studio && npm test -- layout`.

- [x] **Step 5: Commit**

```bash
git add studio && git commit -m "wip | studio timeline layout"
```

---

### Task 10: Schedule page (drag and drop timeline)

**Files:**
- Create: `studio/src/web/pages/SchedulePage.tsx`, `schedule/LibraryPanel.tsx`, `schedule/Timeline.tsx`, `schedule/TimelineBlock.tsx`, `schedule/ItemEditor.tsx`, `schedule/SlotHeader.tsx`

**Interfaces:**
- Consumes: Task 9 layout, `getConfig`/`putConfig`/`reshuffle`/`getLibrary`.
- Behaviour (spec §8.2.4 and the approved mockup `docs/superpowers/specs/mockups/studio-schedule-editor.html`):
  - Channel tabs on top; mode shown (switch lives on the Channels page).
  - **LibraryPanel**: shows of the channel folder plus a "Other channels" section (items dragged from there get `from`); each show → seasons → episodes with `m:ss`; rule chips per show and per season: "Random episode", "Random sequential run", "Fill slot"; Ads section with each ad and a "Random ad" chip.
  - **Timeline (timed)**: vertical 24 h scale starting at the first slot, default 120 px per hour, zoom buttons (30/60/120/240 px per hour). Slot start lines with editable `HH:MM` labels; "Add slot" button asks for a time; delete slot. Blocks drawn from `layoutTimed`: exact blocks solid (episode light blue, ad orange), estimated blocks striped, `fill` striped full-height, overlap red with `⚠ N min overlap`, `adfill` orange band, `upnext` grey band.
  - **Timeline (looping)**: one stack with the loop total length at the top.
  - Dropping a library entry inserts an item at the drop position within the slot (dnd-kit sortable per slot); dragging a block reorders within or across slots; `Delete`/`Backspace` or the trash button removes the selected block.
  - **ItemEditor** (side panel for the selected block): `count` (1–50), season (all/N), pick type switch between random/random-run, `ads_between` for fill (0–5), read-only exact duration or estimate.
  - Toolbar: Undo/Redo (in-memory history of the channel's items, 50 steps), Reshuffle, Save. Unsaved changes marked; leaving with unsaved changes asks for confirmation. Save → `putConfig`; errors listed and blocks with errors outlined.

- [x] **Step 1: Implement.**

- [x] **Step 2: Manual check with the sample library**
  1. Build the Cartoon Network schedule from the owner's example: slot 07:00 → random-run Gumball ×3, 1 random ad, random Adventure Time S5 ×5, 2 random ads, Ben 10 S01E05, random Gumball ×2; slot 19:00 → fill Adventure Time with 2 ads between.
  2. Shrink the 07:00 slot by adding a 07:30 slot.
  Expected: block heights proportional to length; the 07:00 slot shows red overlap once the 07:30 slot exists; Save writes `config.toml`; `cable-twii-sched validate` on the drive exits 0 with warnings.

- [x] **Step 3: Commit**

```bash
git add studio && git commit -m "wip | studio schedule timeline"
```

---

### Task 11: Preview page

**Files:**
- Create: `studio/src/web/pages/PreviewPage.tsx`

**Interfaces:**
- Consumes: `getPreview`, `getLibrary`, `getConfig`.
- UI: channel picker, date picker (default today), "Now" marker at the current time when the date is today. TV-guide list of airings: start–end `HH:MM`, kind badge, show icon, show name, `SxxEyy`, title; consecutive ads collapsed into "Ad break (N ads, m:ss)"; up-next rows grey; engine warnings listed at the top with slot/item.

- [x] **Step 1: Implement.**

- [x] **Step 2: Manual check:** preview channel 1 today and tomorrow.
Expected: lists differ between days; reloading the same day shows the identical list; the hash shown in the footer equals `cable-twii-sched hash` for that day.

- [x] **Step 3: Commit**

```bash
git add studio && git commit -m "wip | studio lineup preview"
```

---

### Task 12: End-to-end check on the real flash drive

- [ ] **Step 1:** Owner formats the flash drive as MS-DOS (FAT), Master Boot Record, in Disk Utility (owner does this step themselves). (owner — pending)

- [ ] **Step 2:** In Studio choose the drive, set it up, import one real show (at least one season) and a few ads, build a 2-slot schedule, Save, press Done. (owner, real flash drive — pending)
Expected: `diskutil info` still reports FAT32; `/cable-twii/` contains `config.toml`, `library.toml`, `.studio/`, converted `.mpg` files; no `._*` files remain (`find /Volumes/<drive> -name "._*"` prints nothing); `cable-twii-sched validate /Volumes/<drive>/cable-twii --check-files` exits 0.

- [ ] **Step 3: Commit any fixes** with `fix | …` messages. (only if Steps 1-2 find problems)
