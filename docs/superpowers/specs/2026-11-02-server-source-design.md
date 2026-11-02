# cable-twii — Server source (design)

Date: 2026-11-02
Builds on: [cable-twii design](2026-10-20-cable-twii-design.md), [file formats](../../formats.md)

Adds a **media server on the home network** as a second place the Wii app can get its channels and videos from. The USB drive / SD card stays exactly as it is; the server is an alternative the owner picks.

Decisions marked **(my call)** were made by the designer, not the owner, and may be revisited. Everything else was stated or confirmed by the owner.

---

## 1. Intent

The owner wants more storage than their USB drives allow, wants to test without plugging drives in and out and waiting on long copies, and wants another person to be able to add and edit content easily. A server that holds the schedule and videos, and that the Wii streams from, covers all three.

This narrows one non-goal of the original design ("no network sources"): the Wii may now read from **a server the owner runs on their own home network**. Internet streaming services stay out.

The server will not make playback faster than USB; the owner accepts that trade for the larger library.

### Success looks like

- On the Wii, the owner enters the server's address, picks "Server", and the guide shows the server's channels. Tuning in joins mid-episode exactly as it does from USB.
- Someone at home opens Studio in a browser pointed at the server, uploads a video, adds it to a schedule, and the Wii shows it after a reload, without anyone touching a drive.
- With "Drive" picked, the app behaves exactly as it does today.
- If the server can't be reached, the Wii falls back to the drive and says so.

### Goals

- A server that holds the same library a drive holds today (`config.toml`, `library.toml`, show info, icons, converted videos) and serves it to the Wii.
- Studio runs on the server; anyone at home edits from a browser, with nothing to install.
- Videos are uploaded as originals and converted on the server **(my call)**.
- The Wii can set the server address and source both in a settings file on the SD card and **on the Wii itself** (owner's request).
- Fallback to the drive when the server is unreachable.

### Non-goals

- Access from outside the home network, and HTTPS **(my call: the Wii's TLS support is poor and the network is trusted)**.
- Per-person ownership of content: everyone with the password can edit everything (owner confirmed).
- One guide mixing channels from the drive and the server. The Wii uses one source at a time (owner confirmed).
- Caching or downloading videos onto the Wii's drive, and syncing the server's library to a USB drive (possible later).
- Automatic discovery of the server on the network **(my call)**.
- The Wii writing anything to the server.

### Anti-goals (a failure even if it technically works)

- Drive mode changes behaviour or gets worse.
- Server mode stutters, or tuning in is so slow it stops feeling like TV.
- Studio's preview disagrees with what the Wii airs in server mode.
- The server exposes files outside the library, or lets anyone without the password change anything.
- Two people's edits silently overwrite each other.

---

## 2. Constraints

### Server hardware

- **Development and testing now:** the owner's Mac (at the office, so with Dolphin; the real Wii is at home).
- **Server at home:** the owner's own Windows 11 PC.
- **Later:** a Raspberry Pi with an SSD (Linux, 64-bit ARM), not ready yet.
- Server mode supports **macOS, Windows and Linux**. Studio already runs on macOS and Windows (Node.js 22 + the native `cable-twii-sched` CLI built with `make`). Linux is new but uses the same tools.
- Converting video on a Pi will be slower than on the Mac; it runs in the background queue Studio already has.

### Wii network

- The Wii joins the network with its own **Internet settings** from Wii Settings; the app does not configure Wi-Fi. Built-in Wi-Fi is 802.11b/g. A USB Ethernet adapter is an option.
- Homebrew apps report roughly **0.3–1 MB/s** over Wi-Fi (community reports, not measured here). A converted video needs about **0.23 MB/s** (1.5 Mbit/s video + 192 kbit/s audio). Steady playback should fit with little margin; tuning in (connect, seek, fill a buffer) will be slower than from USB.
- 88 MB of RAM in total, so stream buffers must stay small (a few seconds of video).
- The Wii has a TOML parser (tomlc99) and no JSON parser. zlib is already linked into the app.

### Library size

- Same target as the original design **(my call)**: 50 channels and 5,000 episodes. At half-hour episodes and 0.8 GB per hour that is about 2,500 hours, roughly 2 TB.
- At that size `library.toml` is about 2 MB (300–400 bytes per episode). The server sends it gzip-compressed and the Wii unpacks it with zlib **(my call)**, so it downloads in a second or two. Memory use and load time at full size are measured on the Wii in Phase 2.

### Library and formats

- The library on the server has **exactly the drive layout** (the contents of `/cable-twii/`). The same converted `.mpg` files, the same `config.toml` and `library.toml`.
- `docs/formats.md` is frozen: existing formats do not change. New files and the server's HTTP interface are added to it.
- The engine already parses from text (`ctw_config_parse`, `ctw_library_parse`), so the Wii can parse downloaded files without writing them to disk.
- `pl_mpeg` can read through callbacks (`plm_buffer_create_with_callbacks`: load, seek, tell), so the player can read from the network instead of a file.

---

## 3. Overview

```
 Server (Mac, home PC, later Pi)                 Wii
 ┌───────────────────────────────┐  home network ┌─────────────────────────────┐
 │ Studio, server mode           │  (plain HTTP) │ cable-twii                  │
 │  - library folder (drive      ├──────────────►│  - source: Drive or Server  │
 │    layout) on the server disk │  read-only    │  - downloads config+library │
 │  - web UI, password to edit   │  Wii API      │  - same engine, same lineup │
 │  - uploads, ffmpeg conversion │               │  - streams video by ranges  │
 └───────────────▲───────────────┘               └─────────────────────────────┘
                 │ browser (password)
     owner's Mac, other person's computer
```

The owner's Mac keeps running Studio in today's **drive mode** for USB drives.

---

## 4. Phase 0: streaming test (first; gates the rest)

A throwaway test, like the original video spike:

- A plain HTTP server on the Mac or the home PC serves one converted `.mpg` with range requests.
- A bare Wii test build streams it through `pl_mpeg` callbacks, seeks to a point mid-file, and reports: sustained throughput, buffer underruns and audio dropouts, and time from "tune" to first frame.
- Run over Wi-Fi, and over a USB Ethernet adapter if the owner has one. Dolphin can check that it works, but speed is only meaningful on the real Wii.

**Pass bar (my call):** 30 minutes with no stutter or audio dropout, and tune-in under 3 s (drive mode targets 1.5 s).

If it fails, the options are, in order: a seek index so tuning in needs one request (section 6.4), a lower-bitrate conversion profile for server libraries, or recommending Ethernet. The owner decides which before Phase 1 starts.

---

## 5. The server: Studio in server mode

### 5.1 Running it

- The same Studio code, started in a **server mode** (`npm run serve`, my call on the name) with a small server settings file outside the library **(my call)**: library folder path, port (default 4747, Studio's port today), editing password, imports folder.
- Listens on the home network, not only on `127.0.0.1`. Drive mode keeps listening on `127.0.0.1` with no password, as today.
- How it starts **(my call)**: on Windows, a start script plus a Task Scheduler entry that runs it at sign-in; on Linux, a systemd service; on the Mac, by hand with the same command.
- Windows Firewall must allow the port on private networks, or the Wii can't connect. The install guide covers it, and the Wii's Test connection button (6.2) shows when it's blocked.
- Install steps for the Windows PC and the Pi go in a new guide in `docs/`.
- On first start, if the library folder is empty, Studio creates a new library in it (the same as setting up a new drive today).

### 5.2 Editing

- The web UI asks for the shared password once; a session cookie keeps the person signed in.
- Drive-only features are off in server mode: drive inspection (`diskutil`, mount checks), `dot_clean`, "safe to eject", and the native macOS/Windows folder pickers.
- **Edits from two people at once:** every save carries the version of the file it started from. If someone else saved in between, the save is refused and the editor is asked to reload **(my call)**. Studio already does this for `config.toml` (the `mtimeMs` check in `saveConfig`); server mode extends it to every file Studio writes, such as `show.toml`. The conversion queue is shared and visible to everyone.

### 5.3 Adding videos

- **Upload from the browser:** pick one or more video files; they upload to a temporary file on the server and appear in the import dialog only when complete. Then the existing import flow (show, season, episode naming, conversion) runs on the server.
- **Imports folder:** for big batches, files copied onto the server's disk by other means (a network share, `scp`) are picked from a folder browser in the web UI that replaces the native picker and is limited to the imports folder.
- **Browser uploads** are deleted after a successful conversion to save space; the uploader still has the original on their own computer **(my call)**. `.studio/sources.toml` records the upload's name and size so uploading the same file again is skipped.
- **Files in the imports folder are never changed or deleted**, as in the original design ("originals are never modified"). They stay available for converting again, for example to a lower-bitrate profile.

### 5.4 The Wii's interface (read-only, no password)

Plain HTTP, versioned under `/wii/v1/` **(my call)**:

- `GET /wii/v1/info` returns a small TOML document: interface version, server name, and a **library revision** that changes on every save or finished conversion.
- `GET /wii/v1/files/<path>` returns any file under the library root, with HTTP `Range` support for partial reads. Paths follow the formats rules (relative, `/`, no `..`). `.studio/`, hidden files and anything outside the root are refused.

---

## 6. The Wii app

### 6.1 Settings file

- A new `settings.toml` at `sd:/apps/cable-twii/settings.toml`, **on the SD card only**, wherever `boot.dol` was started from **(my call)**. The start path is often unknown anyway: the Wii Menu channel and wiiload pass none.
- **Write safety:** today the app writes only to the partition with a `/cable-twii/` folder and no `wbfs/` or `games/` folder (`vol_write_allowed`). The one exception added: the app may write `settings.toml` and `state.toml` (and their `.tmp` files) in `sd:/apps/cable-twii/`, only if the SD card has no `wbfs/` or `games/` folder. USB partitions without `/cable-twii/` stay read-only, so the games drive is never written to.
- Without an SD card, the settings screen still works for the current session but says the settings can't be saved.
- Never under a `/cable-twii/` folder, so it can't be mistaken for a media root.
- Keys: `source` (`"drive"` or `"server"`, default `"drive"`) and `server` (address).
- Hand-editable on a computer, or edited on the Wii (6.2).

### 6.2 Settings on the Wii (owner's request)

- A new **Settings** entry in the HOME menu, and a "Settings" button on the error / empty screens so a Wii with no drive can be set up from scratch.
- The screen shows: source (Drive / Server), the server address with an Edit button, a **Test connection** button that shows the result, and Save.
- **Address entry:** an on-screen keyboard in the app's Wii style, used with the pointer and A (D-pad works too). Keys: digits, letters, `.`, `:`, `/`, `-`, backspace, and a shortcut for `http://` **(my call)**. It accepts an IP (`192.168.1.20`), IP with port, a hostname, or a full link (`http://192.168.1.20:4747`); the port defaults to 4747.
- Hostnames work only if the router resolves them. `.local` names (such as `raspberrypi.local`) are not supported, since that needs mDNS on the Wii **(my call)**. The install guide recommends giving the server a fixed address in the router (a DHCP reservation).
- A mockup of the settings screen and keyboard, in the Wii redesign style, is made and approved by the owner before building.

### 6.3 Starting up

- **Drive:** exactly today's startup.
- **Server:** start networking in the background (the logo animation is not held up), fetch `info`, download `config.toml` and `library.toml`, parse them with the engine's text functions, and build lineups as today. Icons are downloaded when first shown.
- **Fallback:** if the server doesn't answer within a timeout (about 10 s, my call) or its files are invalid, the app looks for a drive as today. If one is found it plays from it and shows a short notice ("Server not reachable, playing from USB"). If not, it shows the error screen with the Settings button.

### 6.4 Playback

- The player reads through a small **source layer**: a file on the drive (today's `fopen`) or an HTTP stream. The HTTP stream implements `pl_mpeg`'s load/seek/tell callbacks with range requests on a kept-alive connection and a read-ahead buffer.
- **The clock stays the master**, with the same rules as the drive. If the network stalls, the last frame holds with a small "weak signal" note **(my call)**, and when data returns the player rejoins at the live point, never behind the schedule.
- `pl_mpeg` seeks by probing the file, and each probe over HTTP is a request. If Phase 0 shows tune-in is too slow, Studio writes a **seek index** beside each video at conversion (time → byte offset), and the Wii uses it to jump with one request. Phase 0 decides whether this is needed.
- The guide's hover preview uses the same stream; "one decoder at a time" is unchanged.

### 6.5 Picking up changes

- A **Reload channels** entry in the HOME menu fetches the files again **(my call)**.
- When the guide opens and at the midnight recompute, the app checks `info`'s library revision and reloads only if it changed **(my call)**.

### 6.6 Saved state

- In server mode, `state.toml` (last channel, volume) is saved beside `settings.toml` on the SD card, not on the server **(my call)**. Drive mode keeps saving it on the drive as today.

---

## 7. New formats (added to `docs/formats.md`)

- `settings.toml` (Wii app; read and written by the Wii, may be hand-edited).
- Studio's server settings file.
- The `/wii/v1/` interface, including the `info` document.
- The seek index, if Phase 0 calls for it.

Existing formats do not change and keep `version = 1`.

---

## 8. Delivery order

0. **Streaming test** on the real Wii (section 4). Gates everything after it.
1. **Studio server mode:** serve a library folder, the Wii interface, home-network listening, password, runs on macOS, Windows and Linux.
2. **Wii streaming:** source layer, HTTP client, `settings.toml` read from the SD card; plays from the server with the address set by hand.
3. **Wii settings screen** and on-screen keyboard (mockup first).
4. **Studio adding content:** browser upload, imports folder browser, refused-save check for two editors.
5. **Fallback, reload, docs:** drive fallback and notice, Reload channels, revision check, server install guide (Windows PC, then Pi), new checklist items in `docs/testing/wii-checklist.md`.

---

## 9. Testing

- **Wii (host tests in `wii/tests/`):** address parsing, HTTP response and range handling, the stream buffer, `settings.toml` parsing, fallback decisions, keyboard layout.
- **Studio (vitest):** server-mode routes: path checks (`..`, `.studio/`, hidden files), range responses, password required for editing and not for `/wii/v1/`, refused stale saves, upload completion.
- **Dolphin** for flows (it uses the Mac's network). Asked before running, since captures take over the Mac.
- **Windows home PC:** install from the guide, then connect the real Wii to it.
- **Real Wii** for speed and long runs: several hours streaming, server restart mid-show, Wi-Fi drop and return, midnight rollover in server mode.

---

## 10. Risks

| Risk | Mitigation |
|---|---|
| Wi-Fi too slow or unsteady for playback | Phase 0 first; Ethernet adapter; lower-bitrate profile |
| Tune-in too slow because seeking needs many requests | Seek index (6.4); read-ahead |
| Wii network stack drops on long sessions | Reconnect on error; hold the frame and rejoin live; long-run test |
| Stream buffers squeeze RAM | Fixed small buffer in MEM2; measure with the debug overlay |
| Windows Firewall or sleep blocks the Wii | Install guide: firewall rule for private networks, PC sleep settings; Test connection on the Wii |
| Office testing has no real Wii | Dolphin on the Mac for flows; speed tests (Phase 0) at home |
| Conversion slow on the Pi | Background queue; conversion could move to the uploader's computer later |
| The other person's computer is not trusted | Password for editing; home network only; Wii interface is read-only |

---

## 11. Left to the builder

- Exact HTTP client structure, buffer sizes and timeouts on the Wii.
- The server settings file's name and location, and the service files.
- Keyboard layout details within the Wii redesign style (after the mockup).
- Studio's internal changes for server mode, and how the web UI handles uploads.
- Seek index format, if needed.
