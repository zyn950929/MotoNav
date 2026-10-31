import Foundation

var checks = 0
func check(_ condition: @autoclosure () -> Bool, _ label: String) {
    precondition(condition(), label); checks += 1
}
func rejects(_ label: String, _ body: () throws -> Void) {
    do { try body(); preconditionFailure(label) } catch { checks += 1 }
}
let frame = MapGeo.Frame(refLat: 31, refLng: 121, id: 77)
let plan = try RouteMapPlan([(0, 0), (1200, 0), (1200, 1200)])
check(plan.nodes.count == 5 && plan.length == 2400, "600m route spacing")
check(plan.nodes[2] == RouteMapPlan.Node(x: 1200, y: 0, distance: 1200), "Follow actual corner")
check(plan.nodes.last?.y == 1200, "Include destination")
check(plan.progress(x: 1200, y: 700, previous: 1200) == 1900, "Project onto route")
check(plan.target(progress: 600, initial: false) == 2, "800m forward lookup")
check(plan.target(progress: 0, initial: true) == 0, "Start with first block")
let highway = try RouteMapPlan([(0, 0), (12000, 0)])
for distance in stride(from: 0.0, through: 12000, by: 20) {
    let current = highway.nodes[highway.target(progress: distance, initial: true)]
    check(MapCoveragePolicy.covers(x: distance, y: 0, cx: current.x, cy: 0, kmh: 120, margin: 100), "Current highway block covers complete round screen at 120km/h")
}
check(!MapCoveragePolicy.covers(x: 0, y: 0, cx: 800, cy: 0, kmh: 120), "Forward prefetch cannot be used as display block at highway speed")
check(!MapCoveragePolicy.covers(x: 2500, y: 0, cx: 0, cy: 0, kmh: 120), "Reject late building packet after vehicle moves beyond it")
check(highway.displayTarget(progress: 200, x: 200, y: 0, kmh: 120) == 0, "Do not activate a future block before it covers the viewport")
check(highway.displayTarget(progress: 240, x: 240, y: 0, kmh: 120) == 1, "Start a safe overlapping block before the midpoint")
check(highway.displayTarget(progress: 240, x: 240, y: 0, kmh: 0) == 0, "Stationary display remains on nearest block")
check(MapCoveragePolicy.needsRefresh(x: 100, y: 0, cx: 0, cy: 0, bearing: 90, kmh: 120), "Reserve processing and BLE time ahead of the map edge")
check(!MapCoveragePolicy.needsRefresh(x: 100, y: 0, cx: 0, cy: 0, bearing: 270, kmh: 120), "Returning toward the centre does not refresh early")
for speed in [40, 60, 120, 150] {
    var confirmed = highway.nodes[0], pending: (node: RouteMapPlan.Node, arrives: Double)?
    var time = 0.0, uninterrupted = true
    while time * Double(speed) / 3.6 < highway.length {
        let x = time * Double(speed) / 3.6
        if let delivery = pending, time >= delivery.arrives { confirmed = delivery.node; pending = nil }
        let wanted = highway.nodes[highway.displayTarget(progress: x, x: x, y: 0, kmh: speed)]
        if pending == nil, wanted != confirmed { pending = (wanted, time + 4) }
        uninterrupted = uninterrupted && MapCoveragePolicy.covers(x: x, y: 0, cx: confirmed.x, cy: confirmed.y, kmh: max(120, speed))
        time += 0.2
    }
    check(uninterrupted, "Continuous viewport coverage with four-second map processing and BLE delivery")
}
for speed in [0, 30, 60, 90, 120, 200] {
    let ahead = MapCoveragePolicy.lookahead(kmh: speed)
    check(MapCoveragePolicy.covers(x: 0, y: 0, cx: ahead, cy: 0, kmh: speed, margin: 100), "Non-route lookahead leaves enough screen coverage")
}
let overlapping = try RouteMapPlan([(0, 0), (6000, 0), (6000, 100), (0, 100)])
check(overlapping.progress(x: 1000, y: 90, previous: 1000) < 2000, "Do not jump across overlapping route legs")
let zero = try RouteMapPlan([(0, 0), (0, 0)])
check(zero.nodes.count == 1, "Zero-length route does not divide by zero")
rejects("Reject non-finite routes") { _ = try RouteMapPlan([(0, 0), (.nan, 0)]) }
rejects("Bound route node count") { _ = try RouteMapPlan([(0, 0), (20_000_000, 0)]) }
let n = RasterPacket.size
var pixels = [UInt8](repeating: 0, count: n * n)
pixels[0] = 1; pixels[1] = 2; pixels[2] = 3
for y in 0..<n { for x in 590..<610 {pixels[y*n+x]=1} }
let packet = MapGeo.encodeRaster(frameId: 77, x0: -1200, y0: 1200, w: n, h: n, mmPerPx: 2000, idx: pixels)
check(RasterPacket.valid(packet, frame: 77, x: -1200, y: 1200), "Raster RLE protocol")
check(RasterPacket.decode(packet) == pixels, "Raster exact roundtrip")
check(!RasterPacket.valid(Data(packet.dropLast())), "Reject truncated run")
var extra = packet; extra.append(0)
check(!RasterPacket.valid(extra), "Reject trailing bytes")
check(!RasterPacket.valid(packet, frame: 78, x: -1200, y: 1200), "Reject wrong frame")
check(!RasterPacket.valid(packet, frame: 77, x: 0, y: 1200), "Reject wrong origin")
var illegal = packet; illegal[16] = 7
check(!RasterPacket.valid(illegal), "Reject building index in unflagged packet")
pixels[4] = 4; pixels[5] = 5; pixels[6] = 6; pixels[7] = 7
let building = MapGeo.encodeRaster(frameId: 77, x0: -1200, y0: 1200, w: n, h: n, mmPerPx: 2000 | RasterPacket.flag, idx: pixels)
check(RasterPacket.valid(building) && RasterPacket.isBuilding(building), "Building flag accepted")
let legacy = RasterPacket.forLegacy(building), plain = RasterPacket.decode(legacy)!
check(!RasterPacket.isBuilding(legacy) && RasterPacket.valid(legacy), "Legacy packet remains valid")
check(Array(plain[4...7]) == [0, 0, 2, 2], "Legacy preserves underlying grass/ground")
let carried = AmapBuildings.carry(packet, known: building)
check(RasterPacket.decode(carried)?[5] == 5, "Keep overlapping known roof")
check(RasterPacket.decode(carried)?[0] == 1, "Road remains higher priority")
let moved = MapGeo.encodeRaster(frameId: 77, x0: -1198, y0: 1200, w: n, h: n, mmPerPx: 2000, idx: [UInt8](repeating: 0, count: n * n))
check(RasterPacket.decode(AmapBuildings.carry(moved, known: building))?[3] == 4, "Shift roof by exact map coordinates")
let raw = [UInt32](repeating: 0xffeeeeee, count: 4 * 4)
let sampled = try AmapSampleGrid.resample(raw, width: 4, height: 4, left: 0, top: 0, right: 4, bottom: 4, north: 31.01, south: 31, size: 2)
check(sampled == [UInt32](repeating: 0xffeeeeee, count: 4), "Mercator resampling")
rejects("Reject incomplete bounds") { _ = try AmapSampleGrid.resample(raw, width: 4, height: 4, left: -1, top: 0, right: 4, bottom: 4, north: 31.01, south: 31, size: 2) }
rejects("Reject transparent screenshot") { _ = try AmapSampleGrid.resample([UInt32](repeating: 0x00eeeeee, count: 16), width: 4, height: 4, left: 0, top: 0, right: 4, bottom: 4, north: 31.01, south: 31, size: 2) }
var without = [UInt32](repeating: 0xffeeeeee, count: 64), with = without
for y in 2...3 { for x in 2...4 { with[y * 8 + x] = 0xffaaaaaa } }
let diff = try AmapBuildings.difference(without: without, with: with)
for outputSize in [2, 3, 6] {
    let original = try AmapSampleGrid.resample(diff, width: 8, height: 8, left: 0.2, top: 0.1, right: 7.8, bottom: 7.9, north: 31.01, south: 31, size: outputSize)
    let optimized = try AmapSampleGrid.sample(with, withoutBuildings: without, width: 8, height: 8, left: 0.2, top: 0.1, right: 7.8, bottom: 7.9, north: 31.01, south: 31, size: outputSize)
    check(original == optimized, "Sampled building difference preserves exact map pixels at fractional bounds")
}
let baseSample = try AmapSampleGrid.sample(raw, withoutBuildings: nil, width: 4, height: 4, left: 0, top: 0, right: 4, bottom: 4, north: 31.01, south: 31, size: 2)
check(baseSample == sampled, "Base samples unchanged")
rejects("Reject mismatched reference screenshot") { _ = try AmapSampleGrid.sample(with, withoutBuildings: [], width: 8, height: 8, left: 0, top: 0, right: 8, bottom: 8, north: 31.01, south: 31, size: 2) }
rejects("Reject transparent reference samples") { _ = try AmapSampleGrid.sample(with, withoutBuildings: [UInt32](repeating: 0x00eeeeee, count: 64), width: 8, height: 8, left: 0, top: 0, right: 8, bottom: 8, north: 31.01, south: 31, size: 2) }
var ground = [UInt8](repeating: 0, count: 64); ground[18] = 1; ground[19] = 3
let roofs = try AmapBuildings.apply(diff, base: ground, w: 8, h: 8)
check(roofs.buildings == 1 && roofs.roofPixels == 4, "Real difference produces one roof")
check(roofs.pixels[18] == 1 && roofs.pixels[19] == 3, "Building cannot obscure road/water")
let unmarked = try AmapBuildings.apply(without, base: ground, w: 8, h: 8)
check(unmarked.roofPixels == 0, "Do not infer buildings from ground colors")
check(!AmapCapturePolicy.completed(unmarked), "Zero-roof capture must not be cached as completed buildings")
check(AmapCapturePolicy.completed(roofs), "Real detected roofs complete the layer")
check(AmapCapturePolicy.renderSize(for: 600) == 1200, "Building renderer stays within iPhone drawable texture limits")
check(AmapCapturePolicy.renderSize(for: 1200) == 720, "Base renderer stays within iPhone drawable texture limits")
check(AmapCapturePolicy.aligned([1,2,3,4], [1.4,2,3,4]), "Allow subpixel camera roundoff")
check(!AmapCapturePolicy.aligned([1,2,3,4], [1.6,2,3,4]), "Reject shifted building difference camera")
check(!AmapCapturePolicy.aligned([1,2,3,4], [1,.nan,3,4]), "Reject invalid projection")
check(!AmapCapturePolicy.aligned([1,2], [1,2]), "Reject incomplete projection")
rejects("Reject anomalous building coverage") { _ = try AmapBuildings.apply([UInt32](repeating: 0xffff00ff, count: 64), base: ground, w: 8, h: 8) }
let root = FileManager.default.temporaryDirectory.appendingPathComponent("motonav-map-tests-\(UUID().uuidString)")
defer { try? FileManager.default.removeItem(at: root) }
let cache = RouteMapCache(root: root), node = plan.nodes[0]
try cache.store(packet, frame: frame, source: 3, node: node)
check(cache.read(frame: frame, source: 3, buildings: false, node: node) == packet, "Persistent packet roundtrip")
check(cache.read(frame: frame, source: 1, buildings: false, node: node) == nil, "Provider-specific caches")
try cache.store(building, frame: frame, source: 3, node: node)
check(cache.read(frame: frame, source: 3, buildings: true, node: node) == building, "Separate building and base packets")
check(FileManager.default.fileExists(atPath: root.appendingPathComponent("styles-v2-1/v2-3-buildings/31.0_121.0/0_0.bin").path), "Use new cache generation for corrected building capture")
let oldBuildings = root.appendingPathComponent("styles-v2-1/v1-3-buildings/31.0_121.0/0_0.bin")
try FileManager.default.createDirectory(at: oldBuildings.deletingLastPathComponent(), withIntermediateDirectories: true)
try building.write(to: oldBuildings)
try FileManager.default.removeItem(at: root.appendingPathComponent("styles-v2-1/v2-3-buildings/31.0_121.0/0_0.bin"))
check(cache.read(frame: frame, source: 3, buildings: true, node: node) == nil, "Old unreliable completed building cache ignored")
rejects("Reject invalid cache origin") { try cache.store(packet, frame: frame, source: 3, node: plan.nodes[1]) }
let url = root.appendingPathComponent("styles-v2-1/v1-3-base/31.0_121.0/0_0.bin")
try Data(packet.dropLast()).write(to: url)
check(cache.read(frame: frame, source: 3, buildings: false, node: node) == nil, "Ignore corrupted files")
check(cache.read(frame: frame, source: 3, buildings: false, node: node, style: 0) == nil, "Original style never reads Vice City cache")
print("Map processing: \(checks) checks passed")
