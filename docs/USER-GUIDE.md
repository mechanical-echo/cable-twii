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

Studio has five pages: Drive, Library, Channels, Schedule and Preview.

## 4. Choose and set up the drive

On the **Drive** page:

1. Click **Choose drive…** and pick the flash drive or SD card, or type its path (for example `/Volumes/CABLETV`) and click **Use this folder**.
2. Studio shows the drive type, free space and whether it is set up. If it says the drive cannot be used, read the reason. For example, a drive with a `wbfs` folder is refused because it looks like your games drive, and a drive that is not FAT32 is refused.
3. Click **Set up cable-twii on this drive**. This creates a `cable-twii` folder. Nothing else on the drive is changed.

You can also pick a plain folder on the Mac to try things out. Studio treats it as a test area and there is nothing to eject.

## 5. Add shows and ads (Library page)

- **Add show…** picks a folder of video files. Studio looks at the file names to guess season and episode numbers, lets you fix them and choose the channel, then converts the videos into the format the Wii can play and copies them onto the drive. Conversion takes a while. The **jobs panel** at the top shows progress. If a job fails, use **Retry failed**.
- **Add ads…** does the same for ad clips. Ads belong to a channel.
- **Edit info** on a show lets you change its name, description and icon.
- **Rebuild** re-reads the drive and rebuilds the library. Studio lists anything it skipped and why. Use it if you changed files by hand.
- Adding the same files again is safe. Files that are already on the drive are skipped.

## 6. Set up channels (Channels page)

Click **Add channel**, fill in the form and **Save**:

- **Number**: 1 to 999. Two channels cannot share a number.
- **Name** and **Category**: shown in the Wii guide.
- **Folder**: the channel's folder on the drive. If you leave it empty, Studio uses the name. Pick an existing folder to attach a channel to shows you already imported.
- **Icon**: choose a picture. It is saved right away.
- **Ads**: on or off. When off, random ads are skipped on this channel.
- **Mode**: **timed** (a schedule with start times, like a TV station) or **looping** (a list that plays in order and repeats all day).

A new channel gets a placeholder schedule so it is valid straight away. Build the real schedule on the Schedule page. Edit or Delete a channel from the table. Deleting a channel keeps its files on the drive.

## 7. Build schedules (Schedule page)

Pick the channel with the buttons at the top. Studio shows whether it is Timed or Looping (change that on the Channels page).

- The **timeline** shows what airs and when. **Zoom** buttons change how much of the day you see.
- The **library panel** on the left lists episodes and rules for this channel. **Drag** an item onto the timeline, into a slot (timed) or onto the loop (looping). Drag blocks to reorder them. Click a block to edit it on the right. **Delete** removes the selected block.
- The ready-made rules are:
  - **Random episode**: picks random episodes (set how many with Count), without repeats inside the block.
  - **Random sequential run**: picks a random starting episode, then plays the following episodes in order.
  - **Fill slot** (timed channels only): fills the rest of the slot, with an optional number of ads between episodes.
  - **Random ad** and specific ads: the ads for this channel.
- On a **timed** channel, a **slot** is a start time. A slot runs until the next slot starts. **Add slot** asks for a time as `HH:MM` (24-hour, for example `07:30` or `19:00`). Click a slot's time to rename it. Two slots cannot start at the same time.
- A **looping** channel has no times. It plays its list in order and repeats it all day.
- **Undo** and **Redo** step through your changes.
- **Save** writes your changes to the drive. Studio refuses to save a schedule with errors and tells you which block to fix.
- **Reshuffle** picks a new random seed so every random pick changes. Save first, because Reshuffle is disabled while there are unsaved changes.
- If you edit the schedule in a second place (for example by hand), Studio may say "config.toml changed on disk — Reload". Click **Reload** to load the newer version. Your unsaved changes in the page are lost, so redo them afterwards.

### What the red overlap means

A block marked with a warning and "min overlap" runs past the start of the next slot. The part that does not fit is cut off when the next slot starts. This is a warning, not an error. You can leave it, make the block shorter (for example fewer episodes) or move the next slot later. A block outlined as an error is a real problem and must be fixed before Studio will save. The message names the channel, slot and item.

## 8. Preview

Open the **Preview** page, pick a channel and a date. You get a TV guide for that day: start and end times, show, season and episode, and ad breaks. The **Now** marker shows the current time when you look at today. **Refresh** reloads the guide after you save changes.

Preview is computed with the same engine the Wii uses, so what you see here is what airs on the Wii for the same date and time. The Wii app was only checked against Dolphin so far, not yet on a real Wii.

## 9. Done, then eject

When you finish, go to the **Drive** page and click **Done — safe to eject**.

- This does not eject the drive. It finishes pending work and cleans up files the Mac leaves on FAT drives (the `._` files).
- It refuses while videos are still converting. Wait for the jobs panel to finish and try again.
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

**Fullscreen**
- **D-pad up or right**: next channel. **D-pad down or left**: previous channel.
- **+** and **−**: volume (steps of 10, from 0 to 100).
- **B**: back to the guide.
- Point the remote at the screen to show the **info panel**. It has buttons for the previous channel, the next channel, back to the guide, and volume down and up. Click them with **A**. The panel hides itself after a few seconds without pointer movement.

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
- Fix: plug the drive into the Mac, open Studio, go to the Schedule page and fix the blocks it points to (see "In Studio" below). Save, click **Done — safe to eject**, eject and try again. If it still fails, click **Rebuild** on the Library page first.

**The drive was disconnected**
Plug it back in and start cable-twii again.
- Cause: the drive was unplugged or lost power while a show was playing.
- Fix: plug it back in, press the button to go back to the Homebrew Channel, and start cable-twii again. If it happens without anyone touching the drive, try another USB port, a powered USB hub, or the SD card.

On all three screens press **Back to Homebrew Channel** with the pointer and **A**.

**Black screen on a channel**
If a video file is shorter than the library says, or a file is missing, the picture stays black until the next show starts. Run **Rebuild** in Studio to check the files.

**The Wii shows the wrong show or the wrong time**
Check the Wii clock (section 12).

**The cursor does not appear**
Point the remote at the screen and move it. Check the sensor bar is on.

### In Studio

**Drive messages (Drive page)**
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
- `missing_file`: a file listed in the library is not on the drive. Re-add the show or click **Rebuild**.

**Warnings (shown but not blocking)**
- `cut`: an episode or ad is cut off when the next slot starts (the red overlap).
- `partial`: a random block placed fewer episodes than asked for, or a fixed block could not start because its slot was already full, or the day reached the 20000 airings limit.
- `loop_empty`: a looping channel played nothing, for example it has only ads and ads are off. The day shows as "Up next".

**Other**
- "config.toml changed on disk — Reload": the file changed outside the page. Click **Reload**, then redo your edits.
- "Unsaved changes": you have not clicked **Save** on the Schedule page. Studio asks before you leave the channel.
- "Skipped ...: already on the drive": the file was imported earlier. Nothing to do.
- "Could not read the library": click **Rebuild** on the Library page, or check the drive is connected.
- A job in the jobs panel fails: click **Retry failed**. If it keeps failing, check the source video plays on the Mac and that the drive has free space.
- "Done" will not finish: videos are still converting. Wait for the jobs panel to finish.

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
