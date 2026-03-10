# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

USB Groove (USBAutoPlayer) is a Windows 11 system tray application that automatically plays MP3 files from any inserted USB flash drive. It uses the Windows MCI API (winmm.dll) for audio playback — no external media player required.

## Build Commands

### MSVC (Developer Command Prompt)
```bash
rc /fo USBAutoPlayer.res USBAutoPlayer.rc
cl USBAutoPlayer.cpp /O2 /W4 /EHsc /std:c++17 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /link USBAutoPlayer.res winmm.lib shell32.lib user32.lib gdi32.lib kernel32.lib advapi32.lib /SUBSYSTEM:WINDOWS /OUT:USBGroove.exe
```

### MinGW / MSYS2
```bash
windres USBAutoPlayer.rc -o USBAutoPlayer.res
g++ -std=c++17 -O2 -Wall -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -o USBGroove.exe USBAutoPlayer.cpp USBAutoPlayer.res -lwinmm -lshell32 -luser32 -lgdi32 -ladvapi32 -mwindows
```

### Linux
```bash
# Install dependencies (Ubuntu/Debian)
sudo apt install libappindicator3-dev libnotify-dev libgstreamer1.0-dev libglib2.0-dev libgtk-3-dev

# Install dependencies (Fedora)
sudo dnf install libappindicator-gtk3-devel libnotify-devel gstreamer1-devel glib2-devel gtk3-devel

# Build
g++ -std=c++17 -O2 -Wall linux/USBGroove.cpp -o USBGroove-Linux \
    $(pkg-config --cflags --libs appindicator3-0.1 glib-2.0 gio-2.0 gstreamer-1.0 libnotify) \
    -lstdc++fs
```

## Architecture

### Windows
Single-file C++ application with these major components:

- **WinMain**: Entry point, creates hidden window, single-instance guard via mutex
- **WndProc**: Window procedure handling tray events, device changes, MCI notifications, and timer callbacks
- **Tray Icon Management**: Add/remove/update tray icon, balloon notifications, context menu
- **Device Notifications**: Registers for `WM_DEVICECHANGE` to detect USB drive insertion
- **MP3 Scanner**: Recursive directory scan for `.mp3` files using Win32 `FindFirstFileW`
- **MCI Playback Engine**: Uses `mciSendStringW` for open/play/pause/close operations

### Linux
Single-file C++ application (`linux/USBGroove.cpp`) mirroring Windows structure:

- **main()**: flock single-instance guard, GTK/GStreamer/libnotify init, AppIndicator setup, GVolumeMonitor signals, `g_main_loop_run()`
- **System tray**: libappindicator3 with GTK menu (play/pause, prev, next, stop, shuffle, repeat, playlist submenu, autorun, about, quit)
- **USB detection**: GIO VolumeMonitor `mount-added`/`mount-removed` signals, filtered by `g_mount_can_unmount()`
- **MP3 scanner**: `std::filesystem::recursive_directory_iterator` with case-insensitive `.mp3` filter
- **Playback**: GStreamer `playbin` element with bus watch for EOS/ERROR
- **Autorun**: XDG autostart desktop file (`~/.config/autostart/usb-groove.desktop`)
- **Logging**: `/tmp/USBGroove.log` with timestamps

### Key Windows APIs Used
- `winmm.dll` (MCI): Audio playback via `mciSendStringW`
- `shell32.dll`: System tray via `Shell_NotifyIconW`
- `dbt.h`: Device change notifications (`WM_DEVICECHANGE`, `DEV_BROADCAST_VOLUME`)

### Key Linux Libraries Used
- `GStreamer` (playbin): Audio playback via `gst_element_set_state()`
- `libappindicator3`: System tray via `app_indicator_new()`
- `GIO`: USB detection via `GVolumeMonitor` mount signals
- `libnotify`: Desktop notifications via `notify_notification_new()`

### Message Flow (Windows)
1. USB drive inserted → `WM_DEVICECHANGE` with `DBT_DEVICEARRIVAL`
2. 1.8s timer delay for filesystem mount
3. `OnDriveInserted` scans for MP3s, creates playlist
4. `PlayTrack` opens file with MCI, plays with notify flag
5. `MM_MCINOTIFY` fires when track ends → auto-advance to next track

## Release Process

Pushes to `main` trigger `.github/workflows/build.yml` which:
1. Auto-generates version from commit count (v1.0, v1.1, etc.)
2. Builds with MSVC on `windows-latest`
3. Creates GitHub Release with `USBGroove.exe` attached

## Workflow

**Always create a pull request for changes. Never push directly to main.**

1. Create a feature branch from `main`
2. Commit changes to the branch
3. Push the branch to remote
4. Create a pull request for review
5. Merge only after approval
