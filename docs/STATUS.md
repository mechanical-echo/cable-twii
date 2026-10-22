# cable-twii status (2026-10-22)

One-page overview of the plans in `docs/superpowers/plans/`. Each plan also has its own "Status" section and ticked checkboxes.

## Per plan

| Plan | Done | Left | Blocked on |
|---|---|---|---|
| 00 Roadmap | Repo and toolchain (Mac and Windows), sample library generator, `docs/formats.md`, provisional decision 0002, both spikes built | Decision 0001 (video format), final 0002, final roadmap review | Owner's real-Wii video test; USB spike is paused by the owner and its safety review is not done |
| 01 Engine | Everything; merged, tests pass on the host | Nothing | Nothing |
| 02 Studio | Tasks 1-11, reviewed, plus final review, fix wave and Windows port | Task 12, end-to-end check on a real flash drive | Owner |
| 03 Wii app | Tasks 1-10 and the 50-item checklist | Task 11: run the checklist on a real Wii, fix failures | Owner and real hardware |
| 05 Wii redesign | Tasks 1-11 on branch `wii-redesign` (kit, guide, system bar, OSD, chip, volume, up-next card, HOME menu, intro, empty state, error dialog, checklist items 51-55); guide, chip, OSD and volume panel checked in Dolphin | Dolphin checks of the HOME menu, intro, up-next card, error dialog and the pressed pill looks; the Design System artifact re-sync; merge into master | Owner (emulator runs were stopped at the owner's request) |
| 04 Release | `make dist`, `CHANGELOG.md`, owner's guide | Owner checks of both; forwarder research, build, install; final acceptance; v1.0.0 tag | Owner approval of the research downloads (Task 3); Tasks 4-6 not started |

## Left that does not need hardware

- Forwarder research and build (release Tasks 3-4), after the owner approves the downloads.
- Install document for the channel (release Task 5, Step 1).
- Decision 0001 once the video test numbers exist (the writing, not the test).
- Triage of the deferred minor review findings logged in the progress ledgers (`.superpowers/sdd/*/progress.md`, git-ignored).
- Known Wii gaps:
  - The up-next card has no title for the last airing of the day.
  - The countdown shows minutes, not hours, past 60 minutes.
  - Channel and show icons decode on the render thread.
  - PAL 50 Hz shows thin bands top and bottom; a full-height mode is a possible follow-up.
- Wii redesign (plan 05): remaining Dolphin checks listed above, then the Design System artifact re-sync from the new `theme.h` tokens.
- Owner questions to settle: should Studio's "Done — safe to eject" button actually eject the drive, or just get an honest label (today it finishes pending work and cleans up, but you still eject in Finder); and should picking a folder on the Mac's internal disk get extra safeguards (for example a test-folder confirmation, or refusing broad folders like Documents and Desktop).

## Needs the owner or real hardware

- Wii hardware checklist (`docs/testing/wii-checklist.md`), plan 03 Task 11.
- Studio end-to-end check on a real flash drive (plan 02 Task 12).
- Video test on the real Wii (roadmap Task 2).
- USB two-drive test and its safety review (roadmap Task 3, paused).
- Check the SD card package in the Homebrew Channel (release Task 1, Step 2).
- Follow the owner's guide from a fresh flash drive (release Task 2, Step 2).
- Install the channel and run the final acceptance (release Tasks 5-6).
- Windows smoke test of Studio with a FAT32 stick, once on a drive letter and once on a mount folder.

## Where things are

- Owner's guide: `docs/USER-GUIDE.md`
- Hardware checklist: `docs/testing/wii-checklist.md`
- File formats: `docs/formats.md`
- Engine API: `docs/engine-api.md`
- Release package: `make -C wii dist` (output in `dist/apps/cable-twii/`)
- Changes by version: `CHANGELOG.md`
