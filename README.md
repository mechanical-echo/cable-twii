# cable-twii

cable-twii is a Homebrew Channel app that turns a modded Wii into your own cable TV. It plays custom channels, schedules and ads from a USB drive. The Mac tool, cable-twii Studio, converts videos and builds the schedules.

- [Design spec](docs/superpowers/specs/2026-10-20-cable-twii-design.md)
- Plans:
  - [00 Roadmap](docs/superpowers/plans/2026-10-20-cable-twii-00-roadmap.md)
  - [01 Engine](docs/superpowers/plans/2026-10-20-cable-twii-01-engine.md)
  - [02 Studio](docs/superpowers/plans/2026-10-20-cable-twii-02-studio.md)
  - [03 Wii app](docs/superpowers/plans/2026-10-20-cable-twii-03-wii-app.md)
  - [04 Release](docs/superpowers/plans/2026-10-20-cable-twii-04-release.md)
  - [05 Wii redesign](docs/superpowers/plans/2026-10-22-cable-twii-05-wii-redesign.md) (spec: [Wii redesign](docs/superpowers/specs/2026-10-22-wii-redesign-design.md))
- [File formats](docs/formats.md)
- [Engine C API](docs/engine-api.md)
- [Mac setup](docs/setup.md)

## Try it in Dolphin (Mac)

Run the app against the sample library in the Dolphin emulator, no Wii needed.

1. Build the app and the test media (once):

   ```bash
   make -C wii DEVKITPRO=/opt/devkitpro DEVKITPPC=/opt/devkitpro/devkitPPC   # -> wii/cable-twii.dol
   tools/make-samples.sh samples/out --short                                 # -> samples/out/cable-twii/
   ```

   `samples/out/cable-twii/` already has a `config.toml` and `library.toml` (two channels) matching the `--short` media.

2. Point Dolphin's SD card at the sample folder (once). In `~/Library/Application Support/Dolphin/Config/Dolphin.ini`:

   ```ini
   [Core]
   WiiSDCard = True
   WiiSDCardEnableFolderSync = True
   [General]
   WiiSDCardSyncFolder = /ABSOLUTE/PATH/TO/cable-twii/samples/out
   ```

   The sync folder is the one that *contains* `cable-twii/`, and the path must be absolute. Dolphin may ask for access to the folder the first time — allow it.

3. Launch (boots straight into the app):

   ```bash
   open -a /Applications/Dolphin.app --args -e wii/cable-twii.dol
   ```

Controls (emulated Wii Remote = mouse + keyboard): move the **mouse** to point, **left/right click** = A/B, **arrow keys** navigate, **Enter** = HOME, **E / Q** = volume. If nothing responds, set *Controllers → Wii Remote 1 → Emulated Wii Remote*.
