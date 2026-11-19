# 0004: Streaming from the server

Status: **Accepted** on 2026-11-18. The owner chose a lower video bitrate for server libraries. Plan 08 goes ahead with the client changes below; no seek index for now.

## Context

Plan 06 built a throwaway test (`spikes/stream/` on branch `streaming-test`) that streams one converted video from a range server to the Wii through `pl_mpeg` buffer callbacks and a read-ahead ring, and logs throughput, stalls and tune times. The pass bar (spec section 4): 30 minutes with no stutter or audio dropout, and tune-in under 3 s.

Setup on 2026-11-18: the owner's real Wii on Wi-Fi, a 4:3 CRT at 50 Hz, the Homebrew Channel on IOS58. The server was the owner's MacBook on the same network, also on Wi-Fi (`node spikes/stream/serve.mjs`, port 4748). Test file: a synthetic 7-minute 640×360 MPEG-1 at 1500 kbit/s video plus 192 kbit/s MP2 (about 212 KB/s), made with `spikes/video/encode-tests.sh`.

## Results

### Getting online

| Start | Result |
|---|---|
| First version (libogc `if_config`, 20 retries) | No address for about 5½ minutes, then `-116` (time-out). Twice |
| Normal start, diagnostic build | First attempt `-24` after 3 s; the retry got `192.168.1.155` in 4.5 s |
| Started with `<no_ios_reload/>` | Address after 9 s on the first attempt |

`sntp`, an unrelated network app on the same card, also failed on its first start and worked when started again. Reading libogc 3.2.1 (`network_wii.c`): when it fails late, every step succeeds except the last, so the Wii never receives an address. Retrying inside the same `if_config` call did not recover; a fresh attempt (or a fresh start of the app) usually did. Skipping the IOS reload did not make connecting reliable.

The first TCP connection to the server timed out once in each run, then the next one worked at once.

### Playback and tuning

| | `<no_ios_reload/>` run | Normal run | Needed |
|---|---|---|---|
| Download speed, average / lowest (KB/s) | 111 / 83 (link 131 / 78) | 85 / 23 (link 93 / 20) | about 212 |
| Video frames per second, average | 10.8 | 7.1 | 30 |
| Audio underruns | 358 | 690 | 0 |
| Stall time | 19.6 s in 70 s | 26.3 s in 97 s | 0 |
| Tune time, median / worst | 8.2 s / 12.0 s (4 tunes) | 10.4 s / 17.1 s (7 tunes) | under 3 s |
| Requests per tune | 20 | 18.7 | — |
| Decode time per frame, average / worst | 6.9 ms / 26 ms | 6.8 ms / 26 ms | under 33 ms |

Both runs fail the bar by a wide margin, so the 30-minute run was not done.

### Raw Wi-Fi speed

`wiiload` sent the 9.2 MB app (4.47 MB compressed) to the Homebrew Channel in 16.1 s: about **270 KB/s** in one continuous transfer, over the same Wi-Fi. One sample.

## What the numbers mean

- **The test client used about half the link.** It fetches 64 KiB per request, one request at a time, and polls the socket every millisecond. A single continuous transfer reached about 270 KB/s where the client averaged 93–131 KB/s.
- **Even the full link is too close for the current format.** 1500 kbit/s video with a 2000 kbit/s peak, plus 192 kbit/s audio, averages about 212 KB/s and peaks near 275 KB/s. That is no headroom against 270 KB/s, and the link dipped to 20–80 KB/s during the runs.
- **Tune time is bound by bytes, not requests.** About 0.6 MB per tune: the header, the end of the file for the duration, five or six estimate jumps, and the scan to the keyframe. At 270 KB/s that is about 2.3 s; at the measured client speed it was 8–10 s.
- **Decoding is not the problem.** 640×360 MPEG-1 decodes in about 7 ms per frame on the real Wii, worst 26 ms, inside the 33 ms a 30 fps frame allows. This is also the first real-Wii number for decision 0001 (video format).

## Decision

1. **A lower-bitrate conversion profile for server libraries** (the owner's choice over a USB Ethernet adapter). Studio in server mode converts with:
   - video `mpeg1video` at **900 kbit/s**, maxrate **1100 kbit/s**, bufsize **600 kbit**, same 640×360 / 480×360 frame sizes, 30 fps, GOP 15;
   - audio MP2 48 kHz stereo at **128 kbit/s**, same loudness normalisation.

   That averages about 128 KB/s and peaks near 154 KB/s, roughly 2× under the measured 270 KB/s. Drive libraries (USB / SD) keep today's 1500 kbit/s profile. A server library holds only files converted with the server profile: Studio converts every upload and import with it.
2. **The Wii client streams continuously.** One open-ended range request (`bytes=<pos>-`) per position, read straight into the read-ahead ring until the ring is full or the position changes; a seek closes the connection and opens a new request at the new position. No fixed 64 KiB requests. The read-ahead target is about 10 s of video (the 2 MiB ring holds about 16 s at the new bitrate).
3. **Getting online retries from scratch.** Each attempt is `net_init()` with a 10 s cap; after a failed attempt, `net_deinit()` and try again, up to 3 attempts (about 30 s). Then the app shows "Can't reach the network" with a hint to restart the app, instead of waiting for minutes. The first connection to the server gets one immediate retry.
4. **No seek index for now.** With a continuous client at 270 KB/s and smaller files, a tune reads about 0.45 MB, about 1.7 s. Revisit only if real-Wii tune times stay above 3 s after plan 08.
5. **USB Ethernet stays an option,** not a requirement. An adapter (the official Wii LAN adapter, or one with an ASIX AX88772 chip) would add headroom without code changes.

## Open

- The 30-minute run and the tune times need repeating on the real Wii with the plan 08 client and a server-profile file.
- One `wiiload` sample is a thin basis for the link speed. The plan 08 debug overlay should show the live download rate so every real-Wii session adds a data point.
- The player's load callback waits for at least 16 KiB per pl_mpeg load. This assumes Studio's MPEG-PS output with 2048-byte packets; a mux with packets over 16 KiB would end playback early.
- The 5,000-episode load time and the free MEM1 and MEM2 from a `DEBUG_OVERLAY=1` build are still to be measured, in Dolphin first, then on the real Wii (checklist item 66).
