# cable-twii Studio Library Management Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give every episode and ad its own page (player, info, On/Off air, Delete), add a Shows tab with a page per show and bulk episode actions, and let each show be ticked onto any channel, without changing the Wii app or the formats it reads.

**Architecture:**
- Pure, node-tested modules hold the logic:
  - schedule references (`src/server/scheduleRefs.ts`, no Node imports, shared with the web);
  - off-air storage;
  - library editing plans;
  - Shows-tab views.
- Every library change goes through one server pipeline, `POST /api/library/edit`. It locks, checks for conflicts and running conversions, applies the change, validates the new config against the new library (refusing only for new errors), rolls back on failure, and rebuilds the library.
- Playback re-packages `.mpg` into MPEG-TS with `ffmpeg -c copy`. A vendored JSMpeg script plays it in the browser.

**Tech Stack:** TypeScript, React 18, plain CSS, Vite 5, Fastify 5, vitest 3 (node environment), Node 22, ffmpeg, JSMpeg (vendored, MIT).

**Spec:** `docs/superpowers/specs/2026-11-20-studio-library-management-design.md` (builds on `2026-11-20-studio-redesign-design.md`).

## Global Constraints

- **Wii and formats:** no Wii app or engine changes. `library.toml` and `config.toml` keep their shape (`docs/formats.md` is frozen). New state lives only in `.studio/`.
- **Dependencies:** no new packages. One exception: `studio/src/web/vendor/jsmpeg.min.js` with its `LICENSE`, committed. Before downloading it, ask the owner: name the file, the source (the official JSMpeg GitHub release, MIT) and the size.
- **Exact copy:**
  - "On air"
  - "Off air"
  - "Not airing"
  - "Airs on"
  - "Can't play this file here"
  - "Still converting. Try again when it's done."
  - "Delete can't be undone."
  - Banner suffix in server mode: "A Wii playing from the server picks this up when its guide opens, at midnight, or with Reload channels."
- **Routes added:**
  - `#/shows`
  - `#/shows/<channel folder>/<show folder>`
  - `#/episode/<path>`
  - `#/ad/<path>`

  Folders and paths are URI-encoded per segment.
- **Top tabs:** Episodes, Channels, Shows, Ads, Server/Drive.
- **Same look:** use the existing `ui/` components and tokens.
- **House rules (unchanged from the redesign plan):**
  - Match sibling idioms.
  - No redundant comments.
  - No trailing whitespace.
  - Commit messages are `wip | …` / `fix | …` / `test | …`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
  - Don't push or merge without asking.
- **Checks:**
  - `cd studio && npm test` and `npm run typecheck` pass before every commit.
  - Browser checks use ports 4790 and 4791, never 4747 or 4748.
  - Stop only the processes you started, by PID.
- **Where:**
  - Work on branch `studio-redesign`, in a worktree.
  - The main checkout may be on another session's branch; never switch it.
  - The worktree's `studio/node_modules` may be a symlink to the main checkout's. Never commit it.

## Review Focus

1. **Renaming in a show without seasons:** the `episode` items that pointed at the reordered episodes still play the same videos afterwards. Task 6, test `seasonless rename keeps picks on the same videos`.
2. **Rollback:** a failure part-way through leaves every file, `sources.toml`, `off.toml`, `show.toml` and `config.toml` as they were. Task 7, test `a failed edit puts everything back`.
3. **Running conversions:** a file with a queued or running job can't be renamed, switched off or deleted. The answer is 409 with "Still converting. Try again when it's done." Task 7, test `a converting file can't be edited`.
4. **Errors already there don't block; new errors do.** Task 5, test `only new errors count`. Task 7, test `an edit that adds an error is refused and changes nothing`.
5. **The play path can't escape the library:** `..`, a symlink out of the root, or a non-`.mpg` file gives 404. Task 2, test `play refuses paths outside the library`.

---

## Stage 1 — Pages and player

### Task 1: Schedule references

**Files:**
- Create: `studio/src/server/scheduleRefs.ts` (types-only imports, so the web can import it)
- Modify: `studio/src/web/channels.ts` (`effectiveFolder` becomes a re-export of `channelFolder`)
- Test: `studio/test/scheduleRefs.test.ts`

**Interfaces:**
- **Produces:**
  ```ts
  export interface ShowRef { channel: string; show: string }
  export interface ItemRef { channel: number; slot: number; item: number }
  export function channelFolder(ch: Pick<Channel, "name" | "folder">): string;
  export function itemShow(item: Item, home: string): ShowRef | null;
  export function channelsAiring(config: Config, ref: ShowRef): number[];
  export function episodeChannels(config: Config, ref: ShowRef, season: number, episode: number): number[];
  export function adChannels(config: Config, library: Pick<Library, "ad">, path: string): number[];
  export function itemsNaming(config: Config, target: { show: ShowRef; season?: number; episode?: number } | { ad: string }): ItemRef[];
  export function removeItems(config: Config, refs: ItemRef[]): Config;
  export function retargetEpisodes(config: Config, ref: ShowRef, moves: { from: [number, number]; to: [number, number] }[]): Config;
  export function renameAd(config: Config, from: string, to: string): Config;
  export function appendItems(config: Config, channel: number, slot: number | null, items: Item[]): Config;
  export function describeItem(item: Item, showName: (ref: ShowRef) => string): string;
  ```
- **Rules:**
  - An item names `{ channel: item.from ?? home, show: item.show }`.
  - `slot` is `-1` for loop items, else the index into the channel's slots as stored (sorted by `at`).
  - `episodeChannels` counts:
    - `episode` items with that season and episode;
    - `random`, `random-run` and `fill` items whose `season` is missing or equal. Season 0 means a show without seasons.
  - `adChannels` counts:
    - channels with `ads: true` whose random ads come from the ad's folder. That is the channel's own `ads/` if the library has any ad there, else the shared `ads/`;
    - plus `ad` items naming the path.
  - `itemsNaming` with only `show` returns every item naming the show. With `season` and `episode` it returns only `episode` items for that episode.
  - `appendItems` with `slot === null` appends to the loop, else to the end of that slot.
  - `describeItem` gives "Random episode ×1 · Gumball", "S03E19 · Gumball", "Ad · Cola", "Ad break ×2", etc.

- [ ] **Step 1: Write the failing tests.**
  - `names the home folder unless from is set`
  - `channels airing a show`: a channel whose own folder holds it, and one using `from`.
  - `episode channels`: a pick, a season rule, and an all-seasons rule; a rule for another season doesn't count.
  - `ad channels`:
    - the shared ads reach channels without their own ads only;
    - `ads: false` channels don't count;
    - an `ad` item counts.
  - `items naming an episode are episode items only`
  - `remove items keeps the rest in order`
  - `retarget follows moved episodes`: `episode` items for (1,3) become (1,2) and other shows are untouched.
  - `append to loop and to a slot`
  - `describe items`
- [ ] **Step 2: Run them and see them fail.** Run `npx vitest run test/scheduleRefs.test.ts`.
- [ ] **Step 3: Implement them.** Point `channels.ts` `effectiveFolder` at `channelFolder`.
- [ ] **Step 4: Run them and see them pass.** Then run `npm test` (the existing channel tests must stay green).
- [ ] **Step 5: Commit.** `wip | studio schedule references`

### Task 2: Play endpoint

**Files:**
- Modify: `studio/src/server/ffmpeg.ts` (add `remux`), `studio/src/server/app.ts` (`AppOptions.remuxFn`, the route), `studio/src/web/api.ts`
- Test: `studio/test/app.test.ts`

**Interfaces:**
- **Produces:**
  - `export function remux(file: string, startSec: number): { stream: Readable; stop: () => void }` in `ffmpeg.ts`.
    - It spawns `ffmpeg -v error -ss <start> -i file:<file> -c copy -f mpegts pipe:1`.
    - `stop` kills the process.
  - `AppOptions.remuxFn?: typeof remux`.
  - `GET /api/media/play?path=<rel>&start=<seconds>`:
    - `content-type: video/mp2t`, no `content-length`;
    - `start` is a whole number of seconds from 0, and anything else is 400;
    - when the request closes, it calls `stop`.
  - In `api.ts`: `playUrl(path: string, start: number): string`.
- **Path checks:** the same as `/media/*`. The realpath must be inside the root's realpath, end in `.mpg`, and be a regular file. Otherwise the answer is 404.

- [ ] **Step 1: Write the failing tests** (with a fake `remuxFn` that records its arguments and returns a stream of `"ts"`):
  - `play streams the remux of a library file`: 200, body `"ts"`, `video/mp2t`, the fake gets the absolute path and `start` 0.
  - `play passes the start time`: `start=95` reaches the fake as 95. `start=-1` and `start=x` give 400.
  - `play refuses paths outside the library`: `../x.mpg`, a symlink in the root pointing outside, `config.toml`, and a missing file all give 404.
  - `play stops ffmpeg when the client goes away`: abort the request and `stop` is called.
  - `play needs a session` (server mode, in `imports.test.ts`'s setup): 401 without the cookie.
- [ ] **Step 2: Run them and see them fail.**
- [ ] **Step 3: Implement it.**
- [ ] **Step 4: Run them and see them pass.** Add one real-ffmpeg test, `remux produces mpegts`, that remuxes a 2-second sample made with ffmpeg in a temp dir and checks the output starts with the `0x47` sync byte. Skip it if ffmpeg is missing, the way `ffmpeg.test.ts` does.
- [ ] **Step 5: Commit.** `wip | studio play endpoint`

### Task 3: JSMpeg player component

**Files:**
- Create: `studio/src/web/vendor/jsmpeg.min.js`, `studio/src/web/vendor/LICENSE-jsmpeg`, `studio/src/web/vendor/jsmpeg.d.ts`, `studio/src/web/ui/MpegPlayer.tsx`
- Modify: `studio/src/web/ui/ui.css`

**Interfaces:**
- **Produces:** `MpegPlayer({ path: string; durationMs: number; aspect: "16:9" | "4:3" })`.
  - A canvas player: play/pause, `m:ss / m:ss`, and a progress bar.
  - Clicking the bar restarts the stream at that second (a new `JSMpeg.Player(playUrl(path, s))`, with the old one destroyed).
  - If the stream errors, or no picture arrives in 8 seconds, it shows "Can't play this file here".
  - The player is destroyed on unmount.

- [ ] **Step 1: Get JSMpeg.** Ask the owner first, naming the file, the source and the size. Then save `jsmpeg.min.js` and its MIT licence, and write a minimal `jsmpeg.d.ts` declaring the `JSMpeg.Player` options used (`canvas`, `audio`, `autoplay`, `onVideoDecode`, `onSourceCompleted`) and `destroy`, `play`, `pause`, `currentTime`.
- [ ] **Step 2: Write the component and styles.** The canvas keeps the file's aspect, with the panel radius and a dark backdrop.
- [ ] **Step 3: Typecheck and test.**
- [ ] **Step 4: Check in the browser** (port 4790, scratch library):
  - an episode plays with sound;
  - seeking to the middle works;
  - a deleted file shows the message.
- [ ] **Step 5: Commit.** `wip | studio mpeg player`

### Task 4: Routes, Shows list, show page, episode and ad pages (read-only)

**Files:**
- Modify: `studio/src/web/router.ts`, `studio/test/router.test.ts`, `studio/src/web/Shell.tsx`, `studio/src/web/App.tsx`, `studio/src/server/app.ts` (`GET /api/library/file`), `studio/src/web/api.ts`
- Create: `studio/src/web/shows.ts`, `studio/test/shows.test.ts`, `studio/src/web/pages/ShowsPage.tsx`, `studio/src/web/pages/ShowPage.tsx`, `studio/src/web/pages/EpisodePage.tsx`, `studio/src/web/pages/AdPage.tsx`
- Modify (clickable rows): `channel/ShowsTab.tsx`, `channel/TodayTab.tsx`, `pages/AdsPage.tsx`, `pages/EpisodesPage.tsx` (Recent done rows with `count === 1` link to the episode)

**Interfaces:**
- **Routes:**
  - `{ page: "shows" }`
  - `{ page: "show"; channel: string; show: string }`
  - `{ page: "episode"; path: string }`
  - `{ page: "ad"; path: string }`
  - `topTab`: `shows`, `show` and `episode` give `"shows"`; `ad` gives `"ads"`.
  - `parentsOf`:
    - show → Shows;
    - episode → Shows › show name (the name comes from a lookup, falling back to the folder);
    - ad → Ads.
- **`GET /api/library/file?path=<rel>`:** `{ size: number; mtimeMs: number; source: string | null }`.
  - `source` comes from `sources.toml`, shown as `upload:<name>` or the original path.
  - The path checks are the same as play's.
- **In `shows.ts`:**
  ```ts
  export interface ShowEntry { ref: ShowRef; name: string; icon: string; seasons: number; episodes: number; airsOn: number[] }
  export interface ShowGroup { folder: string; label: string; shows: ShowEntry[] }
  export function showGroups(library: WebLibrary, config: Config): ShowGroup[];
  export function findEpisode(library: WebLibrary, path: string): { show: LibShow; episode: LibEpisode } | null;
  ```
  - Groups go in channel-number order using the channel name as the label, then folders with no channel under "Not on a channel".
  - Shows are sorted by name.
- **`EpisodePage({ path, onNavigate })`:**
  - `PageHead`: the code and title, with the show name as sub;
  - `MpegPlayer`;
  - facts: length, size, file, source, converted date;
  - "Airs on" chips (channel names linking to the channel's Schedule tab), or "Not airing".
- **`AdPage`:** the same shape, with "All channels" or the channel as sub.
- **`ShowPage`:** read-only for now. Seasons with clickable episode rows, and "Airs on" chips.
- **The Shows tab** lists `showGroups` as rows: icon, name, "N seasons · M episodes", and the "Airs on" chips.

- [ ] **Step 1: Write the failing tests.**
  - Router: `parses the new routes`, `round trips paths with spaces and &`, `new parents`, `new top tabs`.
  - `shows.test.ts`: `groups shows by the folder that holds them`, `orphan folders come last`, `airs on uses channelsAiring`, `finds an episode by path`.
  - `app.test.ts`: `file facts give size and source`, `file facts refuse paths outside the library`.
- [ ] **Step 2: Run them and see them fail.**
- [ ] **Step 3: Implement it.**
- [ ] **Step 4: Run them and see them pass.** Then run typecheck.
- [ ] **Step 5: Check in the browser.**
  - Episodes are clickable from a channel's Shows tab, Today and Recent, and ads from Ads.
  - The breadcrumbs read `‹ Shows › Gumball`.
  - The Shows tab shows Adventure Time under its channel.
- [ ] **Step 6: Commit.** `wip | studio episode and show pages`

## Stage 2 — Editing

### Task 5: Off-air storage, build exclusion and candidate validation

**Files:**
- Create: `studio/src/server/off.ts`, `studio/test/off.test.ts`
- Modify: `studio/src/server/library.ts` (`buildLibrary` takes `exclude`, returns `off`; export `seasonlessOrder`), `studio/src/server/sched.ts` (`validateCandidate` takes a library; `newErrors`), `studio/src/server/app.ts` (`rebuildNow` passes the off set, writes `.studio/off-library.toml`; `GET /api/library` adds `off`), `studio/src/web/tree.ts` (`WebLibrary.off?`)
- Test: `studio/test/library.test.ts`, `studio/test/sched.test.ts`, `studio/test/app.test.ts`

**Interfaces:**
- **In `off.ts`:**
  - `readOff(root): Promise<Set<string>>` and `writeOff(root, paths: Set<string>): Promise<void>`.
  - The file is `.studio/off.toml`, `version = 1`, `paths` sorted.
  - A missing file means an empty set. A damaged file throws, with the message naming the file.
- **`buildLibrary(root, sources, probeFn, exclude = new Set<string>())`:**
  - Excluded paths are left out of `library`.
  - `BuildResult.off: { ad: LibAd[]; show: LibShow[] }` holds them, with the same fields.
  - In shows without seasons, numbers are assigned over all files before excluding.
- **`export function seasonlessOrder(files: string[]): string[]`** is the exact file order the build numbers by.
- **`validateCandidate(root, cfg, bin, library?: Library)`** validates against `library` when given.
- **`export function newErrors(before: Issue[], after: Issue[]): Issue[]`** returns errors in `after` with no error of the same `code`, `channel` and `message` in `before`.
- **`GET /api/library`** returns `{ ...library, off }`. `off` is read from `.studio/off-library.toml` (written by every rebuild), or empty.

- [ ] **Step 1: Write the failing tests.**
  - `off.test.ts`: `reads and writes the off list`, `missing file is empty`, `damaged file is an error`.
  - `library.test.ts`:
    - `excluded episodes are left out and listed as off`;
    - `seasonless numbers don't shift when one is off`: E1, E2 and E3 with E2 off give the library E1 and E3, and off E2;
    - `excluded ads are listed as off`.
  - `sched.test.ts`: `only new errors count`, `validates against a given library`.
  - `app.test.ts`: `library lists off items after a rebuild`.
- [ ] **Step 2: Run them and see them fail.**
- [ ] **Step 3: Implement it.**
- [ ] **Step 4: Run them and see them pass.** Then run `npm test`.
- [ ] **Step 5: Commit.** `wip | studio off air storage`

### Task 6: Edit planning (pure)

**Files:**
- Create: `studio/src/server/libraryEdit.ts` (planning functions only, no fs), `studio/test/libraryEdit.test.ts`

**Interfaces:**
- **Consumes:** Task 1's references, `seasonlessOrder`, `episodeFileName` and `fatSafe` (`names.ts`), and `descriptionKey` rules (`SxxEyy` or the file stem).
- **Produces:**
  ```ts
  export type EditAction =
    | { kind: "episode"; path: string; title: string; season: number | null; episode: number | null }
    | { kind: "ad"; path: string; name: string; channel: string }
    | { kind: "move-season"; paths: string[]; season: number }
    | { kind: "off"; paths: string[]; off: boolean }
    | { kind: "delete"; paths: string[] }
    | { kind: "delete-show"; channel: string; show: string }
    | { kind: "airs-on"; channel: string; show: string; number: number; on: boolean; slot: number | null }
    | { kind: "append"; number: number; slot: number | null; items: Item[] };
  export interface EditPlan { moves: { from: string; to: string }[]; deletes: string[]; deleteDirs: string[]; off: Set<string>; exclude: Set<string>; config: Config; removed: { channel: number; label: string }[]; describeKeys: { showDir: string; from: string; to: string }[] }
  export class EditRefused extends Error { status: 400 | 409 }
  export function planEdit(action: EditAction, ctx: { config: Config; library: Library; off: Set<string>; busy: Set<string>; showFiles: (ref: ShowRef) => string[] }): EditPlan;
  ```
- **Rules:**
  - **episode:**
    - The new name is `episodeFileName({ season, episode, title })` in `Season N/` for seasons ≥ 1, or the fat-safe title in the show folder for shows without seasons.
    - A taken code in that show is `EditRefused(409, "S01E05 already exists: <title>")`.
    - Config `episode` items move with `retargetEpisodes`. For a show without seasons, the old and new orders come from `seasonlessOrder(showFiles)` before and after the rename.
    - Descriptions move from the old key to the new one.
  - **ad:** the new path is `<channel or "">/ads/<fat-safe name>.mpg`, and `renameAd` updates config.
  - **move-season:** each path gets `episode = last episode in the target season + k` in the given order, with the same title.
  - **off:**
    - Switching off adds to `off` and `exclude`, and removes `itemsNaming` direct items (`episode` items and `ad` items).
    - Switching on removes from `off` and `exclude` only.
  - **delete:** `deletes` plus `exclude`, and removes the direct items.
  - **delete-show:** every file of the show, `deleteDirs = [show dir]`, and removes every item naming the show.
  - **airs-on:**
    - `on` appends `{ type: "random", show, count: 1, from? }`, with `from` only when the show's folder isn't that channel's folder.
    - `off` removes every item naming the show on that channel.
    - It refuses (400) a timed channel without a slot.
  - **append:** `appendItems`.
  - **Any path in `busy`:** `EditRefused(409, "Still converting. Try again when it's done.")`.
  - **`removed`** labels use `describeItem`.

- [ ] **Step 1: Write the failing tests.** One per rule above, plus:
  - `seasonless rename keeps picks on the same videos`: files "A", "C" and "D", with a pick on episode 3 ("D"). Renaming "A" to "E" makes the order C, D, E, and the pick becomes episode 2.
  - `move to season renumbers after the last episode`;
  - `a converting file can't be planned`;
  - `airs-on off removes picks and rules on that channel only`.
- [ ] **Step 2: Run them and see them fail.**
- [ ] **Step 3: Implement it.**
- [ ] **Step 4: Run them and see them pass.**
- [ ] **Step 5: Commit.** `wip | studio edit planning`

### Task 7: The edit pipeline and endpoint

**Files:**
- Modify: `studio/src/server/libraryEdit.ts` (add `applyPlan`), `studio/src/server/app.ts`, `studio/src/web/api.ts`
- Test: `studio/test/libraryEdit.test.ts`, `studio/test/app.test.ts`

**Interfaces:**
- **`applyPlan(root: string, plan: EditPlan): Promise<() => Promise<void>>`:**
  - It does the moves (making folders with `mkdirContained`, renaming with containment checks), the `sources.toml` target rewrite, the `off.toml` write and the `show.toml` key moves.
  - It returns an undo that reverses those in reverse order.
  - It does not delete files; deletes run after validation passes.
- **`POST /api/library/edit`:**
  - Body `{ mtimeMs: number; dryRun?: boolean; action: EditAction }`.
  - Response `{ mtimeMs: number; removed: { channel: number; label: string }[]; issues: Issue[] }`.
- **Inside `withConfigLock`:**
  1. A config mtime mismatch gives 409 (conflict, as `PUT /api/config` does).
  2. `busy` comes from the queue's queued and running targets.
  3. `before = validateRoot(root)`.
  4. `plan = planEdit(...)`.
  5. For `dryRun`, build the candidate library with `buildLibrary(root, sources, probe, plan.exclude ∪ off)`. No files move, because dry runs are only used for off, delete, delete-show and airs-on. Validate and return without changing anything.
  6. Otherwise `undo = applyPlan`, rebuild the candidate library, and validate the candidate config. If `newErrors(before, after)` is not empty, `undo()` and answer 422 with those issues.
  7. Write the config, delete `plan.deletes` and `plan.deleteDirs`, rebuild for real (`rebuildNow`), and answer.
  8. If anything throws after step 6 began, `undo()` before rethrowing.

- [ ] **Step 1: Write the failing tests.**
  - `libraryEdit.test.ts` (temp dir):
    - `apply moves the file and its sources record`;
    - `undo puts files, sources, off and show.toml back`.
  - `app.test.ts` (fixture library):
    - `renames an episode and its picks follow`;
    - `a failed edit puts everything back`: a `buildLibrary` failure injected through `probeFn` throwing on the renamed path;
    - `an edit that adds an error is refused and changes nothing`: switching off the only episode a `random` rule can pick gives 422 with `empty_pool`, and the files and `off.toml` are unchanged;
    - `a converting file can't be edited`: a blocked `convertFn` keeps the job running, and the answer is 409 with the exact copy;
    - `dry run changes nothing and lists removed items`;
    - `delete removes the file and its direct items`;
    - `conflict on stale mtime`.
- [ ] **Step 2: Run them and see them fail.**
- [ ] **Step 3: Implement it.**
- [ ] **Step 4: Run them and see them pass.** Then run `npm test`.
- [ ] **Step 5: Commit.** `wip | studio library edit pipeline`

### Task 8: Editing on the episode and ad pages

**Files:**
- Modify: `studio/src/web/pages/EpisodePage.tsx`, `studio/src/web/pages/AdPage.tsx`, `studio/src/web/channel/ShowsTab.tsx`, `studio/src/web/pages/AdsPage.tsx`, `studio/src/web/ui/ui.css`
- Create: `studio/src/web/useLibraryEdit.ts`

**Interfaces:**
- **`useLibraryEdit()`:** `{ run(action, opts?: { confirm?: (dry: EditResult) => Promise<boolean> }): Promise<EditResult | null>; busy: boolean; failure: Failure | null }`.
  - It loads the config mtime, does an optional dry run, asks through `confirm`, then runs the edit.
  - It shows a banner like "Saved." or "Switched off." with the Wii suffix in server mode.
  - 422 issues show through `FailureBanner`.
- **Episode page:**
  - Title, season and episode fields. Season and episode are hidden for shows without seasons.
  - The description edits `show.toml` through the existing `putShowMeta`.
  - Save.
  - An On air / Off air switch (`Checkbox` styled as a switch). Switching off with direct items shows a `ConfirmDialog` listing `removed`.
  - Delete: a `ConfirmDialog` naming the file and `removed`, plus "Delete can't be undone."
  - After a rename, the page navigates to the new path with `history.replaceState`, through `onNavigate`.
- **Ad page:** name, where it plays ("All channels" or a channel), the On/Off switch, and Delete.
- **Off-air rows:** greyed with an "Off air" chip in the channel Shows tab, the show page and the Ads page. They come from `library.off`.

- [ ] **Step 1: Implement it.** The logic is in Tasks 6 and 7.
- [ ] **Step 2: Typecheck and test.**
- [ ] **Step 3: Check in the browser** (scratch library):
  - rename an episode and its pick on a channel still plays it (Today tab);
  - switch an episode off and on;
  - try switching off the only episode of a rule (refused with the reason);
  - delete an episode;
  - rename and move an ad.
- [ ] **Step 4: Commit.** `wip | studio episode and ad editing`

## Stage 3 — Shows tab management

### Task 9: Show page management

**Files:**
- Modify: `studio/src/web/pages/ShowPage.tsx`, `studio/src/web/shows.ts`, `studio/test/shows.test.ts`, `studio/src/web/pages/AddPage.tsx`, `studio/src/web/adding.ts`, `studio/test/adding.test.ts`

**Interfaces:**
- **Produces:**
  - `export function seasonTargets(show: LibShow): number[]`: existing seasons plus the next new one. A show without seasons offers seasons from 1.
  - `export function pickedFrom(config: Config, ref: ShowRef): number[]`: the channels with `episode` items naming the show.
- **Show page:**
  - The header has the icon and name, "Stored in <folder>", Add episodes (`#/add?channel=<home number>` when a channel uses that folder), Edit info (the existing `ShowInfoDialog`) and `⋯` → Delete show.
  - Season sections with "tick all".
  - Each episode row has a tick box, code, title, length, the On/Off chip and the conversion status chip, and is clickable.
  - With ticked rows, a bar offers:
    - **Move to season…:** a `Select` of `seasonTargets` plus "New season N", then `move-season`;
    - **Switch off** and **Switch on**;
    - **Delete**, with a confirm listing the files and removed items.
- **Add page warning:** when a destination show has no seasons and `pickedFrom` isn't empty, a warn banner says: "Adding to <show> can shift the episodes picked on <channels>."

- [ ] **Step 1: Write the failing tests.** `season targets`, `picked from lists channels with picks`.
- [ ] **Step 2: Run them and see them fail.**
- [ ] **Step 3: Implement it.**
- [ ] **Step 4: Run them and see them pass.** Then run typecheck.
- [ ] **Step 5: Check in the browser:** move two episodes to a new season, switch three off at once, delete a show (scratch library).
- [ ] **Step 6: Commit.** `wip | studio show management`

## Stage 4 — Sharing

### Task 10: Airs on tick boxes and adding picks to a channel

**Files:**
- Modify: `studio/src/web/pages/ShowPage.tsx`, `studio/src/web/ui/ui.css`
- Create: `studio/src/web/library/SlotPicker.tsx`

**Interfaces:**
- **Consumes:** the `airs-on` and `append` actions (Task 6), `channelsAiring`, and `useLibraryEdit`.
- **Produces:**
  - `SlotPicker({ channel: Channel; onPick: (slot: number) => void; onCancel })`: a dialog listing the slot times, with the last one preselected.
- **Show page:** an "Airs on" row with one `Checkbox` per channel, in number order.
  - **Ticking:**
    - a looping channel runs `airs-on` straight away;
    - a timed channel opens `SlotPicker` first.
  - **Unticking:** a dry run, then a `ConfirmDialog` listing `removed`, then the edit. If the answer is 422 (for example an empty loop), the reason shows.
- **Bulk "Add to a channel…":** pick a channel, and a slot for timed channels. Then `append` with `episode` items for the ticked rows, in shown order, with `from` when needed.

- [ ] **Step 1: Implement it.**
- [ ] **Step 2: Typecheck and test.**
- [ ] **Step 3: Check in the browser:**
  - tick a show onto a looping and a timed channel, and check the Schedule tab;
  - untick it;
  - add three picked episodes from two shows to one channel, and check the Today tab plays them in order.
- [ ] **Step 4: Commit.** `wip | studio airs on and picks`

### Task 11: Docs and full pass

**Files:**
- Modify: `docs/USER-GUIDE.md` (new subsections in section 6 for the Shows tab, episode pages, Airs on and picks; the off-air note in troubleshooting), `docs/SERVER.md` (the Wii catch-up note), `CHANGELOG.md` (Unreleased line), `docs/formats.md` (a short `.studio/off.toml` and `.studio/off-library.toml` section, marked Studio-only)

- [ ] **Step 1: Write the docs.**
- [ ] **Step 2: Do a full browser pass** on fresh scratch copies, in server mode (4790) and drive mode (4791):
  - every new page;
  - the player;
  - one edit of each kind;
  - narrow width 1024 px.
- [ ] **Step 3:** run `npm test` and `npm run typecheck`.
- [ ] **Step 4: Commit.** `wip | studio library management docs`
