# cable-twii — Picture setting: Fit, 14:9, Fill (design)

Date: 2026-11-19
Builds on: [Wii redesign](2026-10-22-wii-redesign-design.md), [server source](2026-11-02-server-source-design.md), [file formats](../../formats.md)
Mockup: [HOME menu: Picture setting, frames 8–9](mockups/wii-settings-and-keyboard.html), approved by the owner on 2026-11-19

Decisions marked **(my call)** were made by the designer, not the owner, and may be revisited. Everything else was stated or confirmed by the owner.

## Intent

The owner watches on a small 4:3 CRT. A 16:9 show is letterboxed there and uses only 3/4 of the screen height. The owner wants such shows to fill the screen the way 4:3 channels once showed widescreen programmes: by cutting the sides, never by stretching. 14:9, the broadcasters' compromise, is wanted as a middle step.

Success: on the 4:3 TV, HOME → Picture → Fill makes the 16:9 show behind the menu fill the screen with 1/8 cut off each side, and it is still Fill after cable-twii restarts.

## Goals, non-goals, anti-goals

- Goals: one global setting with three modes, Fit (today, the default), 14:9 and Fill; changed from the HOME menu while watching; saved in `settings.toml`.
- Non-goals: a per-channel or per-show setting; pan or zoom controls; a Picture row on the Settings screen; changing the guide preview (it stays Fit, owner's call).
- Anti-goals: any stretch or squeeze of the picture; Fit looking any different from today.

## Behaviour

**Modes.** Let `a` be the show's shape and `T` the TV's (4:3, or 16:9 from `asp_pixel_x`). The picture is cut around its centre to a shown shape `S`, then fitted to the screen as today:

| Mode | `S` |
|---|---|
| Fit | `a` (nothing cut) |
| 14:9 | 14:9, kept between `a` and `T` |
| Fill | `T` (no bars) |

When `S` is narrower than `a` the sides are cut; when it is wider the top and bottom are. A show that already matches the TV is the same in every mode. On the owner's 4:3 TV a 16:9 show gets: Fit, full width and 75% of the height; 14:9, 1/16 cut off each side and 86% of the height; Fill, 1/8 cut off each side and the full screen. On a 16:9 TV a 4:3 show gets 14:9 with 1/14 cut off top and bottom, and Fill with 1/8 cut off top and bottom.

**Where it applies.** Full screen uses the mode. The guide preview always fits. The zoom from the preview to full screen (and back) eases the cut in (and out) with the zoom: `S` moves from `a` to the mode's value as the zoom runs **(my call)**, so there is no jump at either end.

**The control.** A Picture switch in the HOME menu's bottom bar, between Volume and the battery, as in mockup frames 8–9; the rects are in the mockup's numbers table. It works like Play from on the Settings card: pointer and A (acting on release, like every HOME pill), hover sound and rumble, the chosen part drawn as the blue thumb. The picture behind the dim changes at once. It is shown on every screen the HOME menu opens over, in Drive and Server mode **(my call)**.

**Saving.** Each change sets the app's settings and saves `settings.toml` straight away under the same rule as Settings (only when no SD partition has `wbfs/` or `games/`). If there is no SD card or the write fails, the choice lasts until cable-twii closes and nothing is said in the HOME menu **(my call: the Settings screen already shows that line)**. The Settings screen keeps whatever the switch last set, so its Save or Cancel never undoes it **(my call)**.

**File.** `picture = "fit"`, `"14:9"` or `"fill"` in `settings.toml`. A missing key or an unknown value means Fit and is not an error **(my call)**. The app always writes the key.

## Approaches

1. **Draw only the middle part of the frame texture (chosen).** `yg_draw` takes the texture window to draw; the quad stays inside its box. Nothing is drawn off screen, and it works in any box, so the preview zoom eases smoothly.
2. Draw the frame larger than the screen and let the edge (or a GX scissor) cut it. No texture change, but during the zoom the picture would spill over the guide unless every box is scissored, and it draws pixels only to throw them away. Not chosen.
3. Crop when converting the videos in Studio. Permanent per file and loses Fit; the owner asked for a setting. Not chosen.

## Shape of the code (my calls)

- `aspect.h` / `aspect.c`: an `asp_mode` enum (`ASP_FIT`, `ASP_14_9`, `ASP_FILL`), a function for the shown shape `S` from `a`, the TV and the mode, and one for the texture window (x, y, w, h in 0..1) that cuts a shape `a` picture to `S`. Pure, host-tested in `wii/tests/test_aspect.c`.
- `settings.h` / `settings.c`: `ctw_settings` gains the mode; parse, format and equal handle it. Tests in `wii/tests/test_settings.c`.
- `media/player.c`: a setter for the mode, like `pl_set_widescreen`, and a 0..1 amount on `pl_draw` for how much of the cut to apply (1 full screen, the zoom's progress in the preview). `media/yuvgx.c`: `yg_draw` takes the texture window.
- `home_layout.c`: the switch's rects and three new buttons, host-tested in `wii/tests/test_home_layout.c`. `screens/homemenu.c` draws and handles it.
- Docs: the key in `docs/formats.md`, text in `docs/USER-GUIDE.md`, a CHANGELOG "Unreleased" line, a checklist item in `docs/testing/wii-checklist.md`.

## Left to the builder

- Whether the track drawing is shared with the Settings screen's or kept in `homemenu.c`.
- Exact function names and signatures, matching the sibling files.
