import Foundation
import UIKit

/// 版本：跟随 App 版本（见 Config.swift，iOS v0.2 新增；和安卓版 MapFeeder.kt 一样）
///
/// 小地图数据：
/// - 导航时下载当前位置附近约 2.4 公里见方的地图（高德 SDK 截图底图），
///   快开出这一块时自动下载下一块（向前方偏移，前面的路多下载一些）。
/// - 选了“卫星图”风格时改下高德卫星照片（范围小一些：半边 700 米）。
/// - 导航路线（连同路上的红绿灯位置）单独打包发给圆屏。
/// - iOS v0.3 原路线（导入的高德分享路线）：大块数据类型 4，圆屏画成橙色细线；"#o" 不显示
/// - 每次定位把当前位置、朝向、车速发给圆屏："#p,坐标系,x分米,y分米,朝向,车速"。
/// - 圆屏收齐一份地图/路线后回 "K,类型,编号"；没确认的 8 秒后重发，重连后也全部重发。
/// 状态都在主线程里读写，下载和计算在后台队列里做。
final class MapFeeder: ObservableObject {
    static let shared = MapFeeder()

    private let HALF = 1200.0            // 地图块半边长（米）
    private let REFRESH = 650.0          // 离地图块中心超过这个距离就准备下一块
    private let AHEAD = 450.0            // 新地图块中心往前方偏移
    private let SAT_HALF = 700.0         // 卫星图地图块半边长（米）
    private let SAT_REFRESH = 330.0
    private let SAT_AHEAD = 260.0
    static let MAX_BYTES = 190_000       // 圆屏线条地图缓冲 200KB，留余量
    static let MAX_FEAT = 7000
    private let RESEND = 8.0             // 秒

    static let TYPE_CHUNK = 1
    static let TYPE_ROUTE = 2
    static let TYPE_RASTER = 3
    static let TYPE_GHOST = 4

    static let SRC_AMAP = 3
    static let THEME_SAT = 10

    @Published var status = "（导航时自动下载附近地图）"
    @Published var offlineStatus = "选择路线后，在前台自动准备沿线离线底图"
    @Published var chunkInfo = ""
    @Published var buildingInfo = ""
    @Published var routeInfo = ""
    @Published var chunkAcked = false
    @Published var routeAcked = false

    private let worker = DispatchQueue(label: "motonav.map", qos: .userInitiated)
    private(set) var frame: MapGeo.Frame?

    // 当前地图块
    private var chunkCx = 0.0, chunkCy = 0.0
    private var haveChunk = false
    private var fetching = false
    private var fetchingCenter: (x: Double, y: Double)?
    private var fetchingBaseReady = false
    private var lastFetchFail = Date.distantPast
    private var lastBuildingTry = Date.distantPast
    private var chunkId = 0
    private var nextChunkId = 0
    private var chunkType = TYPE_CHUNK
    private var chunkBytes: Data?
    private var chunkStyle:Int?
    private var handoff=MapHandoffBuffer()
    private var chunkSat = false
    private var chunkSentAt = Date.distantPast

    // 当前路线
    private var routeKey = ""
    private var routeBuildSerial = 0
    private var routeId = 0
    private var routeBytes: Data?
    private var routeSentAt = Date.distantPast

    // 原路线
    private var ghostCoords: [(lat: Double, lng: Double)]?   // 还没有坐标系时先存着
    private var ghostId = 0
    private var ghostBytes: Data?
    private var ghostAcked = false
    private var ghostSentAt = Date.distantPast

    private var lastBearing: Double?
    private var requestSerial = 0
    private var workToken = MapWorkToken()
    private var offlineMaps: RouteOfflineMaps?
    private var preparedKey = ""
    private var preparedCoords: [(lat: Double, lng: Double)] = []
    private var offlineKey = ""
    private var routeProgress = -1.0
    private var pose: (x: Double, y: Double, kmh: Int)?
    private var chunkBuildingsComplete = false
    private var lastRoofUpgradeProbe=Date.distantPast
    private var deferredRoofs:MapHandoffBuffer.Packet?
    private var upgradeBase:(packet:MapHandoffBuffer.Packet,id:Int)?
    private var confirmedCenter: (x: Double, y: Double)?
    private var lastCoverageTrace = Date.distantPast
    private var buildingMode: Bool { Settings.shared.theme == 1 && Settings.shared.mapSource == Self.SRC_AMAP && Settings.shared.buildings && BleLink.shared.buildingsSupported }


    private var enabled: Bool { Settings.shared.mapOn }
    private var satMode: Bool { Settings.shared.theme == MapFeeder.THEME_SAT }

    /// Preserve prepared offline packets when navigation starts.
    func onNaviStart() { routeKey = "" }
    func onNaviEnd() {
        offlineMaps?.close(); offlineMaps = nil; offlineKey = ""
        handoff.clear()
        deferredRoofs=nil;upgradeBase=nil
        preparedCoords = []; preparedKey = ""; offlineStatus = "沿线下载已暂停，重新选择路线后继续"
        workToken.cancel(); requestSerial += 1; routeBuildSerial += 1; fetching = false
    }
    func resetChunk() {
        Settings.shared.syncBuildingContinuity()
        handoff.clear();confirmedCenter=nil;deferredRoofs=nil;upgradeBase=nil;lastRoofUpgradeProbe = .distantPast
        buildingInfo = ""; lastBuildingTry = .distantPast
        workToken.cancel(); requestSerial += 1; fetching = false
        haveChunk = false; lastFetchFail = .distantPast
        chunkBytes=nil;chunkStyle=nil;chunkAcked=false
        BleLink.shared.cancelMapTransfers()
        offlineMaps?.close(); offlineMaps = nil; offlineKey = ""; routeProgress = -1
        if preparedCoords.count >= 2 { prepareRoute(key: preparedKey, coords: preparedCoords) }
        // A style change must fetch new data even while simulation/location is paused.
        let requestedStyle=Settings.shared.theme
        DispatchQueue.main.async { [weak self] in
            guard let self=self,Settings.shared.theme==requestedStyle,self.enabled,requestedStyle != 11,
                  !self.fetching,let f=self.frame,let p=self.pose else {return}
            self.fetchChunk(f,p.x,p.y)
        }
    }
    func requestMapSample() {
        guard [0,1,5].contains(Settings.shared.theme),frame != nil,pose != nil else {
            MapSampleReport.shared.fail("请先开始一次导航或模拟，再生成地图对照图。");return
        }
        let f=frame!,p=pose!
        resetChunk()
        // No cache node: the report must come from a new capture of the current area.
        fetchChunk(f,p.x,p.y)
    }
    func onBuildingCapability() { if Settings.shared.mapSource == Self.SRC_AMAP { resetChunk() } }
    private func ensureFrame(lat: Double, lng: Double) -> MapGeo.Frame {
        if let f = frame { return f }
        let la = (lat * 10).rounded() / 10, lo = (lng * 10).rounded() / 10
        let id = ((Int((la * 10).rounded()) * 7919 + Int((lo * 10).rounded()) * 104729) & 0x7FFF) | 1
        let f = MapGeo.Frame(refLat: la, refLng: lo, id: id); frame = f; return f
    }
    /// Called when selecting a route, before departure, and after a reroute.
    func prepareRoute(key: String, coords: [(lat: Double, lng: Double)]) {
        guard coords.count >= 2 else { return }
        preparedKey = key; preparedCoords = coords
        let f = ensureFrame(lat: coords[0].lat, lng: coords[0].lng), src = Settings.shared.mapSource
        guard enabled, !satMode, Settings.shared.theme != 11 else { return }
        var hasher = Hasher()
        for c in coords { hasher.combine(c.lat); hasher.combine(c.lng) }
        let identity = "\(key)|\(hasher.finalize())|\(src)|\(buildingMode)"
        if identity == offlineKey, offlineMaps != nil { return }
        handoff.clear()
        offlineMaps?.close(); offlineKey = identity; routeProgress = -1
        workToken.cancel(); requestSerial += 1; fetching = false
        do {
            let plan = try RouteMapPlan(coords.map { (x: f.x($0.lng), y: f.y($0.lat)) })
            let maps = RouteOfflineMaps(frame: f, plan: plan, source: src, buildings: buildingMode,
                foregroundBusy: { [weak self] in self?.fetching ?? true },
                currentSpeed: { [weak self] in self?.pose?.kmh ?? 0 },
                onReady: { [weak self] node in
                    guard let self = self, self.offlineKey == identity, self.enabled, !self.fetching else { return }
                    if self.haveChunk,abs(self.chunkCx-node.x)<1,abs(self.chunkCy-node.y)<1 {
                        self.tryUpgradeBuildings();return
                    }
                    let progress = self.pose.map { plan.progress(x: $0.x, y: $0.y, previous: self.routeProgress) } ?? 0
                    let onRoute = plan.at(progress)
                    if let p = self.pose, hypot(p.x - onRoute.x, p.y - onRoute.y) >= 400 { return }
                    let wantedIndex = self.pose.map { plan.displayTarget(progress: progress, x: $0.x, y: $0.y, kmh: $0.kmh) }
                        ?? plan.target(progress: progress, initial: true)
                    let wanted = plan.nodes[wantedIndex]
                    guard wanted == node else { return }
                    if self.haveChunk && abs(self.chunkCx - node.x) < 1 && abs(self.chunkCy - node.y) < 1 {
                        if !self.buildingMode || self.chunkBuildingsComplete { return }
                        if let packet = self.offlineMaps?.read(node), !RasterPacket.isBuilding(packet) { return }
                    }
                    self.fetchChunk(f, node.x, node.y, node: node)
                }, onStatus: { [weak self] status in
                    if self?.offlineKey == identity { self?.offlineStatus = status }
                })
            offlineMaps = maps; maps.start()
        } catch { offlineStatus = "沿线离线地图：\(error.localizedDescription)" }
    }
    func onLocation(lat: Double, lng: Double, bearing: Double, kmh: Int) {
        let f = ensureFrame(lat: lat, lng: lng), x = f.x(lng), y = f.y(lat)
        pose = (x, y, kmh)
        protectBaseDuringRoofTransfer()
        flushPreparedMap()
        if enabled, Settings.shared.mapSource == Self.SRC_AMAP, Date().timeIntervalSince(lastCoverageTrace) >= 2 {
            lastCoverageTrace = Date()
            MapPipelineTrace.event("view-coverage", ["kmh": kmh, "ready": BleLink.shared.isReady,
                "sentCovered": haveChunk && MapCoveragePolicy.covers(x: x, y: y, cx: chunkCx, cy: chunkCy, kmh: max(120, kmh)),
                "bytes":chunkBytes?.count ?? 0,"awaitAck":!chunkAcked,
                "transferPending":BleLink.shared.bulkPending(type:chunkType,id:chunkId),
                "preparedOffsetM":handoff.pending.map{max(abs(x-$0.cx),abs(y-$0.cy))} ?? -1,
                "viewportOffsetM":confirmedCenter.map{max(abs(x-$0.x),abs(y-$0.y))} ?? -1,
                "confirmedCovered": confirmedCenter.map { MapCoveragePolicy.covers(x: x, y: y, cx: $0.x, cy: $0.y, kmh: max(120, kmh)) } ?? false])
        }
        var hdg = bearing
        if kmh >= 4 || lastBearing == nil { if kmh >= 4 { lastBearing = bearing } } else { hdg = lastBearing! }
        BleLink.shared.send("#p,\(f.id),\(Int((x * 10).rounded())),\(Int((y * 10).rounded())),\(Int(hdg.rounded())),\(kmh)")
        if ghostCoords != nil && ghostBytes == nil { encodeGhost() }
        if !enabled { return }
        if Settings.shared.theme == 11 { status = "龙珠雷达（无需底图）"; checkResend(); return }
        var node: RouteMapPlan.Node?
        var futureNode:RouteMapPlan.Node?
        if let maps = offlineMaps {
            routeProgress = maps.plan.progress(x: x, y: y, previous: routeProgress)
            let projected = maps.plan.at(routeProgress), index = maps.plan.displayTarget(progress: routeProgress, x: x, y: y, kmh: kmh)
            let preparationIndex=maps.plan.preparationTarget(progress:routeProgress,kmh:kmh)
            maps.update(preparationIndex,display:index)
            let candidate = maps.plan.nodes[index]
            if hypot(x - projected.x, y - projected.y) < 400,
               MapCoveragePolicy.covers(x: x, y: y, cx: candidate.x, cy: candidate.y, kmh: max(120, kmh), margin: 50) {
                node = candidate
                futureNode=maps.plan.nodes[preparationIndex]
            }
        }
        if let current=node {
            var relevant=[(x:current.x,y:current.y)]
            if let future=futureNode {relevant.append((x:future.x,y:future.y))}
            if handoff.discardObsolete(x:x,y:y,kmh:kmh,allowed:relevant) {
                MapPipelineTrace.event("staged-map-expired",["kmh":kmh])
            }
        }
        let confirmedCovered=confirmedCenter.map{MapCoveragePolicy.covers(x:x,y:y,cx:$0.x,cy:$0.y,kmh:max(120,kmh),margin:50)} ?? false
        let recovering=haveChunk && !confirmedCovered
        let distance = hypot(x - chunkCx, y - chunkCy)
        let refresh = chunkSat ? SAT_REFRESH : REFRESH
        let changedNode = node.map { abs($0.x - chunkCx) >= 1 || abs($0.y - chunkCy) >= 1 } ?? false
        let needsCoverage = !satMode && Settings.shared.mapSource == Self.SRC_AMAP && !MapCoveragePolicy.covers(x: x, y: y, cx: chunkCx, cy: chunkCy, kmh: max(120, kmh), margin: 50)
        let prepareEarly = !satMode && Settings.shared.mapSource == Self.SRC_AMAP &&
            MapCoveragePolicy.needsRefresh(x: x, y: y, cx: chunkCx, cy: chunkCy, bearing: bearing, kmh: kmh)
        // A building pass must yield before movement leaves its base map. Cached bases can switch immediately.
        if fetching, !satMode, Settings.shared.mapSource == Self.SRC_AMAP, let center = fetchingCenter,
           (fetchingBaseReady && (needsCoverage || changedNode) || !MapHandoffBuffer.mayPrepare(x:x,y:y,cx:center.x,cy:center.y)) {
            MapPipelineTrace.event("movement-cancel", ["serial": requestSerial, "baseReady": fetchingBaseReady, "needsCoverage": needsCoverage, "nextBlock": changedNode, "kmh": kmh])
            workToken.cancel(); requestSerial += 1; fetching = false
        }
        let retryBuildings = offlineMaps == nil && buildingMode && MapCoveragePolicy.mayCaptureBuildings(kmh:kmh) && !chunkBuildingsComplete && UIApplication.shared.applicationState == .active && Date().timeIntervalSince(lastBuildingTry) >= 60
        // Preparation and display targets differ: a future block may be captured now,
        // then held until it covers the viewport and the previous ATT transfer finishes.
        let wanted = haveChunk && prepareEarly && !recovering ? (futureNode ?? node) : node
        let a = bearing * .pi / 180, ahead = kmh > 5 ? (satMode ? SAT_AHEAD : MapCoveragePolicy.lookahead(kmh: kmh, preferred: AHEAD)) : 0
        let targetX=wanted?.x ?? (x+sin(a)*ahead), targetY=wanted?.y ?? (y+cos(a)*ahead)
        let sameCurrent=haveChunk && abs(targetX-chunkCx)<1 && abs(targetY-chunkCy)<1
        let alreadyPrepared=handoff.contains(cx:targetX,cy:targetY,style:Settings.shared.theme)
        if !fetching && handoff.pending == nil && !alreadyPrepared && (!haveChunk || distance > refresh || needsCoverage || changedNode || (prepareEarly && !sameCurrent) || retryBuildings) && Date().timeIntervalSince(lastFetchFail) > 15 {
            fetchChunk(f,targetX,targetY,node:wanted)
        }
        tryUpgradeBuildings();checkResend()
    }
    private func roofDeliveryAllowed(_ bytes:Data,cx:Double,cy:Double)->Bool {
        guard haveChunk,chunkAcked,let p=pose,let center=confirmedCenter,
              abs(center.x-chunkCx)<1,abs(center.y-chunkCy)<1 else {return false}
        let seconds=MapCoveragePolicy.remainingSeconds(x:p.x,y:p.y,cx:center.x,cy:center.y,
            kmh:p.kmh,bearing:lastBearing,plan:offlineMaps?.plan,progress:routeProgress)
        let candidateSeconds=MapCoveragePolicy.remainingSeconds(x:p.x,y:p.y,cx:cx,cy:cy,
            kmh:p.kmh,bearing:lastBearing,plan:offlineMaps?.plan,progress:routeProgress)
        guard seconds>0,candidateSeconds>0 else {return false}
        let upgrade=abs(cx-chunkCx)<1 && abs(cy-chunkCy)<1
        let reserve=upgrade ? max(chunkBytes?.count ?? 0,handoff.pending?.bytes.count ?? 0):0
        return MapDeliveryPolicy.roofsFit(kmh:p.kmh,bytes:bytes.count,rate:BleLink.shared.mapBytesPerSecond,
            coverageSeconds:min(seconds,candidateSeconds),nextBaseBytes:reserve)
    }
    /// A detail transfer begun while stopped must yield if departure makes its
    /// remaining bytes unsafe. Restore the genuinely acknowledged base state;
    /// do not claim the abandoned building packet was received.
    private func protectBaseDuringRoofTransfer() {
        guard !chunkAcked,let saved=upgradeBase,let p=pose else {return}
        let remaining=BleLink.shared.mapPayloadRemaining(id:chunkId)
        guard remaining>0 else {return} // Already awaiting K: let bounded receipt recovery finish.
        let old=saved.packet
        let seconds=MapCoveragePolicy.remainingSeconds(x:p.x,y:p.y,cx:old.cx,cy:old.cy,
            kmh:p.kmh,bearing:lastBearing,plan:offlineMaps?.plan,progress:routeProgress)
        let candidateSeconds=MapCoveragePolicy.remainingSeconds(x:p.x,y:p.y,cx:chunkCx,cy:chunkCy,
            kmh:p.kmh,bearing:lastBearing,plan:offlineMaps?.plan,progress:routeProgress)
        let reserve=max(old.bytes.count,handoff.pending?.bytes.count ?? 0)
        guard min(seconds,candidateSeconds)<=0 || !MapDeliveryPolicy.roofsFit(kmh:p.kmh,bytes:remaining,
            rate:BleLink.shared.mapBytesPerSecond,coverageSeconds:min(seconds,candidateSeconds),nextBaseBytes:reserve) else {return}
        MapPipelineTrace.event("roof-yield",["id":chunkId,"bytes":remaining,"kmh":p.kmh])
        if let bytes=chunkBytes,RasterPacket.isBuilding(bytes) {
            deferredRoofs = .init(bytes:bytes,cx:chunkCx,cy:chunkCy,sat:chunkSat,type:chunkType,info:"暂缓建筑包",source:Self.SRC_AMAP,style:chunkStyle ?? Settings.shared.theme)
        }
        BleLink.shared.cancelMapTransfers()
        chunkBytes=old.bytes;chunkCx=old.cx;chunkCy=old.cy;chunkId=saved.id
        chunkType=old.type;chunkSat=old.sat;chunkStyle=old.style
        chunkAcked=true;chunkBuildingsComplete=RasterPacket.isBuilding(old.bytes)
        confirmedCenter=(old.cx,old.cy);upgradeBase=nil;lastRoofUpgradeProbe=Date()
        status="保留已确认底图，优先准备前方道路";chunkInfo="地图块 #\(chunkId)：已确认底图"
    }
    private func tryUpgradeBuildings() {
        guard enabled,buildingMode,haveChunk,chunkAcked,!chunkBuildingsComplete,!fetching,
              BleLink.shared.isReady,!BleLink.shared.bulkPending,
              Date().timeIntervalSince(lastRoofUpgradeProbe)>=2 else {return}
        lastRoofUpgradeProbe=Date()
        let saved=deferredRoofs.flatMap{p -> MapHandoffBuffer.Packet? in
            p.style==Settings.shared.theme && abs(p.cx-chunkCx)<1 && abs(p.cy-chunkCy)<1 ? p:nil
        }
        let candidate:MapHandoffBuffer.Packet?
        if let saved=saved {candidate=saved}
        else if let maps=offlineMaps,let node=maps.plan.nodes.first(where:{abs($0.x-chunkCx)<1 && abs($0.y-chunkCy)<1}),
                let bytes=maps.read(node),RasterPacket.isBuilding(bytes) {
            candidate = .init(bytes:bytes,cx:node.x,cy:node.y,sat:false,type:Self.TYPE_RASTER,info:"沿线建筑包",source:Self.SRC_AMAP,style:Settings.shared.theme)
        } else {candidate=nil}
        guard let candidate=candidate else {return}
        guard roofDeliveryAllowed(candidate.bytes,cx:candidate.cx,cy:candidate.cy) else {
            MapPipelineTrace.event("roof-deferred",["bytes":candidate.bytes.count,"kmh":pose?.kmh ?? 0,"rate":BleLink.shared.mapBytesPerSecond ?? 0]);return
        }
        MapPipelineTrace.event("roof-upgrade",["bytes":candidate.bytes.count,"kmh":pose?.kmh ?? 0,"rate":BleLink.shared.mapBytesPerSecond ?? 0])
        acceptChunk(candidate.bytes,cx:candidate.cx,cy:candidate.cy,sat:candidate.sat,type:candidate.type,info:candidate.info,src:candidate.source)
    }
    private func acceptChunk(_ incoming: Data, cx: Double, cy: Double, sat: Bool, type: Int, info: String, src: Int) {
        guard enabled, Settings.shared.theme != 11 else { return }
        let kmh=pose?.kmh ?? 0
        if buildingMode,RasterPacket.isBuilding(incoming) {
            deferredRoofs = .init(bytes:incoming,cx:cx,cy:cy,sat:sat,type:type,info:info,source:src,style:Settings.shared.theme)
        }
        let allowRoofs=buildingMode && roofDeliveryAllowed(incoming,cx:cx,cy:cy)
        let b=allowRoofs ? incoming:MapDeliveryPolicy.compactBase(incoming)
        if b.count<incoming.count {
            MapPipelineTrace.event("map-base-priority",["beforeBytes":incoming.count,"bytes":b.count,"kmh":kmh])
            if kmh>=60 {buildingInfo="高速行驶优先保证底图，稍后补充建筑"}
        }
        guard sat || MapStyleTransfer.matches(b,style:Settings.shared.theme) else {return}
        guard src != Self.SRC_AMAP || Settings.shared.theme != 1 || RasterPacket.hasMapContent(b) else {
            status="底图尚未准备完整，保留已有地图";return
        }
        let complete = RasterPacket.isBuilding(b)
        if haveChunk,chunkType==type,chunkSat==sat,chunkBuildingsComplete,!complete,
           abs(cx-chunkCx)<1,abs(cy-chunkCy)<1 {
            // A repeated base callback is not a newer building observation.
            // Preserve already-delivered or in-flight detail at the same location.
            MapPipelineTrace.event("roof-downgrade-blocked",["id":chunkId]);return
        }
        if haveChunk, chunkType == type, chunkSat == sat, abs(cx - chunkCx) < 1, abs(cy - chunkCy) < 1,
           !complete, !chunkAcked {
            MapPipelineTrace.event("base-kept-inflight", ["id": chunkId])
            return
        }
        // The complete incoming detail packet was budgeted; do not inflate it
        // by carrying extra roofs from an older map after that decision.
        let displayBytes = b
        if haveChunk, chunkType == type, chunkSat == sat, abs(cx - chunkCx) < 1, abs(cy - chunkCy) < 1,
           displayBytes == chunkBytes {
            // Cached base then screenshot base often encode the exact same map.
            // Keep the transfer ID so the second callback cannot invalidate fragments already received.
            chunkBuildingsComplete = chunkBuildingsComplete || complete
            return
        }
        let transferPending = haveChunk && !chunkAcked && BleLink.shared.bulkPending(type:chunkType,id:chunkId)
        let outsideView = pose.map { !sat && src == Self.SRC_AMAP && !MapCoveragePolicy.covers(x:$0.x,y:$0.y,cx:cx,cy:cy,kmh:max(120,$0.kmh),margin:50) } ?? false
        if transferPending || outsideView {
            if pose.map({MapHandoffBuffer.mayPrepare(x:$0.x,y:$0.y,cx:cx,cy:cy)}) ?? true {
                handoff.stage(.init(bytes:b,cx:cx,cy:cy,sat:sat,type:type,info:info,source:src,style:Settings.shared.theme))
                MapPipelineTrace.event("map-staged",["transferPending":transferPending,"outsideView":outsideView])
            }
            return
        }
        if handoff.contains(cx:cx,cy:cy,style:Settings.shared.theme) {handoff.clear()}
        if complete,haveChunk,chunkAcked,let previous=chunkBytes {
            upgradeBase=(.init(bytes:previous,cx:chunkCx,cy:chunkCy,sat:chunkSat,type:chunkType,info:"已确认底图",source:src,style:chunkStyle ?? Settings.shared.theme),chunkId)
        } else {upgradeBase=nil}
        chunkBytes = displayBytes
        chunkStyle=Settings.shared.theme
        chunkBuildingsComplete = complete
        if complete,deferredRoofs?.cx==cx,deferredRoofs?.cy==cy {deferredRoofs=nil}
        haveChunk = true; chunkSat = sat; chunkCx = cx; chunkCy = cy
        nextChunkId = (nextChunkId + 1) & 0xFF;chunkId=nextChunkId;chunkType = type; chunkAcked = false
        status = complete ? "高德建筑底图已就绪" : "地图已就绪"
        chunkInfo = "地图块 #\(chunkId)：\(info)，\(b.count / 1024) KB" + (buildingMode && !complete ? "（建筑尚未完成，显示普通底图）" : "")
        sendChunk()
    }
    private func fetchChunk(_ f: MapGeo.Frame, _ cx: Double, _ cy: Double, node: RouteMapPlan.Node? = nil) {
        fetching = true; status = "正在准备附近地图…"; requestSerial += 1
        fetchingCenter = (cx, cy); fetchingBaseReady = false
        let serial = requestSerial
        MapPipelineTrace.event("fetch-start", ["serial": serial, "buildings": buildingMode, "source": Settings.shared.mapSource])
        workToken.cancel(); let token = MapWorkToken(); workToken = token
        let src = Settings.shared.mapSource, sat = satMode, buildings = node == nil && buildingMode && MapCoveragePolicy.mayCaptureBuildings(kmh:pose?.kmh ?? 0), style = Settings.shared.theme
        let half = HALF, satHalf = SAT_HALF, maps = offlineMaps
        // Cached packets can be read and sent even when iOS has suspended graphics rendering.
        if let node = node, let cached = maps?.read(node) {
            acceptChunk(cached, cx: cx, cy: cy, sat: false, type: Self.TYPE_RASTER, info: "沿线离线包", src: src)
            fetchingBaseReady = true
            if !buildings || RasterPacket.isBuilding(cached) || UIApplication.shared.applicationState != .active { fetching = false; return }
            // A cached base is usable immediately, but must not suppress building extraction.
        }
        if buildings { lastBuildingTry = Date() }
        if buildings && UIApplication.shared.applicationState != .active {
            fetching = false; buildingInfo = "回到 App 前台继续提取建筑；已保存底图仍可使用"; return
        }
        worker.async {
            var bytes: Data?, type = Self.TYPE_RASTER, gotSat = false, errorText = "", info = ""
            do {
                if token.isCancelled { throw MapProcessingError.cancelled }
                if sat {
                    bytes = try AmapSdkRaster.build(f,cx: cx,cy: cy,includeBuildings: false,shouldCancel: { token.isCancelled },satellite: true, style: style).bytes
                    gotSat = true; info = "高德卫星图"
                } else if src == Self.SRC_AMAP {
                    bytes = try AmapSdkRaster.build(f, cx: cx, cy: cy, includeBuildings: buildings, shouldCancel: { token.isCancelled }, onBaseReady: { base in
                        if let node = node { try? RouteMapCache.shared.store(base, frame: f, source: src, node: node, style: style) }
                        DispatchQueue.main.async {
                            if serial == self.requestSerial && !token.isCancelled {
                                self.fetchingBaseReady = true
                                self.acceptChunk(base, cx: cx, cy: cy, sat: false, type: Self.TYPE_RASTER, info: "高德无字底图，2 米/像素", src: src)
                            }
                        }
                    }, onBuildingStatus: { message in
                        DispatchQueue.main.async {
                            if serial == self.requestSerial && !token.isCancelled { self.buildingInfo = message }
                        }
                    }, style: style).bytes
                    info = "高德无字底图，2 米/像素"
                }
                if let packet = bytes, let node = node, !sat, !token.isCancelled {
                    try? RouteMapCache.shared.store(packet, frame: f, source: src, node: node, style: style)
                }
            } catch { errorText = error.localizedDescription }
            DispatchQueue.main.async {
                guard serial == self.requestSerial, !token.isCancelled else { return }
                self.fetching = false
                guard let packet = bytes else {
                    self.lastFetchFail = Date();self.status = "地图准备失败，保留已有地图：\(errorText)"
                    MapSampleReport.shared.failIfWaiting("本次取图未完成，请保持 App 前台后重试。");return
                }
                self.acceptChunk(packet, cx: cx, cy: cy, sat: gotSat, type: type, info: info, src: src)
            }
        }
    }

    private func flushPreparedMap() {
        guard enabled,let p=pose else {return}
        let blocked=haveChunk && !chunkAcked && BleLink.shared.bulkPending(type:chunkType,id:chunkId)
        if let next=handoff.take(x:p.x,y:p.y,kmh:p.kmh,style:Settings.shared.theme,transferPending:blocked) {
            acceptChunk(next.bytes,cx:next.cx,cy:next.cy,sat:next.sat,type:next.type,info:next.info,src:next.source)
        }
    }
    private func sendChunk() {
        guard let b = chunkBytes, enabled, Settings.shared.theme != 11,chunkStyle==Settings.shared.theme else { return }
        chunkSentAt = Date()
        let wire = BleLink.shared.buildingsSupported ? b : RasterPacket.forLegacy(b)
        MapPipelineTrace.packet(wire, type: chunkType, id: chunkId, ready: BleLink.shared.isReady)
        BleLink.shared.sendBulk(type: chunkType, id: chunkId, data: wire)
    }

    // MARK: - 路线

    /// 每次收到导航信息时调用，路线变了（新规划 / 偏航重算）就重新发送。
    /// coords、lights 都是高德坐标（纬度, 经度）。还没有坐标系（没定位）时返回 false，稍后再试。
    func onRoute(key: String, coords: [(lat: Double, lng: Double)], lights: [(lat: Double, lng: Double)]) -> Bool {
        guard let f = frame else { return false }
        if key == routeKey { return true }
        if coords.count < 2 { return false }
        routeKey = key
        prepareRoute(key: key, coords: coords)
        routeBuildSerial += 1; let serial = routeBuildSerial
        worker.async {
            var pts = [Double](repeating: 0, count: coords.count * 2)
            for (i, c) in coords.enumerated() { pts[2 * i] = f.x(c.lng); pts[2 * i + 1] = f.y(c.lat) }
            var lp = [Double](repeating: 0, count: lights.count * 2)
            for (i, c) in lights.enumerated() { lp[2 * i] = f.x(c.lng); lp[2 * i + 1] = f.y(c.lat) }
            let b = MapGeo.encodeRoute(frameId: f.id, pts: pts, maxPts: 20000, lights: lp)
            DispatchQueue.main.async {
                guard serial == self.routeBuildSerial else { return }
                self.routeId = (self.routeId + 1) & 0xFF
                self.routeBytes = b
                self.routeAcked = false
                self.routeInfo = "路线 #\(self.routeId)：\(coords.count) 个点、\(lights.count) 个红绿灯 → \(max(1, b.count / 1024)) KB"
                self.sendRoute()
            }
        }
        return true
    }

    private func sendRoute() {
        guard let b = routeBytes, enabled else { return }
        routeSentAt = Date()
        BleLink.shared.sendBulk(type: MapFeeder.TYPE_ROUTE, id: routeId, data: b)
    }

    // MARK: - 原路线

    /// 开始导航时调用：把导入的分享路线发给圆屏画成细线；nil = 不显示
    func setGhost(_ coords: [(lat: Double, lng: Double)]?) {
        ghostCoords = coords
        ghostBytes = nil; ghostAcked = false
        if coords == nil { BleLink.shared.send("#o"); return }
        encodeGhost()
    }

    private func encodeGhost() {
        guard let f = frame, let c = ghostCoords, c.count >= 2 else { return }
        var pts = [Double](repeating: 0, count: c.count * 2)
        for (i, p) in c.enumerated() { pts[2 * i] = f.x(p.lng); pts[2 * i + 1] = f.y(p.lat) }
        ghostBytes = MapGeo.encodeRoute(frameId: f.id, pts: pts, maxPts: 5000)
        ghostId = (ghostId + 1) & 0xFF
        ghostAcked = false
        sendGhost()
    }

    private func sendGhost() {
        guard let b = ghostBytes, enabled else { return }
        ghostSentAt = Date()
        BleLink.shared.sendBulk(type: MapFeeder.TYPE_GHOST, id: ghostId, data: b)
    }

    // MARK: - 确认与重发

    /// 圆屏回复 "K,类型,编号"：这一份收齐了
    func onAck(type: Int, id: Int) {
        if type == chunkType && id == chunkId {
            chunkAcked = true; status = "圆屏已收到地图"
            upgradeBase=nil
            confirmedCenter = (chunkCx, chunkCy);lastRoofUpgradeProbe = .distantPast
            DispatchQueue.main.async { [weak self] in self?.flushPreparedMap();self?.tryUpgradeBuildings() }
            MapPipelineTrace.event("map-ack", ["type": type, "id": id, "buildings": chunkBytes.map(RasterPacket.isBuilding) ?? false])
        }
        if type == MapFeeder.TYPE_ROUTE && id == routeId { routeAcked = true }
        if type == MapFeeder.TYPE_GHOST && id == ghostId { ghostAcked = true }
    }

    /// 蓝牙重新连上 / 小地图重新打开：圆屏可能重启过，全部重发
    func resendAll() {
        chunkAcked = false; routeAcked = false; ghostAcked = false
        sendChunk(); sendRoute(); sendGhost()
    }

    private func checkResend() {
        let b = BleLink.shared
        if !b.isReady || b.bulkPending { return }
        if chunkBytes != nil && !chunkAcked && Date().timeIntervalSince(chunkSentAt) > 2 { sendChunk() }
        if routeBytes != nil && !routeAcked && Date().timeIntervalSince(routeSentAt) > RESEND { sendRoute() }
        if ghostBytes != nil && !ghostAcked && Date().timeIntervalSince(ghostSentAt) > RESEND { sendGhost() }
    }

    /// 高德坐标 → 小地图平面坐标（米）；还没有坐标系时返回 nil
    func toFrame(_ lat: Double, _ lng: Double) -> (x: Int, y: Int)? {
        guard let f = frame else { return nil }
        return (Int(f.x(lng).rounded()), Int(f.y(lat).rounded()))
    }

    /// iOS v0.6 首页用的一句话状态
    var shortStatus: String {
        if !enabled { return "已关闭" }
        if chunkAcked { return "圆屏已收到地图" }
        if status.hasPrefix("地图下载失败") { return "下载失败，15 秒后重试" }
        if status.hasPrefix("正在下载") { return "正在下载" }
        if haveChunk { return "已下载，等圆屏收" }
        return "导航时自动下载"
    }

    var infoText: String {
        if !enabled { return "小地图已关闭" }
        return "\(status)\n\(chunkInfo)\(chunkAcked ? "（圆屏已确认）" : "")\n\(routeInfo)\(routeAcked ? "（圆屏已确认）" : "")\n\(offlineStatus)"
    }
}

/// Explicitly enabled, bounded timing diagnostics; no images, payloads or coordinates.
enum MapPipelineTrace {
    private static let gate=NSLock()
    private static let queue=DispatchQueue(label:"motonav.map.trace")
    private static var enabled=false
    private static var buffer=MapTraceBuffer()
    private static var startUptime=0.0
    static func begin() {
        gate.lock();defer{gate.unlock()}
        queue.async {buffer=MapTraceBuffer();startUptime=ProcessInfo.processInfo.systemUptime}
        enabled=true
    }
    static func event(_ name:String,_ fields:[String:Any]=[:]) {
        gate.lock();defer{gate.unlock()}
        guard enabled else {return}
        let uptime=ProcessInfo.processInfo.systemUptime
        queue.async {buffer.append(name:name,fields:fields,seconds:uptime-startUptime)}
    }
    static func packet(_ bytes:Data,type:Int,id:Int,ready:Bool) {
        gate.lock();let recording=enabled;gate.unlock()
        guard recording else {return}
        event("map-send",["type":type,"id":id,"bytes":bytes.count,"ready":ready,
            "decodeValid":RasterPacket.valid(bytes),"buildings":RasterPacket.isBuilding(bytes)])
    }
    static func finish(completion:@escaping(URL?)->Void) {
        gate.lock();defer{gate.unlock()};enabled=false
        queue.async {
            let root=FileManager.default.urls(for:.documentDirectory,in:.userDomainMask)[0].appendingPathComponent("MotoNav-Diagnostics",isDirectory:true)
            let url=root.appendingPathComponent("map-timing-"+UUID().uuidString+".json")
            var report=buffer.document
            report["version"]=APP_VERSION;report["build"]=Bundle.main.object(forInfoDictionaryKey:"CFBundleVersion") ?? ""
            report["note"]="User-started timing session. No keys, cookies, raw packets, images, route names or geographic coordinates."
            do {
                try FileManager.default.createDirectory(at:root,withIntermediateDirectories:true)
                try JSONSerialization.data(withJSONObject:report,options:[.prettyPrinted,.sortedKeys]).write(to:url,options:.atomic)
                DispatchQueue.main.async{completion(url)}
            } catch {DispatchQueue.main.async{completion(nil)}}
        }
    }
}

final class MapTraceReport:ObservableObject {
    static let shared=MapTraceReport()
    @Published private(set) var recording=false
    @Published private(set) var exporting=false
    @Published private(set) var files:[URL]=[]
    @Published private(set) var status="开始模拟前记录，出现底图空白后停止并导出。"
    private var timer:Timer?
    private init() {
        let manager=FileManager.default
        let saved=manager.urls(for:.documentDirectory,in:.userDomainMask)[0].appendingPathComponent("MotoNav-Diagnostics",isDirectory:true)
        let legacy=manager.urls(for:.cachesDirectory,in:.userDomainMask)[0].appendingPathComponent("map-pipeline",isDirectory:true)
        if let previous=MapReportFiles.latestTiming(in:[saved,legacy]) {
            files=[previous];status="已找到上次保存的衔接记录，可直接保存或分享。"
        }
    }
    func begin() {
        guard !recording && !exporting else {return}
        files=[];recording=true;status="正在记录地图衔接，最长 10 分钟。"
        MapPipelineTrace.begin()
        MapPipelineTrace.event("session-start",["maxWrite":BleLink.shared.maxWrite,"ready":BleLink.shared.isReady,"style":Settings.shared.theme])
        timer=Timer.scheduledTimer(withTimeInterval:600,repeats:false){[weak self] _ in self?.finish()}
    }
    func finish() {
        guard recording else {return}
        timer?.invalidate();timer=nil;recording=false;exporting=true;status="正在整理记录…"
        MapPipelineTrace.event("session-stop")
        MapPipelineTrace.finish{url in
            self.exporting=false
            self.files=url.map{[$0]} ?? []
            self.status=url == nil ? "记录保存失败，请重试。":"记录已保存，仅含地图准备、传输与确认的时间和状态。"
        }
    }
}
