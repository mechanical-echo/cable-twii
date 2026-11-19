# cable-twii — Today's list and skipping on looping channels (design)

Date: 2026-11-19
Builds on: [Wii redesign](2026-10-22-wii-redesign-design.md), [server source](2026-11-02-server-source-design.md), [picture setting](2026-11-19-picture-setting-design.md)
Mockup: [Wii Today list and the Today pill](mockups/wii-today-and-skipping.html), variant A of the pill chosen by the owner on 2026-11-19
Reviewed by a builder check on 2026-11-19; its one open question (which day a timed channel's list shows after midnight) was answered by the owner.

Decisions marked **(my call)** were made by the designer, not the owner, and may be revisited. Everything else was stated or confirmed by the owner.

## Intent

The owner wants to play around with and test channels on the real Wii without waiting on the clock. Two features:

1. **Today's list:** the whole day's lineup for any channel, timed or looping, on the Wii, with the same content as Studio's Preview.
2. **Next and previous on looping channels:** step through a looping channel's schedule one item at a time, episode or ad. Timed channels stay live TV.

"Nothing fancy": this is a tool for trying things out, not a viewer feature, so skips are kept only until cable-twii restarts.

Success: on the Cartoon Network loop (2 episodes then 1 ad), D-pad → jumps to the start of the next item, ← to the start of the previous one, and the guide, info panel and Today's list all agree with what plays. Pressing 1 shows today's list with what's on now marked. On a looping channel, A on a row plays from there.

## Goals, non-goals, anti-goals

- Goals: Today's list for every channel, opened from fullscreen and the guide. Prev/next on looping channels in fullscreen. Picking a row in the list jumps there on looping channels. Works the same from the drive and the server.
- Non-goals: saving skips across a restart (owner: "until restart"). Lists for other days. Skipping on timed channels. Seeking inside an item. A "back to live" control (a restart does it). Any change to `engine/` or Studio.
- Anti-goals: timed channels behaving differently, apart from D-pad ← → no longer flipping channels. Times on screen that disagree with the Wii's clock after a skip. Anything written to disk for this.

## Behaviour

### Controls (owner's choice: D-pad + 1)

| Where | Button | Does |
|---|---|---|
| Fullscreen | D-pad ↑ / ↓ | next / previous channel (unchanged) |
| Fullscreen | D-pad → / ← | looping channel: next / previous item. Timed channel: shows the notice `This channel is live` **(my call)**. These no longer flip channels. |
| Fullscreen | 1, or pointer + A on the `1 Today` hint | open Today's list for the channel playing |
| Guide | 1, or pointer + A on the `1 Today` hint | open Today's list for the highlighted channel |
| Today's list | D-pad ↑ ↓, pointer | move the highlight; the list scrolls to keep it in view, and the pointer near the top or bottom edge scrolls it, as in the guide |
| Today's list | A | looping channel: play from the highlighted row. Timed channel: nothing **(my call)** |
| Today's list | B or 1 | close **(my call: 1 toggles)** |
| Anywhere | HOME | HOME menu, over the list too |

D-pad ← → stay unused in the guide **(my call)**.

### Skipping (looping channels only)

- **Next** jumps to the start of the next item in the channel's schedule: the next episode, the next single ad, or an up-next gap. **Prev** jumps to the start of the item before the current one, whatever it is and however far into the current one you are. Each press acts from where the channel is now, so quick presses add up.
- After a jump the item plays from its start. The info panel shows for its usual few seconds even without the pointer, so you can see where you landed **(my call; today the panel needs the pointer)**. The channel-change sound plays **(my call)**.
- Each looping channel keeps its own jump for as long as cable-twii runs. Flip away and back and it is where you left it. A restart, Reload channels or switching the source (drive / server) puts every channel back on the clock.
- **Times on screen stay real clock times.** After a skip, the guide's info card, the info panel's start, end and "at" times, the up-next card and Today's list show when things will actually be on by the Wii's clock. Countdowns and progress bars already are relative and do not change.
- A skip can cross midnight both ways. It then plays from the next or previous day's loop, which is a different random draw, as the engine already builds each day.

### Today's list

As in mockup frame 4:

- A card over the current screen, which keeps running behind a full-screen dim (`THEME_SCRIM`). In fullscreen the video and sound go on. On the guide the preview goes on.
- **Header:** channel icon, name and number pill, and `Today · <weekday dd/mm>` with the date of the lineup shown (see *The content*). On looping channels a grey `Looping` tag sits on the right.
- **Rows:** one per airing, except that consecutive ads merge into one `Ad break (2 ads, 1:30)` row, as Studio's Preview does. The duration uses Studio's `m:ss` format. An up-next gap shows `Up next`. Episode rows: `HH:MM – HH:MM`, the show's icon (or the channel's), show name, `SxxEyy`, title. Long text ends in "…". A time on the day after the list's date gets Studio's ` +1` (`00:30 +1`), one on the day before gets ` −1` (only possible after a skip across midnight) **(my call)**. The time column is wide enough for `23:30 – 00:45 +1`.
- **What's on now:** pale blue row (`THEME_ROW_ACTIVE_FILL`) with a blue edge and a thin progress line, moving on as time passes. Earlier rows are drawn faded (`THEME_TEXT_FADED`).
- **The highlight** starts on the now row. The list opens scrolled so the now row is the third row visible, clamped at the ends **(my call)**.
- **The content is the broadcast day that's on now (owner's choice).** That is the whole lineup holding the item on now, straight from the tuner, so it is exactly what the Wii airs and the same rows Studio's Preview shows for that date. Looping lineups run 00:00–24:00, so this is the channel's own day (after a skip, possibly not the Wii's date). A timed lineup runs 24 hours from its first slot, e.g. 07:00 to 07:00, so at 01:30 a timed channel shows *yesterday's* lineup (`tuner_entry.yesterday`), with yesterday's date in the header. There is always a now row, unless the channel has no lineup at all. If the lineup holding now changes while the list is open, the list rebuilds **(my call)**.
- **Footer hints:** `A Play from here` and `B Close` on looping channels, `B Close` only on timed ones, and `◀ ▶ skip in fullscreen` on the right on looping channels.
- **A on a looping row** jumps the channel so that row (for an ad break, its first ad) starts now, then closes the list. From fullscreen you stay in fullscreen. From the guide the channel is tuned and opens fullscreen **(my call)**.
- Sounds and rumble follow the guide: select on open, back on close, hover on highlight changes.

### The `1 Today` hint (variant A)

- The dark see-through pill with a white button disc, like the guide preview's `A Watch` hint (`THEME_HINT_FILL`, `THEME_HINT_DISC_BOTTOM`, 26.7 tall, disc 19.6, label bold 12).
- **Fullscreen:** right-aligned with the info panel's right edge, 8 px above the panel. Shown, and sliding, with the panel on every channel. The pointer on it draws the strong ring and plays the hover sound. A opens the list.
- **Guide:** in the preview's bottom-left, 6 px right of `A Watch`, shown whenever a channel is highlighted, even before its preview plays. Pointer + A on it opens the list for the highlighted channel, taking precedence over the preview's A (watch).

Pixel numbers are the mockup script's (`todayCard`, `darkHint`, `osdPanel`). They are drawn at `pixel_x` like the rest of the app.

## Approaches

1. **A per-channel shift held in the tuner (chosen by the owner).** Each looping channel's tuner entry keeps a signed `shift` in ms, applied when the tuner works out what is on: channel time = wall clock + shift. Next and prev change the shift. Everything that asks the tuner what is on (live playback, guide, info panel, HOME, the list) follows without change. Only displayed clock times subtract the shift. Crossing midnight works because the tuner already rebuilds per day. A skip is a re-tune, so drive and server mode need nothing new.
2. A playlist cursor, playing item after item from an index once you skip. It needs a second playback mode in `live.c` and second code paths for countdowns and up-next. Not chosen.
3. Changing the engine so a loop can start elsewhere. It touches `engine/` and Studio's preview, and breaks "Studio shows what the Wii airs". Not chosen.

## Shape of the code (my calls)

- **`tuner.h` / `tuner.c`:**
  - `tuner_entry` gains the shift.
  - `tn_current` applies it, rolling the day forward or back with `ctw_date_next` / `ctw_date_prev`. With shift 0 it behaves exactly as today.
  - New functions:
    - the previous airing (within today's lineup, else yesterday's last);
    - next / prev skip, which refuses timed channels and returns whether it moved;
    - jump-to-airing;
    - reading the shift;
    - which lineup (today or yesterday) holds the airing on now, with its date.
  - Host tests in `wii/tests/test_tuner.c` use the fixture's looping channel 2 and timed channel 1. They cover next and prev in mid-item, at the first item of a day and at the last, crossing midnight both ways, quick repeated presses, and timed channels refusing.
- **`today_model.h` / `today_model.c`** (pure):
  - rows from one lineup (ads merged, with count and total);
  - the now row for a time of day;
  - the row holding an airing;
  - scroll-to and hit math for fixed-height rows.
  - Tests in `wii/tests/test_today_model.c`.
- **`guide_text.c`:** the `Ad break (N ad(s), m:ss)` label, Studio's `m:ss` duration, and a row time range with the ` +1` / ` −1` day marker. Tests in `test_guide_text.c`.
- **`today_layout.h` / `today_layout.c`** (pure): the card, header, list, row columns, footer hints, and the two `1 Today` hint rects (fullscreen and guide). Tests in `test_today_layout.c`.
- **`osd.c`:** a skip shows the panel for the usual timeout without the pointer. Tests in `test_osd.c`.
- **`screens/today.c` / `today.h`:** the list overlay (open for a channel, update, draw).
  - `main.c` runs it like the HOME menu. It updates before the screen, takes the input while open (so B closes it and does not also leave fullscreen), keeps the cursor drawn, and gives way to HOME.
- **`screens/full.c`:** D-pad ← → skip or show the notice, 1 opens the list, and the hint is drawn and hit with the panel.
- **`screens/guide.c` / `guide_info.c`:** 1 and the hint.
- **`panel.c`, `guide_info.c`, `upnext.c`:** displayed clock times subtract the channel's shift.
- **`wii/tests/Makefile`:** the new pure sources.
- **Docs:**
  - the controls section of `docs/USER-GUIDE.md` (also dropping its stale line about buttons on the info panel, which has had none since the redesign);
  - a CHANGELOG "Unreleased" line;
  - items in `docs/testing/wii-checklist.md`.

### Builder's calls, from the review

- A shift longer than a day (many presses) is handled with day arithmetic, not a single step.
- The info panel's and guide info card's cached text is keyed on the shift too, so a jump to the item already on still refreshes its times.
- Reload channels and the automatic server reload (a new library revision, at boot of the guide or midnight) close the list, since it points into the lineups, and clear the skips.
- While the list is open, D-pad ← → do nothing.
- Season 0 shows as `S00Exx`, as `gt_episode` already does.

## Left to the builder

- Exact names and signatures, matching the sibling files.
- How row text is cached for a list of about 200 rows.
- Whether the list's edge scrolling reuses `gm_edge_scroll`.
