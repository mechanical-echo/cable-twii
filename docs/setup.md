# Setup

How to rebuild the Mac toolchain for cable-twii from scratch.

## Homebrew packages

```bash
brew install ffmpeg node@22
```

```bash
brew install --cask dolphin
```

The default ffmpeg build has no `drawtext` filter, and the project does not need it.

`node@22` is keg-only, so it is not on your PATH until you add it (see Environment).

## devkitPro

Download the latest `devkitpro-pacman-installer.pkg` from https://github.com/devkitPro/pacman/releases and open it.

The package is not notarized, so macOS blocks it. Open System Settings, go to Privacy & Security, scroll down and click Open Anyway, then run the installer again.

Check the package names:

```bash
dkp-pacman -Sl | grep -E "wii-dev|ppc-"
```

Install the Wii toolchain and libraries:

```bash
sudo dkp-pacman -S wii-dev ppc-libpng ppc-freetype ppc-libjpeg-turbo ppc-zlib
```

## GRRLIB

GRRLIB is built from source.

```bash
git clone https://github.com/GRRLIB/GRRLIB.git ~/src/GRRLIB
```

Build as your normal user:

```bash
make -C ~/src/GRRLIB/GRRLIB clean all
```

The install writes into root-owned `/opt/devkitpro/portlibs`, so run only the install under sudo:

```bash
sudo -E make -C ~/src/GRRLIB/GRRLIB install
```

`sudo -E` keeps `DEVKITPRO` and `DEVKITPPC` from your shell. `libgrrlib.a` and the GRRLIB headers go to `/opt/devkitpro/portlibs/wii/{lib,include}`. `libpngu.a` and `pngu.h` go to `/opt/devkitpro/portlibs/ppc/{lib,include}`.

## Environment

Add these lines to `~/.zprofile`, then open a new terminal.

```bash
export DEVKITPRO=/opt/devkitpro
```

```bash
export DEVKITPPC=$DEVKITPRO/devkitPPC
```

```bash
export PATH="$DEVKITPRO/tools/bin:$DEVKITPPC/bin:/opt/homebrew/opt/node@22/bin:$PATH"
```

## Dolphin

Dolphin is installed by the cask above. Use it to run `.dol` files on the Mac without a real Wii.

## Wii

You need a Wii that has:

- Homebrew Channel installed
- Wi-Fi set up and connected

Find the Wii's IP address in Wii Settings → Internet → Console Information, or in the Homebrew Channel's network status. Then set it in your shell, and add it to `~/.zprofile` if the IP is fixed:

```bash
export WIILOAD=tcp:<wii-ip>
```

Open the Homebrew Channel on the Wii before sending a build. `wiiload app.dol` then sends it over the network.

## Verify

```bash
powerpc-eabi-gcc --version | head -1
```

```bash
wiiload 2>&1 | head -1
```

```bash
ffmpeg -version | head -1
```

```bash
node --version
```

Expected: four lines, with node showing `v22.x`. With `WIILOAD` unset, `wiiload` prints a hint to set it, which is fine.
