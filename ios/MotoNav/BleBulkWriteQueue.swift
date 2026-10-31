import Foundation

/// Keep one ATT write in flight; replacing a map must not resurrect its obsolete fragments.
struct BleBulkWriteQueue {
    struct Part { let type: Int, id: Int, data: Data; var attempts = 0 }
    enum Result: Equatable { case sent, retry, obsolete, failed }
    private var parts: [Part] = []
    private var latest: [Int: Int] = [:]
    private var acknowledged: [Int: Int] = [:]
    private(set) var inFlight: Part?
    var hasData: Bool { inFlight != nil || !parts.isEmpty }
    var canStart: Bool { inFlight == nil && !parts.isEmpty }
    var queuedCount: Int { parts.count }
    mutating func replace(type: Int, id: Int, packets: [Data], prioritize: Bool = false) {
        parts.removeAll { $0.type == type }
        latest[type] = id; acknowledged.removeValue(forKey: type)
        let incoming=packets.map { Part(type: type, id: id, data: $0) }
        if prioritize {parts.insert(contentsOf:incoming,at:0)} else {parts.append(contentsOf:incoming)}
    }
    mutating func next() -> Part? {
        guard canStart else { return nil }
        var part = parts.removeFirst(); part.attempts += 1; inFlight = part
        return part
    }
    mutating func complete(success: Bool) -> Result {
        guard let part = inFlight else { return .obsolete }
        inFlight = nil
        if success || acknowledged[part.type] == part.id { return .sent }
        guard latest[part.type] == part.id else { return .obsolete }
        if part.attempts < 3 { parts.insert(part, at: 0); return .retry }
        parts.removeAll { $0.type == part.type && $0.id == part.id }
        return .failed
    }
    mutating func acknowledge(type: Int, id: Int) {
        acknowledged[type] = id
        parts.removeAll { $0.type == type && $0.id == id }
        // An application K can arrive before the ATT callback; still wait for that callback.
    }
    mutating func reset() { parts.removeAll(); latest.removeAll(); acknowledged.removeAll(); inFlight = nil }
    /// Keep the ATT callback slot occupied, but retire all queued packets of these kinds.
    mutating func cancel(types:Set<Int>) {
        parts.removeAll{types.contains($0.type)}
        for type in types {latest.removeValue(forKey:type);acknowledged.removeValue(forKey:type)}
    }
}

enum BleOutboundPriority {
    enum Choice: Equatable { case command, bulk, image, idle }
    static func choose(commandReady: Bool, bulkReady: Bool, bulkPending: Bool, imageReady: Bool) -> Choice {
        if commandReady { return .command }
        if bulkReady { return .bulk }
        if !bulkPending && imageReady { return .image }
        return .idle
    }
}

/// Bound outstanding commands without paying an ATT round trip for every map fragment.
/// First/last fragments and failed retries always validate the transport with a response.
struct BleBulkBarrier {
    private var commands = 0
    mutating func needsResponse(seq: Int, parts: Int, retry: Bool, commandReady: Bool) -> Bool {
        if seq == 0 || seq == parts - 1 || retry || !commandReady || commands >= 4 { return true }
        commands += 1
        return false
    }
    mutating func reset() { commands = 0 }
}
