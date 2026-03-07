# Repeat All Playback Mode — Design

## Summary

Add a "Repeat All" toggle that restarts the playlist from the beginning when it ends.

## Requirements

- Single toggle: Repeat All (on/off)
- No persistence — always starts Off at app launch
- Menu item with checkmark when enabled
- Tooltip shows `⟳` symbol when active
- Works with shuffle (repeat restarts the shuffled playlist)
- Both Windows and macOS implementations

## Implementation

### 1. State & Menu IDs

**Windows (`USBAutoPlayer.cpp`)**
```cpp
// Add to globals (~line 63)
static bool g_repeatAll = false;

// Add menu ID (~line 48)
#define ID_TRAY_REPEAT    1008
```

**macOS (`USBGroove.swift`)**
```swift
// Add to AppDelegate properties (~line 25)
private var repeatAll: Bool = false
```

### 2. Track-End Logic

**Windows (`MM_MCINOTIFY` handler)**
- When playlist ends and `g_repeatAll` is true, call `PlayTrack(0)`
- Log "Repeat All — restarting playlist."

**macOS (`audioPlayerDidFinishPlaying`)**
- When playlist ends and `repeatAll` is true, call `playTrack(0)`
- Log "Repeat All — restarting playlist."

### 3. Menu UI

**Windows**
- Add menu item "Repeat All" after Shuffle in `ShowContextMenu`
- Handle `ID_TRAY_REPEAT` in `WM_COMMAND`
- Show checkmark when `g_repeatAll` is true

**macOS**
- Add menu item "Repeat All" after Shuffle in `updateMenu`
- Add `toggleRepeat()` action method
- Set `.on`/`.off` state based on `repeatAll`

### 4. Tooltip

- Append `⟳` symbol to tooltip when repeat is active
- Update both `UpdateTrayTip()` (Windows) and `updateTooltip()` (macOS)

## Files to Modify

- `USBAutoPlayer.cpp` — Windows implementation
- `macos/USBGroove.swift` — macOS implementation
