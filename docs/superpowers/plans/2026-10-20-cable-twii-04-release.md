# cable-twii Release and Forwarder Channel Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Ship cable-twii as a Homebrew Channel app with an owner's guide, pass a full end-to-end acceptance run, then (optionally) add a Wii Menu channel through a forwarder WAD installed safely.

**Architecture:** `make dist` assembles `dist/apps/cable-twii/`. The forwarder is a small separate channel that only launches `apps/cable-twii/boot.dol` from SD or USB; the app itself is never packed inside a WAD, so updates only replace `boot.dol`.

**Tech Stack:** devkitPPC (from plan 03), a forwarder base and WAD packing tool chosen in Task 3 (candidates: an open-source HBC-style forwarder plus Python WAD tools such as libWiiPy/WiiPy, which run on macOS), the owner's existing WAD manager on the Wii.

**Spec:** `docs/superpowers/specs/2026-10-20-cable-twii-design.md` (§9 steps 10–11, §11 risks)

## Status (2026-10-22)

- Done: Task 1 Steps 1 and 3 (`make dist` and `CHANGELOG.md`), Task 2 Steps 1 and 3 (`docs/USER-GUIDE.md`, merged).
- Waiting on the owner: Task 1 Step 2 (check the package on a real SD card and the Homebrew Channel) and Task 2 Step 2 (follow the guide from a fresh flash drive).
- Done: Task 3 (forwarder research), recorded in `docs/decisions/0003-forwarder.md`: our own forwarder source after the EasyRPG design, IOS58, title ID `CTWA`, libWiiPy/WiiPy packing, our own banner builder.
- In progress: Task 4 (forwarder build) on branch `forwarder`: `forwarder/` with host tests, banner assets, `tools/build_banner.py`, `tools/build_wad.py` and `tools/build-wad.sh` are done; Step 3 (Dolphin): the channel starts the app and shows the missing-file message (checked 2026-10-22); the banner, icon and sound check waits for a Wii Menu in Dolphin (the owner's NAND backup imported).
- Not started: Tasks 5-6 (safe install on the Wii, final acceptance and the v1.0.0 tag).

## Global Constraints

- The forwarder is installed **only** after: a BootMii NAND backup exists and is copied to the Mac (`nand.bin` + `keys.bin`, size checked), and Priiloader is installed and confirmed to open on boot by holding Reset.
- The owner performs every install on the Wii themselves; no step of this plan installs anything on the console.
- Never touch system titles, IOS, the System Menu, or any existing channel.
- The forwarder's title ID must not belong to any real Nintendo or third-party title; its title type is a normal channel (`00010001`).
- No Nintendo assets in the banner, icon or sound.
- No comments in source code. No trailing whitespace.
- Commits: `wip | …`, `test | …`, `fix | …` plus the attribution line.

## Review Focus

- The forwarder must still start cable-twii when `boot.dol` is on USB but there is no SD card in the slot (Task 4 Step 3 test case).
- If `apps/cable-twii/boot.dol` is missing, the forwarder must return to the Wii Menu with a message, not hang on a black screen (Task 4 Step 3 test case).
- A WAD built with a clashing title ID would overwrite another channel: the ID check in Task 3 Step 3 must happen before any build.
- The owner's guide must say plainly that the games drive is never formatted by any step (Task 2).
- Uninstalling the channel must leave the console exactly as before (Task 5 Step 4).

---

### Task 1: Homebrew Channel package

**Files:**
- Modify: `wii/Makefile` (add `dist` target)
- Create: `wii/meta.xml` final values, `CHANGELOG.md`

- [x] **Step 1: Add `make dist`** producing `dist/apps/cable-twii/boot.dol` (release build, no `CTW_DEBUG_OVERLAY`), `meta.xml` (version from `CHANGELOG.md`'s top entry, release date `YYYYMMDDHHMMSS`), `icon.png` (128×48).

- [ ] **Step 2: Verify** (owner, real Wii — pending)

Run: `make -C wii dist && ls dist/apps/cable-twii`
Expected: `boot.dol  icon.png  meta.xml`. Copy to the SD card; the Homebrew Channel lists "cable-twii" with the icon and description, and launching it reaches the guide.

- [x] **Step 3: Commit**

```bash
git add wii CHANGELOG.md && git commit -m "wip | homebrew channel package"
```

---

### Task 2: Owner's guide

**Files:**
- Create: `docs/USER-GUIDE.md`

- [x] **Step 1: Write the guide**, in plain language, covering in this order: what you need (Wii with Homebrew Channel, flash drive or SD card, Mac); formatting **only the flash drive** in Disk Utility (MS-DOS (FAT), Master Boot Record), with a bold warning to double-check the drive name so the games drive is never selected; starting Studio (`cd studio && npm start`); adding shows and ads; setting up channels; building schedules (looping vs timed, random blocks, what red overlap means); Preview; Done & eject; copying `apps/cable-twii/` to the SD card; which USB port to use (from decision 0002); setting the Wii clock; controls on the Wii; troubleshooting for every error message in plan 03 Task 3 and Task 4 copy.

- [ ] **Step 2: Owner reads it** and follows it once from a fresh flash drive; every place they got stuck is fixed in the guide. (owner reads and follows it — pending)

- [x] **Step 3: Commit**

```bash
git add docs/USER-GUIDE.md && git commit -m "wip | owner guide"
```

---

### Task 3: Forwarder research and decision

**Files:**
- Create: `docs/decisions/0003-forwarder.md`

- [x] **Step 1: Ask the owner to approve the research downloads** (forwarder source code and WAD tools, each named with source and size). Wait for a yes. (approved 2026-10-22)

- [x] **Step 2: Evaluate forwarder bases** against: open source and buildable with devkitPPC on macOS; loads a configurable path from SD then USB; works under the IOS the Homebrew Channel uses; returns to the Wii Menu with a message when the file is missing. Evaluate WAD packing on macOS: building a channel WAD from a DOL + `opening.bnr` + title ID, fakesigned, using maintained tooling (start with libWiiPy/WiiPy). (done 2026-10-22)

- [x] **Step 3: Pick a title ID** (4 characters, e.g. `CTWA`), and check it against public title databases (e.g. WiiBrew's title list and GameTDB) so it collides with no known channel or game. Record the search results. (done 2026-10-22)

- [x] **Step 4: Write 0003** with the chosen forwarder, tool versions, build commands, title ID, and the exact path the forwarder loads (`apps/cable-twii/boot.dol`). (done 2026-10-22)

- [x] **Step 5: Commit** (done 2026-10-22)

```bash
git add docs/decisions/0003-forwarder.md && git commit -m "wip | forwarder decision"
```

---

### Task 4: Forwarder build

**Files:**
- Create: `forwarder/` (forwarder source or a pinned submodule per 0003), `forwarder/banner/` (banner image, icon image, banner sound sources), `tools/build-wad.sh`

**Interfaces:**
- Produces: `dist/cable-twii-forwarder.wad`.

- [x] **Step 1: Make the banner assets**: banner 608×456 and icon 128×96 in the guide's Wii style (stripes, cable-twii logo); a short original jingle (under 5 s) for the banner sound. (done 2026-10-22: `forwarder/banner/banner.png` and `icon.png` from `tools/make_banner_assets.py`; the 2.4 s jingle is generated at build time by `tools/make_jingle.py`)

- [x] **Step 2: Write `tools/build-wad.sh`** following 0003: build the forwarder DOL with the `apps/cable-twii/boot.dol` path, build `opening.bnr` from the assets, pack the WAD with the chosen title ID. (done 2026-10-22: `make -C forwarder`, `tools/build_banner.py`, `tools/build_wad.py`; the script stops with instructions until `.tooling/cert-chain.bin` has been fetched)

- [ ] **Step 3: Test in Dolphin** (Tools → Install WAD into Dolphin's own emulated NAND, never the real console): (partly done 2026-10-22 by booting `dist/cable-twii-forwarder.wad` directly, which installs it into Dolphin's NAND; a first build showed only a black screen until the real-mode boot stub at 0x3400 was added, see 0003)
  - the channel shows on Dolphin's Wii Menu with the banner, icon and sound; (open: needs a Wii Menu in Dolphin, which this Mac gets only from the owner's NAND backup)
  - starting it launches cable-twii from the virtual SD card; (done 2026-10-22: intro at 3 s, guide at 5 s)
  - with no `boot.dol` on the SD card it returns to the Wii Menu with the message; (done 2026-10-22: status lines, a 10 s USB wait, the message for 5 s, then the return, which ends emulation because Dolphin has no Wii Menu)
  - USB-only launch cannot be tested in Dolphin and is checked in Task 5.

- [x] **Step 4: Commit** (done 2026-10-22 on branch `forwarder`; Step 3 stays open)

```bash
git add forwarder tools/build-wad.sh && git commit -m "wip | forwarder channel build"
```

---

### Task 5: Safe installation on the real Wii (owner performs)

**Files:**
- Create: `docs/INSTALL-CHANNEL.md`

- [ ] **Step 1: Write the install document** with these checks, each with how to confirm it: (not started)
  1. BootMii NAND backup made, `nand.bin` and `keys.bin` copied to the Mac, `nand.bin` about 528 MB.
  2. Priiloader installed; holding Reset while turning the Wii on opens Priiloader.
  3. The WAD manager the owner already uses is ready on the SD card.
  4. The WAD's title ID matches 0003.
  Then installing the WAD with the WAD manager, and how to uninstall it (WAD manager uninstall, or Data Management → Channels).

- [ ] **Step 2: Owner completes checks 1–4** and confirms each in chat before installing. (not started)

- [ ] **Step 3: Owner installs the WAD.** Verify: channel appears on the Wii Menu; launches cable-twii with `boot.dol` on SD; launches it with `boot.dol` only on USB and no SD card; the games drive still works in the USB loader. (not started) Note from the Task 4 review (2026-10-22): the channel's USB search is libogc's single-device one, so with the games drive plugged in it may not see the flash drive (0003, "USB limit"); if the USB-only launch fails with both drives, keep `boot.dol` on the SD card or follow up by giving the forwarder the app's device scan.

- [ ] **Step 4: Owner tests the uninstall once and reinstalls**: after uninstalling, the Wii Menu looks as before and the console boots normally. (not started)

- [ ] **Step 5: Commit** (not started)

```bash
git add docs/INSTALL-CHANNEL.md && git commit -m "wip | channel install guide"
```

---

### Task 6: Final acceptance run

- [ ] **Step 1: Full run with the owner's real library**, both drives plugged in: Studio import → schedules → Preview; Wii from the Wii Menu channel: logo → guide → preview → fullscreen → flip → OSD → HOME → back to the Wii Menu. Compare 5 random times of day with Studio's Preview for the same date: all match. (not started)

- [ ] **Step 2: Tag the release** (not started)

```bash
git tag v1.0.0
```
