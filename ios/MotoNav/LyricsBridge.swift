import Foundation

/// Optional third-party lyrics, independent of HID controls and AMS metadata.
/// All state is used on the main thread. No request is made while the toggle is off.
final class LyricsBridge {
    static let shared = LyricsBridge()
    private struct Record: Decodable {
        let trackName: String, artistName: String, albumName: String?, duration: Double
        let instrumental: Bool?, syncedLyrics: String?
    }
    private struct Cached { var timeline: LyricsTimeline?; var status: String; var expires: TimeInterval }
    private var cache: [String: Cached] = [:]
    private var key = "", token = UUID(), task: URLSessionDataTask?, work: DispatchWorkItem?, timer: Timer?
    private var titleRevision: UInt16 = 0, artistRevision: UInt16 = 0
    private var generation: UInt32?, playbackRevision: UInt16 = 0, clock: MediaPlaybackClock?
    private var track: LyricsTrack?, timeline: LyricsTimeline?, lastLine: String?, lyricsEnabled = false
    func update(_ metadata: AMSMetadata, force: Bool = false) {
        guard Settings.shared.musicLyrics && Settings.shared.musicHID && BleLink.shared.mediaLyricsSupported && BleLink.shared.isReady else { stop(); return }
        lyricsEnabled = true
        if generation != metadata.generation { resetTrack(); generation = metadata.generation; playbackRevision = 0; clock = nil }
        if titleRevision != metadata.values[0].revision || artistRevision != metadata.values[1].revision {
            titleRevision = metadata.values[0].revision; artistRevision = metadata.values[1].revision; lastLine = nil
        }
        if playbackRevision != metadata.values[4].revision {
            playbackRevision = metadata.values[4].revision
            clock = MediaPlaybackClock(metadata.values[4].text, now: ProcessInfo.processInfo.systemUptime)
        }
        guard metadata.ready, !metadata.title.isEmpty, !metadata.artist.isEmpty,
              !metadata.values[0].partial, !metadata.values[1].partial,
              let duration = Double(metadata.values[3].text), duration.isFinite, (1...3600).contains(duration) else {
            resetTrack(); BleLink.shared.mediaLyricsStatus = "等待完整歌曲信息"; return
        }
        let incoming = LyricsTrack(title: metadata.title, artist: metadata.artist, album: metadata.album, duration: duration)
        if incoming.key != key {
            resetTrack(); key = incoming.key; track = incoming
            BleLink.shared.mediaLyricsStatus = "正在匹配逐句歌词…"
            let current = token
            let item = DispatchWorkItem { [weak self] in
                guard let self = self, self.token == current else { return }
                self.fetch(incoming, token: current)
            }
            work = item; DispatchQueue.main.asyncAfter(deadline: .now() + 0.8, execute: item)
        }
        if force { lastLine = nil }
        if timer == nil { timer = Timer.scheduledTimer(withTimeInterval: 0.25, repeats: true) { [weak self] _ in self?.tick() } }
        tick()
    }
    func stop() {
        if lyricsEnabled { resetTrack(); timer?.invalidate(); timer = nil; lyricsEnabled = false }
        generation = nil; playbackRevision = 0; clock = nil; BleLink.shared.mediaLyricsStatus = ""
    }
    private func resetTrack() {
        token = UUID(); task?.cancel(); task = nil; work?.cancel(); work = nil; key = ""; track = nil; timeline = nil
        sendLine("")
    }
    private func fetch(_ track: LyricsTrack, token: UUID) {
        if let cached = cache[track.key], cached.expires > Date().timeIntervalSince1970 { apply(cached, track: track, token: token); return }
        guard let url = track.url else { return }
        var request = URLRequest(url: url); request.timeoutInterval = 12
        request.setValue("MotoNav/0.12.1 (iOS; optional synced lyrics)", forHTTPHeaderField: "User-Agent")
        task = URLSession.shared.dataTask(with: request) { [weak self] data, response, error in
            var result = Cached(timeline: nil, status: "歌词服务暂不可用，保留歌名和歌手", expires: Date().timeIntervalSince1970 + 60)
            if (response as? HTTPURLResponse)?.statusCode == 404 { result.status = "未找到匹配的逐句歌词"; result.expires += 540 }
            else if error == nil, (response as? HTTPURLResponse)?.statusCode == 200, let data = data, data.count <= 128 * 1024,
                    let record = try? JSONDecoder().decode(Record.self, from: data),
                    track.matches(title: record.trackName, artist: record.artistName, album: record.albumName ?? "", duration: record.duration) {
                if record.instrumental == true { result.status = "纯音乐，没有歌词" }
                else if let lrc = record.syncedLyrics {
                    let timeline = LyricsTimeline(lrc)
                    if timeline.cues.contains(where: { !$0.text.isEmpty }) { result = Cached(timeline: timeline, status: "逐句歌词已就绪 · LRCLIB", expires: Date().timeIntervalSince1970 + 86400) }
                    else { result.status = "此歌曲暂无逐句歌词" }
                } else { result.status = "此歌曲暂无逐句歌词" }
                if result.timeline == nil { result.expires += 540 }
            }
            let resolved = result
            DispatchQueue.main.async {
                guard let self = self, self.token == token, self.key == track.key, self.lyricsEnabled else { return }
                if self.cache.count >= 20, let oldest = self.cache.min(by: { $0.value.expires < $1.value.expires })?.key { self.cache.removeValue(forKey: oldest) }
                self.cache[track.key] = resolved; self.apply(resolved, track: track, token: token)
            }
        }
        task?.resume()
    }
    private func apply(_ result: Cached, track: LyricsTrack, token: UUID) {
        guard self.token == token, key == track.key, lyricsEnabled else { return }
        timeline = result.timeline; BleLink.shared.mediaLyricsStatus = result.status; tick()
    }
    private func tick() {
        guard lyricsEnabled, let track = track, let timeline = timeline, let clock = clock else { sendLine(""); return }
        sendLine(timeline.line(at: clock.position(now: ProcessInfo.processInfo.systemUptime, duration: track.duration)))
    }
    private func sendLine(_ text: String) {
        guard lastLine != text else { return }; lastLine = text
        if !text.isEmpty, let generation = generation {
            BleLink.shared.sendOnce("#LY,\(generation),\(titleRevision),\(artistRevision)")
        }
        TextImage.send(slot: 12, String(text.prefix(200)), maxW: 360, size: 22, minSize: 16)
    }
}
