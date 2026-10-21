# 0002: Drive setup

Status: **Provisional**. To be revised after the two-drive USB test (`spikes/usb`, Task 3) has been run on the real Wii.

## Context

The owner's 120 GB games drive holds WBFS files for a USB loader. cable-twii and Studio must never write to, format or repartition it (spec section 2).

The plan was to keep the cable-twii media on a separate flash drive in the Wii's other USB port. Homebrew USB storage support has traditionally talked to one device, often the first one found, so it is not known whether cable-twii can reach the flash drive while the games drive is plugged in.

The two-drive test has not been run yet, because the owner's flash drive is not ready.

## Decision (for now)

- The **SD card is the working data location** for development and early use. The data root is `sd:/cable-twii/`.
- The app's search order stays as designed: every mountable USB FAT partition that has a `/cable-twii/` folder, then `sd:/cable-twii/`. Nothing in the app changes because of this decision.
- A drive without a `/cable-twii/` folder, including the games drive, is never written to.
- The spec (section 2, Storage) is not changed yet. It still describes the flash drive as the owner's plan.

## Open question

Can cable-twii reach the flash drive while the WBFS games drive is plugged into the other USB port, and can it tell the two apart?

## Fallbacks (from spec section 2, in order)

1. Pick the device that has a `/cable-twii/` folder on a FAT partition and ignore the rest.
2. Use an SD or SDHC card in the front slot (up to 32 GB, roughly 40 hours of video). It is always supported.
3. Swap drives when switching between games and TV.

## Test results

To be filled in after running `spikes/usb` on the real Wii. A setup passes when the flash drive is listed, `/cable-twii/` is found on it, and the read speed is at least 1 MB/s (video needs about 0.2 MB/s).

| Setup | Devices listed | `/cable-twii/` found | Read speed (MB/s) | Result |
|---|---|---|---|---|
| Flash drive alone in port 0 | | | | |
| Flash drive alone in port 1 | | | | |
| Games drive port 0, flash drive port 1 | | | | |
| Flash drive port 0, games drive port 1 | | | | |

After the runs: does the games drive still work in the owner's USB loader? Not yet checked.

## Next step

Run the test, fill in the table, then change this record to Accepted with the final choice (two drives with which port, SD card, or swapping) and update spec section 2 to match.
