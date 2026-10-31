import Foundation

/// 版本：跟随 App 版本（见 Config.swift，iOS v0.3 新增；和安卓版 AmapShare.kt 一样）
///
/// 导入高德 App 分享的路线（比如在高德里规划好的摩托车路线）：
/// 1. 从分享文字里找出短链接（surl.amap.com/…），按手机浏览器的方式一路跟着跳转，
///    找到链接里的起点、终点和加密的路线参数 shareParam
/// 2. 用高德手机网页自己用的接口（m.amap.com/service/navigation/…）把这条路线原样还原出来：
///    整条路线的坐标、每条路的名字和长度
/// 3. 从路线上挑十几个“途经点”（每条路的中间，长路多挑几个），交给高德导航 SDK 规划，
///    这样 SDK 算出的路线基本和分享的路线走同一批路，原来的导航功能（转向、车道、摄像头、语音、偏航重算）都能用
///
/// 注意：第 2 步用的是高德网页内部接口，不是公开接口，高德改了就会失效（到时候要跟着改）；仅供个人使用。
/// 这里的网络函数都是同步的，只在后台队列里调用。
enum AmapShare {
    static let UA = "Mozilla/5.0 (Linux; Android 14; Pixel 7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0 Mobile Safari/537.36"
    static let MAX_WAYPOINTS = 15               // 高德 SDK 最多 16 个途经点，留一个余量

    typealias LL = (lat: Double, lng: Double)

    struct Road { let name: String; let length: Int; let pts: [LL] }

    struct Route {
        let startName: String, startLat: Double, startLng: Double
        let endName: String, endLat: Double, endLng: Double
        let pts: [LL]                           // 整条路线（高德坐标）
        let roads: [Road]
        let distance: Int, timeS: Int, lights: Int
        let restored: Bool                      // true = 高德原样还原了分享的路线
        let kind: String                        // 驾车 / 摩托车
        let cum: [Double]                       // 每个点离起点的路程（米）
        var total: Double { cum.last ?? 0 }

        init(startName: String, startLat: Double, startLng: Double, endName: String, endLat: Double, endLng: Double,
             pts: [LL], roads: [Road], distance: Int, timeS: Int, lights: Int, restored: Bool, kind: String) {
            self.startName = startName; self.startLat = startLat; self.startLng = startLng
            self.endName = endName; self.endLat = endLat; self.endLng = endLng
            self.pts = pts; self.roads = roads; self.distance = distance; self.timeS = timeS; self.lights = lights
            self.restored = restored; self.kind = kind
            var c = [Double](repeating: 0, count: pts.count)
            for i in 1..<max(1, pts.count) { c[i] = c[i - 1] + AmapShare.distM(pts[i - 1], pts[i]) }
            cum = c
        }
    }

    struct Fail: LocalizedError {
        let msg: String
        init(_ m: String) { msg = m }
        var errorDescription: String? { msg }
    }

    // MARK: - 正则小工具

    private static func matches(_ pattern: String, _ s: String, _ opts: NSRegularExpression.Options = []) -> [[String]] {
        guard let re = try? NSRegularExpression(pattern: pattern, options: opts) else { return [] }
        let ns = s as NSString
        return re.matches(in: s, range: NSRange(location: 0, length: ns.length)).map { m in
            (0..<m.numberOfRanges).map { i in
                let r = m.range(at: i)
                return r.location == NSNotFound ? "" : ns.substring(with: r)
            }
        }
    }
    private static func first(_ pattern: String, _ s: String, group: Int = 0, _ opts: NSRegularExpression.Options = []) -> String? {
        matches(pattern, s, opts).first.map { $0[group] }
    }

    /// 分享文字里有没有高德链接
    static func findUrl(_ text: String?) -> String? {
        guard let t = text, !t.isEmpty else { return nil }
        return first(#"https?://[A-Za-z0-9.\-]*amap\.com/[^\s，。、"'<>）)]+"#, t)
    }

    /// 读出分享的路线；失败时抛出带中文说明的错误
    static func load(_ text: String,cookies:[HTTPCookie]=[],receiveCookies:(([HTTPCookie])->Void)?=nil) throws -> Route {
        let previous=Thread.current.threadDictionary[ShareHTTPContext.threadKey]
        let context=ShareHTTPContext(cookies:cookies)
        Thread.current.threadDictionary[ShareHTTPContext.threadKey]=context
        defer {
            receiveCookies?(context.snapshot())
            if let previous=previous{Thread.current.threadDictionary[ShareHTTPContext.threadKey]=previous}
            else{Thread.current.threadDictionary.removeObject(forKey:ShareHTTPContext.threadKey)}
        }
        guard let url = findUrl(text) else { throw Fail("没找到高德的分享链接") }
        guard let p = try resolve(url) else {
            throw Fail("链接里没找到路线信息（可能不是路线分享，或者链接已过期）\n最后打开的是：\(lastUrl.prefix(300))")
        }
        if p.mode.contains("ride") { return try loadRide(p) }
        if p.mode.contains("motor") { return try loadMotor(p) }
        if p.mode.isEmpty || p.mode.contains("car") { return try loadCar(p) }
        throw Fail("目前只支持导入“驾车”和“摩托车”路线（这条是 \(p.mode)）")
    }

    private static func json(_ s: String) throws -> [String: Any] {
        guard let o = try? JSONSerialization.jsonObject(with: Data(s.utf8)) as? [String: Any] else { throw Fail("高德返回的数据看不懂") }
        return o
    }
    private static func num(_ v: Any?) -> Double? { (v as? NSNumber)?.doubleValue ?? (v as? String).flatMap(Double.init) }
    private static func int(_ v: Any?) -> Int { Int(num(v) ?? 0) }

    /// 摩托车路线：m.amap.com/service/navigation/motor（数据里有多条备选路线，取分享时选中的那条）
    private static func loadMotor(_ p: Share) throws -> Route {
        let q = "https://m.amap.com/service/navigation/motor?shareParam=\(enc(p.share))&uuid=\(UUID().uuidString)"
        let j = try json(try get(q).body)
        if int(j["code"]) != 1 { throw Fail("高德没有返回路线（\(int(j["code"]))）") }
        guard let data = j["data"] as? [String: Any], let paths = data["paths"] as? [[String: Any]], !paths.isEmpty else {
            throw Fail("高德返回的数据里没有路线")
        }
        let pl = paths[(0..<paths.count).contains(p.pathIdx) ? p.pathIdx : 0]
        var all: [LL] = []
        for c in pl["coords"] as? [[String: Any]] ?? [] {
            if let la = num(c["lat"]), let lo = num(c["lon"]) { all.append((la, lo)) }
        }
        if all.count < 2 { throw Fail("路线是空的") }
        // 每条路（group）= 连续几个 segment；segment 里的 link 记着在 coords 里的起止下标
        let segs = pl["segments"] as? [[String: Any]] ?? []
        func segRange(_ si: Int) -> (Int, Int)? {
            guard si >= 0, si < segs.count, let links = segs[si]["links"] as? [[String: Any]], !links.isEmpty else { return nil }
            let b = Int(num(links[0]["begin_index"]) ?? -1), e = Int(num(links[links.count - 1]["end_index"]) ?? -1)
            return b >= 0 && e >= b ? (b, e) : nil
        }
        var roads: [Road] = []
        for g in pl["group"] as? [[String: Any]] ?? [] {
            let s0 = Int(num(g["start_id"]) ?? -1), n = Int(num(g["count"]) ?? 1)
            guard let a = segRange(s0) else { continue }
            let b = segRange(s0 + n - 1) ?? a
            let from = min(all.count - 1, max(0, a.0)), to = min(all.count - 1, max(from, b.1))
            roads.append(Road(name: g["name"] as? String ?? "", length: int(g["dist"]), pts: Array(all[from...to])))
        }
        return Route(startName: p.sName, startLat: p.sLat, startLng: p.sLng, endName: p.eName, endLat: p.eLat, endLng: p.eLng,
                     pts: dedup(all), roads: roads, distance: int(pl["distance"]), timeS: int(pl["travel_time"]),
                     lights: int(pl["traffic_light_count"]), restored: (j["isRestoreRoute"] as? Bool) ?? false, kind: "摩托车")
    }

    /// 驾车路线：m.amap.com/service/navigation/driving.json
    private static func loadRide(_ p: Share) throws -> Route {
        guard p.rideType != "elebike" else { throw Fail("不支持电瓶车分享，请选择自行车") }
        let q = "https://m.amap.com/service/navigation/ride.json?origin=\(p.sLng),\(p.sLat)&destination=\(p.eLng),\(p.eLat)&rideType=bike&shareParam=\(enc(p.share))&uuid=\(UUID().uuidString)"
        let j = try json(try get(q).body)
        guard int(j["code"]) == 1, let data = j["data"] as? [String: Any], let paths = data["paths"] as? [[String: Any]], !paths.isEmpty else { throw Fail("高德未返回自行车路线") }
        let path = paths[paths.indices.contains(p.pathIdx) ? p.pathIdx : 0]
        var roads: [Road] = [], all: [LL] = []
        for step in path["steps"] as? [[String: Any]] ?? [] {
            var points: [LL] = []
            for pair in (step["polyline"] as? String ?? "").split(separator: ";") {
                let xy = pair.split(separator: ",")
                guard xy.count == 2, let lo = Double(xy[0]), let la = Double(xy[1]), la.isFinite, lo.isFinite, abs(la)<=90, abs(lo)<=180 else { throw Fail("自行车路线坐标无效") }
                points.append((la,lo))
            }
            guard all.count + points.count <= 150000 else { throw Fail("路线过长") }
            all += points
            roads.append(Road(name: step["instruction"] as? String ?? step["road"] as? String ?? "", length: int(step["distance"]), pts: points))
        }
        let points = dedup(all); guard points.count >= 2 else { throw Fail("自行车路线没有有效坐标") }
        return Route(startName:p.sName,startLat:p.sLat,startLng:p.sLng,endName:p.eName,endLat:p.eLat,endLng:p.eLng,pts:points,roads:roads,distance:int(path["distance"]),timeS:int(path["duration"]),lights:0,restored:(j["isRestoreRoute"] as? Bool) ?? false,kind:"自行车")
    }

    private static func loadCar(_ p: Share) throws -> Route {
        let q = "https://m.amap.com/service/navigation/driving.json?origin=\(p.sLng),\(p.sLat)&originid=&origintype=" +
            "&destination=\(p.eLng),\(p.eLat)&destinationid=&destinationtype=&waypoints=&strategy=10" +
            "&shareParam=\(enc(p.share))&uuid=\(UUID().uuidString)"
        let j = try json(try get(q).body)
        if int(j["code"]) != 1 { throw Fail("高德没有返回路线（\(int(j["code"]))）") }
        guard let pl = (j["path_list"] as? [[String: Any]])?.first else { throw Fail("高德返回的数据里没有路线") }
        var roads: [Road] = []
        for r in pl["path"] as? [[String: Any]] ?? [] {
            var rp: [LL] = []
            for s in r["segments"] as? [[String: Any]] ?? [] {
                for c in s["coor"] as? [[Any]] ?? [] where c.count >= 2 {
                    if let lo = num(c[0]), let la = num(c[1]) { rp.append((la, lo)) }
                }
            }
            roads.append(Road(name: r["name"] as? String ?? "", length: int(r["length"]), pts: rp))
        }
        var all: [LL] = []
        for c in pl["oneRoute"] as? [[Any]] ?? [] where c.count >= 2 {
            if let lo = num(c[0]), let la = num(c[1]) { all.append((la, lo)) }
        }
        if all.isEmpty { all = roads.flatMap { $0.pts } }
        if all.count < 2 { throw Fail("路线是空的") }
        let lights = pl["traffic_lights"] != nil ? int(pl["traffic_lights"]) : int(pl["trafficlights"])
        return Route(startName: p.sName, startLat: p.sLat, startLng: p.sLng, endName: p.eName, endLat: p.eLat, endLng: p.eLng,
                     pts: dedup(all), roads: roads, distance: int(pl["distance"]), timeS: int(pl["drivetime"]),
                     lights: lights, restored: (j["isRestoreRoute"] as? Bool) ?? false, kind: "驾车")
    }

    // MARK: - 解析分享链接

    /// mode = 网址里的 navigation/xxx（carmap 驾车、motormap 摩托车……），pathIdx = 分享时选中的第几条路线
    private struct Share { let sLat, sLng: Double; let sName: String; let eLat, eLng: Double; let eName: String; let share: String; let mode: String; let pathIdx: Int; var rideType: String = "bike" }

    private static var lastUrl = ""                    // 调试用：最后打开的网址

    /// 跟着跳转一路找，直到某个网址或网页内容里出现路线信息
    private static func resolve(_ start: String) throws -> Share? {
        var url = start
        var seen = Set<String>()
        for _ in 0..<10 {
            if !seen.insert(url).inserted { return nil }
            lastUrl = url
            if let s = parse(url) { return s }
            let (loc, body) = try get(url, follow: false)
            if let loc = loc {
                url = URL(string: loc, relativeTo: URL(string: url))?.absoluteString ?? loc
                continue
            }
            if let s = parse(body) { return s }
            // 网页用脚本跳转：找里面的高德网址
            guard let next = first(#"(?:https?:)?//m\.amap\.com/navigation/[^"'\s<>\\]+"#, body)
                    ?? first(#"location(?:\.href)?\s*=\s*["']([^"']+)["']"#, body, group: 1)
                    ?? first(#"http-equiv=["']refresh["'][^>]*url=([^"'>\s]+)"#, body, group: 1, .caseInsensitive)
            else { return nil }
            let n = next.hasPrefix("//") ? "https:" + next : next
            url = URL(string: n, relativeTo: URL(string: url))?.absoluteString ?? n
        }
        return nil
    }

    /// 从一段文字（网址或网页）里找路线信息（可能被编码了好几层，一层层解开找）：
    /// 1. 优先找 navigation/<类型>/__r=起点纬度,经度,名字,终点纬度,经度,名字,?,类型,第几条,,,,加密参数,...
    ///    也认 callAPP 跳转页里的 action=shorturl&r=…（格式一样，类型 11 = 摩托车）
    /// 2. 再找 shareParam=…&saddr=…&daddr=…
    private static func parse(_ raw: String) -> Share? {
        var s = raw
        for _ in 0..<5 {
            // Recent bicycle shares can contain only an encoded native route URI.
            let nested = first(#"[?&]schema=([^&"'\s<>]+)"#, s, group: 1).map(fullDecode)
            let native = nested ?? s
            if native.hasPrefix("amapuri://route/plan?") || native.hasPrefix("androidamap://route?") {
                var q: [String:String] = [:]
                for item in URLComponents(string: native)?.queryItems ?? [] { q[item.name] = item.value }
                guard q["t"] == "3", q["rideType"] != "elebike", q["dev"] == nil || q["dev"] == "0",
                      (Int(q["vian"] ?? "0") ?? 0) == 0,
                      let slat = q["slat"].flatMap(Double.init), let slng = q["slon"].flatMap(Double.init),
                      let dlat = q["dlat"].flatMap(Double.init), let dlng = q["dlon"].flatMap(Double.init),
                      slat.isFinite, slng.isFinite, dlat.isFinite, dlng.isFinite,
                      abs(slat)<=90, abs(dlat)<=90, abs(slng)<=180, abs(dlng)<=180 else { return nil }
                return Share(sLat:slat,sLng:slng,sName:q["sname"] ?? "分享起点",eLat:dlat,eLng:dlng,eName:q["dname"] ?? "分享目的地",share:"",mode:"ridemap",pathIdx:0)
            }
            for m in matches(#"(?:__r|[?&]r)=([^&"'\s<>]+)"#, s) {
                let f = m[1].components(separatedBy: ",")
                if f.count < 13 { continue }
                guard let sLat = Double(fullDecode(f[0])), let sLng = Double(fullDecode(f[1])),
                      let eLat = Double(fullDecode(f[3])), let eLng = Double(fullDecode(f[4])) else { continue }
                let share = fullDecode(f[12])
                if share.count <= 10 { continue }
                // 路线类型：网址里的 navigation/xxx；没有就看第 8 项（11 = 摩托车）或 t= 参数
                let mode = first(#"navigation/([A-Za-z]+)/"#, s, group: 1)?.lowercased()
                    ?? ((fullDecode(f[7]) == "3") ? "ridemap" : (fullDecode(f[7]) == "11" || first(#"[?&]t=11(?:&|$)"#, s) != nil) ? "motormap" : "carmap")
                return Share(sLat: sLat, sLng: sLng, sName: fullDecode(f[2]), eLat: eLat, eLng: eLng, eName: fullDecode(f[5]),
                             share: share, mode: mode, pathIdx: Int(fullDecode(f[8])) ?? 0, rideType: s.contains("elebike") ? "elebike" : "bike")
            }
            if let sp = first(#"shareParam=([^&"'\s,<>]+)"#, s, group: 1) {
                let share = fullDecode(sp)
                let a = first(#"saddr=([^&"'\s<>]+)"#, s, group: 1).flatMap { lngLatName(fullDecode($0)) }
                let b = first(#"daddr=([^&"'\s<>]+)"#, s, group: 1).flatMap { lngLatName(fullDecode($0)) }
                let mode = first(#"navigation/([A-Za-z]+)/"#, s, group: 1)?.lowercased() ?? ""
                if let a = a, let b = b {
                    return Share(sLat: a.0, sLng: a.1, sName: a.2, eLat: b.0, eLng: b.1, eName: b.2, share: share, mode: mode, pathIdx: 0, rideType: s.contains("elebike") ? "elebike" : "bike")
                }
            }
            let d = s.removingPercentEncoding ?? s
            if d == s { break }
            s = d
        }
        return nil
    }

    /// "经度,纬度,名字" → (纬度, 经度, 名字)
    private static func lngLatName(_ s: String) -> (Double, Double, String)? {
        let p = s.components(separatedBy: ",")
        guard p.count >= 2, let lng = Double(p[0]), let lat = Double(p[1]) else { return nil }
        return (lat, lng, p.dropFirst(2).joined(separator: ","))
    }

    private static func fullDecode(_ s: String) -> String {
        var t = s
        for _ in 0..<4 { let d = t.removingPercentEncoding ?? t; if d == t { return t }; t = d }
        return t
    }

    private static func enc(_ s: String) -> String {
        var ok = CharacterSet.alphanumerics
        ok.insert(charactersIn: "-._*")
        return s.addingPercentEncoding(withAllowedCharacters: ok) ?? s
    }

    /// 不自动跟随跳转（要自己一步步看跳到了哪里）
    /// Isolated cookie jar for this import; redirects cannot send an AMap login to another domain.
    private static func get(_ url: String, follow: Bool = true) throws -> (loc: String?, body: String) {
        let u=url.hasPrefix("http://") ? "https://"+url.dropFirst(7):url
        guard let target=URL(string:u),ShareCookiePolicy.allowed(target) else{throw Fail("分享地址不是有效的高德 HTTPS 页面")}
        let context=(Thread.current.threadDictionary[ShareHTTPContext.threadKey] as? ShareHTTPContext) ?? ShareHTTPContext(cookies:[])
        var req=URLRequest(url:target,timeoutInterval:20)
        req.setValue(UA,forHTTPHeaderField:"User-Agent");req.setValue("zh-CN,zh;q=0.9",forHTTPHeaderField:"Accept-Language")
        req.setValue("https://www.amap.com/",forHTTPHeaderField:"Referer")
        req.setValue(ShareCookiePolicy.header(context.snapshot(),url:target),forHTTPHeaderField:"Cookie")
        let configuration=URLSessionConfiguration.ephemeral
        configuration.httpCookieStorage=nil;configuration.httpShouldSetCookies=false
        let delegate=ShareRedirectDelegate(follow:follow,context:context)
        let session=URLSession(configuration:configuration,delegate:delegate,delegateQueue:nil)
        defer{session.finishTasksAndInvalidate()}
        let sem=DispatchSemaphore(value:0)
        var data:Data?,response:HTTPURLResponse?,failure:Error?
        let task=session.dataTask(with:req){bytes,resp,error in
            data=bytes;response=resp as? HTTPURLResponse;failure=error;sem.signal()
        }
        task.resume()
        if sem.wait(timeout:.now()+25) == .timedOut{task.cancel();throw Fail("读取分享路线超时，请检查网络后重试")}
        if let failure=failure{throw Fail("网络请求失败：\(failure.localizedDescription)")}
        guard let resp=response else{throw Fail("高德没有返回有效响应")}
        if let responseURL=resp.url{context.store(response:resp,url:responseURL)}
        if !follow,(300...399).contains(resp.statusCode){return(resp.value(forHTTPHeaderField:"Location"),"")}
        if [401,403].contains(resp.statusCode){throw Fail("高德要求网页登录或验证，请点“网页验证后重试”")}
        guard resp.statusCode<400 else{throw Fail("网络请求失败（\(resp.statusCode)）")}
        guard let bytes=data,bytes.count<=16*1024*1024 else{throw Fail("分享路线数据过大或为空")}
        let body=String(data:bytes,encoding:.utf8) ?? ""
        if body.contains("安全验证") || body.contains("滑动验证") || body.contains("访问验证"){throw Fail("高德要求网页验证，请点“网页验证后重试”")}
        return(nil,body)
    }

    private static func dedup(_ l: [LL]) -> [LL] {
        var out: [LL] = []
        out.reserveCapacity(l.count)
        for p in l {
            if let q = out.last, abs(q.lat - p.lat) <= 1e-7, abs(q.lng - p.lng) <= 1e-7 { continue }
            out.append(p)
        }
        return out
    }

    // MARK: - 挑途经点

    /// 从当前位置往后，在每条路的中间放一个途经点（长路每 7 公里多放一个）；离起点、终点太近的不要；
    /// 超过 15 个时，去掉和前后挨得最近的那些，让剩下的分布均匀
    static func waypoints(_ r: Route, myLat: Double, myLng: Double) -> [LL] {
        waypointsAfter(r, from: myLat.isNaN ? 0 : progressOf(r, myLat, myLng))
    }

    /// 只要路程 from 米以后的途经点（偏航后重新规划用）
    static func waypointsAfter(_ r: Route, from: Double) -> [LL] {
        let end = r.total
        var list: [(d: Double, p: LL)] = []
        var base = 0.0                                       // 这条路起点在整条路线上的路程
        for road in r.roads {
            var rc = [Double](repeating: 0, count: road.pts.count)
            for i in 1..<max(1, road.pts.count) { rc[i] = rc[i - 1] + distM(road.pts[i - 1], road.pts[i]) }
            let len = rc.last ?? 0
            if len >= 100 {
                let n = 1 + Int(len / 7000)
                for k in 0..<n {
                    let at = len * (Double(k) + 0.5) / Double(n)
                    let g = base + at
                    if g > from + 250 && g < end - 250 { list.append((g, pointAt(road.pts, rc, at))) }
                }
            }
            base += len
        }
        list.sort { $0.d < $1.d }
        while list.count > MAX_WAYPOINTS {
            var bi = 0, bv = Double.greatestFiniteMagnitude
            for i in list.indices {
                let prev = i > 0 ? list[i - 1].d : from
                let next = i < list.count - 1 ? list[i + 1].d : end
                if next - prev < bv { bv = next - prev; bi = i }
            }
            list.remove(at: bi)
        }
        return list.map { $0.p }
    }

    /// 当前位置到路线上每一小段的最近点：调用 body(距离, 走到的路程)
    private static func scan(_ r: Route, _ lat: Double, _ lng: Double, skipFar: Bool = false, _ body: (Double, Double, Int) -> Void) {
        if r.pts.count < 2 { return }
        let kx = cos(lat * .pi / 180) * 111_320.0, ky = 110_540.0
        for i in 0..<(r.pts.count - 1) {
            let ax = (r.pts[i].lng - lng) * kx, ay = (r.pts[i].lat - lat) * ky
            if skipFar && (abs(ax) > 3000 || abs(ay) > 3000) { continue }       // 粗筛：远的不算
            let bx = (r.pts[i + 1].lng - lng) * kx, by = (r.pts[i + 1].lat - lat) * ky
            let dx = bx - ax, dy = by - ay
            let l2 = dx * dx + dy * dy
            let t = l2 > 0 ? min(1, max(0, (-ax * dx - ay * dy) / l2)) : 0
            let px = ax + t * dx, py = ay + t * dy
            body((px * px + py * py).squareRoot(), r.cum[i] + t * l2.squareRoot(), i)
        }
    }

    /// 偏航后找“回到原路线的点”：在已经走到的路程 minD 之后，离当前位置最近的那一点（不管多远），返回它的路程（米）
    static func progressAhead(_ r: Route, _ lat: Double, _ lng: Double, minD: Double) -> Double {
        var best = Double.greatestFiniteMagnitude, bd = max(0, minD)
        scan(r, lat, lng) { d, at, i in
            if r.cum[i + 1] < minD { return }
            if d < best && at >= minD { best = d; bd = at }
        }
        return bd
    }

    /// 当前位置在原路线上（60 米以内）时返回走到的路程，否则 nil
    static func progressNear(_ r: Route, _ lat: Double, _ lng: Double, near: Double = 60) -> Double? {
        var best = Double.greatestFiniteMagnitude, bd = 0.0
        scan(r, lat, lng, skipFar: true) { d, at, _ in if d < best { best = d; bd = at } }
        return best <= near ? bd : nil
    }

    /// 当前位置在路线上走到了哪里（米）；离路线超过 300 米就当作还没上路线（0）
    static func progressOf(_ r: Route, _ lat: Double, _ lng: Double) -> Double {
        var best = Double.greatestFiniteMagnitude, bd = 0.0
        scan(r, lat, lng) { d, at, _ in if d < best { best = d; bd = at } }
        return best <= 300 ? bd : 0
    }

    /// 高德 SDK 算出来的路线和分享的路线有多吻合：分享路线（从当前位置往后）每隔 40 米取一个点，
    /// 看离 SDK 路线是否在 45 米以内。返回 (吻合比例 0~1, 不吻合的长度 米)
    static func match(_ r: Route, sdk: [LL], myLat: Double, myLng: Double) -> (ratio: Double, bad: Int) {
        if sdk.count < 2 || r.pts.count < 2 { return (0, Int(r.total)) }
        let from = myLat.isNaN ? 0 : progressOf(r, myLat, myLng)
        let lat0 = r.pts[0].lat
        let kx = cos(lat0 * .pi / 180) * 111_320.0, ky = 110_540.0
        let sx = sdk.map { $0.lng * kx }, sy = sdk.map { $0.lat * ky }
        var ok = 0, all = 0
        var d = from + 20
        var seg = 0
        while d < r.total - 20 {
            while seg < r.cum.count - 2 && r.cum[seg + 1] < d { seg += 1 }
            let p = pointAt(r.pts, r.cum, d, hint: seg)
            let x = p.lng * kx, y = p.lat * ky
            var near = false
            for i in 0..<(sdk.count - 1) {
                if min(sx[i], sx[i + 1]) - 45 > x || max(sx[i], sx[i + 1]) + 45 < x ||
                    min(sy[i], sy[i + 1]) - 45 > y || max(sy[i], sy[i + 1]) + 45 < y { continue }
                let dx = sx[i + 1] - sx[i], dy = sy[i + 1] - sy[i]
                let l2 = dx * dx + dy * dy
                let t = l2 > 0 ? min(1, max(0, ((x - sx[i]) * dx + (y - sy[i]) * dy) / l2)) : 0
                let ex = sx[i] + t * dx - x, ey = sy[i] + t * dy - y
                if ex * ex + ey * ey <= 45 * 45 { near = true; break }
            }
            if near { ok += 1 }
            all += 1
            d += 40
        }
        if all == 0 { return (1, 0) }
        return (Double(ok) / Double(all), (all - ok) * 40)
    }

    /// 折线上路程为 d 的点
    private static func pointAt(_ pts: [LL], _ cum: [Double], _ d: Double, hint: Int = 0) -> LL {
        if pts.isEmpty { return (0, 0) }
        if pts.count == 1 { return pts[0] }
        var i = min(max(0, hint), pts.count - 2)
        while i < pts.count - 2 && cum[i + 1] < d { i += 1 }
        let seg = cum[i + 1] - cum[i]
        let t = seg > 0 ? min(1, max(0, (d - cum[i]) / seg)) : 0
        return (pts[i].lat + (pts[i + 1].lat - pts[i].lat) * t, pts[i].lng + (pts[i + 1].lng - pts[i].lng) * t)
    }

    static func distM(_ a: LL, _ b: LL) -> Double { NaviBridge.distM(a.lat, a.lng, b.lat, b.lng) }
}

/// 按导入的分享路线导航时的“跟随”：
/// - 记住在原路线上已经走到哪里（只在原路线 60 米以内时更新，最多往前跳 3 公里，防止路线自己交叉时跳错）
/// - 高德判定偏航并自动重算后，马上自己再算一次：从当前位置出发，只用“回到原路线的点”之后的途经点，
///   这样不会被拽回去补走已经跳过的途经点，而是就近回到原路线继续走
/// - 原路线发给圆屏画成细线（MapFeeder.setGhost）
/// 只在主线程里用。
/// Cookie scope is checked for every redirect, including path, expiry and secure transport.
enum ShareCookiePolicy {
    static func allowed(_ url:URL)->Bool {
        guard url.scheme?.lowercased()=="https",url.user==nil,url.password==nil,let host=url.host?.lowercased() else{return false}
        return host=="amap.com" || host.hasSuffix(".amap.com")
    }
    static func header(_ cookies:[HTTPCookie],url:URL)->String? {
        guard allowed(url),let host=url.host?.lowercased() else{return nil}
        let path=url.path.isEmpty ? "/":url.path
        let usable=cookies.filter{cookie in
            let domain=cookie.domain.lowercased().trimmingCharacters(in:CharacterSet(charactersIn:"."))
            let domainOK=host==domain || (cookie.domain.hasPrefix(".") && host.hasSuffix("."+domain))
            let base=cookie.path.isEmpty ? "/":cookie.path
            let pathOK=path==base || (path.hasPrefix(base) && (base.hasSuffix("/") || path.dropFirst(base.count).hasPrefix("/")))
            return domainOK && pathOK && (cookie.expiresDate == nil || cookie.expiresDate!>Date())
        }.sorted{$0.path.count>$1.path.count}
        guard !usable.isEmpty else{return nil}
        return usable.map{"\($0.name)=\($0.value)"}.joined(separator:"; ")
    }
}

private final class ShareHTTPContext:NSObject {
    static let threadKey="motonav.share.http.context"
    private var cookies:[HTTPCookie]
    private let lock=NSLock()
    init(cookies:[HTTPCookie]){self.cookies=cookies}
    func snapshot()->[HTTPCookie]{lock.lock();defer{lock.unlock()};return cookies}
    func store(response:HTTPURLResponse,url:URL){
        guard ShareCookiePolicy.allowed(url),let host=url.host else{return}
        let fields=response.allHeaderFields.reduce(into:[String:String]()){$0[String(describing:$1.key)]=String(describing:$1.value)}
        let incoming=HTTPCookie.cookies(withResponseHeaderFields:fields,for:url).filter{cookie in
            let domain=cookie.domain.trimmingCharacters(in:CharacterSet(charactersIn:"."))
            return host==domain || (cookie.domain.hasPrefix(".") && host.hasSuffix("."+domain))
        }
        lock.lock();defer{lock.unlock()}
        for cookie in incoming{cookies.removeAll{$0.name==cookie.name && $0.domain==cookie.domain && $0.path==cookie.path};cookies.append(cookie)}
    }
}

private final class ShareRedirectDelegate:NSObject,URLSessionTaskDelegate {
    let follow:Bool;let context:ShareHTTPContext
    init(follow:Bool,context:ShareHTTPContext){self.follow=follow;self.context=context}
    func urlSession(_ session:URLSession,task:URLSessionTask,willPerformHTTPRedirection response:HTTPURLResponse,newRequest request:URLRequest,completionHandler:@escaping(URLRequest?)->Void){
        if let url=response.url{context.store(response:response,url:url)}
        guard follow,let url=request.url,ShareCookiePolicy.allowed(url) else{completionHandler(nil);return}
        var next=request;next.setValue(ShareCookiePolicy.header(context.snapshot(),url:url),forHTTPHeaderField:"Cookie")
        completionHandler(next)
    }
}

final class ShareGuide {
    static let shared = ShareGuide()
    private(set) var route: AmapShare.Route?
    private var emulate = false
    private var avoidHw = true
    private var maxProg = 0.0
    private var lat = Double.nan, lng = Double.nan
    private var yawAt: Date?                        // 高德开始偏航重算的时间
    private var ourCalc = false                     // 我们自己发起的重算还没回来
    private var lastOurAt = Date.distantPast

    /// 开始导航时调用；r = nil 表示这次不是按分享路线导航
    func start(_ r: AmapShare.Route?, avoidHighway: Bool, emulate: Bool, myLat: Double, myLng: Double) {
        route = r
        avoidHw = avoidHighway
        self.emulate = emulate
        lat = myLat; lng = myLng
        maxProg = (r != nil && !myLat.isNaN) ? AmapShare.progressOf(r!, myLat, myLng) : 0
        yawAt = nil; ourCalc = false; lastOurAt = .distantPast
        MapFeeder.shared.setGhost(r?.pts)
    }

    func stop() {
        if route == nil { return }
        route = nil
        yawAt = nil; ourCalc = false
        MapFeeder.shared.setGhost(nil)
    }

    /// 每次定位
    func onLocation(_ la: Double, _ lo: Double) {
        lat = la; lng = lo
        guard let r = route, let p = AmapShare.progressNear(r, la, lo) else { return }
        if p > maxProg && p < maxProg + 3000 { maxProg = p }
    }

    /// 高德判定偏航，开始自己重算
    func onYaw() {
        if route == nil { return }
        yawAt = Date()
        NaviBridge.shared.extraInfo = "偏航了：等高德重算完，再按分享的路线重新规划"
    }

    /// 每次算路结束：成功 / 失败
    func onCalcDone(ok: Bool) {
        guard let r = route else { return }
        if ourCalc {
            ourCalc = false
            let m = AMapNaviDriveManager.sharedInstance()
            if ok {
                // 导航中算路成功：如果导航停了就用新路线接着导航
                if m.naviMode.rawValue == 0 { _ = emulate ? m.startEmulatorNavi() : m.startGPSNavi() }
                NaviBridge.shared.extraInfo = String(format: "已按分享的路线重新规划（从原路线 %.1f 公里处往后）", maxProg / 1000)
            } else {
                NaviBridge.shared.extraInfo = "按分享的路线重新规划失败，先沿用高德的路线"
            }
            return
        }
        if let y = yawAt, Date().timeIntervalSince(y) < 30 {
            yawAt = nil
            reroute(r)
        }
    }

    private func reroute(_ r: AmapShare.Route) {
        if lat.isNaN || Date().timeIntervalSince(lastOurAt) < 8 { return }
        let from = AmapShare.progressAhead(r, lat, lng, minD: maxProg - 50)
        let wps = AmapShare.waypointsAfter(r, from: from).compactMap { AMapNaviPoint.location(withLatitude: CGFloat($0.lat), longitude: CGFloat($0.lng)) }
        guard let s = AMapNaviPoint.location(withLatitude: CGFloat(lat), longitude: CGFloat(lng)),
              let e = AMapNaviPoint.location(withLatitude: CGFloat(r.endLat), longitude: CGFloat(r.endLng)) else { return }
        lastOurAt = Date()
        let strategy = ConvertDrivingPreferenceToDrivingStrategy(false, false, avoidHw, false, false)
        ourCalc = AMapNaviDriveManager.sharedInstance().calculateDriveRoute(withStart: [s], end: [e], wayPoints: wps, drivingStrategy: strategy)
        NaviBridge.shared.extraInfo = ourCalc ? "偏航：按分享的路线重新规划（\(wps.count) 个途经点）…" : "偏航：按分享的路线重新规划没能开始"
        if ourCalc { NaviBridge.shared.setRerouting(true) }       // 圆屏继续显示“重新规划中”
    }
}
