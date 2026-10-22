# 0003: Forwarder channel

Status: **Accepted** for the build. The Dolphin check (release Task 4) and the real-Wii install (Task 5) can still send it back here.

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
- `__exception_closeall` is not needed on libogc 3.2.1: `SYS_ResetSystem(SYS_SHUTDOWN, 0, 0)` stops the DSP, the interrupts and the thread system and clears the exception table at 0x80003000 itself (libogc 3.2.1 `system.c`).
- `cable-twii.dol` (9.3 MB) occupies 0x80003F00 to 0x809A7020 (text, data and bss), so everything from 0x81230000 up is free for a forwarder, with about 8.5 MB of headroom.

## Decision

### Forwarder

Our own source in `forwarder/` (release Task 4), written for libogc 3.2.1 after the EasyRPG design. No code is copied from that repository, which has no licence. The program is about 150 lines of C:

1. Start video with a text console. The screen stays black unless something goes wrong.
2. Mount `sd` (`__io_wiisd`) and look for `sd:/apps/cable-twii/boot.dol`. If it is not there, mount `usb` (`__io_usbstorage`, retrying for up to 10 s because drives take time to answer) and look for `usb:/apps/cable-twii/boot.dol`. The path is a build-time macro.
3. Read the file to 0x92000000 in MEM2. The limit is 0x933E0000, so a DOL may be up to about 19 MB.
4. Check the DOL header: every text, data and bss range must lie inside 0x80003400 to 0x81230000 and inside the file. Otherwise show a message.
5. Unmount, shut both devices down, and clear the Homebrew Channel stub magic at 0x80001804. The app looks for `STUBHAXX` there (`wii/source/quit.c`) and must not try to return to a loader that is not in memory.
6. Copy the sections into place, zero the bss, flush the data cache and invalidate the instruction cache.
7. `SYS_ResetSystem(SYS_SHUTDOWN, 0, 0)`, disable interrupts, jump to the entry point.
8. On any failure: print "cable-twii", the reason ("apps/cable-twii/boot.dol was not found on the SD card or a USB drive.") and "Returning to the Wii Menu.", wait 5 s, then `SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0)`.

No arguments are passed. cable-twii's `main` ignores `argc` and `argv` and finds its data root by scanning the devices (decision 0002).

Build: the Wii Makefile pattern from `wii/Makefile` (`include $(DEVKITPPC)/wii_rules`) with `LIBS := -lfat -logc -lm` and `LDFLAGS += -Wl,--section-start,.init=0x81230000`. A prototype of exactly this built on 2026-10-22: `forwarder.dol` is 310 KB, the entry point is 0x81230000, the bss ends at 0x812A7280 and `__Arena1Lo` follows it, so the forwarder's heap and stack sit in the free top of MEM1 and never touch the region the app is copied into. The prototype is kept at `.tooling/forwarder-proto/` (not committed); Task 4 moves it to `forwarder/` and adds host tests for the header check.

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

Build (Task 4): `tools/build-wad.sh` runs `make -C forwarder`, builds `opening.bnr` from `forwarder/banner/`, and assembles `dist/cable-twii-forwarder.wad` with a Python step on libWiiPy (title ID, IOS, both contents, fakesign).

### Banner

`opening.bnr` is content 0 and nothing produces it yet. Its structure (WiiBrew, Opening.bnr): an IMET header (0x80 bytes of padding, the sizes of the three parts, the channel name in ten languages, an MD5) followed by a U8 archive with `meta/banner.bin`, `meta/icon.bin` and `meta/sound.bin`. `banner.bin` and `icon.bin` are IMD5-wrapped U8 archives (LZ77 optional) holding `arc/blyt/*.brlyt` layouts, `arc/anim/*.brlan` animations and `arc/timg/*.tpl` textures. `sound.bin` is IMD5-wrapped 16-bit PCM WAV (with an optional intro and loop) or BNS.

No maintained command-line tool builds the whole file. libWiiPy packs U8 archives, writes IMD5 headers and compresses LZ77; devkitPro's `gxtexconv` writes TPL textures; ffmpeg is on the Mac for the WAV. The layout and animation files must be written by our tool. benzin converts XML to BRLYT and BRLAN but its current source location and licence are unverified; libWiiSharp (the engine behind CustomizeMii) is GPLv3 .NET, would need mono here, and its banner templates would end up inside our WAD.

Decision: Task 4 writes `tools/build-banner.py` on libWiiPy with a minimal static layout (one picture pane with the 608 by 456 banner texture, the 128 by 96 icon the same way), a minimal animation file, TPL through `gxtexconv` and the sound through ffmpeg, and verifies it on Dolphin's Wii Menu (Task 4 Step 3). If the hand-written layout does not render, the fallback is to author the layout with benzin and keep its output in `forwarder/banner/`.

## Open question

Started from the channel, the HOME menu's "Back to Homebrew Channel" goes to the Wii Menu, because no loader stub is present (`quit_route_for`). The app could launch the Homebrew Channel by title ID instead (`WII_LaunchTitle`; HBC's IDs are `OHBC`, `LULZ`, `JODI` or `HAXX` depending on its version) or hide the pill when no stub is present. Not needed for the release.

## Next step

Release Task 4: banner assets, `forwarder/` from the prototype with host tests, `tools/build-banner.py`, `tools/build-wad.sh`, then the Dolphin checks: the channel shows with its banner, icon and sound; it starts cable-twii from the virtual SD card; without `boot.dol` it shows the message and returns to the Wii Menu.
