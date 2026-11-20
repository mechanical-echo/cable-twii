# cable-twii — Studio redesign (design)

Date: 2026-11-20
Builds on: [Studio](2026-10-20-cable-twii-design.md) as built, [server source](2026-11-02-server-source-design.md), [Wii redesign](2026-10-22-wii-redesign-design.md) (for the look)
Mockups: [Studio redesign](mockups/studio-redesign.html), with the look (A · Wii Menu), Add episodes and the Channels pages approved by the owner on 2026-11-20
Reviewed by a builder check on 2026-11-20; its six questions were settled by the designer (marked **(my call)**).

Decisions marked **(my call)** were made by the designer, not the owner, and may be revisited. Everything else was stated or confirmed by the owner.

## Intent

Studio works but is ugly, and its layout follows the data's storage rather than what the owner does. Setting up one channel means visiting four pages, the channel for an import is typed instead of picked, and files have to be uploaded before they can be unticked. Now that the data model is settled (channels, shows, episodes, ads, schedules, conversions), Studio gets a new look and a new structure built around how it is used.

What the owner does in Studio, in their words: **"adding new episodes and checking they converted right"**. Checking means **status checking** (did it convert, is it on air, did it fail), not watching the converted video. Scheduling is occasional; the owner has not really used the schedule editor yet.

Where it is used: a desktop browser, mostly on another PC talking to the Pi server (server mode). Drive mode (Studio on the Mac with the USB drive) must keep working.

Success: the owner picks a folder of new episodes, unticks the ones they don't want before anything uploads, sees each file's season, episode, title and length, plays a file to check it is the right one, presses Add, and then watches the home page move each episode from uploading to converting to on air (or failed, with Retry). A channel's settings, schedule, shows and today's lineup are all on one page. It all looks like the Wii app.

## Goals, non-goals, anti-goals

- **Goals:**
  - The Wii Menu look across every Studio page, using the Wii app's colours, shapes and font.
  - A new structure: an Episodes home, then Channels with one page per channel, Ads, and Server (Drive in drive mode).
  - A new Add episodes page:
    - review and untick before upload
    - preview each source file before importing it
    - channel and show picked from lists
    - warnings for episodes that already exist, with a Replace option
  - Conversion status on the home page that survives a server restart.
- **Non-goals:**
  - Playing the converted `.mpg` files in the browser. A Wii-like player is a later project that will live on the channel's Today tab.
  - A rethink of how the schedule editor works. It gets the new look and fixes for obvious rough edges only.
  - Phone layouts. A laptop-width browser (1024 px and up) is the target.
  - Deleting episodes or shows from Studio.
  - Changes to the engine, the Wii app, `config.toml` or `library.toml` formats.
- **Anti-goals:**
  - Studio ends up heavier or slower on the Pi 3B+, e.g. a large UI kit, a CSS framework, or anything loaded from the internet. Studio must work with no internet.
  - Drive mode breaks.
  - Any upload of a file the owner unticked.
  - A Replace that leaves an episode missing if the new conversion fails.

## Constraints

- **Stack:** React 18 + plain CSS (`studio/src/web/styles.css`), Fastify server, vitest, Node 22. No new runtime dependencies **(my call, follows from approach 1)**.
- **Formats:** browsers cannot play MPEG-1/MP2 `.mpg`. Source files can often be played: MP4 (H.264/AAC) always, MKV usually in Chrome. AVI and others usually can't.
- **Upload limit:** `POST /api/upload` takes at most 50 files per request (`UPLOAD_FILES`). The server probes each upload with ffprobe and rejects too-short or unreadable files after upload.
- **Finished jobs:** the conversion queue only persists queued and running jobs (`queue.ts`, `persist`), so finished and failed jobs vanish on restart.
- **Duplicates:** the server already refuses a source that was converted before (`ALREADY_CONVERTED`, by source records) and gives a re-converted episode its earlier file name.
- **Modes:** server mode imports from a browser upload or the server's imports folder. Drive mode imports from a folder picked through the Mac's file dialog (`/api/import/pick-source`) and has no upload.
- **Font:** the Wii app's font is Rounded M+ 1c (Medium and ExtraBold), under the SIL Open Font License, in `wii/data/font_*.ttf`. It can be bundled with Studio.

## Approaches

1. **Rebuild the web pages on the current server (chosen by the owner).** Keep the server, its API and tests, and add the few endpoints the new pages need. Rebuild the pages on a small set of shared components that copy the Wii design system. Ship in usable stages. Owner's reason: it fixes adding episodes early and keeps the app small enough for the Pi.
2. **Start over with a ready-made UI kit (Tailwind + a component library).** Polished widgets for free, but every one needs restyling to look like the Wii, it adds many dependencies, and the schedule editor stays custom anyway. Rejected.
3. **Restyle first, restructure later.** The fastest visible change, but the owner's main complaint (adding episodes) waits, and every page gets styled twice. Rejected.

## Look

The look is "A · Wii Menu" (owner's choice): the same look as the Wii app.

- **Tokens:** come from `wii/source/ui/theme.h` and the Design System artifact (see memory `cable-twii-design-system-artifact`): stripes `#F4F5F7`/`#ECEEF1`, white panels with border `#D6DBE0` and a soft shadow, accent `#34BEED` (light `#7FD0EF`, dark `#2A8FB3`), text `#444` / `#999`, active row `#EEFAFF`, focus ring `#34BEED40`, warm `#F3CF8A` for warnings. Define them as CSS variables on `:root`, named after the theme (`THEME_ACCENT` → `--accent`) **(my call)**.
- **Font:** use the Wii's Rounded M+ 1c for all Studio text, with the system font as fallback **(my call)**. No WOFF2 tools are installed, so the build copies the two TTFs (about 3.5 MB each, cached by the browser) and `OFL.txt` from `wii/data` and `wii/` into the web build. They are not duplicated in git.
- **Surfaces:** a striped page background, a glossy top bar, rounded white panels (14 px radius), pill buttons with a top-to-bottom gloss, and a filled Wii-blue pill for the main action.
- **Shared components** (built once, used everywhere) **(my call)**:
  - PillButton (plain, primary, danger)
  - Panel
  - ListRow
  - Chip (neutral, ok, warn, error)
  - Tabs
  - Select (a pill-styled native `<select>`)
  - TextField, Checkbox, ProgressBar
  - Dialog (confirmations only; no multi-step dialogs)
  - Banner, EmptyState
- **Light theme only** **(my call)**: the Wii app has no dark theme.

## Structure

The top bar has the Studio name and four tabs: **Episodes** (home), **Channels**, **Ads**, and **Server** (**Drive** in drive mode). The left sidebar goes away.

Routes live in the URL hash (`#/`, `#/add`, `#/channels`, `#/channels/1/today`, `#/channels/1/schedule`, `#/channels/1/shows`, `#/channels/1/settings`, `#/folders/<folder>`, `#/ads`, `#/server`), so reload and Back work **(my call)**. A small hand-written router, no new library.

The existing unsaved-changes prompt for the schedule editor applies to any route change away from a schedule with unsaved edits.

Retired pages:
- Library: its episodes move to each channel's Shows tab, its ads to Ads, its Rebuild to Server/Drive.
- Schedule: moves into each channel page.
- Preview: becomes each channel's Today tab.

### Episodes (home)

Mockup: the "A · Wii Menu" card.

1. **Add episodes:** a drop zone that takes files dragged onto it, or a click to pick files. In server mode it also has "Use the imports folder", and in drive mode "Pick a folder" (the existing native picker). Each opens the Add episodes page.
2. **In progress:** uploads (from this browser tab) and conversions, queued or running, grouped by show and season, with per-file progress. Consecutive episodes of the same show and season collapse into one row ("Gumball S03E20–E40 · 21 waiting") **(my call)**. If uploads are running, a note says "Keep this tab open until uploads finish", because a browser upload stops if the tab closes **(my call)**.
3. **Recent:** finished and failed conversions from the last 7 days **(my call)**, newest first, grouped the same way.
   - Done rows: "18 on air".
   - Failed rows: the reason and a **Retry failed** button (the existing retry-all endpoint).
   - Upload rejections (too short, unreadable, already added) also show here, with their reason, until dismissed or the tab reloads **(my call)**.
4. "Make older videos smaller" (shrink) jobs appear in the same lists, labelled as such.

**Empty state:** "No episodes yet. Add some to get started." with the drop zone.

### Add episodes

Mockup: "Adding episodes". This is a full page, not a dialog.

- **Opening it:**
  - From the home page, the destination is guessed.
  - From a channel page's Add episodes button, that channel is preselected.
  - From the Ads page, the destination is Ads.
- **Sources:**
  - Browser files (server mode) are listed straight from the browser. Nothing is uploaded yet.
  - The imports folder (server mode) starts with today's folder browser (breadcrumbs and subfolders). Only the folder you choose is scanned, so the Pi doesn't re-probe the whole imports folder, which is never emptied **(my call)**.
  - A picked folder (drive mode) is scanned as today.
  - Both use the existing scan, which already has lengths and aspect. Files the server has converted before (its source records) are listed unticked as "Already added" **(my call)**.
  - Browser files are checked the same way before upload, by name and size against earlier uploads (the rule the upload endpoint already uses for `already on the server`) **(my call)**.
- **Each row:**
  - a tick box
  - a preview button that plays the source file in a small player
  - season and episode, worked out from the file name with the existing rules in `names.ts` (shared with the web build or mirrored), and editable
  - the file name, small, under them
  - an editable title
  - the length
  - notes
- **Previews:**
  - Browser files play from the local file.
  - Server-side sources play through a new endpoint that streams a scanned import source, limited to files under the imports folder or the picked folder.
  - If the browser can't play the format, the player says "This browser can't preview this file" instead.
- **Lengths for browser files** are read from the browser's own metadata where it can. Otherwise the length shows "—", and the server's check after upload still applies.
- **Description:** kept, as an optional field that opens per row **(my call)**.
- **Destination:**
  - One Channel select (channels from `config.toml` plus channel folders in the library, then "New channel…") and one Show select (that channel's shows, then "New show…").
  - Both are filled from the file names and the library when they match.
  - If the picked files look like more than one show, the page groups the rows by show, each group with its own Channel and Show selects **(my call)**.
  - "New show…" asks for a name and an optional icon inline.
  - "New channel…" asks for a name and creates the channel the same way the Channels page does.
- **Already there:** a row whose show, season and episode match an episode already in the library (or already queued) is unticked and marked "Already on air · Replace?".
  - Ticking it means the new file replaces the old one.
  - The episode keeps its existing file name, so on a Replace row the title is fixed to the old one and can't be edited **(my call)**.
  - The old file stays on air until the new conversion succeeds. If the conversion fails, the old file is kept. This is new server behaviour.
  - Ads have no Replace. Duplicates are simply flagged.
- **Footer:** "21 of 23 ticked · 3.9 GB to upload", with Cancel and a primary **Add 21 episodes**.
  - The mockup's conversion-time estimate is dropped, because Studio has no logic for it **(my call)**.
- **On Add:**
  - Browser files upload in batches of at most 50, ticked files only.
  - Each batch is imported once uploaded (the server probes it, then `/api/import/show` or `/api/import/ads`).
  - The page returns to the home page at once, where the uploads and conversions show.
- **Upload rejections** (too short, unreadable) appear on the home page with their reason. Successfully uploaded files that end up not imported are discarded (the existing `/api/upload/discard`).
- **Ads:** use the same page with the destination "Ads" and an "All channels / specific channel" select (the existing `channelFolder` of `/api/import/ads`). No season or episode columns.

### Channels

Mockup: left half of "Channels, and one page per channel".

- **Rows:**
  - number, logo, name
  - "Now: …" from today's lineup (`/api/preview` for today)
  - a Timed or Looping chip
  - a count of shows and episodes
  - warning chips: no shows, or schedule errors and warnings from `/api/validate`
- Click a row to open the channel page.
- **Add channel** creates a channel as today (with a placeholder schedule) and opens its Settings tab.
- **Not on a channel:** a section under the channels for library channel folders that have shows but no channel in `config.toml` (today's Add show can create these). Each opens a page with only the Shows tab, at `#/folders/<folder>` **(my call)**.

### Channel page

Mockup: right half. The header has the number, logo, name, "category · mode · ads on/off" and a primary **Add episodes**. Then four tabs:

- **Today:**
  - The day's lineup with what's on now highlighted and its progress shown.
  - It keeps the old Preview's date picker for other days **(my call)**.
  - This is where the later Wii-like player will go.
- **Schedule:**
  - The existing editor (timeline, library panel, block editor, undo/redo, zoom, Reshuffle, Save) in the new look, for this channel only. The channel chips at its top go away.
  - Rough edge to fix while restyling **(my call)**: at the default zoom (120 px per hour) a timed day is about 2,900 px tall. Timed channels open at 30 px per hour so a whole day fits on screen. Looping channels keep 120.
  - The Block panel already shows a how-to when nothing is selected; it keeps it.
- **Shows:**
  - The channel's shows, each with its icon, name, "Edit info" (the existing show meta: name, icon, descriptions), and seasons with episodes.
  - Each episode row has its title, length and a status chip: **On air**, **Converting 62%**, **Queued** or **Failed**. Status comes from the library plus the job list.
  - This tab replaces the Library page for that channel.
- **Settings:**
  - Name, number, category, folder, logo, ads on or off, and mode, with the same checks and the same mode-switch behaviour as today's Channels dialog.
  - Folder stays, because it is how a channel attaches to shows already imported (see the user guide).
  - **Delete channel** at the bottom with a confirmation. As today, this keeps the files.

### Ads

- **Ad list:** grouped under "All channels" and then each channel that has its own ads, with name and length.
- **Skipped files:** ads that were skipped (too short) are listed with the reason, from the last rebuild's notes (server change 4).
- **Add ads** opens the Add episodes page set to Ads.

### Server / Drive

- **Server mode:**
  - server name, library folder, free space, Sign out
  - Make older videos smaller
  - **Re-read library**, the old Rebuild, with its notes shown here **(my call)**
- **Drive mode:** drive choice, setup, safe to eject and Re-read library, all as today, restyled.

### Sign-in

The same form in the new look.

## Server changes

1. **Job history:**
   - Keep finished and failed jobs (with error text and a finish time) on disk for 7 days next to the queue file, so the home page's Recent list survives a restart **(my call)**.
   - Include them in `GET /api/jobs` and the stream.
   - Add `finishedAt` (and `startedAt` if cheap) to `Job`.
   - Uploaded files are still swept after 24 hours as today. A failed job whose source file is gone is flagged, shows "Add the file again" instead of being retried, and is left out of Retry failed **(my call)**.
2. **Replace an episode:**
   - `/api/import/show` accepts, per episode, the library path of the episode it replaces.
   - The conversion targets that file name.
   - Conversions already write to `<target>.part` and rename only after ffmpeg succeeds, so the old file stays on air until then, and a failure leaves it untouched.
3. **Preview a server-side source:** a new endpoint streams a file from the last import scan (imports folder or picked folder), with range requests so the browser player can seek. It is contained to those folders and gives 404 for anything else.
4. **Rebuild notes:** remember the notes from the last library rebuild, manual or automatic, in memory, and serve them, so the Ads and Server/Drive pages can show skipped files without a slow full rebuild. After a restart they are empty until the next rebuild **(my call)**.
5. **Already-added checks:** the import scan marks sources the server has converted before, using the existing source records. A new upload check answers which browser files (by name and size) were uploaded and converted before.
6. **Nothing else.** The library, config, preview, validate, show-meta, icon, shrink and drive endpoints are reused as they are.

## Staging

Each stage leaves Studio fully usable **(my call)**.

1. Look and shell:
   - tokens, font, shared components
   - new top bar and router
   - restyled Sign-in and Server/Drive
   - old pages still there, reached from the new tabs where they fit
2. Episodes home and Add episodes, with server changes 1 to 3 and 5. Retires the Add show, Add ads and Add videos dialogs.
3. Channels list, folder pages and the channel page's Today, Shows and Settings tabs, with server change 4. Retires Library, Preview and the Channels table.
4. Schedule tab: the editor in the new look inside the channel page. Retires the Schedule page.
5. Ads page and clean-up: remove the dead CSS and components, and update the user guide.

## What's left to the builder

- Component internals, exact spacing and sizes (match the mockups and the Wii tokens).
- How `names.ts`'s parsing is shared with the web build.
- How the 7-day history is stored.
- How Replace swaps files safely.
- Test layout.

Expected: vitest for the new logic (grouping and collapsing job rows, destination guessing, duplicate detection, the router, job history, Replace, the preview endpoint's containment). Each stage ends with a real-browser check against `samples/out/cable-twii` in server mode, and a check of drive mode.

## Docs

- `docs/USER-GUIDE.md`: rewrite the Studio sections for the new pages.
- `CHANGELOG.md`: add the redesign under Unreleased.
