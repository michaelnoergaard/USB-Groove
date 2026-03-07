# USB Groove

![Version](https://img.shields.io/badge/version-2.0-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
![Platform](https://img.shields.io/badge/platform-Windows%20%7C%20macOS-lightgrey.svg)

A lightweight tray/menu bar application that automatically plays MP3 files from any inserted USB flash drive. No external media player required — uses built-in OS audio engines.

## Features

- **Auto-play** - Automatically detects USB drives and starts playing MP3 files
- **System tray (Windows) / Menu bar (macOS)** - Runs quietly in the background
- **Shuffle mode** - Randomize playback order with one click
- **Full playback controls** - Play, pause, stop, next/previous track
- **Recursive scanning** - Finds MP3 files in all subdirectories
- **No dependencies** - Uses only built-in OS libraries
- **Single file** - One executable, nothing to install
- **Logging** - Debug log in `%TEMP%\USBAutoPlayer.log` (Windows) or `/tmp/USBGroove.log` (macOS)

## Download

Download the latest release from [GitHub Releases](https://github.com/michaelnoergaard/USB-Groove/releases).

| Platform | File | How to run |
|----------|------|------------|
| **Windows** | `USBGroove.exe` | Just download and run |
| **macOS** | `USBGroove-macOS.zip` | Unzip, move `USBGroove.app` to Applications |

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

| Control | Action |
|---------|--------|
| **Pause/Resume** | Toggle playback |
| **Previous track** | Go to the previous song |
| **Next track** | Skip to the next song |
| **Stop** | Stop playback and clear playlist |
| **Shuffle** | Enable/disable shuffle mode |
| **About** | View version information |

## Build from source

### Windows (MSVC)

```cmd
cl USBAutoPlayer.cpp /O2 /W4 /EHsc /std:c++17 ^
    /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
    /link winmm.lib shell32.lib user32.lib gdi32.lib kernel32.lib ^
    /SUBSYSTEM:WINDOWS /OUT:USBGroove.exe
```

### Windows (MinGW / MSYS2)

```bash
g++ -std=c++17 -O2 -Wall -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN \
    -o USBGroove.exe USBAutoPlayer.cpp \
    -lwinmm -lshell32 -luser32 -lgdi32 -mwindows
```

### macOS

```bash
swiftc macos/USBGroove.swift -o USBGroove \
    -framework AVFoundation \
    -framework DiskArbitration \
    -framework AppKit -O
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

## Distribution

- **Windows winget:** See [WINGET.md](WINGET.md)
- **macOS Homebrew:** See [homebrew/usb-groove.rb](homebrew/usb-groove.rb) for the cask formula

## License

This project is available under the MIT License.
