# cable-twii Wii App Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** The `boot.dol` that turns the Wii into cable TV: animated logo, Wii-style guide with live preview, fullscreen viewing with D-pad channel flipping and a pointer-activated info panel, all driven by the shared schedule engine and the Wii clock.

**Architecture:** C11 on devkitPPC/libogc. A single main loop at 60 Hz drives a small screen state machine (`logo`, `guide`, `full`, `error`) plus a HOME-menu overlay. A media module owns one MPEG-1 decoder thread (pl_mpeg) feeding a frame queue drawn through GX and an ASND audio stream. A tuner module asks the engine what is airing now and tells the player what to open and where to seek. Every piece of logic that doesn't touch hardware lives in plain C files that are also compiled and unit-tested on the Mac.

**Tech Stack:** devkitPPC, libogc (WPAD, ASND, LWP threads, GX), libfat, GRRLIB (+ FreeType, libpng), pl_mpeg (MIT), tomlc99 + the engine (plan 01), greatest for host tests, M PLUS Rounded 1c (SIL Open Font License), Dolphin, wiiload.

**Spec:** `docs/superpowers/specs/2026-10-20-cable-twii-design.md` (§7), mockup `docs/superpowers/specs/mockups/wii-guide-and-fullscreen.html`, decisions `docs/decisions/0001-video-format.md` and `0002-drive-setup.md`.

## Global Constraints

- Output 640×480 EFB, 60 Hz render loop; honour the console's 4:3/16:9 setting (`CONF_GetAspectRatio`) and 50/60 Hz mode.
- Data root search order: every mountable USB FAT partition with `/cable-twii/`, then `sd:/cable-twii/`; never write to a partition without `/cable-twii/`. The app only writes `state.toml`.
- One decoder at a time; preview and fullscreen share the same stream.
- Clock is master: airings change when the schedule says, not when files end.
- Colours: background stripes `#F4F5F7` / `#ECEEF1` (3 px each), panels `#FFFFFF` with 2 px border `#D6DBE0`, corner radius 18 px (panels) / 12 px (rows) / full (pills), accent Wii blue `#34BEED` with glow `rgba(52,190,237,0.25)`, primary text `#444444`, secondary `#999999`, active row fill `#EEFAFF`.
- Font: M PLUS Rounded 1c (Medium for body, ExtraBold for headings), bundled in `data/`.
- Timings: logo 2.5 s total; guide hover-to-preview debounce 300 ms; channel chip 2 s; OSD auto-hide 3 s without pointer motion; ease-out cubic for all UI motion; preview→fullscreen expand 350 ms.
- Controls: pointer + A select; D-pad up/down move highlight in guide; in fullscreen D-pad up/right next channel, down/left previous; `+`/`−` volume (steps of 10, 0–100); B back to guide; HOME opens HOME menu; 1/2 unused.
- Targets: steady 30 fps video with 0 audio underruns; channel change under 1.5 s; guide UI 60 fps while the preview plays.
- No Nintendo assets. No comments in source code. No trailing whitespace.
- Commits: `wip | …`, `test | …`, `fix | …` plus the attribution line.

## Review Focus

- The remote is not pointed at the screen (IR invalid) for an hour of watching: no OSD, no cursor and no rumble must appear, and the remote must not disconnect (Task 9 test `osd_hidden_without_ir`; Task 1 sets `WPAD_SetIdleTimeout(3600)`).
- The flash drive is pulled out while watching: playback stops on a Wii-style error dialog, never a crash or a frozen frame (Task 4 manual check "unplug during playback").
- Midnight passes while a channel is on: the lineup rolls over without a visible hiccup in an airing that spans midnight (Task 3 test `tuner_spans_midnight`).
- Channels with long names, show names or descriptions (60+ characters, accents) must ellipsize inside their panels rather than overflow (Task 2 test `text_fit_ellipsizes`).
- A `.mpg` file that is shorter than `library.toml` claims shows black until the next airing, never loops or freezes (Task 5 manual check "short file").

## Progress (2026-10-21/22)

Tasks 1–5 are mostly implemented, host-tested (`make -C wii/tests`), and verified visually in Dolphin — including once with real converted video+audio from the owner's USB flash drive (Studio → engine → Wii app, full pipeline). **Not yet run on a real Wii** — every "Verify on the real Wii" step so far was done in Dolphin only; treat Tasks 1–5 as provisionally correct until confirmed on hardware (Task 11 territory). Steps that need a real-Wii run are unticked. The deliverables that were missing (`wii/icon.png`, the `CTW_DEBUG_OVERLAY` build, the chip and up-next channel icons) were added on branch `wii-hardening-prep`, together with the 50/60 Hz video mode fix-up and `docs/testing/wii-checklist.md`.

The review fixes on branch `wii-review-fixes` added: the 256-entry LRU text cache (text is rasterised once into a texture with our own UTF-8 decoder), error dialog line wrapping, the up-next card across midnight (`tn_next`), `tuner_spans_midnight`, and the drive search over every USB FAT partition (read-only) then the SD card. `st_find_root` now also fills an `st_volume` (`app.drive`) naming the chosen partition, for Task 10's `state.toml` write.

A second batch added: the "The drive was disconnected" dialog (the decoder tells a read error or a vanished file from a real end of file with `pb_classify_end`; on `PL_ERROR`, or on `PL_ENDED` more than a second before the airing ends, the full screen probes the chosen partition once with `st_drive_present` (a raw sector read that bypasses libfat's cache, since a failed read can look like end of file) and shows the dialog only if the drive is gone, otherwise it stays black until the next airing like a short file; `pb_needs_probe`/`pb_on_status`); 16:9 handling (`CONF_GetAspectRatio` → `app.widescreen`; `asp_fit` corrects for the 4:3-wide EFB pixels so a 4:3 file is pillarboxed on 16:9 and a 16:9 file is letterboxed on 4:3; rounded corners and the cursor are narrowed by `asp_pixel_x` on 16:9); texture cache invalidation after each frame upload and YUV tiling by each plane's own stride (`tile_i8`); and the audio double buffer starting on a prefilled `bufs[0]` with the first callback filling `bufs[1]` (`ab_start`/`ab_take`), with both buffers cleared on every re-tune. All of this is host-tested or build-checked only; none of it has run on a real Wii, and video playback, unplugging and 16:9 have not been checked in Dolphin either.

Known gaps, recorded and not fixed yet:
- The up-next card has no title for the last airing of today's lineup, because its successor lives in tomorrow's lineup (`tn_next` returns NULL there).
- 50 Hz (handled in code, **unverified on hardware**): libogc 3.2.1's `VIDEO_GetPreferredMode` returns `TVPal576IntDfScale` on PAL 50 Hz interlaced, whose `viTVMode` is `VI_TVMODE_PAL_INT` = 4. GRRLIB's `case VI_DEBUG_PAL:` (also 4) matches it and swaps in `TVPal528IntDf` (EFB and XFB 528, `viYOrigin` 24), so every retail PAL 50 Hz console runs in the 528-line mode. EURGB60 gets `TVEurgb60Hz480IntDf` (480/480, same as NTSC) and is left alone. `video_init` (`source/video.c`) runs `GRRLIB_Init` and then, when `vm_plan_for` (`source/vmode.c`, host-tested) says the EFB is not 480 lines and the mode is not anti-aliased, sets `rmode->efbHeight` to 480 and redoes the copy Y scale (480→528), copy source and destination, projection, viewport and scissor. The 640×480 layout then fills all 528 XFB lines, centred. On a 576-line TV, expect thin bands of about 24 lines at the top and bottom and about 8% vertical squash. Possible Task 11 follow-up if the owner dislikes the bands: force the full-height `TVPal576IntDfScale` (EFB 480, XFB 576) after `GRRLIB_Init`. Dolphin could not be put into 50 Hz for a DOL, so none of this has been seen.
- The up-next countdown prints whole minutes (`566:23` for a gap of more than nine hours); it never switches to hours.
- In the debug overlay build, heap use climbs for the first minutes because each new overlay string fills the 256-entry text cache; the soak check reads memory after the cache is full.
- Text is not narrowed on 16:9 (the plan lets the 640×480 layout stretch horizontally); only corners and the cursor are corrected. The cursor correction is approximate while the pointer is tilted, because GRRLIB scales before it rotates.
- The video fit uses the coded frame size, so Studio's 16-pixel padding (640×368 for a 640×360 picture) leaves a few black pixels at the edges of a 16:9 file on a 16:9 TV.

Known, deliberate deviations from the plan text:
- Task 2's temporary demo (panel, hot pill, rumble, ellipsized string) was never added to `main.c`; the UI kit was only checked through the screens that use it.
- Task 7: tuning and playback (`play_airing`, the per-frame re-tune, the early-end drive probe and the disconnect dialog) moved out of `full.c` into `source/live.c`, shared by the guide preview and fullscreen, so A expands the open stream and B returns to the guide without reopening it. `tune()` in `full.c` is `live_tune` plus the channel sound and chip. Channel and show icons load lazily into a 24-entry LRU texture cache (`ui/icons.c` on top of `tc_init_capacity`), at most two PNG loads per frame, with a grey rounded placeholder for missing ones. The preview debounce is `preview.c` (`pv_update`), the layout is `guide_layout.c`, the row and info strings are `guide_text.c`, all host-tested. `ui_image` now draws from the top-left corner: GRRLIB textures pivot on their top-left handle, so the old centred maths drew every scaled image (including the header logo) shifted up and left.

**This environment is Windows, not macOS** — the plan (and roadmap Task 0) assume a Mac. What actually got installed/built here, for whoever picks this up next:
- devkitPro via its Windows GUI installer (`devkitProUpdater`) — **requires a human to click through it (UAC + component picker)**, can't be scripted. Installed to `C:\devkitPro`.
- GRRLIB isn't in devkitPro's pacman repos — cloned `https://github.com/GRRLIB/GRRLIB` and `make install`ed it (matches roadmap Task 0 Step 3). Needed `ppc-freetype`, `ppc-libpng`, `ppc-libjpeg-turbo` from devkitPro's own pacman first, plus linking `-lbz2 -lbrotlidec -lbrotlicommon` (freetype's static lib pulls those in).
- **Critical gotcha:** MSYS2's `make.exe` silently drops Windows-style env vars (anything with a `C:\...` value, not just `TMP`/`TEMP`) when they're inherited from a non-MSYS2 parent process (regular `cmd`/PowerShell/git-bash). Two workarounds, both needed: (a) run builds through devkitPro's *own* bundled MSYS2 shell (`C:\devkitPro\msys2\usr\bin\bash.exe -lc "..."`), where `DEVKITPRO`/`DEVKITPPC`/`TMP` are set natively and survive; or (b) pass them as `make VAR=value` command-line args instead of environment exports, which always survive regardless of shell.
- Real GX bug (not Windows-specific, just worth flagging): mixing raw `GX_*` calls (needed for the YUV→RGB TEV conversion in `media/yuvgx.c`) with GRRLIB's own drawing corrupts everything GRRLIB draws afterward unless you restore **`GX_SetTevOrder` for TEVSTAGE0 back to `GX_COLOR0A0`** (not just `GX_SetTevOp`/`GX_SetNumTevStages`) — GRRLIB's `PASSCLR` mode depends on that specific order binding for its vertex-color input. Getting this wrong doesn't crash; it just makes every GRRLIB draw after the first video frame render invisible/black while the video itself (which doesn't need vertex color) keeps working fine.
- Dolphin's Wii SD-card folder sync: ini keys are split across two sections — `WiiSDCardEnableFolderSync` under `[Core]`, but `WiiSDCardSyncFolder` under `[General]` (easy to put both in `[Core]` and get a silent no-op).
- Studio itself was also ported to Windows this session (`studio/` commit `a1433ee` on `master` — drive detection, native folder pickers, Windows ffmpeg concurrency fix); unrelated to this plan but same session, same machine. (`a64a687` is this plan's Wii UI kit commit.)

---

## File Structure

```
wii/
├── Makefile                    devkitPro wii template; SOURCES include ../engine/src ../engine/vendor/tomlc99
├── meta.xml  icon.png          Homebrew Channel metadata (icon 128×48)
├── data/                       bin2o-embedded assets
│   ├── font_medium.ttf  font_bold.ttf  logo.png  cursor.png  cursor_shadow.png
│   └── sfx_hover.pcm  sfx_select.pcm  sfx_back.pcm  sfx_channel.pcm   (48 kHz, 16-bit, big-endian, mono)
├── source/
│   ├── main.c                  init, main loop, screen state machine
│   ├── app.h                   app struct shared by screens
│   ├── storage.c/.h            mounting and root search
│   ├── volume.c/.h             boot sector/MBR parsing and root choice               [host-tested]
│   ├── playback.c/.h           end-of-file vs read error, disconnect decision        [host-tested]
│   ├── aspect.c/.h             4:3/16:9 video fit and EFB pixel width                [host-tested]
│   ├── clock.c/.h              Wii time → (date, ms_of_day)
│   ├── tuner.c/.h              lineup cache + current/next airing per channel        [host-tested]
│   ├── nav.c/.h                channel order and wrap-around                         [host-tested]
│   ├── guide_model.c/.h        rows grouped by category, hit-testing, scrolling      [host-tested]
│   ├── osd.c/.h                OSD/chip visibility timers                            [host-tested]
│   ├── state.c/.h              state.toml load/save                                  [host-tested]
│   ├── input.c/.h              WPAD wrapper, edge detection, rumble pulses
│   ├── vmode.c/.h              EFB/XFB height plan for 50/60 Hz modes                [host-tested]
│   ├── video.c/.h              GRRLIB init plus the 480-line EFB fix-up
│   ├── debug/dbgstats.c/.h     fps averaging and overlay text (DEBUG_OVERLAY only)   [host-tested]
│   ├── debug/overlay.c/.h      corner debug overlay (DEBUG_OVERLAY only)
│   ├── media/player.c/.h       decoder thread, frame queue, A/V sync, seeking
│   ├── media/yuvgx.c/.h        Y/U/V planes → GX textures → TEV → screen
│   ├── media/audio.c/.h        ASND streaming ring buffer, volume
│   ├── media/tile.c/.h         I8 texture tiling                                     [host-tested]
│   ├── media/audiobuf.c/.h     ASND double-buffer order and ring drain               [host-tested]
│   ├── ui/theme.h              colours, sizes, timings from Global Constraints
│   ├── ui/tween.c/.h           easing and tweens                                     [host-tested]
│   ├── ui/textfit.c/.h         ellipsizing and line wrapping with a width callback   [host-tested]
│   ├── ui/textcache.c/.h       256-entry LRU text cache                              [host-tested]
│   ├── ui/utf8.c/.h            UTF-8 decoding                                        [host-tested]
│   ├── ui/draw.c/.h            stripes, rounded panels, pills, glow, images
│   ├── ui/text.c/.h            FreeType text with a texture cache
│   ├── ui/cursor.c/.h          pointer, tilt, shadow
│   ├── ui/sfx.c/.h             UI sounds
│   └── screens/logo.c  guide.c  full.c  homemenu.c  error.c  upnext.c (each with .h)
└── tests/                      host tests for [host-tested] files
    ├── Makefile  main.c
    └── test_tuner.c  test_nav.c  test_guide_model.c  test_osd.c  test_state.c  test_tween.c  test_textfit.c  test_clock.c
```

## Shared app struct (`source/app.h`)

```c
typedef enum { SCR_LOGO, SCR_GUIDE, SCR_FULL, SCR_ERROR } screen_id;

typedef struct {
    char root[256];
    ctw_db *db;
    tuner tuner;
    screen_id screen;
    bool home_open;
    int current_channel;
    int volume;
    bool widescreen;
    uint32_t now_ms;
    ctw_date today;
    uint32_t ms_of_day;
    char error_title[64];
    char error_body[512];
} app;

void app_go(app *a, screen_id s);
void app_fail(app *a, const char *title, const char *body);
```

Each screen exposes `void <name>_enter(app *a)`, `void <name>_update(app *a)`, `void <name>_draw(app *a)`.

---

### Task 1: Project skeleton and host test harness

**Files:**
- Create: `wii/Makefile`, `wii/meta.xml`, `wii/icon.png`, `wii/source/main.c`, `wii/source/app.h`, `wii/source/input.c/.h`, `wii/source/ui/theme.h`, `wii/source/ui/tween.c/.h`, `wii/tests/Makefile`, `wii/tests/main.c`, `wii/tests/test_tween.c`

**Interfaces:**
- Produces:
  - `float tw_ease_out(float t);` (cubic: `1 - (1-t)^3`, `t` clamped to 0..1)
  - `typedef struct { float from; float to; uint32_t start_ms; uint32_t dur_ms; } tween;`
  - `float tw_value(const tween *t, uint32_t now_ms);` / `bool tw_done(const tween *t, uint32_t now_ms);` / `tween tw_make(float from, float to, uint32_t now_ms, uint32_t dur_ms);`
  - `void in_init(void); void in_poll(void); bool in_pressed(uint32_t wpad_button); bool in_held(uint32_t wpad_button); bool in_ir(float *x, float *y, float *angle); void in_rumble_pulse(uint32_t ms);` (rumble stopped in `in_poll` after the duration)
- `meta.xml`: name `cable-twii`, coder (owner's handle, asked at execution), version `0.1.0`, short description "Your own cable TV", long description, `<ahb_access/>`.

- [x] **Step 1: Write the failing tween tests**

```c
TEST ease_out_endpoints(void) { ASSERT_IN_RANGE(0.0f, tw_ease_out(0.0f), 0.0001f); ASSERT_IN_RANGE(1.0f, tw_ease_out(1.0f), 0.0001f); ASSERT_IN_RANGE(0.875f, tw_ease_out(0.5f), 0.0001f); PASS(); }
TEST ease_out_clamps(void)    { ASSERT_IN_RANGE(1.0f, tw_ease_out(2.0f), 0.0001f); ASSERT_IN_RANGE(0.0f, tw_ease_out(-1.0f), 0.0001f); PASS(); }
TEST tween_progress(void)     { tween t = tw_make(10, 20, 1000, 100); ASSERT_IN_RANGE(10.0f, tw_value(&t, 1000), 0.001f); ASSERT_IN_RANGE(20.0f, tw_value(&t, 1100), 0.001f); ASSERT(tw_done(&t, 1100)); ASSERT(!tw_done(&t, 1050)); PASS(); }
```

- [x] **Step 2: Run** `make -C wii/tests` → FAIL (missing `tw_ease_out`).

- [x] **Step 3: Implement `tween.c`** (`wii/icon.png` 128×48 is generated by `tools/make-logo.py`), the host test Makefile (compiles `../source/ui/tween.c` etc. plus engine sources with the host compiler), and a `main.c` that initialises video (GRRLIB), WPAD (`WPAD_SetDataFormat(WPAD_CHAN_ALL, WPAD_FMT_BTNS_ACC_IR)`, `WPAD_SetIdleTimeout(3600)`), draws the striped background and exits to the Homebrew Channel on HOME.

- [x] **Step 4: Verify**

Run: `make -C wii/tests && make -C wii`
Expected: tests PASS; `wii/cable-twii.dol` built. Open it in Dolphin: striped grey background; HOME exits.

- [x] **Step 5: Commit**

```bash
git add wii && git commit -m "wip | wii app skeleton and tweens"
```

---

### Task 2: UI kit (drawing, text, cursor, sounds)

**Files:**
- Create: `wii/source/ui/draw.c/.h`, `wii/source/ui/text.c/.h`, `wii/source/ui/textfit.c/.h`, `wii/source/ui/cursor.c/.h`, `wii/source/ui/sfx.c/.h`, `wii/data/*`, `wii/tests/test_textfit.c`

**Interfaces:**
- Produces:
  - `void ui_stripes(void);`
  - `void ui_panel(float x, float y, float w, float h, float radius, uint32_t fill, uint32_t border);`
  - `void ui_pill(float x, float y, float w, float h, const char *label, bool hot);` (gradient `#FFFFFF`→`#E6EAEE`, border `#C9CFD5`; hot = blue border + glow)
  - `void ui_glow(float x, float y, float w, float h, float radius, float alpha);`
  - `void ui_image(GRRLIB_texImg *img, float x, float y, float w, float h, float alpha);`
  - `typedef enum { FONT_BODY, FONT_BOLD } ui_font; typedef enum { ALIGN_LEFT, ALIGN_CENTER, ALIGN_RIGHT } ui_align;`
  - `void ui_text(ui_font f, int px, float x, float y, const char *utf8, uint32_t rgba, ui_align a);`
  - `float ui_text_width(ui_font f, int px, const char *utf8);`
  - `void ui_text_fit(ui_font f, int px, const char *utf8, float max_w, char *out, size_t out_size);` (wraps `textfit`)
  - `size_t tf_fit(const char *utf8, float max_w, float (*width)(const char *s, size_t len, void *ctx), void *ctx, char *out, size_t out_size);` (pure; whole string if it fits, else the longest prefix on a UTF-8 boundary plus `…`)
  - `void cur_update(void); void cur_draw(void); bool cur_visible(void); float cur_x(void); float cur_y(void);` (hand cursor rotated by the remote's roll, shadow offset 3 px)
  - `typedef enum { SFX_HOVER, SFX_SELECT, SFX_BACK, SFX_CHANNEL } sfx_id; void sfx_init(void); void sfx_play(sfx_id id);`
  - `GRRLIB_texImg *ui_load_png_file(const char *path);` (from the drive; returns NULL on failure; 128×128 max)
- Text cache: key (font, px, string, colour) → rendered texture; 256 entries, LRU eviction.
- Assets: the owner approves downloading M PLUS Rounded 1c from Google Fonts (OFL, include `OFL.txt` in `wii/data/`). Cursor and logo are original PNGs drawn for this project. Sounds are synthesised short blips (generated with ffmpeg `sine`/`anoisesrc` + envelopes, script `tools/make-sfx.sh`), never sampled from Nintendo.

- [x] **Step 1: Write the failing textfit tests**

```c
static float fixed_width(const char *s, size_t len, void *ctx) { (void)s; (void)ctx; return (float)len * 10.0f; }
TEST text_fits_whole(void)        { char o[64]; tf_fit("Gumball", 100, fixed_width, NULL, o, sizeof o); ASSERT_STR_EQ("Gumball", o); PASS(); }
TEST text_fit_ellipsizes(void)    { char o[64]; tf_fit("The Amazing World of Gumball", 100, fixed_width, NULL, o, sizeof o); ASSERT_STR_EQ("The Amaz…", o); PASS(); }
TEST text_fit_utf8_boundary(void) { char o[64]; tf_fit("Pokémon Indigo League", 60, fixed_width, NULL, o, sizeof o); ASSERT(o[strlen(o) - 4] != (char)0xC3); PASS(); }
```

(The ellipsis is 3 bytes and counted as width of 1 character in `fixed_width`'s terms; `"The Amaz…"` is 8 + 1 = 90 px ≤ 100.)

- [x] **Step 2: Run** `make -C wii/tests` → FAIL.

- [x] **Step 3: Implement** the UI kit.

- [ ] **Step 4: Verify** — host tests PASS; temporary demo in `main.c` (removed in Task 7) draws a panel, a pill that turns hot under the cursor with `SFX_HOVER` and a 30 ms rumble pulse, and a long ellipsized string. Check in Dolphin and on the Wii via `wiiload`. (Dolphin only — real Wii pending, Task 11; the demo was never added to `main.c`)

- [x] **Step 5: Commit**

```bash
git add wii tools/make-sfx.sh && git commit -m "wip | wii ui kit"
```

---

### Task 3: Storage, loading, clock, tuner and error screen

**Files:**
- Create: `wii/source/storage.c/.h`, `wii/source/clock.c/.h`, `wii/source/tuner.c/.h`, `wii/source/screens/error.c/.h`, `wii/tests/test_clock.c`, `wii/tests/test_tuner.c`

**Interfaces:**
- Consumes: engine `ctw_db_load`, `ctw_lineup_build`, `ctw_lineup_at`, `ctw_lineup_next`, `ctw_lineup_free`.
- Produces:
  - `int st_find_root(char *out, size_t out_size, st_volume *vol);` (0 found / -1 none; `vol` names the chosen partition; mounts per `docs/decisions/0002-drive-setup.md`; never opens a file for writing)
  - `void clk_split(int64_t seconds, ctw_date *day, uint32_t *ms_of_day);` (pure; treats `seconds` as local time since 1970-01-01 00:00, uses `gmtime_r`) and `void clk_now(ctw_date *day, uint32_t *ms_of_day);` (`time(NULL)`)
  - `typedef struct { int channel; ctw_date day; ctw_lineup today; ctw_lineup yesterday; bool valid; } tuner_entry;`
  - `typedef struct { const ctw_db *db; tuner_entry *entries; size_t count; } tuner;`
  - `void tn_init(tuner *t, const ctw_db *db);` / `void tn_free(tuner *t);`
  - `const ctw_airing *tn_current(tuner *t, int channel, ctw_date day, uint32_t ms_of_day, uint32_t *offset_ms);` (rebuilds `today`/`yesterday` lazily when `day` changes; for looping channels `yesterday` is unused)
  - `uint32_t tn_ms_remaining(const ctw_airing *a, uint32_t offset_ms);`
  - `void error_enter(app *a); void error_update(app *a); void error_draw(app *a);` — Wii-style dialog: white rounded panel centred, title in bold, body text, one pill button "Back to Homebrew Channel".
- Boot messages (exact copy):
  - no root: title `No cable-twii drive found`, body `Plug in the drive prepared with cable-twii Studio, or put a cable-twii folder on the SD card.`
  - load errors: title `There's a problem with the schedule`, body = first three issue messages, each prefixed `config.toml line N:` / `library.toml:` when known, followed by `Open cable-twii Studio to fix it.`

- [x] **Step 1: Write the failing tests**

```c
TEST clock_epoch(void)              { ctw_date d; uint32_t ms; clk_split(0, &d, &ms); ASSERT_EQ(1970, d.year); ASSERT_EQ(1, d.month); ASSERT_EQ(1, d.day); ASSERT_EQ(0u, ms); PASS(); }
TEST clock_known_time(void)         { ctw_date d; uint32_t ms; clk_split(1792501490, &d, &ms); /* 2026-10-20 */ ASSERT_EQ(2026, d.year); ASSERT_EQ(10, d.month); ASSERT_EQ(20, d.day); PASS(); }
TEST tuner_current_matches_engine(void) { /* engine root_basic fixture, channel 1, 2026-10-20, 07:00:05 → offset 5000, same airing as ctw_lineup_at */ }
TEST tuner_caches_by_day(void)      { /* two calls same day → entry.today.items pointer unchanged */ }
TEST tuner_spans_midnight(void)     { /* timed channel 07:00+19:00: airing at 2026-10-19 23:59:00 and at 2026-10-20 00:00:30 are the same episode path with offsets 90 s apart … the second lookup comes from the 2026-10-20 tuner entry's yesterday lineup */ }
TEST tuner_remaining(void)          { /* duration 600000, offset 5000 → 595000 */ }
```

(The `clock_known_time` value must be checked against `date -u -r 1792501490` while writing the test.)

- [x] **Step 2: Run** `make -C wii/tests` → FAIL.

- [x] **Step 3: Implement**, and wire boot in `main.c`: `st_find_root` → `ctw_db_load` → `tn_init` → logo screen (placeholder until Task 6) or `app_fail` with the copy above.

- [x] **Step 4: Verify** — host tests PASS. In Dolphin with the sample library on the virtual SD: boot reaches the placeholder. Remove `config.toml`: the "problem with the schedule" dialog shows `config.toml not found`. Remove the folder: the "No cable-twii drive found" dialog shows.

- [x] **Step 5: Commit**

```bash
git add wii && git commit -m "wip | wii boot loading, clock and tuner"
```

---

### Task 4: Video player

**Files:**
- Create: `wii/source/media/player.c/.h`, `wii/source/media/yuvgx.c/.h`, `wii/source/media/audio.c/.h`, `wii/source/media/pl_mpeg.h` (vendored)

**Interfaces:**
- Produces:
  - `typedef enum { PL_IDLE, PL_SEEKING, PL_PLAYING, PL_ENDED, PL_ERROR } pl_state;`
  - `int pl_open(const char *path, uint32_t offset_ms);` (stops any current stream, opens, seeks, starts the decode thread; returns 0 or -1)
  - `void pl_close(void);`
  - `pl_state pl_status(void);`
  - `void pl_draw(float x, float y, float w, float h, float alpha);` (draws the latest frame letterboxed/pillarboxed into the rect; black if no frame yet)
  - `void pl_set_volume(int volume_0_100);`
  - `uint32_t pl_position_ms(void);`
  - `int pl_video_size(int *w, int *h);`
- Internals: decode thread (`LWP_CreateThread`, priority below main) reads with a 256 KB buffered FILE, decodes into a 3-frame ring of planar YUV buffers in MEM2; main thread uploads the newest due frame each vsync (`yg_upload`, `yg_draw` per decision 0001: TEV or CPU path); audio samples (`plm_set_audio_decode_callback`) converted float→s16 big-endian stereo into an ASND double buffer (`ASND_SetVoice` with `VOICE_STEREO_16BIT`, 48000 Hz, 4096-sample chunks); audio clock is the sync master: a video frame is shown when its PTS ≤ audio PTS, frames more than 2 frames late are dropped. Read errors set `PL_ERROR`.

- [x] **Step 1: Implement** from the Task 2 spike's learnings (roadmap plan 00), without spike shortcuts.

- [x] **Step 2: Temporary harness in `main.c`** (removed in Task 5): A opens `Cartoon Network/Gumball/Season 1/S01E01 - Test One.mpg` at offset 0; 1 reopens at offset 20 000; +/− volume.

- [ ] **Step 3: Verify on the real Wii** (`wiiload wii/cable-twii.dol`), sample library at full length (not `--short`) on the flash drive: (Dolphin only — real Wii pending, Task 11; the overlay is built with `make -C wii DEBUG_OVERLAY=1` → `wii/cable-twii-debug.dol` and was seen in Dolphin (UI 59.9 fps, memory lines), but not yet with video playing; 16:9 handling and the "The drive was disconnected" screen are built and host-tested but not yet checked in Dolphin or on the Wii)
  - fps overlay (debug build flag `-DCTW_DEBUG_OVERLAY`) shows 30.0 average, 0 underruns over 10 minutes;
  - reopen at 20 000 ms shows burned-in timecode within 00:00:19.5–00:00:20.5, in under 1.5 s;
  - 4:3 Ben 10 file is pillarboxed on a 16:9 TV setting and fills the width on 4:3;
  - lip-sync check: the sample's beep and flash line up by eye;
  - **unplug during playback**: status becomes `PL_ERROR` and the harness shows the error screen with title `The drive was disconnected` and body `Plug it back in and start cable-twii again.`

- [x] **Step 4: Commit**

```bash
git add wii && git commit -m "wip | wii video player"
```

---

### Task 5: Fullscreen viewing, channel flipping, up-next card

**Files:**
- Create: `wii/source/nav.c/.h`, `wii/source/osd.c/.h`, `wii/source/screens/full.c/.h`, `wii/source/screens/upnext.c/.h`, `wii/tests/test_nav.c`, `wii/tests/test_osd.c`

**Interfaces:**
- Consumes: tuner, player, UI kit.
- Produces:
  - `int nav_step(const ctw_db *db, int current_number, int dir);` (`dir` ±1; channels sorted by number; wraps)
  - `typedef struct { uint32_t chip_until; uint32_t osd_until; bool osd; } osd_state;`
  - `void osd_channel_changed(osd_state *s, uint32_t now_ms);` (chip for 2000 ms)
  - `void osd_pointer(osd_state *s, bool ir_valid, bool moved, uint32_t now_ms);` (valid + moved → OSD visible until now + 3000; invalid → hidden immediately)
  - `bool osd_chip_visible(const osd_state *s, uint32_t now_ms); bool osd_visible(const osd_state *s, uint32_t now_ms);`
  - `void full_enter(app *a); void full_update(app *a); void full_draw(app *a);`
  - `void upnext_draw(app *a, const ctw_airing *gap, const ctw_airing *next, uint32_t ms_left);`
  - `void tune(app *a, int channel);` in `full.c`, also used by the guide: `tn_current` → if `CTW_AIR_UPNEXT` → `pl_close`, else `pl_open(root/path, offset)`.
- Behaviour: every frame, if `tn_ms_remaining` hits 0 → tune the next airing (`ctw_lineup_next`, or re-query `tn_current` after midnight). If `pl_status() == PL_ENDED` before the airing ends → black until then (**short file**). D-pad flips with `SFX_CHANNEL`; chip top-right: rounded white pill, channel icon 40×40 + number in Wii blue ExtraBold 36 px. B → guide. Up-next card: stripes background, white panel centred with channel icon, `Up next`, show name + episode title, `at HH:MM`, countdown `m:ss`.

- [x] **Step 1: Write the failing tests**

```c
TEST nav_wraps_forward(void)   { /* channels 1,2,3: nav_step(db,3,+1) == 1 */ }
TEST nav_wraps_back(void)      { /* nav_step(db,1,-1) == 3 */ }
TEST nav_gaps_in_numbers(void) { /* channels 1,5,9: nav_step(db,1,+1) == 5 */ }
TEST nav_unknown_current(void) { /* nav_step(db,4,+1) == 5 (next higher), nav_step(db,4,-1) == 1 */ }
TEST chip_shows_two_seconds(void) { osd_state s = {0}; osd_channel_changed(&s, 1000); ASSERT(osd_chip_visible(&s, 2999)); ASSERT(!osd_chip_visible(&s, 3000)); PASS(); }
TEST osd_shows_on_motion(void)    { osd_state s = {0}; osd_pointer(&s, true, true, 1000); ASSERT(osd_visible(&s, 3999)); ASSERT(!osd_visible(&s, 4000)); PASS(); }
TEST osd_hidden_without_ir(void)  { osd_state s = {0}; osd_pointer(&s, true, true, 1000); osd_pointer(&s, false, false, 1100); ASSERT(!osd_visible(&s, 1100)); PASS(); }
TEST osd_still_pointer_times_out(void) { osd_state s = {0}; osd_pointer(&s, true, true, 1000); osd_pointer(&s, true, false, 2000); ASSERT(!osd_visible(&s, 4000)); PASS(); }
```

- [x] **Step 2: Run** `make -C wii/tests` → FAIL.

- [x] **Step 3: Implement**; temporarily boot straight into fullscreen on the last/first channel. (chip: 40×40 channel icon + 36 px number, laid out by `osd_chip_layout`; up-next card shows the channel icon and ellipsizes its titles; both seen in Dolphin with a real channel icon and with the placeholder)

- [ ] **Step 4: Verify on the real Wii** with the sample library and a schedule from Studio: (Dolphin only — real Wii pending, Task 11)
  - tune-in lands mid-episode at the time Studio's Preview shows (compare burned-in timecode);
  - flipping away and back after 1 minute lands 1 minute further;
  - an episode ends and the next airing starts within one second of its scheduled time;
  - a gap on the MTV channel shows the up-next card with a correct countdown;
  - channel change time under 1.5 s.

- [x] **Step 5: Commit**

```bash
git add wii && git commit -m "wip | wii fullscreen viewing and channel flipping"
```

---

### Task 6: Logo animation

**Files:**
- Create: `wii/source/screens/logo.c/.h`, `wii/data/logo.png`

**Interfaces:**
- Produces: `void logo_enter(app *a); void logo_update(app *a); void logo_draw(app *a);` and `void logo_header_rect(float *x, float *y, float *w, float *h);` (the logo's final place in the guide header, used by both screens so the hand-off is seamless).
- Sequence (2.5 s): 0–0.6 s logo scales 0.6→1.0 and fades in at screen centre on the stripes; 0.6–1.4 s a diagonal shine sweeps across it; 1.4–2.0 s it shrinks and moves to `logo_header_rect` while the guide panels slide in from the left/right by 40 px and fade in; at 2.0 s switch to the guide. Pressing A skips to the end. Loading runs before the logo starts, so the animation never stalls.

- [ ] **Step 1: Implement**, logo artwork as an original PNG wordmark "cable-twii" in Wii blue + grey (owner may replace `data/logo.png`).

- [ ] **Step 2: Verify** in Dolphin and on the Wii: smooth at 60 fps, no jump when the guide takes over, A skips.

- [ ] **Step 3: Commit**

```bash
git add wii && git commit -m "wip | wii logo animation"
```

---

### Task 7: Guide screen

**Files:**
- Create: `wii/source/guide_model.c/.h`, `wii/source/screens/guide.c/.h`, `wii/tests/test_guide_model.c`

**Interfaces:**
- Produces:
  - `typedef enum { GM_HEADER, GM_CHANNEL } gm_kind;`
  - `typedef struct { gm_kind kind; const char *category; const ctw_channel *channel; float y; float h; } gm_row;`
  - `size_t gm_build(const ctw_db *db, gm_row *rows, size_t cap);` (categories in order of first appearance by channel number; header row h 28, channel row h 64, spacing 6)
  - `int gm_hit(const gm_row *rows, size_t n, float scroll, float list_top, float px, float py);` (index of channel row under the point or -1; headers not hittable)
  - `float gm_scroll_to(const gm_row *rows, size_t n, int index, float scroll, float view_h);` (minimum scroll that makes the row fully visible)
  - `int gm_step(const gm_row *rows, size_t n, int index, int dir);` (next/previous channel row, skipping headers, no wrap)
  - `void guide_enter(app *a); void guide_update(app *a); void guide_draw(app *a);`
- Layout (640×480 logical, 16:9 stretches horizontally): header 0–44 (logo left, clock `HH:MM` right, ExtraBold 20 px); grid inset 3% left/right, 4% bottom; left column 42% = channel list panel; right column top = preview (16:9 rounded frame, 3 px white border), bottom = info panel.
- Channel row: channel icon 44×44, number small grey, channel name bold 16 px, line 2: current show icon 20×20 + `Show · SxxEyy Title` 13 px grey, line 3: `HH:MM – HH:MM` + 4 px progress bar (`#E1E5E9` track, `#34BEED` fill). Hover/highlight: `#EEFAFF` fill, `#34BEED` border, glow, `SFX_HOVER` + 30 ms rumble on change. Ads airings show `Commercial break`, up-next shows `Up next: <show>`.
- Preview: highlighted channel tuned 300 ms after the highlight settles; audio on; frame-in fade 150 ms.
- Info panel: channel name (blue ExtraBold 18 px), show icon 48×48 + show name bold, episode `SxxEyy · Title`, description (up to 3 lines, ellipsized), times and progress.
- Input: pointer hover highlights; D-pad up/down `gm_step` and `gm_scroll_to`; pointer within 24 px of the list's top/bottom edge scrolls 6 px per frame; A on a channel → `SFX_SELECT`, expand the preview rect to fullscreen over 350 ms with ease-out, then `app_go(a, SCR_FULL)` without reopening the stream; B in fullscreen returns here with the reverse animation.

- [x] **Step 1: Write the failing tests**

```c
TEST gm_groups_by_category(void) { /* channels 1 Kids, 2 Music, 3 Kids → rows: H Kids, C1, C3, H Music, C2 */ }
TEST gm_hit_rows(void)           { /* point inside C1 → index 1; inside header → -1; scrolled by 70 → hits shift */ }
TEST gm_step_skips_headers(void) { /* from C3 (index 2) +1 → index 4 (C2); from index 1 -1 → 1 */ }
TEST gm_scroll_to_visible(void)  { /* view_h 200, row at y 250 h 64 → scroll 114 */ }
```

- [x] **Step 2: Run** `make -C wii/tests` → FAIL.

- [x] **Step 3: Implement**; remove the temporary boot-to-fullscreen; boot flow becomes logo → guide.

- [ ] **Step 4: Verify** in Dolphin and on the Wii against the approved mockup: layout matches; hover sweeps don't thrash the preview; 60 fps with the preview playing (debug overlay); 3 channels + headers scroll correctly when the TV is 4:3. (Dolphin only, with a throwaway build that embeds a 9-channel library and scripts the D-pad, A and B: the layout matches the mockup on a 16:9 Dolphin, the panels slide in under the moving logo, D-pad scrolling and the A expand / B return work. No video files, so the preview stayed black; pointer hover, the 60 fps check (the debug overlay exists now but has not been run with a playing preview) and 4:3 were not seen. Real Wii pending, Task 11.)

- [ ] **Step 5: Commit**

```bash
git add wii && git commit -m "wip | wii guide screen"
```

---

### Task 8: Fullscreen info panel and volume

**Files:**
- Modify: `wii/source/screens/full.c`

**Interfaces:**
- Consumes: `osd_state` (Task 5).
- Behaviour: when `osd_visible`, a white rounded panel slides up (200 ms) at the bottom: channel name (blue), show icon, `Show · SxxEyy Title`, `HH:MM – HH:MM`, progress bar, pill buttons `◀ CH`, `CH ▶`, `Guide`, and a volume control (`−` pill, 10-segment bar, `+` pill). Pills are pointer-clickable with hover sound/rumble. The cursor is drawn only while the OSD is visible. `+`/`−` buttons show the volume bar alone for 2 s even without the OSD.

- [x] **Step 1: Implement.** (`osd_layout.c` pure layout and hit-testing, `screens/panel.c` drawing, volume step and timers in `osd.c`, per-screen cursor via `full_cursor_visible`)

- [ ] **Step 2: Verify on the Wii:** (Dolphin only with scripted IR, real Wii pending, Task 11) pointing shows the panel; holding still for 3 s hides it; pointing away hides it at once; clicking `CH ▶` flips; volume persists across channel changes.

- [ ] **Step 3: Commit**

```bash
git add wii && git commit -m "wip | wii fullscreen info panel"
```

---

### Task 9: HOME menu, power/reset, remote idle

**Files:**
- Create: `wii/source/screens/homemenu.c/.h`
- Modify: `wii/source/main.c`

**Interfaces:**
- Produces: `void home_open(app *a); void home_update(app *a); void home_draw(app *a);` (overlay drawn above any screen; `a->home_open` pauses input to the screen below, video keeps playing behind a 60% dark dim).
- Overlay: top bar `HOME Menu` style strip, two large pills `Wii Menu` and `Homebrew Channel`, `Resume` at the bottom, remote battery (`WPAD_BatteryLevel`) as 4 bars. `Wii Menu` → `SYS_ResetSystem(SYS_RETURNTOMENU, 0, 0)` after `pl_close`, `st_save`; `Homebrew Channel` → `exit(0)` after the same cleanup. Power button → `SYS_ResetSystem(SYS_POWEROFF, 0, 0)` via `SYS_SetPowerCallback`; Reset button → return to Homebrew Channel via `SYS_SetResetCallback`. Callbacks only set flags; the main loop acts on them.

- [x] **Step 1: Implement.**

- [ ] **Step 2: Verify on the Wii (Dolphin only — real Wii pending, Task 11):** each option works; Power turns the console off cleanly; Reset returns to HBC; HOME again closes the menu; leaving the remote untouched for 30 minutes while watching does not disconnect it.

- [x] **Step 3: Commit**

```bash
git add wii && git commit -m "wip | wii home menu and power handling"
```

---

### Task 10: Remembered state

**Files:**
- Create: `wii/source/state.c/.h`, `wii/tests/test_state.c`

**Interfaces:**
- Produces:
  - `typedef struct { int last_channel; int volume; } ctw_state;`
  - `int state_parse(const char *toml_text, ctw_state *out);` (defaults `last_channel = 0`, `volume = 80`; clamps volume 0–100; bad file → defaults, returns -1)
  - `size_t state_format(const ctw_state *s, char *out, size_t out_size);` (`last_channel = N\nvolume = N\n`)
  - `void state_load(const char *root, ctw_state *out); void state_save(const char *root, const ctw_state *s);` (write `state.toml.tmp`, then rename)
- Guide opens with `last_channel` highlighted (first channel if missing); volume restored; saved on channel change (at most once every 10 s) and on exit.

- [x] **Step 1: Write the failing tests**

```c
TEST state_round_trip(void) { /* {3, 60} → format → parse → {3, 60} */ }
TEST state_defaults(void)   { /* "" → {0, 80} */ }
TEST state_clamps(void)     { /* "volume = 250" → 100 */ }
TEST state_garbage(void)    { /* "%%%" → returns -1, defaults */ }
```

- [x] **Step 2–4: Fail, implement, pass** — `make -C wii/tests`.

- [x] **Step 5: Commit**

```bash
git add wii && git commit -m "wip | wii remembered channel and volume"
```

---

### Task 11: Hardening on real hardware

**Files:**
- Create: `docs/testing/wii-checklist.md`

- [x] **Step 1: Write the checklist** (`docs/testing/wii-checklist.md`, 50 items including the debug-overlay build and `wiiload` steps) from spec §10 plus this plan's Review Focus: tune-in mid-episode; D-pad flipping; preview→fullscreen and back; OSD show/hide; HOME menu options; remote disconnect/reconnect; no drive; bad config; drive unplugged while watching; short file; midnight rollover on a timed and a looping channel (set the Wii clock to 23:58); 4:3 and 16:9 TV settings; 50 Hz and 60 Hz; 4-hour soak with memory free shown in the debug overlay staying flat.

- [ ] **Step 2: Run every item on the real Wii** with the owner's real library and both drives plugged in (per decision 0002). Record pass/fail with notes in the checklist.

- [ ] **Step 3: Fix failures** test-first where the cause is in host-tested code; commit each with `fix | …`.

- [ ] **Step 4: Commit the completed checklist**

```bash
git add docs/testing && git commit -m "test | wii hardware checklist results"
```
