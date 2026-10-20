# cable-twii — Design

Date: 2026-10-20
Name: **cable-twii** (chosen by the owner)

Two connected projects:

1. **cable-twii** — a homebrew app for a modded Nintendo Wii that behaves like cable television, playing the owner's own videos and ads from a USB flash drive on a custom schedule.
2. **cable-twii Studio** — a tool on the owner's Mac that converts videos into a Wii-playable format, builds the library on the drive, and lets the owner design channel schedules with a drag-and-drop timeline.

Decisions marked **(my call)** were made by the designer, not the owner, and may be revisited. Everything else was stated by the owner.

**Mockups** (open in a browser; the owner approved both layouts):

- [mockups/wii-guide-and-fullscreen.html](mockups/wii-guide-and-fullscreen.html) — the Wii guide, the fullscreen channel chip, and the fullscreen info/controls panel. Channel number circles in the list are replaced by channel icons (owner's request after the mockup).
- [mockups/studio-schedule-editor.html](mockups/studio-schedule-editor.html) — the Studio schedule timeline.

---

## 1. Intent

The owner has a modded Wii and wants it to feel like a TV with cable: turn it on, see a guide of channels, point the Wii Remote at one, and watch whatever is "on" right now. The content, channels, schedule and ads are all the owner's, stored on a hard drive.

It must feel **live**: tuning to a channel joins whatever is playing at this moment, part-way through, exactly as real TV does. Flipping away and back finds the same show further along. It must look **authentically Wii**: like a first-party Nintendo channel, not generic homebrew.

An earlier idea (a YouTube frontend) was dropped because it depends on YouTube's ever-changing internals. This project is deliberately **offline and self-contained**.

### Success looks like

- The owner launches cable-twii, the logo animates, and the guide appears with all their channels and what's on each right now.
- Hovering a channel shows its live preview; clicking goes fullscreen; the D-pad flips channels like a remote.
- Schedules they designed in Studio play out on the Wii exactly as Studio's preview said they would.
- Video plays smoothly with audio in sync.

### Goals

- Live-TV behaviour driven purely by the Wii's clock.
- Two schedule kinds: **looping** and **timed** (tied to real clock times).
- Random programming (random episodes, random sequential runs, random ads) that is nevertheless **identical on every tune-in** for a given day.
- Channel icons and per-show icons in the UI.
- An editor where episodes are dragged onto a timeline and blocks are sized by real duration.
- Wii-Menu-style visuals, cursor, sounds and rumble.
- Runs from the Homebrew Channel; optional Wii Menu channel via a forwarder WAD at the end.

### Non-goals (for this version)

- No internet streaming of any kind (no YouTube, no network sources).
- No pausing, rewinding or recording live channels — it is TV **(my call)**.
- No different schedules per day of the week (easy to add later).
- No editing schedules on the Wii itself; editing happens in Studio.
- No subtitles **(my call)**.
- No Wii U native build, no GameCube/Classic Controller support **(my call)**.
- Studio is for the owner's Mac only; no Windows/Linux support required **(my call)**.

### Anti-goals (would make this a failure even if it technically works)

- Tuning in restarts an episode from the beginning, or shows a different random pick each time.
- Video stutters, tears, or audio drifts out of sync.
- It looks like a typical homebrew app (grey text on black, default fonts) instead of a Wii channel.
- Studio's preview disagrees with what the Wii actually airs.
- Installing it puts the console at risk (bricking).
- Using Nintendo's copyrighted assets (fonts, sounds, logos) in the app **(my call)** — the style is imitated with original or openly licensed assets.

---

## 2. Constraints

### Wii hardware

- CPU: 729 MHz PowerPC "Broadway". RAM: 88 MB total (24 MB MEM1 + 64 MB MEM2).
- GPU: "Hollywood", programmed through GX. No YUV texture format; textures max 1024×1024; textures must be in GX tiled formats.
- No hardware video decoder. All video decoding is software on the CPU.
- Video output at most 480p (640×480 framebuffer), 4:3 or 16:9 (anamorphic) depending on the console setting.
- Audio output 48 kHz stereo.
- Real-time clock set by the owner in Wii Settings (local time).

### Storage

- The owner's existing **120 GB hard drive holds WBFS game files for a USB loader. cable-twii and Studio must never write to, format or repartition it.**
- cable-twii media goes on a **separate ~120 GB USB flash drive in the Wii's other USB port** (owner's plan), formatted **FAT32** with the MBR partition scheme (macOS Disk Utility: "MS-DOS (FAT)", "Master Boot Record"), which is what libfat reads reliably **(my call on format)**. The 4 GB per-file limit doesn't matter for the converted files. At about 0.8 GB per hour of converted video, 120 GB holds roughly 140 hours.
- **Open risk — two USB devices at once:** homebrew USB storage support on the Wii has traditionally talked to one device, often the first one found. Phase 2 tests on the real Wii, with both drives plugged in, whether cable-twii can reach the flash drive and tell it apart from the games drive. Fallbacks, in order: (1) select the device that has a `/cable-twii/` folder on a FAT partition and ignore the rest; (2) an **SD/SDHC card** in the front slot (up to 32 GB, roughly 40 hours of video), which is always supported; (3) swap drives when switching between games and TV.
- Expected size **(owner)**: about 3 channels with many shows. Designed for up to 50 channels and 5,000 episodes **(my call)**.
- macOS writes hidden `._*` and `.DS_Store` files onto FAT drives; both sides must ignore/clean them.

### Owner's environment

- Mac (macOS) for development and for running Studio.
- A modded Wii with the Homebrew Channel.
- Before any WAD install (final phase only): BootMii NAND backup and Priiloader installed.

---

## 3. System overview

```
 Owner's Mac                                  Wii
 ┌────────────────────────────┐  media drive  ┌──────────────────────────┐
 │ cable-twii Studio          │  (FAT32)      │ cable-twii (boot.dol)    │
 │  - import & convert videos ├──────────────►│  - reads config+library  │
 │  - channel settings        │ /cable-twii/  │  - computes today's      │
 │  - schedule timeline       │               │    lineup (same engine)  │
 │  - day preview             │               │  - guide, preview, play  │
 │      uses schedule engine  │               │                          │
 └────────────────────────────┘               └──────────────────────────┘
          ▲                                               ▲
          └──────── shared C "schedule engine" ───────────┘
```

The **schedule engine** is one C library compiled twice: natively on the Mac (as a command-line tool Studio calls) and into the Wii app. This is how Studio's preview and the Wii are guaranteed to agree **(my call)**.

### Source repository layout **(my call)**

```
cable-twii/
├── engine/            shared C: config/library parsing, schedule resolution, PRNG
│   ├── src/
│   ├── include/
│   ├── tests/         host unit tests (run on the Mac)
│   └── cli/           cable-twii-sched: native CLI used by Studio
├── wii/               devkitPPC app (Makefile, src/, data/ fonts+sounds+images)
├── studio/            Mac tool (server/ + web/)
├── tools/             helper scripts (deploy, sample-library generator)
├── samples/           tiny sample library for testing
└── docs/
```

---

## 4. The drive

### Layout

The data root is `/cable-twii/` on the media drive (or SD card). The Wii app looks for a `/cable-twii/` folder on every FAT partition it can mount on USB, then on `sd:` **(my call — USB first because that's where the owner keeps it; SD is needed for testing in Dolphin)**. Drives without that folder, including the WBFS games drive, are never written to.

```
usb:/cable-twii/
├── config.toml                  channels + schedules (written by Studio)
├── library.toml                 index of every video with duration (written by Studio)
├── state.toml                   last channel, volume (written by the Wii app)
├── ads/                         shared ads, used by any channel with ads = true
│   ├── soda-commercial.mpg
│   └── toy-commercial.mpg
├── Cartoon Network/             a channel folder (default name = channel name)
│   ├── channel.png              channel icon
│   ├── ads/                     optional: this channel's own ads, used instead of shared ads
│   ├── Gumball/                 a show folder
│   │   ├── show.png             show icon (optional; falls back to channel icon)
│   │   ├── show.toml            optional: display name, episode descriptions
│   │   ├── Season 1/
│   │   │   ├── S01E01 - The DVD.mpg
│   │   │   └── S01E02 - The Responsible.mpg
│   │   └── Season 2/
│   ├── Adventure Time/
│   └── Ben 10/
└── mtv-00s/                     custom folder for a channel
    ├── channel.png
    └── Music Videos/            show without seasons = treated as one season
        ├── Artist - Song.mpg
        └── ...
```

The Homebrew Channel app itself lives separately at `sd:/apps/cable-twii/` (`boot.dol`, `meta.xml`, `icon.png`), or `usb:/apps/cable-twii/`.

### Naming rules

- Episode files: `SxxEyy - Title.mpg`. Title is optional. In a show without season folders, files are ordered by filename and the filename (minus extension) is the title.
- Season folders: `Season N`.
- Names must be FAT-safe; Studio sanitizes illegal characters (`\ / : * ? " < > |`) when it writes files.
- Icons: PNG, 128×128, RGBA **(my call)**. Studio resizes whatever image the owner provides.

### Video format (converted by Studio)

**(my call, final numbers fixed by the Phase 1 spike)**

| Property | Value |
|---|---|
| Container | MPEG program stream (`.mpg`) |
| Video | MPEG-1 video, 30 fps, keyframe every 15 frames (0.5 s) |
| Resolution | 640×360 for 16:9 sources, 480×360 for 4:3 sources (both multiples of 16); lower if the spike shows the Wii can't keep up |
| Video bitrate | about 1.5 Mbit/s |
| Audio | MP2, 48 kHz stereo, 192 kbit/s, loudness-normalized so ads and shows play at the same volume |

Why MPEG-1: it is light enough to decode in software on the Wii, and the small `pl_mpeg` library decodes it (video + audio + seeking) with no other dependencies. Frequent keyframes make joining mid-episode fast.

---

## 5. File formats

All files are TOML **(my call)**: readable by hand, allow comments, and parsed on the Wii by the small `tomlc99` C library. Studio writes `config.toml` and `library.toml`; the owner may hand-edit `config.toml`, but Studio does not preserve hand-written comments when it saves **(my call)**.

### 5.1 `config.toml`

```toml
version = 1
seed = 1234          # changing it reshuffles every random pick (Studio's "Reshuffle" button)

[[channel]]
number   = 1
name     = "Cartoon Network"
category = "Kids"
folder   = "Cartoon Network"   # optional; defaults to name
icon     = "channel.png"       # optional; defaults to <folder>/channel.png
ads      = true
mode     = "timed"             # "timed" or "looping"

  [[channel.slot]]
  at = "07:00"
  items = [
    { type = "random-run", show = "Gumball", count = 3 },
    { type = "ads", count = 1 },
    { type = "random", show = "Adventure Time", season = 5, count = 5 },
    { type = "ads", count = 2 },
    { type = "episode", show = "Ben 10", season = 1, episode = 5 },
    { type = "random", show = "Gumball", count = 2 },
  ]

  [[channel.slot]]
  at = "19:00"
  items = [
    { type = "fill", show = "Adventure Time", ads_between = 2 },
  ]

[[channel]]
number   = 2
name     = "MTV"
category = "Music"
folder   = "mtv-00s"
ads      = false
mode     = "looping"
loop = [
  { type = "random", show = "Music Videos", count = 10 },
]
```

**Channel fields:** `number` (unique, 1–999), `name`, `category`, `folder`, `icon`, `ads`, `mode`. A timed channel has `slot` entries; a looping channel has `loop`.

**Slot fields:** `at` (HH:MM, 24-hour). A slot runs until the next slot's `at`; the last slot of the day runs until the first slot's `at` on the next day. A timed channel must have at least one slot.

**Item types:**

| type | fields | meaning |
|---|---|---|
| `episode` | `show`, `season`, `episode` | one specific episode |
| `random` | `show`, `season`?, `count` | `count` random episodes, no repeats within the item |
| `random-run` | `show`, `season`?, `count` | random starting episode, then the following episodes in order |
| `ads` | `count` | `count` random ads (skipped when the channel has `ads = false`) |
| `ad` | `path` | one specific ad (path relative to `/cable-twii/`); always airs, even on a channel with `ads = false`, because it was placed deliberately **(my call)** |
| `fill` | `show`, `season`?, `ads_between`? | random episodes until the slot is full (timed channels only) |

`show` refers to the show's folder name inside this channel's folder. `season` omitted means all seasons. Any show item may add `from = "<other channel folder>"` to air a show that lives in another channel's folder, so files never need copying to put a show on two channels **(my call)**.

### 5.2 `library.toml` (machine-generated by Studio)

```toml
version = 1
generated = 2026-10-20T14:00:00

[[ad]]
path = "ads/soda-commercial.mpg"
channel = ""                     # "" = shared; otherwise channel folder name
duration_ms = 30030

[[show]]
channel = "Cartoon Network"
folder = "Gumball"
name = "The Amazing World of Gumball"   # from show.toml, else folder name
icon = "Cartoon Network/Gumball/show.png" # "" if none

  [[show.episode]]
  season = 1
  episode = 1
  title = "The DVD"
  description = "Gumball and Darwin..."
  path = "Cartoon Network/Gumball/Season 1/S01E01 - The DVD.mpg"
  duration_ms = 665120
```

Paths are relative to `/cable-twii/`. The Wii app trusts this file and does not scan the drive or open videos to learn durations.

### 5.3 `show.toml` (optional, per show; edited in Studio)

```toml
name = "The Amazing World of Gumball"

[episodes]
"S01E01" = "Gumball and Darwin make a TV ad for the video store."
```

Studio folds these into `library.toml`.

### 5.4 `state.toml` (written by the Wii app)

```toml
last_channel = 1
volume = 80
```

---

## 6. Schedule engine (shared C library)

The heart of the system. Given the config, the library, a date and a channel, it produces that day's **lineup**: an ordered list of *airings* (start time, length, what plays). Everything is deterministic: same inputs → same lineup, on the Mac and on the Wii.

### 6.1 Determinism

- Own PRNG (PCG32) seeded from `hash(config.seed, channel.number, date YYYYMMDD)` **(my call)**. Never `rand()`, never floating point in decisions, explicit byte-wise hashing so big-endian Wii and little-endian Mac agree.
- Times are integer milliseconds since local midnight.

### 6.2 Random picking

- Each show+season pool has a **shuffle bag** per channel per day: episodes come out in a random order without repeats until the bag is empty, then it refills **(my call)**. This satisfies "5 random episodes, not sequential, no duplicates" and avoids repeats during a day.
- `random-run`: choose a random start among valid starting points so that `count` consecutive episodes exist in the pool (in season/episode order).
- Ads: random from the channel's own `ads/` folder if it has any, otherwise from shared ads; never the same ad twice in a row.

### 6.3 Timed channels

For each slot, the window is `[at, next slot's at)`. Items are placed one after another from the window start:

- `episode`: placed; if it would run past the window end it is **cut off at the window end**, like real TV (Studio flags it red).
- `random` / `random-run` / `fill`: only episodes that **fit in the remaining time** are chosen. If no candidate fits, the item stops early. For `random-run`, if the full run doesn't fit, the longest run that fits is used **(my call)**. This answers "what if a random episode is longer than expected": it never overruns; it is swapped for one that fits.
- `fill` repeats random picks (with `ads_between` ads between them on ad channels) until nothing more fits.
- `ads` items: skipped when `ads = false`.
- **Leftover time** at the end of a window:
  - ads channel: filled with random ads; if what remains is shorter than any ad, an "Up next" card covers it.
  - no-ads channel: an **"Up next" card** (channel icon, next show and its start time, countdown) covers the gap **(my call)**.
- The last slot of the day continues past midnight into the next day until the first slot's time. To find what's on at time *t*, the engine checks today's lineup and the tail of yesterday's.

### 6.4 Looping channels

- The loop list starts at 00:00 each day and repeats until midnight; random picks are re-drawn each pass (bags continue).
- Whatever is playing at midnight is cut off and the next day's loop starts fresh **(my call — keeps every day computable on its own)**.
- `fill` is not allowed in loops (validation error).

### 6.5 Validation (shared by Studio and the Wii)

Errors: duplicate channel numbers, unknown show, missing episode, `fill` in a loop, timed channel without slots, malformed time, empty item pool, missing files listed in the library.
Warnings: fixed episode cut off by the next slot, random item that could not place all `count` episodes.

### 6.6 API (sketch, builder may refine)

```c
int  ctw_load(const char *root, ctw_db **out, ctw_error *err);
int  ctw_lineup(const ctw_db *db, int channel_number, ctw_date day, ctw_lineup *out);
int  ctw_now(const ctw_db *db, int channel_number, ctw_datetime now, ctw_airing *out, uint32_t *offset_ms);
void ctw_free(ctw_db *db);
```

An airing is: start_ms, duration_ms, kind (episode / ad / up-next card), pointer to the episode or ad, plus the item it came from.

### 6.7 CLI `cable-twii-sched` (built natively on the Mac)

```
cable-twii-sched validate <root>
cable-twii-sched lineup   <root> --channel 1 --date 2026-10-20 --json
cable-twii-sched now      <root> --channel 1 --at "2026-10-20 19:42"
```

Studio calls it for validation and the day preview.

---

## 7. cable-twii (the Wii app)

### 7.1 Technology **(my call)**

- C, built with devkitPro **devkitPPC** + **libogc**, `libfat` (SD + USB), WPAD (Wii Remote), **GRRLIB** for 2D drawing, PNG and TrueType text (FreeType), **ASND** for audio, **pl_mpeg** for MPEG-1 video/audio, **tomlc99** for TOML, and the shared engine.
- Loaded via IOS58 (Homebrew Channel default) for USB 2.0 speed.
- Developed and tested in the **Dolphin** emulator on the Mac (data on Dolphin's virtual SD card), then on real hardware by sending `boot.dol` over Wi-Fi with `wiiload`.

### 7.2 Screens and flow

```
Boot → Logo animation → Guide ⇄ Fullscreen
                 any screen → HOME menu overlay
       errors → Error dialog (Wii style)
```

**Boot:** mount drives, find `/cable-twii/`, load `config.toml` + `library.toml`, validate, compute today's lineups, read `state.toml`. Errors (no drive, no data folder, config error with line number, library missing — "Prepare this drive with cable-twii Studio") are shown in a Wii-style dialog with a button to return to the Homebrew Channel.

**Logo animation:** the cable-twii logo animates in at the centre (scale + fade + shine), then smoothly shrinks and moves into its place in the guide header while the guide panels slide/fade in. Loading happens behind the animation.

**Guide** (as in the approved mockup):

- **Header** **(my call)**: small logo top-left, current time (Wii-Menu style clock).
- **Left — channel list:** a white rounded panel. Each row: channel icon, channel name, current show's icon, current show title + episode, and start–end times of what's on (e.g. `14:30 – 14:41`) with a thin progress bar. Rows are grouped under **category headings** **(my call — the owner's `category` field needed a place in the UI)**. Hovered row gets the Wii blue outline and glow. The list scrolls when there are more channels than fit: point near the top/bottom edge or use the D-pad **(my call)**.
- **Top right — live preview** (quarter of the screen): the hovered channel playing live, with sound **(my call)**, in a rounded frame. Changing hover waits ~300 ms before switching, to avoid thrashing while the cursor sweeps.
- **Bottom right — info panel:** channel name, show icon, show name, episode name, description, time and progress.
- **Pointing/D-pad:** pointer hovers rows; D-pad up/down also moves the highlight.
- **A / click on a row:** the preview expands smoothly into fullscreen, keeping the same video stream so there is no re-load.

**Fullscreen:**

- Video fills the screen (letterboxed/pillarboxed to keep aspect ratio; respects the Wii's 4:3/16:9 setting).
- **D-pad up/right = next channel, down/left = previous** **(my call on the left/right mapping)**. On switch, a rounded chip in the top-right shows the channel **icon and number** for 2 seconds.
- **Pointing at the screen** shows the info + controls panel at the bottom: channel, show, episode, times, progress, and pill buttons `◀ CH`, `CH ▶`, `Guide`, plus volume. It hides after 3 s without pointer movement or when the pointer leaves the screen.
- **+ / −** change volume (on-screen volume bar). **B** returns to the guide **(my call)**.
- If the remote is not pointing at the screen, nothing covers the picture.

**Up next card:** full-screen Wii-style card with channel icon, "Up next", show name and start time, and a countdown, used to cover schedule gaps.

**HOME menu** **(my call — authentic Wii behaviour)**: the HOME button dims the screen and shows an overlay with "Wii Menu", "Homebrew Channel", "Resume", and the remote's battery level. Power and Reset buttons exit cleanly.

### 7.3 Visual style

- Based on the Wii Menu: light grey striped background, white rounded panels with soft grey borders, Wii blue (`#34BEED`) highlights and glow, pill-shaped gradient buttons, hand-shaped Wii-style cursor that tilts with the remote's roll.
- Font: **M PLUS Rounded 1c** (open font licence), close in feel to the Wii's rounded type **(my call)**.
- Hover **sounds** (soft clicks/ticks) and a **tiny rumble pulse** when the cursor enters a button or row, as the Wii Menu does **(my call)**. Sounds are original, not Nintendo's.
- Animations eased (ease-out), 60 fps.
- Logo: an original placeholder logo; owner can replace the image file later.

### 7.4 Playback

- One video decoder at a time (the preview *is* the fullscreen stream).
- To tune: ask the engine what's airing now and the offset into it, open the file, seek to the offset (pl_mpeg seeks to the nearest keyframe), start.
- The **clock is the master**: when the airing's scheduled end arrives, move to the next airing even if the file has a few frames left; if a file ends early, show black until the next airing. This keeps every channel locked to the schedule.
- At midnight lineups are recomputed for the new day.
- Threads: a decode thread reads the file and fills a small frame queue (~3 frames) and an audio ring buffer; ASND plays audio; the main thread renders at 60 Hz (each video frame shown twice at 30 fps). Audio is the A/V sync reference.
- Frames are converted from YUV on the GPU: the Y, U and V planes are uploaded as three 8-bit textures and the GX TEV stages combine them into RGB, avoiding a CPU colour conversion.
- Targets: steady 30 fps with no audio dropouts; channel change under 1.5 s; guide UI stays at 60 fps while the preview plays.

### 7.5 Other behaviour

- `state.toml` saves last channel and volume; next launch opens the guide with that channel hovered **(my call)**.
- Wii Remote idle timeout lengthened so the remote doesn't disconnect while someone is watching but not touching it; any remote can control the app.
- Files starting with `._` and `.DS_Store` are ignored.
- Text strings are rendered to textures once and cached, to keep the UI fast.

---

## 8. cable-twii Studio (the Mac tool)

### 8.1 Technology **(my call)**

- **Node.js (LTS) + TypeScript** server on `localhost`, a **React + Vite** web UI opened in the browser, **dnd-kit** for drag and drop, **ffmpeg/ffprobe** (installed with Homebrew) for conversion, `sharp` for icon resizing, a TOML library for reading/writing, and the native `cable-twii-sched` CLI for validation and preview.
- Started with one command (`npm start`), which opens the browser.

### 8.1a Source of truth **(my call)**

- **The drive is the source of truth.** Studio reads from and writes straight to the drive's `/cable-twii/` folder; there is no separate project on the Mac and no export step. Conversions write directly to the drive (the files are small and low-bitrate, so USB write speed is not a bottleneck).
- Studio's own bookkeeping lives on the drive in `/cable-twii/.studio/` (ignored by the Wii app): `sources.toml` maps every converted file to its original Mac path, size and modified time (used to skip re-conversion), plus the conversion queue so an interrupted batch can resume.
- Conversions write to a temporary `*.part` file and rename on success, so an unplugged drive never leaves a half-written video that looks complete.
- If the drive is unplugged, Studio shows "Drive not connected" and disables editing until it returns. Hand edits to `config.toml` are picked up when Studio reloads.

### 8.2 Screens

1. **Drive** — choose the drive (or any folder, for testing); show free space, whether it is FAT32, whether `/cable-twii/` exists; create the folder structure. Studio never formats or repartitions a drive. "Done" button runs `dot_clean` and reminds the owner to eject safely.
2. **Library** — import and convert:
   - **Add show**: pick a source folder on the Mac, choose the target channel and show name. Studio lists the video files and guesses season/episode/title from names (`S01E02`, `1x02`, `Episode 2`, etc.); the owner fixes any guesses in a table; optional show icon image and episode descriptions.
   - **Add ads**: pick files, choose shared or channel-specific.
   - **Conversion queue** with progress per file; runs several conversions in parallel; skips files already converted unless the source changed; originals are never modified.
   - After conversion, durations are measured and `library.toml` is regenerated.
3. **Channels** — add/edit/delete/reorder channels: number, name, category, folder, icon (image picker, resized to 128×128), ads on/off, timed or looping.
4. **Schedule** — the timeline editor (as in the approved mockup):
   - Left: library list of the selected channel — shows → seasons → episodes with durations, plus draggable rule blocks: "random episode", "random sequential run", "fill", "random ad", and specific ads.
   - Right: the channel's timeline. **Timed:** a vertical 24-hour timeline with slot start lines; items stack one after another inside each slot; slots are added at a time and their start time is editable. **Looping:** a vertical list with the loop's total length.
   - **Block height = video length.** Fixed episodes and ads are exact. Random blocks are drawn striped with an estimated height (average length of their pool).
   - Drag to add, drag to reorder, click to edit (count, season, pick type, ads between), delete key/button to remove. Zoom the timeline.
   - Overlaps and problems are shown live: a block running into the next slot turns red with the overlap length; leftover ad-filled time is shown as an orange band.
   - **Reshuffle** button changes `seed`.
   - **Save** validates via `cable-twii-sched` and writes `config.toml` (previous version kept as `config.bak.toml`).
5. **Preview** — pick a channel and a date; shows the exact resolved lineup (what the Wii will air), as a TV-guide list, using `cable-twii-sched lineup`. This is where random picks become concrete.

### 8.3 Conversion command (starting point; tuned by the Phase 1 spike)

```
ffmpeg -i <src> \
  -vf "scale=640:360:force_original_aspect_ratio=decrease,pad=ceil(iw/16)*16:ceil(ih/16)*16:(ow-iw)/2:(oh-ih)/2,fps=30" \
  -c:v mpeg1video -b:v 1500k -maxrate 2000k -bufsize 1000k -g 15 \
  -af loudnorm=I=-16:TP=-1.5:LRA=11 -c:a mp2 -ar 48000 -ac 2 -b:a 192k \
  -f mpeg <dst>.mpg
```

(4:3 sources use 480×360.)

---

## 9. Delivery order

1. **Setup:** Homebrew, ffmpeg, Node, devkitPro (devkitPPC, libogc, libfat, GRRLIB and its portlibs), Dolphin, wiiload; confirm the Wii's Homebrew Channel version and that it can receive wiiload over Wi-Fi.
2. **Video spike (throwaway):** a bare Wii app that plays one converted `.mpg` with audio, seeks to an offset, and reports fps — first in Dolphin, then on the real Wii. Try 640×360, 480×360 and lower. Decides the final resolution and bitrate. The same spike, on the real Wii with **both the WBFS games drive and the media flash drive plugged in**, lists every USB device and partition it can see and reads the test video from the flash drive; this decides whether the two-drive setup works or a fallback from section 2 is needed. **These are the biggest technical risks and go first.**
3. **Formats:** freeze the drive layout and the TOML schemas above; create the sample library.
4. **Schedule engine** + host unit tests + `cable-twii-sched` CLI, plus a tiny Wii test app proving the Wii computes identical lineups to the Mac.
5. **Studio:** drive + library import + conversion.
6. **Studio:** channels + schedule timeline + preview.
7. **Wii app core:** boot/loading/errors, player (seek, A/V sync, YUV on GPU), fullscreen, channel switching, clock-driven airing changes.
8. **Wii app UI:** guide, live preview, info panel, OSD, channel chip, logo animation, HOME menu, sounds, rumble, up-next card, state saving.
9. **Hardening:** long-run test (several hours, midnight rollover), real-hardware performance, USB drive compatibility, error cases.
10. **Release packaging:** Homebrew Channel folder (`boot.dol`, `meta.xml`, `icon.png`).
11. **Forwarder WAD (optional, last):** a Wii Menu channel with its own banner and icon that launches `apps/cable-twii/boot.dol`, with a unique title ID. Requires research into which forwarder base and WAD packing tools work from macOS (e.g. Python-based WAD tools). **Only installed after a BootMii NAND backup and with Priiloader present.**

## 10. Testing

- **Engine:** host unit tests for every item type, fitting rules, shuffle bags, midnight wrap, empty pools, validation; golden lineup files; a Wii-side check that lineup hashes match the Mac's.
- **Studio:** unit tests for filename parsing, TOML read/write round-trips and validation display; manual UI checks for drag and drop.
- **Wii app:** Dolphin for UI flows; real Wii for performance and USB; a written checklist (tune-in mid-episode, D-pad flipping, preview→fullscreen, OSD show/hide, HOME menu, remote disconnect/reconnect, no drive, bad config, midnight rollover, 4:3 and 16:9 TV settings).

## 11. Risks

| Risk | Mitigation |
|---|---|
| Wii too slow to decode MPEG-1 at 640×360 | Spike first; lower resolution/bitrate; fall back to the faster `libavcodec` MPEG decoder if needed |
| cable-twii can't reach the flash drive while the games drive is also plugged in | Tested in the Phase 2 spike; fallbacks: pick the drive with `/cable-twii/`, SD card, or swapping drives |
| Some USB drives don't work with the Wii | Document drive requirements; SD card fallback |
| Seek latency makes channel changes slow | Frequent keyframes (every 0.5 s); measure in spike |
| Dolphin behaves differently from hardware | Test on hardware from Phase 2 onward via wiiload |
| Wii clock wrong | Shown in guide header so it's obvious; owner sets it in Wii Settings |
| WAD install bricks the console | Forwarder only, last, after NAND backup + Priiloader |

## 12. Left to the builder

- Exact C API and data structures of the engine, module boundaries in the Wii app.
- Exact animation timings, colours beyond the Wii blue, panel sizes (follow the mockup).
- The sound design and the placeholder logo.
- Studio's internal API between server and web UI.
- Final ffmpeg settings within the limits set by the spike.
