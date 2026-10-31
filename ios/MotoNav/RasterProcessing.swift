import Foundation

/// Protocol-compatible with Android RasterPacket: 1200², 2 m/pixel, bounded RLE.
enum RasterPacket {
    static let size = 1200, flag = 0x4000
    static func u16(_ b: [UInt8], _ p: Int) -> Int { Int(b[p]) | Int(b[p + 1]) << 8 }
    static func i32(_ b: [UInt8], _ p: Int) -> Int {
        Int(Int32(bitPattern: UInt32(b[p]) | UInt32(b[p+1]) << 8 | UInt32(b[p+2]) << 16 | UInt32(b[p+3]) << 24))
    }
    static func isBuilding(_ data: Data) -> Bool {
        let b = [UInt8](data); return b.count >= 16 && u16(b, 14) == (2000 | flag)
    }
    static func valid(_ data: Data, frame: Int, x: Int, y: Int) -> Bool {
        let b = [UInt8](data)
        guard b.count >= 17, b.count <= 480000, u16(b, 0) == frame, i32(b, 2) == x, i32(b, 6) == y,
              u16(b, 10) == size, u16(b, 12) == size, [2000, 2000 | flag, 2000 | 0x8000].contains(u16(b, 14)) else { return false }
        let customPalette = u16(b, 14) & 0x8000 != 0
        guard !customPalette || b.count >= 49 else { return false }
        var pos = customPalette ? 48 : 16, pixels = 0
        let building = u16(b, 14) == (2000 | flag)
        while pos < b.count && pixels < size * size {
            let v = Int(b[pos]); pos += 1
            if v & 15 > (customPalette ? 15 : building ? 7 : 3) { return false }
            var run = (v >> 4) + 1
            if v >> 4 == 15 {
                guard pos < b.count else { return false }
                run = 16 + Int(b[pos]); pos += 1
            }
            guard run <= size * size - pixels else { return false }
            pixels += run
        }
        return pixels == size * size && pos == b.count
    }
    static func valid(_ data: Data) -> Bool {
        let b = [UInt8](data)
        guard b.count >= 16 else { return false }
        return valid(data, frame: u16(b, 0), x: i32(b, 2), y: i32(b, 6))
    }
    /// Check RLE runs without expanding a 1.44M-pixel frame on the main thread.
    static func hasMapContent(_ data:Data)->Bool {
        guard valid(data) else{return false}
        let b=[UInt8](data)
        if u16(b,14)&0x8000 != 0{return true}
        var p=16,materials=0
        let minimum=MapGeo.minMaterialPixels(w:size,h:size)
        while p<b.count {
            let v=b[p];p+=1
            var count=Int(v>>4)+1
            if v>>4==15 {count+=Int(b[p]);p+=1}
            if v&15 != 0 {materials+=count;if materials>=minimum{return true}}
        }
        return false
    }
    static func forLegacy(_ data: Data) -> Data {
        guard isBuilding(data), valid(data) else { return data }
        var b = [UInt8](data); b[14] = UInt8(2000 & 255); b[15] = UInt8(2000 >> 8)
        var p = 16
        while p < b.count {
            let v = b[p]
            if v & 15 > 3 { b[p] = (v & 240) | (v & 2) }
            p += v >> 4 == 15 ? 2 : 1
        }
        return Data(b)
    }
    static func decode(_ data: Data) -> [UInt8]? {
        guard valid(data) else { return nil }
        let b = [UInt8](data)
        var out = [UInt8](repeating: 0, count: size * size), p = u16(b, 14) & 0x8000 != 0 ? 48 : 16, k = 0
        while p < b.count {
            let v = b[p]; p += 1
            var run = Int(v >> 4) + 1
            if v >> 4 == 15 { run += Int(b[p]); p += 1 }
            for i in k..<(k + run) { out[i] = v & 15 }; k += run
        }
        return out
    }
}

enum MapStyleTransfer {
    static func command(_ style:Int)->String {"#t,\(style),1"}
    static func matches(_ packet:Data,style:Int)->Bool {
        guard RasterPacket.valid(packet) else {return false}
        let bytes=[UInt8](packet),flags=RasterPacket.u16(bytes,14)
        if style==0 {return flags&0x8000 != 0}
        if style==1 {return flags&0x8000 == 0}
        if style==5 {
            guard flags==2000 else {return false}
            var p=16
            while p<bytes.count {
                let v=bytes[p];p+=1
                if v&15>1 {return false}
                if v>>4==15 {p+=1}
            }
        }
        return true
    }
}


enum AmapCapturePolicy {
    // SDK zoom is based on logical view bounds, not on Retina screenshot pixel count.
    // Keep the 3x drawable below 4096 pixels; 1200 points still exceeds zoom 17
    // for a 1.2km building tile. Android pixel dimensions are not iOS points.
    static func renderSize(for tileSize: Int) -> Int { tileSize == 600 ? 1200 : 720 }
    static func aligned(_ reference: [Double], _ current: [Double]) -> Bool {
        guard reference.count == 4, current.count == 4 else { return false }
        return zip(reference, current).allSatisfy { a, b in a.isFinite && b.isFinite && abs(a - b) <= 0.5 }
    }
    static func completed(_ result: AmapBuildings.Result) -> Bool { result.buildings > 0 && result.roofPixels > 0 }
}

enum AmapSampleGrid {
    /// Nearest-neighbour sampling commutes with the per-pixel building difference.
    /// Compare only the map samples, rather than millions of unused Retina pixels.
    static func sample(_ pixels: [UInt32], withoutBuildings: [UInt32]?, width: Int, height: Int,
                       left: Double, top: Double, right: Double, bottom: Double,
                       north: Double, south: Double, size: Int) throws -> [UInt32] {
        let current = try resample(pixels, width: width, height: height, left: left, top: top,
                                   right: right, bottom: bottom, north: north, south: south, size: size)
        guard let reference = withoutBuildings else { return current }
        let previous = try resample(reference, width: width, height: height, left: left, top: top,
                                    right: right, bottom: bottom, north: north, south: south, size: size)
        return try AmapBuildings.difference(without: previous, with: current)
    }
    static func resample(_ pixels: [UInt32], width: Int, height: Int, left: Double, top: Double,
                         right: Double, bottom: Double, north: Double, south: Double, size: Int) throws -> [UInt32] {
        guard width > 0, height > 0, size > 0, pixels.count == width * height,
              [left, top, right, bottom, north, south].allSatisfy({ $0.isFinite }), left >= 0, top >= 0,
              right <= Double(width), bottom <= Double(height), right > left, bottom > top,
              north > south, north < 85, south > -85 else { throw MapProcessingError.invalid("截图没有完整覆盖地图范围") }
        func mercator(_ lat: Double) -> Double { log(tan(.pi / 4 + lat * .pi / 360)) }
        let mn = mercator(north), ms = mercator(south)
        var cols = [Int](repeating: 0, count: size), rows = cols
        for i in 0..<size {
            let u = (Double(i) + 0.5) / Double(size)
            cols[i] = Int(floor(left + u * (right - left)))
            rows[i] = Int(floor(top + (mn - mercator(north + (south - north) * u)) / (mn - ms) * (bottom - top)))
            guard cols[i] >= 0, cols[i] < width, rows[i] >= 0, rows[i] < height else { throw MapProcessingError.invalid("截图采样越界") }
        }
        var result = [UInt32](repeating: 0, count: size * size)
        for y in 0..<size { for x in 0..<size {
            let c = pixels[rows[y] * width + cols[x]]
            guard c >> 24 >= 250 else { throw MapProcessingError.invalid("截图仍有透明区域") }
            result[y * size + x] = c
        } }
        return result
    }
}

enum MapProcessingError: LocalizedError {
    case invalid(String), cancelled
    var errorDescription: String? {
        switch self { case .invalid(let message): return message; case .cancelled: return "地图任务已取消" }
    }
}

enum AmapBuildings {
    struct Result { let pixels: [UInt8]; let buildings: Int; let roofPixels: Int }
    static func colorDifference(_ a: UInt32, _ b: UInt32) -> Int {
        let r = abs(Int((a >> 16) & 255) - Int((b >> 16) & 255))
        let g = abs(Int((a >> 8) & 255) - Int((b >> 8) & 255))
        let blue = abs(Int(a & 255) - Int(b & 255))
        return max(r, max(g, blue))
    }
    static func difference(without: [UInt32], with: [UInt32]) throws -> [UInt32] {
        guard without.count == with.count else { throw MapProcessingError.invalid("楼块截图尺寸发生变化") }
        return try zip(without, with).map { a, b in
            guard a >> 24 >= 250, b >> 24 >= 250 else { throw MapProcessingError.invalid("楼块截图含透明区域") }
            let delta = max(abs(Int(a >> 16 & 255) - Int(b >> 16 & 255)),
                            max(abs(Int(a >> 8 & 255) - Int(b >> 8 & 255)), abs(Int(a & 255) - Int(b & 255))))
            return delta >= 4 ? 0xffff00ff : 0xfff8f9fa
        }
    }
    static func marked(_ c: UInt32) -> Bool {
        let r = c >> 16 & 255, g = c >> 8 & 255, b = c & 255
        return r >= 140 && b >= 140 && g * 2 < min(r, b)
    }
    static func apply(_ raw: [UInt32], base: [UInt8], w: Int, h: Int) throws -> Result {
        guard w > 0, h > 0, w * h == raw.count, raw.count == base.count else { throw MapProcessingError.invalid("楼块图尺寸不一致") }
        var mask = raw.map { marked($0) ? UInt8(1) : 0 }
        guard mask.filter({ $0 == 1 }).count <= raw.count / 2 else { throw MapProcessingError.invalid("建筑图层覆盖异常") }
        var component: [Int] = []; component.reserveCapacity(raw.count / 8)
        var buildings = 0
        for i in mask.indices where mask[i] == 1 {
            component.removeAll(keepingCapacity: true); component.append(i); mask[i] = 2
            var head = 0, minX = i % w, maxX = minX, minY = i / w, maxY = minY
            while head < component.count {
                let p = component[head], x = p % w, y = p / w; head += 1
                minX = min(minX, x); maxX = max(maxX, x); minY = min(minY, y); maxY = max(maxY, y)
                func visit(_ n: Int) { if mask[n] == 1 { mask[n] = 2; component.append(n) } }
                if x > 0 { visit(p - 1) }; if x + 1 < w { visit(p + 1) }
                if y > 0 { visit(p - w) }; if y + 1 < h { visit(p + w) }
            }
            let bw = maxX - minX + 1, bh = maxY - minY + 1
            let valid = component.count >= 6 && component.count <= raw.count / 8 && bw >= 2 && bh >= 2 && component.count * 8 >= bw * bh
            if valid { buildings += 1 }
            for p in component { mask[p] = valid ? 3 : 4 }
        }
        var out = base, count = 0
        for y in 0..<h { for x in 0..<w {
            let i = y * w + x
            if mask[i] != 3 || (base[i] != 0 && base[i] != 2) { continue }
            let edge = x == 0 || x + 1 == w || y == 0 || y + 1 == h || mask[i-1] != 3 || mask[i+1] != 3 || mask[i-w] != 3 || mask[i+w] != 3
            out[i] = (edge ? 5 : 4) | (base[i] & 2); count += 1
        } }
        return Result(pixels: out, buildings: buildings, roofPixels: count)
    }
    /// Preserve known roofs in overlapping packets. This does not mark a partial layer complete.
    static func carry(_ incoming: Data, known: Data?) -> Data {
        guard let known = known, RasterPacket.valid(incoming), RasterPacket.valid(known), RasterPacket.isBuilding(known) else { return incoming }
        let a = [UInt8](incoming), b = [UInt8](known)
        guard RasterPacket.u16(a, 0) == RasterPacket.u16(b, 0) else { return incoming }
        let ox = Int(floor(Double(RasterPacket.i32(a, 2) - RasterPacket.i32(b, 2) + 1) / 2))
        let oy = Int(floor(Double(RasterPacket.i32(b, 6) - RasterPacket.i32(a, 6) + 1) / 2))
        let n = RasterPacket.size
        guard abs(ox) < n, abs(oy) < n, var current = RasterPacket.decode(incoming), let previous = RasterPacket.decode(known) else { return incoming }
        var retained = false
        for y in max(0, -oy)..<min(n, n - oy) { for x in max(0, -ox)..<min(n, n - ox) {
            let i = y * n + x, old = previous[(y + oy) * n + x + ox]
            if (current[i] == 0 || current[i] == 2) && old >= 4 && old <= 7 {
                current[i] = 4 | (old & 1) | (current[i] & 2); retained = true
            }
        } }
        guard retained else { return incoming }
        let packet = MapGeo.encodeRaster(frameId: RasterPacket.u16(a, 0), x0: RasterPacket.i32(a, 2), y0: RasterPacket.i32(a, 6), w: n, h: n, mmPerPx: 2000 | RasterPacket.flag, idx: current)
        return RasterPacket.valid(packet) ? packet : incoming
    }
}
