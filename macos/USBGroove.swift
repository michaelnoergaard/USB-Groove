// =============================================================================
// USBGroove.swift — macOS Menu Bar App
//
// Automatically plays MP3 files from any inserted USB flash drive.
// Uses AVFoundation for audio — no external player required.
// Runs as a macOS menu bar (status bar) application.
//
// Build:
//   swiftc USBGroove.swift -o USBGroove -framework AVFoundation \
//       -framework DiskArbitration -framework AppKit
// =============================================================================

import AppKit
import AVFoundation
import DiskArbitration
import SwiftUI

// MARK: - Track Info

struct TrackInfo: Identifiable {
    let id: Int
    let index: Int
    let name: String
    let url: URL
    var duration: String
    var position: String
    var isPlaying: Bool
}

// MARK: - Playlist Manager

class PlaylistManager: ObservableObject {
    @Published var tracks: [TrackInfo] = []
    @Published var currentTrackIndex: Int = -1
    private var updateTimer: Timer?
    private weak var appDelegate: AppDelegate?

    init(appDelegate: AppDelegate) {
        self.appDelegate = appDelegate
    }

    func startUpdates() {
        stopUpdates()
        updateTimer = Timer.scheduledTimer(withTimeInterval: 1.0, repeats: true) { [weak self] _ in
            self?.refreshPosition()
        }
        RunLoop.main.add(updateTimer!, forMode: .common)
    }

    func stopUpdates() {
        updateTimer?.invalidate()
        updateTimer = nil
    }

    func refreshPosition() {
        guard let appDelegate = appDelegate,
              currentTrackIndex >= 0,
              currentTrackIndex < tracks.count,
              let player = appDelegate.player,
              player.isPlaying else { return }

        let currentTime = player.currentTime
        let positionStr = formatTime(currentTime)
        tracks[currentTrackIndex].position = positionStr
    }

    func updateTracks(_ urls: [URL], current: Int) {
        currentTrackIndex = current
        tracks = urls.enumerated().map { index, url in
            let name = url.deletingPathExtension().lastPathComponent
            let duration: String
            var position: String = ""
            let isPlaying = index == current

            // Get duration from player if available
            if let player = appDelegate?.player, index == current {
                duration = formatTime(player.duration)
                if player.isPlaying {
                    position = formatTime(player.currentTime)
                }
            } else {
                // Try to get duration from file
                duration = getDurationForFile(url)
            }

            return TrackInfo(
                id: index,
                index: index,
                name: name,
                url: url,
                duration: duration,
                position: position,
                isPlaying: isPlaying
            )
        }
    }

    func updateCurrentIndex(_ index: Int) {
        // Clear old playing state
        if currentTrackIndex >= 0 && currentTrackIndex < tracks.count {
            tracks[currentTrackIndex].isPlaying = false
            tracks[currentTrackIndex].position = ""
        }

        currentTrackIndex = index

        // Set new playing state
        if index >= 0 && index < tracks.count {
            tracks[index].isPlaying = true
            if let player = appDelegate?.player {
                tracks[index].duration = formatTime(player.duration)
            }
        }
    }

    private func formatTime(_ time: TimeInterval) -> String {
        let totalSeconds = Int(time)
        let minutes = totalSeconds / 60
        let seconds = totalSeconds % 60
        return String(format: "%d:%02d", minutes, seconds)
    }

    private func getDurationForFile(_ url: URL) -> String {
        let asset = AVAsset(url: url)
        let duration = asset.duration
        let seconds = CMTimeGetSeconds(duration)
        if seconds.isFinite && seconds > 0 {
            return formatTime(seconds)
        }
        return "--:--"
    }
}

// MARK: - Playlist View (SwiftUI)

struct PlaylistView: View {
    @ObservedObject var manager: PlaylistManager
    let onTrackSelected: (Int) -> Void
    @State private var selectedTrackIndex: Int?

    var body: some View {
        Table(manager.tracks, selection: $selectedTrackIndex) {
            TableColumn("#") { track in
                Text("\(track.index + 1)")
                    .frame(width: 30, alignment: .trailing)
            }
            .width(min: 30, max: 40)

            TableColumn("Track") { track in
                HStack(spacing: 4) {
                    if track.isPlaying {
                        Image(systemName: "play.fill")
                            .foregroundColor(.accentColor)
                            .font(.system(size: 10))
                    }
                    Text(track.name)
                        .fontWeight(track.isPlaying ? .bold : .regular)
                }
                .contentShape(Rectangle())
                .onTapGesture(count: 2) {
                    onTrackSelected(track.index)
                }
            }
            .width(min: 150)

            TableColumn("Duration") { track in
                Text(track.duration)
                    .foregroundColor(.secondary)
                    .frame(width: 50, alignment: .trailing)
            }
            .width(min: 50, max: 60)

            TableColumn("Position") { track in
                Text(track.isPlaying ? track.position : "")
                    .foregroundColor(.accentColor)
                    .frame(width: 50, alignment: .trailing)
            }
            .width(min: 50, max: 60)
        }
        .contextMenu {
            ForEach(manager.tracks) { track in
                Button(track.name) {
                    onTrackSelected(track.index)
                }
            }
        }
    }
}

// MARK: - App Delegate

class AppDelegate: NSObject, NSApplicationDelegate, AVAudioPlayerDelegate, NSWindowDelegate {

    private var statusItem: NSStatusItem!
    private(set) var player: AVAudioPlayer?
    private var playlist: [URL] = []
    private var currentTrack: Int = -1
    private var shuffleOn: Bool = false
    private var repeatAll: Bool = false
    private var daSession: DASession?
    private var mountedUSBPaths: Set<String> = []
    private var playlistManager: PlaylistManager!
    private var playlistWindow: NSWindow?

    // MARK: - Lifecycle

    func applicationDidFinishLaunching(_ notification: Notification) {
        playlistManager = PlaylistManager(appDelegate: self)
        setupStatusItem()
        setupDiskArbitration()
        log("USB Groove started. Waiting for USB drives...")
    }

    func applicationWillTerminate(_ notification: Notification) {
        playlistManager.stopUpdates()
        playlistWindow?.close()
        stopPlayback()
        if let session = daSession {
            DASessionSetDispatchQueue(session, nil)
        }
    }

    // MARK: - Menu Bar

    private func setupStatusItem() {
        statusItem = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
        if let button = statusItem.button {
            // Try custom template icon from bundle Resources, fall back to system symbol
            if let iconPath = Bundle.main.path(forResource: "StatusBarIconTemplate", ofType: "png"),
               let icon = NSImage(contentsOfFile: iconPath) {
                icon.isTemplate = true
                button.image = icon
            } else {
                button.image = NSImage(systemSymbolName: "music.note", accessibilityDescription: "USB Groove")
            }
            button.toolTip = "USB Groove — waiting for USB drive"
        }
        updateMenu()
    }

    private func updateMenu() {
        let menu = NSMenu()

        if currentTrack >= 0 && !playlist.isEmpty {
            let title = trackTitle(currentTrack)
            let state = (player?.isPlaying == true) ? "Playing" : "Paused"
            let header = "\(state): \(title)  [\(currentTrack + 1)/\(playlist.count)]"
            let headerItem = NSMenuItem(title: header, action: nil, keyEquivalent: "")
            headerItem.isEnabled = false
            menu.addItem(headerItem)
            menu.addItem(NSMenuItem.separator())
        }

        let playPauseTitle = (player?.isPlaying == true) ? "Pause" : "Resume"
        let playPause = NSMenuItem(title: playPauseTitle, action: #selector(togglePlayPause), keyEquivalent: "p")
        playPause.target = self
        playPause.isEnabled = currentTrack >= 0
        menu.addItem(playPause)

        let prev = NSMenuItem(title: "Previous Track", action: #selector(prevTrack), keyEquivalent: "[")
        prev.target = self
        prev.isEnabled = currentTrack > 0
        menu.addItem(prev)

        let next = NSMenuItem(title: "Next Track", action: #selector(nextTrack), keyEquivalent: "]")
        next.target = self
        next.isEnabled = currentTrack >= 0 && currentTrack < playlist.count - 1
        menu.addItem(next)

        let stop = NSMenuItem(title: "Stop", action: #selector(stopAction), keyEquivalent: "s")
        stop.target = self
        stop.isEnabled = currentTrack >= 0
        menu.addItem(stop)

        menu.addItem(NSMenuItem.separator())

        let shuffle = NSMenuItem(title: "Shuffle", action: #selector(toggleShuffle), keyEquivalent: "")
        shuffle.target = self
        shuffle.state = shuffleOn ? .on : .off
        menu.addItem(shuffle)

        let repeatItem = NSMenuItem(title: "Repeat All", action: #selector(toggleRepeat), keyEquivalent: "")
        repeatItem.target = self
        repeatItem.state = repeatAll ? .on : .off
        menu.addItem(repeatItem)

        menu.addItem(NSMenuItem.separator())

        let playlistItem = NSMenuItem(title: "View Playlist", action: #selector(showPlaylistWindow), keyEquivalent: "l")
        playlistItem.target = self
        playlistItem.isEnabled = !playlist.isEmpty
        menu.addItem(playlistItem)

        menu.addItem(NSMenuItem.separator())

        let about = NSMenuItem(title: "About USB Groove", action: #selector(showAbout), keyEquivalent: "")
        about.target = self
        menu.addItem(about)

        let quit = NSMenuItem(title: "Quit", action: #selector(quitApp), keyEquivalent: "q")
        quit.target = self
        menu.addItem(quit)

        statusItem.menu = menu
    }

    private func updateTooltip() {
        if currentTrack >= 0 && !playlist.isEmpty {
            let title = trackTitle(currentTrack)
            let state = (player?.isPlaying == true) ? "" : " (paused)"
            let suffix = repeatAll ? " ⟳" : ""
            statusItem.button?.toolTip = "\(title) [\(currentTrack + 1)/\(playlist.count)]\(state)\(suffix)"
        } else {
            statusItem.button?.toolTip = "USB Groove — idle"
        }
    }

    private func showNotification(title: String, body: String) {
        let notification = NSUserNotification()
        notification.title = title
        notification.informativeText = body
        notification.soundName = nil
        NSUserNotificationCenter.default.deliver(notification)
    }

    // MARK: - Menu Actions

    @objc private func togglePlayPause() {
        guard currentTrack >= 0, let p = player else { return }
        if p.isPlaying {
            p.pause()
            log("Paused.")
        } else {
            p.play()
            log("Resumed.")
        }
        updateTooltip()
        updateMenu()
    }

    @objc private func nextTrack() {
        guard !playlist.isEmpty else { return }
        let next = currentTrack + 1
        if next < playlist.count {
            playTrack(next)
        }
    }

    @objc private func prevTrack() {
        guard !playlist.isEmpty else { return }
        let prev = currentTrack - 1
        if prev >= 0 {
            playTrack(prev)
        }
    }

    @objc private func stopAction() {
        stopPlayback()
    }

    @objc private func toggleShuffle() {
        shuffleOn.toggle()
        log(shuffleOn ? "Shuffle ON." : "Shuffle OFF.")
        updateMenu()
    }

    @objc private func toggleRepeat() {
        repeatAll.toggle()
        log(repeatAll ? "Repeat All ON." : "Repeat All OFF.")
        updateMenu()
    }

    @objc private func showAbout() {
        let alert = NSAlert()
        alert.messageText = "USB Groove"
        alert.informativeText = """
            macOS Menu Bar App

            Plays MP3 files automatically from any USB drive.
            No media player required — built-in audio engine.

            Click menu bar icon for controls.
            """
        alert.alertStyle = .informational
        alert.runModal()
    }

    @objc private func showPlaylistWindow() {
        // If window already exists, bring it to front
        if let window = playlistWindow {
            window.makeKeyAndOrderFront(nil)
            NSApp.activate(ignoringOtherApps: true)
            return
        }

        // Create SwiftUI view
        let playlistView = PlaylistView(
            manager: playlistManager,
            onTrackSelected: { [weak self] index in
                self?.playTrack(index)
            }
        )

        // Create hosting controller
        let hostingController = NSHostingController(rootView: playlistView)

        // Create window
        let window = NSWindow(
            contentRect: NSRect(x: 0, y: 0, width: 500, height: 400),
            styleMask: [.titled, .closable, .resizable, .miniaturizable],
            backing: .buffered,
            defer: false
        )
        window.title = "USB Groove - Playlist"
        window.contentViewController = hostingController
        window.center()
        window.isReleasedWhenClosed = false
        window.delegate = self

        playlistWindow = window
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)

        // Start position updates
        playlistManager.startUpdates()
    }

    @objc private func quitApp() {
        stopPlayback()
        NSApplication.shared.terminate(nil)
    }

    // MARK: - NSWindowDelegate

    func windowWillClose(_ notification: Notification) {
        if let window = notification.object as? NSWindow, window == playlistWindow {
            playlistManager.stopUpdates()
        }
    }

    // MARK: - Disk Arbitration (USB Detection)

    private func setupDiskArbitration() {
        guard let session = DASessionCreate(kCFAllocatorDefault) else {
            log("Failed to create DiskArbitration session.")
            return
        }
        daSession = session

        let mountCallback: DADiskMountApprovalCallback = { disk, context in
            return nil  // approve all mounts
        }
        _ = mountCallback  // suppress unused warning

        // Watch for volume mounts via NSWorkspace
        let center = NSWorkspace.shared.notificationCenter
        center.addObserver(
            self,
            selector: #selector(volumeDidMount(_:)),
            name: NSWorkspace.didMountNotification,
            object: nil
        )
        center.addObserver(
            self,
            selector: #selector(volumeDidUnmount(_:)),
            name: NSWorkspace.didUnmountNotification,
            object: nil
        )

        log("Disk monitoring active.")
    }

    @objc private func volumeDidMount(_ notification: Notification) {
        guard let userInfo = notification.userInfo,
              let volumePath = userInfo[NSWorkspace.volumeURLUserInfoKey] as? URL else {
            return
        }

        let path = volumePath.path

        // Check if this is a removable/external volume
        guard isExternalVolume(path) else {
            log("Volume \(path) is not external, skipping.")
            return
        }

        log("USB drive detected: \(path)")
        mountedUSBPaths.insert(path)

        // Small delay to let filesystem settle
        DispatchQueue.main.asyncAfter(deadline: .now() + 1.5) { [weak self] in
            self?.onDriveInserted(path: path)
        }
    }

    @objc private func volumeDidUnmount(_ notification: Notification) {
        guard let userInfo = notification.userInfo,
              let volumePath = userInfo[NSWorkspace.volumeURLUserInfoKey] as? URL else {
            return
        }

        let path = volumePath.path
        if mountedUSBPaths.remove(path) != nil {
            log("USB drive removed: \(path)")
            // Stop playback if we were playing from this drive
            if currentTrack >= 0, !playlist.isEmpty,
               playlist[currentTrack].path.hasPrefix(path) {
                stopPlayback()
                // Close playlist window
                playlistWindow?.close()
                playlistWindow = nil
                playlistManager.stopUpdates()
                showNotification(title: "USB Groove", body: "USB drive removed — playback stopped.")
            }
        }
    }

    private func isExternalVolume(_ path: String) -> Bool {
        let url = URL(fileURLWithPath: path)
        do {
            let values = try url.resourceValues(forKeys: [
                .volumeIsRemovableKey,
                .volumeIsEjectableKey,
                .volumeIsInternalKey
            ])
            let isRemovable = values.volumeIsRemovable ?? false
            let isEjectable = values.volumeIsEjectable ?? false
            let isInternal = values.volumeIsInternal ?? true
            return (isRemovable || isEjectable || !isInternal)
        } catch {
            return false
        }
    }

    // MARK: - Drive Handler

    private func onDriveInserted(path: String) {
        let mp3s = scanForMp3s(rootPath: path)
        if mp3s.isEmpty {
            let msg = "No MP3 files found on \(URL(fileURLWithPath: path).lastPathComponent)"
            showNotification(title: "USB Groove", body: msg)
            log(msg)
            return
        }

        let volumeName = URL(fileURLWithPath: path).lastPathComponent
        let msg = "Found \(mp3s.count) MP3 file(s) on \(volumeName) — starting playback."
        showNotification(title: "USB Groove", body: msg)
        log(msg)

        startPlaylist(mp3s)
    }

    // MARK: - MP3 Scanner

    private func scanForMp3s(rootPath: String) -> [URL] {
        let rootURL = URL(fileURLWithPath: rootPath)
        let fm = FileManager.default
        var results: [URL] = []

        guard let enumerator = fm.enumerator(
            at: rootURL,
            includingPropertiesForKeys: [.isRegularFileKey],
            options: [.skipsHiddenFiles, .skipsPackageDescendants]
        ) else {
            return results
        }

        for case let fileURL as URL in enumerator {
            if fileURL.pathExtension.lowercased() == "mp3" {
                results.append(fileURL)
            }
        }

        results.sort { $0.path < $1.path }
        return results
    }

    // MARK: - Playback Engine (AVFoundation)

    private func startPlaylist(_ tracks: [URL]) {
        stopPlayback()
        playlist = tracks

        if shuffleOn {
            playlist.shuffle()
        }

        playTrack(0)
    }

    private func playTrack(_ index: Int) {
        guard index >= 0 && index < playlist.count else { return }

        player?.stop()
        player = nil

        do {
            player = try AVAudioPlayer(contentsOf: playlist[index])
            player?.delegate = self
            player?.play()

            currentTrack = index
            updateTooltip()
            updateMenu()

            // Update playlist manager
            playlistManager.updateTracks(playlist, current: currentTrack)

            log("Playing [\(index + 1)/\(playlist.count)]: \(playlist[index].path)")
        } catch {
            log("Playback error: \(error.localizedDescription) — \(playlist[index].path)")
            // Skip bad file
            let next = index + 1
            if next < playlist.count {
                playTrack(next)
            }
        }
    }

    private func stopPlayback() {
        player?.stop()
        player = nil
        playlist.removeAll()
        currentTrack = -1
        playlistManager.updateTracks([], current: -1)
        updateTooltip()
        updateMenu()
        log("Playback stopped.")
    }

    // MARK: - AVAudioPlayerDelegate

    func audioPlayerDidFinishPlaying(_ player: AVAudioPlayer, successfully flag: Bool) {
        guard flag, currentTrack >= 0 else { return }

        let next = currentTrack + 1
        if next < playlist.count {
            playTrack(next)
        } else if repeatAll {
            playTrack(0)
            log("Repeat All — restarting playlist.")
        } else {
            currentTrack = -1
            self.player = nil
            playlistManager.updateCurrentIndex(-1)
            updateTooltip()
            updateMenu()
            showNotification(title: "USB Groove", body: "Playlist finished.")
            log("Playlist finished.")
        }
    }

    // MARK: - Utility

    private func trackTitle(_ index: Int) -> String {
        guard index >= 0 && index < playlist.count else { return "(none)" }
        var name = playlist[index].deletingPathExtension().lastPathComponent
        if name.count > 60 {
            name = String(name.prefix(57)) + "..."
        }
        return name
    }

    private func log(_ msg: String) {
        let formatter = DateFormatter()
        formatter.dateFormat = "yyyy-MM-dd HH:mm:ss"
        let timestamp = formatter.string(from: Date())
        let logLine = "[\(timestamp)] \(msg)\n"

        let logPath = NSTemporaryDirectory() + "USBGroove.log"
        if let handle = FileHandle(forWritingAtPath: logPath) {
            handle.seekToEndOfFile()
            handle.write(logLine.data(using: .utf8) ?? Data())
            handle.closeFile()
        } else {
            FileManager.default.createFile(atPath: logPath, contents: logLine.data(using: .utf8))
        }

        #if DEBUG
        print(logLine, terminator: "")
        #endif
    }
}

// MARK: - Main

let app = NSApplication.shared
app.setActivationPolicy(.accessory)  // menu bar only, no dock icon
let delegate = AppDelegate()
app.delegate = delegate
app.run()
