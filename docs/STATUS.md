# cable-twii status (2026-11-19)

One-page overview of the plans in `docs/superpowers/plans/`. Each plan also has its own "Status" section and ticked checkboxes.

## Per plan

| Plan | Done | Left | Blocked on |
|---|---|---|---|
| 00 Roadmap | Repo and toolchain (Mac and Windows), sample library generator, `docs/formats.md`, provisional decision 0002, both spikes built | Decision 0001 (video format), final 0002, final roadmap review | Owner's real-Wii video test; USB spike is paused by the owner and its safety review is not done |
| 01 Engine | Everything; merged, tests pass on the host | Nothing | Nothing |
| 02 Studio | Tasks 1-11, reviewed, plus final review, fix wave and Windows port | Task 12, end-to-end check on a real flash drive | Owner |
| 03 Wii app | Tasks 1-10 and the 50-item checklist; first real-Wii runs on 2026-11-18 (4:3 CRT, 50 Hz): error screen, HOME menu and exits work after three fixes on master (50 Hz frame copy overflow, USB start-up, HOME menu without a schedule) | Task 11: the rest of the checklist on the real Wii with shows on the card | Owner and real hardware |
| 05 Wii redesign | Tasks 1-11 on branch `wii-redesign` (kit, guide, system bar, OSD, chip, volume, up-next card, HOME menu, intro, empty state, error dialog, checklist items 51-55), reviewed and fixed; every screen checked in Dolphin with the scripted-input build (`make -C wii DEBUG_SCRIPT=1`); Design System artifact re-synced (tokens and component notes, main previews redrawn) | real-Wii run of checklist items 51-55 | Owner and real hardware |
| 04 Release | `make dist`, `CHANGELOG.md`, owner's guide, decision 0003, forwarder source with host tests, banner assets and builder, `tools/build-wad.sh`, channel boot and missing-file message checked in Dolphin (merged to master 2026-10-22), channel install guide | Owner checks of both; banner, icon and sound check on Dolphin's Wii Menu (Task 4 Step 3); channel install by the owner; final acceptance; v1.0.0 tag | The banner check needs a Wii Menu in Dolphin (owner's NAND backup imported); Tasks 5-6 need the owner and the Wii |
| 06 Streaming test | Test app on branch `streaming-test`, run on the real Wii; decision 0004 accepted: lower-bitrate server profile, continuous streaming client, network retries, no seek index | The 30-minute run, with the plan 08 client | Plan 08 |
| 07 Studio server mode | Tasks 1-8 on branch `studio-server` (running on the owner's Mac): `npm run serve` with a settings file, shared-password sign-in, the read-only Wii interface `/wii/v1/`, browser uploads and an imports folder, the two-editor save check and the web UI; Windows start script, install guide (`docs/SERVER.md`) and `docs/formats.md` sections; builds and tests pass on Linux. Tasks 9-11 (server conversion profile, shrink step, pick-before-upload and channel dropdown) are merged into master and running on the owner's Mac | The check of `docs/SERVER.md` on the Windows 11 PC | Owner |
| 08 Wii server source | Tasks 1-12 on branch `wii-server-source`: settings file, address parsing, HTTP client, library loading, streaming playback, start-up with drive fallback, weak signal and rejoin, Settings screen and keyboard, Reload channels, docs and checklist items 56-66; host tests and the Wii build pass | The Dolphin pass over items 56, 59, 62, 65 and the drive items waits for the owner's go-ahead. Items 57, 58, 60, 61, 63 and 66 need the real Wii at home; the 30-minute run of plan 06 is item 58 | Owner (Dolphin go-ahead) and real hardware |

## Left that does not need hardware

- Banner, icon and sound check on Dolphin's Wii Menu (release Task 4 Step 3) once the owner's NAND backup is imported into Dolphin.
- Decision 0001 once the video test numbers exist (the writing, not the test).
- Triage of the deferred minor review findings logged in the progress ledgers (`.superpowers/sdd/*/progress.md`, git-ignored).
- Known Wii gaps:
  - The up-next card has no title for the last airing of the day.
  - The countdown shows minutes, not hours, past 60 minutes.
  - Channel and show icons decode on the render thread.
  - PAL 50 Hz shows thin bands top and bottom; a full-height mode is a possible follow-up.
- Wii redesign (plan 05): the Design System artifact re-sync from the new `theme.h` tokens.
- Owner questions to settle: should Studio's "Done — safe to eject" button actually eject the drive, or just get an honest label (today it finishes pending work and cleans up, but you still eject in Finder); and should picking a folder on the Mac's internal disk get extra safeguards (for example a test-folder confirmation, or refusing broad folders like Documents and Desktop).

## Needs the owner or real hardware

- Wii hardware checklist (`docs/testing/wii-checklist.md`), plan 03 Task 11, and items 56-66 for plan 08.
- Dolphin pass over items 56, 59, 62, 65 and the drive items that touch boot, state and the HOME menu (plan 08 Task 12 Step 3), once the owner agrees to a Dolphin session.
- Studio end-to-end check on a real flash drive (plan 02 Task 12).
- Video test on the real Wii (roadmap Task 2).
- USB two-drive test and its safety review (roadmap Task 3, paused).
- Check the SD card package in the Homebrew Channel (release Task 1, Step 2).
- Follow the owner's guide from a fresh flash drive (release Task 2, Step 2).
- Install the channel and run the final acceptance (release Tasks 5-6).
- Windows smoke test of Studio with a FAT32 stick, once on a drive letter and once on a mount folder.
- Follow `docs/SERVER.md` on the Windows 11 PC, then run `curl http://<pc-ip>:4747/wii/v1/info` from the Mac and sign in from a phone (server plan Task 8 Step 3).

## Where things are

- Owner's guide: `docs/USER-GUIDE.md`
- Channel install guide: `docs/INSTALL-CHANNEL.md`
- Server install guide: `docs/SERVER.md`
- Hardware checklist: `docs/testing/wii-checklist.md`
- File formats: `docs/formats.md`
- Engine API: `docs/engine-api.md`
- Release package: `make -C wii dist` (output in `dist/apps/cable-twii/`)
- Channel WAD: `tools/build-wad.sh` (output `dist/cable-twii-forwarder.wad`; needs `.tooling/cert-chain.bin`, see decision 0003); tests `make -C forwarder/tests test`, `python3 -m unittest discover -s tools/tests -p 'test_make_*.py'`, `python3 -m unittest discover -s tools/tests -p test_build_banner.py`, `.tooling/wiipy-venv/bin/python -m unittest discover -s tools/tests -p test_build_wad.py`
- Changes by version: `CHANGELOG.md`
