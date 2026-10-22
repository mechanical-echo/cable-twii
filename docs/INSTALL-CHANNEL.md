# Installing the cable-twii channel

The cable-twii channel is a small Wii Menu channel that starts the cable-twii app for you, so you do not have to go through the Homebrew Channel. It looks for `apps/cable-twii/boot.dol` on the SD card, then on a USB drive, and starts it. The app itself stays on the SD card: updating cable-twii later means copying a new `boot.dol`, and the channel never changes.

Installing a channel writes to the Wii's internal memory, which is the one step in this project that can leave a Wii unable to start its menu if something is wrong. That is why the four checks in section 1 come first and why nothing here is optional. Priiloader is the way back if the menu does not appear, and the NAND backup is the way back if everything else fails.

**Nothing in this guide touches the games drive, your IOS files, the System Menu or any other channel. The WAD you install contains only the cable-twii channel, and uninstalling it removes only that.**

## 1. Before you install: four checks

Do all four and confirm each one. If any check fails, stop and sort it out first.

**Check 1: a BootMii NAND backup exists and is on the Mac.**
- How to confirm: open the SD card on the Mac and find `nand.bin` and `keys.bin` in its top folder. `nand.bin` is about 528 MB and `keys.bin` is about 1 KB. Copy both into a folder on the Mac that is backed up, and check the copy's size matches.
- If you do not have them: start BootMii (from the Homebrew Channel menu, or at power-on if BootMii is installed as boot2), choose **Backup NAND** and wait until it finishes without errors. Then copy the two files as above.

**Check 2: Priiloader is installed and opens on boot.**
- How to confirm: turn the Wii off, hold **Reset** on the console and turn it on while still holding Reset. Priiloader's black menu appears instead of the Wii Menu, with **System Menu** and **Homebrew Channel** among its entries. Choose **System Menu** to continue normally.
- If it does not appear: install Priiloader with its installer from the Homebrew Channel before going on. Priiloader is what lets you reach the Homebrew Channel and uninstall the channel again if the Wii Menu stops coming up.

**Check 3: your WAD manager is ready on the SD card.**
- How to confirm: start the Homebrew Channel and see the WAD manager you have used before in the list. It needs a `wad` folder on the SD card (`sd:/wad/`); create it if it is missing.
- While you are in the Homebrew Channel, look at its bottom-right corner: it shows which IOS it runs on. It should read **IOS58**. The channel asks for IOS58 as well, so the app runs under the same IOS either way.

**Check 4: the WAD is the right one.**
- How to confirm: the WAD is built on the Mac by `tools/build-wad.sh` (section 2), and its last line reads `title 0001000143545741, IOS58, version 1`. `0001000143545741` is the ID the project checked against the public title lists so that it collides with no other channel (decision 0003); `43545741` spells `CTWA`. When you select the file in the WAD manager it shows the channel's name, cable-twii, and most managers show the ID too.
- Install only this one file. Do not install IOS files, System Menu files or anything else a guide on the internet suggests.

## 2. Build the WAD on the Mac

1. In the project folder run:

   ```
   tools/build-wad.sh
   ```

2. The first time, the script stops and prints a one-line command that fetches a small certificate file (2.5 KB) from Nintendo's update server into `.tooling/cert-chain.bin`. Run that command, then run `tools/build-wad.sh` again.
3. The result is `dist/cable-twii-forwarder.wad`, about 1.2 MB. Copy it into the `wad` folder on the SD card.

## 3. Put the app on the SD card

The channel starts `apps/cable-twii/boot.dol`, so the app has to be on the card: follow section 10 of the owner's guide (`docs/USER-GUIDE.md`) if it is not there yet. Your shows can be on the same SD card or on the flash drive, as before. Keep `boot.dol` itself on the SD card for now: the channel's USB search is simpler than the app's and may not see a flash drive while the games drive is also plugged in (decision 0003, "USB limit").

## 4. Install

1. Put the SD card in the Wii and start the **Homebrew Channel**.
2. Start your WAD manager. Choose the SD card as the source if it asks.
3. Select `cable-twii-forwarder.wad` and check the name it shows is cable-twii.
4. Choose **Install** and wait for it to report success. It takes a few seconds.
5. Leave the WAD manager and return to the Wii Menu.

The channel appears on the Wii Menu as **cable-twii**, with the striped banner and the logo. If a free slot is not on the first page, it is on a later one.

## 5. Check it works

Do these in order and note the result of each. They are the checks the project needs before the release is called done.

1. The channel appears on the Wii Menu with its banner, icon and sound. Point at it and press **A**, then **Start**.
2. With `boot.dol` on the SD card, cable-twii starts: a line or two of text, the cable-twii logo, then the guide within a few seconds.
3. Take the SD card out, put `apps/cable-twii/boot.dol` on the flash drive instead, plug only the flash drive in and start the channel. Note whether it starts. If you also plug the games drive in, note whether it still starts; this is the two-drive question from section 11 of the owner's guide. If it does not start with both drives, keep `boot.dol` on the SD card.
4. Start your USB loader and check the games drive still works as before.

If `boot.dol` is nowhere to be found, the channel shows a short message ("apps/cable-twii/boot.dol was not found on the SD card or a USB drive. Returning to the Wii Menu.") for five seconds and goes back to the Wii Menu by itself. It never stays on a black screen on purpose.

## 6. Test the uninstall once, then reinstall

Do this once, while everything is fresh, so you know the way back works.

1. Uninstall the channel either way:
   - In the WAD manager, select the same file and choose **Uninstall**; or
   - On the Wii Menu, open **Wii Options, Data Management, Channels, Wii**, select cable-twii and choose **Erase**.
2. Turn the Wii off and on. The Wii Menu should look exactly as it did before the channel, and the console should start normally.
3. Install the channel again as in section 4.

## 7. If the Wii Menu does not come back

If, after installing, the Wii shows a black screen or stops before the Wii Menu:

1. Turn the Wii off.
2. Hold **Reset** and turn it on. Priiloader appears.
3. Choose **Homebrew Channel**, start the WAD manager and **Uninstall** `cable-twii-forwarder.wad`.
4. Turn the Wii off and on again. The Wii Menu is back.

If Priiloader itself does not appear and BootMii is installed as boot2, BootMii can restore the NAND backup from `nand.bin` on the SD card. That is the last resort, which is why the backup from check 1 has to exist before anything is installed.

## 8. Uninstalling for good

Use either way from section 6. The app in `apps/cable-twii/` on the SD card and your shows are not touched; you can keep starting cable-twii from the Homebrew Channel.
