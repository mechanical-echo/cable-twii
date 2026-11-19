# cable-twii Today List and Skipping Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Today's list (the broadcast day's lineup for any channel, opened with 1 or a `1 Today` hint) and prev/next skipping on looping channels (D-pad ← → in fullscreen), kept until restart.

**Architecture:** Each tuner entry holds a signed per-channel shift in ms. `tn_current` applies it, so every "what's on" lookup (live playback, guide, panel, HOME, the list) follows a skip with no other change. Skips, jumps and "which lineup is on now" are tuner functions, host-tested against the engine fixture. The list is a pure row model (ads merged) plus a pure layout, both host-tested, drawn by an overlay screen that `main.c` runs like the HOME menu. Displayed clock times subtract the shift.

**Tech Stack:** C11 on devkitPPC/libogc/GRRLIB, the `engine/` lineup builder, greatest host tests (`make -C wii/tests test`).

**Spec:** `docs/superpowers/specs/2026-11-19-today-and-skipping-design.md`. Look and pixel numbers: `docs/superpowers/specs/mockups/wii-today-and-skipping.html` (variant A, frames 1, 3 and 4; script functions `todayCard`, `darkHint`).

## Global Constraints

- Skipping only on `CTW_MODE_LOOPING` channels. Timed channels: shift is always 0, and D-pad ← → in fullscreen show the notice `This channel is live`.
- With every shift at 0, every screen behaves and draws exactly as today, apart from D-pad ← → no longer flipping channels.
- Shifts live only in memory: lost on restart, Reload channels, the automatic server reload, and a source switch (all go through `tn_free`). Nothing new is written to disk.
- Times on screen are real clock times: lineup time minus the channel's shift. Countdowns and progress bars are unchanged.
- Today's list shows the lineup holding the item on now (looping: the channel's own day; timed after midnight before the first slot: yesterday's lineup). Header `Today · <gt_date of that lineup>`.
- Copy, exact: `This channel is live`, `Today`, `Looping`, `Play from here`, `Close`, `◀ ▶ skip in fullscreen`, `Up next`, `Ad break (1 ad, 1:00)` / `Ad break (2 ads, 1:30)`, day markers ` +1` and ` −1` (U+2212, as the volume panel's minus).
- House rules: match the sibling file; no comments in C source; no trailing whitespace; commits `wip | …` / `fix | …` / `test | …`, short, ending with `Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>`.
- `make -C wii/tests test` passes with 0 failures (host tests build with `-Wall -Wextra -Werror -pedantic`). `make -C wii` builds `wii/cable-twii.dol` with no warnings beyond the existing `-Wchar-subscripts` ones from the vendored `tomlc99/toml.c`.
- Never run Dolphin without the owner's OK. Ask the owner before every wiiload send. Don't touch `~/cable-twii-server` or ports 4747/4748.

## Review Focus

1. The wall clock or the channel's day rolls over while Today's list is open, or `ensure_entry` reallocs `t->entries`. The list must rebuild, not read freed rows. Task 6's rebuild rule keys on `items` pointer + date, never a kept `ctw_lineup *`. Covered by checklist item 68 (f).
2. Reload channels (or the automatic server reload) with the list open: the list closes, skips clear, no crash (Task 6 `today_reset` in `drop_source`; checklist 68 (g)).
3. HOME → Settings, or an error screen, while the list is open: the list must not draw over them (Task 6 main-loop rule; checklist 68 (g)).
4. A on the row already on now: the shift changes but the airing doesn't; the panel and info card times must still refresh (Task 5 adds the shift to their cache keys; checklist 68 (d)).
5. `tn_jump` is given a row's airing pointer, then `tn_current` rebuilds lineups because the day changed: the jump must not read the freed pointer (Task 1 test `jump_reads_target_before_rebuild`).

---

### Task 1: Per-channel shift, skip and jump in the tuner

**Files:**
- Modify: `wii/source/tuner.h`, `wii/source/tuner.c`
- Test: `wii/tests/test_tuner.c`

**Interfaces:**
- Produces:
  - `tuner_entry` gains `int64_t shift_ms;` (0 from `ensure_entry`'s memset).
  - `tn_current(t, channel, day, ms_of_day, offset_ms)`: unchanged signature. `day` / `ms_of_day` are the wall clock. It looks up the airing at wall + `shift_ms`, rolling `day` with `ctw_date_next` / `ctw_date_prev` in a loop until the time is in 0..`CTW_DAY_MS`. The entry's `day` (and so `tn_airing_start`) is the channel's day.
  - `int64_t tn_shift(const tuner *t, int channel);`: 0 when there is no entry.
  - `const ctw_airing *tn_prev(tuner *t, int channel, const ctw_airing *a);`: `a - 1` inside today's or yesterday's lineup. For today's first item it returns yesterday's last. NULL for yesterday's first, or a pointer in neither.
  - `bool tn_skip(tuner *t, int channel, ctw_date day, uint32_t ms_of_day, int dir);`: false (shift unchanged) for unknown or non-looping channels, or no current airing.
    - `dir > 0`: `shift += duration - offset` of the current airing.
    - `dir < 0`: `shift -= offset + (start(cur) - start(prev))`, using `tn_airing_start`; false when `tn_prev` is NULL.
  - `bool tn_jump(tuner *t, int channel, ctw_date day, uint32_t ms_of_day, const ctw_airing *to);`: looping only. It reads `tn_airing_start(to)` **before** calling `tn_current`, and returns false if that is < 0. Then `shift += start(to) - (start(cur) + offset)`.
  - `const ctw_lineup *tn_lineup_now(tuner *t, int channel, ctw_date day, uint32_t ms_of_day, ctw_date *lineup_day);`: calls `tn_current`.
    - If the current airing is in `yesterday`, it returns `&e->yesterday` and sets `*lineup_day` to the entry's day minus one.
    - Otherwise it returns `&e->today` with the entry's day.
    - NULL if the entry isn't valid.
    - The pointer is good only until the next tuner call.

- [ ] **Step 1: Write the failing tests** in `test_tuner.c`, registered in `tuner_suite`. Use fixture channel 2 (looping) and 1 (timed, first slot 07:00), day 2026-10-20. Build references with `ctw_lineup_build` as `tuner_current_matches_engine` does.
  - `shift_zero_matches_today`: a fresh tuner has `tn_shift == 0` for channel 2, and `tn_current` equals the engine's `ctw_lineup_at` at 10:00:05.
  - `skip_next_starts_the_following_item`: at 10:00:05, `cur = tn_current`. `tn_skip(+1)` is true. Then `tn_current` at the same wall time returns the airing whose `start_ms` is `cur->start_ms + cur->duration_ms`, with offset 0.
  - `skip_prev_starts_the_item_before`: at 10:00:05 (offset > 0 in the current item), `tn_skip(-1)` gives the airing just before `cur`, at offset 0. It does not restart `cur`.
  - `quick_presses_add_up`: three `tn_skip(+1)` at the same wall time land on the third successor of `cur`, following `tn_next` three times, at offset 0.
  - `skip_next_crosses_midnight`: at 23:59:59.000, `tn_skip(+1)` is true. `tn_current` then returns `start_ms == 0`, `ctw_date_key(t.entries[0].day)` is 2026-10-21's key, and `tn_shift` is 1000.
  - `skip_prev_crosses_midnight_back`: at 00:00:05.000, `tn_skip(-1)` is true. The current airing is the last item of the 2026-10-19 lineup (`t.entries[0].today.items[count-1]`), at offset 0, and the entry's day is 2026-10-19.
  - `prev_after_crossing_comes_back`: after `skip_next_crosses_midnight`'s press, one `tn_skip(-1)` returns to the last item of 2026-10-20 at offset 0.
  - `timed_channel_never_skips`: `tn_skip(+1)`, `tn_skip(-1)` and `tn_jump` on channel 1 return false, and `tn_shift(1)` stays 0.
  - `jump_starts_the_chosen_airing`: at 10:00:05, `tn_jump` to `today.items[k]` for `k` = current index + 5, then to current index − 3. `tn_current` returns that airing at offset 0 after each.
  - `jump_reads_target_before_rebuild`: take `to = &today.items[0]` on 2026-10-20, then call `tn_jump` with wall day 2026-10-21 at 00:00:05. It returns true, and the current airing is the 2026-10-20 lineup's first item at offset 0. Under AddressSanitizer there is no use-after-free; plain builds just pass.
  - `lineup_now_timed_after_midnight_is_yesterday`: channel 1 at 02:00. `tn_lineup_now` returns `&t.entries[0].yesterday` with `lineup_day` 2026-10-19. At 08:00 it returns `&t.entries[0].today` with 2026-10-20.
  - `lineup_now_looping_is_the_channel_day`: channel 2 after `skip_next_crosses_midnight`'s press returns `today` with 2026-10-21.

- [ ] **Step 2: Run** `make -C wii/tests test`. Expected: the build fails on the undeclared `tn_shift` / `tn_prev` / `tn_skip` / `tn_jump` / `tn_lineup_now`.

- [ ] **Step 3: Implement** as the Interfaces block says. Use a static helper for wall + shift → (day, ms). `tn_free` already drops shifts with the entries.

- [ ] **Step 4: Run** `make -C wii/tests test`. Expected: 0 failures (all old tuner tests still pass).

- [ ] **Step 5: Commit** `wip | tuner shift, skip and jump`.

### Task 2: Text for the list

**Files:**
- Modify: `wii/source/guide_text.h`, `wii/source/guide_text.c`
- Test: `wii/tests/test_guide_text.c`

**Interfaces:**
- Produces:
  - `void gt_clock(int64_t ms, char *out, size_t out_size);`: `HH:MM` of `ms` within its day. Let `d = floor(ms / CTW_DAY_MS)`. It appends ` +d` when `d > 0` and ` −|d|` (U+2212) when `d < 0`.
  - `void gt_span(int64_t start_ms, int64_t end_ms, char *out, size_t out_size);`: `gt_clock(start) " – " gt_clock(end)` with the en dash, as `gt_times`.
  - `void gt_duration(uint32_t ms, char *out, size_t out_size);`: Studio's `formatDuration`. Round to whole seconds (`(ms + 500) / 1000`), then `m:ss`.
  - `void gt_ad_break(size_t count, uint32_t total_ms, char *out, size_t out_size);`: `Ad break (1 ad, 1:00)`, or `Ad break (N ads, m:ss)`.
  - `void gt_today(ctw_date d, char *out, size_t out_size);`: `Today · ` + `gt_date(d)` (middle dot as `gt_line`).

- [ ] **Step 1: Write the failing tests**, registered in `guide_text_suite`:
  - `gt_clock_marks_other_days`: 0 gives `00:00`, 73,800,000 gives `20:30`, 88,200,000 gives `00:30 +1`, −1,800,000 gives `23:30 −1`, and 2·86,400,000 + 60,000 gives `00:01 +2`.
  - `gt_span_with_marker`: `gt_span(84,600,000, 89,100,000)` gives `23:30 – 00:45 +1`, and `gt_span(72,720,000, 73,380,000)` gives `20:12 – 20:23`.
  - `gt_duration_rounds_like_studio`: 90,000 gives `1:30`, 59,500 gives `1:00`, 0 gives `0:00`, and 754,000 gives `12:34`.
  - `gt_ad_break_counts`: `(1, 60000)` gives `Ad break (1 ad, 1:00)`, and `(2, 90000)` gives `Ad break (2 ads, 1:30)`.
  - `gt_today_header`: 2026-11-19 gives `Today · Thu 19/11`.

- [ ] **Step 2: Run** `make -C wii/tests test`. Expected: build fails on the new names.
- [ ] **Step 3: Implement** in `guide_text.c`. Name the minus as a `#define` like `EN_DASH`.
- [ ] **Step 4: Run** `make -C wii/tests test`. Expected: 0 failures.
- [ ] **Step 5: Commit** `wip | today list text`.

### Task 3: Today row model

**Files:**
- Create: `wii/source/today_model.h`, `wii/source/today_model.c`, `wii/tests/test_today_model.c`
- Modify: `wii/tests/Makefile` (`SRC` gains `../source/today_model.c`), `wii/tests/main.c` (`SUITE_EXTERN` / `RUN_SUITE(today_model_suite)`)

**Interfaces:**
- Produces (`today_model.h`, includes `"ctw/ctw.h"`):
  - `#define TD_ROW_H 32.0f`, `#define TD_ROW_GAP 2.0f`, `#define TD_PAD 4.0f`, `#define TD_NOW_SLOT 2`.
  - `typedef enum { TD_EPISODE, TD_ADS, TD_UPNEXT } td_kind;`
  - `typedef struct { td_kind kind; const ctw_airing *first; size_t count; int64_t start_ms; int64_t end_ms; uint32_t total_ms; } td_row;`
  - `size_t td_build(const ctw_lineup *l, td_row *rows, size_t cap);`: one row per airing, except that consecutive `CTW_AIR_AD` airings merge into one `TD_ADS` row. `count` is the number of airings, `total_ms` the sum, and `end_ms` the last one's end. NULL or empty gives 0. It never writes more than `cap` rows.
  - `int td_row_of(const td_row *rows, size_t n, const ctw_airing *a);`: the row whose `[first, first + count)` holds `a`, else −1.
  - `float td_content_h(size_t n);`: `2·TD_PAD + n·TD_ROW_H + (n−1)·TD_ROW_GAP`, or 0 for n = 0.
  - `float td_row_y(int index);`: `TD_PAD + index·(TD_ROW_H + TD_ROW_GAP)`.
  - `float td_open_scroll(int now, size_t n, float view_h);`: `td_row_y(now − TD_NOW_SLOT) − TD_PAD`, clamped to `0..max(0, content − view_h)`. For `now < 0` it is 0.
  - `float td_scroll_to(int index, float scroll, float view_h, size_t n);`: the smallest change that keeps the row fully in view, clamped.
  - `int td_hit(float scroll, float py_in_list, size_t n);`: the row under `py_in_list` (pixels from the list's top), or −1 in a gap or past the end.

- [ ] **Step 1: Write the failing tests** in `test_today_model.c`. Build lineups by hand in a `ctw_airing[]` with a `ctw_lineup` wrapper. Each test frees nothing.
  - `build_merges_ads`: kinds E, A, A, E, A, U with durations 600000, 45000, 45000, 600000, 60000, 300000. Rows: E; ADS (count 2, total 90000, end = first ad's start + 90000); E; ADS (count 1); UPNEXT.
  - `build_respects_cap`: the same lineup with cap 2 gives 2 rows.
  - `build_empty_is_zero`: NULL and `count == 0` both give 0.
  - `row_of_finds_merged_ad`: the second ad of the first break gives row 1, the first episode row 0, and a pointer outside gives −1.
  - `open_scroll_puts_now_third`: n = 50, view_h = 314. now = 10 gives `td_row_y(8) − TD_PAD`, i.e. 272. now = 1 gives 0. now = 49 gives `content − 314`.
  - `scroll_to_keeps_row_in_view`: row 20 from scroll 0 with view 314 gives `td_row_y(20) + TD_ROW_H − 314`. Row 0 from scroll 300 gives 0. A row already in view keeps the scroll.
  - `hit_maps_rows_and_gaps`: with scroll 0, `py = TD_PAD + 1` gives 0, `py = TD_PAD + TD_ROW_H + 1` (in the gap) gives −1, and the bottom past the end gives −1. With scroll 34, `py = TD_PAD + 1` gives 1.

- [ ] **Step 2: Run** `make -C wii/tests test`. Expected: build fails (missing header).
- [ ] **Step 3: Implement** `today_model.c`.
- [ ] **Step 4: Run** `make -C wii/tests test`. Expected: 0 failures.
- [ ] **Step 5: Commit** `wip | today row model`.

### Task 4: Today layout and the hint rects

**Files:**
- Create: `wii/source/today_layout.h`, `wii/source/today_layout.c`, `wii/tests/test_today_layout.c`
- Modify: `wii/tests/Makefile` (`SRC`), `wii/tests/main.c` (`today_layout_suite`)

**Interfaces:**
- Consumes: `asp_rect` (`aspect.h`), `TD_ROW_H` (Task 3).
- Produces (`today_layout.h`):
  - `#define TL_HINT_H 26.7f`, `#define TL_HINT_DISC 19.6f`, `#define TL_HINT_PX 12`.
  - `typedef struct { asp_rect card, icon, name, date, tag, rule_top, list, scroll, rule_bottom, hints, skip_note; } today_layout;` For text rects, `x`, `y` is the em-box top as `ui_text` takes it. `skip_note.x` is its right edge.
  - `today_layout today_layout_for(float pixel_x);`
  - `typedef struct { float time_x, icon_x, icon_w, show_x, show_w, code_x, title_x, title_w; } today_cols;`
  - `today_cols today_cols_for(asp_rect row, float time_w, float code_w, float pixel_x);`
  - `float today_hint_w(float label_w, float pixel_x);`: `2.7 + 19.6·px + 4 + label_w + 9·px`.
  - `asp_rect today_hint_full(asp_rect panel, float hint_w);`: `x = panel.x + panel.w − hint_w`, `y = panel.y − 8 − TL_HINT_H`.
  - `asp_rect today_hint_guide(asp_rect preview, float hint_w, float pixel_x);`: inner = preview inset by `5·px` left/right and 5 top/bottom. The `A Watch` hint is at `inner.x + 9`, `inner.y + inner.h − 33.8`, w 73.3. This one is at that x + 73.3 + 6, same y.

Numbers (from the mockup, `C` = card):
- `card` {30, 22, 580, 436}
- `icon` {C.x+14, C.y+12, 36·px, 36}
- `name` x = icon right + 9, y = C.y+12, w = to `tag`'s x − 16
- `date` same x, y = C.y+35
- `tag` right edge C.x+C.w−16, y = C.y+17, h 20 (w is set by the screen from the text)
- `rule_top` {C.x+12, C.y+58, C.w−24, 1}
- `list` {C.x+8, C.y+64, C.w−30, C.h−108}
- `scroll` {C.x+C.w−12.3, list.y+4, 3, list.h−8}
- `rule_bottom` {C.x+12, C.y+C.h−40, C.w−24, 1}
- `hints` x = C.x+18, y = C.y+C.h−30
- `skip_note` right edge C.x+C.w−18, y = hints.y+4

Columns:
- `time_x` = row.x+14·px
- `icon_x` = time_x+time_w+12·px; `icon_w` 22·px
- `show_x` = icon_x+26·px; `show_w` 110·px
- `code_x` = show_x+show_w+8·px
- `title_x` = code_x+code_w+10·px; `title_w` = row.x+row.w−10·px−title_x

- [ ] **Step 1: Write the failing tests**, registered in `today_layout_suite`:
  - `card_matches_the_mockup`: the rects above at px 1.0 and 0.75. At 0.75, `icon.w` is 27.
  - `list_sits_between_the_rules`: `rule_top` is below `icon` and above `list`. `list` ends above `rule_bottom`. `scroll` is inside `card`, right of `list`.
  - `columns_fit_a_marked_time`: with `row = {38, 90, 550, 32}`, `time_w = 96` (the width of `23:30 – 00:45 +1`) and `code_w = 46`, the columns run left to right without overlap, and `title_w > 100` at px 1.0 and 0.75.
  - `full_hint_sits_above_the_panel`: `today_hint_full({26, 348, 588, 108}, 80)` is {534, 313.3, 80, 26.7}.
  - `guide_hint_follows_watch`: `today_hint_guide({307.3, 58.7, 313.3, 235.1}, 70, 1.0)` is {400.6, 255.0, 70, 26.7} (±0.01). At px 0.75, x is 399.35.
  - `hint_w_counts_disc_and_label`: `today_hint_w(30, 1)` is 65.3, and `today_hint_w(30, 0.75)` is 58.15.

- [ ] **Step 2: Run** `make -C wii/tests test`. Expected: build fails.
- [ ] **Step 3: Implement** with the numbers as named `#define`s.
- [ ] **Step 4: Run** `make -C wii/tests test`. Expected: 0 failures.
- [ ] **Step 5: Commit** `wip | today layout`.

### Task 5: Skipping in fullscreen

**Files:**
- Modify: `wii/source/osd.h`, `wii/source/osd.c`, `wii/source/screens/full.h`, `wii/source/screens/full.c`, `wii/source/screens/panel.c`, `wii/source/screens/guide_info.c`, `wii/source/screens/upnext.c`
- Test: `wii/tests/test_osd.c`

**Interfaces:**
- Consumes: `tn_skip`, `tn_shift` (Task 1).
- Produces:
  - `osd_state` gains `uint32_t forced_until;`.
  - `void osd_skipped(osd_state *s, uint32_t now_ms);`: if the panel isn't visible, it sets `shown_at = now`. It sets `forced_until = now + THEME_OSD_TIMEOUT_MS`.
  - `osd_visible` is true while `(osd && now < osd_until) || now < forced_until`. `osd_pointer(false, …)` still clears only `osd`.
  - `void full_jumped(app *a);` in `full.h`: `sfx_play(SFX_CHANNEL)` and `osd_skipped(&g_osd, a->now_ms)`.

- [ ] **Step 1: Write the failing tests** in `test_osd.c`:
  - `skip_shows_panel_without_pointer`: `osd_pointer(&s, false, false, 0)`, then `osd_skipped(&s, 100)`. `osd_panel_visible` is true at 101 and still true after `osd_pointer(&s, false, false, 200)`. It is false at `100 + THEME_OSD_TIMEOUT_MS`.
  - `skip_slides_in_when_hidden`: after `osd_skipped(&s, 100)`, `osd_slide(&s, 100)` is 0.
  - `skip_keeps_slide_when_shown`: `osd_pointer(&s, true, true, 0)`, then `osd_skipped(&s, 500)`. `osd_slide(&s, 500)` is 1.
  - `skip_hides_behind_volume`: `osd_skipped` and then `osd_volume_changed` at the same time. `osd_panel_visible` is false, as for the pointer today.
- [ ] **Step 2: Run** `make -C wii/tests test`. Expected: build fails on `osd_skipped`.
- [ ] **Step 3: Implement** `osd.c` / `osd.h`.
- [ ] **Step 4: Run** `make -C wii/tests test`. Expected: 0 failures.
- [ ] **Step 5: Fullscreen D-pad (`full.c`).**
  - ↑ flips +1 and ↓ flips −1. Drop ← / → from the flip.
  - → / ← call `skip(a, ±1)`. If the channel's mode isn't `CTW_MODE_LOOPING`, `notice_show(a, "This channel is live")`. Otherwise, if `tn_skip(&a->tuner, a->current_channel, a->today, a->ms_of_day, dir)` succeeds, call `full_jumped(a)`.
  - `live_update` at the end of `full_update` re-tunes on the same frame.
- [ ] **Step 6: Displayed times subtract the shift.** Take `shift = tn_shift(&a->tuner, channel)`:
  - `panel.c` `refresh`: `gt_hhmm(start - shift)` for start and end, `gt_at(next_ep->start_ms - shift)`.
  - `guide_info.c` `refresh`: `gt_times(air->start_ms - shift, …)`.
  - `upnext.c` `refresh`: `gt_at(ep->start_ms - shift)`, with the channel from `a->current_channel`.
  - Each of the three caches gains `int64_t shift`, compared with the start / airing keys, so a change of shift alone refreshes the text.
- [ ] **Step 7: Build** `make -C wii` (no new warnings) and `make -C wii/tests test` (0 failures).
- [ ] **Step 8: Commit** `wip | skip on looping channels`.

### Task 6: Today's list overlay

**Files:**
- Create: `wii/source/screens/today.h`, `wii/source/screens/today.c`
- Modify: `wii/source/app.h`, `wii/source/main.c`, `wii/source/screens/full.c`, `wii/source/screens/guide.c`

**Interfaces:**
- Consumes: Tasks 1–5 (`tn_lineup_now`, `tn_current`, `tn_jump`, `td_*`, `today_layout_for`, `today_cols_for`, `gt_span`, `gt_ad_break`, `gt_today`, `gt_episode`, `full_jumped`).
- Produces:
  - `app` gains `bool today_open;`
  - `void today_open(app *a, int channel);`
  - `void today_update(app *a);`
  - `void today_draw(app *a);`
  - `void today_reset(app *a);`: closes, frees the rows, and sets `today_open` false.

- [ ] **Step 1: Write `screens/today.c`.**
  - **State.** Module statics: channel, `td_row *rows` (malloc'd to the lineup's count), `n`, the kept `items` pointer, the lineup's date key, highlight, scroll and a scroll tween (200 ms, as the guide), and a per-row text cache (`calloc`'d, filled when a row is first drawn).
  - **`today_open`.**
    - Build from `tn_lineup_now`.
    - Highlight = the now row (`td_row_of` the `tn_current` airing).
    - Scroll = `td_open_scroll`.
    - `SFX_SELECT`.
  - **`today_update`.** Each frame:
    - Re-call `tn_lineup_now`. If its `items` pointer or date key changed, rebuild and keep the highlight on the now row.
    - B or 1 closes (`SFX_BACK`).
    - D-pad ↑ ↓ move the highlight with `td_scroll_to` (`SFX_HOVER`).
    - Pointer over `list`: `gm_edge_scroll` with `td_content_h`. `td_hit` hover sets the highlight (`SFX_HOVER` + `in_rumble_pulse(THEME_HOVER_RUMBLE_MS)` on change).
    - A on a looping channel jumps:
      - `tn_jump` to `rows[h].first`, then `SFX_SELECT` and close.
      - If the screen is `SCR_GUIDE`: `live_tune(a, channel)` then `app_go(a, SCR_FULL)`.
      - Else if `channel == a->current_channel`: `full_jumped(a)`.
    - A on a timed channel does nothing.
  - **`today_draw`**, following mockup frame 4.
    - Dim and card:
      - Full-screen `THEME_SCRIM`.
      - Card: `ui_card(…, THEME_PANEL_RADIUS, 0.85, THEME_GLOSS_BOTTOM, UI_SHADOW_VIDEO)`.
    - Header:
      - Channel icon via `gt_icon_path` / `icons_draw`.
      - Name: bold 18, `THEME_ACCENT`, then `ui_number_pill` (11 px) 6 px after it.
      - `gt_today`: body 12, secondary.
      - `Looping` tag on looping channels: `THEME_BAR_TRACK` pill, bold 12, `THEME_TEXT_LABEL`.
      - Rules in `THEME_BAR_TRACK`.
    - Rows: drawn inside `GRRLIB_ClipDrawing(list)`, skipping those out of view.
      - Hot row: as `guide_list.c`'s hot row, radius 10.
      - Now row: `THEME_ROW_ACTIVE_FILL`, a 4·px accent edge, and a 3 px `ui_bar` progress line at the bottom.
      - Rows before now: `THEME_TEXT_FADED`.
      - Text: time `gt_span(start - shift, end - shift)` body 12. Show bold 14, code bold 12, title body 13, fitted to `today_cols_for` widths. `time_w` is the measured width of `23:30 – 00:45 +1`, `code_w` that of `S00E00`.
      - Ad and up-next rows: `gt_ad_break` or `Up next`, body 13, at `show_x`.
    - Scrollbar as `guide_list.c`'s.
    - Footer:
      - `A Play from here` + `B Close` (looping) or `B Close` (timed), drawn as a white disc with the key plus a label, bold 12, `THEME_TEXT_LABEL`.
      - `◀ ▶ skip in fullscreen` right-aligned, body 12, on looping channels.
- [ ] **Step 2: Wire `main.c`.**
  - At the top of the loop, if the screen is neither `SCR_GUIDE` nor `SCR_FULL`, call `today_reset`.
  - After `home_update`: `listing = !paused && g_app.today_open`. If `listing`, call `today_update`. Block input and cursor with `paused || listing`.
  - After the screen's draw, draw the list (`if (g_app.today_open) today_draw(&g_app);`) before `home_draw`.
  - The cursor also draws while `today_open`.
  - `drop_source` calls `today_reset`.
- [ ] **Step 3: Button 1.**
  - `full.c` `full_update`: `in_pressed(WPAD_BUTTON_1)` → `today_open(a, a->current_channel)`.
  - `guide.c` `handle_input`: the same with `highlighted_channel()` when it isn't 0.
- [ ] **Step 4: Build** `make -C wii` (no new warnings) and `make -C wii/tests test` (0 failures).
- [ ] **Step 5: Commit** `wip | today list overlay`.

### Task 7: The `1 Today` hints

**Files:**
- Modify: `wii/source/ui/draw.h`, `wii/source/ui/draw.c`, `wii/source/screens/guide_info.h`, `wii/source/screens/guide_info.c`, `wii/source/screens/guide.c`, `wii/source/screens/full.c`

**Interfaces:**
- Consumes: `today_hint_w`, `today_hint_full`, `today_hint_guide`, `TL_HINT_*` (Task 4); `today_open` (Task 6).
- Produces:
  - `void ui_key_hint(float x, float y, float w, const char *key, const char *label, bool hot);` in `ui/draw.c`. It holds `guide_info.c`'s `draw_hint` body: `THEME_HINT_FILL` pill, gradient disc, key and label, bold 12. When `hot` it draws `ui_ring(…, THEME_RING_STRONG, THEME_RING_SPREAD)` first.
  - `void ginfo_draw_preview(app *a, asp_rect r, float expand, bool today_hot);`

- [ ] **Step 1: Move the hint drawing** into `ui_key_hint`. `A Watch` then draws through it unchanged. Drop the `HINT_*` defines from `guide_info.c` that it no longer uses.
- [ ] **Step 2: Guide (`ginfo_draw_preview`).** When `keep >= 1`, draw `ui_key_hint` at `today_hint_guide(r, …)` with `"1"`, `"Today"` and `today_hot`, live or not. `draw_transition` passes `false`.
- [ ] **Step 3: Guide input (`guide.c`).**
  - Keep `today_hot` = cursor visible and inside the hint rect. Entering it plays `SFX_HOVER` with the rumble pulse.
  - On A, a hot hint calls `today_open(a, highlighted_channel())` before the preview's watch check.
- [ ] **Step 4: Fullscreen (`full.c`).**
  - When `osd_panel_visible`, draw `ui_key_hint` at `today_hint_full(l.panel, w)` after `panel_draw`. It moves with the slide offset because `l` already has it.
  - `panel_input` keeps a `today_hot` the same way.
  - A pressed while the hint is hot calls `today_open(a, a->current_channel)`.
- [ ] **Step 5: Build** `make -C wii` (no new warnings) and `make -C wii/tests test` (0 failures).
- [ ] **Step 6: Commit** `wip | today hints`.

### Task 8: Docs and checklist

**Files:**
- Modify: `docs/USER-GUIDE.md` (section 13), `CHANGELOG.md`, `docs/testing/wii-checklist.md`

- [ ] **Step 1: `docs/USER-GUIDE.md` section 13.**
  - **Guide** gains: "**1** (or point at **1 Today** on the preview and press **A**) opens today's list for the highlighted channel."
  - **Fullscreen** is replaced by:
    - "**D-pad up**: next channel. **D-pad down**: previous channel."
    - "**D-pad right** and **left** on a looping channel: jump to the start of the next or previous item in its schedule (an episode or an ad). Timed channels are live; they show *This channel is live*. A jump lasts until cable-twii restarts or reloads its channels."
    - "**1** (or **1 Today** above the info panel) opens today's list."
    - "**+** and **−**: volume (steps of 10, from 0 to 100)."
    - "**B**: back to the guide."
    - "Point the remote at the screen to show the **info panel**; it hides itself after a few seconds without pointer movement. While the volume panel shows, point at its **−** and **+** and press **A**."
  - New **Today's list** paragraph:
    - It shows the day's schedule as Studio's Preview does, with ads grouped into breaks and what's on now in blue.
    - On a timed channel after midnight it shows the schedule day that is still running.
    - **D-pad up/down** or the pointer moves the highlight.
    - On a looping channel **A** plays from the highlighted item.
    - **B** or **1** closes it.
- [ ] **Step 2: `CHANGELOG.md` "Unreleased"** gets a top line: "Wii app: Today's list (press 1, or the `1 Today` hint in the guide and above the info panel) shows a channel's schedule for the day, like Studio's Preview. On looping channels D-pad right and left jump to the next or previous item, until cable-twii restarts, and A in the list plays from the chosen item. D-pad left and right no longer change channels. Not yet run on a real Wii."
- [ ] **Step 3: `docs/testing/wii-checklist.md`:** a new `### Today's list and skipping` section after Picture setting, in the same table format, with item **68 Today's list and skipping**.
  - Steps:
    - (a) On the Cartoon Network loop press → five times, then ← twice, from drive and from server.
    - (b) Flip to another channel and back.
    - (c) Press 1 in fullscreen and in the guide, then point at `1 Today` and press A in both. Scroll with the D-pad and the pointer.
    - (d) In the list on the loop, press A on a later row, then open it again and press A on the now row.
    - (e) On a timed channel press → and open the list after midnight before its first slot.
    - (f) Leave the list open across midnight.
    - (g) With the list open: HOME → `Reload channels`; and HOME → `Settings`.
  - Expected:
    - (a) Each press starts the next or previous item from its beginning, with the channel sound and the info panel. Its start, end and "at" times match the Wii's clock. Fast presses add up.
    - (b) It is where you left it.
    - (c) The list opens with now third and in blue, earlier rows grey, and ads as `Ad break (N ads, m:ss)`. Studio's Preview shows the same rows for that date.
    - (d) That item plays from its start. On the now row it restarts it, and the panel's times update.
    - (e) *This channel is live*; the list shows the schedule day still running, with the now row marked.
    - (f) It rebuilds without a crash.
    - (g) The list closes and the jumps are gone.
- [ ] **Step 4: Run** `make -C wii/tests test` and `make -C wii`. Expected: 0 failures, no new warnings. `git diff master --stat` shows only the files in this plan, and `grep -rn '[[:space:]]$'` over them finds nothing.
- [ ] **Step 5: Commit** `wip | today list and skipping docs`.
- [ ] **Step 6: Offer a Wii run.** Ask the owner to open the Homebrew Channel and OK a send of `WIILOAD=tcp:192.168.1.155 /opt/devkitpro/tools/bin/wiiload wii/cable-twii.dol`. Walk item 68 with them, and send nothing without that OK.
