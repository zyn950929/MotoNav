import Foundation

struct RasterResult { let bytes: Data; let tiles: Int; let fromCache: Int }
enum SatelliteRaster {
    static let SAT_MPP=3.5
    static func encode(_ f: MapGeo.Frame,cx: Double,cy: Double,half: Double,raw: [UInt32],
                       shouldCancel: () -> Bool) throws -> RasterResult {
        if shouldCancel() { throw MapProcessingError.cancelled }
        let n=Int((2*half/SAT_MPP).rounded())
        guard n>=16, raw.count==n*n else { throw MapProcessingError.invalid("高德卫星图尺寸不正确") }
        let x0=Int((cx-half).rounded()),y0=Int((cy+half).rounded())
        let R=raw.map { Int(($0 >> 16) & 255) },G=raw.map { Int(($0 >> 8) & 255) },B=raw.map { Int($0 & 255) }
        // 3×3 模糊
        func blur(_ a: [Int]) -> [Int] {
            var o = [Int](repeating: 0, count: n * n)
            for j in 0..<n {
                for i in 0..<n {
                    var sum = 0, cnt = 0
                    for dj in -1...1 {
                        let jj = j + dj
                        if jj < 0 || jj >= n { continue }
                        for di in -1...1 {
                            let ii = i + di
                            if ii < 0 || ii >= n { continue }
                            sum += a[jj * n + ii]; cnt += 1
                        }
                    }
                    o[j * n + i] = sum / cnt
                }
            }
            return o
        }
        let r1 = blur(R), g1 = blur(G), b1 = blur(B)
        // k 均值找 16 种颜色：先按亮度排序均匀取 16 个起点，迭代 10 轮
        var rng = SeededRandom(seed: 7)
        let S = min(6000, n * n)
        let sIdx = (0..<S).map { _ in rng.next(n * n) }
        let sorted = sIdx.sorted { r1[$0] * 3 + g1[$0] * 6 + b1[$0] < r1[$1] * 3 + g1[$1] * 6 + b1[$1] }
        let K = 16
        var cr = (0..<K).map { Double(r1[sorted[($0 * 2 + 1) * S / (2 * K)]]) }
        var cg = (0..<K).map { Double(g1[sorted[($0 * 2 + 1) * S / (2 * K)]]) }
        var cb = (0..<K).map { Double(b1[sorted[($0 * 2 + 1) * S / (2 * K)]]) }
        func nearest(_ r: Int, _ g: Int, _ b: Int) -> Int {
            var best = 0, bd = Double.greatestFiniteMagnitude
            for k in 0..<K {
                let dr = Double(r) - cr[k], dg = Double(g) - cg[k], db = Double(b) - cb[k]
                let d = dr * dr * 2 + dg * dg * 4 + db * db * 3
                if d < bd { bd = d; best = k }
            }
            return best
        }
        for _ in 0..<10 {
            if shouldCancel() { throw MapProcessingError.cancelled }
            var sr = [Double](repeating: 0, count: K), sg = sr, sb = sr
            var sc = [Int](repeating: 0, count: K)
            for p in sIdx {
                let k = nearest(r1[p], g1[p], b1[p])
                sr[k] += Double(r1[p]); sg[k] += Double(g1[p]); sb[k] += Double(b1[p]); sc[k] += 1
            }
            for k in 0..<K where sc[k] > 0 { cr[k] = sr[k] / Double(sc[k]); cg[k] = sg[k] / Double(sc[k]); cb[k] = sb[k] / Double(sc[k]) }
        }
        let idx = (0..<(n * n)).map { p in UInt8(nearest(r1[p], g1[p], b1[p])) }
        let pal = (0..<K).map { k -> Int in
            let r = min(255, max(0, Int(cr[k].rounded()))), g = min(255, max(0, Int(cg[k].rounded()))), b = min(255, max(0, Int(cb[k].rounded())))
            return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        }
        if shouldCancel() { throw MapProcessingError.cancelled }
        let bytes = MapGeo.encodeRaster(frameId: f.id, x0: x0, y0: y0, w: n, h: n, mmPerPx: Int(SAT_MPP * 1000), idx: idx, pal: pal)
        if bytes.count > 480_000 { throw MapProcessingError.invalid("卫星图压缩后 \(bytes.count / 1024) KB，超过圆屏缓冲") }
        return RasterResult(bytes: bytes, tiles: 1, fromCache: 0)
    }
}

/// 固定种子的随机数（每次结果一样，卫星图颜色不会每次都变）
struct SeededRandom {
    private var s: UInt64
    init(seed: UInt64) { s = seed &* 6364136223846793005 &+ 1442695040888963407 }
    mutating func next(_ bound: Int) -> Int {
        s = s &* 6364136223846793005 &+ 1442695040888963407
        return Int((s >> 33) % UInt64(bound))
    }
}
