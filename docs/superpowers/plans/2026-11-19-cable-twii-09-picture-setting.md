# cable-twii Picture Setting Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** A global Picture setting (Fit, 14:9, Fill) that lets a 16:9 show fill a 4:3 TV by cutting its sides, chosen from a switch in the HOME menu's bottom bar and saved in `settings.toml`.

**Architecture:** The crop math is pure and host-tested in `aspect.c`: a shown shape `S` per mode, and the texture window that cuts the frame to `S`. `pl_draw` fits `S` into its box and `yg_draw` draws only that window of the frame textures. The guide preview passes its zoom progress as the crop amount, so the cut eases in to full screen. The HOME switch's rects and hits are host-tested in `home_layout.c`; drawing reuses the Settings card's segmented look through a shared `ui_segment`.

**Tech Stack:** C11 on devkitPPC/libogc/GRRLIB/GX, tomlc99, greatest host tests (`make -C wii/tests test`).

**Spec:** `docs/superpowers/specs/2026-11-19-picture-setting-design.md`. Look and numbers: frames 8–9 and the "HOME menu: Picture switch in the bottom bar" rows of `docs/superpowers/specs/mockups/wii-settings-and-keyboard.html`.

## Global Constraints

- Fit is the default and draws exactly as today (`asp_fit` of the frame's own shape, full texture).
- Never stretch: the drawn rect always has the shape of the texture window shown.
- Mode values in `settings.toml`: `picture = "fit"`, `"14:9"` or `"fill"`; missing or unknown means Fit and never fails the parse.
- Saving follows the existing rule: only when `a->settings_saveable` (no SD partition with `wbfs/` or `games/`).
- The guide preview stays Fit; full screen uses the mode.
- House rules: match the sibling file; no comments in C source; no trailing whitespace; commits `wip | …` / `fix | …` / `test | …`, short, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- Both `make -C wii/tests test` and `make -C wii` stay clean (no warnings; host tests build with `-Wall -Wextra -Werror -pedantic`).
- Never run Dolphin without the owner's OK. Ask the owner before every wiiload send.

## Review Focus

1. HOME opened over the Settings screen, Picture changed, then Settings Save or Cancel: the new Picture must survive (Task 5, step "Settings keeps the switch's choice", and checklist item 67 (c)).
2. A hand-edited `settings.toml` with `picture = "FILL"`, `picture = 3` or a long string: Fit, with `source` and `server` kept (Task 2 test `settings_odd_picture_is_fit`).
3. No frame decoded yet, or a zero-sized frame: no crop, nothing drawn outside the box (Task 1 tests `shown_keeps_bad_aspect`, `crop_bad_aspect_is_whole_frame`).
4. The end of the preview zoom must match full screen exactly, so no jump (Task 1 test `shown_eases_with_amount`).
5. No SD card, or a card with `wbfs/`: the switch still works for the session and nothing is written (checklist item 67 (d)).

---

### Task 1: Crop math

**Files:**
- Modify: `wii/source/aspect.h`, `wii/source/aspect.c`
- Test: `wii/tests/test_aspect.c`

**Interfaces:**
- Produces:
  - `#define ASP_MODES 3`
  - `typedef enum { ASP_FIT, ASP_14_9, ASP_FILL } asp_mode;`
  - `float asp_shown(float src_aspect, bool widescreen, asp_mode mode, float amount);`: the shape to show. TV shape `T = (640/480) / asp_pixel_x(widescreen)`. Mode shape: Fit `a`; Fill `T`; 14:9 is `14/9` clamped between `min(a, T)` and `max(a, T)`. Result `a + (mode shape - a) * amount`, with `amount` clamped to 0..1. `src_aspect <= 0` returns `src_aspect` unchanged.
  - `asp_rect asp_crop(float src_aspect, float shown_aspect);`: the texture window in 0..1 units, centred. `shown < src`: `w = shown/src`, `x = (1-w)/2`, `y = 0`, `h = 1`. `shown > src`: `h = src/shown`, `y = (1-h)/2`, `x = 0`, `w = 1`. Equal, or either `<= 0`: `{0, 0, 1, 1}`.

- [ ] **Step 1: Write the failing tests** in `test_aspect.c` (EPS 0.01f as the file has), registered in `aspect_suite`:
  - `shown_on_4_3_tv_for_16_9_show`: `asp_shown(16/9, false, ASP_FIT, 1)` = 1.778; `ASP_14_9` = 1.556; `ASP_FILL` = 1.333.
  - `shown_leaves_matching_shows`: `asp_shown(4/3, false, m, 1)` = 1.333 and `asp_shown(16/9, true, m, 1)` = 1.778 for all three modes.
  - `shown_on_16_9_tv_for_4_3_show`: `asp_shown(4/3, true, ASP_FIT, 1)` = 1.333; `ASP_14_9` = 1.556; `ASP_FILL` = 1.778.
  - `shown_14_9_stays_between_show_and_tv`: `asp_shown(2.35, false, ASP_14_9, 1)` = 1.556; `asp_shown(1.2, false, ASP_14_9, 1)` = 1.333.
  - `shown_eases_with_amount`: for `asp_shown(16/9, false, ASP_FILL, k)`, `k = 0` gives 1.778, `k = 0.25` gives 1.667, `k = 1` gives 1.333, `k = 2` gives 1.333 and `k = -1` gives 1.778.
  - `shown_keeps_bad_aspect`: `asp_shown(0, false, ASP_FILL, 1)` = 0.
  - `crop_cuts_sides_of_16_9_on_4_3`: `asp_crop(16/9, 4/3)` = {0.125, 0, 0.75, 1}; `asp_crop(16/9, 14/9)` = {0.0625, 0, 0.875, 1}.
  - `crop_cuts_top_and_bottom_of_4_3_on_16_9`: `asp_crop(4/3, 16/9)` = {0, 0.125, 1, 0.75}; `asp_crop(4/3, 14/9)` = {0, 0.0714, 1, 0.857}.
  - `crop_bad_aspect_is_whole_frame`: `asp_crop(16/9, 16/9)`, `asp_crop(0, 4/3)` and `asp_crop(16/9, 0)` are each {0, 0, 1, 1}.
  - `fourteen_nine_fits_4_3_tv`: `asp_fit(14/9, false, 0, 0, 640, 480)` = {0, 34.29, 640, 411.43}.

- [ ] **Step 2: Run** `make -C wii/tests test`. Expected: build fails on the undeclared `asp_shown` / `asp_crop` / `ASP_*`.

- [ ] **Step 3: Implement** the enum, `ASP_MODES`, `asp_shown` and `asp_crop` in `aspect.h` / `aspect.c` as the Interfaces block says. Reuse `EFB_W` / `EFB_H` and `asp_pixel_x`; name the 14:9 value as a `#define` like `TV_WIDE`.

- [ ] **Step 4: Run** `make -C wii/tests test`. Expected: all suites pass, 0 failures.

- [ ] **Step 5: Commit** `wip | picture crop math` (aspect.h, aspect.c, test_aspect.c).

### Task 2: `picture` in settings.toml

**Files:**
- Modify: `wii/source/settings.h`, `wii/source/settings.c`, `docs/formats.md` (the `settings.toml` section)
- Test: `wii/tests/test_settings.c`, `wii/tests/test_srcpick.c` (initialisers only)

**Interfaces:**
- Consumes: `asp_mode` (Task 1). `settings.h` includes `"aspect.h"`.
- Produces: `ctw_settings` gains a third field `asp_mode picture;` after `server`. `settings_parse`, `settings_format` and `settings_equal` handle it; `settings_restarts` ignores it. `set_defaults` sets `ASP_FIT`.

- [ ] **Step 1: Add the third field to every existing initialiser.** Clang's `-Wmissing-field-initializers` (in `-Wextra`) rejects the old two-field ones once the field exists. All 46 are one-liners:

```bash
perl -pi -e 's/(ctw_settings \w+ = \{ SRC_\w+, "(?:[^"\\]|\\.)*") \};/$1, ASP_FIT };/' wii/tests/test_settings.c wii/tests/test_srcpick.c
```

- [ ] **Step 2: Write the failing tests** in `test_settings.c`, registered in `settings_suite`:
  - Update `settings_round_trip`: `in = { SRC_SERVER, "192.168.1.20:4747", ASP_14_9 }`. The formatted text is `"source = \"server\"\nserver = \"192.168.1.20:4747\"\npicture = \"14:9\"\n"`, and it parses back to `ASP_14_9`.
  - Update `settings_format_escapes_quotes`: the expected text gains `picture = \"fit\"\n` at the end.
  - `settings_picture_values`: parsing `picture = "fit"`, `"14:9"` and `"fill"` gives `ASP_FIT`, `ASP_14_9` and `ASP_FILL`; with no `picture` key it is `ASP_FIT`.
  - `settings_odd_picture_is_fit`: `"source = \"server\"\nserver = \"pc\"\npicture = \"FILL\"\n"`, then the same with `picture = 3`, then with `picture = "fill the whole screen please"`. Each returns 0 with `SRC_SERVER`, `"pc"` and `ASP_FIT`.
  - `settings_save_then_load` also round-trips `ASP_FILL` through the file.
  - `settings_equal_compares_picture`: `{ SRC_DRIVE, "", ASP_FIT }` vs `{ SRC_DRIVE, "", ASP_FILL }` is not equal.
  - `settings_picture_never_restarts`: `settings_restarts` is false for the same pair, and for a server pair that differs only in `picture`.

- [ ] **Step 3: Run** `make -C wii/tests test`. Expected: the new and updated tests fail.

- [ ] **Step 4: Implement.**
  - Parse `picture` with `read_string`. A failure there (non-string, too long) does not set `rc`; it just leaves the mode at Fit.
  - Map `"fit"`, `"14:9"` and `"fill"` exactly (case-sensitive); anything else is `ASP_FIT`.
  - Format writes `picture = "<name>"` as the third line, always.
  - Add `picture` to `settings_equal`.

- [ ] **Step 5: Document the key in `docs/formats.md`.**
  - Purpose line: "tells the Wii app where to play from (the drive or a server) and how to fit pictures that don't match the TV".
  - Key table row: `picture` | string | no | `"fit"` | `"fit"`, `"14:9"` or `"fill"`. Fit shows the whole picture with bars. 14:9 cuts 1/16 off each side of a 16:9 show on a 4:3 TV (1/14 off the top and bottom of a 4:3 show on a 16:9 TV) and leaves thin bars. Fill cuts 1/8 and leaves no bars. Any other value means Fit.
  - The writer line also names the HOME menu's Picture switch.
  - Add `picture = "fill"` to the complete example.

- [ ] **Step 6: Run** `make -C wii/tests test`. Expected: 0 failures.

- [ ] **Step 7: Commit** `wip | picture key in settings` (settings.h, settings.c, both test files, formats.md).

### Task 3: Draw the cut picture

**Files:**
- Modify: `wii/source/media/yuvgx.h`, `wii/source/media/yuvgx.c`, `wii/source/media/player.h`, `wii/source/media/player.c`, `wii/source/screens/full.c`, `wii/source/screens/guide_info.c`, `wii/source/main.c`

**Interfaces:**
- Consumes: `asp_shown`, `asp_crop`, `asp_fit`, `asp_mode` (Task 1); `ctw_settings.picture` (Task 2).
- Produces:
  - `void yg_draw(int slot, int frame_w, int frame_h, asp_rect window, float x, float y, float w, float h);`: texture coordinates run from `window.x * frame_w / tex_w` to `(window.x + window.w) * frame_w / tex_w`, and the same for t with `frame_h` / `tex_h`. `yuvgx.h` includes `"../aspect.h"`.
  - `void pl_set_picture(asp_mode mode);`: stored like `widescreen_s`.
  - `void pl_draw(float x, float y, float w, float h, float alpha, float crop);`: `crop` is 0..1 (0 = Fit, 1 = the full mode).

- [ ] **Step 1: Implement `yg_draw`'s window** (four texture coordinate pairs; the TEV setup is unchanged).

- [ ] **Step 2: Implement in `pl_draw`.**
  - `a = frame_w / frame_h` (0 when `frame_h` is 0).
  - `shown = asp_shown(a, widescreen_s, picture_s, crop)`.
  - `q = asp_fit(shown, widescreen_s, x, y, w, h)`.
  - `yg_draw(draw_slot, frame_w, frame_h, asp_crop(a, shown), q.x, q.y, q.w, q.h)`.
  - The black backing rect and the fade rect keep using `q`, as now.

- [ ] **Step 3: Update the call sites.**
  - `full.c`: `pl_draw(0, 0, 640, 480, 1.0f, 1.0f)`.
  - `guide_info.c` `ginfo_draw_preview`: pass `expand` as `crop`. It is 0 at rest and runs 0→1 on the zoom in and 1→0 on the way back.
  - `main.c` `boot()`: `pl_set_picture(a->settings.picture)` right after `settings_load`.

- [ ] **Step 4: Build** `make -C wii`. Expected: builds `wii/cable-twii.dol` with no warnings. Then `make -C wii/tests test`: 0 failures.

- [ ] **Step 5: Commit** `wip | draw the picture setting's cut`.

### Task 4: HOME switch layout

**Files:**
- Modify: `wii/source/home_layout.h`, `wii/source/home_layout.c`
- Test: `wii/tests/test_home_layout.c`

**Interfaces:**
- Consumes: `ASP_MODES`, `asp_mode` (Task 1).
- Produces:
  - `home_btn` gains `HOME_BTN_PIC_FIT, HOME_BTN_PIC_14_9, HOME_BTN_PIC_FILL`, consecutive and in `asp_mode` order, so part `i` is `HOME_BTN_PIC_FIT + i`.
  - `home_layout` gains `asp_rect pic_label, pic_track; asp_rect pic[ASP_MODES]; asp_rect pic_thumb[ASP_MODES];`.
  - `home_hit` returns the part's button inside `pic[i]`.

Numbers (from the mockup; the same on both TVs except the thumb's x inset):
- `pic_label` 265, 436.4, 49, 16
- `pic_track` 322, 426.9, 168, 32
- `pic[i]` 322 + 56·i, 426.9, 56, 32
- `pic_thumb[i]` is `pic[i]` inset by 3·pixel_x left and right and 3 top and bottom

- [ ] **Step 1: Write the failing tests**, registered in `home_layout_suite`:
  - `home_picture_switch_matches_the_mockup`: the numbers above at pixel_x 1.0 and 0.75. At 0.75, `pic_thumb[1]` is {380.25, 429.9, 51.5, 26}.
  - `home_picture_switch_sits_in_the_bottom_bar`: label, track and parts are `within` `bottom`. `vol_bar.x + vol_bar.w < pic_label.x`, `pic_label.x + pic_label.w <= pic_track.x`, and `pic_track.x + pic_track.w < battery_label.x`. The parts are `apart` from each other and tile the track exactly.
  - `hit_finds_each_picture_part`: the centre of `pic[i]` hits `HOME_BTN_PIC_FIT + i`, in both server and drive mode. A point on `pic_label` hits `HOME_BTN_NONE`.

- [ ] **Step 2: Run** `make -C wii/tests test`. Expected: fails to build on the new names.

- [ ] **Step 3: Implement** the rects as named `#define`s in `home_layout.c` and the hits in `home_hit`.

- [ ] **Step 4: Run** `make -C wii/tests test`. Expected: 0 failures.

- [ ] **Step 5: Commit** `wip | home picture switch layout`.

### Task 5: The switch in the HOME menu

**Files:**
- Modify: `wii/source/ui/draw.h`, `wii/source/ui/draw.c`, `wii/source/screens/settings_screen.c`, `wii/source/screens/homemenu.c`, `wii/source/app.h`, `wii/source/main.c`, `docs/USER-GUIDE.md`, `CHANGELOG.md`, `docs/testing/wii-checklist.md`

**Interfaces:**
- Consumes: Tasks 1–4.
- Produces:
  - `void ui_segment(asp_rect part, asp_rect thumb, const char *label, int px, bool chosen, ui_pill_state state);` in `ui/draw.c` (`draw.h` includes `"../aspect.h"`). It holds today's `source_half` body from `settings_screen.c`: label centred in `part` at `px`, thumb drawing, 2 px press offset, pill shadow (dy 1, blur 4), 2 px accent-dark border.
  - `void app_set_picture(app *a, asp_mode mode);` in `main.c` / `app.h`.

- [ ] **Step 1: Move `source_half`'s drawing into `ui_segment`.** `settings_screen.c` calls `ui_segment(half, thumb, label, LABEL_PX, chosen, state_for(btn))`. Drop constants from `settings_screen.c` that are no longer used there.

- [ ] **Step 2: Implement `app_set_picture`.**
  - Return if the mode is unchanged.
  - Otherwise set `a->settings.picture` and call `pl_set_picture`.
  - When `a->settings_saveable`, call `settings_save(SETTINGS_DIR, &a->settings)`; on failure set `a->settings_saveable = false`. Nothing is shown.

- [ ] **Step 3: HOME menu (`homemenu.c`).**
  - `choose()` calls `app_set_picture` for the three new buttons and leaves the menu open.
  - `home_draw` draws `"Picture"` (bold 13, `THEME_TEXT_LABEL`) at `pic_label`.
  - It draws the track at `pic_track` as `draw_settings` draws `l->track` (`THEME_TRACK_INSET`, then `THEME_BAR_TRACK` 1 px lower, full radius).
  - It draws one `ui_segment(pic[i], pic_thumb[i], label, 13, a->settings.picture == i, state_for(HOME_BTN_PIC_FIT + i))` per part. The labels are `"Fit"`, `"14:9"` and `"Fill"`.
  - Hover sound, rumble and act-on-release already come from `home_update`.

- [ ] **Step 4: Settings keeps the switch's choice.** The first line of `save()` in `settings_screen.c` is `edited.picture = a->settings.picture;`, so a change made from HOME over the Settings screen is neither undone nor counted as a Settings change.

- [ ] **Step 5: Build** `make -C wii` (no warnings) and `make -C wii/tests test` (0 failures).

- [ ] **Step 6: Docs.**
  - `docs/USER-GUIDE.md` section 13, **HOME menu**, gets a second bullet: "The bottom bar has **Picture**: **Fit** shows the whole picture with black bars when a show doesn't match the TV's shape. **14:9** cuts a little off the sides (or the top and bottom) and leaves thin bars. **Fill** cuts more and fills the screen. On a 4:3 TV, Fill cuts 1/8 off each side of a 16:9 show. Nothing is ever stretched. Point at one and press **A**; the picture changes at once. The guide's preview always shows the whole picture. The choice is saved in `settings.toml` on the SD card, with the same rules as Settings (section 15)."
  - `CHANGELOG.md` "Unreleased" gets a top line: "Wii app: a Picture switch in the HOME menu's bottom bar. Fit (as before), 14:9 or Fill lets 16:9 shows fill a 4:3 TV by cutting the sides instead of showing black bars, never by stretching. Saved in `settings.toml`. Not yet run on a real Wii."
  - `docs/testing/wii-checklist.md` gets a new `### Picture setting` section after Server source, in the same table format. It holds item **67 Picture switch**:
    - Steps: (a) Wii at 4:3 and 50 Hz, watch a 16:9 show full screen, HOME, pick 14:9 then Fill then Fit. (b) Pick Fill, quit, start again. (c) Open Settings, then HOME over it, pick 14:9, Resume, then Save; repeat with Cancel. (d) No SD card, or a card with `wbfs/`, pick Fill. (e) Watch a 4:3 show on Fill. (f) Zoom from the guide preview to full screen and back on Fill.
    - Expected: (a) the picture behind the dim changes at once: thin bars with a little cut off the sides, then full screen with 1/8 cut off each side, then the letterbox as before. Nothing is stretched, and the pointed part rings with the hover sound. (b) Fill again, and `settings.toml` has `picture = "fill"`. (c) 14:9 stays after both. (d) it works until cable-twii closes and nothing is written to the card. (e) unchanged. (f) the preview shows the whole picture and the cut grows smoothly during the zoom, with no jump at either end.

- [ ] **Step 7: Commit** `wip | picture switch in the home menu` (code and the three docs).

### Task 6: Verify on the Wii

- [ ] **Step 1: Run** `make -C wii/tests test` and `make -C wii`. Expected: 0 failures, no warnings. `git diff master --stat` shows no stray files, and `grep -rn '[[:space:]]$'` over the changed files finds nothing.
- [ ] **Step 2: Ask the owner** to open the Homebrew Channel and OK a send. Then run `WIILOAD=tcp:192.168.1.155 /opt/devkitpro/tools/bin/wiiload wii/cable-twii.dol`. A wiiload copy isn't saved: reopening from the Homebrew Channel runs the SD card's version.
- [ ] **Step 3: Walk checklist item 67 with the owner** and record the results they report. Anything wrong goes back as a `fix | …` commit, with another send only after asking again.
