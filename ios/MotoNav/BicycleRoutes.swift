import Foundation
import ObjectiveC

/// SDK-independent parsing, including responses with a full path but no step geometry.
enum BicycleRouteParser {
    struct Segment {
        let polyline: String, instruction: String, road: String, action: String
        let distance: Int, duration: Int
    }
    struct Path {
        let polyline: String, distance: Int, duration: Int
        let segments: [Segment]
    }
    private static func points(_ polyline: String) throws -> [MotorPoint] {
        var result: [MotorPoint] = []
        for pair in polyline.split(separator: ";") {
            let xy=pair.split(separator: ",",omittingEmptySubsequences:false)
            guard xy.count==2,let lo=Double(xy[0]),let la=Double(xy[1]),MotorRequest.valid(la,lo) else {
                throw AmapShare.Fail("自行车路线坐标无效")
            }
            let p=MotorPoint(lat:la,lng:lo)
            if let previous=result.last {
                if RouteShape.metres(previous,p)<0.01 {continue}
                guard RouteShape.metres(previous,p)<5000 else {throw AmapShare.Fail("自行车路线不连续")}
            }
            guard result.count<150000 else {throw AmapShare.Fail("自行车路线过长")}
            result.append(p)
        }
        return result
    }
    static func parse(_ path: Path, request: MotorRequest) throws -> MotorRoute {
        var all: [MotorPoint]=[],steps: [MotorStep]=[]
        let missingGeometry=path.segments.isEmpty || path.segments.contains{$0.polyline.trimmingCharacters(in:.whitespacesAndNewlines).isEmpty}
        if !missingGeometry {
            for segment in path.segments {
                let part=try points(segment.polyline),start=max(0,all.count-1)
                for p in part {
                    if let previous=all.last {
                        if RouteShape.metres(previous,p)<0.01 {continue}
                        guard RouteShape.metres(previous,p)<5000 else {throw AmapShare.Fail("自行车路线不连续")}
                    }
                    guard all.count<150000 else {throw AmapShare.Fail("自行车路线过长")}
                    all.append(p)
                }
                if !all.isEmpty {steps.append(MotorStep(instruction:segment.instruction,road:segment.road,action:segment.action,end:all.count-1,start:start,distance:segment.distance,duration:segment.duration))}
            }
        }
        let detailed = !missingGeometry && all.count>=2 && !steps.isEmpty
        if !detailed {
            // Do not draw a straight line between endpoints or invent turn instructions.
            all=try points(path.polyline)
            guard all.count>=2 else {throw AmapShare.Fail("高德未返回可用的自行车路线坐标，请稍后重试")}
            steps=[MotorStep(instruction:"沿自行车路线前进",road:"",action:"",end:all.count-1,start:0,distance:max(0,path.distance),duration:max(0,path.duration))]
        }
        let total=RouteShape(points:all).cum.last ?? 0
        guard total>=3,total<=5_000_000 else {throw AmapShare.Fail("自行车路线长度无效")}
        return MotorRoute(request:request,points:all,steps:steps,seconds:max(0,path.duration),detailedGuidance:detailed)
    }
    static func routes(_ paths: [Path], request: MotorRequest) throws -> [MotorRoute] {
        var result: [MotorRoute]=[]
        var failure: Error=AmapShare.Fail("高德没有返回自行车路线")
        for path in paths.prefix(10) {
            do {result.append(try parse(path,request:request))}
            catch {failure=error}
        }
        guard !result.isEmpty else {throw failure}
        return result
    }
}

/// Public iOS SDK bicycle planning. No driving conversion or electric-bike fallback.
final class BicycleRouteQuery: NSObject {
    private static var pending: [UUID:BicycleRouteQuery]=[:]
    private let id=UUID(), request: MotorRequest
    private var api: AMapSearchAPI?
    private var continuation: CheckedContinuation<[MotorRoute],Error>?
    private var timeout: DispatchWorkItem?
    private init(_ request: MotorRequest, _ continuation: CheckedContinuation<[MotorRoute],Error>) {
        self.request=request;self.continuation=continuation;super.init()
        class_addProtocol(BicycleRouteQuery.self,AMapSearchDelegate.self)
    }
    static func load(_ request: MotorRequest) async throws -> [MotorRoute] {
        try await withCheckedThrowingContinuation { continuation in
            DispatchQueue.main.async {
                let query=BicycleRouteQuery(request,continuation)
                Self.pending[query.id]=query; query.start()
            }
        }
    }
    private func start() {
        guard request.vias.isEmpty else { finish(.failure(AmapShare.Fail("自行车规划暂不支持途经点")));return }
        api=AMapSearchAPI(); api?.delegate=(self as AnyObject) as? AMapSearchDelegate
        let r=AMapRidingRouteSearchRequest()
        r.requireExtension=true
        r.origin=AMapGeoPoint.location(withLatitude:CGFloat(request.start.lat),longitude:CGFloat(request.start.lng))
        r.destination=AMapGeoPoint.location(withLatitude:CGFloat(request.end.lat),longitude:CGFloat(request.end.lng))
        let work=DispatchWorkItem { [weak self] in self?.finish(.failure(AmapShare.Fail("自行车规划超时，请检查网络、定位和高德 iOS Key"))) }
        timeout=work;DispatchQueue.main.asyncAfter(deadline:.now()+25,execute:work)
        api?.aMapRidingRouteSearch(r)
    }
    private func finish(_ result: Result<[MotorRoute],Error>) {
        if !Thread.isMainThread {DispatchQueue.main.async{[weak self] in self?.finish(result)};return}
        guard let c=continuation else { return };continuation=nil
        timeout?.cancel();timeout=nil;api?.delegate=nil;api=nil
        Self.pending.removeValue(forKey:id);c.resume(with:result)
    }
    @objc(onRouteSearchDone:response:)
    func completed(_ r: AMapRouteSearchBaseRequest?, response: AMapRouteSearchResponse?) {
        do {
            let paths=response?.route?.paths ?? []
            guard !paths.isEmpty else { throw AmapShare.Fail("高德没有返回自行车路线") }
            let options=paths.prefix(10).map { path -> BicycleRouteParser.Path in
                let segments=(path.steps ?? []).map{BicycleRouteParser.Segment(polyline:$0.polyline ?? "",instruction:$0.instruction ?? "",road:$0.road ?? "",action:$0.action ?? "",distance:$0.distance,duration:$0.duration)}
                let pathLine=path.polyline?.trimmingCharacters(in:.whitespacesAndNewlines) ?? ""
                // A response-level polyline is unambiguous only for a single alternative.
                let fullLine=pathLine.isEmpty && paths.count==1 ? (response?.route?.polyline ?? "") : pathLine
                return .init(polyline:fullLine,distance:path.distance,duration:path.duration,segments:segments)
            }
            finish(.success(try BicycleRouteParser.routes(options,request:request)))
        } catch { finish(.failure(error)) }
    }
    @objc(AMapSearchRequest:didFailWithError:)
    func failed(_ request: Any?, error: NSError?) {
        finish(.failure(AmapShare.Fail("自行车规划失败（\(error?.code ?? 0)）：\(error?.localizedDescription ?? "请检查高德 iOS Key 权限")")))
    }
}
