import Foundation

/// Binary AMS relay from the round screen. Reassemble bytes before decoding UTF-8.
/// Header: A6, generation LE32, revision LE16, field, part, count, partial flag.
struct AMSMetadata {
    struct Value { var text = ""; var partial = false; var revision: UInt16 = 0 }
    private struct Assembly { var revision: UInt16; var partial: Bool; var parts: [Data?] }
    private(set) var generation: UInt32?
    private(set) var values = Array(repeating: Value(), count: 7)
    private var assemblies: [Int: Assembly] = [:]
    var ready: Bool { values[6].text == "READY" }
    var title: String { values[0].text }
    var artist: String { values[1].text }
    var album: String { values[2].text }
    var player: String { values[5].text }
    var playing: Bool? {
        let parts = values[4].text.split(separator: ",", omittingEmptySubsequences: false)
        guard parts.count == 3, let state = Int(parts[0]), (0...3).contains(state),
              let rate = Double(parts[1]), rate.isFinite, let elapsed = Double(parts[2]), elapsed.isFinite, elapsed >= 0 else { return nil }
        return state == 1
    }
    var summary: String {
        guard ready else { return values[6].text == "SEARCHING" ? "正在连接 iPhone 歌曲信息…" : "iPhone 暂未提供歌曲信息；媒体按键仍可使用" }
        guard !title.isEmpty else { return "歌曲信息已连接，请先在网易云或 QQ 音乐播放歌曲" }
        return [title + (values[0].partial ? "…" : ""), artist, album, player].filter { !$0.isEmpty }.joined(separator: " · ")
    }
    mutating func reset() { self = AMSMetadata() }
    /// Returns true only after a complete new field, or a new connection generation.
    mutating func receive(_ data: Data) -> Bool {
        let bytes = [UInt8](data)
        guard bytes.count >= 11, bytes[0] == 0xa6, bytes[7] < 7, bytes[9] > 0, bytes[9] <= 128,
              bytes[8] < bytes[9], bytes[10] <= 1 else { return false }
        let incoming = (0..<4).reduce(UInt32(0)) { $0 | (UInt32(bytes[1 + $1]) << (8 * $1)) }
        let revision = UInt16(bytes[5]) | UInt16(bytes[6]) << 8
        guard revision != 0 else { return false }
        var changed = false
        if generation != incoming {
            if let old = generation, Int32(bitPattern: incoming &- old) <= 0 { return false }
            reset(); generation = incoming; changed = true
        }
        let field = Int(bytes[7]), index = Int(bytes[8]), count = Int(bytes[9]), partial = bytes[10] == 1
        let old = values[field].revision
        guard old == 0 || Int16(bitPattern: revision &- old) > 0 else { return changed }
        if let pending = assemblies[field], pending.revision != revision,
           Int16(bitPattern: revision &- pending.revision) <= 0 { return changed }
        var assembly = assemblies[field].flatMap { $0.revision == revision ? $0 : nil }
            ?? Assembly(revision: revision, partial: partial, parts: Array(repeating: nil, count: count))
        guard assembly.parts.count == count, assembly.partial == partial else { assemblies[field] = nil; return changed }
        let payload = Data(bytes.dropFirst(11))
        if let previous = assembly.parts[index], previous != payload { assemblies[field] = nil; return changed }
        assembly.parts[index] = payload
        guard assembly.parts.compactMap({ $0 }).reduce(0, { $0 + $1.count }) <= 512 else { assemblies[field] = nil; return changed }
        assemblies[field] = assembly
        guard assembly.parts.allSatisfy({ $0 != nil }) else { return changed }
        let complete = assembly.parts.reduce(into: Data()) { $0.append($1!) }
        assemblies[field] = nil
        guard let text = String(data: complete, encoding: .utf8) else { return changed }
        values[field] = Value(text: text, partial: partial, revision: revision)
        return true
    }
}
