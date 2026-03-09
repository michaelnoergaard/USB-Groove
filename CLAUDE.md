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

## Architecture

Single-file C++ application with these major components:

- **WinMain**: Entry point, creates hidden window, single-instance guard via mutex
- **WndProc**: Window procedure handling tray events, device changes, MCI notifications, and timer callbacks
- **Tray Icon Management**: Add/remove/update tray icon, balloon notifications, context menu
- **Device Notifications**: Registers for `WM_DEVICECHANGE` to detect USB drive insertion
- **MP3 Scanner**: Recursive directory scan for `.mp3` files using Win32 `FindFirstFileW`
- **MCI Playback Engine**: Uses `mciSendStringW` for open/play/pause/close operations

### Key Windows APIs Used
- `winmm.dll` (MCI): Audio playback via `mciSendStringW`
- `shell32.dll`: System tray via `Shell_NotifyIconW`
- `dbt.h`: Device change notifications (`WM_DEVICECHANGE`, `DEV_BROADCAST_VOLUME`)

### Message Flow
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
