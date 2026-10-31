import Foundation

/// ACK-driven lyrics setting; never changes the independently selected media mode.
struct LyricsDisplaySync {
    private(set) var desired = false
    private(set) var actual: Bool?
    private var lastSent: TimeInterval?
    mutating func configure(_ enabled: Bool, force: Bool = false) {
        if desired != enabled || force { desired = enabled; actual = nil; lastSent = nil }
    }
    mutating func reset() { actual = nil; lastSent = nil }
    mutating func acknowledge(_ enabled: Bool) { actual = enabled }
    mutating func command(now: TimeInterval, ready: Bool) -> String? {
        guard ready, actual != desired else { return nil }
        if let last = lastSent, now - last < 2 { return nil }
        lastSent = now
        return desired ? "#ML,1" : "#ML,0"
    }
}
