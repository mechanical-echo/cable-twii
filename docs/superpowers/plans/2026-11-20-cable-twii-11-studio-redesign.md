# cable-twii Studio Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Rebuild Studio's web pages in the Wii Menu look, around an Episodes home, a full-page Add episodes flow, and one page per channel, on the existing server plus a few small endpoints.

**Architecture:**
- Pure, node-tested modules hold all new logic: routing, job rows, add-row building and duplicate checks, channel summaries. React components stay thin and are checked in a real browser.
- Shared look components live in `src/web/ui/`, styled by `ui/ui.css`, which uses tokens copied from `wii/source/ui/theme.h`.
- The server gains five small changes: job history, Replace, source previews, rebuild notes, and already-added checks.
- The work ships in five stages, and each one leaves Studio usable.

**Tech Stack:** TypeScript, React 18, plain CSS, Vite 5, Fastify 5, vitest 3 (node environment, no DOM tests), Node 22.

**Spec:** `docs/superpowers/specs/2026-11-20-studio-redesign-design.md`. Look and layout: `docs/superpowers/specs/mockups/studio-redesign.html` (look A, "Adding episodes", "Channels, and one page per channel").

## Global Constraints

- **Dependencies:** no new runtime or dev dependencies, and nothing loaded from the internet. Studio works offline.
- **Font:** "Rounded Mplus 1c", from `wii/data/font_medium.ttf` (weight 500) and `wii/data/font_bold.ttf` (weight 800), copied into the web build together with `wii/OFL.txt`. Never committed under `studio/`.
- **Tokens:**

  | Use | Value |
  |---|---|
  | Stripes | `#F4F5F7` / `#ECEEF1` |
  | Panel fill / border | `#FFFFFF` / `#D6DBE0` |
  | Accent / light / dark | `#34BEED` / `#7FD0EF` / `#2A8FB3` |
  | Text primary / secondary / label / clock | `#444444` / `#999999` / `#555555` / `#777788` |
  | Active row | `#EEFAFF` |
  | Bar track / inset | `#E1E5E9` / `#C7CED4` |
  | Pill fill (top → bottom) / border | `#FFFFFF` → `#E6EAEE` / `#C9CFD5` |
  | Panel shadow / focus ring | `#344A601A` / `#34BEED40` |
  | Warm | `#F3CF8A` |

  Panel radius is 14 px. Pills are fully rounded.
- **Light theme only.** The target width is 1024 px and up.
- **Tabs:** Episodes, Channels, Ads, and Server (label "Drive" in drive mode). The Studio name sits at the left of the top bar.
- **Routes:** `#/`, `#/add`, `#/add?channel=N`, `#/add?to=ads`, `#/channels`, `#/channels/N/(today|schedule|shows|settings)`, `#/folders/<folder>`, `#/ads`, `#/server`. Anything else goes to `#/`.
- **Recent list:** covers 7 days (`HISTORY_MS = 604800000`). Uploads are still swept after 24 hours, as today.
- **Upload batches:** at most 50 files per request (`UPLOAD_BATCH_FILES`). Only ticked files are uploaded.
- **Schedule zoom:** timed channels default to 30 px per hour; looping channels keep 120.
- **Exact copy:**
  - "Add episodes"
  - "Add N episodes"
  - "N of M ticked · X to upload"
  - "Already on air · Replace?"
  - "Already added"
  - "Keep this tab open until uploads finish"
  - "This browser can't preview this file"
  - "Add the file again"
  - "Retry failed"
  - "Re-read library"
  - "Not on a channel"
  - "No episodes yet. Add some to get started."
- **Drive mode:** keeps working throughout. There is no upload, and "Pick a folder" uses the existing native picker.
- **House rules:**
  - Match sibling files' idioms. Testable web modules use `.js` relative imports.
  - Add no redundant comments, and leave no trailing whitespace.
  - Commits are `wip | …`, `fix | …` or `test | …`, as short as possible, and end with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
  - Don't push or merge without asking.
- **Checks:**
  - Before every commit, `cd studio && npm test` and `npm run typecheck` must pass. Run `source ~/.nvm/nvm.sh && nvm use` first.
  - Browser checks run Studio on ports 4790 (server mode) and 4791 (drive mode), never 4747 or 4748.
  - Stop only the processes you started, by PID.

## Review Focus

1. **Back button with an unsaved schedule:** pressing Back (or clicking another tab) with unsaved edits must ask first. Choosing to stay keeps the old hash and the edits (Task 3, test `leaving a dirty page asks and can stay`).
2. **Renaming a channel whose folder is implicit:** a channel with no explicit folder and a new name must keep its old folder, so its shows don't vanish (Task 14, test `rename keeps the old folder`).
3. **Duplicate episodes:** the same season and episode twice in one batch, or already queued, must leave only one row ticked. The second is marked (Task 9, tests `second copy in the batch is unticked` and `queued episode counts as existing`).
4. **Server restart:** after a restart during and after conversions, Recent still shows the finished jobs, the running job comes back queued, and no job is listed twice (Task 4, tests `keeps finished jobs across a restart` and `requeued job is not also in history`).
5. **Source preview paths:** a crafted path (`..`, a symlink out of the folder, a file outside the last scanned folder) must give 404 (Task 6, tests `refuses a file outside the scanned folders` and `refuses a symlink out of the folder`).

---

## Stage 1 — Look and shell

### Task 1: Tokens, font and the shared look components

**Files:**
- Create: `studio/scripts/copy-fonts.mjs`
- Modify: `studio/scripts/build-web.mjs`, `studio/package.json` (`predev`), `.gitignore`
- Create: `studio/src/web/ui/ui.css`, `studio/src/web/ui/controls.tsx`, `studio/src/web/ui/layout.tsx`, `studio/src/web/ui/status.tsx`, `studio/src/web/ui/Tabs.tsx`, `studio/src/web/ui/ConfirmDialog.tsx`, `studio/src/web/ui/Gallery.tsx`
- Modify: `studio/src/web/main.tsx` (import `./ui/ui.css` after `./styles.css`)
- Test: `studio/test/fonts.test.ts`

**Interfaces:**
- **Produces, in `controls.tsx`:**
  - `PillButton({ kind?: "plain" | "primary" | "danger"; small?: boolean; disabled?: boolean; onClick?: () => void; type?: "button" | "submit"; children })`
  - `Select<T extends string>({ value: T; options: { value: T; label: string }[]; onChange: (v: T) => void; disabled?: boolean; label?: string })`
  - `TextField({ value: string; onChange: (v: string) => void; label?: string; placeholder?: string; disabled?: boolean; width?: "short" | "full"; type?: "text" | "password" | "date" })`
  - `Checkbox({ checked: boolean; onChange: (v: boolean) => void; disabled?: boolean; label?: string })`
- **Produces, in `layout.tsx`:**
  - `Panel({ title?: string; actions?: ReactNode; children })`
  - `ListRow({ selected?: boolean; dimmed?: boolean; onClick?: () => void; children })`
  - `Banner({ kind: "error" | "warn" | "ok"; children })`
  - `EmptyState({ children })`
  - `PageHead({ title: string; sub?: ReactNode; actions?: ReactNode })`
- **Produces, in `status.tsx`:**
  - `Chip({ kind?: "neutral" | "ok" | "warn" | "error"; children })`
  - `ProgressBar({ value: number })`, where `value` runs from 0 to 1
- **Produces, in `Tabs.tsx`:** `Tabs<T extends string>({ tabs: { id: T; label: string }[]; active: T; onChange: (id: T) => void })`
- **Produces, in `ConfirmDialog.tsx`:** `ConfirmDialog({ title: string; body: ReactNode; confirm: string; danger?: boolean; onConfirm: () => void; onCancel: () => void })`. It renders `.overlay`, so `SchedulePage.dialogOpen()` still detects it.
- **CSS class prefix:** `ui-`, so the new classes don't clash with the old `styles.css` while both are loaded.
- **Scripts:** `copy-fonts.mjs` exports `copyFonts(studio: string): Promise<string[]>`. It copies the files into `src/web/public/fonts/` and returns the destination paths.

- [ ] **Step 1: Write the failing test.** In `test/fonts.test.ts`, test `copies the Wii fonts and licence`: call `copyFonts(<tmp copy of the studio layout with ../wii/data and ../wii/OFL.txt>)` and expect `["font_bold.ttf", "font_medium.ttf", "OFL.txt"]` under `src/web/public/fonts/`, byte-equal to the sources.
- [ ] **Step 2: Run it and see it fail.** Run `cd studio && npx vitest run test/fonts.test.ts`. Expected: FAIL (module not found).
- [ ] **Step 3: Implement the fonts.**
  - Implement `copyFonts` in `scripts/copy-fonts.mjs`. When run directly, it copies into the real studio folder.
  - Call it from `build-web.mjs` before `vite build`, and add it to `predev` (after `check-node`).
  - Add `studio/src/web/public/fonts/` to `.gitignore`.
- [ ] **Step 4: Run it and see it pass.** Run the same command. Expected: PASS.
- [ ] **Step 5: Write `ui/ui.css`.**
  - `:root` tokens, named after the theme: `--stripe-a`, `--stripe-b`, `--panel`, `--panel-border`, `--accent`, `--accent-light`, `--accent-dark`, `--text`, `--text-2`, `--text-label`, `--text-clock`, `--row-active`, `--track`, `--track-inset`, `--pill-top`, `--pill-bottom`, `--pill-border`, `--shadow`, `--ring`, `--warm`.
  - `@font-face` for 500 and 800 from `/fonts/…`.
  - Body: the font, `var(--text)`, and the striped `repeating-linear-gradient(0deg, var(--stripe-a) 0 6px, var(--stripe-b) 6px 12px)`.
  - The `ui-*` component classes, matching the mockup's look A (glossy pill gradient, primary pill `--accent-light` → `--accent` with white text, 14 px panels, focus ring `0 0 0 3px var(--ring)`).
- [ ] **Step 6: Write the components** in the files above, and a `Gallery` page that renders every component in each of its variants.
- [ ] **Step 7: Verify.** Run `npm run typecheck`, `npm test`, then `npm run serve` with the scratch settings on port 4790. Open `http://127.0.0.1:4790/#/ui` (wired in Task 3; until then, temporarily render `Gallery` from `main.tsx` and revert). Expected: the font is Rounded M+, the stripes show, and the pills match the mockup.
- [ ] **Step 8: Commit.** `git commit -m "wip | studio look tokens, font and components"`.

### Task 2: Drive-mode port override for browser checks

**Files:**
- Modify: `studio/src/server/main.ts:6`
- Test: `studio/test/check-node.test.ts` (sibling style); add `studio/test/main-port.test.ts` if that's cleaner

**Interfaces:**
- **Produces:** `export function drivePort(env: NodeJS.ProcessEnv): number` in `main.ts`, or in a new `src/server/port.ts` if `main.ts` can't be imported without starting the server. It returns `CTW_PORT` when that is a whole number from 1 to 65535, else `4747`.

- [ ] **Step 1: Write the failing tests.**
  - `port defaults to 4747`: `drivePort({}) === 4747`.
  - `port comes from CTW_PORT`: `drivePort({ CTW_PORT: "4791" }) === 4791`.
  - `bad CTW_PORT is ignored`: `drivePort({ CTW_PORT: "0" })` and `drivePort({ CTW_PORT: "x" })` both return `4747`.
- [ ] **Step 2: Run them and see them fail.**
- [ ] **Step 3: Implement it.** Put `drivePort` in `src/server/port.ts`, and make `main.ts` use it for `listen` and `open`.
- [ ] **Step 4: Run them and see them pass.**
- [ ] **Step 5: Commit.** `git commit -m "wip | studio drive port override"`.

### Task 3: Router, top bar shell, sign-in and Server/Drive in the new look

**Files:**
- Create: `studio/src/web/router.ts`, `studio/src/web/useRoute.ts`, `studio/src/web/Shell.tsx`
- Modify: `studio/src/web/App.tsx`, `studio/src/web/LoginPage.tsx`, `studio/src/web/pages/DrivePage.tsx`
- Test: `studio/test/router.test.ts`

**Interfaces:**
- **Produces, in `router.ts`:**
  ```ts
  export type ChannelTab = "today" | "schedule" | "shows" | "settings";
  export type Route =
    | { page: "episodes" }
    | { page: "add"; to: "episodes" | "ads"; channel: number | null }
    | { page: "channels" }
    | { page: "channel"; number: number; tab: ChannelTab }
    | { page: "folder"; folder: string }
    | { page: "ads" }
    | { page: "server" }
    | { page: "library" }
    | { page: "schedule" }
    | { page: "preview" }
    | { page: "ui" };
  export function parseRoute(hash: string): Route;
  export function routeHash(route: Route): string;
  export function topTab(route: Route): "episodes" | "channels" | "ads" | "server" | null;
  export function leaveAllowed(dirty: boolean, confirm: () => boolean): boolean;
  ```
- `library`, `schedule` and `preview` are transitional routes (`#/library`, `#/schedule`, `#/preview`), removed in Tasks 18 and 19. `ui` is `#/ui`, removed in Task 21.
- **Produces, in `useRoute.ts`:** `useRoute(dirty: boolean): [Route, (next: Route) => void]`.
  - It listens to `hashchange`.
  - If `leaveAllowed(dirty, () => window.confirm(UNSAVED_PROMPT))` is false, it puts the previous hash back with `history.replaceState` and keeps the route.
- **Produces, in `Shell.tsx`:** `Shell({ server: boolean; route: Route; onNavigate: (r: Route) => void; children })`. It is the top bar with the Studio name and the tabs.
- **Tabs in this stage:** Episodes (shows the old `LibraryPage` until Task 12), Channels (old `ChannelsPage`), Schedule (`#/schedule`), Preview (`#/preview`) and Server/Drive. Ads arrives in Task 20.

- [ ] **Step 1: Write the failing tests** in `test/router.test.ts`:
  - `parses every route`: each hash in Global Constraints maps to the expected `Route`, `#/channels/3` maps to `{ page: "channel", number: 3, tab: "today" }`, and `#/folders/mtv-00s` maps to `{ page: "folder", folder: "mtv-00s" }` (the folder is URI-decoded).
  - `unknown hash is episodes`: `parseRoute("#/nope")`, `parseRoute("")` and `parseRoute("#/channels/x")` all give `{ page: "episodes" }`.
  - `round trips`: `parseRoute(routeHash(r))` deep-equals `r` for each route, including a folder with a space and `&`.
  - `top tab`: `topTab({ page: "add", to: "ads", channel: null }) === "ads"`, a channel route gives `"channels"`, and a folder route gives `"channels"`.
  - `leaving a dirty page asks and can stay`: `leaveAllowed(true, () => false) === false`, `leaveAllowed(true, () => true) === true`, and `leaveAllowed(false, <throws>) === true` (it doesn't ask when clean).
- [ ] **Step 2: Run them and see them fail.** Run `npx vitest run test/router.test.ts`.
- [ ] **Step 3: Implement `router.ts`.**
- [ ] **Step 4: Run them and see them pass.**
- [ ] **Step 5: Implement the shell.**
  - Add `useRoute` and `Shell`.
  - In `App.tsx`, replace `tab` state with the route. `dirty` still comes from `SchedulePage`'s `onDirty`. A drive that isn't ready still shows the "choose a drive" panel on every page except Server/Drive.
  - Restyle `LoginPage` with `Panel`, `TextField` and `PillButton`.
  - Restyle `DrivePage` (Server and Drive) with `PageHead`, `Panel` and `PillButton`. Behaviour stays the same.
- [ ] **Step 6: Check in the browser.**
  - Start server mode on 4790 against a scratch copy of `samples/out/cable-twii`, laid out as `<library>/cable-twii/…`, with a generated test password in the settings file.
  - Start drive mode on 4791 (`CTW_PORT=4791 CTW_NO_OPEN=1 npm start`, then pick a scratch drive copy).
  - Check that the tabs switch, reload keeps the page, and Back works.
  - Check that an unsaved schedule edit followed by Back prompts, and that Cancel keeps the edit.
- [ ] **Step 7: Commit.** `git commit -m "wip | studio router and top bar"`.

## Stage 2 — Episodes home and Add episodes

### Task 4: Job history that survives a restart

**Files:**
- Modify: `studio/src/server/types.ts:27` (`Job`), `studio/src/server/queue.ts`
- Create: `studio/src/server/history.ts` (no Node imports, so the web can import it)
- Test: `studio/test/queue.test.ts`

**Interfaces:**
- **Produces:**
  - `Job` gains `finishedAt?: number` (ms since epoch, set on `done` and `failed`) and `missing?: boolean` (failed and the source no longer exists).
  - `QueueOptions` gains `now?: () => number` (default `Date.now`).
  - `export const HISTORY_MS = 7 * 24 * 60 * 60 * 1000` in `history.ts`, used by `queue.ts` and Task 8's `episodes.ts`.
- **History file:** `<root>/.studio/history.toml` holds `{ version = 1, job = [{ id, kind?, source, target, aspect, state, error?, finishedAt }] }`.
  - It is written atomically on every finish, through the same `writeChain` as `persist`.
  - It is loaded in the constructor, dropping entries older than `HISTORY_MS` and any `id` that is also in `queue.toml`.
- `list()` returns queue jobs plus history jobs. For failed jobs it sets `missing` with `existsSync(source)`.
- `retryFailed()` skips `missing` jobs.

- [ ] **Step 1: Write the failing tests** in `test/queue.test.ts`:
  - `keeps finished jobs across a restart`:
    - Enqueue two jobs: one with a `convertFn` that succeeds, one that throws `"boom"`.
    - Wait until both finish, stop the queue, and build a new `ConversionQueue` on the same root.
    - `list()` has both, with states `done` and `failed`, `error === "boom"`, and numeric `finishedAt`.
  - `drops history older than seven days`:
    - With `now: () => t`, finish a job, restart with `now: () => t + HISTORY_MS + 1`.
    - `list()` is empty.
  - `requeued job is not also in history`:
    - Stop while a job is running, then restart.
    - `list()` has that `id` exactly once, state `queued`.
  - `failed job with a gone source is missing and not retried`:
    - Fail a job, delete its source.
    - `list()[0].missing === true`, and `retryFailed()` returns `[]`.
- [ ] **Step 2: Run them and see them fail.** Run `npx vitest run test/queue.test.ts`.
- [ ] **Step 3: Implement it** in `queue.ts`.
- [ ] **Step 4: Run them and see them pass.** Then run the full `npm test`, since app tests use the queue.
- [ ] **Step 5: Commit.** `git commit -m "wip | studio job history"`.

### Task 5: Already-added checks

**Files:**
- Modify: `studio/src/server/drive.ts:46-53` (`ScanResult`), `studio/src/server/app.ts` (`/api/import/scan`, `/api/import/pick-source`, new `/api/upload/check`), `studio/src/web/api.ts`
- Test: `studio/test/imports.test.ts`, `studio/test/uploads.test.ts`, `studio/test/app.test.ts`

**Interfaces:**
- **Produces:**
  - `ScanResult` gains `added?: boolean`.
  - A module-level `async function alreadyConverted(root: string, records: SourceRecord[], source: string): Promise<boolean>` in `app.ts`. It is true when a record has this `source`, its target file exists, and `needsConversion(rec, stat(source), true)` is false.
  - `POST /api/upload/check`, server mode only:
    - Body: `{ files: { name: string; size: number }[] }`.
    - Response: `{ added: string[] }`, the names for which `convertedUpload(root, records, name, size)` is true.
  - In `api.ts`: `checkUploads(files: { name: string; size: number }[]): Promise<string[]>`.

- [ ] **Step 1: Write the failing tests:**
  - `scan marks files converted before` (`imports.test.ts`): import a folder, wait for the conversion, scan again. The file has `added: true` and a new file has no `added`.
  - `picked folder marks files converted before` (`app.test.ts`): the same, through `pickFolderFn`.
  - `upload check finds earlier uploads` (`uploads.test.ts`):
    - Upload `a.mp4`, import it, wait for the conversion.
    - `POST /api/upload/check` with `[{name:"a.mp4", size}, {name:"b.mp4", size:1}]` gives `{ added: ["a.mp4"] }`.
  - `upload check needs a session`: without the cookie, the answer is 401.
- [ ] **Step 2: Run them and see them fail.**
- [ ] **Step 3: Implement it.**
- [ ] **Step 4: Run them and see them pass.**
- [ ] **Step 5: Commit.** `git commit -m "wip | studio already-added checks"`.

### Task 6: Source preview endpoint

**Files:**
- Modify: `studio/src/server/app.ts`, `studio/src/web/api.ts`
- Test: `studio/test/imports.test.ts`, `studio/test/app.test.ts`

**Interfaces:**
- **Consumes:** `parseRange` from `src/server/wiiApi.ts:23` and `isVideoFile` from `names.ts`.
- **Produces:**
  - In `buildApp`, a `previewDirs: Set<string>` of realpaths. `/api/import/scan` adds the scanned folder, and `/api/import/pick-source` adds the picked folder.
  - `GET /api/import/file?path=<abs>`:
    - 200, or 206 with `content-range` and `accept-ranges: bytes` when a range is asked for. 416 when the range can't be satisfied.
    - Content types: `.mp4`/`.m4v` → `video/mp4`, `.mkv` → `video/x-matroska`, `.webm` → `video/webm`, `.mov` → `video/quicktime`, anything else → `application/octet-stream`.
    - 404 unless the realpath is inside one of `previewDirs` and `isVideoFile` is true.
    - Session-guarded as `/api/*` already is.
  - In `api.ts`: `sourceUrl(path: string): string` returns `/api/import/file?path=<encoded>`.

- [ ] **Step 1: Write the failing tests:**
  - `serves a scanned file with ranges`: after a scan, `range: bytes=0-9` gives 206, a 10-byte body, `content-range: bytes 0-9/<size>`, and `content-type: video/mp4` for `.mp4`.
  - `refuses a file outside the scanned folders`: a file in the imports folder but outside the scanned subfolder, and `/etc/hosts`, both give 404.
  - `refuses a symlink out of the folder`: a symlink in the scanned folder pointing outside it gives 404.
  - `refuses a non-video file`: a `.txt` in the folder gives 404.
- [ ] **Step 2: Run them and see them fail.**
- [ ] **Step 3: Implement it.** Reuse the 206/416 pattern in `wiiApi.ts:91-117`.
- [ ] **Step 4: Run them and see them pass.**
- [ ] **Step 5: Commit.** `git commit -m "wip | studio source preview endpoint"`.

### Task 7: Replace an episode

**Files:**
- Modify: `studio/src/server/app.ts:738-796` (`/api/import/show`), `studio/src/web/api.ts:39` (`ImportEpisode`)
- Test: `studio/test/app.test.ts`

**Interfaces:**
- **Produces:** `ImportEpisode` gains `replaces?: string`, the library-relative path of the episode file being replaced.
- **When set:**
  - It must be under `${channelFolder}/${showFolder}/`, end with `.mpg`, exist, and pass `assertContained`. Otherwise the answer is 400 `"replaces must be an episode of this show."`.
  - The job's target is `replaces`, not a new `uniqueName`. `earlierConversion` and `checkSource` still apply.

- [ ] **Step 1: Write the failing tests:**
  - `replace converts onto the existing file`:
    - With a fixture show that has `Season 1/S01E01 - Old.mpg`, import a new source with `replaces: "<ch>/<show>/Season 1/S01E01 - Old.mpg"`.
    - The returned job's `target` equals that path.
    - The file still has its old bytes while a `convertFn` blocked on a promise is running.
    - After release, it has the new bytes, and no `S01E01 - … (2).mpg` exists.
  - `replace outside the show is refused`: `replaces: "<other show>/…"` and `replaces: "../x.mpg"` both give 400.
  - `failed replace keeps the old file`: with a `convertFn` that writes `dst.part` and then throws, the old bytes stay and the job is `failed`.
- [ ] **Step 2: Run them and see them fail.**
- [ ] **Step 3: Implement it.**
- [ ] **Step 4: Run them and see them pass.**
- [ ] **Step 5: Commit.** `git commit -m "wip | studio replace an episode"`.

### Task 8: Job rows for the home page

**Files:**
- Create: `studio/src/web/episodes.ts`
- Test: `studio/test/episodes.test.ts`

**Interfaces:**
- **Consumes:** `Job` (with `finishedAt` and `missing`), `HISTORY_MS` from `../server/history.js`, and `formatEpisodeCode` from `preview.ts`.
- **Produces:**
  ```ts
  export interface TargetInfo { channel: string; show: string | null; season: number | null; episode: number | null; ad: boolean; name: string }
  export function describeTarget(target: string): TargetInfo;
  export type RowState = "uploading" | "queued" | "running" | "done" | "failed" | "missing" | "rejected";
  export interface JobRow { key: string; state: RowState; title: string; detail: string; progress: number | null; count: number; shrink: boolean; error: string | null; finishedAt: number | null }
  export interface UploadTask { id: string; name: string; size: number; loaded: number; state: "waiting" | "uploading" | "importing" | "done" | "rejected"; reason: string | null }
  export function homeRows(jobs: Job[], uploads: UploadTask[], now: number): { active: JobRow[]; recent: JobRow[] };
  ```
- **Target parsing:** `<ch>/<show>/Season N/SxxEyy - T.mpg`, `<ch>/<show>/T.mpg`, `ads/x.mpg` or `<ch>/ads/x.mpg`.
- **`active`:** uploads first, then `running`, each with its own row, progress from 0 to 1. Then `queued`.
- **`recent`:**
  - `done`, `failed`, `missing` and `rejected` uploads, where `finishedAt >= now - HISTORY_MS`, newest first.
  - Uploads are `rejected` with their reason.
- **Collapsing:** neighbouring `queued` rows, and neighbouring `done` rows, of the same channel, show and season (after sorting by target) become one row. Its `count` is the number of rows, and its title is `"Gumball S03E20–E40"`. Failed rows never collapse.
- **Detail text:** queued `"N waiting"`, done `"N on air"`, running `"62%"`, missing `"Add the file again"`. Shrink jobs get `shrink: true` and detail `"Making smaller"`.

- [ ] **Step 1: Write the failing tests** in `test/episodes.test.ts`:
  - `reads episode targets`: `describeTarget("Cartoon Network/Gumball/Season 3/S03E19 - The Pest.mpg")` deep-equals `{ channel: "Cartoon Network", show: "Gumball", season: 3, episode: 19, ad: false, name: "S03E19 - The Pest" }`. Also cover `ads/a.mpg` and `MTV/ads/a.mpg` (`ad: true`) and `mtv-00s/Music Videos/Song.mpg`.
  - `collapses queued runs`: 21 queued Gumball S03E20–E40 jobs give one active row with `title: "Gumball S03E20–E40"`, `count: 21` and `detail: "21 waiting"`.
  - `running rows stand alone`: one running job at progress 0.62 gives its own row, `progress: 0.62` and `detail: "62%"`.
  - `recent keeps seven days`: a done job with `finishedAt = now - HISTORY_MS - 1` is not in `recent`.
  - `failed rows never collapse`: two failed jobs of the same season give two rows, each with its `error`.
  - `missing source`: a failed job with `missing: true` gives state `missing` and detail `"Add the file again"`.
  - `uploads come first`: an uploading task at `loaded/size = 0.5` is `active[0]`, with progress 0.5.
- [ ] **Step 2: Run them and see them fail.**
- [ ] **Step 3: Implement it.**
- [ ] **Step 4: Run them and see them pass.**
- [ ] **Step 5: Commit.** `git commit -m "wip | studio home job rows"`.

### Task 9: Add-episodes rows, groups, destinations and duplicates

**Files:**
- Create: `studio/src/web/adding.ts`
- Test: `studio/test/adding.test.ts`

**Interfaces:**
- **Consumes:** `guessEpisode` from `../server/names.js` (it has no Node imports; a value import is fine for Vite), `ScanResult`, `Config`, `Job`, `WebLibrary` (`tree.ts`), `effectiveFolder` (`channels.ts`) and `describeTarget` (Task 8).
- **Produces:**
  ```ts
  export type RowNote = "added" | "on-air" | "in-batch" | "too-short" | "unreadable" | null;
  export interface AddRow { id: string; name: string; size: number | null; durationMs: number | null; aspect: "16:9" | "4:3" | null; season: number | null; episode: number | null; title: string; description: string; show: string; ticked: boolean; note: RowNote; replaces: string | null; source: string | null; file: File | null }
  export function guessShow(fileName: string): string;
  export function rowsFromFiles(files: File[], added: string[]): AddRow[];
  export function rowsFromScan(scan: ScanResult[]): AddRow[];
  export interface Destination { channel: string; show: string; isNewShow: boolean }
  export interface AddGroup { show: string; rows: AddRow[]; dest: Destination }
  export function groupRows(rows: AddRow[], fallbackShow: string, library: WebLibrary, config: Config, channel: number | null): AddGroup[];
  export function markExisting(group: AddGroup, library: WebLibrary, jobs: Job[]): AddGroup;
  export function summary(rows: AddRow[]): { ticked: number; total: number; bytes: number };
  ```
- **`guessShow`:** the text before the `SxxEyy` or `NxNN` match, cleaned like `guessEpisode`'s title (`.`/`_` become spaces, trimmed, trailing ` -` dropped). It returns `""` when there is no match or no text before it.
- **Rows:**
  - `rowsFromFiles`: rows in `added` are unticked with note `"added"`.
  - `rowsFromScan`: `added` is unticked with note `"added"`, `tooShort` with note `"too-short"`, `error` with note `"unreadable"`. All three are unticked.
- **`groupRows`:**
  - Groups by `show`, and rows with `""` use `fallbackShow`: the scanned folder's name, or the most common show among the rows for browser files.
  - **Channel:** a library show whose `name` or `folder` matches the group's show (case-insensitive) gives that show's channel and folder, with `isNewShow: false`.
  - Otherwise the channel is `effectiveFolder` of channel `channel` if given, else of the first channel by number. The show is the group's show, with `isNewShow: true`.
- **`markExisting`:**
  - A row whose season and episode match an episode of `dest.show` in the library is unticked, with note `"on-air"` and `replaces` set to that episode's `path`.
  - A row matching a `queued` or `running` job target in that show is unticked with note `"on-air"` and `replaces: null` (no Replace for queued ones).
  - A second row with the same season and episode within the group is unticked with note `"in-batch"`.

- [ ] **Step 1: Write the failing tests** in `test/adding.test.ts`:
  - `guesses the show`:
    - `guessShow("Gumball.S03E19.The.Pest.mkv") === "Gumball"`
    - `guessShow("The Amazing World of Gumball - 3x19 - The Pest.mp4") === "The Amazing World of Gumball"`
    - `guessShow("Pilot.mp4") === ""`
  - `files already added start unticked`: `rowsFromFiles([fileA, fileB], ["a.mp4"])` gives row a `ticked: false` with `note: "added"`, and row b ticked.
  - `scan problems start unticked`: a `tooShort` scan gives `"too-short"`, an `error` scan gives `"unreadable"`, and an `added` scan gives `"added"`.
  - `matches an existing show`: rows of "Gumball" with a library show `{channel:"Cartoon Network", folder:"Gumball"}` give `dest` `{ channel: "Cartoon Network", show: "Gumball", isNewShow: false }`.
  - `new show goes to the preferred channel`: with `channel: 2` (MTV, folder `mtv-00s`), `dest.channel === "mtv-00s"` and `isNewShow: true`.
  - `splits two shows`: Gumball and Ben 10 files give two groups, and a file with no show joins the fallback group.
  - `on-air episode offers replace`: S01E01 in the library gives `note: "on-air"`, unticked, with `replaces` equal to the library path.
  - `queued episode counts as existing`: a queued job target `Cartoon Network/Gumball/Season 3/S03E20 - The Sale.mpg` marks S03E20 `"on-air"` with `replaces: null`.
  - `second copy in the batch is unticked`: two S03E19 rows give the first ticked and the second `"in-batch"`, unticked.
  - `summary counts ticked bytes`: three rows (1 MB ticked, 2 MB ticked, 4 MB unticked) give `{ ticked: 2, total: 3, bytes: 3_000_000 }`.
- [ ] **Step 2: Run them and see them fail.**
- [ ] **Step 3: Implement it.**
- [ ] **Step 4: Run them and see them pass.**
- [ ] **Step 5: Commit.** `git commit -m "wip | studio add-episodes logic"`.

### Task 10: Episodes home and the upload runner

**Files:**
- Create: `studio/src/web/pages/EpisodesPage.tsx`, `studio/src/web/useUploads.ts`
- Modify: `studio/src/web/App.tsx`, `studio/src/web/api.ts` (`getJobs` type), `studio/src/web/ui/ui.css`

**Interfaces:**
- **Consumes:** `homeRows`, `UploadTask` (Task 8), `subscribeJobs`, `getJobs`, `retryJobs`, `mergeResync` (`jobs.ts`) and `uploadVideos`, `importShow`, `importAds`, `discardUploaded` (`api.ts`).
- **Produces, in `useUploads.ts`:**
  ```ts
  export interface AddPlan { kind: "show"; channelFolder: string; showFolder: string; showName: string; icon: File | null; rows: AddRow[] }
    | { kind: "ads"; channelFolder: string; rows: AddRow[] }
  export function useUploads(): { tasks: UploadTask[]; start: (plans: AddPlan[]) => void; dismiss: (id: string) => void }
  ```
- It lives in `App` so uploads survive page changes.
- `start` uploads ticked browser files in batches of 50 with `uploadVideos`, updating `loaded`.
- After each batch it calls `importShow` or `importAds` with that batch's `ScanResult`s, matched to rows by file name:
  - `title`, `season`, `episode`, `description` and `replaces` come from the row.
  - `aspect` comes from the scan.
- Rejected uploads and rejected imports become `rejected` tasks with their reason. Uploaded files that weren't imported go to `discardUploaded`.
- Rows from a scan (`source` set) skip the upload and are imported straight away.
- The show icon is uploaded with `uploadIcon("show", …)` after the first successful import of that show.
- **`EpisodesPage({ server: boolean; uploads: ReturnType<typeof useUploads> })`:**
  - `PageHead` "Episodes".
  - A drop zone panel: drag files on it or click to pick, which navigates to `#/add` with the files handed over through a small module-level hand-off in `useUploads.ts`, `pendingFiles`.
  - Server mode adds "Use the imports folder"; drive mode adds "Pick a folder".
  - Then "In progress", "Recent" and "Retry failed" from `homeRows`.
  - "Keep this tab open until uploads finish" shows while any task is `waiting` or `uploading`.
  - The empty state is "No episodes yet. Add some to get started."
- The Episodes tab now shows `EpisodesPage`. The old Library page moves to the transitional `#/library` route, with a "Library" tab until Task 18.

- [ ] **Step 1: Implement it.** The logic is covered by Tasks 8 and 9. This task is wiring and look.
- [ ] **Step 2: Typecheck and test.** Run `npm run typecheck && npm test`. Expected: PASS.
- [ ] **Step 3: Check in the browser** (server mode, port 4790). Use "Use the imports folder" against a scratch imports folder holding two small sample `.mpg` copies renamed `.mp4`. After Task 11, check that the rows move from In progress to Recent, and that a restart keeps Recent.
- [ ] **Step 4: Commit.** `git commit -m "wip | studio episodes home"`.

### Task 11: Add episodes page

**Files:**
- Create: `studio/src/web/pages/AddPage.tsx`, `studio/src/web/library/ImportsBrowser.tsx` (moved from `AddVideosDialog`'s `ImportsTab`), `studio/src/web/library/SourcePreview.tsx`
- Modify: `studio/src/web/App.tsx`, `studio/src/web/ui/ui.css`

**Interfaces:**
- **Consumes:** Task 9's `rowsFromFiles`, `rowsFromScan`, `groupRows`, `markExisting` and `summary`, plus `checkUploads`, `scanImports`, `getImports`, `pickSource`, `sourceUrl`, `getLibrary`, `getConfig`, `getJobs`, `useUploads().start`, `ChannelPicker`'s choices (`channelChoices`) and `NEW_CHANNEL`.
- **Produces:**
  - `AddPage({ route: Extract<Route, { page: "add" }>; server: boolean; uploads; onDone: () => void })`.
  - `SourcePreview({ src: string; onClose: () => void })`: a `<video controls>` in a `ConfirmDialog`-style overlay. On `error` it shows "This browser can't preview this file".
- **Steps in the page:**
  1. **No files yet.** Show the pick, drop, imports-folder (server mode) or pick-folder (drive mode) choices.
  2. **Files picked.** The groups, each with a destination bar holding a Channel select (channel choices plus "New channel…", which asks for a name inline and creates it with `applyEdit(newDraft(...))` + `putConfig` on submit) and a Show select (that channel's shows plus "New show…", which asks for a name and an optional icon). Then the rows table, then the footer.
- **Rows:**
  - Tick, preview, `S03E19` code (season and episode editable), file name, title (read-only when `replaces` is set and ticked), length (`formatDuration`, or `—`) and a note chip.
  - Notes: `"Already on air · Replace?"` (ticking it keeps `replaces`), `"Already added"`, `"Too short"`, `"Can't read this file"`.
  - Description opens per row.
- **Browser files:** lengths come from a hidden `<video preload="metadata">` per file, filled in as they load.
- **Ads mode** (`to: "ads"`): one group, no season or episode columns, and a destination of "All channels" or a channel (`ChannelPicker` with `all`).
- **Footer:** "N of M ticked · X to upload" (`formatBytes`; for scan sources "N of M ticked"), Cancel, and the primary "Add N episodes" (or "Add N ads").
- **Submit:** build `AddPlan`s from the groups and call `start`. In drive mode and for imports-folder sources, the same `start` imports directly. Then `onDone()` navigates to `#/`.

- [ ] **Step 1: Implement it.**
- [ ] **Step 2: Typecheck and test.** Run `npm run typecheck && npm test`.
- [ ] **Step 3: Check in the browser** (both modes):
  - Untick one file and check that the server's `.uploads` never receives it.
  - Check that previews play for `.mp4` and that a non-playable file shows the message.
  - Re-adding a converted episode shows "Already on air · Replace?", and Replace works.
  - "New show…" and "New channel…" both work.
  - Ads mode works.
  - From `#/channels/1` (after Task 16) the channel is preselected.
- [ ] **Step 4: Commit.** `git commit -m "wip | studio add episodes page"`.

### Task 12: Retire the add dialogs

**Files:**
- Delete: `studio/src/web/library/AddVideosDialog.tsx`, `ImportShowDialog.tsx`, `ImportAdsDialog.tsx`, `JobsPanel.tsx`
- Modify: `studio/src/web/pages/LibraryPage.tsx` (its Add show / Add ads buttons navigate to `#/add` / `#/add?to=ads`, and its conversions panel goes away), `studio/src/web/importing.ts` plus its tests (remove exports nothing uses)

- [ ] **Step 1: Remove the dialogs** and repoint the buttons.
- [ ] **Step 2: Typecheck and test.** Run `npm run typecheck && npm test`.
- [ ] **Step 3: Commit.** `git commit -m "wip | studio retire add dialogs"`.

## Stage 3 — Channels

### Task 13: Rebuild notes

**Files:**
- Modify: `studio/src/server/app.ts:332-353` (`rebuildNow`, `scheduleRebuild`), `studio/src/web/api.ts`
- Test: `studio/test/app.test.ts`

**Interfaces:**
- **Produces:**
  - In `buildApp`, `lastBuild: { at: number; skipped: SkippedFile[]; warnings: string[] } | null`, set at the end of every `rebuildNow`. The scheduled rebuild goes through `rebuildNow`.
  - `GET /api/library/notes` returns `lastBuild ?? { at: null, skipped: [], warnings: [] }`.
  - In `api.ts`: `getNotes(): Promise<{ at: number | null; skipped: { path: string; reason: string }[]; warnings: string[] }>`.

- [ ] **Step 1: Write the failing tests:**
  - `notes start empty`: `{ at: null, skipped: [], warnings: [] }`.
  - `notes keep the last automatic rebuild`:
    - Write `ads/short-x.mpg` into the fixture root. The fake probe makes `short…` names 500 ms long.
    - Import any show to trigger `scheduleRebuild`.
    - After `waitFor`, the notes list `ads/short-x.mpg` with the too-short reason.
- [ ] **Step 2: Run them and see them fail.**
- [ ] **Step 3: Implement it.**
- [ ] **Step 4: Run them and see them pass.**
- [ ] **Step 5: Commit.** `git commit -m "wip | studio rebuild notes"`.

### Task 14: Channel summaries, folder pages and rename safety

**Files:**
- Create: `studio/src/web/channelList.ts`
- Modify: `studio/src/web/channels.ts` (`applyEdit` or `toChannel`)
- Test: `studio/test/channelList.test.ts`, `studio/test/channels.test.ts`

**Interfaces:**
- **Consumes:** `Config`, `WebLibrary`, `Issue`, `LineupResult`, `Job`, `effectiveFolder`, `nowPosition`, `groupAirings` and `describeTarget`.
- **Produces:**
  ```ts
  export interface ChannelSummary { number: number; name: string; folder: string; icon: string; mode: "timed" | "looping"; shows: number; episodes: number; now: string | null; warnings: string[] }
  export function channelSummaries(config: Config, library: WebLibrary, issues: Issue[]): ChannelSummary[];
  export function nowLine(lineup: LineupResult, ms: number): string | null;
  export function orphanFolders(config: Config, library: WebLibrary): string[];
  export type EpisodeStatus = { state: "on-air" } | { state: "queued" } | { state: "running"; progress: number } | { state: "failed"; error: string };
  export function episodeStatus(path: string, jobs: Job[]): EpisodeStatus;
  ```
- **`channelSummaries`:**
  - Sorted by number. `icon` is `iconPath(ch)`.
  - `warnings`: `"No shows yet"` when no library show has this folder. Then each `issues` entry with this `channel` and level `error`, then each with level `warning`, as `describeIssue` text.
  - `now` is filled later from `nowLine`.
- **`nowLine`:** `"Gumball S03E04 · The Hero"` for the airing at `ms`, `"Ad break"` for ads, `null` when nothing is on.
- **`orphanFolders`:** library show channel folders that no channel's `effectiveFolder` matches, sorted.
- **`episodeStatus`:** the newest queued or running job for that target wins; otherwise a failed job newer than the file; otherwise `on-air`.
- **Rename rule, in `toChannel` or `applyEdit`:** when an edited channel had no explicit folder and its name changes, the saved channel gets `folder` set to the old name.

- [ ] **Step 1: Write the failing tests:**
  - `rename keeps the old folder`: editing `{name:"Cartoon Network"}` (no folder) to name "CN" saves `{name:"CN", folder:"Cartoon Network"}`.
  - `rename with explicit folder keeps it`: `folder:"mtv-00s"` stays.
  - `summaries count shows and flag empty channels`: with the sample config and library, Cartoon Network has 3 shows and 12 episodes; a channel with no shows has `warnings[0] === "No shows yet"`.
  - `summaries carry issues`: an error issue on channel 1 appears in channel 1's warnings.
  - `now line`: an airing at the given ms gives `"Gumball S03E04 · The Hero"`, an ad gives `"Ad break"`, and the time after the last airing gives `null`.
  - `orphan folders`: a library show in folder `"Old Channel"` with no matching channel gives `["Old Channel"]`.
  - `episode status`: a queued job for the path gives `queued`, a running one at 0.4 gives `{state:"running", progress:0.4}`, and none gives `on-air`.
- [ ] **Step 2: Run them and see them fail.**
- [ ] **Step 3: Implement it.**
- [ ] **Step 4: Run them and see them pass.**
- [ ] **Step 5: Commit.** `git commit -m "wip | studio channel summaries"`.

### Task 15: Channels list and folder pages

**Files:**
- Create: `studio/src/web/pages/ChannelListPage.tsx`, `studio/src/web/pages/FolderPage.tsx`
- Modify: `studio/src/web/App.tsx`

**Interfaces:**
- **Consumes:** `channelSummaries`, `nowLine`, `orphanFolders` (Task 14), `getConfig`, `getLibrary`, `validate`, `getPreview(n, localDate(now))` (one per channel, run in parallel) and `newDraft`, `applyEdit`, `putConfig` for Add channel.
- **Produces:**
  - `ChannelListPage({ onNavigate })`. Rows match the mockup: number pill, logo (`IconImage`), name, "Now …", mode chip, "N shows · M episodes" chip and warning chips.
  - "Add channel" creates a channel from `newDraft` with a placeholder schedule, saves it, and navigates to its `settings` tab.
  - The "Not on a channel" section lists `orphanFolders`.
  - `FolderPage({ folder })`: `PageHead` with the folder name and the Shows view (`ShowsTab` from Task 17, given `folder`). Until Task 17 it uses a simple list.

- [ ] **Step 1: Implement it.**
- [ ] **Step 2: Typecheck and test.**
- [ ] **Step 3: Check in the browser:** the sample library shows Cartoon Network, MTV, "No shows yet" chips and the now-playing lines.
- [ ] **Step 4: Commit.** `git commit -m "wip | studio channels list"`.

### Task 16: Channel page and Today tab

**Files:**
- Create: `studio/src/web/pages/ChannelPage.tsx`, `studio/src/web/channel/TodayTab.tsx` (from `PreviewPage.tsx`)
- Modify: `studio/src/web/App.tsx`

**Interfaces:**
- **Consumes:** `Tabs`, `groupAirings`, `nowPosition`, `adBreakLabel`, `formatClock`, `formatEpisodeCode`, `findShow`, `localDate`, `localMs`, `getPreview` and `FailureBanner`.
- **Produces:**
  - `ChannelPage({ number: number; tab: ChannelTab; onNavigate; onDirty: (d: boolean) => void })`.
  - The header has the number pill, logo, name, "category · Mode · ads on/off" and the primary "Add episodes" (`#/add?channel=N`).
  - It uses `Tabs` for Today, Schedule, Shows and Settings. Schedule shows "Coming in the next stage" with a link to `#/schedule` until Task 19.
  - `TodayTab({ channel: Channel })`: the lineup with the now row highlighted (`--row-active`, accent left border, `ProgressBar`), plus a date `TextField` and a "Today" pill.
  - An unknown channel number shows `EmptyState` "No channel N." and a link to `#/channels`.

- [ ] **Step 1: Implement it.**
- [ ] **Step 2: Typecheck and test.**
- [ ] **Step 3: Check in the browser:** `#/channels/1/today` matches the mockup's right half, and changing the date works.
- [ ] **Step 4: Commit.** `git commit -m "wip | studio channel page and today"`.

### Task 17: Shows tab

**Files:**
- Create: `studio/src/web/channel/ShowsTab.tsx`, `studio/src/web/channel/ShowInfoDialog.tsx` (from `library/ShowMetaDialog.tsx`, restyled, plus an icon field using `uploadIcon("show", …)`)
- Delete: `studio/src/web/library/ShowMetaDialog.tsx`

**Interfaces:**
- **Consumes:** `buildTree`, `seasonLabel`, `descriptionKey` (`tree.ts`), `episodeStatus` (Task 14), `getJobs` and `subscribeJobs`.
- **Produces:**
  - `ShowsTab({ folder: string })`: that channel folder's shows, each a `Panel` with its icon, name and "Edit info".
  - Seasons are listed with their episode rows: code, title, length and a status chip (On air / Converting 62% / Queued / Failed). Ads are not listed here.
  - The empty state is "No shows yet." with an "Add episodes" pill.
  - `ShowInfoDialog({ node: ShowNode; channelFolder: string; onClose; onSaved(library) })`.

- [ ] **Step 1: Implement it.**
- [ ] **Step 2: Typecheck and test.**
- [ ] **Step 3: Check in the browser:** status chips update live while a conversion runs, Edit info saves the name, descriptions and icon, and the folder page uses the same view.
- [ ] **Step 4: Commit.** `git commit -m "wip | studio shows tab"`.

### Task 18: Settings tab and retiring Library, Preview and the Channels table

**Files:**
- Create: `studio/src/web/channel/SettingsTab.tsx` (the form from `ChannelsPage.tsx`'s dialog)
- Delete: `studio/src/web/pages/ChannelsPage.tsx`, `pages/LibraryPage.tsx`, `pages/PreviewPage.tsx`
- Modify: `studio/src/web/App.tsx`, `studio/src/web/router.ts` (drop `library` and `preview`) plus `test/router.test.ts`

**Interfaces:**
- **Consumes:** `draftFromChannel`, `setMode`, `validateDraft`, `applyEdit` (with Task 14's rename rule), `applyDelete`, `folderSuggestions`, `uploadIcon`, `putConfig`, `FailureBanner`, `ConfirmDialog`.
- **Produces:** `SettingsTab({ number: number; onDeleted: () => void; onSaved: () => void })`.
  - Fields: Name, Number, Category, Folder (datalist of `folderSuggestions`, placeholder = name), Logo, Ads (Checkbox) and Mode (`Select`: Timed / Looping).
  - Save and Cancel pills.
  - Delete channel opens a `ConfirmDialog` with "Delete channel N name? Its files stay on the drive." and `danger`, then navigates to `#/channels`.
  - A 409 conflict shows `FailureBanner` with Reload.

- [ ] **Step 1: Update the router test.** `parseRoute("#/library")` and `parseRoute("#/preview")` now give `{ page: "episodes" }`.
- [ ] **Step 2: Run it and see it fail.**
- [ ] **Step 3: Implement it.** Remove the transitional routes, tabs and pages.
- [ ] **Step 4: Run the tests and see them pass.** Then run `npm run typecheck`.
- [ ] **Step 5: Check in the browser:** rename Cartoon Network to "CN" and its shows stay. Switch the mode and save. Delete a scratch channel.
- [ ] **Step 6: Commit.** `git commit -m "wip | studio channel settings"`.

## Stage 4 — Schedule

### Task 19: Schedule tab

**Files:**
- Create: `studio/src/web/channel/ScheduleTab.tsx` (from `pages/SchedulePage.tsx`)
- Delete: `studio/src/web/pages/SchedulePage.tsx`
- Modify: `studio/src/web/schedule/ScheduleToolbar.tsx` (drop the channel chips and the `channels`/`onChannel` props), `studio/src/web/schedule/useScheduleEditor.ts` (take `number: number` instead of holding `selected`), `studio/src/web/styles.css` (schedule editor rules move into `ui/ui.css` restyled, as `.ed`, `.tl`, `.blk` and friends, rewritten with the tokens), `studio/src/web/router.ts` (drop `schedule`) plus `test/router.test.ts`
- Create: `studio/src/web/schedule/zoom.ts`
- Test: `studio/test/schedule-zoom.test.ts`

**Interfaces:**
- **Produces:**
  - `ScheduleTab({ number: number; onDirty: (d: boolean) => void })`.
  - `useScheduleEditor(number: number)`, which returns the same object minus `channels` and `selectChannel`.
  - In `zoom.ts`: `defaultZoom(mode: "timed" | "looping"): number` returns 30 for timed and 120 for looping.
- `dirty` reaches `App` through `ChannelPage`'s `onDirty`, so `useRoute`'s guard covers tab changes inside the channel page too.

- [ ] **Step 1: Write the failing tests.**
  - `timed opens at 30 px per hour`: `defaultZoom("timed") === 30`.
  - `looping opens at 120 px per hour`: `defaultZoom("looping") === 120`.
  - In `test/router.test.ts`, `#/schedule` now gives `{ page: "episodes" }`.
- [ ] **Step 2: Run them and see them fail.**
- [ ] **Step 3: Implement it.**
- [ ] **Step 4: Run them and see them pass.** Then run `npm run typecheck && npm test`.
- [ ] **Step 5: Check in the browser:**
  - Cartoon Network and MTV schedules open at the right zoom.
  - Dragging blocks, undo/redo, Reshuffle and Save all work.
  - With unsaved edits, switching to Today asks, and Cancel keeps the edits.
- [ ] **Step 6: Commit.** `git commit -m "wip | studio schedule tab"`.

## Stage 5 — Ads, Server and clean-up

### Task 20: Ads page, and Re-read library on Server/Drive

**Files:**
- Create: `studio/src/web/ads.ts`, `studio/src/web/pages/AdsPage.tsx`
- Modify: `studio/src/web/pages/DrivePage.tsx`, `studio/src/web/Shell.tsx` (add the Ads tab), `studio/src/web/App.tsx`
- Test: `studio/test/ads.test.ts`

**Interfaces:**
- **Consumes:** `getLibrary`, `getConfig`, `getNotes` (Task 13), `rebuildLibrary` and `formatDuration`.
- **Produces:**
  ```ts
  export interface AdGroup { channel: string; label: string; ads: { path: string; name: string; durationMs: number }[] }
  export function adGroups(library: WebLibrary, config: Config): AdGroup[];
  export function skippedAds(notes: { skipped: { path: string; reason: string }[] }): { path: string; reason: string }[];
  ```
- **`adGroups`:** "All channels" first (`channel: ""`), then channels by number, using their name as the label (or the folder name when no channel matches). `name` is the file name without `.mpg`.
- **`skippedAds`:** entries whose path is `ads/…` or `<ch>/ads/…`.
- **`AdsPage({ onNavigate })`:** `PageHead` "Ads" with "Add ads" (`#/add?to=ads`), a `Panel` per group, and a "Skipped" panel with the reasons when there are any.
- **`DrivePage`:** gets a "Re-read library" pill calling `rebuildLibrary` and showing its skipped files and warnings. It shows `getNotes()` on load.

- [ ] **Step 1: Write the failing tests:**
  - `groups ads by channel`: the sample library gives `[{ channel: "", label: "All channels", ads: [ad-a, ad-c] }]`, sorted by name.
  - `channel ads use the channel name`: a library ad with `channel: "Cartoon Network"` gives a group labelled "Cartoon Network" after "All channels".
  - `skipped ads only`: notes with `ads/ad-b.mpg` and `Cartoon Network/Gumball/x.mpg` give only the ad.
- [ ] **Step 2: Run them and see them fail.**
- [ ] **Step 3: Implement it.**
- [ ] **Step 4: Run them and see them pass.**
- [ ] **Step 5: Check in the browser:** the Ads tab shows ad-a and ad-c, ad-b is under Skipped, and Re-read library refreshes the notes.
- [ ] **Step 6: Commit.** `git commit -m "wip | studio ads page"`.

### Task 21: Clean-up, docs and the full check

**Files:**
- Modify: `studio/src/web/styles.css` (delete every rule no remaining component uses; move what's left into `ui/ui.css` and delete `styles.css`, together with its import in `main.tsx`), `studio/src/web/router.ts` (drop `ui`) plus `test/router.test.ts`, `docs/USER-GUIDE.md` (rewrite the Studio sections: Episodes, Add episodes, Channels and the channel page, Ads, Server/Drive), `CHANGELOG.md` (Unreleased: "Studio has a new look and layout: an Episodes home, a full-page Add episodes, and one page per channel")
- Delete: `studio/src/web/ui/Gallery.tsx`, `studio/src/web/library/Dialog.tsx` (if unused), and any web module with no remaining importers (check with `grep -r`)

- [ ] **Step 1: Update the router test.** `#/ui` gives `{ page: "episodes" }`.
- [ ] **Step 2: Remove the dead code and CSS.** Run `npm run typecheck && npm test`. Expected: PASS.
- [ ] **Step 3: Do the full browser pass in both modes** against fresh scratch copies of `samples/out/cable-twii`:
  1. Sign in.
  2. Add episodes from browser files, the imports folder and a picked folder (drive mode), with one file unticked.
  3. Use Replace.
  4. Restart the server and check that Recent stays.
  5. Use Retry failed.
  6. Check the channels list, every channel tab, a rename, the schedule editor, Ads, Re-read library and Sign out.
  7. Check the narrowest supported width (1024 px).
- [ ] **Step 4: Update the docs.** Rewrite the Studio sections of `docs/USER-GUIDE.md` and add the `CHANGELOG.md` line.
- [ ] **Step 5: Commit.** `git commit -m "wip | studio redesign clean-up and guide"`.
