import Foundation
import CoreLocation

/// Requires ten minutes of fresh, accurate, stationary fixes; losing GPS never counts as parking.
final class CruiseIdleTimer {
    private var started = Date.distantFuture
    private var stamp = Date.distantPast
    private var lastReceived: TimeInterval?
    private var stationarySince: TimeInterval?
    private var anchor: CLLocation?
    func start(at date: Date = Date()) { stop(); started = date }
    func stop() { started = .distantFuture; stamp = .distantPast; clear() }
    private func clear() { lastReceived = nil; stationarySince = nil; anchor = nil }
    func onFix(_ location: CLLocation, now: TimeInterval = ProcessInfo.processInfo.systemUptime, wallTime: Date = Date()) {
        guard location.timestamp >= started, location.timestamp > stamp else { return }
        stamp = location.timestamp
        guard abs(location.timestamp.timeIntervalSince(wallTime)) <= 15,
              location.horizontalAccuracy >= 0, location.horizontalAccuracy <= 50,
              location.speed.isFinite, location.speed >= 0,
              CLLocationCoordinate2DIsValid(location.coordinate) else { clear(); return }
        if let last = lastReceived, now - last > 15 { clear() }
        lastReceived = now
        if location.speed * 3.6 >= 3 {
            stationarySince = nil; anchor = location; return
        }
        if let point = anchor {
            let accuracy = max(point.horizontalAccuracy > 0 ? point.horizontalAccuracy : 15,
                               location.horizontalAccuracy > 0 ? location.horizontalAccuracy : 15)
            if location.distance(from: point) >= max(25, 2 * accuracy) {
                stationarySince = nil; anchor = location; return
            }
        } else { anchor = location }
        if stationarySince == nil { stationarySince = now }
    }
    func shouldStop(now: TimeInterval = ProcessInfo.processInfo.systemUptime) -> Bool {
        guard let last = lastReceived, let since = stationarySince else { return false }
        return now >= last && now - last <= 15 && now - since >= 600
    }
}
