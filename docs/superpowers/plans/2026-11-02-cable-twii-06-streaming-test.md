# cable-twii Streaming Test Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Measure whether the real Wii can stream a converted video from a home server smoothly and tune in fast enough, before any Wii server feature is built.

**Architecture:** A throwaway Wii app in `spikes/stream/`, copied from the video spike, reads the `.mpg` through `pl_mpeg` buffer callbacks backed by HTTP range requests and a read-ahead thread. A tiny Node range server in the same folder serves the file and logs every request. The numbers go into decision record 0004.

**Tech Stack:** devkitPPC/libogc sockets (`if_config`, `net_*`), pl_mpeg, Node 22 (`node:http`), Dolphin, wiiload.

**Spec:** `docs/superpowers/specs/2026-11-02-server-source-design.md` (section 4)

## Plan set and order

| # | Plan | Depends on |
|---|---|---|
| 06 | this streaming test | — |
| 07 | `2026-11-02-cable-twii-07-studio-server.md` | — (its scope does not depend on 06's numbers, so it may run while 06 waits for the real Wii) |
| 08 | `2026-11-02-cable-twii-08-wii-server-source.md` | 06 (decision 0004), 07 Tasks 1–3 (the `/wii/v1/` interface) |

If decision 0004 calls for a seek index, plan 09 is written from its numbers before plan 08 Task 5 starts.

## Global Constraints

- Throwaway: no tests, no review, never imported by product code.
- Test file: the 640×360 / 1500k file made by `spikes/video/encode-tests.sh` (today's product format), at least 6 minutes, timecode burned in.
- Pass bar (spec section 4): 30 minutes with no stutter or audio dropout, and tune-in under 3 s.
- Server port 4747.
- Dolphin captures take over the Mac: ask the owner before every Dolphin run.
- Commits: `wip | …`, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.

## Review Focus

Throwaway code, so none; the real-Wii numbers in Task 3 are the review.

---

### Task 1: Range server

**Files:**
- Create: `spikes/stream/serve.mjs`

**Interfaces:**
- Produces: `node spikes/stream/serve.mjs <file.mpg> [port]` serving `GET`/`HEAD /test.mpg` on `0.0.0.0`.

- [x] **Step 1: Write `serve.mjs`** with `node:http`: single ranges only (`bytes=a-b`, `bytes=a-`, `bytes=-n`) → 206 with `Content-Range`; no `Range` → 200; out of range → 416; `Content-Length` and `Accept-Ranges: bytes` on every response, never chunked; keep-alive on. One log line per request: time, client address, range, bytes sent, ms.
- [x] **Step 2: Verify**
  Run: `node spikes/stream/serve.mjs <file.mpg> &` then `curl -s -r 0-99 -o /dev/null -w '%{http_code} %{size_download}\n' http://127.0.0.1:4747/test.mpg`
  Expected: `206 100`. `curl -sI http://127.0.0.1:4747/test.mpg` shows `Accept-Ranges: bytes` and the file's `Content-Length`.
- [x] **Step 3: Commit** `wip | streaming test range server`

---

### Task 2: Streaming spike app

**Files:**
- Create: `spikes/stream/Makefile` (copy of `spikes/video/Makefile`), `spikes/stream/source/main.c` (from `spikes/video/source/main.c`), `spikes/stream/source/pl_mpeg.h` (copy), `spikes/stream/README.md`

**Interfaces:**
- Consumes: Task 1 server.
- Produces: `spikes/stream/boot.dol`; log file `sd:/cable-twii-spike/stream-log.txt`.

- [x] **Step 1: Replace file reading with HTTP** in `main.c`:
  - Read `sd:/cable-twii-spike/server.txt`, one line `host:port`.
  - Bring the network up with `if_config(ip, mask, gw, true, 20)`, showing "connecting" until it returns.
  - Keep one keep-alive TCP connection and send `GET /test.mpg` with `Range` in 64 KiB chunks. Take the total length from the first response's `Content-Range`.
  - A fetch thread (`LWP_CreateThread`) fills a 2 MiB ring in MEM2 ahead of the read position.
  - Create the buffer with `plm_buffer_create_with_callbacks(load, seek, tell, length, ctx)`:
    - `load` follows pl_mpeg's file load callback: `plm_buffer_discard_read_bytes`, then copy from the ring up to capacity, blocking until data is there and counting the stall ms.
    - `seek` resets the ring when the target is outside the buffered window.
    - `tell` returns the read position.
  - Reconnect once on a socket error.
- [x] **Step 2: Stats and controls**:
  - On screen, and appended to the log every 5 s: fps, audio underruns, KB/s over the last 5 s, ring fill %, total stall ms, total requests.
  - Button 1 seeks to `SEEK_TARGET` seconds (default 300).
  - Button 2 tunes: close the connection, reconnect, and open at a random offset between 0 and (duration − 60 s).
  - For both buttons, log the ms from the press to the first new frame shown, and the number of requests made.
  - The `AUTOTEST=1` build presses 2 every 20 s, the same as the video spike's autotest.
- [x] **Step 3: Build**
  Run: `make -C spikes/stream`
  Expected: `spikes/stream/boot.dol` links with no errors.
- [ ] **Step 4: Run in Dolphin** (skipped: the owner ran it on the real Wii instead) (ask the owner first):
  1. Start the server on the Mac.
  2. Put `cable-twii-spike/server.txt` in Dolphin's SD sync folder, containing `<ip>:4747`, where `<ip>` is from `ipconfig getifaddr en0`.
  3. Run `open -a /Applications/Dolphin.app --args -e spikes/stream/boot.dol`.

  Expected: the video plays, the server log shows range requests, and button 2 re-tunes. Ignore Dolphin's speed.
- [x] **Step 5: Write `README.md`**: how to build, start the server and write `server.txt`, and what to record in Task 3.
- [x] **Step 6: Commit** `wip | streaming test app`

---

### Task 3: Real Wii run (owner, at home)

- [x] **Step 1:** Set up the server on the Windows PC or the Mac at home, wired to the router if possible. The Wii stays on Wi-Fi. `server.txt` holds that machine's address.
- [x] **Step 2 (partly: short runs only, no 30-minute run, no Ethernet; see decision 0004):** Run `WIILOAD=tcp:<wii-ip> wiiload spikes/stream/boot.dol` for each run:
  - 30 minutes of continuous play over Wi-Fi
  - 10 tunes with button 2 over Wi-Fi
  - the same two runs over a USB Ethernet adapter, if one is available
  - while at it, the pending video spike run (roadmap plan 00, Task 2 Step 4)
- [x] **Step 3:** Record for each run:
  - underruns (target 0)
  - stall ms
  - average and worst KB/s
  - tune ms, median and worst (target under 3000)
  - requests per tune

---

### Task 4: Decision record

**Files:**
- Create: `docs/decisions/0004-server-streaming.md`
- Modify: `docs/STATUS.md`

- [x] **Step 1: Write the record**:
  - The Task 3 numbers, and pass or fail against the bar.
  - The decision, one of:
    - **Both pass:** plan 08 runs as written. Record the ring size and chunk size to use.
    - **Playback passes, tune-in fails:** a seek index is needed. Plan 09 is written first.
    - **Playback fails:** the owner picks a lower-bitrate server profile or Ethernet before plan 08 starts.
- [x] **Step 2: Update `STATUS.md`** with a row for plans 06–08.
- [x] **Step 3: Commit** `wip | streaming test results`
