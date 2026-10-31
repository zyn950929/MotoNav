/// Navigation lifecycle only. The bridge and its callback relay access it on the main queue.
/// A destination-ahead icon (including a predicted zero distance) is never a terminal event.
final class ArrivalPolicy {
    enum State: Equatable { case idle, planning, gps, simulation, cruise, arrived, cancelled }
    enum EndEvent: Equatable { case destination, emulator }
    private(set) var generation: UInt64 = 0
    private(set) var state: State = .idle

    @discardableResult func preparePlan() -> UInt64 { advance(.planning) }
    @discardableResult func begin(simulation: Bool) -> UInt64 { advance(simulation ? .simulation : .gps) }
    @discardableResult func beginCruise() -> UInt64 { advance(.cruise) }
    private func advance(_ next: State) -> UInt64 {
        generation += 1
        state = next
        return generation
    }
    func isCurrent(_ token: UInt64) -> Bool { token == generation }
    func acceptsCallbacks(_ token: UInt64) -> Bool {
        isCurrent(token) && [.planning, .gps, .simulation, .cruise].contains(state)
    }
    func acceptsGuidance(_ token: UInt64) -> Bool {
        isCurrent(token) && (state == .gps || state == .simulation)
    }
    func complete(_ token: UInt64, event: EndEvent) -> Bool {
        guard acceptsGuidance(token), event != .emulator || state == .simulation else { return false }
        state = .arrived
        return true
    }
    func cancel(_ token: UInt64) { if isCurrent(token) { state = .cancelled } }
    func canClearArrival(_ token: UInt64) -> Bool { isCurrent(token) && state == .arrived }

    static func wireDirection(_ direction: String) -> String { direction == "A" ? "S" : direction }
}
