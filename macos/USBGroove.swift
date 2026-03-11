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

// MARK: - App Delegate

class AppDelegate: NSObject, NSApplicationDelegate, AVAudioPlayerDelegate {

    private var statusItem: NSStatusItem!
    private(set) var player: AVAudioPlayer?
    private var playlist: [URL] = []
    private var currentTrack: Int = -1
    private var shuffleOn: Bool = false
    private var repeatAll: Bool = false
    private var daSession: DASession?
    private var mountedUSBPaths: Set<String> = []
    private var currentUSBPath: String?

    // MARK: - Lifecycle

    func applicationDidFinishLaunching(_ notification: Notification) {
        let version = Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "dev"
        setupStatusItem()
        setupDiskArbitration()
        log("USB Groove v\(version) started. Waiting for USB drives...")
    }

    func applicationWillTerminate(_ notification: Notification) {
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

        let eject = NSMenuItem(title: "Eject USB", action: #selector(ejectUSB), keyEquivalent: "e")
        eject.target = self
        eject.isEnabled = currentUSBPath != nil
        menu.addItem(eject)

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

        // Playlist submenu — list all tracks, current track gets a checkmark
        if !playlist.isEmpty {
            let playlistMenu = NSMenu()
            let limit = min(playlist.count, 50)
            for i in 0..<limit {
                let label = "\(i + 1). \(trackTitle(i))"
                let item = NSMenuItem(title: label, action: #selector(playlistTrackSelected(_:)), keyEquivalent: "")
                item.target = self
                item.tag = i
                if i == currentTrack {
                    item.state = .on
                }
                playlistMenu.addItem(item)
            }
            if playlist.count > 50 {
                let moreItem = NSMenuItem(title: "... and \(playlist.count - 50) more", action: nil, keyEquivalent: "")
                moreItem.isEnabled = false
                playlistMenu.addItem(moreItem)
            }
            let playlistItem = NSMenuItem(title: "Playlist", action: nil, keyEquivalent: "")
            playlistItem.submenu = playlistMenu
            menu.addItem(playlistItem)
        }

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

    @objc private func ejectUSB() {
        guard let path = currentUSBPath else { return }
        let volumeName = URL(fileURLWithPath: path).lastPathComponent
        stopPlayback()
        let success = NSWorkspace.shared.unmountAndEjectDevice(atPath: path)
        if success {
            mountedUSBPaths.remove(path)
            showNotification(title: "USB Groove", body: "\(volumeName) safely ejected.")
            log("Ejected USB drive: \(path)")
        } else {
            showNotification(title: "USB Groove", body: "Failed to eject \(volumeName).")
            log("Failed to eject USB drive: \(path)")
        }
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
        let version = Bundle.main.infoDictionary?["CFBundleShortVersionString"] as? String ?? "dev"
        let alert = NSAlert()
        alert.messageText = "USB Groove v\(version)"
        alert.informativeText = """
            macOS Menu Bar App

            Plays MP3 files automatically from any USB drive.
            No media player required — built-in audio engine.

            Click menu bar icon for controls.
            """
        alert.alertStyle = .informational
        alert.runModal()
    }

    @objc private func playlistTrackSelected(_ sender: NSMenuItem) {
        playTrack(sender.tag)
    }

    @objc private func quitApp() {
        stopPlayback()
        NSApplication.shared.terminate(nil)
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

        currentUSBPath = path
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
        currentUSBPath = nil
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
