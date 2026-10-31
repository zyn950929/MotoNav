import Foundation

/// An existing encrypted HID connection may outlive the app and its U,1 notifications.
/// Probe after the navigation RX/TX channels are ready; GATT still enforces pairing.
struct BleCapabilityProbe {
    private(set) var attempts = 0
    private var lastAttempt: TimeInterval?
    mutating func next(ready: Bool, supported: Bool, now: TimeInterval) -> Bool {
        guard ready, !supported, attempts < 3 else { return false }
        if let lastAttempt, now - lastAttempt < 2 { return false }
        attempts += 1; lastAttempt = now
        return true
    }
    mutating func reset() { attempts = 0; lastAttempt = nil }
}
