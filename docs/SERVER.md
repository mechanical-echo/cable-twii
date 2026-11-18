# cable-twii server guide

The cable-twii server is Studio running on a computer that stays on at home. The shows, ads and schedules live in a folder on that computer, laid out exactly like a drive. People at home open Studio in a browser, sign in with one shared password, add videos and edit schedules. The Wii reads the library from the server over the home network.

The USB drive and the SD card work exactly as before. Nothing in this guide changes them.

Read this first:

- **Home network only.** The server speaks plain HTTP, with no HTTPS. Do not forward its port on the router to the internet.
- **The password protects editing.** Everyone who has it can edit everything. It is stored as plain text in the settings file, so keep that file private.
- **The Wii's part has no password.** It is read-only, but anyone on your network can read the library files through it. Nobody can change anything without the password.
- **Not everything here has been tried on the real machines yet.** The server was checked on Linux in a container, but the Windows steps have not been tried on the real PC and the Raspberry Pi steps have not been tried on a Pi. After you try them, the guide should be corrected where it was wrong.

## 1. What you need

- A computer that stays on whenever the Wii is used: the Windows 11 PC (section 3), a Raspberry Pi with an SSD (section 4), or a Mac for testing (section 5).
- Node 22, Git and ffmpeg (which includes `ffprobe`) on that computer.
- A C compiler and `make`. Studio builds its schedule tool, `cable-twii-sched`, from the engine source. On Linux and Mac the first start does it by itself. On Windows you build it once by hand.
- Room on the disk: converted video takes about 0.5 GB per hour. The server converts videos smaller than a drive does, because the Wii's Wi-Fi is too slow for the drive size. A video that is uploaded from a browser also needs room until it is converted.
- The Wii joined to the same home network, with its own Internet settings.

## 2. The settings file

The server reads one small TOML file when it starts. By default it is `.cable-twii-server.toml` in your home folder. To keep it somewhere else, set the `CTW_SERVER_SETTINGS` environment variable to its path.

| Key | Needed | Default | Meaning |
|---|---|---|---|
| `library` | yes | none | The folder that holds the library. It must already exist. |
| `password` | yes | none | The shared password, at least 8 characters. |
| `imports` | no | `<library>/imports` | The folder for big batches of videos (see section 6). It must not be inside `cable-twii/` or `.uploads/`. |
| `port` | no | `4747` | The port to listen on. |
| `name` | no | the computer's name | Shown on the sign-in page and sent to the Wii. |

Example for Linux and Mac:

```toml
library  = "/srv/cable-twii-library"
password = "choose a long password"
name     = "Home server"
```

Example for Windows:

```toml
library  = 'D:\cable-twii-library'
password = "choose a long password"
name     = "Home PC"
```

On Windows put paths in single quotes, as above, or write them with forward slashes (`"D:/cable-twii-library"`). Inside double quotes a backslash is an escape and the file will not load.

The first time it starts, the server creates what it needs inside the library folder: `cable-twii/` (the library itself), `imports/` and `.uploads/`. You only create the library folder.

If the file is missing or wrong, the server stops at once and says what to fix.

## 3. Windows 11 PC

Do the steps in order. Windows commands go in **Terminal** (PowerShell) unless a step says otherwise. Steps marked **administrator** need Terminal opened as administrator: right-click the Start button and choose **Terminal (Admin)**.

### 3.1 Install the programs

1. Node 22: download the Windows installer for version 22 from https://nodejs.org/en/download and run it. Studio needs Node 22 or newer and is tested on 22.
2. Git, ffmpeg and MSYS2 (this gives you `gcc` and `make` to build the schedule tool):

   ```powershell
   winget install Git.Git
   winget install Gyan.FFmpeg
   winget install MSYS2.MSYS2
   ```

3. Close Terminal and open a new one, so the new programs are found. Check them:

   ```powershell
   node --version
   git --version
   ffmpeg -version
   ffprobe -version
   ```

   `node --version` should start with `v22`.
4. Let PowerShell run `npm`. PowerShell does not run script files by default, and `npm` is one (`npm.ps1`), so without this step the first `npm` command stops with "running scripts is disabled on this system". This allows scripts that are on your PC, for your user only. Answer `Y` if it asks:

   ```powershell
   Set-ExecutionPolicy -Scope CurrentUser RemoteSigned
   npm --version
   ```

5. Open **MSYS2 UCRT64** from the Start menu and update MSYS2 first. Answer `Y` when it asks. If it says it must close the window, open **MSYS2 UCRT64** again and run the same command once more:

   ```bash
   pacman -Syu
   ```

   Then install the compiler and `make`. Answer `Y` when it asks:

   ```bash
   pacman -S --needed make mingw-w64-ucrt-x86_64-gcc
   ```

### 3.2 Get the project

In a normal Terminal:

```powershell
git clone https://github.com/mechanical-echo/cable-twii.git C:\cable-twii
cd C:\cable-twii\studio
npm ci
```

Git asks you to sign in to GitHub if the project is private.

If you put the project in another folder, use that folder instead of `C:\cable-twii` in every command below.

### 3.3 Build the schedule tool

In the **MSYS2 UCRT64** window:

```bash
make -C /c/cable-twii/engine cli CC=gcc
```

This makes `C:\cable-twii\engine\build\cable-twii-sched.exe`. Studio finds it by itself. You do this once, and again only after an update that changes the engine.

### 3.4 Write the settings file

Make the library folder on a disk with plenty of room (change `D:` to a drive that exists on your PC), then open the settings file in Notepad:

```powershell
mkdir D:\cable-twii-library
notepad "$HOME\.cable-twii-server.toml"
```

Notepad asks whether to create the file. Choose **Yes**, type the Windows example from section 2 (with your own folder and password) and save.

### 3.5 Let the Wii through the firewall (administrator)

Windows Firewall blocks other devices from reaching the server unless you allow it. In **Terminal (administrator)**:

```powershell
New-NetFirewallRule -DisplayName "cable-twii server" -Direction Inbound -Protocol TCP -LocalPort 4747 -Profile Private -Action Allow
```

If you changed `port` in the settings file, use that number instead of `4747`.

The rule only applies to **Private** networks, so the home network must be set to Private. Check it:

```powershell
Get-NetConnectionProfile
```

`NetworkCategory` must say `Private`. If it says `Public`, open **Settings, Network & internet**, choose Wi-Fi or Ethernet, open your network and set **Network profile type** to **Private network**. Or, in the administrator Terminal, use the `InterfaceAlias` from the list above:

```powershell
Set-NetConnectionProfile -InterfaceAlias "Wi-Fi" -NetworkCategory Private
```

### 3.6 Try it by hand

```powershell
cd C:\cable-twii\studio
npm run serve
```

Every start first builds Studio's web page, which takes a few seconds. The server then prints log lines in JSON. Look for a line like this one, which says it is ready:

```
cable-twii server "Home PC" on port 4747, library D:\cable-twii-library
```

If Windows still asks whether Node.js can use the network, tick **Private networks** only and click **Allow access**. Do not click Cancel: Windows may then block Node.js, and a block overrides the rule from section 3.5. On the PC, open http://127.0.0.1:4747/ in a browser and sign in with your password. Then press Ctrl+C in Terminal to stop the server.

### 3.7 Start the server when you sign in (administrator)

In the administrator Terminal, switch to the old Command Prompt for one command. This command needs its quotes exactly as written:

```powershell
cmd
```

```bat
schtasks /Create /TN "cable-twii server" /SC ONLOGON /TR "\"C:\cable-twii\studio\scripts\serve-windows.cmd\""
exit
```

Windows stops a task that has run for 3 days. A server must not stop, so turn that limit off. Back in PowerShell (administrator):

```powershell
$settings = New-ScheduledTaskSettingsSet -ExecutionTimeLimit ([TimeSpan]::Zero) -AllowStartIfOnBatteries -DontStopIfGoingOnBatteries
Set-ScheduledTask -TaskName "cable-twii server" -Settings $settings
```

Start it now without signing out:

```powershell
schtasks /Run /TN "cable-twii server"
```

A black window opens and stays open while the server runs. Minimise it. Closing it stops the server.

Turn off QuickEdit for that window, or a click inside it can pause the server: right-click the window's title bar, choose **Properties**, untick **QuickEdit Mode** and click **OK**. If the server ever seems stuck, click its window and press Esc.

The server starts when the Windows account that created the task signs in. After the PC restarts, for example after Windows updates, sign in to that account once so the server starts.

To remove the task later: `schtasks /Delete /TN "cable-twii server" /F`.

### 3.8 Never sleep while plugged in

A PC that sleeps cannot serve the Wii. Open **Settings, System, Power & battery, Screen, sleep & hibernate timeouts** and set **When plugged in, put my device to sleep after** to **Never**. Or, in Terminal (administrator):

```powershell
powercfg /change standby-timeout-ac 0
powercfg /change hibernate-timeout-ac 0
```

### 3.9 Give the PC a fixed address

The Wii needs the PC's address, so the address must not change. Find it:

```powershell
ipconfig
```

Look for **IPv4 Address** under the Wi-Fi or Ethernet adapter that is in use, for example `192.168.1.20`.

Then open your router's settings page in a browser, find the list of connected devices (it may be called DHCP clients), pick the PC and reserve its current address for it. Routers name this a **DHCP reservation**, **static lease** or **address reservation**. After that the router always gives the PC the same address.

### 3.10 Check it from another device

From another computer on the network (use the PC's address from section 3.9):

```bash
curl http://192.168.1.20:4747/wii/v1/info
```

On a Windows computer type `curl.exe` instead of `curl`. The answer looks like this:

```
version = 1
name = "Home PC"
revision = "0-0"
```

Then take a phone on the same Wi-Fi, open `http://192.168.1.20:4747/` in its browser and sign in with the password.

If the check on the PC itself works but the check from another device does not, it is nearly always the firewall rule or the network type (section 3.5).

## 4. Raspberry Pi

Use the 64-bit Raspberry Pi OS and run these steps as your normal user, not as root.

### 4.1 Install the programs

The `nodejs` package in Raspberry Pi OS is usually older than 22, so the first command adds the NodeSource package source for Node 22:

```bash
curl -fsSL https://deb.nodesource.com/setup_22.x | sudo -E bash -
sudo apt install -y nodejs ffmpeg build-essential git
node --version
```

`node --version` should start with `v22`.

### 4.2 Get the project

```bash
git clone https://github.com/mechanical-echo/cable-twii.git ~/cable-twii
cd ~/cable-twii/studio
npm ci
```

### 4.3 Write the settings file

Mount the SSD and make the library folder on it. The example uses `/mnt/ssd`. Use your own mount point.

```bash
mkdir -p /mnt/ssd/cable-twii-library
nano ~/.cable-twii-server.toml
chmod 600 ~/.cable-twii-server.toml
```

In the file:

```toml
library  = "/mnt/ssd/cable-twii-library"
password = "choose a long password"
name     = "Pi server"
```

### 4.4 Try it by hand

```bash
npm run serve
```

The first start builds the schedule tool, which takes a few seconds. When it prints `cable-twii server "Pi server" on port 4747, ...`, press Ctrl+C.

### 4.5 Start it at boot

This writes a systemd service that runs as your user, from the project folder you just made:

```bash
sudo tee /etc/systemd/system/cable-twii.service >/dev/null <<EOF
[Unit]
Description=cable-twii server
After=network-online.target
Wants=network-online.target

[Service]
User=$USER
WorkingDirectory=$HOME/cable-twii/studio
ExecStart=/usr/bin/npm run serve
Restart=on-failure
RestartSec=5

[Install]
WantedBy=multi-user.target
EOF
sudo systemctl enable --now cable-twii
```

If `command -v npm` prints a path other than `/usr/bin/npm`, put that path in `ExecStart`.

Check that it runs, and read its log:

```bash
systemctl status cable-twii
journalctl -u cable-twii -f
```

Converting video on a Pi is slower than on a PC. It runs in the background queue, so the web page stays usable.

### 4.6 Fixed address and check

Find the Pi's address with `hostname -I`. Reserve it in the router as in section 3.9. Then check from another device:

```bash
curl http://<pi-ip>:4747/wii/v1/info
```

The Wii cannot use names such as `raspberrypi.local`. Always give it the number address.

## 5. Mac, for testing

On the Mac you start the server by hand when you want it. Put the settings in any file and point `CTW_SERVER_SETTINGS` at it:

```bash
cd studio
CTW_SERVER_SETTINGS=/path/to/server-settings.toml npm run serve
```

If the shell says Studio needs Node 22, run `nvm use` in the `studio` folder first.

Use a `library` folder for tests, not a real drive. The Mac asks whether Node may accept incoming connections. Choose **Allow** if other devices need to reach it.

Studio's normal drive mode (`npm start`) uses port 4747 as well. To run both at once, give the server another `port` in its settings file.

## 6. Using the server

Open `http://<server-address>:4747/` in a browser on any computer or phone at home and sign in.

- You stay signed in for 30 days. You sign in again after the server restarts. If a page is open then, the sign-in box opens over it and unsaved changes stay, so you can sign in and save again.
- Five wrong passwords within a minute stop sign-in for up to a minute. The page says "Too many tries. Wait a minute and try again."
- The first tab is called **Server**. It shows the server's name, the library folder and the free space, and has **Sign out**.
- Library, Channels, Schedule and Preview work as in the owner's guide (`docs/USER-GUIDE.md`, sections 5 to 8). Everyone sees the same library, and the conversion queue is shared.

**Adding videos.** On the Library page, click **Add show…** or **Add ads…**. The **Add videos** window has two tabs:

- **Upload.** Pick one or more videos on the computer you are using and click **Upload**. When they have arrived, the usual steps follow: names, seasons and episodes, then conversion on the server. A video is kept on the server only until it is converted, and then deleted. You still have the original. Uploading the same file again is skipped with the note "already on the server".
- **Imports folder.** For big batches, copy the videos into the imports folder on the server (for example with `scp`, over a network share you have set up, or by hand), then pick them here. The folder is `imports` inside the library folder unless you set `imports` in the settings file. Files there are never changed or deleted, so you can convert them again later.

Uploads can be up to 20 GB per file.

**Making older videos smaller.** Videos converted before the server used its own, lighter video settings can be too big for the Wii's Wi-Fi to stream. When there are any, the **Server** tab shows a card "Make older videos smaller for Wi-Fi" with how many there are. Click **Make smaller** and the server converts each one again, in place, through the normal conversion queue. The Library page's Conversions list shows the progress, with "Smaller:" before each name. The originals you uploaded are gone, so this starts from the already converted file, which is fine at these sizes. If a conversion fails the old file stays as it was. A video playing on the Wii at the moment it is replaced may stop or skip, so it is best to shrink when nobody is watching.

An uploaded video that is not waiting to be converted, for example because the import window was closed or its conversion failed, is deleted when the server starts once it is more than a day old. To free the space sooner, stop the server and delete what is inside the `.uploads` folder in the library folder by hand.

**Two people editing.** If someone saves a show or the schedule while you are editing it, your save is refused and the page asks you to reload. For a show it says "show.toml was changed somewhere else. Reload it and try again." The first person's changes are kept.

## 7. Telling the Wii where the server is

The Wii app gets a Settings screen for this in a later step of the project. The address it will need is:

```
<server-address>:4747
```

For example `192.168.1.20:4747`. Use the number address from the fixed-address step, not a computer name.

## 8. If something goes wrong

| What you see | What to do |
|---|---|
| `was not found. Create it with a library and a password` | The settings file is not where the server looks. Check the file name and folder, or set `CTW_SERVER_SETTINGS`. |
| `library ... must be an existing folder` | Create the library folder first. |
| `password is required and must be at least 8 characters` | Add or lengthen `password`. |
| `must be an absolute path` or `is not valid TOML` | On Windows, put paths in single quotes (section 2). |
| `could not build ...cable-twii-sched` | The compiler or `make` is missing. On Windows, build the tool as in section 3.3. On Linux, install `build-essential`. |
| `EADDRINUSE` | Another program uses the port. Studio's drive mode uses 4747 too. Stop it, or change `port`. |
| `curl` works on the server but not from another device | On Windows, check the firewall rule and that the network is **Private** (section 3.5). Check that the server's address is the one you used. |
| The Wii cannot connect but a phone can | Use the number address, not a name. Check that the Wii is on the same network. |
| The server stops after a few days (Windows) | The 3-day limit is still on. Do the step in section 3.7 that turns it off. |

## 9. Updating

Get the new version, install its packages and restart the server.

On Windows, in Terminal (stop the server first by closing its black window):

```powershell
cd C:\cable-twii
git pull
cd studio
npm ci
schtasks /Run /TN "cable-twii server"
```

On the Pi:

```bash
cd ~/cable-twii
git pull
cd studio
npm ci
sudo systemctl restart cable-twii
```

If the update changed the engine, build the schedule tool again: `make -C engine cli` on Linux and Mac, or section 3.3 on Windows.
