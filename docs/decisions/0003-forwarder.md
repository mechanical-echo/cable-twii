# 0003: Forwarder channel

Status: **Accepted** and built (release Task 4, merged to master on 2026-10-22). Checked in Dolphin on 2026-10-22: the channel starts the app from the SD card and shows the missing-file message. The banner has not been seen yet (no Wii Menu in Dolphin), and the real-Wii install (Task 5) can still send it back here.

## Context

The Wii Menu channel is a forwarder: a small program installed as a channel that only starts `apps/cable-twii/boot.dol` from the SD card or a USB drive. The app itself is never packed into the WAD, so an update only replaces `boot.dol`.

Release plan Task 3 asked for a forwarder base that is open source and builds with devkitPPC on macOS, loads a fixed path from SD and then USB, works under the IOS the Homebrew Channel uses, and returns to the Wii Menu with a message when the file is missing. It also asked for a way to build the channel WAD on the Mac (DOL plus `opening.bnr` plus title ID, fakesigned) with maintained tooling, and for a title ID that collides with nothing known.

Toolchain on the Mac (plan 03): devkitPPC r51 (gcc 16.2.0, binutils 2.47), libogc 3.2.1, libfat 2.1.0. CMake is not installed.

## Forwarder bases evaluated

Cloned on 2026-10-22 into `.tooling/forwarders/` (ignored by git) with the owner's approval.

| Base | Licence | How it starts the app | Builds here | Devices and path | Missing file |
|---|---|---|---|---|---|
| wii-forwarder-metaxml (saulfabregwiivc, commit 6cd7b32, 2022) | zlib-style notice in `main.c`, no licence file; ships `app_booter.bin`, a GPLv2 binary by Dimok | Reads the DOL into MEM2 and jumps into app_booter, which copies it into place | No: needs `ntfs.h` and `ext2.h` (libntfs and libext2fs are not devkitPro portlibs) and calls `__exception_closeall`, which libogc 3.2.1 no longer exports | SD, then up to four USB partitions (FAT, NTFS, ext2); paths fixed to `apps/usbloader` | Returns to the Wii Menu silently from a black screen |
| nintendont-autoboot-forwarder (FIX94, commit 7a7cafe, 2017) | MIT; bundled app_booter sources GPLv2, "requires devkitPPC R17" | Same app_booter approach | No: `ogc/lwp_threads.h` and `__lwp_thread_stopmultitasking` are gone from libogc 3 | SD only | Prints "boot.dol not found!", waits 2 s, exits |
| EasyRPG Wii-Channel (commit 5f53420, 2021) | No licence file (GitHub reports none) | Self-contained: linked at 0x81230000, high in MEM1; reads the DOL into MEM2 at 0x92000000, copies the sections into place itself and jumps. No app_booter | Not as shipped: CMake, and `__exception_closeall` again. Its three C files otherwise compile against libogc 3.2.1 | sd, usb, gca, gcb through libfat; a list of paths | Shows a message for 5 s, then exits |

What the evaluation showed:

- app_booter would put a GPLv2 binary inside our WAD. The self-contained approach needs only libogc and libfat.
- `__exception_closeall` is not needed on libogc 3.2.1: `SYS_ResetSystem(SYS_SHUTDOWN, 0, 0)` runs the registered reset functions, shuts the IOS subsystems down, disables interrupts, disables the locked cache and clears the low-memory areas at 0x80000040 and 0x80002FC0 to 0x80003080 (libogc 3.2.1 `system.c`, checked in the disassembly during the review).
- `cable-twii.dol` (9.3 MB) occupies 0x80003F00 to 0x809A7020 (text, data and bss), so everything from 0x81230000 up is free for a forwarder, with about 8.5 MB of headroom.

## Decision

### Forwarder

Our own source in `forwarder/` (release Task 4), written for libogc 3.2.1 after the EasyRPG design. No code is copied from that repository, which has no licence. The program is about 150 lines of C:

1. Start video with a text console and print what it is doing (looking on the SD card, on USB, starting), so a stuck boot is readable.
2. Mount `sd` (`__io_wiisd`) and look for `sd:/apps/cable-twii/boot.dol`. If it is not there, mount `usb` (`__io_usbstorage`, retrying for up to 10 s because drives take time to answer) and look for `usb:/apps/cable-twii/boot.dol`. The path is a build-time macro.
3. Read the file to 0x92000000 in MEM2. The limit is 0x933E0000, so a DOL may be up to about 19 MB.
4. Check the DOL header: every text, data and bss range must lie inside 0x80003400 to 0x81230000 and inside the file. Otherwise show a message.
5. Unmount, shut both devices down, and clear the Homebrew Channel stub magic at 0x80001804. The app looks for `STUBHAXX` there (`wii/source/quit.c`) and must not try to return to a loader that is not in memory.
6. Copy the sections into place, zero the bss, flush the data cache and invalidate the instruction cache.
7. `SYS_ResetSystem(SYS_SHUTDOWN, 0, 0)`, disable interrupts, jump to the entry point.
8. On any failure: print "cable-twii", the reason ("apps/cable-twii/boot.dol was not found on the SD card or a USB drive.") and "Returning to the Wii Menu.", wait 5 s, then `SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0)`.

No arguments are passed. cable-twii's `main` ignores `argc` and `argv` and finds its data root by scanning the devices (decision 0002).

USB limit, found in the review: the forwarder uses libogc's single-device `__io_usbstorage`, which binds to the first mass-storage device IOS lists and mounts its first FAT partition. The app itself scans every USB device, LUN and partition (`wii/source/storage.c`). With the owner's two drives on USB (games drive and flash drive), the channel may therefore not see `boot.dol` on the flash drive when the games drive is listed first, while the app, once started, still finds its library. Until the two-drive test on the Wii (Task 5 Step 3, and decision 0002's open question) says otherwise, `apps/cable-twii/boot.dol` belongs on the SD card; if that test fails for USB, the follow-up is to give the forwarder the app's device scan as a shared source file.

A channel is not started the way the Homebrew Channel starts an app. IOS loads the DOL's sections and starts the CPU in real mode at physical address 0x3400, and Dolphin's bootstrap does the same. devkitPPC's linker script reserves the `.stub` section there, and nothing in libogc 3.2.1 fills it, so `forwarder/source/stub.S` provides the stub: it clears the BAT registers, maps MEM1 and MEM2 (cached at 0x80000000 and 0x90000000, uncached at 0xC0000000 and 0xD0000000), turns address translation on and jumps to `__app_start`. It becomes text section 0 of the DOL at 0x00003400. Without it the channel shows a black screen, which is what the first Dolphin boot did; `tools/build_wad.py` refuses a DOL whose text sections do not include 0x3400.

Build: the Wii Makefile pattern from `wii/Makefile` (`include $(DEVKITPPC)/wii_rules`) with `LIBS := -lfat -logc -lm` and `LDFLAGS += -Wl,--section-start,.init=0x81230000`. A prototype of exactly this built on 2026-10-22: `forwarder.dol` is 310 KB, the entry point is 0x81230000, the bss ends at 0x812A7280 and `__Arena1Lo` follows it, so the forwarder's heap and stack sit in the free top of MEM1 and never touch the region the app is copied into. The source lives in `forwarder/` (`make -C forwarder`, `APP_PATH` overridable); the header parsing and range check are in `forwarder/source/dol.c` with host tests in `forwarder/tests` (`make -C forwarder/tests test`).

### IOS

The TMD requests IOS58 (`000000010000003A`), the IOS the Homebrew Channel 1.1.x runs apps under; cable-twii's `meta.xml` names no other. SD (`/dev/sdio`) and USB mass storage (libogc's usbstorage over IOS58's USB module) both work there. `meta.xml` declares `<ahb_access/>`, but the app touches no protected hardware (no register access anywhere in `wii/source`), so the TMD's AHB access flag stays off. Check for Task 5: the Homebrew Channel shows the IOS it runs on in its bottom corner; it must read IOS58.

### Title ID

`CTWA`, title ID `0001000143545741`, title type `00010001` (channel), region free. Checked on 2026-10-22:

- WiiBrew's Title database lists no `CTW?` title. Every title there whose system code is `C` is a Commodore 64 Virtual Console title with a digit as the second character (`C9xx`).
- GameTDB answers HTTP 404 for `https://www.gametdb.com/Wii/CTWA` (also for `CTWI` and `CTWV`), and HTTP 200 for known IDs (`RSBE01`, and the Homebrew Channel's `HAXX` and `LULZ`), so the 404 means "no such title".

The channel name in the banner header is "cable-twii".

### WAD packing

libWiiPy and WiiPy (NinjaCheetah, MIT, Python 3.10 or newer) in a venv at `.tooling/wiipy-venv` (ignored by git). Pinned on 2026-10-22: libWiiPy from git `main` at 71ad5b9 (version 1.0.0; the last tagged release is 0.6.0) and WiiPy at 0587517 (1.6.0). WiiPy runs from its checkout with `PYTHONPATH` pointing at it, because its `commands` package is a plain directory; it does not run under `python -I`.

The dry run on the Mac proved:

- A title built from scratch with the libWiiPy API fakesigns and reads back correctly. TMD: IOS58, title ID `CTWA`, type channel, region 3, boot index 1, access rights 0, two content records. Ticket: a random title key encrypted with the retail common key (index 0) and eight empty limit entries. Contents: index 0 is `opening.bnr`, index 1 is the forwarder DOL (the dry run used `cable-twii.dol`). Both body hashes start with `00`; `wiipy info` reports the fields above and "Fakesigned: True". The TMD is 556 bytes and the ticket 676 bytes, as on a real channel.
- `Title.add_content` cannot add the first content to an empty title (libWiiPy 1.0.0 takes `max()` of an empty list). The build script adds content 0 with `ContentRegion.add_enc_content` after `encrypt_content(data, key, 0)`, then the rest with `add_content`.
- libWiiPy embeds no certificate chain, and `dump_wad()` writes a WAD with an empty certificate section without complaint; libWiiPy itself then refuses to load that WAD back ("Unknown certificate in chain!"), and an installer needs the chain to check the title. The build script fetches the retail chain (the public certificates Root-CA00000001, CP00000004 and XS00000003, present in every WAD) once with `libWiiPy.title.nus.download_cert_chain()` and caches it under `.tooling/`.
- A fakesigned WAD installs only through a WAD manager running under an IOS with the signature bug, which the owner's existing manager does. Once installed, the Wii Menu launches it like any channel.

Build: `tools/build-wad.sh` runs `make -C forwarder`, generates the jingle and `opening.bnr` into `dist/forwarder/`, and assembles `dist/cable-twii-forwarder.wad` with `tools/build_wad.py` on libWiiPy (title ID, IOS, both contents, fakesign). The script refuses to run until `.tooling/cert-chain.bin` exists and prints the one-line fetch command for it. `tools/build_wad.py` refuses an empty chain, because libWiiPy would otherwise write a placeholder certificate section.

### Banner

`opening.bnr` is content 0 and nothing produces it yet. Its structure (WiiBrew, Opening.bnr): an IMET header (0x80 bytes of padding, the sizes of the three parts, the channel name in ten languages, an MD5) followed by a U8 archive with `meta/banner.bin`, `meta/icon.bin` and `meta/sound.bin`. `banner.bin` and `icon.bin` are IMD5-wrapped U8 archives (LZ77 optional) holding `arc/blyt/*.brlyt` layouts, `arc/anim/*.brlan` animations and `arc/timg/*.tpl` textures. `sound.bin` is IMD5-wrapped 16-bit PCM WAV (with an optional intro and loop) or BNS.

No maintained command-line tool builds the whole file. libWiiPy packs U8 archives, writes IMD5 headers and compresses LZ77; devkitPro's `gxtexconv` writes TPL textures; ffmpeg is on the Mac for the WAV. The layout and animation files must be written by our tool. benzin converts XML to BRLYT and BRLAN but its current source location and licence are unverified; libWiiSharp (the engine behind CustomizeMii) is GPLv3 .NET, would need mono here, and its banner templates would end up inside our WAD.

Decision, as built in Task 4: `tools/build_banner.py` (standard library plus Pillow for reading the PNGs, no libWiiPy) writes every part itself: RGB565 TPL textures (608 by 456 banner, 128 by 96 icon, both from `forwarder/banner/*.png`), a minimal layout per file (one picture pane, one material with one texture map, a root pane and a root group), one material per layout with a texture map, an identity texture matrix and a texture coordinate generator (the review found the matrix missing; without it the Wii Menu would reuse whatever matrix it loaded last), a single `banner.brlan` that fades the pane in over 30 frames and a single `icon.brlan` that holds it, IMD5 wrappers, uncompressed U8 archives (LZ77 is optional and skipped) and the IMET header with "cable-twii" in all ten languages. The sound is a 2.4 s original four-note jingle from `tools/make_jingle.py`, stored as 16-bit stereo 32 kHz WAV inside `sound.bin`. Unit tests check every structure (`tools/tests/test_build_banner.py`); rendering is verified on Dolphin's Wii Menu in Task 4 Step 3, which needs a Wii Menu in Dolphin (the owner's NAND backup). If the hand-written layout does not render, the fallback is to author the layout with benzin and keep its output in `forwarder/banner/`.

## Open question

Started from the channel, the HOME menu's "Back to Homebrew Channel" goes to the Wii Menu, because no loader stub is present (`quit_route_for`). The app could launch the Homebrew Channel by title ID instead (`WII_LaunchTitle`; HBC's IDs are `OHBC`, `LULZ`, `JODI` or `HAXX` depending on its version) or hide the pill when no stub is present. Not needed for the release.

## Next step

Release Task 4: banner assets, `forwarder/` from the prototype with host tests, `tools/build-banner.py`, `tools/build-wad.sh`, then the Dolphin checks: the channel shows with its banner, icon and sound; it starts cable-twii from the virtual SD card; without `boot.dol` it shows the message and returns to the Wii Menu.
