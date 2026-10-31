import Foundation

struct LyricsTrack: Equatable {
    let title: String, artist: String, album: String
    let duration: Double
    var key: String { "\(title)\n\(artist)\n\(album)\n\(duration)" }
    var url: URL? {
        guard !title.isEmpty, !artist.isEmpty, duration.isFinite, (1...3600).contains(duration) else { return nil }
        var url = URLComponents(string: "https://lrclib.net/api/get")!
        url.queryItems = [URLQueryItem(name: "track_name", value: title), URLQueryItem(name: "artist_name", value: artist), URLQueryItem(name: "duration", value: String(duration))]
        if !album.isEmpty { url.queryItems?.append(URLQueryItem(name: "album_name", value: album)) }
        return url.url
    }
    func matches(title: String, artist: String, album: String, duration: Double) -> Bool {
        func normalized(_ text: String) -> String {
            text.folding(options: [.caseInsensitive, .diacriticInsensitive, .widthInsensitive], locale: Locale(identifier: "en_US_POSIX"))
                .unicodeScalars.filter { CharacterSet.alphanumerics.contains($0) }.map(String.init).joined()
        }
        return duration.isFinite && abs(self.duration - duration) <= 2.1 && normalized(self.title) == normalized(title)
            && normalized(self.artist) == normalized(artist)
            && (self.album.isEmpty || album.isEmpty || normalized(self.album) == normalized(album))
    }
}

struct MediaPlaybackClock {
    let state: Int, rate: Double, elapsed: Double, anchor: TimeInterval
    init?(_ text: String, now: TimeInterval) {
        let p = text.split(separator: ",", omittingEmptySubsequences: false)
        guard p.count == 3, let state = Int(p[0]), (0...3).contains(state), let rate = Double(p[1]), rate.isFinite,
              let elapsed = Double(p[2]), elapsed.isFinite, elapsed >= 0 else { return nil }
        self.state = state; self.rate = rate; self.elapsed = elapsed; anchor = now
    }
    func position(now: TimeInterval, duration: Double) -> Double {
        // Paused clocks stay fixed even if the player supplied an old non-zero rate.
        let movement = state == 0 ? 0 : rate * max(0, now - anchor)
        return min(max(0, elapsed + movement), max(0, duration))
    }
}

struct LyricsTimeline {
    struct Cue: Equatable { let time: Double; let text: String }
    let cues: [Cue]
    init(_ lrc: String) {
        guard lrc.utf8.count <= 128 * 1024 else { cues = []; return }
        let stamp = try! NSRegularExpression(pattern: #"^\[(\d{1,3}):([0-5]\d)(?:[.:](\d{1,3}))?\]"#)
        let offsetTag = try! NSRegularExpression(pattern: #"\[offset:([+-]?\d+)\]"#, options: .caseInsensitive)
        var offset: Double = 0
        if let m = offsetTag.firstMatch(in: lrc, range: NSRange(lrc.startIndex..., in: lrc)),
           let r = Range(m.range(at: 1), in: lrc), let milliseconds = Double(lrc[r]), abs(milliseconds) <= 300_000 { offset = milliseconds / 1000 }
        var lines: [(Double, Int, String)] = []
        for (order, raw) in lrc.components(separatedBy: .newlines).prefix(3000).enumerated() {
            var line = raw.trimmingCharacters(in: .whitespaces); var times: [Double] = []
            while let m = stamp.firstMatch(in: line, range: NSRange(line.startIndex..., in: line)), times.count < 32 {
                let ns = line as NSString
                let minutes = Double(ns.substring(with: m.range(at: 1)))!, seconds = Double(ns.substring(with: m.range(at: 2)))!
                var fraction = 0.0
                if m.range(at: 3).location != NSNotFound { let digits = ns.substring(with: m.range(at: 3)); fraction = Double(digits)! / pow(10, Double(digits.count)) }
                times.append(max(0, minutes * 60 + seconds + fraction + offset))
                line = ns.substring(from: m.range.length)
            }
            let text = String(line.trimmingCharacters(in: .whitespaces).prefix(400))
            for time in times { lines.append((time, order, text)) }
        }
        lines.sort { $0.0 == $1.0 ? $0.1 < $1.1 : $0.0 < $1.0 }
        var merged: [Cue] = []
        for (time, _, text) in lines {
            if let last = merged.last, last.time == time {
                if !text.isEmpty && !last.text.components(separatedBy: " / ").contains(text) {
                    merged[merged.count - 1] = Cue(time: time, text: last.text.isEmpty ? text : last.text + " / " + text)
                }
            } else { merged.append(Cue(time: time, text: text)) }
        }
        cues = merged
    }
    func line(at seconds: Double) -> String {
        guard seconds.isFinite, seconds >= 0 else { return "" }
        var low = 0, high = cues.count
        while low < high { let mid = (low + high) / 2; if cues[mid].time <= seconds { low = mid + 1 } else { high = mid } }
        return low > 0 ? cues[low - 1].text : ""
    }
}
