# cable-twii Wii Redesign Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Every Wii screen draws as the redesign deck shows it: glossy cards with highlights and shadows, a curved system bar, accent-gloss parts, pressable pills, and the new guide, OSD, chip, volume, up-next, HOME, intro, empty and error layouts.

**Architecture:** The UI kit (`wii/source/ui/draw.c`) grows the new parts (gradients, shadows, rings, cards, pill states, bars, segments, number pills, battery, radial bloom, glyphs, dashes) on top of GRRLIB with a precomputed corner table. Geometry lives in host-tested layout modules (`guide_layout`, new `sysbar_layout`, `osd_layout`, `home_layout`, `logoanim`) that take `pixel_x`, and screens only draw from those rects. Pills act on release through a host-tested press-state module.

**Tech Stack:** C11 on devkitPPC/libogc/GRRLIB 4.6.1, greatest for host tests, Pillow for the generated PNGs, Dolphin for visual checks (`scratchpad/dolphin-shot.sh` in this session; the owner uses the real Wii).

**Spec:** `docs/superpowers/specs/2026-10-22-wii-redesign-design.md` (the deck it maps is linked from it).

## Global Constraints

- 640×480 frame; all values from the spec are Wii pixels; squares are height × `ui_pixel_x()` wide; parts right of a square flow from its real edge.
- Palette and type unchanged: `#34BEED` accent, `#444444`/`#999999` text, M PLUS Rounded 1c Medium (`FONT_BODY`) and ExtraBold (`FONT_BOLD`).
- Timings unchanged: logo 2500 ms (600/1400/2000 phases), hover debounce 300 ms, chip 2000 ms, OSD 3000 ms, preview expand 350 ms; new: chip ring 600 ms, name pill 200 ms, intro dot 300 ms, volume panel 2000 ms.
- No comments in source, no trailing whitespace, `-Wall -Wextra -Werror -pedantic` host build must stay clean, `make -C wii` must build.
- Commits: `wip | …` for new drawing, `fix | …` for fixes, `test | …` for tests alone, each ending with `Co-Authored-By: Claude Fable 5.1 <noreply@anthropic.com>`.
- No Nintendo assets; glyphs are drawn by `tools/make-glyphs.py`.

## Review Focus

- A channel number of three digits ("128"): the row's number pill, the chip tile and the OSD number pill must widen, never clip (Task 2 `number pill width`, Task 7 `chip_grows_with_the_number`).
- A 4:3 console (`pixel_x` 1.0): the HOME-menu title, the system-bar label and the chip icon must not overlap the disc or tile to their left (Task 5 `sysbar_4_3_label_clears_disc`, Task 7 `chip_4_3_icon_clears_tile`, Task 9 `home_4_3_title_clears_badge`).
- A gap whose next airing is a commercial break: the up-next card must still name the next episode and count to it, and show the faded pill (Task 4 `next_episode_skips_ads`, Task 8 manual check).
- A and the pointer: pressing A on a pill, dragging off it and releasing must not fire; pressing elsewhere and releasing on a pill must not fire (Task 3 `press_release_off_pill_does_not_fire`, `press_started_elsewhere_does_not_fire`).
- The last airing of the day: "Up next" columns must show the label alone without a crash (Task 4 `next_episode_none_at_end`).

---

### Task 1: Tokens and generated art

**Files:**
- Create: `tools/make-glyphs.py`, `wii/data/glyph_home.png`, `wii/data/glyph_play.png`
- Modify: `wii/data/cursor.png` (regenerated), `wii/source/ui/theme.h`

**Interfaces:**
- Produces: the theme tokens of the spec's token table, named exactly: `THEME_GLOSS_BOTTOM 0xF8FAFCFFu`, `THEME_GLOSS_SOFT 0xF4F7FAFFu`, `THEME_GLOSS_COOL 0xF1F4F7FFu`, `THEME_GLOSS_DEEP 0xE9EDF1FFu`, `THEME_PANEL_HIGHLIGHT 0xFFFFFFFFu`, `THEME_PANEL_SHADOW 0x344A601Au`, `THEME_PANEL_SHADOW_DY 2.0f`, `THEME_PANEL_SHADOW_BLUR 5.0f`, `THEME_VIDEO_SHADOW 0x00000052u`, `THEME_VIDEO_SHADOW_DY 5.0f`, `THEME_VIDEO_SHADOW_BLUR 16.0f`, `THEME_ACCENT_LIGHT 0x7FD0EFFFu`, `THEME_ACCENT_DARK 0x2A8FB3FFu`, `THEME_RING 0x34BEED40u`, `THEME_RING_STRONG 0x34BEED99u`, `THEME_RING_SPREAD 4.0f`, `THEME_PILL_BAND_MID 0xEEF1F4FFu`, `THEME_PILL_BAND_BOTTOM 0xE6EAEEFFu`, `THEME_PILL_SHADOW 0x344A601Fu`, `THEME_PILL_SHADE 0x344A6033u`, `THEME_TRACK_INSET 0xC7CED4FFu`, `THEME_SCRIM 0x0000008Cu`, `THEME_HINT_FILL 0x14232F9Eu`, `THEME_HINT_DISC_BOTTOM 0xE6EAEEFFu`, `THEME_TEXT_CLOCK 0x777788FFu`, `THEME_TEXT_LABEL 0x555555FFu`, `THEME_TEXT_FADED 0xAAAAAAFFu`, `THEME_WARM 0xF3CF8AFFu`, `THEME_STRIPE_DARK_A 0x262626FFu`, `THEME_STRIPE_DARK_B 0x222222FFu`, `THEME_ROW_HOT_TOP 0xF7FDFFFFu`, `THEME_ROW_HOT_MID 0xEEFAFFFFu`, `THEME_ROW_HOT_BOTTOM 0xE3F6FDFFu`, `THEME_ROW_HOT_SHADOW 0x34BEED33u`, `THEME_DISC_MID 0xF1F4F7FFu`, `THEME_DISC_EDGE 0xDFE5EAFFu`, `THEME_DOT_2 0x7FD0EFFFu`, `THEME_DOT_3 0xBFE6F6FFu`, `THEME_HEADER_H 48.0f`, `THEME_SYSBAR_H 47.0f`, `THEME_SYSBAR_HUMP 18.0f`, `THEME_SYSBAR_HUMP_TOP_W 79.0f`, `THEME_SYSBAR_HUMP_SLOPE_W 47.0f`, `THEME_CHIP_GLOW_MS 600u`, `THEME_CHIP_NAME_MS 200u`, `THEME_INTRO_DOT_MS 300u`, `THEME_VOLUME_MS 2000u`.
- Produces: `glyph_home.png` and `glyph_play.png`, 48×48 RGBA, white strokes on transparent (house: a roof chevron over a body with a door gap; play: an outlined rounded triangle), 4 px strokes; `cursor.png` 32×32 as the spec's glossy disc.

- [x] **Step 1: Write `tools/make-glyphs.py`** with Pillow (supersample 4×, LANCZOS down), writing the three PNGs; run it: `python3 tools/make-glyphs.py`.
- [x] **Step 2: Verify** `file wii/data/glyph_home.png wii/data/glyph_play.png wii/data/cursor.png` → three `48 x 48` / `32 x 32` RGBA PNGs; view them.
- [x] **Step 3: Add the tokens to `theme.h`**; keep every existing token.
- [x] **Step 4: Build** `make -C wii` → `wii/cable-twii.dol` links (the new PNGs become `glyph_home_png.h`, `glyph_play_png.h` through bin2o).
- [x] **Step 5: Commit** `wip | redesign tokens, glyphs and glossy cursor`.

---

### Task 2: Draw kit

**Files:**
- Modify: `wii/source/ui/draw.c`, `wii/source/ui/draw.h`
- Create: `wii/source/ui/glyphs.c`, `wii/source/ui/glyphs.h`

**Interfaces:**
- Consumes: Task 1 tokens and PNG headers.
- Produces (all in `draw.h`, coordinates in Wii px, radii vertical with the horizontal radius scaled by `pixel_x`):
  - `typedef struct { float pos; uint32_t color; } ui_stop;`
  - `typedef enum { UI_SHADOW_NONE, UI_SHADOW_PANEL, UI_SHADOW_VIDEO } ui_shadow_kind;`
  - `typedef enum { UI_PILL_REST, UI_PILL_HOT, UI_PILL_PRESSED } ui_pill_state;`
  - `void ui_gradient_rect(float x, float y, float w, float h, uint32_t top, uint32_t bottom);` one `GRRLIB_GXEngine` quad.
  - `void ui_rounded_gradient(float x, float y, float w, float h, float radius, const ui_stop *stops, int n);` corners from a 12-step quarter table (`static const float CORNER_COS[13], CORNER_SIN[13]` filled once), bands between stops.
  - `void ui_shadow(float x, float y, float w, float h, float radius, uint32_t color, float dy, float blur);` three rounded rects at blur×⅓, ⅔, 1 with alpha ⅓ each of the colour's alpha.
  - `void ui_shadow_kind(float x, float y, float w, float h, float radius, ui_shadow_kind kind, float alpha_scale);`
  - `void ui_ring(float x, float y, float w, float h, float radius, uint32_t color, float spread);`
  - `void ui_card(float x, float y, float w, float h, float radius, float mid, uint32_t bottom, ui_shadow_kind shadow);` border `THEME_PANEL_BORDER` 2 px, highlight 1 px, gradient white→white at `mid`→`bottom` (mid ≤ 0 means two stops).
  - `void ui_card_alpha(...same..., float alpha);` for the OSD's 97%.
  - `void ui_pill_ex(float x, float y, float w, float h, const char *label, int px, ui_pill_state state, ui_shadow_kind shadow);` and `ui_pill` kept as `ui_pill_ex(..., 14, hot ? UI_PILL_HOT : UI_PILL_REST, UI_SHADOW_PANEL)`.
  - `void ui_bar(float x, float y, float w, float h, float fraction);` recessed track + accent-gloss fill (same signature as today).
  - `void ui_segments(float x, float y, float w, float h, int count, float gap, int lit, bool ring_newest);`
  - `float ui_number_pill_w(const char *text, int px);` = max(22, text width + 10).
  - `void ui_number_pill(float x, float y, float h, const char *text, int px, bool tuned);`
  - `void ui_accent_tile(float x, float y, float w, float h, float radius);`
  - `void ui_battery(float x, float y, float pill_h, int lit, int label_px, float *out_w);` draws "P1", the pill and four bars; header size when `pill_h` is 22, HOME size when 27.
  - `void ui_radial(float cx, float cy, float rx, float ry, uint32_t color);` 64×64 alpha texture, alpha 1 at the centre to 0 at the rim (smoothstep).
  - `void ui_disc(float x, float y, float h, uint32_t inner, uint32_t mid, uint32_t edge);` flat `edge` disc, then `ui_radial` in `mid`, then a smaller `ui_radial` in `inner`.
  - `void ui_dashed_rounded(float x, float y, float w, float h, float radius, float thickness, float dash, float gap, uint32_t color);`
  - `void ui_highlight_line(float x, float y, float w, float h, float radius, uint32_t color);` the 1 px top highlight following the corners.
  - `glyphs.h`: `typedef enum { GLYPH_HOME, GLYPH_PLAY } ui_glyph; void glyphs_init(void); void ui_glyph_draw(ui_glyph g, float x, float y, float h, uint32_t color);` (width = h × `pixel_x`).
  - Keeps: `ui_set_widescreen`, `ui_pixel_x`, `ui_stripes`, `ui_stripes_rect`, `ui_panel`, `ui_glow` (now `ui_ring` with `THEME_RING`'s alpha scaled by the argument), `ui_rounded`, `ui_corner_mask`, `ui_image`, `ui_load_png_file`, `ui_text*`.
  - Adds `void ui_stripes_colors(uint32_t a, uint32_t b);` for the dark stripes.

- [x] **Step 1: Implement the primitives** in `draw.c`; the rounded corner fan draws `GX_TRIANGLEFAN` through `GRRLIB_GXEngine` with the table; `ui_rounded` and `ui_panel` switch to the table too.
- [x] **Step 2: Implement `glyphs.c`** loading the two PNGs once (`GRRLIB_LoadTexture`), drawing with `GRRLIB_DrawImg` scaled to `h`, tinted by `color`.
- [x] **Step 3: Build** `make -C wii` → clean build (`-Wall`), no behaviour change yet except the glossier `ui_pill`/`ui_bar`.
- [x] **Step 4: Dolphin smoke** `dolphin-shot.sh … wait:14 shot:guide` → the guide draws, pills and bars glossy, nothing missing.
- [x] **Step 5: Commit** `wip | ui kit: gradients, shadows, rings, cards, pill states, glyphs`.

---

### Task 3: Press state and A release

**Files:**
- Create: `wii/source/ui/press.c`, `wii/source/ui/press.h`, `wii/tests/test_press.c`
- Modify: `wii/source/input.c`, `wii/source/input.h`, `wii/tests/Makefile` (add `../source/ui/press.c`), `wii/tests/main.c` (`press_suite`)

**Interfaces:**
- Produces: `typedef struct { int armed; } ps_state;` `int ps_update(ps_state *s, int hot, bool pressed, bool held, bool released);` returns the id fired this frame (0 otherwise); `int ps_pressed(const ps_state *s, int hot);` returns `hot` when the pill should draw pressed (armed on that id and held). `bool in_released(uint32_t wpad_button);` (`WPAD_ButtonsUp`, blocked like the others).

- [x] **Step 1: Write failing tests** in `test_press.c`:
  - `press_fires_on_release_over_same_pill`: `ps_update(&s, 2, true, true, false)` → 0; `ps_update(&s, 2, false, true, false)` → 0; `ps_update(&s, 2, false, false, true)` → 2; `ps_pressed(&s, 2)` → 0 afterwards.
  - `press_release_off_pill_does_not_fire`: press on 2, then `ps_update(&s, 0, false, false, true)` → 0 and `armed` cleared.
  - `press_started_elsewhere_does_not_fire`: `ps_update(&s, 0, true, true, false)`; `ps_update(&s, 3, false, false, true)` → 0.
  - `pressed_only_while_held_on_armed_pill`: after press on 2, `ps_pressed(&s, 2)` → 2, `ps_pressed(&s, 1)` → 0.
- [x] **Step 2: Run** `make -C wii/tests` → FAIL (undefined `ps_update`).
- [x] **Step 3: Implement** `press.c` and `in_released`.
- [x] **Step 4: Run** `make -C wii/tests` → PASS, all suites.
- [x] **Step 5: Commit** `wip | pills press on hold and act on release`.

---

### Task 4: Text and tuner helpers

**Files:**
- Modify: `wii/source/guide_text.c`, `wii/source/guide_text.h`, `wii/source/tuner.c`, `wii/source/tuner.h`, `wii/tests/test_guide_text.c`, `wii/tests/test_tuner.c`

**Interfaces:**
- Produces: `void gt_date(ctw_date d, char *out, size_t out_size);` → "Thu 22/10" (English three-letter weekday, Sakamoto's method, `%02d/%02d`); `void gt_number(int number, char *out, size_t out_size);` → `%02d` ("01", "128"); `void gt_countdown(uint32_t ms_left, char *out, size_t out_size);` → "M:SS" with minutes unbounded ("4:32", "566:23"); `void gt_at(int64_t start_ms, char *out, size_t out_size);` → "at 14:41"; `const ctw_airing *tn_next_episode(tuner *t, int channel, const ctw_airing *a);` first airing after `a` whose kind is `CTW_AIR_EPISODE`, at most 64 steps, else NULL; `int64_t tn_ms_until(const tuner *t, int channel, const ctw_airing *from, uint32_t offset_ms, const ctw_airing *to);` milliseconds from now (`from` at `offset_ms`) to the start of `to` using `tn_airing_start`, 0 when either start is unknown.

- [x] **Step 1: Write failing tests**: `gt_date_formats_weekday` (`{2026,10,22}` → "Thu 22/10"; `{2026,1,1}` → "Thu 01/01"; `{2024,2,29}` → "Thu 29/02"), `gt_number_pads_to_two` ("01", "12", "128"), `gt_countdown_minutes_unbounded` (272000 → "4:32"; 33983000 → "566:23"; 0 → "0:00"), `gt_at_formats` ("at 14:41"); in `test_tuner.c` `next_episode_skips_ads` (from channel 1's first Gumball at 07:00 the next episode is the second Gumball, and after the 3 Gumballs the next episode is an Adventure Time one, skipping the ad), `next_episode_none_at_end` (from the last airing of the day → NULL), `ms_until_next_episode` (from the third Gumball at offset 60 s, `tn_ms_until` to the next episode equals the remaining Gumball time plus the ad's 30 s).
- [x] **Step 2: Run** → FAIL.
- [x] **Step 3: Implement.**
- [x] **Step 4: Run** → PASS.
- [x] **Step 5: Commit** `wip | date, countdown and next-episode helpers`.

---

### Task 5: Guide grid, system bar and header geometry

**Files:**
- Create: `wii/source/sysbar_layout.c`, `wii/source/sysbar_layout.h`, `wii/tests/test_sysbar_layout.c`
- Modify: `wii/source/guide_layout.c`, `wii/source/guide_layout.h`, `wii/source/logoanim.c`, `wii/source/logoanim.h`, `wii/tests/test_guide_layout.c`, `wii/tests/test_logoanim.c`, `wii/tests/Makefile`, `wii/tests/main.c`

**Interfaces:**
- Produces: `GL_HEADER_H` becomes `THEME_HEADER_H` (48), `GL_INFO_MIN_H 101.8f`, `GL_BOTTOM 409.8f` (= 480 − 47 − 18 − 5.2), `LIST_COL 0.447f`, `GAP_COL 0.032f`, `GAP_ROW 0.04f`; `guide_layout_for(float pixel_x)` unchanged in signature.
- `sysbar_layout.h`: `typedef enum { SYSBAR_BTN_NONE, SYSBAR_BTN_HOME, SYSBAR_BTN_PREV, SYSBAR_BTN_NEXT } sysbar_btn;` `typedef struct { asp_rect body; asp_rect hump; float slope_w; asp_rect clock; asp_rect date; asp_rect home_disc; asp_rect home_label; asp_rect home_hit; asp_rect prev; asp_rect next; } sysbar_layout;` `sysbar_layout sysbar_layout_for(float pixel_x);` `sysbar_btn sysbar_hit(const sysbar_layout *l, float x, float y, bool channels);` `float sysbar_top_at(const sysbar_layout *l, float x);` (the outline's y at `x`: body top 433, hump top 415, cubic ease across each slope). Values: body 0,433 640×47; hump x 280.7 w 79 y 415; home_disc 21.3,437.3 (37 × 37·pixel_x); home_label x = disc right + 6.3, y 449, w 90, h 15; home_hit spans disc and label; prev 496.7,440.9 58×32; next 560.7,440.9 58×32; clock 253.3,420 133.3×28; date 253.3,453 133.3×15.
- `logoanim.h`: `LOGO_HEADER_BAND_H THEME_HEADER_H`, `LOGO_HEADER_H 25.0f`, `LOGO_HEADER_X 19.3f`.

- [x] **Step 1: Update `test_guide_layout.c`** to the new values, computed from the spec: inset 19.2 (`640×0.03`), top 58.7, bottom 409.8, gw 601.6, list w 268.9, col gap 19.25, rx 307.35, rw 313.45, gh 351.1, row gap 14.04. 16:9 (`pixel_x` 0.75): ph = 313.45/0.75×9/16 = 235.1 (fits under the 235.3 cap), info y 307.8, info h 102.0. Assert `list.x 19.2`, `list.y 58.7`, `list.w 268.9`, `list.h 351.1`, `preview.x 307.35`, `preview.w 313.45`, `preview.h 235.1`, `info.y 307.8`, `info.h 102.0`. 4:3 (`pixel_x` 1.0): `preview.h 176.3`, `info.h 160.8`, `info.w 313.45`. Use `EPS 0.2f`.
- [x] **Step 2: Write `test_sysbar_layout.c`**: `sysbar_body_and_hump` (body y 433, h 47; hump y 415, w 79, centred at 320), `sysbar_top_follows_the_curve` (`sysbar_top_at` = 433 at x 100, 415 at x 320, between at x 257 and 383, and monotone across each slope), `sysbar_hit_finds_each_button` (centres → HOME/PREV/NEXT, `channels` true), `sysbar_hides_channel_pills` (`channels` false → PREV/NEXT centres give NONE), `sysbar_4_3_label_clears_disc` (`pixel_x` 1.0 → `home_label.x ≥ home_disc.x + home_disc.w`; 16:9 → `home_label.x` ≈ 55.3 ± 1), `sysbar_hit_misses_the_clock`.
- [x] **Step 3: Update `test_logoanim.c`** header rect: `logo_header_rect_for(256, 64)` → x 19.3, y 11.5, w 100, h 25; adjust the four `19.2f/8.0f/112.0f/28.0f` expectations throughout.
- [x] **Step 4: Run** → FAIL on the three suites.
- [x] **Step 5: Implement** `guide_layout.c`, `sysbar_layout.c`, `logoanim.c` constants.
- [x] **Step 6: Run** → PASS.
- [x] **Step 7: Commit** `wip | guide grid, system bar and header geometry`.

---

### Task 6: Guide screens

**Files:**
- Create: `wii/source/screens/sysbar.c`, `wii/source/screens/sysbar.h`, `wii/source/screens/header.c`, `wii/source/screens/header.h`, `wii/source/screens/empty.c`, `wii/source/screens/empty.h`
- Modify: `wii/source/screens/guide.c`, `wii/source/screens/guide_list.c`, `wii/source/screens/guide_info.c`, `wii/source/screens/logo.c` (header draw moves), `wii/source/main.c` (`glyphs_init`)

**Interfaces:**
- Consumes: Tasks 2–5.
- Produces: `void header_draw(app *a, float logo_alpha);` (bar, shadow, logo via `logo_draw_header`, battery); `void sysbar_draw(app *a, const sysbar_layout *l, bool channels, sysbar_btn hot, sysbar_btn pressed, float alpha);` `sysbar_btn sysbar_update(app *a, const sysbar_layout *l, bool channels, ps_state *ps);` returns the button fired this frame after playing the tones; `void empty_draw(app *a, ps_state *ps);` and `void empty_update(app *a, ps_state *ps);` (pill quits to the Homebrew Channel); `glist_draw` gains `int tuned_channel` and draws the scrollbar; `ginfo_draw_info` draws the new layout; `ginfo_draw_preview` draws the hint.
- The guide's `draw_body` draws, in order: stripes, white bloom, header, list panel, preview, info, system bar (cached texture, drawn with `ui_image`), and keeps the intro slide.

- [x] **Step 1: Implement** the files per the spec sections "Header", "System bar", "Guide", "Empty state". The system bar texture: 640×66 (`GRRLIB_CreateEmptyTexture`), filled once through `GRRLIB_CompoStart`/`GRRLIB_CompoEnd` by drawing the strip at y 0 (slices every 2 px along `sysbar_top_at`), then `ui_image` at y 414.
- [x] **Step 2: Build** `make -C wii`.
- [x] **Step 3: Dolphin**: `shot:guide` matches slide 3 (compare side by side with `.playwright-mcp/slide03.png`); `key:125` moves the pointed row; pointer on "CH ▶" hot, held A pressed, release moves the highlight; HOME disc opens the HOME menu; an empty root (`config.toml` with no channels) shows the empty state.
- [x] **Step 4: Commit** `wip | guide redesign: header, glossy list, preview hint, info panel, system bar, empty state`.

---

### Task 7: Full screen: OSD, chip, volume

**Files:**
- Modify: `wii/source/osd.c`, `wii/source/osd.h`, `wii/source/osd_layout.c`, `wii/source/osd_layout.h`, `wii/source/screens/panel.c`, `wii/source/screens/panel.h`, `wii/source/screens/full.c`, `wii/tests/test_osd.c`, `wii/tests/test_osd_layout.c`, `docs/testing/wii-checklist.md`

**Interfaces:**
- `osd.h`: adds `float osd_chip_glow(const osd_state *s, uint32_t now_ms);` (1 → 0 over `THEME_CHIP_GLOW_MS` after a change), `float osd_chip_name_drop(const osd_state *s, uint32_t now_ms);` (ease-out 0 → 1 over `THEME_CHIP_NAME_MS`), `bool osd_volume_visible(const osd_state *s, uint32_t now_ms);` (replaces `osd_volume_alone`), `bool osd_panel_visible(const osd_state *s, uint32_t now_ms);` (= `osd_visible && !osd_volume_visible`). `osd_volume_changed` also extends the OSD's own timer (activity).
- `osd_layout.h`: `osd_btn` keeps only `OSD_BTN_NONE, OSD_BTN_VOL_DOWN, OSD_BTN_VOL_UP`; `osd_layout` fields: `panel, icon, name, number, line, start, bar, end, divider, next_icon, next_label, next_title, next_at, countdown, vol_label, vol_panel, vol_down, vol_bar, vol_up`; `osd_layout osd_layout_for(float offset_y, float pixel_x, float name_w, float start_w);` (the number pill follows the name's width, the bar follows the start time's width); `typedef struct { asp_rect chip; asp_rect tile; asp_rect icon; asp_rect name_pill; } osd_chip;` `osd_chip osd_chip_layout(float number_w, float name_w, float pixel_x);` `osd_btn osd_hit(const osd_layout *l, float x, float y, bool volume_visible);` `osd_segment`/`osd_segments_lit` kept; `asp_rect osd_scrim(void);` → 0,248.9 640×231.1.

- [x] **Step 1: Rewrite `test_osd_layout.c`**: `layout_panel_sits_at_the_bottom` (unchanged values), `layout_offset_moves_everything_down`, `layout_contents_stay_inside_the_panel` (all fields except vol_*), `layout_number_pill_follows_the_name` (name_w 100 → `number.x = 86.7 + 100 + 5.3` relative), `layout_bar_follows_the_start_time` (`bar.x = start.x + start_w + 5.9`, `bar.x + bar.w = panel.x + 352`), `hit_only_volume_pills_and_only_while_visible`, `segments_tile_the_volume_bar` (vol_bar 185.3 wide, 10 segments gap 3), `standalone_volume_is_centred_above_the_bottom` (panel 190,412 260×44; vol_label above at y 387), `chip_sits_in_the_top_right_corner` (`chip.x + chip.w = 628`, y 16, h 57.8, tile 40 tall at +7.3,+8.9), `chip_grows_with_the_number` (number_w 60 → tile w 72, chip wider by 42 than number_w 18), `chip_4_3_icon_clears_tile` (`pixel_x` 1 → `icon.x ≥ tile.x + tile.w`), `chip_name_pill_hangs_right_aligned` (`name_pill.x + name_pill.w = chip.x + chip.w`, y 84.4, h 28.4, w = name_w + 28 min 60).
- [x] **Step 2: Update `test_osd.c`**: rename `volume_bar_shows_alone_for_two_seconds` → `volume_panel_shows_for_two_seconds` (`osd_volume_visible`), `volume_bar_yields_to_panel` → `panel_yields_to_volume` (`osd_panel_visible` false while the volume shows, true after), add `chip_glow_fades_in_600ms` (1.0 at change, 0.5 at +300, 0 at +600) and `chip_name_drops_in_200ms` (0 at change, 0.875 at +100, 1 at +200).
- [x] **Step 3: Run** → FAIL.
- [x] **Step 4: Implement** `osd.c`, `osd_layout.c`; rewrite `panel.c` (`panel_draw`, `panel_draw_volume(const osd_layout *l, int volume, osd_btn hot, osd_btn pressed)`), and `full.c`: scrim, chip with glow and name pill, volume panel pills through `ps_update`, `osd_hit` only on the volume pills; remove the pill row.
- [x] **Step 5: Run** `make -C wii/tests` → PASS; `make -C wii` → builds.
- [x] **Step 6: Dolphin**: `click` a row, `shot:full` (chip + name pill, ring fading), move the pointer, `shot:osd` matches slide 4, `key:14`, `shot:volume` matches slide 5 (seven lit, newest ringed), hold A on "+" → pressed, release → eight lit.
- [x] **Step 7: Checklist**: rewrite the "clicking CH ▶ flips" item to "the OSD panel has no buttons; the pointer presses only − and + on the volume panel"; add "chip name pill drops in and the ring fades".
- [x] **Step 8: Commit** `wip | fullscreen redesign: glass osd, chip with name, glossy volume`.

---

### Task 8: Up-next screen

**Files:**
- Modify: `wii/source/screens/upnext.c`, `wii/source/screens/upnext.h`, `wii/source/screens/full.c` (call site passes `next_episode` and `ms_until`), `wii/source/screens/guide_info.c` (the preview's gap text uses `gt_countdown`)

**Interfaces:**
- `void upnext_draw(app *a, const ctw_airing *gap, uint32_t gap_offset_ms, const ctw_airing *next, const ctw_airing *next_episode, int64_t ms_until_episode);` draws the dark stripes, bloom, card, and the faded pill when `next != NULL && next->kind == CTW_AIR_AD`.

- [x] **Step 1: Implement** per the spec's "Up-next screen" (bar fraction = `(gap_offset) / (gap_offset + ms_until_episode)`, clamped).
- [x] **Step 2: Build**, then Dolphin with the test root at a gap (channel 1 after 09:10 in the sample schedule or a root whose slot ends early): `shot:upnext` matches slide 6.
- [x] **Step 3: Commit** `wip | up next card with countdown, bloom and break pill`.

---

### Task 9: HOME menu

**Files:**
- Modify: `wii/source/home_layout.c`, `wii/source/home_layout.h`, `wii/source/screens/homemenu.c`, `wii/tests/test_home_layout.c`

**Interfaces:**
- `home_layout home_layout_make(float pixel_x);` fields: `top, bottom, badge, title, playing, wii_menu, hbc, resume, vol_label, vol_bar, battery_label, battery, battery_bars[4]`; values: badge 26,16.9 (37 tall); title x = badge right + 8, y 22.4; playing right edge 614, y 27.6; wii_menu 150,133.3 340×60; hbc 150,221.3 340×60; resume 225,311.1 190×40; bottom 0,408 640×72; vol_label 26,436.4; vol_bar 68.2,436.4 121×16 (10 segments, gap 2.35); battery pill 572.3,431.1 41.7×26.7 with the label at 554.1. `home_hit` unchanged; `home_battery_bars` unchanged; `HOME_DIM_ALPHA` unchanged.

- [x] **Step 1: Update `test_home_layout.c`**: keep the hit and battery tests; change `layout_resume_and_battery_sit_in_the_bottom_bar` → `layout_resume_sits_above_the_bottom_bar` (`resume.y + resume.h ≤ bottom.y`) and `layout_battery_and_volume_sit_in_the_bottom_bar`; add `home_4_3_title_clears_badge` (`pixel_x` 1.0 → `title.x ≥ badge.x + badge.w`; 0.75 → `title.x` ≈ 62 ± 1); add `layout_segments_fit_the_volume_bar`.
- [x] **Step 2: Run** → FAIL. **Step 3: Implement** layout and `homemenu.c` (bars, badge, title, now-playing text through `ctw_db_channel`/`tn_current`, pills with hot and pressed states through `ps_update` and `in_released`, segments, battery). **Step 4: Run** → PASS; build.
- [x] **Step 5: Dolphin**: `key:36`, `shot:home` matches slide 7; pointer on "Wii Menu" shows the strong ring; `key:36` closes.
- [x] **Step 6: Commit** `wip | home menu redesign`.

---

### Task 10: Intro

**Files:**
- Modify: `wii/source/logoanim.c`, `wii/source/logoanim.h`, `wii/source/screens/logo.c`, `wii/tests/test_logoanim.c`

**Interfaces:**
- `logo_frame` gains `float px, py, pw, ph, panel_alpha; int dot;` `LOGO_PANEL_W 333.3f`, `LOGO_PANEL_H 124.4f`, `LOGO_FULL_W 285.3f` (the wordmark's full width inside the panel; height from the image aspect); `logo_frame_at(uint32_t t_ms, bool skipped, float img_w, float img_h)` unchanged in signature: 0–600 the panel and wordmark scale 0.6 → 1 around 320,231.1 with alpha = intro; 1400–2000 the wordmark lerps to the header rect, `panel_alpha` = 1 − move, the panel rect lerps with the wordmark (same scale); `dot` = (t / 300) % 3 while `panel_alpha` > 0.

- [x] **Step 1: Update `test_logoanim.c`**: `logo_starts_small_and_clear` (w = 285.3×0.6 = 171.2 for a 356×64 image: use `img 356, 64` in the tests; h = 171.2×64/356 = 30.8; panel 200×74.6 centred at 320,231.1; `panel_alpha` 0), `logo_is_full_size_at_600` (w 285.3, h 51.3, x 177.3, y 205.5; panel 153.3,168.9 333.3×124.4; `panel_alpha` 1), `logo_moves_to_header_1400_to_2000` (at 1700: `panel_alpha` 0.125, x lerped 87.5% toward 19.3), `logo_hands_off_at_2000_in_header` (x 19.3, y 11.5, w 139.1, h 25; `panel_alpha` 0), `logo_dots_cycle_every_300ms` (dot 0 at 0, 1 at 300, 2 at 600, 0 at 900).
- [x] **Step 2: Run** → FAIL. **Step 3: Implement** `logoanim.c`; in `logo.c`: bloom (alpha = `panel_alpha`), panel through `ui_card` with ring and the 16% shadow, wordmark, shine as two gradient quads (`GRRLIB_GXEngine`) tilted 22° clipped by `GRRLIB_ClipDrawing` to the panel, dots; `guide_draw_intro` draws the header and system bar with `alpha = panels`. **Step 4: Run** → PASS; build.
- [x] **Step 5: Dolphin**: `wait:4 shot:intro` (during the shine, at ~1 s after boot… capture at `wait:3`) resembles slide 8; the handoff lands the logo in the header without a jump.
- [x] **Step 6: Commit** `wip | intro panel, shine and loading dots`.

---

### Task 11: Error dialog, docs and design-system sync

**Files:**
- Modify: `wii/source/screens/error.c`, `CHANGELOG.md`, `docs/STATUS.md`, `docs/superpowers/plans/2026-10-20-cable-twii-03-wii-app.md` (status note), `docs/testing/wii-checklist.md` (new items: system bar buttons, pressed pills, empty state, intro panel)

- [x] **Step 1: Restyle `error.c`** with `ui_card` (info gradient, panel shadow) and `ui_pill_ex` through `ps_update`.
- [x] **Step 2: Build and run** the full host suite and `make -C wii`; Dolphin with an SD folder that has no `library.toml` → the dialog.
- [x] **Step 3: Docs**: CHANGELOG entry under the next version ("Wii app redesigned at system detail: …"), STATUS line, checklist items.
- [x] **Step 4: Commit** `wip | error dialog restyle, redesign notes and checklist items`.
- [x] **Step 5: Re-sync the Design System artifact** per the memory note (tokens from `theme.h`), republish changed files only.
