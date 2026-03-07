# Contributing to USB Groove

Thanks for your interest in contributing! This document will help you get started.

## Reporting Bugs

Found a bug? Please open an issue on [GitHub Issues](../../issues) with:

- A clear, descriptive title
- Steps to reproduce the problem
- Expected behavior vs actual behavior
- Your OS and version (Windows 10/11, macOS 12+)
- Any relevant log entries from `%TEMP%\USBAutoPlayer.log` (Windows) or `/tmp/USBGroove.log` (macOS)

## Suggesting Features

Have an idea for improvement? Open an issue on [GitHub Issues](../../issues) with:

- A clear description of the feature
- Why it would be useful
- Any implementation ideas you have

## Pull Requests

We welcome pull requests! Here's how to submit one:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-feature`)
3. Make your changes following the code style guidelines below
4. Test your changes on the relevant platform(s)
5. Commit with a clear message
6. Push to your fork and open a pull request

## Development Setup

### Windows

**Requirements:** Windows 10/11 with MSVC or MinGW.

**MSVC** (Developer Command Prompt):

```cmd
rc /fo USBAutoPlayer.res USBAutoPlayer.rc
cl USBAutoPlayer.cpp /O2 /W4 /EHsc /std:c++17 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /link USBAutoPlayer.res winmm.lib shell32.lib user32.lib gdi32.lib kernel32.lib /SUBSYSTEM:WINDOWS /OUT:USBGroove.exe
```

**MinGW / MSYS2:**

```bash
windres USBAutoPlayer.rc -O coff -o USBAutoPlayer.res
g++ -std=c++17 -O2 -Wall -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -o USBGroove.exe USBAutoPlayer.cpp USBAutoPlayer.res -lwinmm -lshell32 -luser32 -lgdi32 -mwindows
```

### macOS

**Requirements:** macOS 12+ with Xcode command line tools (`xcode-select --install`).

```bash
swiftc macos/USBGroove.swift -o USBGroove \
    -framework AVFoundation \
    -framework DiskArbitration \
    -framework AppKit -O
```

### Regenerating Icons

Icons are generated programmatically. To regenerate after modifying the icon design:

```bash
pip3 install Pillow
python3 icons/generate_icons.py
```

This creates `icons/app.ico` (Windows), PNG assets, and macOS menu bar template images.

## Code Style Guidelines

### Windows (C++)

- **C++ Standard**: Use C++17 features where appropriate
- **Strings**: Use `std::wstring` and wide-character functions (`L"..."` literals) for Unicode support
- **Windows API**: Follow standard Windows API conventions
  - Use `W` suffix for wide-character functions (e.g., `CreateWindowExW`)
  - Use `wchar_t` and `std::wstring` for string handling
  - Call `FindClose()`, `CloseHandle()`, etc. to release resources
- **Naming**: Use camelCase for functions, `g_` prefix for globals

### macOS (Swift)

- **Swift conventions**: Use standard Swift naming and style
- **Frameworks**: AVFoundation for audio, DiskArbitration for USB detection, AppKit for UI
- **Menu bar app**: Use `NSStatusItem` with template images for proper dark/light mode support

## Testing

This project uses manual testing:

### Windows
1. Build the executable
2. Run it (it will appear in the system tray)
3. Insert a USB drive with MP3 files
4. Verify automatic playback starts
5. Test tray icon controls (play/pause, next, previous, stop, shuffle, repeat all)
6. Check `%TEMP%\USBAutoPlayer.log` for any errors

### macOS
1. Build and run (music note appears in the menu bar)
2. Insert a USB drive with MP3 files
3. Verify automatic playback starts
4. Test menu bar controls (play/pause, next, previous, stop, shuffle, repeat all)
5. Check `/tmp/USBGroove.log` for any errors

---

Questions? Feel free to open an issue and ask!
