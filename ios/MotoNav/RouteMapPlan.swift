import Foundation

/// Match the round display's speed-dependent raster viewport (including its displaced arrow).
enum MapCoveragePolicy {
    /// Existing building packets remain usable; extra screenshot passes yield at highway speeds.
    static func mayCaptureBuildings(kmh:Int)->Bool {kmh<60}
    static func radius(kmh: Int) -> Double {
        let scale = min(0.6, max(0.38, 0.85 - Double(max(0, kmh)) * 0.006))
        return 292 / scale
    }
    static func covers(x: Double, y: Double, cx: Double, cy: Double, kmh: Int, half: Double = 1200, margin: Double = 0) -> Bool {
        max(abs(x - cx), abs(y - cy)) + radius(kmh: kmh) + margin <= half
    }
    static func lookahead(kmh: Int, half: Double = 1200, preferred: Double = 450) -> Double {
        max(0, min(preferred, half - radius(kmh: kmh) - 180))
    }
    static func needsRefresh(x: Double, y: Double, cx: Double, cy: Double, bearing: Double, kmh: Int,
                             preparationSeconds: Double = 8) -> Bool {
        // Leave time for rendering, processing and BLE delivery before the current map expires.
        let distance = Double(max(0, kmh)) / 3.6 * max(1, min(30, preparationSeconds))
        let angle = bearing * .pi / 180
        let viewSpeed = max(120, kmh) // Firmware zoom recovers gradually after slowing down.
        return !covers(x: x, y: y, cx: cx, cy: cy, kmh: viewSpeed, margin: 100) ||
            !covers(x: x + sin(angle) * distance, y: y + cos(angle) * distance,
                    cx: cx, cy: cy, kmh: viewSpeed, margin: 100)
    }
}

/// Samples the actual route every 600 m, with 800 m lookahead and overlapping 2.4 km blocks.
struct RouteMapPlan {
    struct Node: Equatable { let x: Double, y: Double, distance: Double }
    static let step = 600.0, half = 1200.0
    let points: [(x: Double, y: Double)], cumulative: [Double], length: Double
    private(set) var nodes: [Node] = []
    init(_ points: [(x: Double, y: Double)]) throws {
        guard points.count >= 2, points.allSatisfy({ $0.x.isFinite && $0.y.isFinite }) else { throw MapProcessingError.invalid("路线坐标无效") }
        self.points = points
        var cumulative = [Double](repeating: 0, count: points.count)
        for i in 1..<points.count { cumulative[i] = cumulative[i-1] + hypot(points[i].x - points[i-1].x, points[i].y - points[i-1].y) }
        self.cumulative = cumulative; length = cumulative.last!
        guard length.isFinite, length / Self.step <= 20000 else { throw MapProcessingError.invalid("路线过长") }
        var s = 0.0
        while s < length { nodes.append(at(s)); s += Self.step }
        nodes.append(at(length))
    }
    func at(_ distance: Double) -> Node {
        let s = min(length, max(0, distance))
        var lo = 1, hi = cumulative.count - 1
        while lo < hi { let mid = (lo + hi) / 2; if cumulative[mid] < s { lo = mid + 1 } else { hi = mid } }
        let span = cumulative[lo] - cumulative[lo-1], t = span == 0 ? 0 : (s - cumulative[lo-1]) / span
        let x = points[lo-1].x + (points[lo].x - points[lo-1].x) * t, y = points[lo-1].y + (points[lo].y - points[lo-1].y) * t
        return Node(x: (x / 2).rounded(.toNearestOrEven) * 2, y: (y / 2).rounded(.toNearestOrEven) * 2, distance: s)
    }
    func progress(x: Double, y: Double, previous: Double) -> Double {
        var best = Double.infinity, result = previous
        for i in 1..<points.count {
            if previous >= 0 && (cumulative[i] < previous - 150 || cumulative[i-1] > previous + 5000) { continue }
            let a = points[i-1], dx = points[i].x - a.x, dy = points[i].y - a.y, d = dx * dx + dy * dy
            let t = d == 0 ? 0 : min(1, max(0, ((x - a.x) * dx + (y - a.y) * dy) / d))
            let error = hypot(x - a.x - dx * t, y - a.y - dy * t), s = cumulative[i-1] + (cumulative[i] - cumulative[i-1]) * t
            let score = error + (previous < 0 ? s * 1e-9 : abs(s - previous) * 1e-5)
            if score < best { best = score; result = s }
        }
        if best > 400, previous >= 0 { return progress(x: x, y: y, previous: -1) }
        return max(0, result)
    }
    func target(progress: Double, initial: Bool) -> Int {
        max(0, min(nodes.count - 1, Int(floor((progress + (initial ? Self.step / 2 : 800)) / Self.step))))
    }
    func displayTarget(progress: Double, x: Double, y: Double, kmh: Int) -> Int {
        let nearest = target(progress: progress, initial: true), next = min(nodes.count - 1, nearest + 1)
        // Switch to the next overlapping block as soon as it contains the full viewport.
        // Download lookahead alone must never choose a block that leaves the screen uncovered.
        if kmh > 5, next != nearest,
           MapCoveragePolicy.covers(x: x, y: y, cx: nodes[next].x, cy: nodes[next].y, kmh: max(120, kmh), margin: 50) {
            return next
        }
        return nearest
    }
}

/// Atomic, size-limited persistent packets. Invalid/truncated/wrong-coordinate packets are ignored.
final class RouteMapCache {
    private let root: URL
    private let lock = NSLock()
    static let shared = RouteMapCache()
    init(root: URL? = nil) {
        self.root = root ?? FileManager.default.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0].appendingPathComponent("route_maps")
    }
    private func file(frame: MapGeo.Frame, source: Int, buildings: Bool, node: RouteMapPlan.Node, style: Int) -> URL {
        let generation=style == 1 ? 2 : 1
        return root.appendingPathComponent("styles-v\(generation)-\(style)/\(buildings && source == 3 ? "v2" : "v1")-\(source)-\(buildings ? "buildings" : "base")/\(frame.refLat)_\(frame.refLng)/\(Int(node.x))_\(Int(node.y)).bin")
    }
    func read(frame: MapGeo.Frame, source: Int, buildings: Bool, node: RouteMapPlan.Node, style: Int = 1) -> Data? {
        lock.lock(); defer { lock.unlock() }
        let url = file(frame: frame, source: source, buildings: buildings, node: node, style: style)
        guard let attrs = try? FileManager.default.attributesOfItem(atPath: url.path),
              let size = attrs[.size] as? NSNumber, (17...480000).contains(size.intValue),
              let data = try? Data(contentsOf: url), RasterPacket.isBuilding(data) == buildings,
              RasterPacket.valid(data, frame: frame.id, x: Int(node.x - 1200), y: Int(node.y + 1200)),
              style != 1 || RasterPacket.hasMapContent(data) else { return nil }
        return data
    }
    func store(_ data: Data, frame: MapGeo.Frame, source: Int, node: RouteMapPlan.Node, style: Int = 1) throws {
        guard RasterPacket.valid(data, frame: frame.id, x: Int(node.x - 1200), y: Int(node.y + 1200)),
              style != 1 || RasterPacket.hasMapContent(data) else { throw MapProcessingError.invalid("离线底图校验失败") }
        guard style != 1 || RasterPacket.hasMapContent(data) else {throw MapProcessingError.invalid("空白底图未写入缓存")}
        lock.lock(); defer { lock.unlock() }
        // Old classified maps are unusable after the gray-roof fix. Reclaim them
        // before enforcing the quota, so a full old cache cannot block its replacement.
        if style == 1 {
            let obsolete=root.appendingPathComponent("styles-v1-1",isDirectory:true)
            if FileManager.default.fileExists(atPath:obsolete.path) {try FileManager.default.removeItem(at:obsolete)}
        }
        let file = file(frame: frame, source: source, buildings: RasterPacket.isBuilding(data), node: node, style: style)
        try FileManager.default.createDirectory(at: file.deletingLastPathComponent(), withIntermediateDirectories: true)
        var folder = root
        var values = URLResourceValues(); values.isExcludedFromBackup = true
        try folder.setResourceValues(values)
        if let capacity = (try? root.resourceValues(forKeys: [.volumeAvailableCapacityKey]))?.volumeAvailableCapacity,
           capacity < 32 * 1024 * 1024 { throw MapProcessingError.invalid("手机存储空间不足") }
        let existingSize = (try? file.resourceValues(forKeys: [.fileSizeKey]))?.fileSize ?? 0
        if totalBytesLocked() - Int64(existingSize) + Int64(data.count) > 512 * 1024 * 1024 { throw MapProcessingError.invalid("沿线离线缓存已达 512 MB") }
        try data.write(to: file, options: .atomic)
        #if os(iOS)
        try FileManager.default.setAttributes([.protectionKey: FileProtectionType.completeUntilFirstUserAuthentication], ofItemAtPath: file.path)
        #endif
    }
    private func totalBytesLocked() -> Int64 {
        let files = FileManager.default.enumerator(at: root, includingPropertiesForKeys: [.fileSizeKey, .isRegularFileKey])
        var bytes: Int64 = 0
        while let url = files?.nextObject() as? URL {
            if let v = try? url.resourceValues(forKeys: [.fileSizeKey, .isRegularFileKey]), v.isRegularFile == true { bytes += Int64(v.fileSize ?? 0) }
        }
        return bytes
    }
    func sizeText() -> String {
        lock.lock(); defer { lock.unlock() }
        return "离线缓存 \(totalBytesLocked() / 1024 / 1024) MB / 512 MB"
    }
}
