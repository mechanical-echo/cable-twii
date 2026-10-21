# Wii cross-check

This check shows that the Wii (big-endian PowerPC) computes the same lineups as the Mac (little-endian). `engine/wii-check/` is a small Wii program built from the engine sources in `engine/src` and `engine/vendor`. It loads `sd:/cable-twii/` and prints one line for each date and channel:

```
HASH 2026-10-20 1 3d4042a108c37619
...
DONE
```

The dates are 2026-10-20, 2026-10-21, 2026-12-31 and 2027-01-01, in that order, with every channel inside each date. If the load reports problems, the program prints `ERROR ...` (and `WARNING ...`) lines. Each line goes to the screen and to Dolphin's log (OSReport).

`engine/tests/wii_check_expected.txt` holds the 8 lines the Mac CLI makes for `engine/tests/fixtures/root_basic`.

## Run it in Dolphin

```sh
make -C engine wii-check
```

You need devkitPPC, libogc, libfat and Dolphin at `/Applications/Dolphin.app` (see `docs/setup.md`). The script `engine/tests/wii_check.sh`:

1. builds the CLI and the `AUTOTEST=1` build, `engine/wii-check/wii-check-autotest.dol`. It fails if the build prints a warning for any file in `engine/src` or `engine/wii-check`
2. makes the expected lines with the CLI and checks that they still equal `wii_check_expected.txt`, which must not be empty
3. makes a new temporary Dolphin user folder (`-u`) and a temporary SD folder that holds `cable-twii/` copied from `root_basic`
4. turns on the SD card and "Automatically Sync with Folder" for that folder, and turns on file logging of OSReport, all in the temporary user folder
5. runs `Dolphin -b -v Null -e wii-check-autotest.dol` and waits for `DONE` in the log (60 s at most; set `WII_CHECK_TIMEOUT` to change it)
6. checks that the log has exactly 8 `HASH` lines, compares them with the expected lines and exits 0 only when they are the same

It never uses your own Dolphin user folder. When it ends, it deletes the temporary folders and the autotest build (`build-autotest/` and `wii-check-autotest.*`). Set `DOLPHIN` to use a different Dolphin binary.

The `AUTOTEST` build turns the console off right after `DONE`, so Dolphin stops by itself. If `wii_check_expected.txt` is out of date, the script prints the difference and fails. Update the file only when an engine change is meant to change lineups.

## Run it on a real Wii

```sh
make -C engine/wii-check clean
make -C engine/wii-check
```

Copy `engine/tests/fixtures/root_basic` to the SD card as `cable-twii/`, then send `engine/wii-check/boot.dol` with `wiiload` or start it from the Homebrew Channel. Compare the lines on screen with `wii_check_expected.txt`. This build waits for HOME (Wii Remote) or START (GameCube pad), then goes back to the loader.

The normal build (`boot.dol`, in `build/`) and the `AUTOTEST=1` build (`wii-check-autotest.dol`, in `build-autotest/`) use separate output files, so the check never replaces `boot.dol`.
