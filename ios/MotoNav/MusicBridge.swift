import Foundation
import MediaPlayer

/// HID controls the active player; AMS metadata arrives from the round screen.
/// The optional App control mode uses the system Music player.
final class MusicBridge {
    static let shared = MusicBridge()
    private lazy var player = MPMusicPlayerController.systemMusicPlayer
    private var observing = false
    private var lastText = ""
    private var lastState = ""
    private var lyricsSync = LyricsDisplaySync()
    private var replay=MediaMetadataReplay()
    private(set) var metadata = AMSMetadata()
    func receiveAMS(_ data: Data) {
        let generation=metadata.generation
        guard metadata.receive(data) else { return }
        if generation != metadata.generation {replay.reset()}
        BleLink.shared.mediaSongInfo = metadata.summary
        refresh()
    }
    func resetAMS() {
        metadata.reset(); replay.reset(); lyricsSync.reset(); LyricsBridge.shared.stop(); lastText = ""; lastState = ""
        BleLink.shared.mediaSongInfo = ""
    }
    func syncMode() {
        guard BleLink.shared.mediaHIDSupported else { return }
        BleLink.shared.sendOnce(Settings.shared.musicHID ? "#HM,1" : "#HM,0")
        syncLyrics()
        pollMetadata(force:true)
    }
    func pollMetadata(force:Bool=false) {
        guard Settings.shared.musicHID,BleLink.shared.isReady,BleLink.shared.verified,BleLink.shared.mediaHIDSupported else {return}
        if replay.request(now:ProcessInfo.processInfo.systemUptime,needed:metadata.needsReplay,force:force) {BleLink.shared.sendOnce("#AMS?")}
    }
    func syncLyrics(force: Bool = false) {
        let desired = Settings.shared.musicLyrics && Settings.shared.musicHID
        let changed = lyricsSync.desired != desired
        lyricsSync.configure(desired, force: force)
        if !desired { LyricsBridge.shared.stop() }
        guard let command = lyricsSync.command(now: ProcessInfo.processInfo.systemUptime, ready: BleLink.shared.isReady || force || changed) else { return }
        BleLink.shared.sendLyricsMode(command)
    }
    func lyricsReply(_ enabled: Bool) { lyricsSync.acknowledge(enabled); syncLyrics() }
    func authorize() {
        MPMediaLibrary.requestAuthorization { _ in
            DispatchQueue.main.async { self.refresh(force: true) }
        }
    }
    func refresh(force: Bool = false) {
        if Settings.shared.musicHID {
            LyricsBridge.shared.update(metadata, force: force)
            let ready = BleLink.shared.mediaHIDSupported && BleLink.shared.mediaHIDReady && BleLink.shared.mediaHIDMode
            if metadata.ready {
                let title = metadata.title.isEmpty ? "等待播放器歌曲信息" : metadata.title + (metadata.values[0].partial ? "…" : "")
                let artist = metadata.artist
                publish(title: title, artist: artist, available: !metadata.title.isEmpty, playing: metadata.playing == true, force: force)
            } else {
                publish(title: "蓝牙媒体按键", artist: ready ? "请先在网易云或 QQ 音乐播放歌曲" : "请在 iPhone 蓝牙设置完成配对", available: false, playing: false, force: force)
            }
            return
        }
        LyricsBridge.shared.stop()
        guard MPMediaLibrary.authorizationStatus() == .authorized else {
            publish(title: "请在手机授权 Apple Music", artist: "仅支持系统音乐 App", available: false, playing: false, force: force)
            return
        }
        if !observing {
            observing = true
            player.beginGeneratingPlaybackNotifications()
            NotificationCenter.default.addObserver(self, selector: #selector(changed), name: .MPMusicPlayerControllerNowPlayingItemDidChange, object: player)
            NotificationCenter.default.addObserver(self, selector: #selector(changed), name: .MPMusicPlayerControllerPlaybackStateDidChange, object: player)
        }
        let item = player.nowPlayingItem
        publish(title: item?.title ?? "请先在音乐 App 播放歌曲", artist: item?.artist ?? "Apple Music", available: item != nil, playing: player.playbackState == .playing, force: force)
    }
    @objc private func changed() { DispatchQueue.main.async { self.refresh() } }
    private func publish(title: String, artist: String, available: Bool, playing: Bool, force: Bool) {
        let text = title + "\n" + artist, state = "#Z,\(available ? 1 : 0),\(playing ? 1 : 0),\(available ? 7 : 0)"
        if force || text != lastText {
            lastText = text
            for (slot, value) in [(5, title), (6, artist)] {
                let im = TextImage.render(String(value.prefix(200)), maxW: 360, size: slot == 5 ? 32 : 24, minSize: 18,
                    byteLimit:TextBitmapBudget.bytes(maxWrite:BleLink.shared.maxWrite))
                BleLink.shared.sendImage(slot: slot, w: im?.w ?? 0, h: im?.h ?? 0, data: im?.data ?? Data())
            }
        }
        if force || state != lastState { lastState = state; BleLink.shared.send(state) }
    }
    func handle(_ action: String) {
        if action == "Q", Settings.shared.musicHID { pollMetadata(force:true) }
        refresh(force: action == "Q")
        guard ["P", "N", "T"].contains(action) else { return }
        // A delayed legacy App event must not also control Music while HID is selected.
        guard !Settings.shared.musicHID else { return }
        guard MPMediaLibrary.authorizationStatus() == .authorized, player.nowPlayingItem != nil else {
            BleLink.shared.sendOnce("#ZACK,5"); return
        }
        BleLink.shared.sendOnce("#ZACK,2")
        let before = player.nowPlayingItem?.persistentID, wasPlaying = player.playbackState == .playing
        switch action {
        case "P": player.skipToPreviousItem()
        case "N": player.skipToNextItem()
        default: wasPlaying ? player.pause() : player.play()
        }
        DispatchQueue.main.asyncAfter(deadline: .now() + 1) {
            let changed = action == "T" ? (self.player.playbackState == .playing) != wasPlaying : self.player.nowPlayingItem?.persistentID != before
            BleLink.shared.sendOnce(changed ? "#ZACK,3" : "#ZACK,4")
            self.refresh(force: true)
        }
    }
}
