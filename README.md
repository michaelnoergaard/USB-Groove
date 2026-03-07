# USB Groove

![Version](https://img.shields.io/badge/version-2.0-blue.svg)
![License](https://img.shields.io/badge/license-MIT-green.svg)
![Platform](https://img.shields.io/badge/platform-Windows-lightgrey.svg)

A lightweight Windows system tray application that automatically plays MP3 files from any inserted USB flash drive. No external media player required - uses the built-in Windows MCI audio engine.

## Features

- **Auto-play** - Automatically detects USB drives and starts playing MP3 files
- **System tray app** - Runs quietly in the background with minimal footprint
- **Shuffle mode** - Randomize playback order with one click
- **Full playback controls** - Play, pause, stop, next/previous track
- **Recursive scanning** - Finds MP3 files in all subdirectories
- **No dependencies** - Uses only built-in Windows DLLs (winmm.dll, shell32.dll, user32.dll, gdi32.dll, kernel32.dll)
- **Single instance** - Prevents multiple copies from running
- **Logging** - Debug log stored in `%TEMP%\USBAutoPlayer.log`

## Download

Download the latest release from [GitHub Releases](https://github.com/michael/USB-Groove/releases).

## Usage

1. Run `USBAutoPlayer.exe`
2. Insert a USB drive containing MP3 files
3. Playback starts automatically!

### Tray Icon Controls

| Action | Result |
|--------|--------|
| **Right-click** | Open context menu with playback controls |
| **Double-click** | Show current track info (or idle status) |

### Context Menu Options

- **Pause/Resume** - Toggle playback
- **Previous track** - Go to the previous song
- **Next track** - Skip to the next song
- **Stop** - Stop playback and clear playlist
- **Shuffle** - Enable/disable shuffle mode (toggle)
- **About** - View version information
- **Exit** - Close the application

## Build Instructions

### Prerequisites

- Windows 10/11
- C++17 compatible compiler

### MSVC (Developer Command Prompt)

```cmd
cl USBAutoPlayer.cpp /O2 /W4 /EHsc /std:c++17 ^
    /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN ^
    /link winmm.lib shell32.lib user32.lib gdi32.lib kernel32.lib ^
    /SUBSYSTEM:WINDOWS /OUT:USBAutoPlayer.exe
```

### MinGW / MSYS2

```bash
g++ -std=c++17 -O2 -Wall -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN \
    -o USBAutoPlayer.exe USBAutoPlayer.cpp \
    -lwinmm -lshell32 -luser32 -lgdi32 -mwindows
```

## How It Works

1. **Device Detection** - The application registers for Windows device change notifications to detect when new drives are mounted

2. **Drive Validation** - When a drive is inserted, it waits ~1.8 seconds for the filesystem to mount, then validates the drive type (skips network drives and RAM disks)

3. **MP3 Scanning** - Recursively scans all directories on the drive for `.mp3` files

4. **Playback** - Uses Windows MCI (Media Control Interface) via `winmm.dll` to play audio - no external codecs or players needed

5. **Auto-advance** - When a track finishes, automatically plays the next track in the playlist

## License

This project is available under the MIT License.
