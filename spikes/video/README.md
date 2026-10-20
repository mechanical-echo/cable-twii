# Video playback spike

A throwaway Wii homebrew app. It checks whether the real Wii can decode our MPEG-1 + MP2 files fast enough, and how fast and how accurately it can seek. Its numbers decide the final video resolution and bitrate. Product code never uses it.

It plays a `.mpg` file with [pl_mpeg](https://github.com/phoboslab/pl_mpeg) (vendored in `source/pl_mpeg.h`, MIT). It shows the video at the correct aspect for the Wii's 4:3/16:9 setting, plays the MP2 audio through ASND at 48 kHz, and shows its stats on screen. It also writes the stats to a log file.

## Build

```sh
make -C spikes/video
```

This makes `spikes/video/boot.dol`. You need devkitPPC and libogc (see `docs/setup.md`). If `DEVKITPPC` is not set, the Makefile uses `/opt/devkitpro`.

Build options (run `make -C spikes/video clean` first, because changing an option alone does not rebuild anything):

- `make -C spikes/video SEEK_TARGET=20` changes where button 1 seeks to. The default is 300 s. Use a lower value with short files, such as the 30 s samples.
- `make -C spikes/video AUTOTEST=1` makes a build that plays without any input, which is useful in Dolphin. It skips the file list and plays `test.mpg` (or the first file found). At 6 s it presses 1 (seek), at 12 s it presses 2 (switches to the CPU path), at 18 s it presses − (seek to 0) and at 24 s it presses HOME. Don't use this build for the real measurements.

## Make the test files

You need one real episode that is at least 6 minutes long:

```sh
spikes/video/encode-tests.sh "<episode-file>" <out-dir>
```

This writes four files to `<out-dir>`:

| File | Video | Bitrate |
|---|---|---|
| `640x360.mpg` | 640×360 | 1500k |
| `480x360.mpg` | 480×360 | 1500k |
| `512x288.mpg` | 512×288 | 1200k |
| `384x216.mpg` | 384×216 | 900k |

All four use MPEG-PS, mpeg1video at 30 fps, `-g 15`, and MP2 audio at 48 kHz stereo 192k with `loudnorm=I=-16:TP=-1.5:LRA=11`. The episode is scaled to fit the frame without changing its aspect, and any space left over is filled with black bars (square pixels).

This ffmpeg has no `drawtext` filter, so the script makes the timecode from ffmpeg's `testsrc2` source instead. It crops the timer out of `testsrc2` and places it as a small box in the bottom-right corner. The box shows `HH:MM:SS.mmm` and the frame number, and both count the episode's own time. Read this timer to measure how far off a seek landed.

## Copy to the Wii

Make a folder named `cable-twii-spike` at the root of the SD card or the USB flash drive, and copy the four `.mpg` files into it. Copying one of them as `test.mpg` as well is optional; the app puts the cursor on `test.mpg` when it finds one (it checks the SD card first, then USB).

```
sd:/cable-twii-spike/640x360.mpg
sd:/cable-twii-spike/480x360.mpg
sd:/cable-twii-spike/512x288.mpg
sd:/cable-twii-spike/384x216.mpg
```

The app lists every `.mpg` file in `sd:/cable-twii-spike/` and `usb:/cable-twii-spike/`. It skips names that start with `._` (files macOS adds when it copies).

## Run on the Wii

Start the Homebrew Channel on the Wii, then send the app over the network:

```sh
export WIILOAD=tcp:<wii-ip>
wiiload spikes/video/boot.dol
```

`make -C spikes/video run` does the same thing. You can also copy `boot.dol` to `sd:/apps/cable-twii-spike/boot.dol` and start it from the Homebrew Channel.

## Controls

| Where | Wii Remote | GameCube pad | Action |
|---|---|---|---|
| List | D-pad up/down | D-pad up/down | Choose a file |
| List | A | A | Play the file |
| List / playing | 2 | Y | Switch between the TEV and CPU YUV→RGB paths |
| Playing | 1 | X | Seek to 300 s (`SEEK_TARGET`) |
| Playing | − | L | Seek to 0 s |
| Playing | + | R | Exact seek on/off (off: jump to the keyframe before the target, which is what `plm_seek` does by default) |
| Playing | A | A | Hide or show the stats |
| Playing | B | B | Back to the list |
| Anywhere | HOME | START | Exit to the Homebrew Channel |

Switching the path with 2 resets the stats, so each path gets its own numbers. The top line of the stats shows which path is active (`TEV` or `CPU`).

## What the stats mean

```
TEV 640x360 60Hz 16:9      path, video size, TV refresh, Wii aspect setting
fps 30.0 avg 30.00         frames shown in the last second / average since start or the last reset
dec 7.0 max 28.5 ms        MPEG-1 decode time per frame: average / worst
cnv 0.4 max 0.4 ms         time to upload a frame: tiling Y/Cb/Cr (TEV) or YUV→RGB565 (CPU)
underrun 0 drop 0          audio buffers that ran empty / video frames skipped because they were late
seek 27 show 67 ms         plm_seek time / time from pressing 1 until the new frame is on screen
to 300.00 got 299.767      seek target / PTS of the first frame after the seek
t 123.4/360 exact off      playback clock / file length, exact seek setting
```

- **Average fps** does not count time spent seeking or after the file ends.
- **Seek time to 300 s** is `seek` (the `plm_seek` call). `show` adds the time until the new frame is on screen.
- **Timecode error after seek** is the burned-in timer you see right after the seek minus 300.000. With exact seek off it should be under 0.5 s, because there is a keyframe every 15 frames. The `got` value shows the same thing in numbers.

Every line is also written to `cable-twii-spike/spike-log.txt` on the same device (the SD card if it is mounted, otherwise USB). A `stats` line is written every 10 s of playback, a `seek` line after every seek, and an `end`/`stop` summary when a file ends or you press B. The same lines go to Dolphin's log (OSReport). The list screen shows the last summary.

## Dolphin

Dolphin only shows that the app works; ignore its fps. Point its SD card at a folder that holds `cable-twii-spike/`: in Config → Wii → SD Card, turn on "Enable SD Card" and "Automatically Sync with Folder". Then start `boot.dol`. Set Graphics → Advanced → Texture Cache Accuracy to **Safe**. Otherwise Dolphin keeps stale copies of the video textures, because the app rewrites them in place every frame, and the picture and stats come out garbled. The real Wii does not have this problem.

## Results

Play each file for 2 minutes and press 1 once during playback. Do this with the Wii set to 60 Hz, then again at 50 Hz (Wii Settings → Screen → TV Resolution / EURGB60). Targets: average fps 30.0, underruns 0, seek time under 1.5 s, timecode error under 0.5 s.

| File | Refresh | Path | Average fps | Worst decode ms | Underruns | Seek time to 300 s | Timecode error after seek |
|---|---|---|---|---|---|---|---|
| 640x360 | 60 Hz | TEV | | | | | |
| 480x360 | 60 Hz | TEV | | | | | |
| 512x288 | 60 Hz | TEV | | | | | |
| 384x216 | 60 Hz | TEV | | | | | |
| 640x360 | 50 Hz | TEV | | | | | |
| 480x360 | 50 Hz | TEV | | | | | |
| 512x288 | 50 Hz | TEV | | | | | |
| 384x216 | 50 Hz | TEV | | | | | |

If 640×360 misses 30 fps, press 2 and measure it once more on the CPU path:

| File | Refresh | Path | Average fps | Worst decode ms | Underruns | Seek time to 300 s | Timecode error after seek |
|---|---|---|---|---|---|---|---|
| 640x360 | 60 Hz | CPU | | | | | |

Then pick the largest resolution that holds 30 fps with zero underruns. If nothing above 384×216 does, add "evaluate libavcodec MPEG-1 decoder" to Task 4 as a follow-up.
