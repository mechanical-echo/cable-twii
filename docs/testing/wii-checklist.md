# cable-twii Wii hardware checklist

Run every item on a real Wii with your real library and both drives plugged in (see `docs/decisions/0002-drive-setup.md`). Fill in **Result** (Pass or Fail) and **Notes** as you go. Note the Wii's region, the TV, and the cable (composite or component) at the top.

- Wii region / system menu version:
- TV and cable:
- Drives (model, size, which partition holds `/cable-twii/`):
- Build (commit hash):
- Date:

## Builds

Release build (the one you install):

```bash
make -C wii DEVKITPRO=/opt/devkitpro DEVKITPPC=/opt/devkitpro/devkitPPC
```

This writes `wii/cable-twii.dol`. To install it, copy it to `sd:/apps/cable-twii/boot.dol` with `wii/meta.xml` and `wii/icon.png` next to it.

Debug overlay build (for the fps, underrun, frame and memory items):

```bash
make -C wii DEVKITPRO=/opt/devkitpro DEVKITPPC=/opt/devkitpro/devkitPPC DEBUG_OVERLAY=1
```

This writes `wii/cable-twii-debug.dol` and builds in `wii/build-debug/`, so it never mixes with the release objects. The release build has none of the overlay code in it.

Scripted-input build (for emulator screenshots without a remote):

```bash
make -C wii DEVKITPRO=/opt/devkitpro DEVKITPPC=/opt/devkitpro/devkitPPC DEBUG_SCRIPT=1
```

This writes `wii/cable-twii-script.dol`, which replays `debug-script.txt` from the data root (next to `config.toml`) on top of the real remote. Each line is `<ms since boot> <command>`: `ir X Y` points at Wii pixel (X, Y), `ir off` hides the pointer, `press BTN` taps a button, `down BTN` and `up BTN` hold and release one; buttons are `A B UP DOWN LEFT RIGHT PLUS MINUS HOME ONE TWO`. Never install this build on the Wii.

To send a build over the network, open the Homebrew Channel on the Wii, note the IP address shown at the bottom right, then on the computer:

```bash
export WIILOAD=tcp:192.168.1.50
wiiload wii/cable-twii-debug.dol
```

(Use your Wii's address. `make -C wii run` also runs `wiiload` on the release `.dol`.)

The overlay sits in the top-left corner and refreshes once per second:

| Line | Meaning |
|---|---|
| `UI 59.9 fps  video 30.0 fps` | Screen refreshes per second and video frames shown per second, both measured against the Wii's real-time clock |
| `audio underruns N` | Times the audio callback ran out of samples while a file was still playing. Adds up across channel changes |
| `frames N decoded  N dropped` | Video frames decoded, and frames skipped because they were late. Both add up across channel changes; a frame or two dropped on each channel change is expected |
| `MEM1 …K  MEM2 …K free` | Free arena memory not yet handed to the heap |
| `heap …K used  …K free` | Memory in use and free inside the malloc heap |

The text cache holds up to 256 rendered strings, and the overlay adds one or two new strings each second, so heap use climbs for the first few minutes until the cache is full. After that it should stay flat.

## Checklist

### Playback and tuning

| # | Item | Steps | Expected | Result | Notes |
|---|---|---|---|---|---|
| 1 | Tune-in mid-episode | Open Studio's Preview for a channel and note what airs now and how far in. Start cable-twii and open that channel in fullscreen. | The same episode plays, and its burned-in timecode matches Studio's offset to within about a second. | | |
| 2 | Flip away and back | On a channel, note the timecode. Flip to another channel, wait 1 minute, flip back. | It lands 1 minute further into the same airing, not back at the start. | | |
| 3 | Episode boundary | Watch a channel through the end of an airing. | The next airing starts within 1 s of its scheduled time. | | |
| 4 | Channel change time | Flip channels with the D-pad 10 times. | Each change shows the new picture in under 1.5 s. | | |
| 5 | Steady playback (debug build) | Watch one channel for 10 minutes. Look closely at motion (pans, scrolling credits) and at the first second after each channel change. | Overlay shows `video` about 30.0 fps and `UI` about 60 fps (50 on a 50 Hz TV); `audio underruns` stays 0; `dropped` does not rise while watching. Motion is smooth with no judder, and frames are not shown early (picture ahead of the sound). Suspected: `pl_draw` may take a queued frame whose time is still in the future; note any judder or picture running ahead. | | |
| 6 | Lip sync | Watch a scene with speech or the sample beep-and-flash file. | Sound and picture line up by eye. | | |
| 7 | Short file | Use an episode whose `.mpg` is shorter than `library.toml` says (cut a copy short). Watch it end. | Black screen until the next airing starts on time. It never loops, freezes on the last frame, or shows an error. | | |
| 8 | Up-next card | Open a timed channel during a gap. | Dark stripes with a faint blue bloom, the channel chip top-right, and a white glossy card with a ringed blue dot, `Up next`, the next episode as `SxxExx · title` (ellipsized if long) next to its icon, the show name under it, `at HH:MM`, a bar that fills toward air time and a large countdown that ticks down. When a commercial break comes before that episode, a faded pill `Up next: Commercial break` with a warm dot sits under the card. | | |
| 9 | Up-next card at the end of the day | Open a timed channel in its last gap of the day (after its last airing). | Known gap: no title, only the countdown. Note what you see. | | |

### Guide, preview and fullscreen

| # | Item | Steps | Expected | Result | Notes |
|---|---|---|---|---|---|
| 10 | Logo | Start the app. | Logo grows in, shine sweeps across it, then it moves into the guide header while the panels slide in. No jump when the guide takes over. A skips. | | |
| 11 | Guide layout | Look at the guide on your TV. | Matches the mockup: channel list left, preview top right, info panel bottom right, clock top right. | | |
| 12 | Hover sweep | Sweep the pointer quickly up and down the channel list. | The highlight follows with hover sound and a short rumble; the preview only retunes after the pointer rests about 300 ms. | | |
| 13 | D-pad in the guide | Press up and down through all channels. | The highlight skips category headers, the list scrolls to keep it visible, and it stops at the first and last channel. | | |
| 14 | Guide frame rate (debug build) | Leave the guide open with the preview playing. | `UI` stays about 60 fps (50 on a 50 Hz TV). | | |
| 15 | Preview to fullscreen and back | Press A on a channel, then B. | The preview expands smoothly to fullscreen in about a third of a second without restarting the video; B shrinks it back the same way. | | |
| 16 | Fullscreen D-pad flipping | In fullscreen press up, right, down and left. | Up and right go to the next channel, down and left to the previous, wrapping around at the ends, with the channel sound. | | |
| 17 | Channel chip | Flip channels in fullscreen. | Top-right white glossy pill holding the number in a blue tile and the 40-tall channel icon (grey rounded placeholder if the channel has none); a blue ring around it fades out over about half a second and the channel name drops in as a pill under it; both stay 2 seconds. | | |
| 18 | OSD show and hide | In fullscreen, point at the screen and move; then hold still; then point away. | Moving shows the info panel sliding up; holding still for 3 s hides it; pointing away hides it at once. The cursor only shows with the panel. | | |
| 19 | OSD panel and volume pills | Point at the screen to open the panel; then press `+` on the remote, point at the volume panel's `+` pill, hold A and release. | The panel is a glass card over a dark scrim with the channel, show, times, bar and an `Up next` column; it has no buttons. The volume panel replaces it for 2 s after `+`; its `+` pill rings blue when pointed, looks pressed while A is held, and the volume rises on release with the select sound. | | |
| 20 | Volume | Press `+` and `−` in fullscreen without pointing; then change channel. | The glossy volume panel with `Volume` above it shows for 2 s, the newest lit segment ringed; the volume moves in steps of 10 and stays the same after a channel change and after restarting the app. | | |
| 21 | Remote not pointed at the screen | Watch fullscreen for 55 minutes with the remote lying on the table, IR not seeing the sensor bar, no buttons pressed. | No OSD, no cursor, no rumble ever appears, and the remote stays connected. The idle timeout is exactly 60 minutes (`WPAD_SetIdleTimeout(3600)`), so a disconnect after more than 60 untouched minutes is by design. | | |
| 22 | Long text | Use a channel, show and episode with 60+ character names and accents. | Every name ellipsizes with `…` inside its panel, row, chip and up-next card; nothing overflows. | | |
| 23 | Text after expand | Expand a channel to fullscreen and return to the guide a dozen times, then scroll the whole list. | No text goes missing or blank anywhere (text cache). | | |
| 24 | Shine edges | Watch the logo shine and any highlighted row closely. | No hard edges, stray lines or one-pixel gaps at the shine's or the rounded corners' edges. | | |

### Remote, HOME and power

| # | Item | Steps | Expected | Result | Notes |
|---|---|---|---|---|---|
| 25 | HOME menu | Press HOME on the guide and in fullscreen. | The overlay dims the screen with the video still playing behind it; HOME again or `Resume` closes it. | | |
| 26 | HOME → Wii Menu | Open HOME and choose `Wii Menu`. | Returns to the Wii Menu. | | |
| 27 | HOME → Homebrew Channel | Open HOME and choose `Homebrew Channel`. | Returns to the Homebrew Channel (not the Wii Menu, not a black screen). | | |
| 28 | Battery bars | Open HOME with a fresh set of batteries, then with nearly empty ones. | The 4 bars reflect the battery level. | | |
| 29 | Power button | While watching, press Power on the console (and once on the remote). | The console turns off cleanly; `state.toml` keeps the last channel. | | |
| 30 | Reset button | While watching, press Reset on the console. | Returns to the Homebrew Channel. | | |
| 31 | Remote disconnect and reconnect | While watching, take the batteries out of the remote, wait, put them back and press a button. | Playback continues without the remote; after reconnecting, buttons work again. | | |
| 32 | Idle timeout | Leave the remote untouched for 30 minutes while watching. | The remote does not disconnect. | | |
| 33 | Idle timeout for remotes that connect later | Start the app with the remote off, connect it after the guide is showing, then leave it untouched for 30 minutes while watching. | It does not disconnect either. | | |

### Drives, files and state

| # | Item | Steps | Expected | Result | Notes |
|---|---|---|---|---|---|
| 34 | No drive | Unplug all USB drives and remove the SD card's `cable-twii` folder, then start the app. | Dialog `No cable-twii drive found` with the body "Plug in the drive prepared with cable-twii Studio, or put a cable-twii folder on the SD card.", and `Back to Homebrew Channel` works. | | |
| 35 | Bad config | Put a typo in `config.toml` (for example a channel number of 1000), then start the app. | Dialog `There's a problem with the schedule` naming the problem (with the line number when known), ending with `Open cable-twii Studio to fix it.` | | |
| 36 | USB 2.0 drive and IOS | Start from the Homebrew Channel with the flash drive in each USB port. | The drive is found in both ports. If it is not found, note the drive and port; this tells us whether the app must reload IOS58 for USB 2.0. | | |
| 37 | USB pulled before detection | Start the app and pull the drive out while the screen is still black or the logo is showing. | The app does not stall or freeze the picture; it shows the no-drive dialog (or the SD card's folder if there is one). | | |
| 38 | Drive unplugged while watching | Pull the drive out while a channel plays in fullscreen. | Dialog `The drive was disconnected` / `Plug it back in and start cable-twii again.`; no crash and no frozen frame. | | |
| 39 | `state.toml` only on the cable-twii partition | With both drives plugged in (the games drive with `/wbfs` or `/games`, and the cable-twii drive), change channel and quit. Check both drives on a computer. | `state.toml` was written only inside `/cable-twii/` on the cable-twii partition. Nothing new on the games drive. | | |
| 40 | `state.toml` never on a games drive | Copy a `cable-twii` folder onto the drive that has `/wbfs` or `/games`, unplug the other drive, start the app, change channel and quit. | The app does not write `state.toml` there. | | |
| 41 | Remembered channel and volume | Change channel and volume, quit through HOME, start again. | The guide opens on the last channel with the same volume. | | |
| 42 | Quit returns to the Homebrew Channel | Quit with `Back to Homebrew Channel` on a dialog, and with HOME → `Homebrew Channel`. | Both land in the Homebrew Channel. | | |

### Clock

| # | Item | Steps | Expected | Result | Notes |
|---|---|---|---|---|---|
| 43 | Midnight on a timed channel | Set the Wii clock to 23:58 and watch a timed channel whose airing spans midnight. | The airing carries on through midnight without a hiccup; the guide's times and the up-next card are right after 00:00. | | |
| 44 | Midnight on a looping channel | Set the Wii clock to 23:58 and watch a looping channel through midnight. | The lineup rolls over to the new day without a visible hiccup. | | |

### TV settings

| # | Item | Steps | Expected | Result | Notes |
|---|---|---|---|---|---|
| 45 | 4:3 TV setting | Set the Wii to 4:3. Watch a 4:3 and a 16:9 file. | 4:3 fills the width; 16:9 is letterboxed. Guide and chip look right. | | |
| 46 | 16:9 TV setting | Set the Wii to 16:9. Watch a 4:3 and a 16:9 file. | 4:3 is pillarboxed; 16:9 fills the screen (a few black pixels at the edges are a known gap). Corners and cursor are not stretched. | | |
| 47 | 16:9 text | On 16:9, look at guide text, the clock and the chip number. | Letters have the same shape as on 4:3 (text is narrowed with the pixel width); nothing looks squashed or stretched, and long names still ellipsize. | | |
| 48 | 60 Hz | Set the Wii to 60 Hz (EURGB60 on a PAL Wii, or any NTSC Wii). | The picture fills the screen top to bottom; debug overlay shows `UI` about 60 fps. | | |
| 49 | 50 Hz | On a PAL Wii, set 50 Hz. | The picture is centred with thin, even bands at the top and bottom (about 24 lines each on a 576-line TV), nothing is cut off, and the picture is only slightly squashed vertically: rounded corners and the cursor still look round. Debug overlay shows `UI` about 50 fps; video still plays smoothly with 0 underruns. If the bands bother you, note it (a full-height mode is a possible follow-up). | | |

### Redesign

| # | Item | Steps | Expected | Result | Notes |
|---|---|---|---|---|---|
| 51 | System bar | In the guide, point at the HOME disc, then at `◀ CH` and `CH ▶`; hold A on each and release. | The curved bar carries the clock and date; pointed parts ring blue with the hover sound and rumble; a pill looks pressed while A is held; on release `◀ CH` moves the highlight up, `CH ▶` down, and the HOME disc opens the HOME menu, each with the select sound. | | |
| 52 | Pressed pills | In the HOME menu and the error dialog, hold A on a pill, drag off it and release; then press and release on it. | Dragging off never fires; releasing on the pill fires once. | | |
| 53 | Empty library | Use a `config.toml` with no channels. | Header and system bar without channel pills, a card with a dashed placeholder, `No channels yet`, the Studio hint and `Back to Homebrew Channel`, which quits to the Homebrew Channel. | | |
| 54 | Intro | Start the app. | A glossy panel with a blue ring fades and scales in with the wordmark, a diagonal shine crosses it, three dots pulse under it, then the wordmark shrinks into the header while the panel fades and the guide slides into place; A skips it. | | |
| 55 | HOME menu | Press HOME while watching. | Glossy top and bottom bars over the dimmed video: a blue HOME badge, `HOME Menu`, what is playing on the right, three pills (`Wii Menu`, `Back to Homebrew Channel`, `Resume`), and `Volume` with the segments and the remote battery in the bottom bar. | | |

### Soak

| # | Item | Steps | Expected | Result | Notes |
|---|---|---|---|---|---|
| 50 | 4-hour soak (debug build) | Note the overlay's memory lines after 10 minutes. Leave a channel playing for 4 hours, flipping channels now and then, then read them again. | `heap … used` and `MEM1`/`MEM2 free` are about the same as at 10 minutes (no steady climb or fall); `audio underruns` is still 0 or close to it; no crash or freeze. | | |

### Server source

Run these with the server running on your PC (`docs/SERVER.md`) and a server-profile library on it. Items 57, 58, 60, 61, 63 and 66 need the real Wii at home. Items 56, 59, 62 and 65 can also be tried in Dolphin once you approve a session, except the HOME-while-`Connecting…` parts of 56 and 62, which need the real Wii.

| # | Item | Steps | Expected | Result | Notes |
|---|---|---|---|---|---|
| 56 | Server boot | In Settings pick Server, type the PC's address, Test connection, Save. Restart cable-twii. Then press HOME → `Back to Homebrew Channel` while `Connecting to <address>…` is showing. On the real Wii, restart and try HOME → `Wii Menu` while it shows, then once more with the Power button. | Test connection says `Connected to "<name>".`. After the restart the logo plays and holds with `Connecting to <address>…`, then the guide slides in with the server's channels and the last channel and volume. HOME works while connecting and goes to the Homebrew Channel. `Wii Menu` goes to the Wii Menu and Power turns the Wii off, without hanging. | | |
| 57 | Tune-in time from the server | Open a channel in fullscreen from the guide, 10 times, on different channels. Use the debug overlay build. Then move the pointer up and down the guide so the hover preview tunes several channels in a row. | Each channel shows its first picture within about 3 s (the tune-in target in decision 0004). Note the typical time and the live download rate. Note any hitch of the pointer or the list while a hover preview tunes. | | |
| 58 | 30-minute stream | Watch one channel for 30 minutes with the debug overlay build. | No `Weak signal`, no stall, 0 or close to 0 `audio underruns`, and the download rate stays above the video's rate (about 130 KB/s). Sound and picture stay in sync. | | |
| 59 | Server stopped or wrong address | (a) Stop the server and start cable-twii. (b) Start the server again, change the address in Settings to a wrong IP that nothing answers on (for example one digit off), Save. Do both once with the drive plugged in and once with no drive and no `cable-twii` folder on the SD card. (c) With no drive, on `Can't reach the server`, start the server, press the Settings pill and Save without changing anything. With the drive, while it plays from USB, start the server, then HOME → `Settings` → Save. | With a drive: after the retries (up to about 30 s for getting online, then the server tries) the guide opens from the drive with `Server not reachable, playing from USB` (or `... from the SD card`). With no drive: the error `Can't reach the server` with the address in its text and a Settings pill beside `Back to Homebrew Channel`. A wrong IP behaves like a stopped server, and the Wii does not freeze while waiting. The logo's dots may pause for a moment while the USB drive is scanned on fallback. (c) Save restarts as at start-up and the server's guide opens, in both cases. | | |
| 60 | Wi-Fi off for 20 s | While a channel plays from the server, turn the router's Wi-Fi off for 20 s, then on. While `Weak signal` shows, flip channels up and down and press B. | After about 1 s of no data `Weak signal` shows over the last frame and the picture holds. When the Wi-Fi is back, `Weak signal` goes and the channel continues at the live point, not where it stopped. Flips and B respond at once while `Weak signal` shows (no freeze of a few seconds). No crash. | | |
| 61 | Server restarted mid-show | While a channel plays, stop the server for 30 s, then start it. | The frame holds with `Weak signal`. Within a few seconds of the server answering again (it is checked every 3 s) the channel tunes back to live by itself. | | |
| 62 | Settings from scratch with no drive | Remove the USB drive and any `cable-twii` folder from the SD card. Delete `settings.toml`. Start cable-twii. Press the Settings pill on the empty or error screen, then type the address with the on-screen keyboard (try pointer + A, then D-pad + A, B to close), Test connection and Save. Also press HOME → `Back to Homebrew Channel` while `Connecting to <address>…` shows, and on the real Wii also HOME → `Wii Menu` and the Power button (one per start). | Settings opens from the pill. The keyboard types and `http://` shows only on an empty box. Save with an empty address is refused and opens the keyboard. After Save the Wii restarts as at start-up and the server's guide opens. `settings.toml` is on the SD card. Back to the Homebrew Channel, the Wii Menu and Power off work while connecting. | | |
| 63 | Test connection with the firewall blocking | On the PC, turn off the rule that lets port 4747 through (`docs/SERVER.md` section 3.5). On the Wii press Test connection with the right address. Then try a wrong IP, a PC name that does not exist, and the address of something that is not a cable-twii server (the router's web page). Then put the rule back. | Blocked firewall and wrong IP: `No answer from <address>. Check that the server is running, the address is right and the PC's firewall allows it.` Not a cable-twii server: `<address> answered, but it isn't a cable-twii server.` With the rule back: `Connected to "<name>".`. Studio with a broken `config.toml`: `The server's channels have a problem. Open cable-twii Studio on the server to fix it.` The Wii never freezes; the result comes back in about 10 s or less. | | |
| 64 | No SD card, or a card with `wbfs/` | (a) Start without an SD card (drive only) and change settings. (b) Put in an SD card that has a `wbfs/` or `games/` folder, change settings and play from the server. Check the card on a computer. | The Settings screen shows `No SD card. These settings last until cable-twii closes.` and the settings work until the app closes. Nothing is written to the card: no `settings.toml`, no `state.toml`, no `.tmp` files. Next start uses the drive as before. | | |
| 65 | Reload channels and the revision check | In Studio on the PC change a channel (rename it). Then: (a) open the guide again and wait, (b) HOME → `Reload channels`, (c) stop the server and press `Reload channels` again, (d) in drive mode open HOME, (e) start the server with a broken `config.toml` and press `Reload channels`. | The app picks up the change: (a) when the guide opens, or after midnight, it reloads quietly and shows `Channels updated`. (b) shows `Connecting to <address>…`, then `Channels updated`, and the new name. (c) shows `Can't reach the server` and keeps the current channels, still playing. (d) has no `Reload channels` pill. (e) shows `The server's channels have a problem. Open cable-twii Studio on the server to fix it.` and keeps the current channels. A failed automatic check changes nothing. | | |
| 66 | 5,000-episode library | Run `python3 tools/make-big-library.py <folder>` on an empty folder (it refuses to overwrite an existing `cable-twii/library.toml` or `config.toml` unless you add `--force`), serve that folder, point the Wii at it with the debug overlay build and start. Open the guide, flip channels, open HOME → `Reload channels`. | The library loads within the timeouts (no `Can't reach the server`), and the guide and channel flips stay smooth. Write down the overlay's `net up N ms  load N ms` and `MEM1`/`MEM2 free` in Notes. Videos are missing, so playback itself is not tested here. | | |

### Picture setting

| # | Item | Steps | Expected | Result | Notes |
|---|---|---|---|---|---|
| 67 | Picture switch | (a) Wii at 4:3 and 50 Hz: watch a 16:9 show full screen, press HOME and pick `14:9`, then `Fill`, then `Fit` in the bottom bar. (b) Pick `Fill`, quit, start again. (c) Open Settings, press HOME over it, pick `14:9`, `Resume`, then Save; repeat with Cancel. (d) With no SD card, or a card with a `wbfs/` folder, pick `Fill`. (e) Watch a 4:3 show on `Fill`. (f) On `Fill`, zoom from the guide preview to full screen and back. | (a) The picture behind the dim changes at once: thin bars with a little cut off the sides, then the full screen with 1/8 cut off each side, then the letterbox as before. Nothing is stretched; the pointed part rings blue with the hover sound. (b) It is still `Fill`, and `settings.toml` has `picture = "fill"`. (c) `14:9` stays after both. (d) It works until cable-twii closes; nothing is written to the card. (e) Unchanged. (f) The preview shows the whole picture, and the cut grows smoothly during the zoom with no jump at either end. | | |
