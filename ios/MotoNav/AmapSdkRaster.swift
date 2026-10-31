import Foundation

/// Serial leases prevent foreground and route-prefetch jobs changing the same screenshot camera.
enum AmapSdkRaster {
    private static let lease = NSLock()
    static func build(_ f: MapGeo.Frame, cx: Double, cy: Double, includeBuildings: Bool,
                      shouldCancel: @escaping () -> Bool, onBaseReady: ((Data) -> Void)? = nil, onBuildingStatus: ((String) -> Void)? = nil, satellite: Bool = false, style: Int = 1, preparedBase:Data? = nil) throws -> RasterResult {
        precondition(!Thread.isMainThread)
        func check() throws { if shouldCancel() { throw MapProcessingError.cancelled } }
        while !lease.try() { try check(); Thread.sleep(forTimeInterval: 0.1) }
        defer { lease.unlock() }
        let job = UUID().uuidString
        MapPipelineTrace.event("capture-start", ["job": job, "buildings": includeBuildings])
        func take(x: Int, y: Int, n: Int) throws -> [UInt32] {
            try check()
            let started = Date()
            MapPipelineTrace.event("tile-start", ["job": job, "size": n])
            let box = CaptureResultBox(), sem = DispatchSemaphore(value: 0)
            DispatchQueue.main.async {
                if shouldCancel() { box.set(.failure(MapProcessingError.cancelled)); sem.signal(); return }
                AmapCaptureHost.shared.capture(f, x: x, y: y, size: n, satellite: satellite, style: style) { result in box.set(result); sem.signal() }
            }
            var elapsed = 0.0
            do {
                while sem.wait(timeout: .now() + 0.15) == .timedOut {
                    try check(); elapsed += 0.15
                    if elapsed > 45 { throw MapProcessingError.invalid("高德截图超时") }
                }
                try check()
                let shot = try box.get().get()
                let sampled = try AmapSampleGrid.sample(shot.pixels, withoutBuildings: shot.withoutBuildings,
                                                       width: shot.width, height: shot.height, left: shot.left,
                                                       top: shot.top, right: shot.right, bottom: shot.bottom,
                                                       north: shot.north, south: shot.south, size: n)
                MapPipelineTrace.event("tile-ready", ["job": job, "size": n, "seconds": Date().timeIntervalSince(started)])
                return sampled
            } catch {
                MapPipelineTrace.event("tile-failed", ["job": job, "size": n, "seconds": Date().timeIntervalSince(started), "error": error.localizedDescription])
                // Enqueue cancellation before releasing the lease; a stale job never cancels the next one.
                DispatchQueue.main.async { AmapCaptureHost.shared.cancel() }
                throw error
            }
        }
        let half = satellite ? 700.0 : 1200.0
        let n = satellite ? 400 : 1200, x = Int((cx - half).rounded()), y = Int((cy + half).rounded())
        let roads:[UInt8],base:Data
        if includeBuildings,!satellite,style==1,let cached=preparedBase,
           RasterPacket.valid(cached,frame:f.id,x:x,y:y),!RasterPacket.isBuilding(cached),
           MapStyleTransfer.matches(cached,style:1),let decoded=RasterPacket.decode(cached) {
            roads=decoded;base=cached
        } else {
        let raw = try take(x: x, y: y, n: n)
        guard stride(from: 0, to: raw.count, by: 7).contains(where: { raw[$0] != raw[0] }) else { throw MapProcessingError.invalid("高德返回纯色底图") }
        if satellite { return try SatelliteRaster.encode(f,cx: cx,cy: cy,half: half,raw: raw,shouldCancel: shouldCancel) }
        if style == 0 {
            let q = MapStyles.original(raw)
            let packet = MapGeo.encodeRaster(frameId: f.id, x0: x, y0: y, w: n, h: n, mmPerPx: 2000, idx: q.indices, pal: q.palette)
            MapSampleReport.shared.capture(raw:raw,indices:q.indices,palette:q.palette,packet:packet,w:n,h:n,style:style)
            guard packet.count <= 480000, RasterPacket.valid(packet) else { throw MapProcessingError.invalid("高德原版底图过大") }
            onBaseReady?(packet); return RasterResult(bytes: packet, tiles: 1, fromCache: 0)
        }
        if style == 5 {
            let minimal=MapStyles.minimal(raw,size:n)
            let packet = MapGeo.encodeRaster(frameId: f.id, x0: x, y0: y, w: n, h: n, mmPerPx: 2000, idx: minimal)
            MapSampleReport.shared.capture(raw:raw,indices:minimal,packet:packet,w:n,h:n,style:style)
            guard RasterPacket.valid(packet) else { throw MapProcessingError.invalid("黑白道路数据无效") }
            onBaseReady?(packet); return RasterResult(bytes: packet, tiles: 1, fromCache: 0)
        }
        roads=MapGeo.classifyAmapBase(raw,w:n,h:n)
        base=MapGeo.encodeRaster(frameId:f.id,x0:x,y0:y,w:n,h:n,mmPerPx:2000,idx:roads)
        MapSampleReport.shared.capture(raw:raw,indices:roads,packet:base,w:n,h:n,style:style)
        guard MapGeo.materialPixelCount(roads)>=MapGeo.minMaterialPixels(w:n,h:n) else {
            throw MapProcessingError.invalid("底图尚未准备完整，保留已有地图")
        }
        try check()
        guard RasterPacket.valid(base) else { throw MapProcessingError.invalid("高德普通底图编码不完整") }
        try check()
        }
        MapPipelineTrace.event("base-ready", ["job": job, "bytes": base.count])
        onBaseReady?(base)
        guard includeBuildings else { return RasterResult(bytes: base, tiles: 1, fromCache: 0) }
        onBuildingStatus?("正在从高德近景截图提取建筑轮廓…")
        do {
            var roofs = [UInt32](repeating:0xfff8f9fa,count:n*n)
            for ty in 0...1 { for tx in 0...1 {
                try check()
                let tile = try take(x: x + tx * 1200, y: y - ty * 1200, n: 600)
                for j in 0..<600 { for i in 0..<600 where AmapBuildings.marked(tile[j * 600 + i]) {
                    roofs[(ty * 600 + j) * n + tx * 600 + i] = 0xffff00ff
                } }
            } }
            let buildings = try AmapBuildings.apply(roofs, base: roads, w: n, h: n)
            MapPipelineTrace.event("buildings-ready", ["job": job, "components": buildings.buildings, "pixels": buildings.roofPixels])
            guard AmapCapturePolicy.completed(buildings) else {
                onBuildingStatus?("未取得建筑轮廓，保留普通底图并稍后重试")
                return RasterResult(bytes: base, tiles: 5, fromCache: 0)
            }
            onBuildingStatus?("高德建筑：\(buildings.buildings) 片轮廓，\(buildings.roofPixels) 个建筑像素")
            let packet = MapGeo.encodeRaster(frameId: f.id, x0: x, y0: y, w: n, h: n, mmPerPx: 2000 | RasterPacket.flag, idx: buildings.pixels)
            guard RasterPacket.valid(packet) else { onBuildingStatus?("建筑地图数据超出限制，保留普通底图"); return RasterResult(bytes: base, tiles: 1, fromCache: 0) }
            return RasterResult(bytes: packet, tiles: 5, fromCache: 0)
        } catch MapProcessingError.cancelled { throw MapProcessingError.cancelled }
        catch { onBuildingStatus?("建筑提取未完成：\(error.localizedDescription)"); return RasterResult(bytes: base, tiles: 1, fromCache: 0) }
    }
}

private final class CaptureResultBox {
    private let lock = NSLock()
    private var result: Result<AmapCaptureHost.Shot, Error>?
    func set(_ value: Result<AmapCaptureHost.Shot, Error>) { lock.lock(); result = value; lock.unlock() }
    func get() -> Result<AmapCaptureHost.Shot, Error> {
        lock.lock(); defer { lock.unlock() }
        return result ?? .failure(MapProcessingError.invalid("高德截图未完成"))
    }
}
