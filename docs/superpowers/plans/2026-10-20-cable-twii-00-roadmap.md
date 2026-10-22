# cable-twii Roadmap and Foundations Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Set up the repository and toolchains, run the two hardware spikes that decide the video format and the drive setup, and freeze the shared file formats so the engine, Studio and Wii app plans can run.

**Architecture:** One monorepo (this folder) with `engine/` (shared C schedule engine), `studio/` (Mac tool), `wii/` (homebrew app), `spikes/` (throwaway experiments), `tools/`, `samples/` (generated test library) and `docs/`. Spikes run on the real Wii before any product code exists.

**Tech Stack:** Homebrew, devkitPro (devkitPPC, libogc, libfat), GRRLIB, pl_mpeg, ffmpeg, Node.js 22 LTS, Dolphin emulator, wiiload.

**Spec:** `docs/superpowers/specs/2026-10-20-cable-twii-design.md`

## Plan set and order

| # | Plan | Depends on |
|---|---|---|
| 00 | this roadmap (setup, spikes, format freeze) | — |
| 01 | `2026-10-20-cable-twii-01-engine.md` | 00 |
| 02 | `2026-10-20-cable-twii-02-studio.md` | 01 (uses `cable-twii-sched`) |
| 03 | `2026-10-20-cable-twii-03-wii-app.md` | 01, and the spike decisions from 00 |
| 04 | `2026-10-20-cable-twii-04-release.md` | 02, 03 |

Plans 02 and 03 are independent of each other once 01 is done and may run in parallel.

## Status (2026-10-22)

- Done: Task 0 (repo and toolchain; also set up on Windows) and Task 1 (sample library generator). Task 4 Step 3 (`docs/formats.md`) is done and reviewed.
- Task 2 video spike is built and works in Dolphin; the owner's real-Wii run (Steps 2, 4, 5) is pending.
- Task 3 USB spike is built but PAUSED by the owner; its safety review is not done and it must not be run until it is.
- Task 4: `0002-drive-setup.md` exists as a provisional record (SD card as data location); `0001-video-format.md` is open until the Task 2 numbers exist.
- The final roadmap review is deferred until the hardware tasks finish.

## Global Constraints

- App name: `cable-twii`; Mac tool: `cable-twii Studio`; CLI: `cable-twii-sched`; C symbol prefix `ctw_`.
- Data root on the drive: `/cable-twii/`. HBC app folder: `apps/cable-twii/`.
- The 120 GB WBFS games drive is never written to, formatted or repartitioned by any code or step.
- Media drive: FAT32, MBR. Wii searches every mountable USB FAT partition for `/cable-twii/`, then `sd:/cable-twii/`.
- Video: MPEG-PS `.mpg`, MPEG-1 video 30 fps, GOP 15, 640×360 (16:9) / 480×360 (4:3), ~1500 kbit/s; MP2 48 kHz stereo 192 kbit/s, `loudnorm=I=-16:TP=-1.5:LRA=11`. Final resolution/bitrate are set by Task 2.
- No Nintendo assets (fonts, sounds, logos).
- No comments in source code (owner's rule). No trailing whitespace.
- Commit messages: `wip | <short>` for new functionality, `fix | <short>` for bug fixes, `test | <short>` for tests; end with the `Co-Authored-By` attribution line.
- Downloading or installing anything (Homebrew packages, devkitPro, Dolphin, npm packages, vendored libraries) needs the owner's explicit yes at execution time.

## Review Focus

- The games drive is plugged in during hardware tests: the USB spike must prove it is never opened for writing.
- Seeking into the middle of a file must land within 0.5 s of the requested offset, or "live" tune-in is wrong.
- Dolphin results are not trusted for performance: every fps number in the decision record comes from the real Wii.
- macOS `._*` files on the FAT drive must not break file listing (spike skips names starting with `._`).
- A Wii set to 50 Hz must still play the 30 fps test file with audio in sync (Task 2 Step 4 runs both).

---

### Task 0: Repository and Mac toolchain

> **Note (2026-10-22):** development has also happened on Windows (not just Mac) — Studio was ported (see `studio/` commits on `master`) and the Task 0 tools (devkitPro, GRRLIB, Dolphin) were installed there too, by hand where the installer needs a GUI. See the "Progress" section near the top of `2026-10-20-cable-twii-03-wii-app.md` for exactly what was installed and the Windows-specific gotchas hit along the way. This task's steps below are still written Mac-first; treat them as reference, not as unstarted.

**Files:**
- Create: `README.md`, `.gitignore`, `docs/setup.md`

- [x] **Step 1: Ask the owner to approve the installs in Step 2** (Homebrew packages, devkitPro installer, Dolphin, GRRLIB clone). Wait for a yes.

- [x] **Step 2: Install tools**

```bash
brew install ffmpeg node@22
```

```bash
brew install --cask dolphin
```

devkitPro: install the latest `devkitpro-pacman-installer.pkg` from `github.com/devkitPro/pacman/releases`, then confirm package names with `dkp-pacman -Sl | grep -E "wii-dev|ppc-"` and install:

```bash
sudo dkp-pacman -S wii-dev ppc-libpng ppc-freetype ppc-libjpeg-turbo ppc-zlib
```

Add to `~/.zprofile`: `export DEVKITPRO=/opt/devkitpro`, `export DEVKITPPC=$DEVKITPRO/devkitPPC`, `export PATH=$DEVKITPRO/tools/bin:$PATH`.

- [x] **Step 3: Install GRRLIB from source**

```bash
git clone https://github.com/GRRLIB/GRRLIB.git ~/src/GRRLIB && make -C ~/src/GRRLIB/GRRLIB clean all install
```

Expected: `libgrrlib.a` installed under `$DEVKITPRO/portlibs/wii/lib`.

- [x] **Step 4: Verify toolchain**

Run: `powerpc-eabi-gcc --version | head -1; wiiload 2>&1 | head -1; ffmpeg -version | head -1; node --version`
Expected: four version lines, node `v22.x`.

- [x] **Step 5: Create repo skeleton**

`git init` in this folder; create `engine/ studio/ wii/ spikes/ tools/ samples/ docs/decisions/`. `.gitignore`: `node_modules/`, `dist/`, `build/`, `*.dol`, `*.elf`, `.superpowers/`, `.DS_Store`, `._*`, `samples/out/`. `docs/setup.md` records Steps 2–4 plus the Wii prerequisites: Homebrew Channel installed, Wii on Wi-Fi, its IP noted, `export WIILOAD=tcp:<wii-ip>`. `README.md`: one paragraph on what cable-twii is, links to the spec and plans.

- [x] **Step 6: Commit**

```bash
git add -A && git commit -m "wip | repo skeleton and setup notes"
```

---

### Task 1: Sample media library generator

**Files:**
- Create: `tools/make-samples.sh`, `samples/README.md`

**Interfaces:**
- Produces: `samples/out/cable-twii/`, a complete drive data folder (spec §4) generated by ffmpeg, used by every later plan's manual tests:
  - `Cartoon Network/Gumball/Season 1/S01E01 - Test One.mpg` … `S01E04`, and `Season 2/S02E01`, `S02E02`: 600 s each.
  - `Cartoon Network/Adventure Time/Season 5/S05E01` … `S05E05`: 660 s each.
  - `Cartoon Network/Ben 10/Season 1/S01E05 - Long One.mpg`: 1320 s, 4:3.
  - `ads/ad-a.mpg` 30 s, `ads/ad-b.mpg` 15 s, `ads/ad-c.mpg` 60 s.
  - `mtv-00s/Music Videos/Artist 1 - Song 1.mpg` … `Artist 6 - Song 6.mpg`: 200 s each.
  - `channel.png` in both channel folders and `show.png` in Gumball: 128×128 solid-colour PNGs.

- [x] **Step 1: Write `tools/make-samples.sh <out-dir> [--short]`.** Video source `testsrc2` (shows) / `smptebars` (ads) with `drawtext` burning in show, `SxxEyy` and `%{pts\:hms}` timecode; audio `sine`. Encode with the Global Constraints settings. `--short` divides every duration by 20. The burned-in timecode is how later manual tests check seek accuracy.

- [x] **Step 2: Run it**

Run: `tools/make-samples.sh samples/out --short && ffprobe -v error -show_entries format=duration -of csv=p=0 "samples/out/cable-twii/Cartoon Network/Gumball/Season 1/S01E01 - Test One.mpg"`
Expected: a value between `29.9` and `30.1`.

- [x] **Step 3: Commit**

```bash
git add tools/make-samples.sh samples/README.md && git commit -m "wip | sample library generator"
```

---

### Task 2: Video playback spike (throwaway, real Wii)

**Files:**
- Create: `spikes/video/Makefile` (from devkitPro's wii template), `spikes/video/source/main.c`, `spikes/video/source/pl_mpeg.h` (vendored from `github.com/phoboslab/pl_mpeg`, MIT)

Throwaway: no tests, no review, never imported by product code.

- [x] **Step 1: Build a bare app** that mounts `sd:` and `usb:`, opens `/cable-twii-spike/test.mpg` with `plm_create_with_filename`, uploads the Y/U/V planes as three `GX_TF_I8` textures, combines them into RGB in TEV stages, draws fullscreen at the correct aspect, plays MP2 audio through ASND at 48 kHz, and shows fps, decode ms per frame and audio underruns on screen. Button 1 seeks to 300 s with `plm_seek` and logs how long the seek took.

- [ ] **Step 2: Encode test files** at 640×360/1500k, 480×360/1500k, 512×288/1200k and 384×216/900k from one real episode the owner provides (at least 6 minutes long, with a timecode burned in via `drawtext`).  (needs the owner's real episode — pending)

- [x] **Step 3: Run in Dolphin** with its virtual SD card pointing at the files. This only proves the program works; ignore its fps.

- [ ] **Step 4: Run on the real Wii** with `wiiload spikes/video/boot.dol`, each file for 2 minutes, with the Wii set to 60 Hz and then 50 Hz.  (owner, real Wii — pending)
Expected record per file: average fps (target 30.0), worst decode ms, underruns (target 0), seek time to 300 s (target under 1.5 s), timecode error after seek (target under 0.5 s).

- [ ] **Step 5: If 640×360 misses 30 fps,** compare the TEV path with a CPU YUV→RGB path once, then pick the largest resolution that holds 30 fps with zero underruns. If nothing above 384×216 does, record "evaluate libavcodec MPEG-1 decoder" as a follow-up in Task 4.  (depends on Step 4 — pending)

---

### Task 3: Two-drive USB spike (throwaway, real Wii)

**Files:**
- Create: `spikes/usb/Makefile`, `spikes/usb/source/main.c`

- [x] **Step 1: Build an app that only reads.** Initialise USB storage under IOS58, enumerate every device and partition libogc/libfat can see, print each with its type (FAT/other) and whether `/cable-twii/` exists on it, then read 50 MB from `/cable-twii/` on the flash drive and print MB/s. Nothing is opened for writing.

- [ ] **Step 2: Run on the real Wii** in four setups: flash drive alone in port 0; flash alone in port 1; games drive port 0 + flash port 1; flash port 0 + games port 1.  (owner, real Wii — PAUSED by owner, do not run)
Expected record per setup: devices listed, `/cable-twii/` found or not, read speed (needs at least 1 MB/s; video is about 0.2 MB/s).

- [ ] **Step 3: Confirm the games drive still works** in the owner's USB loader afterwards.  (owner — paused; safety review of the no-write guarantee not done)

---

### Task 4: Decision record and format freeze

**Files:**
- Create: `docs/decisions/0001-video-format.md`, `docs/decisions/0002-drive-setup.md`, `docs/formats.md`
- Modify: `docs/superpowers/specs/2026-10-20-cable-twii-design.md` (§2 Storage, §4 Video format, §8.3)

- [ ] **Step 1: Write 0001** with the Task 2 results table and the chosen resolutions, bitrate and YUV path (TEV or CPU). Update spec §4 and §8.3 to match.  (open — waits on the Task 2 hardware results)

- [ ] **Step 2: Write 0002** with the Task 3 results table and the decision: two drives (with which port), SD card, or swapping. Update spec §2.  (provisional 0002 written (SD card as data location); final waits on Task 3)

- [x] **Step 3: Write `docs/formats.md`**: the frozen schemas of `config.toml`, `library.toml`, `show.toml`, `state.toml`, `.studio/sources.toml` and the CLI JSON outputs, taken from spec §5, §6.7, §8.1a and plan 01 Task 9, all at `version = 1`. Any later change bumps `version` and updates engine and Studio in the same commit.

- [ ] **Step 4: Commit**  (pending — only Step 3 and provisional Step 2 are committed)

```bash
git add docs/ && git commit -m "wip | spike decisions and frozen formats"
```

Next: plan 01.
