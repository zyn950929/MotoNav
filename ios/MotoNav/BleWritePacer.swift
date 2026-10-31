import Foundation

/// CoreBluetooth backpressure protects the host stack, not the display's 47-packet RX ring.
/// Share one burst budget across all pump calls, including ready callbacks and navigation updates.
struct BleWritePacer {
    static let burst = 6
    static let interval: TimeInterval = 0.02
    private var window: TimeInterval?
    private var used = 0
    mutating func take(now: TimeInterval) -> Bool {
        if window == nil || now - window! >= Self.interval { window = now; used = 0 }
        guard used < Self.burst else { return false }
        used += 1
        return true
    }
    func delay(now: TimeInterval) -> TimeInterval {
        max(0.001, Self.interval - (now - (window ?? now)))
    }
    mutating func reset() { window = nil; used = 0 }
}
