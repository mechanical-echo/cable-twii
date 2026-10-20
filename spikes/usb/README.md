# Two-drive USB spike

A throwaway Wii homebrew app. It answers two questions: can the Wii reach the FAT32 flash drive that holds the videos while the WBFS games drive is also plugged in, and how fast does it read? Product code never uses it.

## Build and send

```sh
make -C spikes/usb
export WIILOAD=tcp:<wii-ip>
wiiload spikes/usb/boot.dol
```

You need devkitPPC and libogc (see `docs/setup.md`). If `DEVKITPPC` is not set, the Makefile uses `/opt/devkitpro`.

## What it does

1. Reloads IOS58 and starts USB.
2. Lists every USB mass storage device (`USB_GetDeviceList`, class 8) and opens each one with its own handle (`USBStorage_Open`). libogc's stock `__io_usbstorage` can only talk to one device, so this app uses the lower-level calls to see all of them.
3. For each device and LUN it reads sector 0 and the partition table, then the first sector of each partition. It prints the partition type byte, size and a filesystem guess: FAT32, FAT16, NTFS, exFAT, WBFS (magic `WBFS`), GPT, extended or unknown.
4. It mounts only partitions whose boot sector says FAT32 or FAT16, and checks whether `/cable-twii/` exists. WBFS, NTFS, GPT and everything else are never mounted.
5. For each mounted partition that has `/cable-twii/`, it finds the largest file (up to two folders deep) and reads it, looping if it is smaller, until 50 MB are read. It prints MB/s. If `/cable-twii/` is missing it says so and skips.
6. It prints how many write attempts it refused (should be 0).

Buttons: A scans again, HOME exits to the Homebrew Channel. Between scans it closes every device and starts USB again, so changing ports and pressing A may work. Hot-plug is untested. If A does not show the new setup, relaunch the app.

The screen is the plain text console. Photograph it with the final table and the `READ ... MB/s` line in view.

## Run it: four setups

Check each setup, one after the other. Do not move cables while the app is reading.

1. Flash drive alone in port 0.
2. Flash drive alone in port 1.
3. Games drive in port 0, flash drive in port 1.
4. Flash drive in port 0, games drive in port 1.

Afterwards, boot the owner's USB loader and confirm the games drive still lists and launches games as before.

## Results

| Setup | Devices listed | Partitions (type, fs) | `/cable-twii/` found | Read speed (MB/s) | Notes |
|---|---|---|---|---|---|
| 1. flash port 0 | | | | | |
| 2. flash port 1 | | | | | |
| 3. games 0 + flash 1 | | | | | |
| 4. flash 0 + games 1 | | | | | |

Needs at least 1 MB/s (the video needs about 0.2 MB/s). Games drive still works in the USB loader afterwards: yes / no.

## Why nothing is written

- No source line opens a file for writing, and the app calls no write, format, mkdir, rename, delete or sync function. The only file call is `fopen(path, "rb")`.
- Sectors are read with `USBStorage_Read` only. `USBStorage_Write` is never called.
- libfat is the part that could write by itself (FSInfo and dirty flags). To rule that out, libfat never gets libogc's USB interface. It gets a wrapper whose `writeSectors` always returns false without touching the device, and whose feature flags do not claim write support. Every refused write is counted and shown on screen. Any write libfat tried would be refused and visible.
- libfat in libogc 3.2.1 is FatFs-based. It only writes when a write operation happens (write, close of a written file, sync, label change), not on mount or unmount. The app does none of these.
- Non-FAT partitions (WBFS, NTFS, exFAT, GPT, extended) are not mounted at all. They are only read as raw sectors.
- The WBFS games drive is therefore only ever read, with raw sector reads.
