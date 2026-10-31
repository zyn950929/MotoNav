import Foundation
import ObjectiveC
import CoreLocation

/// 版本：跟随 App 版本（见 Config.swift）
///
/// 高德导航 ↔ 圆屏：
/// - 规划路线、开始 / 停止导航（实时 GPS 或模拟）
/// - 高德每秒给一次导航信息：转向图标、到路口的距离、路名、剩余里程和时间 → 按安卓版同样的格式发给圆屏
/// - 两次导航信息之间按车速推算距离（每 0.2 秒），圆屏上的数字和进度环走得更顺
/// - 车速、电子眼限速、播报文字（交给 Speech 念，播报时压低音乐）
/// - iOS v0.2 和安卓版对齐：
///   · 路口实际转弯角度 "L,153,-128"（第三个数：负数向左、正数向右），按路线形状算
///   · 车道 "#j,…"；前方红绿灯距离 "#g,米"（300 米内）
///   · 测速牌按车速提前提醒（500~1000 米），按位置跟踪到经过为止；前方 1 公里电子眼画在小地图上 "#c,…"
///   · 小地图：位置 "#p"、路线和路上红绿灯（MapFeeder）、路况颜色 "#u"
///   · GPS 信号弱 "#x"、更快路线 "#h"（导航画面上可以一键切换）、拥堵重算也显示“重新规划中”
/// - iOS v0.3：
///   · 一次规划最多 3 条路线（躲避拥堵），可选“不走高速”，界面上对比后选一条
///   · 按导入的高德分享路线规划（沿路挑途经点，见 AmapShare.swift），偏航后按原路线回去（ShareGuide）
///   · 巡航模式（不设目的地）：高德“智能巡航”提醒电子眼和道路安全设施，"#z,1/0"，小地图照常显示
/// - iOS v0.4：途经点（最多 3 个，按顺序经过，比如先去加油站）
/// - iOS v0.5.2：打开高德多路线导航模式（途中才会推荐更快的路线）；到达后圆屏显示“已到达”4 秒再清屏回到待机
/// - iOS v0.6：规划失败自动重试一次 + 可点“重新规划”；模拟导航暂停 / 调速；首页剩余里程；
///   导航中也把前方 1 公里的道路安全设施（急弯、事故多发、落石、村庄…，来自高德路线数据）画在小地图上
final class NaviBridge: NSObject, ObservableObject {
    static let shared = NaviBridge()

    @Published var calculating = false
    @Published var routeReady = false
    @Published var navigating = false
    @Published var info = "（还没开始导航）"
    @Published var routeSummary = ""
    // 给界面看
    @Published var uiDir = ""
    @Published var uiDist = -1
    @Published var uiSpeed = 0
    @Published var uiRoad = ""
    @Published var uiSuggestMin = 0                 // 更快路线能省几分钟（0 = 没有）
    @Published var uiRemainM = 0                    // iOS v0.6 剩余里程（米）、剩余时间（秒），首页显示
    @Published var uiRemainS = 0
    @Published var planFailed = false               // iOS v0.6 规划失败 / 超时：界面上显示“重新规划”
    // iOS v0.6 模拟导航：暂停 / 继续、车速 40 → 60 → 100 → 150 循环
    @Published private(set) var emulating = false
    @Published private(set) var emuPaused = false
    @Published private(set) var emuSpeed = 60
    private static let EMU_SPEEDS = NavigationSpeed.values
    @Published var preferredSimulationSpeed=60
    // 调试信息
    @Published var laneRaw = "（还没收到车道信息）"
    @Published var camRaw = "（还没收到电子眼信息）"
    @Published var extraInfo = ""
    @Published var showArrived = false              // 刚到达目的地（手机和圆屏显示“已到达”4 秒）
    private var arriveSeq = 0
    // iOS v0.3 选路线
    struct RouteOption: Identifiable {
        let id: Int                                 // 高德的路线编号
        let label: String
        let timeS: Int, length: Int, lights: Int, toll: Int
        let coords: [CLLocationCoordinate2D]
    }
    @Published var routeOptions: [RouteOption] = []
    @Published var selectedRouteID = -1
    @Published var shareMatch = ""                  // 和分享路线的吻合程度
    @Published private(set) var shareRoute: AmapShare.Route?
    struct Via { let name: String; let lat: Double; let lng: Double }
    @Published private(set) var vias: [Via] = []     // iOS v0.4 途经点（分享的路线自带途经点，不用这个）
    var destCoord: CLLocationCoordinate2D? { destLat.isNaN ? nil : CLLocationCoordinate2D(latitude: destLat, longitude: destLng) }
    private var destLat = Double.nan, destLng = Double.nan
    private var planSeq = 0
    private var lastRouteDoneAt = Date.distantPast
    // iOS v0.3 巡航
    @Published var cruising = false
    @Published var cruiseRaw = "（还没收到巡航数据）"

    private var manager: AMapNaviDriveManager { AMapNaviDriveManager.sharedInstance() }
    private var attached = false
    private let arrival = ArrivalPolicy()
    private var sessionCallbacks: NaviSessionCallbacks?

    // Each SDK delegate captures its own generation. Queued or retained old delegates
    // cannot finish, restart, or publish guidance for a newer trip.
    private func installCallbacks() {
        let m = manager
        if let old = sessionCallbacks?.asRepresentative { m.removeDataRepresentative(old) }
        m.delegate = nil
        let callbacks = NaviSessionCallbacks(bridge: self, token: arrival.generation)
        sessionCallbacks = callbacks
        m.delegate = callbacks.asDelegate
        if let r = callbacks.asRepresentative { m.addDataRepresentative(r) }
    }

    func acceptsCallback(_ token: UInt64) -> Bool { arrival.acceptsCallbacks(token) }
    func performCallback(_ token: UInt64, _ body: (NaviBridge) -> Void) {
        dispatchPrecondition(condition: .onQueue(.main))
        guard arrival.acceptsCallbacks(token) else { return }
        body(self)
    }

    private func invalidateArrival() {
        arriveSeq += 1
        showArrived = false
    }

    private static let TICK = 0.2                   // 推算频率：每秒 5 次
    private static let MAX_PREDICT = 1.2            // 超过 1.2 秒没新数据就不再推算，减少急停时的误差
    private static let CAMERA_TIMEOUT = 90.0        // 90 秒没有任何更新就不再显示测速牌
    private static let LIGHT_SHOW_M = 300           // 红绿灯 300 米以内才在圆屏上显示

    // 推算用
    private var curDir = ""
    private var curAngle: Int?                       // 当前路口的实际转弯角度（度），算不出来时为 nil
    private var angleKey = ""
    private var lastRouteKey = ""                    // 已发给小地图的路线
    private var baseDist = 0
    private var baseAt = Date()
    private var speedMps = 0.0
    private var speedKmhNow = 0
    private var shownDir = ""
    private var shownDist = -1
    private var lastSent = ""
    private var tick: Timer?
    private var lastRoadText = ""
    private var lastBottomText = ""
    private var tripMeters = 0.0, tripMaxKmh = 0, tripStarted = Date()
    private var tripPosition: (lat: Double,lng: Double)?
    private var lastTrip = ""
    private var lastSpeedMsg = ""
    private var rerouting = false
    private var myLat = Double.nan, myLng = Double.nan
    var lastLocation: CLLocation? {
        myLat.isNaN ? nil : CLLocation(latitude: myLat, longitude: myLng)
    }
    // 红绿灯
    private var lightBase = -1                       // 最近一次算出的前方红绿灯距离（米，-1 = 没有）
    private var lightAt = Date()
    private var lastLightMsg = ""
    // 测速（正在跟踪的测速点）
    private var lastCamMsg = ""
    private var lastCamAt = Date.distantPast
    private var camLimit = 0
    private var camLat = 0.0, camLng = 0.0
    private var camMinDist = Int.max
    private var lastCamMapMsg = ""
    /// 小地图上的一个电子眼 / 道路安全提示点
    struct MapPt { let x: Int, y: Int, spd: Int, icon: Int, dist: Int; var at = Date() }
    private var camPts: [MapPt] = []                 // 导航中的电子眼
    private var cruiseCamPts: [MapPt] = []           // 巡航中的电子眼
    private var facPts: [String: MapPt] = [:]        // 道路安全提示（按位置合并，20 秒没更新就去掉）
    private let cruiseIdle = CruiseIdleTimer()
    private var cruiseTick: Timer?
    private var cruiseHeading = 0.0
    // iOS v0.6 导航中的道路安全设施：路线上每个设施离路线起点多远（米）、位置、图标；当前前方 1 公里内的画在小地图上
    private var routeFacs: [(along: Double, lat: Double, lng: Double, icon: Int)] = []
    private var routeFacKey = ""
    private var navFacPts: [MapPt] = []
    // 车道、路况、GPS、更快路线
    private var lastLaneMsg = ""
    private var lastTrafficMsg = ""
    private var lastTrafficAt = Date.distantPast
    private var gpsWeakSent = false
    private weak var suggestRoute: AMapNaviRoute?
    private var suggestTimer: Timer?

    /// 测速提前提醒：按车速留出约 20 秒反应时间，最少 500 米，最多 1000 米
    private var cameraRange: Int { min(1000, max(500, Int(Double(speedKmhNow) / 3.6 * 20))) }

    /// iOS v0.7 填了（或换了）高德 key：导航模块在没有 key 时初始化过，不会自己换 key，
    /// 销毁重建一次，用新 key 重新初始化（导航、巡航中不动，等下次）
    func rebuildManager() {
        if navigating || cruising || WebMotorSession.shared.running { return }
        if attached {
            let m = manager
            arrival.cancel(arrival.generation)
            if let r = sessionCallbacks?.asRepresentative { m.removeDataRepresentative(r) }
            sessionCallbacks = nil
            m.delegate = nil
            attached = false
        }
        let ok = AMapNaviDriveManager.destroyInstance()
        extraInfo = ok ? "已用新的高德 key 重新初始化导航" : "导航模块没能重建，重启一次 App 新 key 就生效了"
        routeReady = false; routeOptions = []; selectedRouteID = -1
        attach()
    }

    func attach() {
        if attached { return }
        WebRouteMetadata.shared.stop()
        attached = true
        let m = manager
        // Runtime protocol registration lives on the per-session callback relay.
        arrival.preparePlan()
        installCallbacks()
        m.allowsBackgroundLocationUpdates = true
        m.pausesLocationUpdatesAutomatically = false
        m.isUseInternalTTS = false              // 高德自己的播报关掉，文字交给我们念（播报时压低音乐）
        // 多路线导航模式：导航途中高德才会推荐更快的路线（onSuggestChangeMainNaviRoute）。
        // 要在算路前设置；还要求多路线策略、没有途经点、起终点直线 80 公里以内，否则高德自动按单路线导航
        m.setMultipleRouteNaviMode(true)
        m.updateCameraInfo = true
        m.updateTrafficInfo = true
    }

    /// Relinquish planning callbacks while independent guidance reads static camera metadata.
    /// The next SDK plan calls attach() and installs a new arrival/callback generation.
    func releaseForWebMetadata() -> Bool {
        guard !navigating, !cruising else { return false }
        planSeq += 1; arrival.cancel(arrival.generation)
        if let callbacks=sessionCallbacks?.asRepresentative { manager.removeDataRepresentative(callbacks) }
        sessionCallbacks=nil;manager.delegate=nil;attached=false
        calculating=false;routeReady=false;routeOptions=[];selectedRouteID = -1
        return true
    }

    // MARK: - 路线和导航

    /// 现在的位置（高德坐标）：导航定位优先，其次手机定位
    func myPosition() -> (lat: Double, lng: Double)? {
        if !myLat.isNaN { return (myLat, myLng) }
        guard let l = LocationService.shared.location else { return nil }
        let g = MapGeo.wgsToGcj(l.coordinate.latitude, l.coordinate.longitude)
        return (g.lat, g.lng)
    }

    /// 规划去 (lat, lng)（高德坐标）的路线。share 不为 nil 时按导入的高德分享路线走（沿路挑途经点，只要一条）；
    /// 否则一次规划最多 3 条（躲避拥堵），“不走高速”看设置。
    func plan(toLat lat: Double, lng: Double, share: AmapShare.Route? = nil) {
        guard !navigating,!WebMotorSession.shared.running else { return }
        attach()
        destLat = lat; destLng = lng
        shareRoute = share
        vias = []                                    // 新目的地：途经点清空
        replan()
    }

    /// 加一个途经点（最多 3 个），重新规划
    func addVia(name: String, lat: Double, lng: Double) {
        if vias.count >= 3 || shareRoute != nil { return }
        vias.append(Via(name: name, lat: lat, lng: lng))
        replan()
    }

    func replaceVias(_ points:[MotorPoint]) {
        guard !navigating,!cruising,shareRoute == nil,points.count<=3,
              points.allSatisfy({MotorRequest.valid($0.lat,$0.lng)}) else{return}
        vias=points.map{Via(name:$0.name,lat:$0.lat,lng:$0.lng)}
        replan()
    }

    func clearVias() {
        if vias.isEmpty { return }
        vias = []
        replan()
    }

    /// 不去这个目的地了（换目的地）
    func clearPlan() {
        if navigating { return }
        let holdingArrival = arrival.canClearArrival(arrival.generation)
        arrival.cancel(arrival.generation)
        if holdingArrival { manager.stopNavi() }
        invalidateArrival()
        MapFeeder.shared.onNaviEnd()
        planSeq += 1
        destLat = .nan; destLng = .nan
        shareRoute = nil; vias = []
        calculating = false; routeReady = false; planFailed = false
        routeOptions = []; selectedRouteID = -1; shareMatch = ""; routeSummary = ""
        info = "（还没开始导航）"
    }

    /// 按上次的目的地重新规划（比如切换了“不走高速”）
    func replan(retry: Int = 1) {
        guard !destLat.isNaN, !navigating, !WebMotorSession.shared.running else { return }
        planFailed = false
        MapFeeder.shared.onNaviEnd()
        if cruising { stopCruise() }
        let holdingArrival = arrival.canClearArrival(arrival.generation)
        arrival.cancel(arrival.generation)
        if holdingArrival { manager.stopNavi() }
        arrival.preparePlan()
        invalidateArrival()
        installCallbacks()
        guard let end = AMapNaviPoint.location(withLatitude: CGFloat(destLat), longitude: CGFloat(destLng)) else { return }
        planSeq += 1
        calculating = true
        routeReady = false
        routeOptions = []; selectedRouteID = -1; shareMatch = ""
        let avoid = Settings.shared.avoidHighway
        let me = myPosition()
        var wps: [AMapNaviPoint] = []
        let strategy: AMapNaviDrivingStrategy
        if let sr = shareRoute {
            wps = AmapShare.waypoints(sr, myLat: me?.lat ?? .nan, myLng: me?.lng ?? .nan)
                .compactMap { AMapNaviPoint.location(withLatitude: CGFloat($0.lat), longitude: CGFloat($0.lng)) }
            strategy = ConvertDrivingPreferenceToDrivingStrategy(false, false, avoid, false, false)   // 不躲拥堵，免得绕开原路线
            info = "正在按高德分享的路线规划（\(wps.count) 个途经点）…"
        } else {
            wps = vias.compactMap { AMapNaviPoint.location(withLatitude: CGFloat($0.lat), longitude: CGFloat($0.lng)) }
            strategy = ConvertDrivingPreferenceToDrivingStrategy(true, true, avoid, false, false)     // 多条路线 + 躲避拥堵
            info = vias.isEmpty ? "正在规划路线…" : "正在规划路线（途经 \(vias.count) 个地方）…"
        }
        let ok: Bool
        if let me = me, let start = AMapNaviPoint.location(withLatitude: CGFloat(me.lat), longitude: CGFloat(me.lng)) {
            ok = manager.calculateDriveRoute(withStart: [start], end: [end], wayPoints: wps, drivingStrategy: strategy)
        } else {
            ok = manager.calculateDriveRoute(withEnd: [end], wayPoints: wps, drivingStrategy: strategy)
        }
        let seq = planSeq
        if !ok {
            calculating = false
            if retry > 0 {                          // 导航引擎可能还没准备好：1.5 秒后自动再试一次（和安卓一样）
                info = "规划路线没能开始，马上自动再试一次…"
                DispatchQueue.main.asyncAfter(deadline: .now() + 1.5) { [weak self] in
                    guard let self = self, seq == self.planSeq else { return }
                    self.replan(retry: retry - 1)
                }
            } else {
                planFailed = true
                info = "规划路线没能开始（还没定位？检查定位权限和高德 key）"
            }
            return
        }
        DispatchQueue.main.asyncAfter(deadline: .now() + 20) { [weak self] in
            guard let self = self, seq == self.planSeq, self.calculating else { return }
            self.calculating = false
            self.planFailed = true
            self.info = "规划超时"
        }
    }

    /// 选一条路线（开始导航时用这条）
    func select(routeID: Int) {
        selectedRouteID = routeID
        _ = manager.selectNaviRoute(withRouteID: routeID)
        if let o = routeOptions.first(where: { $0.id == routeID }) {
            routeSummary = "\(NaviBridge.distText(o.length))，约 \(NaviBridge.durText(o.timeS))"
            MapFeeder.shared.prepareRoute(key: "selected-\(routeID)", coords: o.coords.map { (lat: $0.latitude, lng: $0.longitude) })
        }
        checkShareMatch()
    }

    static func distText(_ m: Int) -> String { m >= 1000 ? String(format: "%.1f 公里", Double(m) / 1000) : "\(m) 米" }
    static func durText(_ s: Int) -> String {
        let m = max(1, (s + 30) / 60)
        return m >= 60 ? "\(m / 60) 小时 \(m % 60) 分" : "\(m) 分钟"
    }

    /// 比较 SDK 算出的路线和分享的路线，结果写在 shareMatch
    private func checkShareMatch() {
        guard let sr = shareRoute, let o = routeOptions.first(where: { $0.id == selectedRouteID }) else { return }
        let me = myPosition()
        let sdk = o.coords.map { (lat: $0.latitude, lng: $0.longitude) }
        let seq = planSeq
        shareMatch = "正在比较和分享路线的吻合程度…"
        DispatchQueue.global(qos: .userInitiated).async {
            let r = AmapShare.match(sr, sdk: sdk, myLat: me?.lat ?? .nan, myLng: me?.lng ?? .nan)
            DispatchQueue.main.async {
                guard seq == self.planSeq else { return }
                let pct = Int(r.ratio * 100)
                self.shareMatch = pct >= 95 ? "✓ 和分享的路线吻合 \(pct)%"
                    : "⚠ 和分享的路线吻合 \(pct)%，约 \(NaviBridge.distText(r.bad)) 不一样（地图上橙色虚线是原路线）"
            }
        }
    }

    func start(emulate: Bool) {
        guard routeReady, !navigating, !WebMotorSession.shared.running else { return }
        attach()
        if cruising { endCruiseQuietly() }
        // Stop a previous arrival hold before attaching a new trip's callbacks.
        arrival.cancel(arrival.generation)
        manager.stopNavi()
        if selectedRouteID >= 0 { _ = manager.selectNaviRoute(withRouteID: selectedRouteID) }
        resetState()
        invalidateArrival()
        let token = arrival.begin(simulation: emulate)
        installCallbacks()
        navigating = true // Arm before start: SDK callbacks may be synchronous.
        BleLink.shared.send("C")
        MapFeeder.shared.onNaviStart()
        let me = myPosition()
        ShareGuide.shared.start(shareRoute, avoidHighway: Settings.shared.avoidHighway, emulate: emulate,
                                myLat: me?.lat ?? .nan, myLng: me?.lng ?? .nan)          // 偏航跟随原路线 + 圆屏原路线细线
        emulating = emulate; emuPaused = false
        if emulate { emuSpeed = NavigationSpeed.values.contains(preferredSimulationSpeed) ? preferredSimulationSpeed : 60; manager.setEmulatorNaviSpeed(Int32(emuSpeed)) }
        let ok = emulate ? manager.startEmulatorNavi() : manager.startGPSNavi()
        if ok && arrival.acceptsGuidance(token) {
            if !emulate { RideRecorder.shared.start("导航"); LocationService.shared.setPrecise(true) }
            info = emulate ? "模拟导航中" : "导航中"
            tick?.invalidate()
            tick = Timer.scheduledTimer(withTimeInterval: NaviBridge.TICK, repeats: true) { [weak self] _ in
                guard let self = self, self.arrival.acceptsGuidance(token) else { return }
                self.onTick()
            }
        } else if !ok && arrival.acceptsGuidance(token) {
            finish("C", "导航没能开始")
        }
    }

    /// 模拟导航：暂停 / 继续
    func toggleEmuPause() {
        guard navigating, emulating else { return }
        emuPaused.toggle()
        if emuPaused { manager.pauseNavi(); speedMps = 0 } else { manager.resumeNavi() }   // 暂停时不再按车速往前推算距离
    }

    /// 模拟导航车速：40 → 60 → 100 → 150 km/h 循环
    func cycleEmuSpeed() {
        guard navigating, emulating else { return }
        let all = NaviBridge.EMU_SPEEDS
        emuSpeed = all[((all.firstIndex(of: emuSpeed) ?? 0) + 1) % all.count]
        manager.setEmulatorNaviSpeed(Int32(emuSpeed))
    }

    func stop() {
        // stopNavi may emit emulator-end immediately. Close the gate first.
        arrival.cancel(arrival.generation)
        manager.stopNavi()
        finish("C", "已退出导航")
    }

    private func finish(_ msg: String, _ text: String, event: ArrivalPolicy.EndEvent? = nil) {
        let token = arrival.generation
        let arrived = msg == "A,0"
        if arrived {
            guard let event = event, arrival.complete(token, event: event) else { return }
        } else { arrival.cancel(token) }
        RideRecorder.shared.finish()
        LocationService.shared.setPrecise(false)
        MapFeeder.shared.onNaviEnd()
        ShareGuide.shared.stop()
        tick?.invalidate(); tick = nil
        arriveSeq += 1
        showArrived = arrived                       // 先设这个再设 navigating：导航画面据此决定马上关还是等 4 秒
        navigating = false
        info = text
        shownDir = ""; shownDist = -1
        clearSuggest()
        if arrived {
            let minutes = max(1,Int(Date().timeIntervalSince(tripStarted)/60))
            BleLink.shared.send("#O\(Int(tripMeters))/\(minutes)/\(tripMaxKmh)")
        }
        send(msg)
        if arrived {
            // 和安卓一样：圆屏显示“已到达”4 秒，然后清屏回到待机（测速牌、车道、电子眼这些都清掉）
            let seq = arriveSeq
            DispatchQueue.main.asyncAfter(deadline: .now() + 4) { [weak self] in
                guard let self = self, seq == self.arriveSeq, self.arrival.canClearArrival(token),
                      !self.navigating, !self.cruising else { return }
                self.manager.stopNavi()
                self.resetState()
                self.showArrived = false
                self.lastSent = "C"
                BleLink.shared.send("C")
            }
        }
    }

    private func resetState() {
        tripMeters = 0; tripMaxKmh = 0; tripStarted = Date(); tripPosition = nil
        curDir = ""; curAngle = nil; angleKey = ""; lastRouteKey = ""
        baseDist = 0; speedMps = 0
        shownDir = ""; shownDist = -1; lastSent = ""
        lastRoadText = ""; lastBottomText = ""; lastTrip = ""; lastSpeedMsg = ""; rerouting = false
        lightBase = -1; lastLightMsg = ""
        lastCamMsg = ""; lastCamAt = .distantPast; camLimit = 0; camMinDist = Int.max; lastCamMapMsg = ""
        camPts = []; cruiseCamPts = []; facPts = [:]; navFacPts = []; routeFacKey = ""; uiRemainM = 0; uiRemainS = 0
        lastLaneMsg = ""; lastTrafficMsg = ""; lastTrafficAt = .distantPast; gpsWeakSent = false
        suggestRoute = nil; uiSuggestMin = 0; suggestTimer?.invalidate()
        uiDir = ""; uiDist = -1; uiRoad = ""; extraInfo = ""
    }

    /// 高德转向图标编号 → 圆屏方向代码（和安卓版一样）
    static func iconToDir(_ icon: Int) -> String {
        switch icon {
        case 2, 6, 21: return "L"       // 左转、左后方、环岛左转
        case 3, 7, 22: return "R"       // 右转、右后方、环岛右转
        case 4, 65: return "SL"         // 左前方、靠左
        case 5, 66: return "SR"         // 右前方、靠右
        case 8, 19, 24: return "U"      // 掉头
        case 15: return "A"             // 前方终点提示；不是已到达，发往圆屏时转换为 S
        default: return "S"             // 直行及其他
        }
    }

    private func roundDist(_ d: Int) -> Int { d >= 1000 ? (d + 50) / 100 * 100 : d }

    private func send(_ msg: String) {
        if msg != lastSent { lastSent = msg; BleLink.shared.send(msg) }
    }

    private func pushNav(_ dir: String, _ dist: Int, real: Bool) {
        guard arrival.acceptsGuidance(arrival.generation) else { return }
        let d = roundDist(dist)
        // 同一方向下，推算值不让数字往回跳（真实数据来了允许小幅修正）
        if dir == shownDir && shownDist >= 0 && d > shownDist {
            if !real || d - shownDist < 20 { return }
        }
        shownDir = dir; shownDist = d
        uiDir = dir; uiDist = d
        // Reserve wire A,0 for a real terminal callback, even when prediction reaches zero.
        let wireDir = ArrivalPolicy.wireDirection(dir)
        send(curAngle != nil ? "\(wireDir),\(d),\(curAngle!)" : "\(wireDir),\(d)")
    }

    /// 两次真实数据之间按车速推算距离；顺便检查测速提示是否过期、每分钟刷新路况
    private func onTick() {
        guard arrival.acceptsGuidance(arrival.generation) else { return }
        let el = Date().timeIntervalSince(baseAt)
        if speedMps > 0.5 && el < NaviBridge.MAX_PREDICT && !curDir.isEmpty {
            pushNav(curDir, max(0, Int((Double(baseDist) - speedMps * el).rounded())), real: false)
            if lightBase >= 0 { pushLight(max(0, Int((Double(lightBase) - speedMps * Date().timeIntervalSince(lightAt)).rounded()))) }
        }
        trackCamera()
        if Date().timeIntervalSince(lastTrafficAt) > 60 { pushTraffic(nil) }
    }

    // MARK: - 路口实际转弯角度

    /// 方位角：0 = 正北，顺时针为正（度）
    private static func bearing(_ lat1: Double, _ lng1: Double, _ lat2: Double, _ lng2: Double) -> Double {
        let k = Double.pi / 180
        let x = (lng2 - lng1) * cos((lat1 + lat2) / 2 * k)
        return atan2(x, lat2 - lat1) * 180 / .pi
    }

    /// 两点间距离（米），短距离用平面近似足够
    static func distM(_ lat1: Double, _ lng1: Double, _ lat2: Double, _ lng2: Double) -> Double {
        let k = Double.pi / 180
        let x = (lng2 - lng1) * k * cos((lat1 + lat2) / 2 * k)
        let y = (lat2 - lat1) * k
        return (x * x + y * y).squareRoot() * 6_371_000.0
    }

    /// 用路线坐标算当前路段终点（下一个路口）的转弯角度：进路口前 30 米的方向 → 出路口后 30 米的方向。
    /// 负数 = 向左，正数 = 向右。只对左转/右转/左前/右前计算；算不出来或方向对不上时返回 nil（圆屏用默认角度）。
    private func turnAngle(step: Int, icon: Int, dir: String) -> Int? {
        if !["L", "R", "SL", "SR"].contains(dir) { return nil }
        let fallback: Int? = icon == 6 ? -135 : (icon == 7 ? 135 : nil)   // 左后方 / 右后方
        guard let segs = manager.naviRoute?.routeSegments, step >= 0, step + 1 < segs.count else { return fallback }
        let inC = segs[step].coordinates, outC = segs[step + 1].coordinates
        if inC.count < 2 || outC.count < 2 { return fallback }
        func ll(_ p: AMapNaviPoint) -> (Double, Double) { (Double(p.latitude), Double(p.longitude)) }
        let j = ll(inC[inC.count - 1])
        // 往回走 30 米找进路口方向的起点
        var a = ll(inC[inC.count - 2]), acc = 0.0
        for i in stride(from: inC.count - 1, through: 1, by: -1) {
            let p = ll(inC[i]), q = ll(inC[i - 1])
            acc += NaviBridge.distM(p.0, p.1, q.0, q.1)
            a = q
            if acc >= 30 { break }
        }
        // 往前走 30 米找出路口方向的终点
        var b = ll(outC[1]); acc = 0
        for i in 0..<(outC.count - 1) {
            let p = ll(outC[i]), q = ll(outC[i + 1])
            acc += NaviBridge.distM(p.0, p.1, q.0, q.1)
            b = q
            if acc >= 30 { break }
        }
        var t = NaviBridge.bearing(j.0, j.1, b.0, b.1) - NaviBridge.bearing(a.0, a.1, j.0, j.1)
        while t > 180 { t -= 360 }
        while t <= -180 { t += 360 }
        let left = dir == "L" || dir == "SL"
        // 方向对不上（比如高德说左转，几何上却是右转）就不用
        if left && t > -10 { return fallback }
        if !left && t < 10 { return fallback }
        let mag = min(150, max(20, abs(t)))
        let r = Int((mag / 5).rounded()) * 5                 // 取到 5 度，避免箭头抖动
        return left ? -r : r
    }

    // MARK: - 路名、底部信息、行程

    private func updateTexts(dir: String, curRoad: String, nextRoad: String, remainM: Int, remainS: Int, lights: Int) {
        let road: String
        if dir == "A" { road = "前方为目的地" }
        else if dir == "S" && !curRoad.isEmpty { road = "沿 \(curRoad) 直行" }
        else if !nextRoad.isEmpty { road = "进入 \(nextRoad)" }
        else { road = "" }
        uiRoad = road
        if road != lastRoadText {
            lastRoadText = road
            TextImage.send(slot: 0, road, maxW: 290, size: 26, minSize: 18)
        }
        let km = remainM >= 1000 ? String(format: "%.1fkm", Double(remainM) / 1000) : "\(remainM)m"
        let eta = Calendar.current.date(byAdding: .second, value: remainS, to: Date()) ?? Date()
        let hh = Calendar.current.component(.hour, from: eta), mm = Calendar.current.component(.minute, from: eta)
        var bottom = String(format: "%@ · %02d:%02d到达", km, hh, mm)
        if lights > 0 { bottom += " · \(lights)个红绿灯" }
        if bottom != lastBottomText {
            lastBottomText = bottom
            TextImage.send(slot: 1, bottom, maxW: 240, size: 19, minSize: 15)
        }
        let rm = remainM >= 1000 ? (remainM + 50) / 100 * 100 : remainM
        let trip = "#e,\(rm),\(hh * 100 + mm),\(max(0, lights))"
        if trip != lastTrip { lastTrip = trip; BleLink.shared.send(trip) }
    }

    // MARK: - 前方红绿灯

    /// 算到前方下一个红绿灯还有多少米（1 公里内找不到返回 nil）。
    /// 高德路线由“路段(segment) → 小段(link)”组成，每个 link 标明终点有没有红绿灯；
    /// 当前 link 剩余长度 = 当前路段剩余距离 − 本路段后面几个 link 的长度。
    private func nextLightDist(step: Int, curLink: Int, stepRemain: Int) -> Int? {
        guard curLink >= 0, let segs = manager.naviRoute?.routeSegments, step >= 0, step < segs.count else { return nil }
        var ls = segs[step].links
        if curLink >= ls.count { return nil }
        var after = 0
        for k in (curLink + 1)..<max(curLink + 1, ls.count) { after += ls[k].length }
        var acc = max(0, stepRemain - after)
        if ls[curLink].isHadTrafficLights { return acc }
        var s = step, k = curLink + 1
        while acc <= 1000 {
            if k >= ls.count {
                s += 1
                if s >= segs.count { return nil }
                ls = segs[s].links
                k = 0
                continue
            }
            acc += ls[k].length
            if ls[k].isHadTrafficLights { return acc }
            k += 1
        }
        return nil
    }

    /// 整条路线上所有红绿灯的位置：优先用高德直接给的，没有就取“有红绿灯的小段”的终点
    private func routeLights(_ r: AMapNaviRoute) -> [(lat: Double, lng: Double)] {
        if let pts = r.routeTrafficLights, !pts.isEmpty { return pts.map { (Double($0.latitude), Double($0.longitude)) } }
        var out: [(lat: Double, lng: Double)] = []
        for s in r.routeSegments {
            for l in s.links where l.isHadTrafficLights {
                if let p = l.coordinates.last { out.append((Double(p.latitude), Double(p.longitude))) }
            }
        }
        return out
    }

    private func pushLight(_ d: Int) {
        let show = d >= 0 && d <= NaviBridge.LIGHT_SHOW_M
        let m = show ? "#g,\((d + 5) / 10 * 10)" : "#g"
        if m != lastLightMsg { lastLightMsg = m; BleLink.shared.send(m) }
    }

    // MARK: - 测速

    /// 每 0.2 秒：用当前位置算到测速点的直线距离，经过之后自动消失
    private func trackCamera() {
        if camLimit <= 0 { return }
        if Date().timeIntervalSince(lastCamAt) > NaviBridge.CAMERA_TIMEOUT { clearCamera(); return }
        if camLat == 0 || myLat.isNaN { return }
        let d = Int(NaviBridge.distM(myLat, myLng, camLat, camLng).rounded())
        if d > cameraRange + 150 { clearCamera(); return }
        // 离测速点最近时不到 60 米，之后又远离 30 米以上 → 已经经过
        if camMinDist < 60 && d > camMinDist + 30 { clearCamera(); return }
        updateCamDist(d)
    }

    private func updateCamDist(_ d: Int) {
        if d < camMinDist { camMinDist = d }
        let m = "#k,\(camLimit),\((d + 5) / 10 * 10)"
        if m != lastCamMsg { lastCamMsg = m; BleLink.shared.send(m) }
    }

    private func clearCamera() {
        camLimit = 0
        camMinDist = Int.max
        if lastCamMsg != "#k" { lastCamMsg = "#k"; BleLink.shared.send("#k") }
    }

    /// 电子眼类型 → 圆屏图标编号（和固件 CAM_ICONS 一致）：
    /// 0 电子眼 1 监控 2 测速 3 闯红灯 4 违章拍照 5 公交道 6 应急车道 7 非机动车道 8 区间起点 9 区间终点 10 流动测速 11 ETC
    static func camIcon(_ type: Int) -> Int {
        [0: 2, 1: 1, 2: 3, 3: 4, 4: 5, 5: 6, 6: 7, 8: 8, 9: 9, 10: 10, 11: 11][type] ?? 0
    }
    private static let CAM_NAMES = ["电子眼", "监控", "测速", "闯红灯", "违章拍照", "公交道", "应急车道", "非机动车道",
                                    "区间起点", "区间终点", "流动测速", "ETC"]

    /// 高德道路设施类型（AMapNaviRoadFacilityType）→ 圆屏图标（16 急弯 18 事故多发 21 村庄 23 落石 25 注意）；限速标志、货车类的不画
    private static func roadFacIcon(_ t: Int) -> Int? {
        switch t {
        case 3, 4, 5: return 16                 // 急转弯、反向转弯、连续转弯
        case 6: return 18                       // 事故多发地
        case 7: return 23                       // 注意落石
        case 12: return 21                      // 村庄
        case 1, 2, 8, 9, 13, 14, 15, 16: return 25   // 合流、铁路道口、易滑、变窄、横风区
        default: return nil
        }
    }

    /// 导航中：路线变了就算一遍路线上每个道路设施的位置（离起点多远）；每次导航信息挑出前方 1 公里内的画在小地图上
    private func updateRouteFacilities(_ r: AMapNaviRoute, key: String, remain: Int) {
        if key != routeFacKey {
            routeFacKey = key
            routeFacs = []
            let pts = r.routeCoordinates
            if pts.count >= 2, let facs = r.roadFacilityInfo, !facs.isEmpty {
                var cum = [Double](repeating: 0, count: pts.count)
                for i in 1..<pts.count {
                    cum[i] = cum[i - 1] + NaviBridge.distM(Double(pts[i - 1].latitude), Double(pts[i - 1].longitude),
                                                         Double(pts[i].latitude), Double(pts[i].longitude))
                }
                for f in facs {
                    guard let icon = NaviBridge.roadFacIcon(f.type.rawValue), let c = f.coordinate else { continue }
                    let la = Double(c.latitude), lo = Double(c.longitude)
                    var best = Double.greatestFiniteMagnitude, bi = 0
                    for i in 0..<pts.count {
                        let d = NaviBridge.distM(la, lo, Double(pts[i].latitude), Double(pts[i].longitude))
                        if d < best { best = d; bi = i }
                    }
                    if best < 80 { routeFacs.append((cum[bi], la, lo, icon)) }
                }
            }
        }
        let done = Double(r.routeLength - remain)          // 已经走了多远
        var pts: [MapPt] = []
        for f in routeFacs {
            let ahead = f.along - done
            if ahead < -20 || ahead > 1000 { continue }
            guard let p = MapFeeder.shared.toFrame(f.lat, f.lng) else { continue }
            pts.append(MapPt(x: p.x, y: p.y, spd: 0, icon: f.icon, dist: max(0, Int(ahead))))
        }
        let changed = pts.map { "\($0.x),\($0.y)" } != navFacPts.map { "\($0.x),\($0.y)" }
        navFacPts = pts
        if changed { pushCamMap() }
    }

    /// 前方 1 公里内的电子眼（和巡航时的道路安全提示）画在小地图上："#c,坐标系,x,y,限速,图标,..."（最近的 10 个）
    private func pushCamMap() {
        let fid = MapFeeder.shared.frame?.id
        let all = (camPts + cruiseCamPts + Array(facPts.values) + navFacPts).sorted { ($0.dist < 0 ? 99_999 : $0.dist) < ($1.dist < 0 ? 99_999 : $1.dist) }.prefix(10)
        var m = "#c"
        if let fid = fid, !all.isEmpty {
            m += ",\(fid)"
            for p in all { m += ",\(p.x),\(p.y),\(p.spd),\(p.icon)" }
        }
        if m != lastCamMapMsg { lastCamMapMsg = m; BleLink.shared.send(m) }
    }

    // MARK: - 车道

    /// 高德的车道字符串（"1|0|0|4" 背景、"255|0|0|0" 推荐）→ "#j,1f00004…"（每条车道两个十六进制数）
    private func onLanes(back: String, select: String) {
        let bg = back.split(separator: "|").map { Int($0.trimmingCharacters(in: .whitespaces)) ?? 255 }
        let fr = select.split(separator: "|").map { Int($0.trimmingCharacters(in: .whitespaces)) ?? 255 }
        var n = bg.firstIndex(of: 255) ?? bg.count
        n = min(n, 8)
        var sb = ""
        for i in 0..<n {
            let b = bg[i], f = i < fr.count ? fr[i] : 255
            sb += (0...14).contains(b) ? String(b, radix: 16) : "f"
            sb += (0...14).contains(f) ? String(f, radix: 16) : "f"
        }
        let m = n > 0 ? "#j,\(sb)" : "#j"
        laneRaw = "车道 \(n) 条：背景 \(back)，推荐 \(select)（编码 \(sb)）"
        if m != lastLaneMsg { lastLaneMsg = m; BleLink.shared.send(m) }
    }

    // MARK: - 路况、重新规划、GPS、更快路线

    /// 路况：只发不畅通的路段（2 缓行、3 拥堵、4 严重拥堵）："#u,起点米,终点米,状态,..."（米 = 离路线起点）
    private func pushTraffic(_ list: [AMapNaviTrafficStatus]?) {
        lastTrafficAt = Date()
        guard let list = list ?? manager.getTrafficStatuses() else { return }
        var segs: [[Int]] = []
        var pos = 0
        for o in list {
            let len = o.length, st = o.status.rawValue
            if st >= 2 && st <= 4 && len > 0 {
                if let last = segs.last, last[2] == st, last[1] >= pos - 5 { segs[segs.count - 1][1] = pos + len }
                else { segs.append([pos, pos + len, st]) }
            }
            pos += len
        }
        var m = "#u"
        for sg in segs {
            let part = ",\(sg[0]),\(sg[1]),\(sg[2])"
            if m.count + part.count > 480 { break }
            m += part
        }
        if m != lastTrafficMsg { lastTrafficMsg = m; BleLink.shared.send(m) }
    }

    /// 圆屏上显示 / 收起 “REROUTING”（重新规划中，只在导航进行中）
    func setRerouting(_ on: Bool) {
        if on && !navigating { return }
        if on == rerouting { return }
        rerouting = on
        BleLink.shared.send(on ? "#y,1" : "#y,0")
    }

    private func setGpsWeak(_ w: Bool) {
        if w == gpsWeakSent { return }
        gpsWeakSent = w
        BleLink.shared.send(w ? "#x,1" : "#x,0")
        extraInfo = w ? "GPS 信号弱" : "GPS 信号恢复"
    }

    /// 导航画面上点了“换这条”：切到高德建议的更快路线
    @discardableResult
    func acceptSuggest() -> Bool {
        defer { clearSuggest() }
        guard let r = suggestRoute else { extraInfo = "更快路线已经失效"; return false }
        let id = (manager.naviRoutes ?? [:]).first { $0.value === r || $0.value.routeUID == r.routeUID }?.key
        guard let rid = id, manager.selectNaviRoute(withRouteID: rid.intValue) else {
            extraInfo = "没能切换路线"
            return false
        }
        extraInfo = "已换到更快的路线"
        return true
    }

    func clearSuggest() {
        suggestTimer?.invalidate(); suggestTimer = nil
        suggestRoute = nil
        if uiSuggestMin == 0 { return }
        uiSuggestMin = 0
        BleLink.shared.send("#h")
    }

    // MARK: - 巡航模式（iOS v0.3，和安卓 v4.7 一样）

    /// 开始巡航（不设目的地）：返回 nil = 成功，否则是出错原因
    func startCruise() -> String? {
        if navigating || WebMotorSession.shared.running { return "导航中不能开巡航，先退出导航" }
        if cruising { return nil }
        attach()
        arrival.cancel(arrival.generation)
        manager.stopNavi()
        invalidateArrival()
        arrival.beginCruise()
        installCallbacks()
        resetState()
        MapFeeder.shared.onNaviStart()
        manager.detectedMode = .cameraAndSpecialRoad          // 电子眼和特殊道路设施都提醒
        cruising = true
        RideRecorder.shared.start("巡航")
        cruiseIdle.start()
        cruiseRaw = "巡航已开始，等高德发来数据…"
        info = "巡航中"
        BleLink.shared.send("#z,1")
        LocationService.shared.setPrecise(true)               // 位置用手机定位（锁屏也继续）
        cruiseTick?.invalidate()
        cruiseTick = Timer.scheduledTimer(withTimeInterval: 0.3, repeats: true) { [weak self] _ in self?.onCruiseTick() }
        return nil
    }

    /// 结束巡航，圆屏回到首页
    func stopCruise() {
        if !cruising { return }
        endCruiseQuietly()
        resetState()
        BleLink.shared.send("C")
        info = "巡航已结束"
    }

    private func endCruiseQuietly() {
        if !cruising { return }
        arrival.cancel(arrival.generation)
        cruising = false
        RideRecorder.shared.finish()
        cruiseIdle.stop()
        MapFeeder.shared.onNaviEnd()
        cruiseTick?.invalidate(); cruiseTick = nil
        manager.detectedMode = .none
        LocationService.shared.setPrecise(false)
        BleLink.shared.send("#z,0")
    }

    /// 巡航时的手机定位（WGS-84）→ 小地图位置、车速、测速牌跟踪
    func onCruiseLocation(_ l: CLLocation) {
        if cruising { cruiseIdle.onFix(l) }
        if l.horizontalAccuracy < 0 || l.horizontalAccuracy > 100 { return }
        let g = MapGeo.wgsToGcj(l.coordinate.latitude, l.coordinate.longitude)
        myLat = g.lat; myLng = g.lng
        let kmh = l.speed >= 0 ? Int((l.speed * 3.6).rounded()) : 0
        speedKmhNow = kmh; uiSpeed = kmh
        if l.course >= 0 { cruiseHeading = l.course }
        MapFeeder.shared.onLocation(lat: g.lat, lng: g.lng, bearing: cruiseHeading, kmh: kmh)
        let msg = "#v,\(kmh)"
        if msg != lastSpeedMsg { lastSpeedMsg = msg; BleLink.shared.send(msg) }
    }

    private func onCruiseTick() {
        if cruiseIdle.shouldStop() { stopCruise(); info = "连续停车 10 分钟，巡航已自动结束"; return }
        trackCamera()
        let now = Date()
        let before = facPts.count
        facPts = facPts.filter { now.timeIntervalSince($0.value.at) <= 20 }
        var changed = facPts.count != before
        if let f = cruiseCamPts.first, now.timeIntervalSince(f.at) > 20 { cruiseCamPts = []; changed = true }
        if changed { pushCamMap() }
    }

    /// 巡航电子眼 / 道路设施：取出位置、类型、限速、距离，合并后发给圆屏
    fileprivate func onCruiseItems(_ items: [AMapNaviTrafficFacilityInfo], camerasOnly camCb: Bool, name: String) {
        let f = DateFormatter(); f.dateFormat = "HH:mm:ss"
        if items.isEmpty {
            if camCb { cruiseCamPts = [] } else { facPts = [:] }
            pushCamMap()
            cruiseRaw = "\(name)：0 个（\(f.string(from: Date()))）"
            return
        }
        let now = Date()
        var raw = ""
        var cams: [MapPt] = []
        var best: MapPt?, bestLat = 0.0, bestLng = 0.0
        if !camCb { facPts = [:] }
        for o in items {
            let type = o.boardcastType
            let spd = max(0, o.limitSpeed), dist = o.distance
            let asCam = camCb || FacTypes.isCamera(type)
            if raw.count < 260 { raw += "[\(asCam ? "眼" : FacTypes.name(type)) 类型\(type) \(spd > 0 ? "限速\(spd) " : "")\(dist)米] " }
            guard let c = o.coordinate, c.latitude != 0, dist <= 1000 else { continue }
            let lat = Double(c.latitude), lng = Double(c.longitude)
            guard let p = MapFeeder.shared.toFrame(lat, lng) else { continue }
            if asCam {
                let mp = MapPt(x: p.x, y: p.y, spd: spd, icon: FacTypes.camIcon(type, spd: spd), dist: dist, at: now)
                cams.append(mp)
                if spd > 0 && dist >= 0 && dist <= cameraRange && (best == nil || dist < best!.dist) { best = mp; bestLat = lat; bestLng = lng }
            } else {
                facPts["\(p.x / 5),\(p.y / 5),\(type)"] = MapPt(x: p.x, y: p.y, spd: 0, icon: FacTypes.icon(type), dist: dist, at: now)
            }
        }
        if camCb || !cams.isEmpty {
            cruiseCamPts = cams
            // 巡航中也显示测速牌（导航中由 updateCameraInfos 负责）
            if let b = best {
                lastCamAt = Date()
                if b.spd != camLimit || camLat != bestLat || camLng != bestLng { camMinDist = Int.max }
                camLimit = b.spd; camLat = bestLat; camLng = bestLng
                updateCamDist(b.dist)
            }
        }
        cruiseRaw = "\(name)：\(items.count) 个 \(raw)（\(f.string(from: Date()))）"
        pushCamMap()
    }
}

/// 道路设施类型（高德 iOS SDK 的 boardcastType）→ 圆屏图标（16~25，和固件 CAM_ICONS 一致）：
///   16 急弯  17 学校  18 事故多发  19 注意行人  20 陡坡  21 村庄  22 窄桥  23 落石  24 隧道  25 注意（其他）
/// 摄像头类的设施按电子眼画：2 测速 1 监控 3 闯红灯 4 违章拍照 5 公交道 6 应急车道 7 非机动车道
enum FacTypes {
    private static let cams: [Int: Int] = [4: 2, 5: 4, 28: 1, 29: 5, 92: 3, 93: 6, 94: 7]
    static func isCamera(_ t: Int) -> Bool { cams[t] != nil }
    static func camIcon(_ t: Int, spd: Int) -> Int { spd > 0 ? 2 : (cams[t] ?? 0) }
    static func icon(_ t: Int) -> Int {
        switch t {
        case 22, 23, 24, 25: return 16          // 向左 / 向右急弯、反向弯路、连续弯路
        case 18: return 17                      // 学校
        case 14, 53: return 18                  // 事故易发、事故多发
        case 47, 48: return 20                  // 上 / 下陡坡
        case 16: return 21                      // 村庄
        case 38: return 22                      // 窄桥
        case 13, 42: return 23                  // 左侧 / 右侧落石
        case 58: return 24                      // 隧道
        default: return 25
        }
    }
    private static let ZH = ["急弯", "学校", "事故多发", "注意行人", "陡坡", "村庄", "窄桥", "落石", "隧道", "注意"]
    static func name(_ t: Int) -> String { ZH[icon(t) - 16] }
}

// MARK: - 高德回调
// 这些回调都写明了 Objective-C 方法名（@objc(...)），高德按这个名字找到并调用。
// 注意这里故意不写 “: AMapNaviDriveManagerDelegate” —— 写了的话 Swift 会要求方法名和 SDK 的 Swift 名字一字不差，
// 否则报 “Objective-C method ... provided by method ...” 冲突。协议在 NaviSessionCallbacks 中登记，
// 由带有会话编号的代理在主线程检查后转发；这里不再直接注册成 SDK 代理。

extension NaviBridge {

    // 路线规划成功（老版本 / 新版本 SDK 各一个名字）；导航中偏航重算成功也会走这里
    @objc(driveManagerOnCalculateRouteSuccess:)
    func mnRouteSuccessOld(_ m: AMapNaviDriveManager) { routeDone() }
    @objc(driveManager:onCalculateRouteSuccessWithType:)
    func mnRouteSuccessNew(_ m: AMapNaviDriveManager, type: Int) { routeDone() }

    private func routeDone() {
        // 高德新旧两种“算路成功”回调可能都来一次：只处理第一次
        if Date().timeIntervalSince(lastRouteDoneAt) < 0.5 { return }
        lastRouteDoneAt = Date()
        if navigating {                              // 导航中的重新规划（偏航、拥堵、按分享路线回去）
            setRerouting(false)
            ShareGuide.shared.onCalcDone(ok: true)
            return
        }
        if !calculating && routeReady { return }     // 新旧两种回调可能都来一次，只处理第一次
        if destLat.isNaN { return }                  // 规划途中点了“换目的地”：结果不要了
        calculating = false
        routeReady = true
        planFailed = false
        let routes = manager.naviRoutes ?? [:]
        let ids = (manager.naviRouteIDs ?? []).map { $0.intValue }.filter { routes[NSNumber(value: $0)] != nil }
        var opts: [RouteOption] = []
        for (i, id) in (ids.isEmpty ? [manager.naviRouteID] : ids).prefix(3).enumerated() {
            guard let r = routes[NSNumber(value: id)] ?? (id == manager.naviRouteID ? manager.naviRoute : nil) else { continue }
            let label = r.routeLabels?.first(where: { !$0.content.isEmpty })?.content ?? (i == 0 ? "推荐" : "方案\(i + 1)")
            opts.append(RouteOption(id: id, label: label, timeS: r.routeTime, length: r.routeLength,
                                    lights: r.routeTrafficLightCount, toll: r.routeTollCost,
                                    coords: r.routeCoordinates.map { CLLocationCoordinate2D(latitude: Double($0.latitude), longitude: Double($0.longitude)) }))
        }
        routeOptions = opts
        info = opts.count > 1 ? "规划好了 \(opts.count) 条路线，点卡片选一条" : "路线规划好了"
        if let f = opts.first { select(routeID: f.id) }
    }

    @objc(driveManager:onCalculateRouteFailure:)
    func mnRouteFail(_ m: AMapNaviDriveManager, error: NSError?) {
        if navigating {
            setRerouting(false)
            extraInfo = "重新规划失败：\(error?.localizedDescription ?? "未知原因")"
            ShareGuide.shared.onCalcDone(ok: false)
            return
        }
        calculating = false
        routeReady = false
        planFailed = true
        info = "路线规划失败：\(error?.localizedDescription ?? "未知原因")（\(error?.code ?? 0)）"
    }

    // 播报文字 → 我们自己念（压低音乐）
    @objc(driveManager:playNaviSoundString:soundStringType:)
    func mnSound(_ m: AMapNaviDriveManager, text: String?, type: Int) {
        if let t = text { Speech.shared.speak(t) }
    }

    @objc(driveManagerIsNaviSoundPlaying:)
    func mnSoundPlaying(_ m: AMapNaviDriveManager) -> Bool { Speech.shared.isSpeaking }

    @objc(driveManagerOnArrivedDestination:)
    func mnArrived(_ m: AMapNaviDriveManager) { finish("A,0", "已到达目的地", event: .destination) }

    @objc(driveManagerDidEndEmulatorNavi:)
    func mnEmuEnd(_ m: AMapNaviDriveManager) { finish("A,0", "模拟导航结束", event: .emulator) }

    @objc(driveManagerNeedRecalculateRouteForYaw:)
    func mnYaw(_ m: AMapNaviDriveManager) { setRerouting(true); ShareGuide.shared.onYaw() }

    @objc(driveManagerNeedRecalculateRouteForTrafficJam:)
    func mnJam(_ m: AMapNaviDriveManager) { setRerouting(true) }

    // GPS 信号强弱：2 = 弱
    @objc(driveManager:updateGPSSignalStrength:)
    func mnGps(_ m: AMapNaviDriveManager, strength: Int) { setGpsWeak(strength == 2) }

    // 高德发现更快的路线
    @objc(driveManager:onSuggestChangeMainNaviRoute:)
    func mnSuggest(_ m: AMapNaviDriveManager, info s: AMapNaviSuggestChangeMainNaviRouteInfo?) {
        guard let s = s, let r = s.backupRoute else { return }
        let min = max(1, (s.savingTime + 30) / 60)
        suggestRoute = r
        uiSuggestMin = min
        BleLink.shared.send("#h,\(min)")
        extraInfo = "发现更快路线，能省约 \(min) 分钟"
        suggestTimer?.invalidate()
        suggestTimer = Timer.scheduledTimer(withTimeInterval: 30, repeats: false) { [weak self] _ in self?.clearSuggest() }
    }

    // 每秒一次的导航信息
    @objc(driveManager:updateNaviInfo:)
    func mnNaviInfo(_ m: AMapNaviDriveManager, info n: AMapNaviInfo?) {
        guard arrival.acceptsGuidance(arrival.generation), let n = n else { return }
        let icon = n.iconType.rawValue
        let dir = NaviBridge.iconToDir(icon)
        let step = n.currentSegmentIndex
        curDir = dir
        // 路线变化（新规划、偏航重算）时把整条路线连同红绿灯发给小地图
        if let r = manager.naviRoute {
            let rk = "\(manager.naviRouteID):\(r.routeUID):\(r.routeLength)"
            if rk != lastRouteKey {
                let coords = r.routeCoordinates.map { (lat: Double($0.latitude), lng: Double($0.longitude)) }
                if MapFeeder.shared.onRoute(key: rk, coords: coords, lights: routeLights(r)) {
                    lastRouteKey = rk
                    pushTraffic(nil)                        // 新路线：路况也重新发
                }
            }
            let ak = "\(rk):\(step):\(icon)"
            if ak != angleKey { angleKey = ak; curAngle = turnAngle(step: step, icon: icon, dir: dir) }
            updateRouteFacilities(r, key: rk, remain: n.routeRemainDistance)
        }
        uiRemainM = n.routeRemainDistance; uiRemainS = n.routeRemainTime
        baseDist = n.segmentRemainDistance
        baseAt = Date()
        pushNav(dir, n.segmentRemainDistance, real: true)
        updateTexts(dir: dir, curRoad: n.currentRoadName ?? "", nextRoad: n.nextRoadName ?? "",
                    remainM: n.routeRemainDistance, remainS: n.routeRemainTime, lights: n.routeRemainTrafficLightCount)
        lightBase = nextLightDist(step: step, curLink: n.currentLinkIndex, stepRemain: n.segmentRemainDistance) ?? -1
        lightAt = Date()
        pushLight(lightBase)
        setRerouting(false)                             // 有新的导航信息 = 重新规划好了
        info = "图标 \(icon) → \(dir)（角度 \(curAngle.map(String.init) ?? "无")），还有 \(n.segmentRemainDistance) 米" +
            (lightBase >= 0 ? "，红绿灯 \(lightBase) 米" : "")
    }

    // 位置和车速 → 圆屏车速、小地图位置
    @objc(driveManager:updateNaviLocation:)
    func mnLocation(_ m: AMapNaviDriveManager, location l: AMapNaviLocation?) {
        guard arrival.acceptsGuidance(arrival.generation), let l = l, !cruising else { return }     // 巡航时用手机定位（onCruiseLocation）
        let kmh = max(0, l.speed)
        speedKmhNow = kmh
        speedMps = emuPaused ? 0 : Double(kmh) / 3.6
        uiSpeed = kmh
        if let c = l.coordinate {
            myLat = Double(c.latitude); myLng = Double(c.longitude)
            if let p = tripPosition { let d = Self.distM(p.lat,p.lng,myLat,myLng); if d < 300 { tripMeters += d } }
            tripPosition = (myLat,myLng); tripMaxKmh = max(tripMaxKmh,kmh)
            ShareGuide.shared.onLocation(myLat, myLng)
            MapFeeder.shared.onLocation(lat: myLat, lng: myLng, bearing: l.heading, kmh: kmh)
        }
        let msg = "#v,\(kmh)"
        if msg != lastSpeedMsg { lastSpeedMsg = msg; BleLink.shared.send(msg) }
    }

    // 电子眼：测速牌取按车速提前范围内最近的一个有限速的；前方 1 公里内的都画在小地图上
    @objc(driveManager:updateCameraInfos:)
    func mnCameras(_ m: AMapNaviDriveManager, cams: [AMapNaviCameraInfo]?) {
        let list = cams ?? []
        var best: AMapNaviCameraInfo?
        var raw = ""
        var pts: [MapPt] = []
        for c in list {
            let t = c.cameraType.rawValue
            if raw.count < 240 {
                let name = NaviBridge.CAM_NAMES[safe: NaviBridge.camIcon(t)] ?? "电子眼"
                raw += "[类型\(t)(\(name)) 限速\(c.cameraSpeed) \(c.distance)米] "
            }
            if c.cameraSpeed > 0 && c.distance >= 0 && c.distance <= cameraRange && (best == nil || c.distance < best!.distance) { best = c }
            if c.distance >= 0 && c.distance <= 1000, let p = c.coordinate,
               let fp = MapFeeder.shared.toFrame(Double(p.latitude), Double(p.longitude)) {
                pts.append(MapPt(x: fp.x, y: fp.y, spd: max(0, c.cameraSpeed), icon: NaviBridge.camIcon(t), dist: c.distance))
            }
        }
        camPts = pts
        pushCamMap()
        let f = DateFormatter(); f.dateFormat = "HH:mm:ss"
        camRaw = "\(list.count) 个：\(raw)（\(f.string(from: Date()))）"
        lastCamAt = Date()
        if let b = best, let p = b.coordinate {
            // 新的（或同一个）测速点：开始跟踪
            let lat = Double(p.latitude), lng = Double(p.longitude)
            if b.cameraSpeed != camLimit || lat != camLat || lng != camLng { camMinDist = Int.max }   // 换了一个测速点
            camLimit = b.cameraSpeed; camLat = lat; camLng = lng
            updateCamDist(b.distance)
        } else if list.isEmpty {
            clearCamera()      // 高德明确告诉我们前方没有电子眼
        }
        // 列表里有电子眼但都不在提醒范围内 / 没有限速：保持当前正在跟踪的那个（如果有）
    }

    // 车道
    @objc(driveManager:showLaneBackInfo:laneSelectInfo:)
    func mnLaneShow(_ m: AMapNaviDriveManager, back: String?, select: String?) {
        onLanes(back: back ?? "", select: select ?? "")
    }

    @objc(driveManagerHideLaneInfo:)
    func mnLaneHide(_ m: AMapNaviDriveManager) {
        if lastLaneMsg != "#j" { lastLaneMsg = "#j"; BleLink.shared.send("#j") }
    }

    // 巡航：道路设施（可能也带摄像头）、前方电子眼、巡航信息
    @objc(driveManager:updateTrafficFacilities:)
    func mnFacilities(_ m: AMapNaviDriveManager, list: [AMapNaviTrafficFacilityInfo]?) {
        if cruising { onCruiseItems(list ?? [], camerasOnly: false, name: "道路设施") }
    }

    @objc(driveManager:updateCruiseElecCameraInfos:)
    func mnCruiseCams(_ m: AMapNaviDriveManager, list: [AMapNaviTrafficFacilityInfo]?) {
        if cruising { onCruiseItems(list ?? [], camerasOnly: true, name: "电子眼") }
    }

    @objc(driveManager:updateCruiseInfo:)
    func mnCruiseInfo(_ m: AMapNaviDriveManager, info c: AMapNaviCruiseInfo?) {
        guard cruising, let c = c else { return }
        info = "巡航中：已走 \(NaviBridge.distText(c.cruisingDriveDistance))，\(NaviBridge.durText(c.cruisingDriveTime))"
    }

    // 路况光柱更新
    @objc(driveManager:updateTrafficStatus:)
    func mnTraffic(_ m: AMapNaviDriveManager, status: [AMapNaviTrafficStatus]?) {
        if navigating { pushTraffic(status) }
    }
}
