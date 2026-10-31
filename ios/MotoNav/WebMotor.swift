import SwiftUI
import WebKit
import CoreLocation
import ObjectiveC

struct MotorPoint: Codable { let lat: Double; let lng: Double; var name = ""; var id = "" }
struct MotorRequest: Codable {
    var start: MotorPoint; let end: MotorPoint; var vias: [MotorPoint] = []; var bicycle = false
    var page: URL {
        var c = URLComponents(string: "https://www.amap.com/ssr/dir")!
        c.queryItems = [URLQueryItem(name: "type", value: "4"), .init(name: "fname", value: start.name), .init(name: "flat", value: "\(start.lat)"), .init(name: "flon", value: "\(start.lng)"), .init(name: "dname", value: end.name), .init(name: "dlat", value: "\(end.lat)"), .init(name: "dlon", value: "\(end.lng)"), .init(name: "did", value: end.id)]
        for (key, values) in [("vlat", vias.map { "\($0.lat)" }), ("vlon", vias.map { "\($0.lng)" }), ("vname", vias.map { $0.name })] where !values.isEmpty { c.queryItems?.append(.init(name: key, value: values.joined(separator: "|"))) }
        return c.url!
    }
    static func parse(_ url: URL) throws -> MotorRequest {
        guard url.scheme == "https", url.host == "www.amap.com", url.path == "/ssr/dir", let c = URLComponents(url: url, resolvingAgainstBaseURL: false) else { throw AmapShare.Fail("请使用高德摩托车路线网页") }
        var q: [String: String] = [:]; for item in c.queryItems ?? [] { q[item.name] = item.value }
        guard q["type"] == "4" else { throw AmapShare.Fail("请在网页选择摩托车") }
        func point(_ prefix: String, _ long: String) throws -> MotorPoint {
            let xy = q["\(long)[lnglat]"]?.split(separator: ",")
            let lat = (xy?.count == 2 ? Double(xy![1]) : nil) ?? q["\(prefix)lat"].flatMap(Double.init)
            let lng = (xy?.count == 2 ? Double(xy![0]) : nil) ?? q["\(prefix)lon"].flatMap(Double.init)
            guard let la = lat, let lo = lng, valid(la, lo) else { throw AmapShare.Fail("网页缺少有效起终点坐标") }
            return MotorPoint(lat: la, lng: lo, name: q["\(long)[name]"] ?? q["\(prefix)name"] ?? "", id: q["\(long)[id]"] ?? q["\(prefix)id"] ?? "")
        }
        var vias: [MotorPoint] = []; var bicycle = false
        let lat = (q["vlat"] ?? "").split(separator: "|"), lng = (q["vlon"] ?? "").split(separator: "|")
        guard lat.count == lng.count, lat.count <= 16 else { throw AmapShare.Fail("途经点格式错误") }
        if !lat.isEmpty {
            for i in lat.indices { guard let la = Double(lat[i]), let lo = Double(lng[i]), valid(la,lo) else { throw AmapShare.Fail("途经点坐标错误") }; vias.append(MotorPoint(lat: la,lng: lo)) }
        } else {
            for i in 0..<16 {
                guard let text = q["via[\(i)][lnglat]"] else { break }; let xy = text.split(separator: ",")
                guard xy.count == 2, let lo = Double(xy[0]), let la = Double(xy[1]), valid(la,lo) else { throw AmapShare.Fail("途经点坐标错误") }
                vias.append(MotorPoint(lat: la, lng: lo, name: q["via[\(i)][name]"] ?? ""))
            }
            guard q["via[16][lnglat]"] == nil, q["vid"] == nil || !vias.isEmpty else { throw AmapShare.Fail("途经点缺少坐标或数量过多") }
        }
        return try MotorRequest(start: point("f", "from"), end: point("d", "to"), vias: vias)
    }
    static func valid(_ lat: Double, _ lng: Double) -> Bool { lat.isFinite && lng.isFinite && abs(lat) <= 90 && abs(lng) <= 180 }
}
struct MotorStep { let instruction: String; let road: String; let action: String; let end: Int
    var start = 0; var distance = 0; var duration = 0
    var dir: String { action.contains("掉头") ? "U" : action.contains("左前") || action.contains("靠左") ? "SL" : action.contains("右前") || action.contains("靠右") ? "SR" : action.contains("左") ? "L" : action.contains("右") ? "R" : "S" }
}
struct MotorRoute: Identifiable {
    let id = UUID(); let request: MotorRequest; let points: [MotorPoint]; let steps: [MotorStep]; let seconds: Int
    private let cumulative: [Double]
    private let coordinates: [(lat:Double,lng:Double)]
    init(request: MotorRequest, points: [MotorPoint], steps: [MotorStep], seconds: Int) {
        self.request=request;self.points=points;self.steps=steps;self.seconds=seconds
        cumulative=RouteShape(points:points).cum
        coordinates=points.map{($0.lat,$0.lng)}
    }
    var cum: [Double] { cumulative }
    var totalMeters:Int {let supplied=steps.reduce(0){$0+max(0,$1.distance)};return supplied>0 ? supplied : Int(cumulative.last ?? 0)}
    var segmentRoads: [String] {
        guard points.count>1 else { return [] }
        var roads=[String](repeating:"",count:points.count-1)
        for step in steps where step.start<step.end {
            let lower=max(0,min(roads.count,step.start)),upper=max(lower,min(roads.count,step.end))
            for i in lower..<upper { roads[i]=step.road }
        }
        return roads
    }
    var coords: [(lat: Double,lng: Double)] { coordinates }
}
enum MotorArrival {
    static func confirmed(remaining: Int, offset: Double, endpointDistance: Double) -> Bool {
        remaining <= 15 && offset <= 35 && endpointDistance <= 35
    }
}
enum MotorRoutes {
    static let ua = "Mozilla/5.0 (Linux; Android 14) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/130.0.0.0 Safari/537.36"
    private final class NoRedirects: NSObject,URLSessionTaskDelegate {
        func urlSession(_ session:URLSession,task:URLSessionTask,willPerformHTTPRedirection response:HTTPURLResponse,newRequest request:URLRequest,completionHandler:@escaping(URLRequest?)->Void) {completionHandler(nil)}
    }
    static func fromShare(_ r: AmapShare.Route) -> MotorRoute {
        let points = r.pts.map { MotorPoint(lat: $0.lat,lng: $0.lng) }
        let request = MotorRequest(start: MotorPoint(lat:r.startLat,lng:r.startLng,name:r.startName),end:MotorPoint(lat:r.endLat,lng:r.endLng,name:r.endName),bicycle:r.kind == "自行车")
        var cursor = 0
        let steps: [MotorStep] = r.roads.map { road in
            if let end = road.pts.last {
                cursor = (cursor..<points.count).min { NaviBridge.distM(points[$0].lat,points[$0].lng,end.lat,end.lng) < NaviBridge.distM(points[$1].lat,points[$1].lng,end.lat,end.lng) } ?? cursor
            }
            return MotorStep(instruction:road.name,road:road.name,action:road.name,end:cursor)
        }
        return MotorRoute(request:request,points:points,steps:steps.isEmpty ? [MotorStep(instruction:"沿路线前进",road:"",action:"",end:points.count-1)] : steps,seconds:r.timeS)
    }
    static func load(_ request: MotorRequest) async throws -> [MotorRoute] {
        if request.bicycle { return try await BicycleRouteQuery.load(request) }
        var url = URLComponents(string: "https://amap-pc-ssr.amap.com/ssr/api/getMotorRoutePlan")!
        url.queryItems = [.init(name: "fromX", value: "\(request.start.lng)"), .init(name: "fromY", value: "\(request.start.lat)"), .init(name: "toX", value: "\(request.end.lng)"), .init(name: "toY", value: "\(request.end.lat)")]
        if !request.start.id.isEmpty { url.queryItems?.append(.init(name: "originid",value: request.start.id)) }
        if !request.end.id.isEmpty { url.queryItems?.append(.init(name: "destinationid",value: request.end.id)) }
        if !request.vias.isEmpty { url.queryItems?.append(.init(name: "waypoints",value: request.vias.map { "\($0.lng),\($0.lat)" }.joined(separator: ";"))) }
        var req = URLRequest(url: url.url!); req.timeoutInterval = 25
        req.setValue(ua, forHTTPHeaderField: "User-Agent"); req.setValue("https://www.amap.com/",forHTTPHeaderField: "Referer")
        // The persistent WK cookie store owns login state. Never print or export cookies.
        let cookies: [HTTPCookie] = await withCheckedContinuation { continuation in
            DispatchQueue.main.async {WKWebsiteDataStore.default().httpCookieStore.getAllCookies {continuation.resume(returning:$0)}}
        }
        let targetHost = url.host ?? ""
        let domainCookies = cookies.filter { c in
            let domain = c.domain.hasPrefix(".") ? String(c.domain.dropFirst()) : c.domain
            return (targetHost == domain || targetHost.hasSuffix("." + domain)) && (url.path.hasPrefix(c.path)) && (c.expiresDate == nil || c.expiresDate! > Date())
        }
        for (key,value) in HTTPCookie.requestHeaderFields(with: domainCookies) { req.setValue(value, forHTTPHeaderField: key) }
        let config=URLSessionConfiguration.ephemeral
        config.httpCookieStorage=nil;config.httpShouldSetCookies=false
        let session=URLSession(configuration:config,delegate:NoRedirects(),delegateQueue:nil)
        defer{session.finishTasksAndInvalidate()}
        let (bytes, response) = try await session.data(for: req)
        if let response=response as? HTTPURLResponse {
            var fields:[String:String]=[:]
            for (key,value) in response.allHeaderFields {fields[String(describing:key)]=String(describing:value)}
            for cookie in HTTPCookie.cookies(withResponseHeaderFields:fields,for:req.url!) {
                let domain=cookie.domain.trimmingCharacters(in:CharacterSet(charactersIn:"."))
                guard targetHost==domain || targetHost.hasSuffix("."+domain) else{continue}
                await withCheckedContinuation { (c:CheckedContinuation<Void,Never>) in
                    DispatchQueue.main.async {WKWebsiteDataStore.default().httpCookieStore.setCookie(cookie){c.resume()}}
                }
            }
            if [401,403].contains(response.statusCode) {throw AmapShare.Fail("高德要求登录或网页验证，请点“高德登录／验证”处理后重试")}
            if response.statusCode==429 {throw AmapShare.Fail("请求较频繁，请稍后再试")}
        }
        guard (response as? HTTPURLResponse)?.statusCode == 200, bytes.count <= 8*1024*1024 else { throw AmapShare.Fail("网页算路请求失败或数据过大") }
        return try parse(bytes, request: request)
    }
    static func parse(_ bytes: Data, request: MotorRequest) throws -> [MotorRoute] {
        guard let json = try JSONSerialization.jsonObject(with: bytes) as? [String: Any], (json["code"] as? NSNumber)?.intValue == 1,
              let paths = json["data"] as? [[String: Any]], (1...10).contains(paths.count) else { throw AmapShare.Fail("高德未返回摩托车路线，请回网页检查登录或验证提示") }
        return try paths.map { path in
            guard let steps = path["steps"] as? [[String: Any]], !steps.isEmpty, steps.count <= 10000 else { throw AmapShare.Fail("路线缺少分段") }
            var points: [MotorPoint] = [], instructions: [MotorStep] = []
            for step in steps {
                let start=max(0,points.count-1)
                guard let poly = step["polyline"] as? String, !poly.isEmpty else { throw AmapShare.Fail("路线缺少坐标") }
                for pair in poly.split(separator: ";") {
                    let xy = pair.split(separator: ","); guard xy.count == 2, let lo = Double(xy[0]), let la = Double(xy[1]), MotorRequest.valid(la,lo) else { throw AmapShare.Fail("无效路线坐标") }
                    if let p = points.last { if p.lat == la && p.lng == lo { continue }; guard NaviBridge.distM(p.lat,p.lng,la,lo) < 5000 else { throw AmapShare.Fail("路线不连续") } }
                    guard points.count < 150000 else { throw AmapShare.Fail("路线过长") }; points.append(MotorPoint(lat: la,lng: lo))
                }
                instructions.append(MotorStep(instruction: step["instruction"] as? String ?? "", road: step["road_name"] as? String ?? "", action: (step["navi"] as? [String: Any])?["action"] as? String ?? "", end: max(0,points.count-1),start:start,distance:(step["step_distance"] as? NSNumber)?.intValue ?? 0,duration:((step["cost"] as? [String:Any])?["duration"] as? NSNumber)?.intValue ?? 0))
            }
            guard points.count >= 2 else { throw AmapShare.Fail("路线坐标不足") }
            let total=RouteShape(points:points).cum.last ?? 0
            guard total>=3,total<=5_000_000 else{throw AmapShare.Fail("路线长度无效")}
            return MotorRoute(request: request, points: points, steps: instructions, seconds: ((path["cost"] as? [String: Any])?["duration"] as? NSNumber)?.intValue ?? 0)
        }
    }
}

/// Independent route following. Never converts motor or bicycle routes into a drive SDK route.
final class WebMotorSession: ObservableObject {
    static let shared = WebMotorSession()
    @Published var route: MotorRoute?
    @Published var running = false
    @Published var simulation = false
    @Published var paused = false
    @Published var speed = 60
    @Published var remaining = 0
    @Published var instruction = ""
    @Published var status = ""
    @Published var position: MotorPoint?
    @Published var arrived = false
    @Published var overview = false
    private var cum: [Double] = [], progress = 0.0, timer: Timer?, token = UUID(), announced = ""
    private var stepIndex = 0, offCount = 0, rerouting = false, lastReplan = Date.distantPast
    private var roadSent = "", footerSent = ""
    private var startedAt = Date(), tripMeters = 0.0, tripMax = 0, previousFix: CLLocation?
    private var viaProgress: [Double] = [], nextVia = 0, lastFix: Date?
    private var tracker: MotorRouteTracker?
    private var confirmedArrival=false
    private var metadataSuspended=false
    private var simulationTick: TimeInterval?
    private var realHeading: Double?
    @Published var turnMetres = 0
    @Published var turnDirection = "S"
    @Published var remainingSeconds = 0
    private var watchdog: Timer?
    func start(_ route: MotorRoute, simulate: Bool, simulationSpeed: Int = 60) {
        guard route.points.count>=2,!route.steps.isEmpty,!NaviBridge.shared.navigating,
              (route.cum.last ?? 0)>=3,(route.cum.last ?? 0)<=5_000_000,
              route.steps.allSatisfy({route.points.indices.contains($0.end)}) else { return }
        stop(); NaviBridge.shared.stop(); NaviBridge.shared.stopCruise()
        self.route = route; cum = route.cum; progress = 0; stepIndex = 0; simulation = simulate; paused = false; arrived = false; running = true
        tracker=MotorRouteTracker(route.points);confirmedArrival=false;metadataSuspended=false;simulationTick=nil;realHeading=nil
        speed = NavigationSpeed.values.contains(simulationSpeed) ? simulationSpeed : 60; offCount = 0; nextVia = 0; lastFix = nil; viaProgress = route.request.vias.map { v in nearestAlong(v) }
        roadSent = ""; footerSent = ""; startedAt = Date(); tripMeters = 0; tripMax = 0; previousFix = nil
        announced = ""; status = simulate ? "模拟导航" : "定位导航"; token = UUID()
        UIApplication.shared.isIdleTimerDisabled = true
        MapFeeder.shared.onNaviStart(); BleLink.shared.send("C")
        MapFeeder.shared.prepareRoute(key: route.id.uuidString, coords: route.coords)
        WebRouteMetadata.shared.load(route)
        let sessionToken=token
        watchdog=Timer.scheduledTimer(withTimeInterval:1,repeats:true) { [weak self] _ in
            guard let self=self,self.running,self.token==sessionToken,!self.simulation else { return }
            if self.lastFix == nil || Date().timeIntervalSince(self.lastFix!)>3 {
                WebRouteMetadata.shared.push(route:route,progress:self.progress,fresh:false,speed:0)
                if Date().timeIntervalSince(self.lastFix ?? self.startedAt)>10 {
                    self.status="GPS 信号弱，等待新定位";BleLink.shared.send("#x,1")
                }
            }
        }
        if simulate {
            let generation = token
            timer = Timer.scheduledTimer(withTimeInterval: 0.25, repeats: true) { [weak self] _ in
                guard let self = self, self.running, self.token == generation else { return }
                let now=ProcessInfo.processInfo.systemUptime
                let dt=min(2,max(0,self.simulationTick.map{now-$0} ?? 0.25));self.simulationTick=now
                if self.paused{return}
                self.progress = min(self.cum.last ?? 0, self.progress + Double(self.speed)/3.6*dt)
                let p=self.at(self.progress)
                if let match=self.tracker?.update(p,accuracy:3,now:ProcessInfo.processInfo.systemUptime) {self.confirmedArrival=match.arrived}
                self.render(p, off: 0)
            }
        } else { RideRecorder.shared.start("网页路线导航"); LocationService.shared.setPrecise(true) }
        render(route.points[0], off: 0)
    }
    func stop(sendCancel:Bool=true) {
        token = UUID(); timer?.invalidate(); timer = nil
        watchdog?.invalidate();watchdog=nil;WebRouteMetadata.shared.stop()
        if running { Speech.shared.stopNow(); RideRecorder.shared.finish(); MapFeeder.shared.onNaviEnd(); if sendCancel {BleLink.shared.send("C")}; LocationService.shared.setPrecise(false) }
        running = false; rerouting = false; UIApplication.shared.isIdleTimerDisabled = false
    }
    private func at(_ distance: Double) -> MotorPoint {
        guard let route = route else { return MotorPoint(lat: 0,lng: 0) }
        var lo = 1, hi = cum.count-1
        while lo < hi { let mid = (lo+hi)/2; if cum[mid] < distance { lo = mid+1 } else { hi = mid } }
        let ratio = min(1,max(0,(distance-cum[lo-1])/max(0.001,cum[lo]-cum[lo-1])))
        let a = route.points[lo-1], b = route.points[lo]
        return MotorPoint(lat: a.lat+(b.lat-a.lat)*ratio,lng: a.lng+(b.lng-a.lng)*ratio)
    }
    func onFix(_ l: CLLocation) {
        guard running, !simulation, l.horizontalAccuracy >= 0, l.horizontalAccuracy <= 60, abs(l.timestamp.timeIntervalSinceNow) < 15,
              lastFix == nil || l.timestamp > lastFix! else { return }
        let g = MapGeo.wgsToGcj(l.coordinate.latitude,l.coordinate.longitude),point=MotorPoint(lat:g.lat,lng:g.lng)
        guard let match=tracker?.update(point,accuracy:l.horizontalAccuracy,now:ProcessInfo.processInfo.systemUptime) else{return}
        lastFix = l.timestamp;confirmedArrival=match.arrived
        if let previous = previousFix { let d = l.distance(from: previous); if d < 300 { tripMeters += d } }
        let measuredSpeed=l.speed.isFinite ? min(250,max(0,l.speed*3.6)) : 0
        previousFix = l; tripMax = max(tripMax,Int(measuredSpeed))
        speed = Int(measuredSpeed)
        if l.course>=0,l.speed>0.3 {realHeading=l.course}
        if match.offset <= 60 { progress = max(progress,match.progress); offCount = 0 }
        else { offCount += 1 }
        if !rerouting {
            status=match.offset>60 ? "已偏离路线 · 距路线 \(Int(match.offset)) 米" : "\(route?.request.bicycle == true ? "自行车" : "摩托车")导航 · 定位精度 \(Int(l.horizontalAccuracy)) 米"
            if match.offset<=60,metadataSuspended,let r=route {metadataSuspended=false;WebRouteMetadata.shared.load(r)}
        }
        while nextVia < viaProgress.count && progress > viaProgress[nextVia]+40 { nextVia += 1 }
        render(point,off: match.offset)
        if offCount >= 3, !rerouting, Date().timeIntervalSince(lastReplan) > 20 { replan(point) }
    }
    private func replan(_ point: MotorPoint) {
        guard let route = route else { return }
        rerouting = true; lastReplan = Date(); status = "偏航，正在重新规划\(route.request.bicycle ? "自行车" : "摩托车")路线…"; let generation = token
        WebRouteMetadata.shared.stop();metadataSuspended=true;BleLink.shared.send("#y,1")
        var request = route.request; request.start = point; request.vias = Array(request.vias.dropFirst(nextVia))
        Task { @MainActor in
            do {
                let routes = try await MotorRoutes.load(request)
                guard running, token == generation, let new = routes.first else { return }
                self.route = new; cum = new.cum; progress = 0; stepIndex = 0; offCount = 0; nextVia = 0
                tracker=MotorRouteTracker(new.points);confirmedArrival=false;lastFix=nil;previousFix=nil;metadataSuspended=false
                // Via coordinates may lie far ahead; search the entire new route for each one.
                viaProgress = new.request.vias.map { v in nearestAlong(v) }
                MapFeeder.shared.prepareRoute(key: new.id.uuidString,coords: new.coords); status = "路线已重新规划"
                WebRouteMetadata.shared.load(new)
            } catch { if token == generation { status = "重算失败，保留原路线：\(error.localizedDescription)" } }
            if token == generation { rerouting = false;BleLink.shared.send("#y,0") }
        }
    }
    private func nearestAlong(_ p: MotorPoint) -> Double {
        guard let route = route else { return 0 }
        let index = route.points.indices.min { NaviBridge.distM(p.lat,p.lng,route.points[$0].lat,route.points[$0].lng) < NaviBridge.distM(p.lat,p.lng,route.points[$1].lat,route.points[$1].lng) } ?? 0
        return cum[index]
    }
    private func render(_ p: MotorPoint, off: Double) {
        guard running, let route = route else { return }
        position = p
        while stepIndex < route.steps.count-1 && progress+2 >= cum[route.steps[stepIndex].end] { stepIndex += 1 }
        let step = route.steps[stepIndex]
        let fraction=min(1,max(0,(cum[step.end]-progress)/max(0.001,cum[step.end]-cum[min(step.start,step.end)])))
        let distance=step.distance>0 ? Int((Double(step.distance)*fraction).rounded()) : max(0,Int(cum[step.end]-progress))
        remaining=route.steps.contains(where:{$0.distance>0}) ? distance+route.steps.dropFirst(stepIndex+1).reduce(0){$0+max(0,$1.distance)} : max(0,Int((cum.last ?? 0)-progress))
        turnMetres=distance;turnDirection=step.dir
        if simulation { tripMeters = progress; tripMax = max(tripMax,speed) }
        instruction = step.instruction.isEmpty ? step.road : step.instruction
        let ahead = at(min(cum.last ?? 0,progress+20))
        let geometryHeading = atan2((ahead.lng-p.lng)*cos(p.lat * .pi/180),ahead.lat-p.lat)*180 / .pi
        let heading = simulation ? geometryHeading : (realHeading ?? geometryHeading)
        MapFeeder.shared.onLocation(lat: p.lat,lng: p.lng,bearing: heading,kmh: speed)
        _ = MapFeeder.shared.onRoute(key: route.id.uuidString,coords: route.coords,lights: [])
        BleLink.shared.send("#v,\(speed)")
        BleLink.shared.send("#x,0")
        WebRouteMetadata.shared.push(route:route,progress:progress,fresh:off<=60 && !rerouting,speed:speed)
        if off <= 60 { BleLink.shared.send("\(step.dir),\(distance)") }
        if roadSent != step.instruction {
            roadSent = step.instruction
            if let img = TextImage.render(String((step.instruction.isEmpty ? step.road : step.instruction).prefix(160)),maxW:290,size:26,minSize:18) { BleLink.shared.sendImage(slot:0,w:img.w,h:img.h,data:img.data) }
        }
        let seconds = route.steps.contains(where:{$0.duration>0}) ? Int(Double(step.duration)*fraction)+route.steps.dropFirst(stepIndex+1).reduce(0){$0+max(0,$1.duration)} : Int(Double(route.seconds)*Double(remaining)/max(1,Double(route.totalMeters)))
        remainingSeconds=seconds
        let formatter = DateFormatter(); formatter.dateFormat = "HHmm"
        let eta = formatter.string(from: Date().addingTimeInterval(Double(seconds)))
        BleLink.shared.send("#e,\(remaining),\(eta),0")
        let footer = "\(NaviBridge.distText(remaining)) · \(eta.prefix(2)):\(eta.suffix(2))到达"
        if footer != footerSent {
            footerSent = footer
            if let img = TextImage.render(footer,maxW:240,size:19,minSize:15) { BleLink.shared.sendImage(slot:1,w:img.w,h:img.h,data:img.data) }
        }
        if off <= 60, announced != "\(stepIndex)-\(distance/100)", distance <= 500 {
            announced = "\(stepIndex)-\(distance/100)"; Speech.shared.speak("前方\(distance)米，\(instruction)")
        }
        let end = route.points.last!
        // Destination-ahead text never completes a trip. Require final progress AND physical proximity.
        if confirmedArrival && MotorArrival.confirmed(remaining: remaining, offset: off, endpointDistance: NaviBridge.distM(p.lat,p.lng,end.lat,end.lng)) {
            let meters = Int(tripMeters), minutes = max(1,Int(Date().timeIntervalSince(startedAt)/60)), maximum = tripMax
            stop(sendCancel:false); arrived = true; status = "已到达"
            BleLink.shared.send("#O\(meters)/\(minutes)/\(maximum)"); BleLink.shared.send("A,0")
            Speech.shared.speak("已到达目的地")
            let generation = token
            DispatchQueue.main.asyncAfter(deadline: .now()+4) { [weak self] in guard let self = self, self.token == generation, !self.running, !NaviBridge.shared.navigating, !NaviBridge.shared.cruising else { return }; BleLink.shared.send("C") }
        }
    }
}

final class MotorBrowser: NSObject, ObservableObject, WKNavigationDelegate {
    @Published var address = "https://www.amap.com/ssr/dir?type=4"
    @Published var status = ""
    let web: WKWebView
    override init() {
        let config = WKWebViewConfiguration(); config.websiteDataStore = .default()
        web = WKWebView(frame: .zero,configuration: config)
        super.init(); web.customUserAgent = MotorRoutes.ua; web.navigationDelegate = self
        web.load(URLRequest(url: URL(string: address)!))
    }
    func webView(_ webView: WKWebView, didFinish navigation: WKNavigation!) { address = webView.url?.absoluteString ?? address }
    func webView(_ webView: WKWebView, didFail navigation: WKNavigation!, withError error: Error) { status = error.localizedDescription }
    func webView(_ webView: WKWebView, decidePolicyFor navigationAction: WKNavigationAction, decisionHandler: @escaping (WKNavigationActionPolicy) -> Void) {
        guard let u = navigationAction.request.url, u.scheme == "https", let host = u.host, host == "amap.com" || host.hasSuffix(".amap.com") || host == "aliyun.com" || host.hasSuffix(".aliyun.com") else {
            status = "此页面试图打开外部应用或其他网站，请在高德网页完成路线规划"; decisionHandler(.cancel); return
        }
        if navigationAction.targetFrame == nil { webView.load(navigationAction.request); decisionHandler(.cancel) } else { decisionHandler(.allow) }
    }
}
struct MotorBrowserView: UIViewRepresentable {
    let browser: MotorBrowser
    func makeUIView(context: Context) -> WKWebView { browser.web }
    func updateUIView(_ uiView: WKWebView,context: Context) {}
}
struct MotorMap:UIViewRepresentable {
    let route:MotorRoute;let position:MotorPoint?;let overview:Bool
    final class Coordinator {
        let delegate=RouteMapDelegate()
        var routeID:UUID?
        var overview:Bool?
        var position:MotorPoint?
        var positionPin:MAPointAnnotation?
        var line:MAPolyline?
    }
    func makeCoordinator()->Coordinator{Coordinator()}
    func makeUIView(context:Context)->MAMapView{
        let map=MAMapView(frame:.zero)
        map.mapType=Settings.shared.paletteNight ? .standardNight:.standard
        map.delegate=context.coordinator.delegate.asDelegate;map.showsCompass=false;return map
    }
    func updateUIView(_ map:MAMapView,context:Context){
        let c=context.coordinator
        let type:MAMapType=Settings.shared.paletteNight ? .standardNight:.standard
        if map.mapType != type{map.mapType=type}
        let routeChanged=c.routeID != route.id
        if routeChanged{
            c.routeID=route.id;c.position=nil;c.positionPin=nil
            if let overlays=map.overlays{map.removeOverlays(overlays)}
            if let annotations=map.annotations{map.removeAnnotations(annotations.filter{!($0 is MAUserLocation)})}
            var coords=route.points.map{CLLocationCoordinate2D(latitude:$0.lat,longitude:$0.lng)}
            c.line=MAPolyline(coordinates:&coords,count:UInt(coords.count));c.line?.title="sel"
            if let line=c.line{map.addOverlays([line])}
            let marks=[route.request.start]+route.request.vias+[route.request.end]
            for (i,p) in marks.enumerated(){
                let mark=MAPointAnnotation();mark.coordinate=CLLocationCoordinate2D(latitude:p.lat,longitude:p.lng)
                mark.title=i==marks.count-1 ? "目的地":i==0 ? "起点":"途经 \(i)";map.addAnnotation(mark)
            }
        }
        if overview,(routeChanged || c.overview != overview),let line=c.line{
            map.showOverlays([line],edgePadding:UIEdgeInsets(top:40,left:30,bottom:40,right:30),animated:false)
        }
        if let p=position{
            let changed=c.position.map{RouteShape.metres($0,p)>=0.5} ?? true
            if changed{
                c.position=p
                if c.positionPin==nil{let pin=MAPointAnnotation();pin.title="当前位置";c.positionPin=pin;map.addAnnotation(pin)}
                c.positionPin?.coordinate=CLLocationCoordinate2D(latitude:p.lat,longitude:p.lng)
            }
            if !overview,(changed || routeChanged || c.overview != overview){
                map.setCenter(CLLocationCoordinate2D(latitude:p.lat,longitude:p.lng),animated:false)
                if routeChanged || c.overview != overview{map.setZoomLevel(17,animated:false)}
            }
        }
        c.overview=overview
    }
}
struct WebMotorView: View {
    @ObservedObject var guide=WebMotorSession.shared
    var mode: IndependentMode = .motorcycle
    var body: some View {
        Group {
            if guide.running || guide.arrived { IndependentNavigationView() }
            else { IndependentPlannerView(mode:mode) }
        }.preferredColorScheme(.dark)
    }
}
