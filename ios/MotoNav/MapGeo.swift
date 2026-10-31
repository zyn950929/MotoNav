import Foundation

/// 版本：跟随 App 版本（见 Config.swift，iOS v0.2 新增）
///
/// 小地图用到的纯计算部分（和安卓版 MapGeo.kt 一一对应，打包格式必须和固件一致）：
/// - WGS-84（GPS 原生坐标）和 GCJ-02（高德用的坐标）互相换算
/// - 经纬度 ↔ 平面米坐标（以所在区域的固定点为原点）
/// - 折线简化（Douglas-Peucker）、多边形拼接、裁剪
/// - 把地图块、导航路线、图片地图打包成发给圆屏的二进制数据
enum MapGeo {
    // MARK: - 坐标换算
    private static let A = 6378245.0
    private static let EE = 0.00669342162296594323

    private static func outOfChina(_ lat: Double, _ lng: Double) -> Bool {
        lng < 72.004 || lng > 137.8347 || lat < 0.8293 || lat > 55.8271
    }

    private static func tLat(_ x: Double, _ y: Double) -> Double {
        var r = -100.0 + 2.0 * x + 3.0 * y + 0.2 * y * y + 0.1 * x * y + 0.2 * abs(x).squareRoot()
        r += (20.0 * sin(6.0 * x * .pi) + 20.0 * sin(2.0 * x * .pi)) * 2.0 / 3.0
        r += (20.0 * sin(y * .pi) + 40.0 * sin(y / 3.0 * .pi)) * 2.0 / 3.0
        r += (160.0 * sin(y / 12.0 * .pi) + 320.0 * sin(y * .pi / 30.0)) * 2.0 / 3.0
        return r
    }

    private static func tLng(_ x: Double, _ y: Double) -> Double {
        var r = 300.0 + x + 2.0 * y + 0.1 * x * x + 0.1 * x * y + 0.1 * abs(x).squareRoot()
        r += (20.0 * sin(6.0 * x * .pi) + 20.0 * sin(2.0 * x * .pi)) * 2.0 / 3.0
        r += (20.0 * sin(x * .pi) + 40.0 * sin(x / 3.0 * .pi)) * 2.0 / 3.0
        r += (150.0 * sin(x / 12.0 * .pi) + 300.0 * sin(x / 30.0 * .pi)) * 2.0 / 3.0
        return r
    }

    /// WGS-84 → GCJ-02
    static func wgsToGcj(_ lat: Double, _ lng: Double) -> (lat: Double, lng: Double) {
        if outOfChina(lat, lng) { return (lat, lng) }
        var dLat = tLat(lng - 105.0, lat - 35.0)
        var dLng = tLng(lng - 105.0, lat - 35.0)
        let radLat = lat / 180.0 * .pi
        var magic = sin(radLat)
        magic = 1 - EE * magic * magic
        let sqrtMagic = magic.squareRoot()
        dLat = (dLat * 180.0) / ((A * (1 - EE)) / (magic * sqrtMagic) * .pi)
        dLng = (dLng * 180.0) / (A / sqrtMagic * cos(radLat) * .pi)
        return (lat + dLat, lng + dLng)
    }

    /// GCJ-02 → WGS-84（迭代两次，误差小于 1 米）
    static func gcjToWgs(_ lat: Double, _ lng: Double) -> (lat: Double, lng: Double) {
        var wLat = lat, wLng = lng
        for _ in 0..<2 {
            let g = wgsToGcj(wLat, wLng)
            wLat -= g.lat - lat
            wLng -= g.lng - lng
        }
        return (wLat, wLng)
    }

    // MARK: - 平面坐标（米）
    /// 以参考点为原点：x 向东，y 向北。几十公里范围内误差很小，够地图用。
    struct Frame {
        let refLat: Double, refLng: Double, id: Int
        private let kx: Double, ky = 110574.0
        init(refLat: Double, refLng: Double, id: Int) {
            self.refLat = refLat; self.refLng = refLng; self.id = id
            kx = cos(refLat * .pi / 180) * 111320.0
        }
        func x(_ lng: Double) -> Double { (lng - refLng) * kx }
        func y(_ lat: Double) -> Double { (lat - refLat) * ky }
        func lng(_ x: Double) -> Double { refLng + x / kx }
        func lat(_ y: Double) -> Double { refLat + y / ky }
    }

    // MARK: - 地图要素（通用几何数据）
    // 道路等级（线）：1 高速/快速路，2 主干道，3 次干道，4 支路/小区路，5 辅路/匝道
    // 面：10 绿地公园，11 水面
    static let CLS_PARK = 10
    static let CLS_WATER = 11

    /// 一个要素：pts 是平面米坐标 [x0,y0,x1,y1,...]
    struct Feature { let cls: Int; let poly: Bool; let pts: [Double] }

    /// Douglas-Peucker 折线简化，tol = 允许偏差（米）
    static func simplify(_ pts: [Double], _ tol: Double) -> [Double] {
        let n = pts.count / 2
        if n <= 2 { return pts }
        var keep = [Bool](repeating: false, count: n)
        keep[0] = true; keep[n - 1] = true
        var stack: [(Int, Int)] = [(0, n - 1)]
        let tol2 = tol * tol
        while let (s, e) = stack.popLast() {
            if e <= s + 1 { continue }
            let ax = pts[2 * s], ay = pts[2 * s + 1]
            let dx = pts[2 * e] - ax, dy = pts[2 * e + 1] - ay
            let len2 = dx * dx + dy * dy
            var maxD = -1.0, idx = -1
            for i in (s + 1)..<e {
                let px = pts[2 * i] - ax, py = pts[2 * i + 1] - ay
                let d2: Double
                if len2 < 1e-9 { d2 = px * px + py * py } else { let c = px * dy - py * dx; d2 = c * c / len2 }
                if d2 > maxD { maxD = d2; idx = i }
            }
            if maxD > tol2 {
                keep[idx] = true
                stack.append((s, idx)); stack.append((idx, e))
            }
        }
        var out: [Double] = []
        out.reserveCapacity(pts.count)
        for i in 0..<n where keep[i] { out.append(pts[2 * i]); out.append(pts[2 * i + 1]) }
        return out
    }

    private static func reversePts(_ p: [Double]) -> [Double] {
        let n = p.count / 2
        var r = [Double](repeating: 0, count: p.count)
        for i in 0..<n { r[2 * i] = p[2 * (n - 1 - i)]; r[2 * i + 1] = p[2 * (n - 1 - i) + 1] }
        return r
    }

    /// 把多段首尾相接的线拼成闭合环（大型公园、河流常由多段边界组成）。端点距离小于 0.5 米就算相接。
    static func joinRings(_ parts: [[Double]]) -> [[Double]] {
        var pool = parts.filter { $0.count >= 4 }
        var rings: [[Double]] = []
        func close(_ ax: Double, _ ay: Double, _ bx: Double, _ by: Double) -> Bool {
            (ax - bx) * (ax - bx) + (ay - by) * (ay - by) < 0.25
        }
        while !pool.isEmpty {
            var cur = pool.removeFirst()
            var progress = true
            while progress {
                let n = cur.count
                if close(cur[0], cur[1], cur[n - 2], cur[n - 1]) { break }      // 已闭合
                progress = false
                for i in pool.indices {
                    let p = pool[i], m = p.count
                    let ex = cur[n - 2], ey = cur[n - 1]
                    if close(ex, ey, p[0], p[1]) { cur += p[2..<m] }
                    else if close(ex, ey, p[m - 2], p[m - 1]) { cur += reversePts(p)[2..<m] }
                    else if close(cur[0], cur[1], p[m - 2], p[m - 1]) { cur = p + cur[2..<n] }
                    else if close(cur[0], cur[1], p[0], p[1]) { cur = reversePts(p) + cur[2..<n] }
                    else { continue }
                    pool.remove(at: i)
                    progress = true
                    break
                }
            }
            rings.append(cur)          // 拼不完整的也保留（按首尾直接相连来填充）
        }
        return rings
    }

    /// 把折线裁剪到方框里（保留进出方框的那一段），返回若干段
    static func clipLine(_ pts: [Double], _ x0: Double, _ y0: Double, _ x1: Double, _ y1: Double) -> [[Double]] {
        let n = pts.count / 2
        if n < 2 { return [] }
        // 一段线的外框和方框有重叠，就保留这段的两个端点（两端都在框外但穿过框的长直路也不会丢）
        var keep = [Bool](repeating: false, count: n)
        for i in 0..<(n - 1) {
            let ax = pts[2 * i], ay = pts[2 * i + 1], bx = pts[2 * i + 2], by = pts[2 * i + 3]
            if max(ax, bx) >= x0 && min(ax, bx) <= x1 && max(ay, by) >= y0 && min(ay, by) <= y1 {
                keep[i] = true; keep[i + 1] = true
            }
        }
        var out: [[Double]] = []
        var i = 0
        while i < n {
            if !keep[i] { i += 1; continue }
            let start = i
            while i < n && keep[i] { i += 1 }
            if i - start >= 2 { out.append(Array(pts[(2 * start)..<(2 * i)])) }
        }
        return out
    }

    // MARK: - 打包

    struct Buf {
        var b = Data()
        mutating func u8(_ v: Int) { b.append(UInt8(truncatingIfNeeded: v)) }
        mutating func u16(_ v: Int) { u8(v); u8(v >> 8) }
        mutating func i32(_ v: Int) { u8(v); u8(v >> 8); u8(v >> 16); u8(v >> 24) }
    }

    private static func ri(_ v: Double) -> Int { Int(v.rounded()) }

    /// 地图块：只保留方框（中心 cx,cy，半边长 half 米）附近的内容，简化后以中心为原点、1 米为单位打包。
    /// 格式：u16 坐标系编号, i32 原点 x, i32 原点 y（米）, u16 要素数,
    ///       每个要素：u8 类别（面 +0x80）, u16 点数, 点数 × (i16 x, i16 y)
    /// 超过 maxBytes 时依次去掉辅路、支路，保证圆屏放得下。
    static func encodeChunk(frameId: Int, cx: Double, cy: Double, half: Double,
                            feats: [Feature], maxBytes: Int, maxFeat: Int) -> Data {
        let ox = ri(cx), oy = ri(cy)
        let m = 150.0
        let bx0 = cx - half - m, bx1 = cx + half + m
        let by0 = cy - half - m, by1 = cy + half + m
        struct Q { let cls: Int; let poly: Bool; let q: [Int] }
        var all: [Q] = []
        for f in feats {
            var pieces: [[Double]] = []
            if f.poly {
                var minX = Double.greatestFiniteMagnitude, maxX = -Double.greatestFiniteMagnitude
                var minY = Double.greatestFiniteMagnitude, maxY = -Double.greatestFiniteMagnitude
                for i in 0..<(f.pts.count / 2) {
                    minX = min(minX, f.pts[2 * i]); maxX = max(maxX, f.pts[2 * i])
                    minY = min(minY, f.pts[2 * i + 1]); maxY = max(maxY, f.pts[2 * i + 1])
                }
                if !(maxX < bx0 || minX > bx1 || maxY < by0 || minY > by1) { pieces = [f.pts] }
            } else {
                pieces = clipLine(f.pts, bx0, by0, bx1, by1)
            }
            for p in pieces {
                let s = simplify(p, f.poly ? 3.0 : 1.5)
                var q: [Int] = []
                var lx = Int.min, ly = Int.min
                for i in 0..<(s.count / 2) {
                    let x = min(32000, max(-32000, ri(s[2 * i] - Double(ox))))
                    let y = min(32000, max(-32000, ri(s[2 * i + 1] - Double(oy))))
                    if x == lx && y == ly { continue }
                    q.append(x); q.append(y); lx = x; ly = y
                }
                let np = q.count / 2
                if np < (f.poly ? 3 : 2) { continue }
                // 单个要素点数上限（超长的切成几段）
                var st = 0
                while st < np - 1 {
                    let en = min(np, st + 4000)
                    all.append(Q(cls: f.cls, poly: f.poly, q: Array(q[(2 * st)..<(2 * en)])))
                    if f.poly { break }
                    st = en - 1
                }
            }
        }
        // 面先画（在底下），然后道路从细到粗
        all.sort { a, b in
            let pa = a.poly ? 0 : 1, pb = b.poly ? 0 : 1
            return pa != pb ? pa < pb : a.cls > b.cls
        }
        func size(_ list: [Q]) -> Int { 12 + list.reduce(0) { $0 + 3 + $1.q.count * 2 } }
        var list = all
        for drop in [5, 4, 3] {
            if size(list) <= maxBytes && list.count <= maxFeat { break }
            list = list.filter { $0.poly || $0.cls < drop }
        }
        var total = size(list)
        while (total > maxBytes || list.count > maxFeat), let last = list.popLast() { total -= 3 + last.q.count * 2 }
        var b = Buf()
        b.u16(frameId); b.i32(ox); b.i32(oy); b.u16(list.count)
        for f in list {
            b.u8(f.cls | (f.poly ? 0x80 : 0))
            b.u16(f.q.count / 2)
            for v in f.q { b.u16(v) }
        }
        return b.b
    }

    /// 导航路线：u16 坐标系编号, i32 起点 x, i32 起点 y（米）, u16 点数, 之后每点 (i16 dx, i16 dy) 相对上一点。
    /// lights：路线上红绿灯的平面坐标（x, y 成对，米），可为 nil。
    /// 路线点后面追加：u16 灯数，每个灯 i32 x, i32 y（米）, u16 所在段号, u8 段内比例（0~255），按路线先后排序。
    static func encodeRoute(frameId: Int, pts: [Double], maxPts: Int, lights: [Double]? = nil) -> Data {
        var tol = 2.0
        var s = simplify(pts, tol)
        while s.count / 2 > maxPts && tol < 50 { tol *= 1.6; s = simplify(pts, tol) }
        var q: [Int] = []
        var lx = 0, ly = 0
        for i in 0..<(s.count / 2) {
            let x = ri(s[2 * i]), y = ri(s[2 * i + 1])
            if !q.isEmpty {
                // 两点相距太远（超过 30 公里）时插入中间点，保证差值放得进 16 位
                var dx = x - lx, dy = y - ly
                while abs(dx) > 30000 || abs(dy) > 30000 {
                    lx += min(30000, max(-30000, dx)); ly += min(30000, max(-30000, dy))
                    q.append(lx); q.append(ly)
                    dx = x - lx; dy = y - ly
                }
                if x == lx && y == ly { continue }
            }
            q.append(x); q.append(y); lx = x; ly = y
        }
        let n = min(q.count / 2, 65535)
        var b = Buf()
        b.u16(frameId)
        if n == 0 { b.i32(0); b.i32(0); b.u16(0); return b.b }
        b.i32(q[0]); b.i32(q[1]); b.u16(n)
        for i in 1..<max(1, n) { b.u16(q[2 * i] - q[2 * i - 2]); b.u16(q[2 * i + 1] - q[2 * i - 1]) }
        // 红绿灯：找到离它最近的路线段，记下段号和段内位置（圆屏用来判断开没开过）
        if let lights = lights, lights.count >= 2, n >= 2 {
            var found: [(x: Double, y: Double, u: Double)] = []
            for li in 0..<(lights.count / 2) {
                let px = lights[2 * li], py = lights[2 * li + 1]
                var best = Double.greatestFiniteMagnitude, bestU = 0.0
                for k in 0..<(n - 1) {
                    let ax = Double(q[2 * k]), ay = Double(q[2 * k + 1])
                    let vx = Double(q[2 * k + 2]) - ax, vy = Double(q[2 * k + 3]) - ay
                    let l2 = vx * vx + vy * vy
                    let t = l2 > 0 ? min(1, max(0, ((px - ax) * vx + (py - ay) * vy) / l2)) : 0
                    let ex = ax + vx * t - px, ey = ay + vy * t - py
                    let d2 = ex * ex + ey * ey
                    if d2 < best { best = d2; bestU = Double(k) + t }
                }
                if best <= 40.0 * 40.0 { found.append((px, py, bestU)) }
            }
            found.sort { $0.u < $1.u }
            let cnt = min(found.count, 600)
            b.u16(cnt)
            for f in found.prefix(cnt) {
                let seg = min(Int(f.u), 65535)
                b.i32(ri(f.x)); b.i32(ri(f.y))
                b.u16(seg); b.u8(min(255, max(0, ri((f.u - Double(seg)) * 255))))
            }
        }
        return b.b
    }

    // MARK: - 高德图片地图
    // 颜色类别（圆屏按同样编号上色）：0 街区，1 道路，2 绿地，3 水面
    static let PX_BLOCK: UInt8 = 0
    static let PX_ROAD: UInt8 = 1
    static let PX_PARK: UInt8 = 2
    static let PX_WATER: UInt8 = 3

    /// 像素颜色统一用 0xRRGGBB
    @inline(__always) private static func lum(_ c: UInt32) -> Int {
        (Int((c >> 16) & 255) * 30 + Int((c >> 8) & 255) * 59 + Int(c & 255) * 11) / 100
    }

    /// 背景亮度：低饱和度像素里出现最多的亮度（就是街区底色）
    static func backgroundLum(_ px: [UInt32]) -> Int {
        var hist = [Int](repeating: 0, count: 256)
        var i = 0
        while i < px.count {
            let c = px[i]
            let r = Int((c >> 16) & 255), g = Int((c >> 8) & 255), b = Int(c & 255)
            let mx = max(r, g, b), mn = min(r, g, b)
            if mx > 0 && (mx - mn) * 100 / mx < 8 { hist[lum(c)] += 1 }
            i += 3
        }
        var best = 240
        for l in 0...255 where hist[l] > hist[best] { best = l }
        return best
    }

    /// 把高德底图的一个像素颜色归类
    static func classifyPixel(_ c: UInt32, _ bgLum: Int) -> UInt8 {
        let r = Int((c >> 16) & 255), g = Int((c >> 8) & 255), b = Int(c & 255)
        let mx = max(r, g, b), mn = min(r, g, b)
        let sat = mx == 0 ? 0.0 : Double(mx - mn) / Double(mx)
        var hue = 0.0
        if mx != mn {
            let d = Double(mx - mn)
            if mx == r { hue = 60.0 * (Double(g - b) / d).truncatingRemainder(dividingBy: 6) }
            else if mx == g { hue = 60.0 * (Double(b - r) / d + 2) }
            else { hue = 60.0 * (Double(r - g) / d + 4) }
            if hue < 0 { hue += 360.0 }
        }
        if sat > 0.12 && hue >= 180 && hue <= 255 { return PX_WATER }
        if sat > 0.10 && hue >= 60 && hue <= 170 { return PX_PARK }
        if sat > 0.18 && hue >= 10 && hue <= 60 { return PX_ROAD }        // 黄色、橙色的主干道
        if lum(c) >= bgLum + 6 { return PX_ROAD }                          // 比底色亮的白色道路
        return PX_BLOCK                                                    // 深色细线（楼块轮廓、图标、描边）不当道路
    }

    /// 清理杂点，让画面干净：
    /// 1. 去掉孤立的小块（小于 minSize 像素的道路 / 绿地 / 水面碎块，多半是图标、楼块、文字残留）；
    /// 2. 3×3 多数投票平滑一遍，去掉锯齿边上的零星像素。
    static func cleanRaster(_ idx: [UInt8], w: Int, h: Int, minRoad: Int = 60, minArea: Int = 40) -> [UInt8] {
        var out = idx
        var seen = [Bool](repeating: false, count: w * h)
        var stack = [Int](repeating: 0, count: w * h)
        var comp = [Int](repeating: 0, count: w * h)
        out.withUnsafeMutableBufferPointer { o in
        seen.withUnsafeMutableBufferPointer { sn in
        stack.withUnsafeMutableBufferPointer { st in
        comp.withUnsafeMutableBufferPointer { cp in
            for start in 0..<(w * h) {
                let c = o[start]
                if c == PX_BLOCK || sn[start] { continue }
                // 找出这一整块（上下左右相连、同一颜色）
                var sp = 0, cn = 0
                st[sp] = start; sp += 1; sn[start] = true
                while sp > 0 {
                    sp -= 1
                    let k = st[sp]
                    cp[cn] = k; cn += 1
                    let x = k % w, y = k / w
                    if x > 0 && !sn[k - 1] && o[k - 1] == c { sn[k - 1] = true; st[sp] = k - 1; sp += 1 }
                    if x < w - 1 && !sn[k + 1] && o[k + 1] == c { sn[k + 1] = true; st[sp] = k + 1; sp += 1 }
                    if y > 0 && !sn[k - w] && o[k - w] == c { sn[k - w] = true; st[sp] = k - w; sp += 1 }
                    if y < h - 1 && !sn[k + w] && o[k + w] == c { sn[k + w] = true; st[sp] = k + w; sp += 1 }
                }
                let limit = c == PX_ROAD ? minRoad : minArea
                if cn < limit { for i in 0..<cn { o[cp[i]] = PX_BLOCK } }
            }
        }}}}
        // 3×3 多数投票
        var sm = out
        out.withUnsafeBufferPointer { o in
        sm.withUnsafeMutableBufferPointer { s in
            var cnt = [Int](repeating: 0, count: 4)
            for y in 1..<max(1, h - 1) {
                for x in 1..<max(1, w - 1) {
                    cnt[0] = 0; cnt[1] = 0; cnt[2] = 0; cnt[3] = 0
                    for dy in -1...1 { let r = (y + dy) * w + x; for dx in -1...1 { cnt[Int(o[r + dx] & 3)] += 1 } }
                    var best = Int(o[y * w + x] & 3)
                    for c in 0..<4 where cnt[c] > cnt[best] { best = c }
                    if cnt[best] >= 5 { s[y * w + x] = UInt8(best) }
                }
            }
        }}
        return sm
    }

    /// 道路加粗一圈（只往街区里扩），细路在圆屏上更清楚
    static func dilateRoads(_ idx: [UInt8], w: Int, h: Int) -> [UInt8] {
        var out = idx
        idx.withUnsafeBufferPointer { i in
        out.withUnsafeMutableBufferPointer { o in
            for y in 1..<max(1, h - 1) {
                let r = y * w
                for x in 1..<max(1, w - 1) {
                    let k = r + x
                    if i[k] != PX_BLOCK { continue }
                    if i[k - 1] == PX_ROAD || i[k + 1] == PX_ROAD || i[k - w] == PX_ROAD || i[k + w] == PX_ROAD { o[k] = PX_ROAD }
                }
            }
        }}
        return out
    }

    /// 图片地图打包：u16 坐标系编号, i32 左上角 x, i32 左上角 y（米）, u16 宽, u16 高, u16 每像素毫米数,
    /// 然后是按行压缩的颜色编号：每字节 低 4 位 = 颜色，高 4 位 n：n < 15 时连续 n+1 个；n = 15 时连续 16 + 下一个字节 个。
    /// pal（卫星图）：16 个 RGB565 颜色；有调色板时“每像素毫米数”最高位置 1，后面跟 32 字节调色板
    static func encodeRaster(frameId: Int, x0: Int, y0: Int, w: Int, h: Int, mmPerPx: Int, idx: [UInt8], pal: [Int]? = nil) -> Data {
        var b = Buf()
        b.b.reserveCapacity(w * h / 6 + 64)
        b.u16(frameId); b.i32(x0); b.i32(y0); b.u16(w); b.u16(h); b.u16(pal != nil ? (mmPerPx | 0x8000) : mmPerPx)
        if let pal = pal { for i in 0..<16 { b.u16(i < pal.count ? pal[i] : 0) } }
        var bytes = [UInt8]()
        bytes.reserveCapacity(w * h / 6)
        idx.withUnsafeBufferPointer { p in
            let n = w * h
            var i = 0
            while i < n {
                let c = p[i] & 15
                var run = 1
                while i + run < n && run < 271 && (p[i + run] & 15) == c { run += 1 }
                if run <= 15 { bytes.append(UInt8((run - 1) << 4) | c) }
                else { bytes.append(0xF0 | c); bytes.append(UInt8(run - 16)) }
                i += run
            }
        }
        b.b.append(contentsOf: bytes)
        return b.b
    }
}
