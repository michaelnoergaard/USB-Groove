# USB Groove

![GitHub Release](https://img.shields.io/github/v/release/michaelnoergaard/USB-Groove)
![License](https://img.shields.io/badge/license-MIT-green.svg)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-lightgrey.svg)

A lightweight cross-platform tray/menu bar application that automatically plays MP3 files from any inserted USB flash drive. No external media player required — uses built-in OS audio engines.

## Features

- **Auto-play** - Automatically detects USB drives and starts playing MP3 files
- **System tray / Menu bar** - Runs quietly in the background on all platforms
- **Shuffle mode** - Randomize playback order with one click
- **Repeat All** - Loop the entire playlist continuously
- **Full playback controls** - Play, pause, stop, next/previous track
- **Eject USB** - Safely eject the active USB drive from the tray menu
- **Playlist browser** - View and jump to any track via the playlist submenu
- **Desktop notifications** - Now-playing notifications on all platforms
- **Recursive scanning** - Finds MP3 files in all subdirectories
- **Autorun at login** - Optional start-at-login on all platforms
- **Single instance** - Only one copy runs at a time
- **No dependencies** - Uses only built-in OS libraries
- **Logging** - Debug log in `%TEMP%\USBAutoPlayer.log` (Windows), `~/Library/Logs/USBGroove.log` (macOS), or `/tmp/USBGroove.log` (Linux)

## Download

Download the latest release from [GitHub Releases](https://github.com/michaelnoergaard/USB-Groove/releases).

| Platform | File | How to run |
|----------|------|------------|
| **Windows** | `USBGroove.exe` | Portable — just download and run |
| **Windows** | `USBGroove_Setup_v*.exe` | Installer — Start Menu shortcut + Add/Remove Programs |
| **macOS** | `USBGroove-macOS.zip` | Unzip, move `USBGroove.app` to Applications, see note below |
| **Linux** | `USBGroove-Linux` | Download, `chmod +x`, and run |

> **macOS Gatekeeper note:** Since the app is not code-signed, macOS may show "app is damaged" when you first open it. Fix this by running:
> ```bash
> /usr/bin/xattr -cr /Applications/USBGroove.app
> ```
> This is not needed when installing via Homebrew.

## Install via package manager

**Windows (winget):**
```powershell
winget install michaelnoergaard.USBGroove
```

**macOS (Homebrew):**
```bash
brew tap michaelnoergaard/usb-groove
brew install usb-groove
```

## Usage

1. Run USB Groove
2. Insert a USB drive containing MP3 files
3. Playback starts automatically!

### Controls

**Windows:** Right-click the system tray icon. Double-click for track info.

**macOS:** Click the music note icon in the menu bar.

**Linux:** Click the system tray icon (requires libappindicator3).

| Control | Action |
|---------|--------|
| **Pause/Resume** | Toggle playback |
| **Previous track** | Go to the previous song |
| **Next track** | Skip to the next song |
| **Stop** | Stop playback and clear playlist |
| **Shuffle** | Enable/disable shuffle mode |
| **Repeat All** | Loop playlist when it reaches the end |
| **Eject USB** | Safely eject the active USB drive |
| **Playlist** | Browse and jump to any track |
| **Start at login** | Toggle autorun at system startup |
| **About** | View version information |

## Build from source

### Windows (MSVC)

```cmd
rc /fo USBAutoPlayer.res USBAutoPlayer.rc
cl USBAutoPlayer.cpp /O2 /W4 /EHsc /std:c++17 ^
    /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
    /link USBAutoPlayer.res ^
    winmm.lib shell32.lib user32.lib gdi32.lib kernel32.lib ^
    /SUBSYSTEM:WINDOWS /OUT:USBGroove.exe
```

### Windows (MinGW / MSYS2)

```bash
windres USBAutoPlayer.rc -O coff -o USBAutoPlayer.res
g++ -std=c++17 -O2 -Wall -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN \
    -o USBGroove.exe USBAutoPlayer.cpp USBAutoPlayer.res \
    -lwinmm -lshell32 -luser32 -lgdi32 -mwindows
```

### macOS

```bash
swiftc macos/USBGroove.swift -o USBGroove \
    -framework AVFoundation \
    -framework DiskArbitration \
    -framework AppKit -O
```

### Linux (Ubuntu/Debian)

```bash
sudo apt install libappindicator3-dev libnotify-dev libgstreamer1.0-dev libglib2.0-dev libgtk-3-dev

g++ -std=c++17 -O2 -Wall linux/USBGroove.cpp -o USBGroove-Linux \
    $(pkg-config --cflags --libs appindicator3-0.1 glib-2.0 gio-2.0 gstreamer-1.0 libnotify) \
    -lstdc++fs
```

## How It Works

### Windows
1. Registers for `WM_DEVICECHANGE` notifications to detect USB drives
2. Waits ~1.8s for filesystem to mount, then scans for MP3 files
3. Plays audio via Windows MCI (`winmm.dll`) — no external codecs needed
4. Auto-advances to next track when current track finishes

### macOS
1. Monitors volume mounts via `NSWorkspace` notifications
2. Identifies external/removable drives via volume resource keys
3. Plays audio via `AVFoundation` (`AVAudioPlayer`) — no external codecs needed
4. Auto-advances to next track when current track finishes

### Linux
1. Monitors mount events via GIO `GVolumeMonitor` signals
2. Filters removable drives via `g_mount_can_unmount()`
3. Plays audio via GStreamer `playbin` element
4. Auto-advances to next track when current track finishes

## Distribution

- **Windows winget:** See [WINGET.md](WINGET.md)
- **macOS Homebrew:** See [homebrew/usb-groove.rb](homebrew/usb-groove.rb) for the cask formula

## License

This project is available under the MIT License.
