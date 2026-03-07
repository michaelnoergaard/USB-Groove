# Contributing to USB AutoPlayer

Thanks for your interest in contributing! This document will help you get started.

## Reporting Bugs

Found a bug? Please open an issue on [GitHub Issues](../../issues) with:

- A clear, descriptive title
- Steps to reproduce the problem
- Expected behavior vs actual behavior
- Your Windows version
- Any relevant log entries from `%TEMP%\USBAutoPlayer.log`

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
4. Test your changes on Windows
5. Commit with a clear message
6. Push to your fork and open a pull request

## Development Setup

### Requirements

- Windows 11 (or Windows 10)
- A C++ compiler: MSVC or MinGW

### Building with MSVC

Open Developer Command Prompt and run:

```
cl USBAutoPlayer.cpp /O2 /W4 /EHsc /std:c++17 /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /link winmm.lib shell32.lib user32.lib gdi32.lib kernel32.lib /SUBSYSTEM:WINDOWS /OUT:USBAutoPlayer.exe
```

### Building with MinGW / MSYS2

From a terminal with `mingw64/bin` in your PATH:

```
g++ -std=c++17 -O2 -Wall -DUNICODE -D_UNICODE -DWIN32_LEAN_AND_MEAN -o USBAutoPlayer.exe USBAutoPlayer.cpp -lwinmm -lshell32 -luser32 -lgdi32 -mwindows
```

## Code Style Guidelines

- **C++ Standard**: Use C++17 features where appropriate
- **Strings**: Use `std::wstring` and wide-character functions (`L"..."` literals) for Unicode support
- **Windows API**: Follow standard Windows API conventions
  - Use `W` suffix for wide-character functions (e.g., `CreateWindowExW`)
  - Use `wchar_t` and `std::wstring` for string handling
  - Call `FindClose()`, `CloseHandle()`, etc. to release resources
- **Naming**: Use camelCase for functions, `g_` prefix for globals
- **Comments**: Keep header comments updated with version and build instructions

## Testing

This project uses manual testing on Windows:

1. Build the executable
2. Run it (it will appear in the system tray)
3. Insert a USB drive with MP3 files
4. Verify automatic playback starts
5. Test tray icon controls (play/pause, next, previous, stop, shuffle)
6. Check `%TEMP%\USBAutoPlayer.log` for any errors

---

Questions? Feel free to open an issue and ask!
