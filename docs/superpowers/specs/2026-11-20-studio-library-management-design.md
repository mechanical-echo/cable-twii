# Studio library management — design

Date: 2026-11-20. Builds on the Studio redesign (`2026-11-20-studio-redesign-design.md`, branch `studio-redesign`).

## Intent

The owner's library is growing and spread over channels. They want to:

- open any episode or ad, watch it, fix its information and switch it on or off air;
- manage each show's seasons and episodes in one place, in a Shows tab of its own;
- choose which channels a show airs on, by ticking and unticking channels;
- put hand-picked episodes from different shows onto one channel (their example: a channel of Christmas episodes from Adventure Time and SpongeBob, which live on Cartoon Network and Nickelodeon).

The Christmas channel is only an example of the last point; it is not a feature of its own.

## Goals

1. **Episode and ad pages.** Clicking an episode or an ad anywhere in Studio opens its page:
   - a player that plays the converted file, picture and sound, as the Wii gets it;
   - its information, editable;
   - an On air / Off air switch;
   - where it airs: the channels whose schedules can play it;
   - file facts: length, size, file path, and the source it was converted from with the date, when Studio knows them;
   - Delete.
2. **A Shows tab.** A new top tab listing every show. Each show has its own page to manage its seasons and episodes, including several episodes at once.
3. **Sharing across channels.**
   - Each show has a list of all channels with a tick box: ticked means the show airs on that channel. Ticking adds it, unticking takes it off.
   - Add picked episodes of any shows to a channel.
   - Moving a show is done by hand with these boxes (untick one channel, tick another). Files never move between channel folders.

## Non-goals

- Changes to the Wii app, the scheduling engine, or any file the Wii reads beyond its normal content. The formats in `docs/formats.md` stay frozen for the Wii.
- A "shuffle my picks" rule. Picked episodes play in the order they were added (my call; it needs an engine change and a Wii test, and the owner said the themed channel is only an example).
- The Wii-like full preview of a channel (postponed earlier; still later).
- Editing the video itself (trimming, cropping).
- Phone layouts.

## Anti-goals

These would make the work a failure even if it technically works:

- Losing a video by accident. Only Delete (after a confirmation that names what goes) and Replace (already built) remove or overwrite a file.
- A schedule left pointing at something that has gone, without the owner being told. Every rename, move, switch-off or delete either updates the schedules that use it, or names the channels it would break and asks first.
- A change that leaves `config.toml` with errors. Studio checks each change with the engine's validator first, and refuses it with the reason if it would.
- More work for the Pi. Playing must not re-encode video.
- Breaking drive mode.

## Constraints

- The server is a Raspberry Pi 3B+. The Mac drive mode must keep working the same way.
- No new runtime dependencies, and nothing loaded from the internet. One exception: a small MPEG-1 player script copied into the repository with its licence (see Player).
- Same look and components as the redesign.
- Converted files are MPEG-1 video with MP2 audio in `.mpg` files (`videoSettings.ts`). Browsers can't play that format themselves.
- A show belongs to one channel folder (`<channel folder>/<show folder>/`). A channel can already air another channel's shows with `from` on a schedule item, and can name one specific episode with an `episode` item. So sharing needs no copied files.
- Shows without seasons number their episodes by file-name order (`library.ts`). Renaming such a file can change the numbers of its neighbours.

## Approaches

1. **Studio and server only (chosen).**
   - Off-air items are kept out of `library.toml` by Studio.
   - File operations live in a new server module that also rewrites the schedules that refer to the files.
   - The player is fed by the server re-packaging the `.mpg` without re-encoding.
   - No Wii update and no new Wii test.
2. **Engine changes.** Add an "off" flag and a "collection" rule (with shuffle) to the formats and the C engine. This is more powerful, but needs a Wii app update and real-Wii testing. Rejected for now.
3. **Copy files to each channel that airs them.** Simple, but doubles disk use and lets copies drift apart, while `from` already shares without copies. Rejected.

For the player:
- **Re-package plus a vendored player (chosen).** The server re-packages the `.mpg` into MPEG-TS with ffmpeg `-c copy`, which is cheap. The browser plays it with JSMpeg (MIT), a single script that decodes MPEG-1 and MP2.
- **Re-encode to H.264 on the fly.** Rejected: too slow on the Pi.
- **Play the original source file.** Rejected: uploaded sources are deleted after conversion.
- **Compile the project's own `pl_mpeg` to WebAssembly.** Rejected: needs a toolchain the project doesn't have, and more work.

## How it works

### Routes and navigation

New routes: `#/shows`, `#/shows/<channel folder>/<show folder>`, `#/episode/<path>`, `#/ad/<path>`. Paths are library-relative and URI-encoded.

- The Shows tab sits between Channels and Ads.
- Breadcrumbs, as in the redesign:
  - episode page: `‹ Shows › Gumball`;
  - ad page: `‹ Ads`;
  - show page: `‹ Shows`.
- Episodes are clickable on the show page, on a channel's Shows tab, on the Today tab and in Recent on the home page (when the file exists). Ads are clickable on the Ads page and on the Today tab.

### Off air

- Studio keeps a Studio-only file, `.studio/off.toml` (`version = 1`, `paths = [...]`). It lists the library-relative paths of switched-off episodes and ads. The Wii never reads it.
- The library build leaves off-air files out of `library.toml`, so the Wii never plays them. In shows without seasons, episode numbers are worked out over all files first and the off ones are left out afterwards, so the numbers of the others don't change.
- Studio still lists off-air items, greyed and marked "Off air", with their titles and lengths. So the build also gives Studio the details of the items it left out, through `GET /api/library` or a Studio-only file. That choice is the builder's.
- Switching off something a schedule names directly (an `episode` or `ad` item) shows those channels, and confirming removes those items. Rules (`random`, `random-run`, `fill`, ads breaks) just stop picking it. If the change would leave a rule with nothing to pick (`empty_pool`), Studio refuses and says which channel and rule to change first.
- Switching back on restores the file to the library only. Removed schedule items are not put back.

### Editing information

- **Episode:** title, description, season and episode number.
  - Changing the title or number renames the file (`SxxEyy - Title.mpg`). A new season moves it into `Season N/`, created if needed.
  - If the new number is already taken in that show, Studio refuses and says which episode has it.
  - In a show without seasons, the title is the file name, so renaming can reorder the show. Studio then rewrites the `episode` items that pointed at the moved episodes, so they keep playing the same videos. These episodes have no number field: their number is their place in file-name order. They can be moved into a season (renamed `SxxEyy - Title.mpg` into `Season N/`); there is no move back to "no season".
- **Ad:** name (renames the file) and where it plays: all channels or one channel's ads (moves the file between `ads/` folders).
- **Show** (on the show page): display name, icon and descriptions, as Edit info does today.
- **Errors that were already there don't block.** Studio compares the engine's issues before and after the change and refuses only when the change adds an error. A channel that is already broken doesn't stop edits elsewhere, including the ones that fix it.
- **The Wii catches up later.** A Wii playing from the server re-reads the library when its guide opens, at midnight, or with Reload channels in HOME. Until then a switched-off item can still air there, and a renamed, moved or deleted one fails to load and that airing is skipped. The banner after each change says so. On a drive this doesn't apply: the drive goes back to the Wii after Done.
- **Every file change is one locked operation:**
  - it moves the file;
  - it updates `.studio/sources.toml` targets, `.studio/off.toml` paths, `show.toml` description keys and the `config.toml` items that name the file;
  - it checks the new config against the new library with the engine's validator;
  - it rebuilds the library.

  If any step fails, nothing that was changed stays changed (file moved back, config not written).
- **A file with a queued or running conversion can't be renamed, moved or deleted.** Studio says it's still converting.

### Delete

- Episode, ad or whole show.
- The confirmation names the file or files, and the channels whose schedules name them directly. Confirming:
  - deletes the files (and the show folder for a show);
  - removes their `sources.toml` and `off.toml` entries;
  - removes the schedule items that name them.
- Refused, with the channel and rule to change first, if it would leave a rule with nothing to pick.
- There is no undo. The confirmation says so.

### Player

- `GET /api/media/play?path=<rel>[&start=<seconds>]` streams `ffmpeg -ss <start> -i <file> -c copy -f mpegts -`.
  - It needs a session in server mode, like the rest of `/api`.
  - The path must be a library `.mpg` inside the root (the same containment checks as `/media`).
  - The ffmpeg process stops when the browser disconnects.
- **The web player:**
  - JSMpeg plays on a canvas, with play/pause, the time, and a progress bar that seeks by restarting the stream at `start`.
  - The JSMpeg script and its licence live in `studio/src/web/vendor/`.
  - The player is 4:3 or 16:9 to match the file.
- **When the stream fails,** the player says "Can't play this file here" and still shows the file's information.

### Where it airs

Worked out from `config.toml` and the library:
- a channel airs a show when any of its items names the show (with `from` when the show lives in another channel's folder);
- it airs a specific episode when an `episode` item names it, or a rule covers its show and season.

Shown as chips with the channel name, linking to that channel's Schedule tab. An off-air item shows "Off air" instead.

### Shows tab

```
Shows                                              [Add episodes]
Cartoon Network
  [icon] Adventure Time   2 seasons · 26 episodes   Airs on: CN, Christmas
  [icon] Gumball          4 seasons · 14 episodes   Airs on: CN
Nickelodeon
  [icon] SpongeBob        1 season · 20 episodes    Airs on: Nick
Not on a channel
  [icon] Retro            1 episode                 Not airing
```

- Shows are grouped by the channel folder that holds them. Folders with no channel come last under "Not on a channel".
- Clicking a show opens its page.

### Show page

```
‹ Shows
[icon] Gumball                       [Add episodes] [Edit info] [⋯]
Stored in Cartoon Network
Airs on  ☑ CN   ☐ MTV   ☐ Nick   ☑ Christmas
Season 1                                                [tick all]
 ☐ S01E01  Test One     0:12  On air   Airs on CN
 ☐ S01E02  Test Two     0:30  Off air
Season 2 …
With 2 ticked: [Move to season…] [Switch off] [Switch on] [Add to a channel…] [Delete]
```

- **Rows:** tick box, code, title, length, the on/off chip and conversion status (from the redesign). Clicking a row opens the episode page.
- **Airs on:** one tick box per channel (see "Channels a show airs on" below).
- **The `⋯` menu:** Delete show.
- **Bulk actions on ticked episodes:**
  - **Move to season N:** renumbers them after the season's last episode, in their current order.
  - **Switch off** and **Switch on.**
  - **Add to a channel…** Appends `episode` items, with `from`, in the shown order. They go at the end of a looping channel's loop, or at the end of a chosen slot on a timed channel.
  - **Delete.**
- **After each change,** a banner says what changed, with a link to the affected channel's Schedule tab.

### Channels a show airs on

- A channel counts as airing a show when any item in its schedule names the show: a rule or a picked episode, with `from` when the show is stored in another channel's folder. The box is ticked for those channels.
- **Ticking a channel** adds one "Random episode ×1" rule for the show (with `from` when needed):
  - on a looping channel, at the end of its loop;
  - on a timed channel, at the end of a slot the owner picks in a small dialog (the last slot is preselected).

  The owner tunes the rule in that channel's schedule editor.
- **Unticking a channel** removes every item on that channel that names the show, rules and picked episodes alike. A confirmation lists them first.
  - If that would leave a looping channel with an empty loop (an engine error), Studio refuses and says to add something else to that channel first.
- The show's files stay where they are. Unticking the channel whose folder stores the show is allowed; the show then airs only where it is still ticked, or nowhere ("Not airing").
- The Shows tab groups shows by the folder that stores them, so a show can be listed under a channel it no longer airs on. Its row shows the channels it airs on.

### Server

- **A new `library-edit` module** does rename, move, renumber, switch on and off, delete, and move show. Each is one call that:
  - locks with the existing config lock;
  - plans the file moves and config rewrite;
  - validates the candidate config against a candidate library (the validator needs to accept a candidate library as well as a config);
  - applies the change, rolling back on failure;
  - rebuilds.
- **Endpoints** (session-guarded):
  - `POST /api/library/rename` (episode or ad);
  - `POST /api/library/renumber`;
  - `POST /api/library/off` (paths, on/off);
  - `POST /api/library/delete` (paths or a show);
  - `POST /api/shows/airs-on` (tick or untick one channel for one show);
  - `POST /api/schedule/append` (add a rule or episode items to a channel's loop or slot);
  - `GET /api/media/play`.
- **Every endpoint takes the config `mtimeMs`,** like `PUT /api/config`, and answers 409 if the config changed.

## Stages

Each stage leaves Studio usable.

1. **Episode and ad pages with the player.** Routes, the read-only information, where it airs, and clickable rows on existing pages.
2. **Editing.** Information edit with renames, On/Off air, and Delete for single episodes and ads.
3. **The Shows tab and show page,** with bulk actions (move season, switch off and on, delete).
4. **Sharing.** The Airs on tick boxes, and adding picked episodes to a channel.

## My calls

- Off air means off on every channel. It is stored in a Studio-only file, and the Wii never sees the item.
- Switching off or deleting removes schedule items that name the file directly; switching back on doesn't put them back.
- Picked episodes play in the order added; there's no shuffle rule.
- Picked episodes of a show without seasons are named by their place in that show. Adding new files to such a show later can shift those places. Fixing that would mean rewriting schedules in the background after each conversion, so it is out of scope. Instead, the Add episodes page warns when the destination is such a show and channels have picked episodes from it, naming those channels.
- Library edits are refused only for errors they add, not for errors that were already there.
- A server-mode Wii catches up with edits when its guide opens, at midnight, or with Reload channels; the banner after a change says so.
- Episodes of a show without seasons have no editable number; they can be moved into a season, not back.
- Delete is included (the redesign had left it out), behind a confirmation with no undo.
- Ticking a channel adds one "Random episode ×1" rule; on a timed channel it goes at the end of a slot the owner picks. The owner tunes it in the schedule editor.
- Unticking a channel removes every item there that names the show, after a confirmation that lists them.
- There is no "move show" operation and files never move between channel folders (the owner's choice: moving is done by hand with the tick boxes).
- The player seeks by restarting the stream at a time, not by byte ranges.
- The Shows tab groups shows by the folder that holds them, not by the channels that air them (a show can air on many).

## Left to the builder

- How Studio gets the details of off-air items (an extra field on `GET /api/library` or a Studio-only file).
- Exact copy of banners and confirmations, in the house style.
- JSMpeg version and how it's loaded (one vendored file), and the player's controls layout.
- How rollback is done (for example moving files back from a list of done steps).
- Whether the "Air on" chips link to the Schedule tab or the Today tab.
