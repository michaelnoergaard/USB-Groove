# View Playlist / Now Playing UI — Design

## Summary

Add a "View Playlist" window that displays all tracks in the current playlist, highlights the currently playing track, shows duration and playback position, and allows clicking to jump to any track.

## Requirements

- "View Playlist" menu item in tray/menu bar context menu
- Opens window showing all MP3s in current playlist
- Highlights current track
- Shows track name, duration, and playback position
- Click on any track to jump to it
- Live updates (position refreshes every second)
- Both Windows and macOS implementations

## UI Approach

- **Windows**: ListView control with `LVS_REPORT` style (details view)
- **macOS**: SwiftUI `Table` or `List`

## UI Layout

```
┌─────────────────────────────────────────────────┐
│  USB Groove - Playlist                    [X]   │
├─────────────────────────────────────────────────┤
│  #  │ Track Name        │ Duration │ Position  │
├─────┼───────────────────┼──────────┼───────────┤
│  1  │ Song One          │  3:45    │           │
│  2  │ Another Track     │  4:12    │  2:30 ▶   │ ← current (highlighted)
│  3  │ Final Song        │  5:01    │           │
└─────────────────────────────────────────────────┘
```

## Implementation Details

### Windows (USBAutoPlayer.cpp)

**New Constants & Globals**
```cpp
#define ID_TRAY_PLAYLIST    1009
#define PLAYLIST_UPDATE_TIMER  200

static HWND g_hPlaylistWnd = nullptr;
static std::vector<int> g_trackDurations;  // cached durations in ms
```

**New Functions**
- `ShowPlaylistWindow()` — Create/show playlist window
- `UpdatePlaylistSelection()` — Update highlight and position
- `PlaylistWndProc()` — Window procedure for playlist window
- `GetTrackDuration(int index)` — Get cached duration
- `FormatTime(int ms)` — Convert ms to "M:SS" string

**Live Updates**
- 1-second timer (`SetTimer`) updates position column
- `mciSendStringW(L"status player position", ...)` to get current position
- On track change, `PostMessage` to playlist window to update selection

**Click-to-Jump**
- Handle `NM_DBLCLK` notification from ListView
- Call `PlayTrack(clickedIndex)` to jump

### macOS (USBGroove.swift)

**New Classes**
```swift
class PlaylistManager: ObservableObject {
    @Published var tracks: [TrackInfo]
    @Published var currentTrack: Int
    @Published var currentPosition: TimeInterval
    private var updateTimer: Timer?
}

struct TrackInfo {
    let index: Int
    let name: String
    let url: URL
    var duration: String
    var position: String
}

struct PlaylistWindow: View {
    @ObservedObject var playlistManager: PlaylistManager
}
```

**Live Updates**
- Timer fires every 1 second
- Reads `player.currentTime` for position
- SwiftUI auto-updates via `@Published`

**Click-to-Jump**
- `.onTapGesture` on row calls `jumpToTrack(index)`

### Duration Caching

Both platforms cache durations when playlist starts to avoid repeated file opens:
- Windows: `std::vector<int> g_trackDurations`
- macOS: `var trackDurations: [TimeInterval]`

## Edge Cases

| Scenario | Behavior |
|----------|----------|
| No playlist loaded | "View Playlist" menu item grayed out |
| Playlist empty | Window shows "No tracks" message |
| Track fails to play | Skip in list, show error indication |
| Window already open | Bring to front, don't create duplicate |
| USB drive removed | Close playlist window, clear playlist |
| App exits | Close playlist window |

## Files to Modify

- `USBAutoPlayer.cpp` — Windows implementation (~200 lines added)
- `macos/USBGroove.swift` — macOS implementation (~150 lines added)
