#!/bin/bash
# Run this locally with `gh` authenticated: bash create-issues.sh
REPO="michaelnoergaard/USB-Groove"

gh issue create --repo "$REPO" --title "Continuous playback mode (loop/repeat)" --label "enhancement" --body "$(cat <<'EOF'
## Feature Request

Add a continuous playback mode that allows the playlist to loop when it reaches the end.

### Options to consider
- **Repeat All**: Loop entire playlist from the beginning when the last track finishes
- **Repeat One**: Loop the current track indefinitely
- **Shuffle + Repeat**: Shuffle the playlist and loop continuously

### Implementation notes
- Add toggle option in the tray context menu (Repeat All / Repeat One / Off)
- Persist the setting between sessions
- Visual indicator in the tray tooltip showing current repeat mode

### Platforms
- Windows
- macOS
EOF
)"

gh issue create --repo "$REPO" --title "Application icon for Windows and macOS" --label "enhancement" --body "$(cat <<'EOF'
## Feature Request

Design and implement a custom application icon for USB Groove on both platforms.

### Requirements
- Custom icon that represents USB + music/audio
- **Windows**: `.ico` format, multiple sizes (16x16, 32x32, 48x48, 256x256), used for system tray and executable
- **macOS**: `.icns` format for the app bundle, plus menu bar icon (template image for light/dark mode support)

### Tasks
- [ ] Design the icon (USB drive with music note or waveform)
- [ ] Generate `.ico` for Windows (embed as resource via `.rc` file)
- [ ] Generate `.icns` for macOS app bundle
- [ ] Create macOS menu bar template image (16x16 and 32x32 @2x)
- [ ] Update build process to include icon resources

### Platforms
- Windows
- macOS
EOF
)"

gh issue create --repo "$REPO" --title "View playlist / Now Playing UI" --label "enhancement" --body "$(cat <<'EOF'
## Feature Request

Add ability to view the current playlist and see which track is playing.

### Proposed behavior
- Right-click tray/menu bar → "View Playlist" opens a small window showing all loaded MP3s
- Highlight the currently playing track
- Display track name, duration (if available), and playback position
- Allow clicking a track to jump to it

### Nice to have
- Scrollable list for large playlists
- Track number / total count in the title bar
- Search/filter within the playlist

### Platforms
- Windows
- macOS
EOF
)"

gh issue create --repo "$REPO" --title "Volume control from tray/menu bar" --label "enhancement" --body "$(cat <<'EOF'
## Feature Request

Add volume control directly from the system tray (Windows) or menu bar (macOS) without needing to open system volume settings.

### Proposed behavior
- Context menu includes a volume submenu or slider
- Keyboard shortcut support for volume up/down while app is focused
- Mute/unmute toggle

### Implementation notes
- Windows: Use `mciSendStringW` with `setaudio ... volume to X`
- macOS: Use AVAudioPlayer volume property

### Platforms
- Windows
- macOS
EOF
)"

gh issue create --repo "$REPO" --title "Shuffle mode" --label "enhancement" --body "$(cat <<'EOF'
## Feature Request

Add a shuffle mode to randomize the playback order of tracks.

### Proposed behavior
- Toggle shuffle on/off from the tray/menu bar context menu
- When enabled, randomize the playlist order
- When disabled, return to the original file-system order
- Combine with repeat mode for continuous shuffled playback

### Platforms
- Windows
- macOS
EOF
)"

gh issue create --repo "$REPO" --title "Support additional audio formats (WAV, FLAC, AAC, OGG)" --label "enhancement" --body "$(cat <<'EOF'
## Feature Request

Extend audio format support beyond MP3.

### Formats to consider
- **WAV** — natively supported by MCI on Windows
- **FLAC** — popular lossless format
- **AAC/M4A** — common on Apple devices
- **OGG** — open-source format

### Implementation notes
- Windows MCI natively supports WAV; others may need codec or library support
- macOS AVFoundation supports MP3, WAV, AAC, FLAC out of the box
- Update file scanner to look for additional extensions
- Make supported formats configurable or auto-detect based on platform

### Platforms
- Windows
- macOS
EOF
)"

gh issue create --repo "$REPO" --title "Remember last playback position across sessions" --label "enhancement" --body "$(cat <<'EOF'
## Feature Request

When a USB drive is removed and re-inserted, resume playback from where it left off.

### Proposed behavior
- Save current track index and playback position when drive is ejected or app exits
- On re-insertion of the same drive, offer to resume from last position
- Identify drives by volume label or serial number

### Storage
- Small local config/state file (JSON or similar)
- Keyed by drive identifier

### Platforms
- Windows
- macOS
EOF
)"

gh issue create --repo "$REPO" --title "Keyboard/media key support for playback controls" --label "enhancement" --body "$(cat <<'EOF'
## Feature Request

Support system media keys (Play/Pause, Next, Previous) for controlling playback.

### Proposed behavior
- Respond to hardware media keys on keyboards
- Play/Pause, Next Track, Previous Track
- Works even when the app window is not focused

### Implementation notes
- Windows: Register global hotkeys or use `RegisterRawInputDevices` for media keys
- macOS: Use `MPRemoteCommandCenter` or `NSEvent.addGlobalMonitorForEvents`

### Platforms
- Windows
- macOS
EOF
)"

echo "Done! All issues created."
