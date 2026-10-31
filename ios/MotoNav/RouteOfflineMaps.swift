import Foundation
import UIKit

/// Download one packet at a time; foreground requests take priority between packets.
/// Lifecycle/control state lives on main; worker cancellation is synchronized.
final class MapWorkToken {
    private let lock = NSLock()
    private var cancelled = false
    func cancel() { lock.lock(); cancelled = true; lock.unlock() }
    var isCancelled: Bool { lock.lock(); defer { lock.unlock() }; return cancelled }
}

final class RouteOfflineMaps {
    let style: Int
    let frame: MapGeo.Frame, plan: RouteMapPlan, source: Int, buildings: Bool
    private let worker = DispatchQueue(label: "motonav.route-offline", qos: .utility)
    private let token = MapWorkToken()
    private var jobToken: MapWorkToken?
    private let foregroundBusy: () -> Bool
    private let currentSpeed: () -> Int
    private let onReady: (RouteMapPlan.Node) -> Void
    private let onStatus: (String) -> Void
    private var ready: [Bool], baseReady: [Bool]
    private var target = 0, display = 0, busy = false
    private var retryAt:[Date]
    private var timer: Timer?
    init(frame: MapGeo.Frame, plan: RouteMapPlan, source: Int, buildings: Bool,
         foregroundBusy: @escaping () -> Bool, currentSpeed:@escaping ()->Int = {0}, onReady: @escaping (RouteMapPlan.Node) -> Void, onStatus: @escaping (String) -> Void) {
        self.style = Settings.shared.theme
        self.frame = frame; self.plan = plan; self.source = source; self.buildings = buildings
        self.foregroundBusy = foregroundBusy; self.currentSpeed=currentSpeed; self.onReady = onReady; self.onStatus = onStatus
        ready = [Bool](repeating: false, count: plan.nodes.count); baseReady = ready
        retryAt=[Date](repeating:.distantPast,count:plan.nodes.count)
    }
    func read(_ node: RouteMapPlan.Node) -> Data? {
        RouteMapCache.shared.read(frame: frame, source: source, buildings: buildings, node: node, style: style)
            ?? (buildings ? RouteMapCache.shared.read(frame: frame, source: source, buildings: false, node: node, style: style) : nil)
    }
    func update(_ index: Int, display:Int) { target = max(0, min(plan.nodes.count - 1, index));self.display=max(0,min(plan.nodes.count-1,display)) }
    func start() {
        busy = true; onStatus("沿线离线地图：检查已有地图包…")
        worker.async {
            var done = [Bool](repeating: false, count: self.plan.nodes.count), base = done
            for (i, node) in self.plan.nodes.enumerated() {
                if self.token.isCancelled { return }
                done[i] = RouteMapCache.shared.read(frame: self.frame, source: self.source, buildings: self.buildings, node: node, style: self.style) != nil
                base[i] = done[i] || RouteMapCache.shared.read(frame: self.frame, source: self.source, buildings: false, node: node, style: self.style) != nil
            }
            DispatchQueue.main.async {
                guard !self.token.isCancelled else { return }
                self.ready = done; self.baseReady = base; self.busy = false
                self.timer = Timer.scheduledTimer(withTimeInterval: 0.5, repeats: true) { [weak self] _ in self?.pump() }
                self.onReady(self.plan.nodes[self.target]); self.pump()
            }
        }
    }
    private func pump() {
        if busy, foregroundBusy() { jobToken?.cancel() }
        guard !token.isCancelled, !busy else { return }
        let count = ready.filter { $0 }.count, baseCount = baseReady.filter { $0 }.count
        if count == ready.count {
            onStatus("沿线底图已全部离线：\(count)/\(ready.count) 块（2 米/像素）")
            timer?.invalidate(); timer = nil; return
        }
        guard UIApplication.shared.applicationState == .active else {
            onStatus("沿线离线地图：\(baseCount)/\(ready.count) 块可用；回到 App 前台继续下载"); return
        }
        guard !foregroundBusy() else { return }
        let now=Date()
        guard let next=RouteMapPreparationPolicy.next(base:baseReady,complete:ready,display:display,target:target,
            buildings:buildings,kmh:currentSpeed(),available:retryAt.map{now >= $0}) else {return}
        let i=next.index,includeBuildings=next.buildings
        busy = true
        let node = plan.nodes[i]
        onStatus("沿线离线地图：\(baseCount)/\(ready.count) 块可用，下载第 \(i + 1) 块\(includeBuildings ? "（补齐建筑）" : "（普通底图优先）")")
        let job = MapWorkToken(); jobToken = job
        worker.async {
            let result = Result { () throws -> Data in
                if self.token.isCancelled { throw MapProcessingError.cancelled }
                let packet: Data
                if self.source == MapFeeder.SRC_AMAP {
                    packet = try AmapSdkRaster.build(self.frame, cx: node.x, cy: node.y, includeBuildings: includeBuildings,
                        shouldCancel: { self.token.isCancelled || job.isCancelled }, onBaseReady: { base in
                            guard !self.token.isCancelled else { return }
                            do {
                                try RouteMapCache.shared.store(base, frame: self.frame, source: self.source, node: node, style: self.style)
                                DispatchQueue.main.async { if !self.token.isCancelled { self.baseReady[i] = true; self.onReady(node) } }
                            } catch { /* The final store reports storage errors in onStatus. */ }
                        }, onBuildingStatus: { message in
                            DispatchQueue.main.async { if !self.token.isCancelled { self.onStatus(message) } }
                        }, style: self.style, preparedBase: includeBuildings ? RouteMapCache.shared.read(frame:self.frame,source:self.source,buildings:false,node:node,style:self.style) : nil).bytes
                } else {
                    throw MapProcessingError.invalid("沿线预下载仅支持高德底图")
                }
                if self.token.isCancelled { throw MapProcessingError.cancelled }
                try RouteMapCache.shared.store(packet, frame: self.frame, source: self.source, node: node, style: self.style)
                return packet
            }
            DispatchQueue.main.async {
                guard !self.token.isCancelled else { return }
                self.busy = false
                switch result {
                case .success(let packet):
                    self.baseReady[i] = true
                    self.ready[i] = !self.buildings || RasterPacket.isBuilding(packet)
                    self.retryAt[i] = self.ready[i] || !includeBuildings ? .distantPast : Date().addingTimeInterval(15)
                    self.onReady(node)
                    if !self.ready[i] && includeBuildings { self.onStatus("沿线普通底图已保存；建筑未完成，稍后重试") }
                case .failure(MapProcessingError.cancelled):
                    self.retryAt[i] = .distantPast
                case .failure(let error):
                    self.retryAt[i] = Date().addingTimeInterval(15)
                    self.onStatus("沿线离线地图：\(error.localizedDescription)，保留已保存地图")
                }
            }
        }
    }
    func close() { token.cancel(); jobToken?.cancel(); timer?.invalidate(); timer = nil }
}
