# cable-twii Wii app redesign ("at system detail")

**Source:** the deck "cable-twii — Wii app redesign" (11 slides, 1920×1080 mocks of the 640×480 frame as it shows on a 16:9 TV; the Slides artifact at https://claude.ai/artifact/8ACfrkasCfWZuKYPmrot8S, slides under `project/slides/*.html`). This document turns the deck into values the app can draw, and records every call that the deck left open. The deck wins where the two disagree on looks; this document wins on behaviour.

## Intent

Every screen of the Wii app gets the depth of a console shell: top-lit gradients, inner highlights, soft shadows, a curved system bar, glossy accent parts and richer pointer states. The palette, the type (M PLUS Rounded 1c Medium and ExtraBold), the 640×480 grid, the sounds, the timings and the controls stay as they are. Nothing copies Nintendo's artwork.

## Goals, non-goals, anti-goals

- Goals: the guide, full-screen OSD, channel chip, volume panel, up-next card, HOME menu, intro, empty state and error dialog look like the deck, at the deck's positions and sizes.
- Non-goals: new features beyond the deck (no new screens, no new settings), no changes to playback, tuning, storage or the engine, no change to the host-test harness beyond the modules named below.
- Anti-goals: a slower guide (it must still draw at 60 fps while the preview plays), a heavier build (no new fonts, no large textures), anything that needs Nintendo's assets.

## Performance without hardware

No real Wii is available during this work (every hardware run is still blocked on the owner, see `docs/STATUS.md`), and Dolphin's frame rate says nothing about the console. So the kit is built to stay cheap: rounded corners are triangle fans from one precomputed 12-step quarter-circle table (no per-vertex trigonometry, unlike `GRRLIB_Ellipse`); the system bar, whose curve is many slices, is rendered once into a texture (`GRRLIB_CompoStart`/`GRRLIB_CompoEnd`) and drawn as one image afterwards; every shadow is at most three rounded rectangles. The debug overlay build is run in Dolphin after each screen to catch gross regressions, and the hardware checklist keeps its 60 fps item for the owner.

## Conversion rules

- Slide pixel to Wii pixel: divide x and widths by 3, y and heights by 2.25. Font sizes divide by 2.25 and round to a whole pixel.
- Anything square on the TV (icons, discs, dots, segments, the pointer) is given its height in Wii pixels; its width is that height times `ui_pixel_x()` (0.75 on 16:9, 1.0 on 4:3), as `icons_draw` already does. Corner radii use the same rule, as `rounded_rect` already does. Parts placed to the right of a square part flow from the square's real right edge (the x values below are the 16:9 ones), so on 4:3 nothing overlaps: `home_layout_make`, `osd_chip_layout` and `sysbar_layout_for` take `pixel_x` like `guide_layout_for` does.
- Font weight 700 and 800 both mean `FONT_BOLD`; 500 means `FONT_BODY`. Letter-spacing is not available and is dropped. Category headers are drawn in upper case (ASCII letters only).
- Text is positioned by the top of its em box, which is how `ui_text` places text. Where the deck gives a box with line-height above 1, the em top is the box top plus half the leading.
- GX has no blur. Every shadow is three stacked rounded rectangles of growing size and falling alpha (the sum of the three alphas is the shadow's alpha). Every ring ("glow") is one rounded rectangle expanded by the spread. Vertical gradients are vertex colours on quads (`GRRLIB_GXEngine`), split at every stop. Radial blooms are one 64×64 alpha texture drawn scaled.
- All values below are Wii pixels unless marked "slide".

## Tokens (added to `wii/source/ui/theme.h`)

| Token | Value | Where |
|---|---|---|
| `THEME_GLOSS_BOTTOM` | `#F8FAFC` | default bottom of a panel gradient; white at the top |
| Panel gradient variants | list: white to 70% then `#F8FAFC`; preview: white to `#F8FAFC`; info and empty card: white to 60% then `#F8FAFC`; header, OSD, up-next card, logo panel: white to 55% then `#F4F7FA`; chip and volume panel: white to 50% then `#F1F4F7`; HOME top bar: white to 60% then `#F1F4F7`; HOME bottom bar: white to 50% then `#E9EDF1`; system bar: white, `#F8FAFC` at 50%, `#E9EDF1` | each panel names its variant |
| `THEME_PANEL_HIGHLIGHT` | white, 1 px, inside the top border | every panel, pill and bar |
| `THEME_PANEL_SHADOW` | `#344A60` at 10%, offset y 2, blur 5 | panels over the stripes |
| `THEME_VIDEO_SHADOW` | black at 32%, offset y 5, blur 16 | OSD, chip, name pill, volume panel, HOME bars, up-next card (at 50%) |
| `THEME_ACCENT_LIGHT` / `THEME_ACCENT_DARK` | `#7FD0EF` / `#2A8FB3` | accent gloss: light at the top, `#34BEED` at 55%, dark at the bottom (progress fills use 60%) |
| `THEME_RING` | `#34BEED` at 25%, spread 4 (vertical; 4 × `pixel_x` horizontal) | pointed rows, the tuned chip, the up-next card, the logo panel, the newest volume segment (spread 2) |
| `THEME_RING_STRONG` | `#34BEED` at 60%, spread 4 | pointed pills |
| `THEME_PILL_BANDS` | white 0–46%, `#EEF1F4` at 54%, `#E6EAEE` at 100% | every pill at rest; flipped when pressed |
| `THEME_PILL_SHADOW` | `#344A60` at 12%, offset 1, blur 4 | pills on light backgrounds; pills over video use `THEME_VIDEO_SHADOW` |
| `THEME_TRACK_INSET` | `#C7CED4`, 1 px line at the top of a track | progress tracks, unlit segments |
| `THEME_SCRIM` | black 0% at mid-screen to 55% at the bottom | behind the OSD |
| `THEME_HINT_FILL` | `#14232F` at 62% | the "A Watch" hint |
| `THEME_TEXT_CLOCK` | `#777788` | clock, date, HOME menu "now playing" |
| `THEME_TEXT_LABEL` | `#555555` | "HOME Menu" label, empty-state body, pointed row line, HOME "Volume" |
| `THEME_TEXT_FADED` | `#AAAAAA` | the commercial-break pill |
| `THEME_WARM` | `#F3CF8A` | the warm dot of the commercial-break pill |
| `THEME_STRIPE_DARK_A` / `_B` | `#262626` / `#222222` | up-next background |
| `THEME_HEADER_H` | 48 | guide and empty-state header |
| `THEME_SYSBAR_H`, `THEME_SYSBAR_HUMP`, `THEME_SYSBAR_HUMP_TOP_W`, `THEME_SYSBAR_HUMP_SLOPE_W` | 47, 18, 79, 47 | the system bar |
| `THEME_CHIP_GLOW_MS`, `THEME_CHIP_NAME_MS`, `THEME_INTRO_DOT_MS` | 600, 200, 300 | chip ring fade, name pill drop, intro dot pulse |

Existing tokens keep their values. `THEME_LOGO_DURATION_MS` (2500), `THEME_HOVER_DEBOUNCE_MS` (300), `THEME_CHIP_DURATION_MS` (2000), `THEME_OSD_TIMEOUT_MS` (3000), `THEME_PREVIEW_EXPAND_MS` (350) are unchanged.

## Parts (the UI kit, `wii/source/ui/draw.c`)

- **Card**: border 2 px `#D6DBE0`, 1 px white highlight inside the top border, gradient fill (variant above), radius 18 (rows 12, pills full). Shadow: panel or video.
- **Pill**: full radius, bands, 2 px border `#C9CFD5`, highlight, pill shadow. Pointed: 3 px border `#34BEED` and the strong ring. Pressed (A held while pointed): bands flipped, drawn 2 px lower, a `#344A60` at 20% shade across the top 3 px instead of the highlight, the normal ring. Label: bold, size per use, centred, ellipsized to the pill width minus 16.
- **Progress bar**: track `#E1E5E9` with the inset line, full radius; fill in the accent gloss (60% stop), drawn when it is at least as wide as it is tall.
- **Volume segments**: lit: accent gloss with a 1 px white at 50% line at the top; unlit: `#E1E5E9` with the inset line; the last lit segment carries the ring at spread 2. Ten segments, gap 3, radius 3.
- **Number pill**: text bold, zero-padded to two digits ("01"), height 19, width max(22, text + 10), 2 px border `#D6DBE0`, text `#999999`; tuned: accent gloss fill, border `#2A8FB3`, white text.
- **Battery**: "P1" bold `#999999`, then a white pill with a 2 px `#C9CFD5` border holding four bars; lit bars `#7FD0EF` to `#34BEED`, unlit `#E1E5E9`, radius 3. Two sizes: header (pill 36×22, bars 4×12, gap 3, text 12 px) and HOME bar (pill 42×27, bars 5×15, gap 3.6, text 13 px). `home_battery_bars` maps the level.
- **Accent tile**: rounded rectangle in the accent gloss (chip number, HOME badge).
- **Radial bloom**: an alpha texture; white at 85% behind the guide (centre 50%/30%, radius 70% of the screen), white 100% to 70% to 0% behind the intro (centre 50%/45%), `#34BEED` at 16% behind the up-next card (centre 50%/45%, radius 60%).
- **Glyphs**: two white PNGs tinted at draw time: a house outline (`glyph_home.png`) and a play triangle outline (`glyph_play.png`), generated by `tools/make-glyphs.py` with Pillow next to the logo. The same script regenerates `cursor.png` as a glossy disc (radial `#EEFAFF` to `#7FD0EF` at 30% to `#34BEED` at 60% to `#2A8FB3`) with a 2 px white rim and a 1 px `#2A8FB3` edge, 32×32 as today. `cursor_shadow.png` stays.
- **Dashed rounded rectangle**: for the empty-state placeholder: 1.5 px thick quads along the outline, dash 4, gap 4, radius 18.

## Pressing

Pills act on release: while the pointer is on a pill and A is held, the pill draws pressed; when A is released on the same pill the select tone plays and the action runs. Entering a pill plays the hover tone and the rumble pulse, as today. Rows in the guide list keep selecting on press. A new `in_released` reads `WPAD_ButtonsUp`. The press state lives in a small host-tested module (`wii/source/ui/press.c`): `ps_update(state, hot_id, pressed, held, released)` returns the id that fired, or 0.

## Screens

### Header (guide, empty state)

Bar 0,0 640×48: header gradient, bottom 1 px `#D6DBE0`, shadow `#344A60` at 8% (offset 2, blur 6) under it. Logo at x 19.3, 25 tall, vertically centred (y 11.5). Right: "P1" 12 px bold at x 567.6, y 17.3; battery pill at 585,12.7 36×22 with bars at x 590.3 + 7 i, y 17.6, 4×12.

### System bar (guide, empty state)

Body 0,433 640×47 plus a hump: flat top 79 wide centred (x 280.7 to 359.3) at y 415, joined to the body by S-curves 47 wide on each side (from x 233.3 to 280.7 and 359.3 to 406.7). Fill: system-bar gradient; outline 1 px `#D6DBE0`; 1 px white highlight below the outline; a 5 px `#344A60` at 7% band above the outline. The curve is drawn as a strip of vertical slices (one per 2 px) along a cubic ease between the two levels.

Contents: clock "HH:MM" 28 px bold `#777788` centred at 320, em top y 420; date "Thu 22/10" 12 px body `#777788` centred, y 453 (weekday abbreviation, day/month, from the Wii clock: `gt_date`). HOME disc at 21.3,437.3, 37 tall (28 wide on 16:9): radial white to `#F1F4F7` at 60% to `#DFE5EA`, 2 px border `#C9CFD5`, panel shadow, highlight, house glyph 19.6 tall `#34BEED` centred; label "HOME Menu" 12 px bold `#555555` at x 55.3, y 449. Channel pills "◀ CH" and "CH ▶" 58×32 at x 496.7 and 560.7, y 440.9, labels 13 px bold. The house glyph is `#34BEED`. The HOME disc and label act as one button that opens the HOME menu. "◀ CH" moves the guide highlight to the previous channel (the lower number, up the list, as D-pad Up does) and "CH ▶" to the next one (down the list, as D-pad Down does). The channel pills are not drawn when there are no channels. Layout and hit-testing live in `wii/source/sysbar_layout.c` (host-tested); drawing and input in `wii/source/screens/sysbar.c`.

### Guide

Grid (`guide_layout.c`): inset 19.3 left and right, top 58.7, bottom 409.8; list 44.7% of the content width; column gap 3.2%; preview 16:9 by `pixel_x` with a 4% row gap above an info panel of at least 101.8. On 16:9 this gives list 19.3,58.7 268.7×351.1, preview 307.3,58.7 313.3×235.1, info 307.3,308 313.3×101.8.

List panel: list gradient card. Rows are inset 6.7 left and 14 right (width 248 on 16:9), 64 tall, 6 apart (unchanged `guide_model`). Scrollbar when the rows overflow: track 3 wide `#E1E5E9` at panel right minus 9.3, 17 in from the top and bottom of the panel, full radius; thumb `#C9CFD5` sized view/content.

Category header (28 tall): label upper case 12 px bold `#999999` at x +6.7, em top +8; rule 1 px `#E1E5E9` from the label's right plus 6 to the row's right, at the label's middle.

Channel row: icon 44 tall at x +7.3, centred vertically, radius 22% as today; name 16 px bold `#444444` at text x = icon right + 7.3, em top +13.4; line 13 px body `#999999` at em top +34; number pill at the right edge minus 3.3, vertically centred, tuned when the row's channel is the one playing. Pointed row: ring, `#34BEED` at 20% shadow (offset 3, blur 7), gradient `#F7FDFF` to `#EEFAFF` at 55% to `#E3F6FD`, 2 px border `#34BEED`, highlight, line in `#555555`, and a progress bar 4.4 tall at em top +52.6 from text x to the number pill minus 7.3. Rows at rest draw no bar and no times. Icons that fail to load keep the grey placeholder tile.

Preview: preview gradient card, frame 5, inner radius 13, video or the up-next text inside as today. "A Watch" hint while the preview shows a channel: pill at inner x +9, inner bottom minus 33.8, 73.3×26.7, fill `#14232F` at 62%; "A" disc 19.6 tall at +2.7,+3.6 (white to `#E6EAEE`, "A" 12 px bold `#444444` centred); "Watch" 12 px bold white at the disc's right plus 4, em top +6.4.

Info panel: info gradient card. Icon 48 tall at x +11.3, vertically centred (the show's icon, else the channel's). Name (the channel) 18 px bold `#34BEED` at x = icon right + 8, em top +22.2. Episode "SxxExx · title" 13 px bold `#444444` at em top +46.2 (for an ad: "Commercial break"; for a gap: the up-next line as today). Times "HH:MM – HH:MM" 12 px body `#999999` at em top +66, followed by a progress bar 6.2 tall from the times' right plus 4.6 to x 212 (relative), at +69.6. Divider 1 px `#E1E5E9` at x +220, from +17.6, 66.7 tall. Right column at x +228.7, 73.3 wide: "Up next" 12 px bold `#34BEED` at +15.1; the next episode's "SxxExx · title" 12 px bold `#444444` wrapped to two lines (line height 14.4) at +31.6; the countdown "M:SS" 21 px bold `#34BEED` at +63.2. The description is no longer shown.

"Next episode" means the first airing after the current one whose kind is episode (`tn_next_episode`, host-tested); ads in between are skipped. The countdown counts to that episode's start. When there is no next episode today the column shows "Up next" alone (the known last-airing gap stays).

### Full screen

Scrim: black 0% at y 248.9 to 55% at y 480, drawn only while the OSD panel shows.

OSD panel (`osd_layout.c`): 26,348 588×108, OSD gradient at 97% alpha, radius 18, video shadow, highlight. Icon 48 tall at +14,+30. Name 18 px bold `#34BEED` at +60.7,+22.3, followed at +5.3 by a number pill 16 tall ("02", 11 px). Line 16 px bold `#444444` at +60.7,+47.2. Start time 12 px body `#999999` at +60.7,+70.8; bar 8 tall from the start time's right plus 5.9 to x +352, at +74.3; end time right-aligned at x +385.3. Divider at x +396, +16.2, 75.6 tall. Icon 40 tall at +407.3,+34; "Up next" 13 px bold `#34BEED` at +445.3,+20.4; next title 13 px bold `#444444` two lines (line height 16) at +38.1; "at HH:MM" 12 px body `#999999` at +73.7; countdown 28 px bold `#34BEED` right-aligned at x +574, em top +40. The panel has no buttons, as the deck draws it: the pill row of the old OSD (◀ CH, CH ▶, Guide, −, +) is removed on purpose, so in full screen the D-pad flips channels, B returns to the guide and + and − change the volume; the pointer only presses the volume panel's pills. This deliberately changes §7.2 of the 2026-10-20 design spec and the hardware checklist item about clicking CH ▶, which is rewritten. The slide-in from the bottom stays.

Chip: 546.7,16 81.3×57.8 (wider when the number needs it), chip gradient, 2 px border `#D6DBE0`, full radius, video shadow (30%), highlight. Number tile 40 tall, width max(30, text + 12), at +7.3,+8.9, radius 10.7, accent gloss, number 24 px bold white centred. Channel icon 40 tall after a 4.7 gap, radius 9. For the first 600 ms after a channel change the chip carries the ring, fading out. The channel name pill drops from behind the chip over 200 ms to y 84.4, right-aligned to the chip, 28.4 tall, width text + 28 (min 60), pill bands, 2 px `#C9CFD5`, video shadow (30%), label 13 px bold `#444444`; it shows as long as the chip does.

Volume panel: "Volume" 13 px bold white centred at em top 387; panel 190,412 260×44, chip gradient, full radius, video shadow (35%), highlight; "−" pill 24×32 at +5.3,+6 and "+" pill at +230.7,+6, labels 20 px bold; segments 185.3 wide at +37.3,+14, 16 tall. The panel shows for 2 s after any volume change, including presses on its own pills, and while it shows the OSD panel does not draw (`osd_panel_visible` is `osd_visible && !osd_volume_visible`). Its pills are the only pointer targets in full screen.

Up-next screen (a gap between programmes): dark stripes (3 px, `#262626`/`#222222`), blue bloom, the chip. Card 160,111.1 320×222.2: OSD gradient, radius 18, ring, video shadow (50%, offset 8, blur 22), highlight. Dot 8 tall `#34BEED` with a ring (spread 2.7) at +19.3,+28; "Up next" 18 px bold `#34BEED` at +30.7,+22.2; icon 53 tall at +19.3,+55.1; title "SxxExx · title" 20 px bold `#444444` at +68.7,+60.8; show name 13 px body `#999999` at +68.7,+86.8; divider 1 px at +121.8 from +19.3 to +300.7; "at HH:MM" 15 px bold `#444444` at +19.3,+142.9; bar 6.2 tall at +77.6,+148.9, 156.3 wide, filling from the start of the gap toward the episode's start; countdown 32 px bold `#34BEED` right-aligned at +300.7, em top +136. The card describes the next episode. When the airing right after the gap is a commercial break, a faded pill is drawn under the card at 220,364.4 200×35.6: white at 8%, 2 px border white at 14%, full radius, warm dot 7 tall at +33.9,+14.3, "Up next: Commercial break" 12 px bold `#AAAAAA` at +44.6,+10.4.

### HOME menu

Dim 60% black (unchanged). Top bar 0,0 640×72: HOME-top gradient, bottom 1 px `#D6DBE0`, video shadow (30%). Badge disc 37 tall at 26,16.9: accent gloss, ring (spread 2), white at 50% highlight, house glyph 19.6 tall white centred. Title "HOME Menu" 24 px bold `#444444` at x 62, em top 22.4. Now playing, right-aligned at x 614, em top 27.6, 13 px body `#777788`: the channel name, then " · SxxExx" when an episode is on.

Pills: "Wii Menu" 340×60 at 150,133.3 with the house glyph 24 tall in `#34BEED` before the label (glyph, 8, label; the group centred), label 20 px bold; "Back to Homebrew Channel" 340×60 at 150,221.3, label 20 px bold; "Resume" 190×40 at 225,311.1, label 15 px bold. Pills over the dim use the video shadow (35%). Bottom bar 0,408 640×72: HOME-bottom gradient, top 1 px `#D6DBE0`, video shadow above (30%), highlight. "Volume" 13 px bold `#555555` at 26,436.4; ten segments 10 wide, 16 tall, gap 2.35, from x 68.2 (no ring on the newest); "P1" 13 px bold at 554.1, battery pill at 572.3,431.1 (HOME size). Layout in `home_layout.c` (host-tested).

### Intro

Stripes and the white bloom. Logo panel 333.3×124.4 centred at 320,231.1 (y 168.9), logo-panel gradient, radius 18, ring, `#344A60` at 16% shadow (offset 7, blur 20), highlight. The wordmark (`logo.png`) sits centred in the panel at 285 wide (so the real asset fits where the deck typed one). Three dots 8.9 tall at y 328.9, x 304, 316.7, 329.3: the active dot `#34BEED` with a ring (spread 2.2), the next `#7FD0EF`, the last `#BFE6F6`; the active dot advances every 300 ms.

Timing (`logoanim.c`, host-tested): 0–600 ms the panel and wordmark scale from 60% and fade in (ease-out). 600–1400 ms a shine crosses the panel: a band 82.8 wide tilted 22° (its gradient transparent to `#EEFAFF` at 95% to transparent) travels from the panel's left to its right, clipped to the panel. 1400–2000 ms the wordmark shrinks and moves to its header place (19.3, 11.5, 25 tall) while the panel, dots and bloom fade out and the guide header, panels and system bar slide 40 px into place. A press of A skips to the end, as today. The frame carries `panel` (x, y, w, h) and `panel_alpha` in addition to today's fields.

### Empty state (no channels)

Drawn by the guide when the channel list is empty: header, white bloom, system bar without channel pills. Card 153.3,88.9 333.3×275.6 (info gradient). Placeholder tile 80 tall at 290,114.7 (60 wide on 16:9): `#E6EAEE`, dashed 1.5 px `#C9CFD5`, radius 18, play glyph 37 tall `#C9CFD5` centred. "No channels yet" 24 px bold `#444444` centred at em top 209. Body 13 px body `#555555` centred, wrapped to the card width minus 38, line height 18.6, at em top 250: "Add one on the Channels page in cable-twii Studio, then plug the drive back in." Pill "Back to Homebrew Channel" 177.8×39.2 at 231.1,299.4, label 14 px bold; it quits to the Homebrew Channel.

### Error dialog

Not in the deck. It keeps its layout and gets the kit: info gradient card with the panel shadow, the pill with pointed and pressed states, acting on release.

## After the screens

The cable-twii Design System artifact mirrors `theme.h` token for token; it is re-synced once the tokens are final (the memory note "cable-twii Design System artifact" says how).

## Known approximations

- Letter-spacing on category headers is dropped.
- The pressed pill's inner shade is a flat band, not a blurred inset.
- The dashed border is drawn with straight dashes along the rounded corners.
- The diagonal shine is clipped to the panel's bounding box, so it can touch the rounded corners by a pixel.

## Left to the builder

Exact alpha splits of the three shadow layers; the bloom texture size; how the system bar's curve is tessellated; the wrap rules for two-line titles (use `ui_text_wrap`); the order of drawing within a frame as long as shadows go under panels and the cursor stays on top.
