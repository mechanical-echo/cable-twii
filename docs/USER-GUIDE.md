# cable-twii owner's guide

cable-twii turns your modded Wii into cable TV. You prepare a flash drive (or SD card) on your Mac with **cable-twii Studio**, then start the **cable-twii** app from the Homebrew Channel on the Wii.

**The games drive is never formatted, repartitioned or written to by any step in this guide, by Studio, or by the Wii app.** The only drive you format is the small flash drive, and only if you choose to.

Not everything in this guide has been tried on a real Wii yet. Where behaviour depends on your hardware (for example, using two USB drives at once), the guide says so.

## 1. What you need

- A Wii with the Homebrew Channel installed.
- A flash drive (USB) or an SD card or SDHC card (up to 32 GB). This is where the shows live. It must be a different drive from your games drive.
- A Mac with Node 22 installed (for Studio).
- Your shows and ads as video files on the Mac.
- The cable-twii app from the release package: a folder called `cable-twii` containing `boot.dol`, `meta.xml` and `icon.png`.

## 2. Format only the flash drive

Skip this if your flash drive or SD card is already FAT32 (MS-DOS) and empty or only holds things you want to keep. Studio never formats anything, so this is the only place a drive is erased.

1. Plug in the flash drive (not the games drive). If you have the games drive connected to the Mac, unplug it now.
2. Open **Disk Utility** on the Mac and choose **View, Show All Devices**.
3. Select the **flash drive** in the sidebar.
4. Click **Erase** and set:
   - Name: anything, for example `CABLETV`
   - Format: **MS-DOS (FAT)**
   - Scheme: **Master Boot Record**
5. Click **Erase**.

**Warning: erasing deletes everything on the selected drive. Before you click Erase, double-check the drive name and its size. Make sure it is your small flash drive and not your games drive. If you are not sure, unplug the games drive first.**

Studio adds a second safety net. It refuses any drive that has a `wbfs` or `games` folder or `.wbfs` files, and any drive that is not FAT32. Studio itself never formats, erases or repartitions anything.

## 3. Start Studio

1. Open Terminal and go to the cable-twii project folder.
2. Run:

   ```
   cd studio && npm start
   ```

3. Studio needs Node 22. If the shell says "cable-twii Studio needs Node 22", run `nvm use` inside the `studio` folder and then `npm start` again.
4. Studio opens in your browser at http://127.0.0.1:4747/. It only runs on your Mac and is not reachable from other computers.

Studio has five tabs along the top: **Episodes**, **Channels**, **Shows**, **Ads** and **Drive**. Episodes is the home page: it is where you add videos and see them convert. Each channel has its own page with Today, Schedule, Shows and Settings, and every show, episode and ad has a page of its own.

## 4. Choose and set up the drive

On the **Drive** tab:

1. Click **Choose drive…** and pick the flash drive or SD card, or type its path (for example `/Volumes/CABLETV`) and click **Use this folder**.
2. Studio shows the drive type, free space and whether it is set up. If it says the drive cannot be used, read the reason. For example, a drive with a `wbfs` folder is refused because it looks like your games drive, and a drive that is not FAT32 is refused.
3. Click **Set up cable-twii on this drive**. This creates a `cable-twii` folder. Nothing else on the drive is changed.

You can also pick a plain folder on the Mac to try things out. Studio treats it as a test area and there is nothing to eject.

The Drive tab also has **Re-read library**. Studio re-reads the drive by itself after every change; use the button after you add or remove files by hand. It lists anything it skipped and why.

## 5. Add episodes (Episodes page)

The **Episodes** page is the home page.

- **Add episodes**: click **Pick a folder** and choose a folder of video files on the Mac. (On a server, drop files on the box or use the imports folder; see `docs/SERVER.md`.) The **Add episodes** page opens with one row per video:
  - a tick box: only ticked videos are added;
  - **▶** plays the source file in a small player, so you can check it is the right video before adding it. If the browser can't play that format it says "This browser can't preview this file";
  - season and episode, guessed from the file name, which you can change;
  - the title, the length and a note.
- **They go to**: Studio guesses the channel and show from the file names and your library. Change them with the **Channel** and **Show** lists. **New channel…** and **New show…** ask for a name (a new show can have an icon). If the files look like more than one show, each show gets its own box.
- Notes on a row:
  - **Already on air · Replace?**: that episode is already on the channel, so the row starts unticked. Tick it to replace the old file with this one. The episode keeps its name and title, and the old file stays on air until the new one has converted. If the conversion fails, the old file is kept.
  - **Already queued**: the same episode is already waiting to convert.
  - **Already in this list**: another row has the same season and episode.
  - **Already added**: this exact file was converted before.
  - **Too short** or **Can't read this file**: Studio can't use it.
- The bottom bar shows how many are ticked. **Add N episodes** starts converting and takes you back to the home page.
- Back on the home page, **In progress** lists the videos converting and waiting, grouped by show and season ("Gumball S03E20–E40 · 21 waiting"). **Recent** lists what finished or failed in the last 7 days, even after Studio restarts. A failed row shows the reason. **Retry failed** tries the failed ones again. A row that says **Add the file again** failed and its source file is gone, so add it again.
- Adding the same files again is safe. Files that are already on the drive are skipped.

## 6. Channels

The **Channels** tab lists your channels with what is on now, the mode, how many shows and episodes they have, and any problems ("No shows yet", "Has errors"). Click a channel to open its page. **Add channel** asks for a number, name, category and mode.

Folders on the drive that have shows but no channel are listed under **Not on a channel**. Give a channel that folder in its Settings to put the shows on air.

Each channel page has **Add episodes** (with that channel already chosen) and four tabs:

- **Today**: the day's lineup as the Wii will air it, with start and end times, shows, episodes and ad breaks. The airing on now is highlighted with its progress. Pick another day with the date field, and go back with **Today**. It is computed with the same engine the Wii uses, so what you see here is what airs on the Wii for the same date and time.
- **Schedule**: the schedule editor (section 7).
- **Shows**: the channel's shows by season, each episode with its length and status (On air, Converting, Queued or Failed). **Edit info** changes a show's name, icon and episode descriptions.
- **Settings**:
  - **Name** and **Category**: shown in the Wii guide. Renaming a channel keeps its folder, so its shows stay.
  - **Number**: 1 to 999. Two channels cannot share a number.
  - **Folder on the drive**: the channel's folder. If you leave it empty, Studio uses the name. Pick an existing folder to attach a channel to shows you already added.
  - **Mode**: **Timed** (a schedule with start times, like a TV station) or **Looping** (a list that plays in order and repeats all day).
  - **Ads**: on or off. When off, random ads are skipped on this channel.
  - **Logo**: choose a picture. It is saved right away.
  - **Save** writes the changes. **Delete channel** removes the channel but keeps its files on the drive.

A new channel gets a placeholder schedule so it is valid straight away. Build the real schedule on its Schedule tab.

### Shows, episodes and ads

The **Shows** tab lists every show, grouped by the channel folder it is stored in, with how many seasons and episodes it has and the channels it airs on. Click a show to open its page.

- **Show page:**
  - **Airs on** has one tick box per channel. Tick a channel to put the show on it: a looping channel gets a "Random episode ×1" rule at the end of its loop, and a timed channel asks which slot it goes into. Tune the rule in that channel's schedule editor. Untick a channel to take the show off it; Studio lists the schedule items that go before it removes them. The files never move.
  - Tick episodes to work on several at once: **Move to** a season (they are numbered after its last episode), **Add to a channel…** (they play there in the order shown, at the end of the loop or of a slot you pick), **Switch off**, **Switch on** or **Delete**.
  - **Edit info** changes the show's name, icon and episode descriptions. **Delete show** deletes the show and all its files.
- **Episode page:** click any episode (on a show page, a channel's Shows or Today tab, or a finished row on the Episodes page). It has:
  - a player that plays the converted file as the Wii gets it. Click the bar to jump. If the browser can't play it, it says "Can't play this file here";
  - the channels it airs on;
  - **Title**, **Season**, **Episode** and **Description**. Changing the title or number renames the file, and schedules that name the episode follow it. A number already used in the show is refused. In a show without seasons only the title can change;
  - **On air / Off air**. An episode switched off stays on the drive but the Wii doesn't play it on any channel. If a schedule names it directly, Studio lists those items and removes them. Switching it back on doesn't put them back;
  - **Delete episode**, which can't be undone. In a show without seasons the later episodes move up a number, and schedules that name them follow.
- **Ad page:** click an ad on the Ads tab. It has the same player, **Name**, **Plays on** (all channels or one channel), On air / Off air and Delete.

Studio refuses a change that would break a schedule (for example switching off every episode a rule can pick) and says which channel to fix first. Errors that were already there don't block. A file that is still converting can't be changed until it is done.

In server mode a Wii playing from the server picks up these changes when its guide opens, at midnight, or with Reload channels in HOME. Until then a switched-off episode may still air, and a renamed or deleted one is skipped.

## 7. Build schedules (Schedule tab)

Open a channel and its **Schedule** tab. Timed channels open zoomed out to the whole day (30 px per hour); looping channels open at 120.

- The **timeline** shows what airs and when. The **zoom** buttons change how much of the day you see.
- The **library panel** on the left lists episodes and rules for this channel. **Drag** an item onto the timeline, into a slot (timed) or onto the loop (looping). Drag blocks to reorder them. Click a block to edit it on the right. **Delete** removes the selected block.
- The ready-made rules are:
  - **Random episode**: picks random episodes (set how many with Count), without repeats inside the block.
  - **Random sequential run**: picks a random starting episode, then plays the following episodes in order.
  - **Fill slot** (timed channels only): fills the rest of the slot, with an optional number of ads between episodes.
  - **Random ad** and specific ads: the ads for this channel.
- On a **timed** channel, a **slot** is a start time. A slot runs until the next slot starts. **Add slot** asks for a time as `HH:MM` (24-hour, for example `07:30` or `19:00`). Click a slot's time to rename it. Two slots cannot start at the same time.
- A **looping** channel has no times. It plays its list in order and repeats it all day.
- **Undo** and **Redo** step through your changes.
- **Save** writes your changes to the drive. Studio refuses to save a schedule with errors and tells you which block to fix. With unsaved changes, Studio asks before you leave the tab or the page.
- **Reshuffle** picks a new random seed so every random pick changes. Save first, because Reshuffle is disabled while there are unsaved changes.
- If you edit the schedule in a second place (for example by hand), Studio may say "config.toml changed on disk — Reload". Click **Reload** to load the newer version. Your unsaved changes in the page are lost, so redo them afterwards.

### What the red overlap means

A block marked with a warning and "min overlap" runs past the start of the next slot. The part that does not fit is cut off when the next slot starts. This is a warning, not an error. You can leave it, make the block shorter (for example fewer episodes) or move the next slot later. A block outlined as an error is a real problem and must be fixed before Studio will save. The message names the channel, slot and item.

## 8. Ads

The **Ads** tab lists your ads: those for all channels first, then each channel's own. Click an ad to play it, rename it, move it or switch it off. **Add ads** opens the same Add page as episodes, with a destination of **All channels** or one channel. Ad files Studio can't use (for example too short) are listed under **Skipped** with the reason.

## 9. Done, then eject

When you finish, go to the **Drive** tab and click **Done — safe to eject**.

- This does not eject the drive. It finishes pending work and cleans up files the Mac leaves on FAT drives (the `._` files).
- It refuses while videos are still converting. Wait until **In progress** on the Episodes page is empty and try again.
- After the message says it is safe, eject the drive in Finder (right-click the drive, Eject) and then unplug it.

## 10. Put the app on the Wii's SD card

1. On the Mac, copy the `cable-twii` folder from the release package into the `apps` folder of the Wii's SD card, so you end up with `apps/cable-twii/boot.dol`, `apps/cable-twii/meta.xml` and `apps/cable-twii/icon.png`.
2. Eject the SD card and put it in the Wii.
3. Start the **Homebrew Channel**, pick **cable-twii** and start it.

If your shows are on the SD card itself, the same card can hold both `apps/cable-twii/` (the app) and `cable-twii/` (the shows, set up by Studio).

To start cable-twii straight from the Wii Menu instead, there is an optional channel; see `docs/INSTALL-CHANNEL.md`. It has its own safety checks, so read that guide fully before installing anything.

## 11. Which drive and USB port

This part is provisional. We have not yet tested two drives at once on a real Wii.

- The app looks at every USB drive, then at the SD card, and uses the first one that has a `cable-twii` folder. It never writes to a drive that has no `cable-twii` folder.
- The simplest setup that is known to work is the **SD card**. It is always supported, holds up to 32 GB (roughly 40 hours of video) and has no port choices.
- To use a USB flash drive, plug it in with the games drive unplugged first. Then, if you want both connected, try one port at a time and see whether cable-twii finds your drive. If it does not, use the SD card or swap drives when switching between games and TV.
- Which port works when both drives are plugged in has not been decided. This guide will be updated when the real-Wii test is done.

## 12. Set the Wii clock

The schedule follows the Wii's clock, so if the time is wrong the shows will be wrong. Go to **Wii Options, Wii Settings, Calendar** (it may be on the second page of the settings) and set the date and time. Do this once, and again if the Wii's battery has run flat.

## 13. Controls on the Wii

Point the Wii Remote at the screen to see the cursor. If you do not point it at the screen the cursor stays hidden, and you can still use the D-pad and buttons.

**Guide**
- Move the cursor over a channel and press **A** to open it fullscreen. The channel previews after a moment when you rest on it.
- **D-pad up and down** moves the highlight. **A** opens the highlighted channel.
- **1** (or point at **1 Today** on the preview and press **A**) opens today's list for the highlighted channel.

**Fullscreen**
- **D-pad up**: next channel. **D-pad down**: previous channel.
- **D-pad right** and **left** on a looping channel: jump to the start of the next or previous item in its schedule (an episode or an ad). Timed channels are live; they show *This channel is live*. A jump lasts until cable-twii restarts or reloads its channels.
- **1** (or **1 Today** above the info panel) opens today's list.
- **+** and **−**: volume (steps of 10, from 0 to 100).
- **B**: back to the guide.
- Point the remote at the screen to show the **info panel**; it hides itself after a few seconds without pointer movement. While the volume panel shows, point at its **−** and **+** and press **A**.

**Today's list**
- It shows the channel's schedule for the day as a channel's Today tab in Studio does: times, shows, episodes and titles, with ads grouped into breaks and what's on now in blue. On a timed channel after midnight it shows the schedule day that is still running.
- **D-pad up and down** or the pointer moves the highlight. On a looping channel **A** plays from the highlighted item. **B** or **1** closes it.

**HOME menu**
- Press **HOME** to open it. Choose **Wii Menu**, **Homebrew Channel** or **Resume** by pointing and pressing **A**. Pressing **HOME** again also resumes. It shows the remote's battery level.
- The bottom bar has **Picture**. **Fit** shows the whole picture with black bars when a show doesn't match the TV's shape. **14:9** cuts a little off the sides (or the top and bottom) and leaves thin bars. **Fill** cuts more and fills the screen: on a 4:3 TV it cuts 1/8 off each side of a 16:9 show. Nothing is ever stretched. Point at one and press **A**; the picture changes at once. The guide's preview always shows the whole picture. The choice is saved in `settings.toml` on the SD card, with the same rules as Settings (section 15).

**Power and Reset**
- **Power** (on the remote or the console) saves your place and turns the Wii off.
- **Reset** saves your place and returns to the Homebrew Channel.

The app remembers the last channel and the volume and picks them up next time. They are saved in `state.toml` in the `cable-twii` folder, and only on the cable-twii drive. The app never saves anything to a drive with a `wbfs` or `games` folder or `.wbfs` files at its top level, or to any drive that has no `cable-twii` folder.

## 14. Troubleshooting

### On the Wii

**No cable-twii drive found**
Plug in the drive prepared with cable-twii Studio, or put a cable-twii folder on the SD card.
- Cause: the app found no drive with a `cable-twii` folder.
- Fix: check the drive is plugged in firmly, was set up on the Drive page in Studio (it has a `cable-twii` folder), and is FAT32. Try the other USB port or use the SD card. Press the button "Back to Homebrew Channel" and start again.

**There's a problem with the schedule**
Followed by up to three lines such as "config.toml line 12: ..." or "library.toml: ..." and "Open cable-twii Studio to fix it."
- Cause: the schedule or library file on the drive has a mistake, for example the file was edited by hand, a show was deleted, or the drive was changed after the schedule was written.
- Fix: plug the drive into the Mac, open Studio, open the channel's Schedule tab and fix the blocks it points to (see "In Studio" below). Save, click **Done — safe to eject**, eject and try again. If it still fails, click **Re-read library** on the Drive tab first.

**The drive was disconnected**
Plug it back in and start cable-twii again.
- Cause: the drive was unplugged or lost power while a show was playing.
- Fix: plug it back in, press the button to go back to the Homebrew Channel, and start cable-twii again. If it happens without anyone touching the drive, try another USB port, a powered USB hub, or the SD card.

On all three screens press **Back to Homebrew Channel** with the pointer and **A**.

**Black screen on a channel**
If a video file is shorter than the library says, or a file is missing, the picture stays black until the next show starts. Click **Re-read library** on Studio's Drive tab to check the files.

**The Wii shows the wrong show or the wrong time**
Check the Wii clock (section 12).

**The cursor does not appear**
Point the remote at the screen and move it. Check the sensor bar is on.

### In Studio

**Drive messages (Drive tab)**
- "That folder doesn't exist." The path you typed is wrong. Use **Choose drive…** instead.
- "Pick the drive itself, not a folder inside it." Choose the drive's top level (for example `/Volumes/CABLETV`).
- "Choose your media drive or a test folder, not a system folder." Pick the flash drive or SD card.
- "This looks like your games drive (it has a wbfs or games folder, or .wbfs files). cable-twii will never write to it." You picked the games drive or a drive that contains game files. Choose the flash drive instead. Do not try to bypass this.
- "This drive is ... cable-twii needs a FAT32 (MS-DOS) drive." Format only the flash drive as described in section 2, then choose it again.
- "... is on a different drive". A folder inside the chosen drive actually points to another drive. Remove the shortcut or alias.

**Schedule and config messages**
Studio refuses to save when there is an error, and the Wii shows these same problems in the schedule dialog. Warnings never block saving.
- `parse`: a file is missing, is not valid, or lacks a required value. If you edited a file by hand, undo the edit or fix the line it names.
- `version`: the file is from a different version of cable-twii. Use the same Studio version that made it.
- `bad_value`: a value is out of range or not allowed (for example a channel number above 999, a loop on a timed channel, a count of zero). Fix the value in the form.
- `bad_time`: a slot time is not a valid `HH:MM`. Use two digits each, 00:00 to 23:59.
- `dup_channel`: two channels have the same number. Give one a different number.
- `dup_slot`: two slots in a channel start at the same time. Rename or delete one.
- `unknown_show`: a block uses a show that is not in the library (it may have been renamed or removed). Delete the block or add the show again.
- `missing_episode`: a block names a season and episode the show does not have. Pick another episode.
- `unknown_ad`: a block uses an ad that is not in the library. Delete it or add the ad again.
- `fill_in_loop`: a Fill slot block is in a looping channel. Remove it or change the channel to timed.
- `no_slots`: a timed channel has no slots. Add a slot.
- `empty_pool`: a random or Fill slot block names a show or season with no episodes. Choose another season or show.
- `missing_file`: a file listed in the library is not on the drive. Re-add the show or click **Re-read library** on the Drive tab.

**Warnings (shown but not blocking)**
- `cut`: an episode or ad is cut off when the next slot starts (the red overlap).
- `partial`: a random block placed fewer episodes than asked for, or a fixed block could not start because its slot was already full, or the day reached the 20000 airings limit.
- `loop_empty`: a looping channel played nothing, for example it has only ads and ads are off. The day shows as "Up next".

**Other**
- "Still converting. Try again when it's done.": the episode or ad is waiting to convert or converting. Wait until it is done on the Episodes page.
- "This change would break a schedule.": the listed channels would have errors afterwards, for example a rule with nothing left to pick. Change those rules first, or pick fewer episodes.
- "S01E05 already exists": another episode of the show has that number. Pick another number or renumber the other episode first.
- "Can't play this file here": the browser couldn't play the file. The Wii may still play it; check on the TV.
- "config.toml changed on disk — Reload": the file changed outside the page. Click **Reload**, then redo your edits.
- "Unsaved changes": you have not clicked **Save** on the Schedule tab. Studio asks before you leave it.
- "Skipped ...: already on the drive": the file was imported earlier. Nothing to do.
- "Could not read the library": click **Re-read library** on the Drive tab, or check the drive is connected.
- A video in Recent says Failed: click **Retry failed**. If it keeps failing, check the source video plays on the Mac (use **▶** on the Add page) and that the drive has free space.
- "Done" will not finish: videos are still converting. Wait until **In progress** on the Episodes page is empty.

## 15. Play from a server

Instead of a drive, the Wii can play from a home server running Studio. Set the server up first with `docs/SERVER.md`. The drive stays the default: nothing changes unless you choose the server.

### Choose the server

1. Start cable-twii. Press **HOME** and choose **Settings**. (If the Wii shows the error or the empty screen, press its **Settings** pill instead.)
2. Pick **Server**.
3. Press **Edit** and type the server's address with the on-screen keyboard. Use the pointer and **A**, or the D-pad and **A**. **B** closes the keyboard without changes. The `http://` key shows only while the box is empty.
4. Press **Test connection**. It tells you one of four things:
   - `Connected to "<name>".` Good.
   - `No answer from <address>. Check that the server is running, the address is right and the PC's firewall allows it.`
   - `<address> answered, but it isn't a cable-twii server.`
   - `The server's channels have a problem. Open cable-twii Studio on the server to fix it.`
5. Press **Save**. The Wii restarts the way it does at start-up when the source or the address changed, or when it isn't playing from the saved source (for example after `Can't reach the server`, or while it plays from the drive after the server didn't answer). Otherwise Save just closes Settings. **Cancel** (or **B**) leaves without saving.

Save refuses if Server is picked and the address is empty (it opens the keyboard) or can't be read (it shows the "No answer" line).

These forms of the address work:
- `192.168.1.20`
- `192.168.1.20:4747`
- a computer name such as `PC-NAME`
- `http://192.168.1.20:4747/`

The port is 4747 when you leave it out. `https://`, bad ports and `.local` names do not work. Use the number address from `docs/SERVER.md`.

### Where the choice is saved

The choice is saved in `settings.toml` in `apps/cable-twii/` on the SD card (format in `docs/formats.md`). In server mode the last channel and volume are saved beside it in `state.toml`.

The app saves there only when no partition on the SD card has a `wbfs` or `games` folder. If there is no SD card, or it holds games, nothing is saved: the settings last until cable-twii closes and the Settings screen says `No SD card. These settings last until cable-twii closes.` If writing the file fails (for example the card is full), Save shows the same line and stays open; press **Save** again to use the settings until cable-twii closes.

### Start-up and the fallback

The logo plays and holds with `Connecting to <address>…`, then the guide opens. Getting online tries up to three times, 10 seconds each.

If the server does not answer, or its files are bad, the app plays from the drive and says `Server not reachable, playing from USB` or `Server not reachable, playing from the SD card`. With no drive it shows `Can't reach the server`, or `Can't reach the network. Check the Wii's internet settings, or start cable-twii again.` Both screens have a **Settings** pill, so you can fix the address from there. To try again once the server is running, press **Settings** and then **Save**, even without changing anything. While the app plays from the drive after a fallback, **HOME** → **Settings** → **Save** tries the server again the same way.

### Reload channels

After you change channels or shows in Studio, press **HOME** and choose **Reload channels** (it is there in server mode only). It shows `Connecting to <address>…` and then `Channels updated`. If the server can't be reached, your current channels stay and it shows `Can't reach the server`. If the server answers but its channels have a problem, they stay too and it shows `The server's channels have a problem. Open cable-twii Studio on the server to fix it.`

The app also checks the server when the guide opens and at midnight. If the library changed, it reloads quietly and shows `Channels updated`. If that check fails, nothing changes.

### Weak signal

The video streams with about 10 seconds read ahead. If data stops for more than a second, `Weak signal` shows over the last frame, or over black while a channel is still tuning in. When data comes back far behind the schedule, the channel rejoins at the live point. If the stream is lost (an error, or 10 seconds without data), the frame holds with `Weak signal`, the app checks the server every 3 seconds and tunes back to live once it answers.

If it happens a lot, move the Wii closer to the router or use a Wii LAN adapter.
