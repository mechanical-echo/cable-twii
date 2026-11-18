# cable-twii Studio Server Mode Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Studio can run as a home server. It holds a library folder, people edit it from a browser after signing in with a password, they add videos by uploading or from an imports folder, and the Wii reads the library over a read-only `/wii/v1/` interface.

**Architecture:**
- `npm run serve` starts the same Fastify app with `AppOptions.server` set.
- In server mode the "drive" is a fixed library folder: drive inspection, native pickers and eject are off, any Host is accepted, and a shared-password session guards `/api/*` and `/media/*`.
- New focused modules: `serverSettings.ts`, `session.ts`, `wiiApi.ts`, `uploads.ts` and `imports.ts`. `app.ts` wires them in.
- Drive mode keeps today's behaviour.

**Tech Stack:** Node 22, TypeScript, Fastify 5, @fastify/multipart, smol-toml, `node:crypto`, `node:zlib`, React 18, vitest, Docker (`node:22` image) for the Linux check.

**Spec:** `docs/superpowers/specs/2026-11-02-server-source-design.md` (sections 5, 7, 8, 9)

## Global Constraints

- Node ≥ 22. No new runtime dependencies: sessions use `node:crypto` and gzip uses `node:zlib`.
- Server settings file: TOML at `CTW_SERVER_SETTINGS`, else `~/.cable-twii-server.toml`. Keys:
  - `library`: absolute path, required.
  - `password`: required, at least 8 characters.
  - `imports`: absolute path, default `<library>/imports`.
  - `port`: default 4747.
  - `name`: default `os.hostname()`.
- The library folder holds `cable-twii/` exactly like a drive. Uploads go to `<library>/.uploads/`, outside the root, so the Wii never sees them.
- Server mode listens on `0.0.0.0`. Drive mode keeps `127.0.0.1`, no password, and every existing test passes unchanged.
- Wii interface:
  - Paths: `GET /wii/v1/info` and `GET /wii/v1/files/<path>`.
  - Read-only and no password.
  - Every response has a `Content-Length`, never chunked encoding.
  - `info` is TOML: `version = 1`, `name`, `revision`.
- Plain HTTP, home network only.
- Copy, exactly:
  - Wrong password: `Wrong password.`
  - No session: `Sign in first.`
  - Source outside allowed folders: `Videos must come from the imports folder or an upload.`
  - Duplicate upload: `already on the server`
  - show.toml conflict: `show.toml was changed somewhere else. Reload it and try again.`
- Match `app.ts` idioms: `HttpError`, `bad()`, `objectBody`, `folderName`, `inside`, `contained`, `atomicWriteContained`. No trailing whitespace. `npm run typecheck` and `npm test` stay clean.
- Commits: `wip | …` for features with their tests, `test | …` for tests alone, `fix | …`, each ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

- **Path tricks on `/wii/v1/files`:** encoded `%2e%2e%2f`, a backslash (`..\config.toml`, which matters on the Windows server) and a symlink out of the root must all give 404 (Task 3 tests `wii files refuse traversal`, `wii files refuse backslash`, `wii files refuse symlink out`).
- **Library names with spaces and accents:** requested percent-encoded, they must be served byte-exact. Use the `root_unicode` fixture (Task 3 `wii files serve unicode names`).
- **Ranges at the edges:** the last byte (`bytes=999-999`) is 206; past the end is 416 with `Content-Range: bytes */<size>` (Task 3 `parseRange` cases).
- **An upload over the limit or cut off:** no file is left in `.uploads/` and nothing is queued (Task 5 `upload over the limit leaves nothing`).
- **Two editors saving the same show:** the second gets 409 and the first edit survives (Task 4 `show-meta conflict keeps the first save`).

---

### Task 1: Server settings and server-mode library

**Files:**
- Create: `studio/src/server/serverSettings.ts`, `studio/src/server/serve.ts`, `studio/test/helpers.ts`, `studio/test/serverSettings.test.ts`, `studio/test/server.test.ts`
- Modify: `studio/src/server/app.ts`, `studio/package.json`, `studio/test/app.test.ts` (import the moved fakes)

**Interfaces:**
- Produces in `serverSettings.ts`:
  - `export interface ServerSettings { library: string; password: string; imports: string; port: number; name: string }`
  - `export function parseServerSettings(text: string, file: string): ServerSettings`: throws `Error` whose message names the file and the key.
  - `export function serverSettingsPath(env: NodeJS.ProcessEnv = process.env): string`
  - `export async function readServerSettings(file: string): Promise<ServerSettings>`
  - `export function uploadsDir(library: string): string`, which returns `path.join(library, ".uploads")`.
  - `export async function initServerLibrary(s: ServerSettings): Promise<void>`: creates `cable-twii/`, `cable-twii/ads/`, `cable-twii/.studio/`, the imports folder and `.uploads/` if they are missing.
- Produces in `app.ts`:
  - `export interface ServerModeOptions { library: string; password: string; imports: string; name: string }`
  - `AppOptions.server?: ServerModeOptions`
  - `GET /api/mode` returns `{ mode: "drive" | "server"; name: string | null }`.
- Produces in `test/helpers.ts`: `fakeProbe`, `fakeConvert`, `multipart`, `waitFor`, moved from `app.test.ts`.

- [ ] **Step 1: Write the failing tests**
  - `serverSettings.test.ts`:
    - Minimal settings `library = "/srv/tv"`, `password = "correct horse"` parse to `port === 4747` and `imports === "/srv/tv/imports"`.
    - A relative `library`, a 7-character `password` and `port = 0` each throw, with the key in the message.
  - `server.test.ts` (an app built with `server: { library: dir, password: "correct horse", imports, name: "Home PC" }` after `initServerLibrary`):
    - `GET /api/drive` gives `usable === true`, `hasRoot === true` and `rootPath === path.join(dir, "cable-twii")`.
    - `POST /api/drive`, `/api/drive/pick`, `/api/drive/init`, `/api/drive/done` and `/api/import/pick-source` each give 404.
    - `GET /api/mode` with `host: "192.168.1.20:4747"` gives 200 and `{ mode: "server", name: "Home PC" }`.
    - `PUT /api/config` with `host: "192.168.1.20:4747"` and `origin: "http://evil.example"` gives 403.
    - The same request with `origin: "http://192.168.1.20:4747"` passes the guard (not 403).
- [ ] **Step 2: Run them to see them fail**
  Run: `cd studio && npx vitest run test/serverSettings.test.ts test/server.test.ts`
  Expected: FAIL, because `serverSettings` and `server` don't exist yet.
- [ ] **Step 3: Implement**
  - In server mode, `currentDrive()` returns a fixed `DriveInfo`:
    - `path`: the library
    - `rootPath`: `<library>/cable-twii`
    - `isVolumeRoot: false`, `fsType: null`, `hasRoot: true`, `hasWbfs: false`, `usable: true`
    - `freeBytes` from `statfs`
  - Register the five drive-only routes only in drive mode.
  - In server mode the `onRequest` guard skips the Host check, still requires `x-cable-twii: 1` on non-GET/HEAD requests, and requires a present `Origin` to match `request.headers.host`.
  - `serve.ts`:
    1. `readServerSettings(serverSettingsPath())` → `initServerLibrary` → `ensureSchedBin`.
    2. `buildApp({ schedBin, logger: true, port, server })`, then `listen({ host: "0.0.0.0", port })`.
    3. Print `cable-twii server "<name>" on port <port>, library <library>`. Never open a browser.
  - `package.json`: add `"preserve": "node scripts/check-node.mjs"` and `"serve": "node scripts/build-web.mjs && tsx src/server/serve.ts"`.
- [ ] **Step 4: Run all Studio tests**
  Run: `cd studio && npm test && npm run typecheck`
  Expected: PASS, with the old `app.test.ts` tests unchanged.
- [ ] **Step 5: Commit** `wip | studio server mode library and settings`

---

### Task 2: Password sign-in

**Files:**
- Create: `studio/src/server/session.ts`, `studio/test/session.test.ts`
- Modify: `studio/src/server/app.ts`, `studio/test/server.test.ts`

**Interfaces:**
- Consumes: Task 1 `ServerModeOptions`.
- Produces:
  - `export const SESSION_COOKIE = "ctw_session"`
  - `export class Sessions { constructor(password: string, ttlMs?: number, now?: () => number); signIn(password: string): string | null; isValid(token: string | undefined): boolean; signOut(token: string | undefined): void }`
    - Default TTL is 30 days.
    - Tokens are `randomBytes(32).toString("base64url")`, held in memory, so a restart signs everyone out.
    - The password is compared with `timingSafeEqual` on SHA-256 digests.
  - `export function sessionToken(cookieHeader: string | undefined): string | undefined`
  - Routes, server mode only:
    - `POST /api/login {password}` → `{ ok: true }` and `Set-Cookie: ctw_session=<token>; Path=/; HttpOnly; SameSite=Strict; Max-Age=2592000`.
    - `POST /api/logout` clears the cookie.
  - `GET /api/mode` adds `signedIn: boolean`.
  - Guard: in server mode, `/api/*` and `/media/*` need a valid session, except `/api/mode` and `/api/login`. Static web files and `/wii/v1/*` stay open.

- [ ] **Step 1: Write the failing tests**
  - `session.test.ts`:
    - A wrong password gives `null`.
    - The right password gives a token where `isValid` is `true`.
    - With a fake `now` moved past the TTL, `isValid` is `false`.
    - `signOut` invalidates the token.
    - `sessionToken("a=1; ctw_session=abc; b=2") === "abc"`.
  - `server.test.ts`:
    - `GET /api/config` without a cookie gives 401 and `{ error: "Sign in first." }`.
    - A wrong login gives 401 `Wrong password.`
    - Logging in, then `GET /api/config` with the cookie, gives 200.
    - After logout, the same cookie gives 401.
    - `GET /api/mode` without a cookie gives 200 and `signedIn: false`.
- [ ] **Step 2: Run and see them fail** `cd studio && npx vitest run test/session.test.ts test/server.test.ts`. Expected: FAIL.
- [ ] **Step 3: Implement** `session.ts` and the routes and guard in `app.ts`. Update the Task 1 server tests so they sign in first.
- [ ] **Step 4: Run** `cd studio && npm test && npm run typecheck`. Expected: PASS.
- [ ] **Step 5: Commit** `wip | studio server sign-in`

---

### Task 3: Wii interface

**Files:**
- Create: `studio/src/server/wiiApi.ts`, `studio/test/wiiApi.test.ts`
- Modify: `studio/src/server/app.ts`

**Interfaces:**
- Consumes: Task 1 server-mode root.
- Produces:
  - `export function parseRange(header: string | undefined, size: number): { start: number; end: number } | "unsatisfiable" | null`: single `bytes=a-b`, `a-` and `-n` only. Anything else gives `null`, which means the whole file is sent.
  - `export function wiiFilePath(rel: string): string | null`: returns `null` for:
    - an empty path, or one with an empty segment
    - a `..` segment
    - a segment starting with `.` (this covers `.studio/`, `._*` and `.DS_Store`)
    - a backslash, or a NUL byte
  - `export async function libraryRevision(root: string): Promise<string>`: `"<config mtime>-<library mtime>"`, truncated integers, 0 when a file is missing.
  - `export function infoToml(name: string, revision: string): string`, written with smol-toml `stringify`.
  - `export function registerWiiApi(app: FastifyInstance, root: string, name: string): void`, registered in server mode only.
    - `GET /wii/v1/info`: `content-type: application/toml`, `cache-control: no-cache`.
    - `GET /wii/v1/files/*`:
      - Decode the path, check it with `wiiFilePath`, check realpath containment, and require a regular file. Otherwise 404 `Not found`.
      - A `.toml` file with `accept-encoding` containing `gzip` is sent `gzipSync` with `content-encoding: gzip`.
      - Otherwise `parseRange` decides:
        - a range → 206, `content-range: bytes s-e/size`
        - `"unsatisfiable"` → 416, `content-range: bytes */size`
        - `null` → 200
      - Always set `accept-ranges: bytes` and `content-length`.
      - Types: `.mpg` is `video/mpeg`, `.png` is `image/png`, `.toml` is `application/toml`.

- [ ] **Step 1: Write the failing tests**
  - `parseRange`, with size 1000:
    - `"bytes=0-99"` → `{0,99}`
    - `"bytes=900-"` → `{900,999}`
    - `"bytes=-100"` → `{900,999}`
    - `"bytes=999-999"` → `{999,999}`
    - `"bytes=0-5000"` → `{0,999}`
    - `"bytes=1000-"` → `"unsatisfiable"`
    - `"bytes=0-1,5-6"` → `null`
    - `undefined` → `null`
  - `wiiFilePath`:
    - `"config.toml"` → `"config.toml"`
    - `null` for each of: `".studio/queue.toml"`, `"a/../b"`, `"Cartoon Network/._x.mpg"`, `"a//b"`, `"..\\config.toml"`
  - Routes, with `root_basic` copied into the library and no cookie:
    - `wii info is open and versioned`: `parse(res.body).version === 1` and `name === "Home PC"`.
    - `wii revision changes after a save`: after a `utimes` on `config.toml` the revision differs.
    - `wii files serve a range`: `range: bytes=0-9` on an episode gives 206, `content-range: bytes 0-9/<size>`, the first 10 bytes of the file, and `content-length: 10`.
    - `wii files gzip toml`: with `accept-encoding: gzip`, `gunzipSync(res.rawPayload)` equals `library.toml`.
    - `wii files refuse traversal` (`/wii/v1/files/%2e%2e%2fsettings`), `wii files refuse backslash`, `wii files refuse dot folders` (`.studio/sources.toml`): all 404.
    - `wii files refuse symlink out`: a symlink in the root pointing at a file outside it gives 404.
    - `wii files serve unicode names` (`root_unicode` fixture): the percent-encoded `Pokémon ✨/Season 1/S01E02 - Café über alles.mpg` gives 200 and the exact bytes.
- [ ] **Step 2: Run and see them fail** `cd studio && npx vitest run test/wiiApi.test.ts`. Expected: FAIL.
- [ ] **Step 3: Implement** `wiiApi.ts` and register it in `buildApp` when `opts.server` is set.
- [ ] **Step 4: Run** `cd studio && npm test && npm run typecheck`. Expected: PASS.
- [ ] **Step 5: Commit** `wip | studio wii interface`

---

### Task 4: show.toml conflict check

**Files:**
- Modify: `studio/src/server/showMeta.ts`, `studio/src/server/app.ts`, `studio/src/web/api.ts`, `studio/src/web/library/ShowMetaDialog.tsx`, `studio/test/app.test.ts`

**Interfaces:**
- Produces:
  - `export async function showMetaMtime(showDir: string): Promise<number>`: the truncated mtime, 0 when the file is missing.
  - `export class ShowMetaConflictError extends Error`, which maps to 409 in the error handler.
  - `GET /api/show-meta?channelFolder&showFolder` returns `{ meta: ShowMeta; mtimeMs: number }`.
  - `PUT /api/show-meta` requires `mtimeMs`. Both writes of `show.toml` (show-meta and import/show) run inside `withConfigLock`.
  - Web: `getShowMeta(channelFolder, showFolder)` and `putShowMeta(req & { mtimeMs })`. `ShowMetaDialog` loads with `getShowMeta` when it opens.
  - This applies in both modes. Drive mode only notices it when two tabs edit the same show.

- [ ] **Step 1: Write the failing tests** in `app.test.ts`:
  - `GET /api/show-meta returns meta and mtime`.
  - `show-meta conflict keeps the first save`:
    1. Two GETs.
    2. PUT A with the first mtime → 200.
    3. PUT B with the same mtime → 409 with the exact copy.
    4. `show.toml` still holds A's name.
  - Update the existing show-meta tests to send `mtimeMs`.
- [ ] **Step 2: Run and see them fail** `cd studio && npx vitest run test/app.test.ts -t show-meta`. Expected: FAIL.
- [ ] **Step 3: Implement** the server and the web changes.
- [ ] **Step 4: Run** `cd studio && npm test && npm run typecheck`. Expected: PASS.
- [ ] **Step 5: Commit** `wip | show.toml conflict check`

---

### Task 5: Browser uploads

**Files:**
- Create: `studio/src/server/uploads.ts`, `studio/test/uploads.test.ts`
- Modify: `studio/src/server/app.ts`, `studio/src/server/drive.ts` (`scanSource` probe parameter)

**Interfaces:**
- Consumes: Task 1 `uploadsDir`, Task 2 sessions.
- Produces:
  - `scanSource(dir: string, probeFn: typeof probe = probe): Promise<ScanResult[]>`. The upload and import routes pass `opts.probeFn`.
  - `export const UPLOAD_LIMIT = 20 * 1024 ** 3`. `AppOptions.uploadLimit?: number` overrides it in tests.
  - `export function uploadSource(name: string): string`, which returns `upload:${name}`.
  - `export function isUploadPath(uploads: string, file: string): boolean`
  - `export function uploadName(name: string): string | null`: `null` when the name is empty, starts with `.`, or contains `/`, `\` or NUL.
  - `export async function saveUpload(uploads: string, batch: string, name: string, stream: Readable): Promise<string>`: writes `<uploads>/<batch>/<name>.part`, then renames it to `<name>`.
  - `POST /api/upload`, server mode, multipart with many files. `batch` is `randomUUID()`. It returns `{ dir, files: ScanResult[], rejected: { name: string; reason: string }[] }`, the same shape as `pick-source` plus `rejected`.
    - Not `isVideoFile` → rejected `not a video`.
    - Size and `uploadSource(name)` match a `SourceRecord` whose target exists → the file is deleted and rejected `already on the server`.
    - `part.file.truncated` → the batch folder is removed and the response is 413.
  - After conversion (queue `converted`, server mode), when `isUploadPath(rec.source)`:
    1. `upsertSource(root, { ...rec, source: uploadSource(basename(rec.source)) })`.
    2. `rm(rec.source)`.
    3. Remove the batch folder if it is empty.

- [ ] **Step 1: Write the failing tests** (`uploads.test.ts`, server app, signed in):
  - `upload stores the file and returns a scan`: `ep.mp4` → `files.length === 1`, and `files[0].source` sits inside `.uploads/`.
  - `upload rejects non-video names`: `notes.txt` is in `rejected` with `not a video`.
  - `converted upload is deleted and recorded`:
    1. Upload.
    2. `POST /api/import/show` with that source.
    3. `waitFor` the job to be done.
    4. The upload file is gone, and `readSources(root)` has `source === "upload:ep.mp4"`.
  - `same upload again is rejected`: the second upload of the same name and size is rejected with `already on the server`.
  - `upload over the limit leaves nothing`: with `uploadLimit: 10` and a 100-byte file → 413, `.uploads/` is empty and `GET /api/jobs` is empty.
  - `uploadName("../x.mp4") === null`.
- [ ] **Step 2: Run and see them fail** `cd studio && npx vitest run test/uploads.test.ts`. Expected: FAIL.
- [ ] **Step 3: Implement**:
  - Stream each part with `request.parts({ limits: { fileSize: uploadLimit, files: 50 } })` and `pipeline` into `saveUpload`.
  - Never use `toBuffer`.
  - The route-level limit overrides the global 10 MiB icon limit.
- [ ] **Step 4: Run** `cd studio && npm test && npm run typecheck`. Expected: PASS.
- [ ] **Step 5: Commit** `wip | studio server uploads`

---

### Task 6: Imports folder

**Files:**
- Create: `studio/src/server/imports.ts`, `studio/test/imports.test.ts`
- Modify: `studio/src/server/app.ts`

**Interfaces:**
- Consumes: Task 3 `wiiFilePath` rules (the same path rules; `""` means the imports root). Task 5 `isUploadPath` and `scanSource(dir, probeFn)`.
- Produces:
  - `export interface ImportEntry { name: string; kind: "folder" | "video" }`
  - `export async function listImports(imports: string, rel: string): Promise<{ dir: string; entries: ImportEntry[] }>`:
    - Folders come first, then videos, each sorted by name.
    - Dot-names, symlinks and non-videos are left out.
    - Throws `ContainmentError` outside the folder.
  - `GET /api/imports?dir=<rel>` returns the listing.
  - `POST /api/import/scan {dir: rel}` returns `{ dir: absolute, files: ScanResult[] }`.
  - In server mode, `POST /api/import/show` and `/api/import/ads` reject any `source` that is not inside the imports folder or `.uploads/`: 400 `Videos must come from the imports folder or an upload.`
  - Imports files are never moved or deleted.

- [ ] **Step 1: Write the failing tests**:
  - `imports listing hides dot files and non-videos`: `a/`, `b.mp4`, `.hidden.mp4` and `c.txt` list as `[a folder, b.mp4 video]`.
  - `imports listing refuses ..`: `dir=..` gives 400.
  - `import scan returns files`.
  - `import refuses a source outside`: `source: "/etc/hosts"` gives 400 with the exact copy.
  - `imports file is kept after conversion`: import from the imports folder, wait until the job is done, and the original still exists.
- [ ] **Step 2: Run and see them fail** `cd studio && npx vitest run test/imports.test.ts`. Expected: FAIL.
- [ ] **Step 3: Implement**.
- [ ] **Step 4: Run** `cd studio && npm test && npm run typecheck`. Expected: PASS.
- [ ] **Step 5: Commit** `wip | studio server imports folder`

---

### Task 7: Web UI for server mode

**Files:**
- Create: `studio/src/web/LoginPage.tsx`, `studio/src/web/library/AddVideosDialog.tsx`, `studio/src/web/uploading.ts`
- Modify: `studio/src/web/api.ts`, `studio/src/web/App.tsx`, `studio/src/web/pages/DrivePage.tsx`, `studio/src/web/pages/LibraryPage.tsx`, `studio/test/web.test.ts`

**Interfaces:**
- Consumes: Tasks 1, 2, 5 and 6 routes.
- Produces:
  - `api.ts`: `getMode()`, `login(password)`, `logout()`, `getImports(dir)` and `scanImports(dir)`.
  - `api.ts`: `uploadVideos(files: File[], onProgress: (ratio: number) => void): Promise<UploadResult>`, sent with `XMLHttpRequest` for upload progress.
  - `uploading.ts`: `export function uploadRatio(loaded: number, total: number): number` (0 to 1, and 0 when `total` is 0) and `export function crumbs(rel: string): { name: string; rel: string }[]`.
  - `App.tsx`:
    - In server mode, show `LoginPage` until `signedIn`.
    - Any `ApiError` with status 401 returns to `LoginPage`.
    - The drive gate is always open in server mode.
  - `DrivePage.tsx`: in server mode it shows the server name, the library path, the free space and a Sign out button, with no drive buttons.
  - `LibraryPage.tsx`: in server mode, Import show and Import ads open `AddVideosDialog` instead of the native picker. It has two tabs:
    - **Upload:** a file input (`multiple`, `accept="video/*"`) with a progress bar.
    - **Imports folder:** a folder browser with breadcrumbs, and "Use this folder".

    Both end in `{dir, files}`, which goes on to `ImportShowDialog` / `ImportAdsDialog` as today. Rejected names are listed.

- [ ] **Step 1: Write the failing tests** in `web.test.ts`:
  - `uploadRatio(50, 200) === 0.25` and `uploadRatio(0, 0) === 0`.
  - `crumbs("")` gives `[{ name: "imports", rel: "" }]`.
  - `crumbs("a/b")` gives `[{imports,""}, {a,"a"}, {b,"a/b"}]`.
- [ ] **Step 2: Run and see them fail** `cd studio && npx vitest run test/web.test.ts`. Expected: FAIL.
- [ ] **Step 3: Implement** the pages and dialogs. Copy:
  - Login heading: `Sign in to <name>`
  - Login button: `Sign in`
  - Tabs: `Upload` / `Imports folder`
  - Folder button: `Use this folder`
- [ ] **Step 4: Run** `cd studio && npm test && npm run typecheck`. Expected: PASS.
- [ ] **Step 5: Manual check on the Mac**:
  1. Write a settings file with the library in the scratchpad.
  2. Run `CTW_SERVER_SETTINGS=<file> npm run serve` in `studio/`.
  3. Open `http://<mac-ip>:4747/` from the Mac and from a phone on the same network.
  4. Sign in, upload a short video, import it as a show, see it convert, and check that the upload is gone afterwards.
  5. Run `curl -s http://<mac-ip>:4747/wii/v1/info` and confirm it shows the revision.
- [ ] **Step 6: Commit** `wip | studio server web ui`

---

### Task 8: Running it on Windows and Linux, and docs

**Files:**
- Create: `studio/scripts/serve-windows.cmd`, `docs/SERVER.md`
- Modify: `docs/formats.md`, `README.md`, `docs/STATUS.md`

**Interfaces:**
- Consumes: everything above.
- Produces:
  - `serve-windows.cmd`: `cd /d "%~dp0.."`, then `npm run serve`.
  - `docs/SERVER.md`, the install guide:
    - **Windows 11 PC:**
      - Install Node 22, Git and ffmpeg.
      - Write the settings file.
      - Firewall, from an admin PowerShell: `New-NetFirewallRule -DisplayName "cable-twii server" -Direction Inbound -Protocol TCP -LocalPort 4747 -Profile Private -Action Allow`. The network must be set to Private.
      - Start at sign-in: `schtasks /Create /TN "cable-twii server" /SC ONLOGON /TR "\"<repo>\studio\scripts\serve-windows.cmd\""`.
      - Sleep set to Never while plugged in.
      - A fixed address through a DHCP reservation in the router.
      - Check with `curl http://<pc-ip>:4747/wii/v1/info` from another machine.
    - **Raspberry Pi:** packages (`nodejs` 22, `ffmpeg`, `build-essential`), the settings file, and a systemd unit (`ExecStart=/usr/bin/npm run serve`, `WorkingDirectory=<repo>/studio`, `Restart=on-failure`, `User=<user>`), plus `systemctl enable --now cable-twii`.
    - **Mac, for testing:** `CTW_SERVER_SETTINGS=<file> npm run serve`.
  - `formats.md` gains three sections: "Server settings file", "Wii interface `/wii/v1/`" (both routes, the headers, the path rules and the `info` keys) and "Uploads".

- [ ] **Step 1: Linux check**
  Run: `docker run --rm -v "$PWD":/src:ro node:22 bash -c "cp -r /src /work && cd /work && rm -rf studio/node_modules engine/build && apt-get update -qq && apt-get install -y -qq ffmpeg >/dev/null && make -C engine test cli && cd studio && npm ci && npm test"`
  Expected: the engine tests and all Studio tests pass on Linux. Fix anything that fails (for example compiler warnings under `-Werror`) with `fix | …` commits.
- [ ] **Step 2: Write** `serve-windows.cmd`, `SERVER.md`, the `formats.md` sections, a README link and a STATUS row.
- [ ] **Step 3: Owner check at home**: follow `SERVER.md` on the Windows 11 PC, then run `curl http://<pc-ip>:4747/wii/v1/info` from the Mac and sign in from a phone.
- [ ] **Step 4: Commit** `wip | studio server install guide and formats`

---

### Task 9: Server conversion profile (decision 0004)

**Files:**
- Modify: `studio/src/server/videoSettings.ts`, `studio/src/server/ffmpeg.ts`, `studio/src/server/queue.ts`, `studio/src/server/app.ts`, `studio/test/ffmpeg.test.ts`, `studio/test/queue.test.ts`, `studio/test/server.test.ts`, `docs/formats.md`, `docs/SERVER.md`

**Interfaces:**
- Produces:
  - `videoSettings.ts`: `export interface VideoProfile { videoBitrate: string; videoMaxrate: string; videoBufsize: string; audioBitrate: string }`, `export const DRIVE_PROFILE: VideoProfile` (today's `1500k` / `2000k` / `1000k` / `192k`) and `export const SERVER_PROFILE: VideoProfile` (`900k` / `1100k` / `600k` / `128k`). Frame sizes, frame rate, GOP, codecs and loudness stay shared.
  - `convert(src, dst, aspect, onProgress, signal?, guard?, profile: VideoProfile = DRIVE_PROFILE)`: the ffmpeg arguments take the bitrates from `profile`.
  - `QueueOptions.profile?: VideoProfile`; the queue passes it to every conversion. `buildApp` in server mode creates queues with `SERVER_PROFILE`; drive mode keeps `DRIVE_PROFILE`.

- [ ] **Step 1: Write the failing tests**:
  - `ffmpeg.test.ts`: the argument list for `SERVER_PROFILE` contains `-b:v 900k`, `-maxrate 1100k`, `-bufsize 600k`, `-b:a 128k`; the default call still contains `1500k`, `2000k`, `1000k`, `192k`.
  - `queue.test.ts`: a queue built with `profile: SERVER_PROFILE` passes it to its `convertFn`.
  - `server.test.ts`: an import in server mode reaches `convertFn` with `SERVER_PROFILE`; drive mode (`app.test.ts` existing import test) still with `DRIVE_PROFILE`.
- [ ] **Step 2: Run and see them fail.**
- [ ] **Step 3: Implement.**
- [ ] **Step 4: Run** `cd studio && npm test && npm run typecheck`. Expected: PASS.
- [ ] **Step 5: Docs:** `formats.md` Uploads/server section states the server profile; `SERVER.md` says server videos are converted smaller for the Wii's Wi-Fi.
- [ ] **Step 6: Real check:** convert one file on the Mac server and confirm with `ffprobe` that the overall bitrate is about 1 Mbit/s.
- [ ] **Step 7: Commit** `wip | server conversion profile`

