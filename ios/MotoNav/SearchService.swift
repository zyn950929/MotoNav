import Foundation
import ObjectiveC
import CoreLocation

/// 版本：跟随 App 版本（见 Config.swift）
///
/// 目的地搜索：高德“输入提示”接口，优先显示附近的地方。
/// iOS v0.2.1 起只在按“搜索”时才查（原来边打字边查，拼音输入时每个字母都查一次，太费高德额度）；
/// 同样的关键词这次打开 App 查过就直接用上次的结果，不再查。
final class SearchService: NSObject, ObservableObject {
    static let shared = SearchService()

    struct Place: Identifiable {
        let id = UUID()
        let name: String
        let address: String
        let lat: Double
        let lng: Double
    }

    @Published var results: [Place] = []
    @Published var status = ""
    private var api: AMapSearchAPI?
    private var activeSearch=""
    private var cache: [String: [Place]] = [:]         // 关键词 → 结果（只存在内存里）

    override init() {
        super.init()
        // 和 NaviBridge 一样：运行时登记协议，避免 Swift 方法名冲突
        class_addProtocol(SearchService.self, AMapSearchDelegate.self)
        rebuildAPI()
    }

    /// iOS v0.7 填了（或换了）高德 key 后重建搜索服务，不用重启 App
    func rebuildAPI() {
        api?.delegate = nil
        api = AMapSearchAPI()
        if let d = (self as AnyObject) as? AMapSearchDelegate { api?.delegate = d }
        cache = [:]
    }

    /// 按了“搜索”时调用（每按一次最多查一次高德）
    func search(_ text: String, near: (Double,Double)? = nil) {
        let t = text.trimmingCharacters(in: .whitespacesAndNewlines)
        if t.isEmpty { clear(); return }
        let around=near ?? LocationService.shared.location.map{l -> (Double,Double) in
            let g=MapGeo.wgsToGcj(l.coordinate.latitude,l.coordinate.longitude);return(g.lat,g.lng)
        }
        let location=around.map{String(format:"%.6f,%.6f",$0.1,$0.0)} ?? ""
        let searchKey=t+"|"+location
        activeSearch=searchKey
        if let r = cache[searchKey] {                           // 查过的不再花额度
            results = r
            status = r.isEmpty ? "没找到，换个说法试试" : ""
            return
        }
        let req = AMapInputTipsSearchRequest()
        req.keywords = t
        if !location.isEmpty {req.location=location}
        status = "搜索中…"
        api?.aMapInputTipsSearch(req)
    }

    func clear() { activeSearch="";results = []; status = "" }

    @objc(onInputTipsSearchDone:response:)
    func mnTipsDone(_ request: AMapInputTipsSearchRequest?, response: AMapInputTipsSearchResponse?) {
        let searchKey=(request?.keywords ?? "")+"|"+(request?.location ?? "")
        guard searchKey==activeSearch else{return}
        let tips = response?.tips ?? []
        results = tips.compactMap { (tip: AMapTip) -> Place? in
            guard let p = tip.location else { return nil }          // 没有坐标的是“搜索建议词”，跳过
            return Place(name: tip.name ?? "", address: (tip.district ?? "") + (tip.address ?? ""),
                         lat: Double(p.latitude), lng: Double(p.longitude))
        }
        cache[searchKey] = results
        status = results.isEmpty ? "没找到，换个说法试试" : ""
    }

    @objc(AMapSearchRequest:didFailWithError:)
    func mnSearchFail(_ request: Any?, error: NSError?) {
        let code = error?.code ?? 0
        let bid = Bundle.main.bundleIdentifier ?? ""
        switch code {
        case 1008, 1009:    // iOS v0.7.2 key 绑定的 Bundle ID / 平台和这个 App 对不上：说清楚去哪改
            status = "搜索失败（\(code)）：高德 key 和这个 App 对不上。去高德开放平台控制台，把这个 key 的服务平台设为「iOS 平台」、" +
                "安全码（Bundle ID）改成 \(bid)，保存后等一两分钟再搜"
        case 1002, 1013:
            status = "搜索失败（\(code)）：高德 key 不对或已经被删除，到「Key 设置」重新填一个"
        case 1004, 1010:
            status = "搜索失败（\(code)）：这个高德 key 今天的搜索次数用完了，明天再试或换一个 key"
        case 1003, 1012:
            status = "搜索失败（\(code)）：这个高德 key 没有搜索权限，到高德控制台检查这个 key 开通的服务"
        default:
            status = "搜索失败：\(error?.localizedDescription ?? "未知错误")（\(code)）"
        }
    }
}

/// iOS v0.4 最近去过（和安卓版一样：最多 12 个，同名且相距 50 米以内算同一个地方，存在手机上）
final class History: ObservableObject {
    static let shared = History()
    @Published private(set) var places: [SearchService.Place] = []
    private let key = "nav_history"

    private init() {
        let arr = UserDefaults.standard.array(forKey: key) as? [[String: Any]] ?? []
        places = arr.compactMap { o in
            guard let n = o["n"] as? String, let la = o["la"] as? Double, let lo = o["lo"] as? Double else { return nil }
            return SearchService.Place(name: n, address: o["a"] as? String ?? "", lat: la, lng: lo)
        }
    }

    func add(_ p: SearchService.Place) {
        var list = [p]
        for o in places where !(o.name == p.name && NaviBridge.distM(o.lat, o.lng, p.lat, p.lng) < 50) {
            if list.count >= 12 { break }
            list.append(o)
        }
        places = list
        save()
    }

    func clear() { places = []; save() }

    private func save() {
        UserDefaults.standard.set(places.map { ["n": $0.name, "a": $0.address, "la": $0.lat, "lo": $0.lng] as [String: Any] }, forKey: key)
    }
}

/// 手机当前位置（搜索时优先附近、查天气、规划路线起点用）。导航中直接用高德导航的定位。
/// iOS v0.3 巡航模式时切成高精度、锁屏后台也继续定位，位置交给 NaviBridge（小地图、车速、测速牌）。
final class LocationService: NSObject, ObservableObject, CLLocationManagerDelegate {
    static let shared = LocationService()
    private let lm = CLLocationManager()
    @Published var location: CLLocation?

    override init() {
        super.init()
        lm.delegate = self
        lm.desiredAccuracy = kCLLocationAccuracyHundredMeters
    }

    func start() {
        lm.requestWhenInUseAuthorization()
        lm.startUpdatingLocation()
    }

    /// 巡航时打开：高精度、后台也继续
    func setPrecise(_ on: Bool) {
        lm.desiredAccuracy = on ? kCLLocationAccuracyBestForNavigation : kCLLocationAccuracyHundredMeters
        lm.distanceFilter = kCLDistanceFilterNone
        lm.activityType = on ? (WebMotorSession.shared.running && WebMotorSession.shared.route?.request.bicycle == true ? .fitness : .automotiveNavigation) : .other
        lm.pausesLocationUpdatesAutomatically = !on
        lm.allowsBackgroundLocationUpdates = on
        lm.startUpdatingLocation()
    }

    func locationManager(_ manager: CLLocationManager, didUpdateLocations locations: [CLLocation]) {
        location = locations.last
        if let l = locations.last { RideRecorder.shared.fix(l); WebMotorSession.shared.onFix(l) }
        if NaviBridge.shared.cruising, let l = locations.last { NaviBridge.shared.onCruiseLocation(l) }
    }

    func locationManagerDidChangeAuthorization(_ manager: CLLocationManager) {
        if manager.authorizationStatus == .authorizedWhenInUse || manager.authorizationStatus == .authorizedAlways {
            manager.startUpdatingLocation()
        }
    }
}
