import Foundation

/// 版本：跟随 App 版本（见 Config.swift）
///
/// 圆屏相关的设置（存在手机里，连上圆屏时发过去）
final class Settings: ObservableObject {
    static let shared = Settings()
    private let ud = UserDefaults.standard

    /// 风格编号（和固件一样）；列表顺序
    static let themeOrder = [1, 0, 5, 11]
    static func themeName(_ t: Int) -> String {
        switch t {
        case 11: return "龙珠雷达"
        case 0: return "原版"; case 2: return "GTA5"; case 3: return "赛博霓虹"; case 4: return "复古纸地图"
        case 5: return "极简黑白"; case 6: return "蓝图"; case 7: return "掌机像素"; case 8: return "雷达"
        case 9: return "寻宝罗盘"; case 10: return "卫星图"; default: return "罪恶都市"
        }
    }
    /// iOS v0.6 风格的一句话说明（和安卓版一样）
    static func themeDesc(_ t: Int) -> String {
        switch t {
        case 11: return "绿色雷达、路线与目标；不下载底图"
        case 0: return "高德默认底图与标注，按圆屏16色传输"
        case 2: return "深色地面、浅色道路、紫色路线"
        case 3: return "黑底青色道路、品红路线，夜里最酷"
        case 4: return "米黄纸色，白天太阳下最清楚；夜间配色时自动换成霓虹"
        case 5: return "独立黑白道路网和导航路线，不含建筑、绿地、水面和文字"
        case 6: return "深蓝底 + 跟着地图走的网格"
        case 7: return "老掌机的 4 色绿屏，大像素，帧率更高"
        case 8: return "墨绿单色 + 转动的扫描线和距离圈"
        case 9: return "六边形网格、罗盘刻度、目的地菱形脉冲"
        case 10: return "高德卫星照片（数据大，范围小一些；要填高德 key）"
        default: return "暖沙色街区、粉紫路线（默认）"
        }
    }

    static let bootBrands = ["MotoNav 默认", "本田 Honda", "雅马哈 Yamaha", "铃木 Suzuki", "川崎 Kawasaki", "宝马 BMW", "杜卡迪 Ducati", "KTM", "凯旋 Triumph", "哈雷 Harley-Davidson", "阿普利亚 Aprilia", "春风 CFMOTO", "钱江 QJMOTOR", "无极 VOGE", "升仕 ZONTES", "贝纳利 Benelli", "皇家恩菲尔德 Royal Enfield", "关闭开机动画"]
    @Published var bootBrand = UserDefaults.standard.integer(forKey: "boot_brand") {
        didSet { ud.set(bootBrand, forKey: "boot_brand"); syncBootBrand() }
    }
    @Published var bootStatus = "连接圆屏后自动同步"
    func syncBootBrand() {
        bootStatus = "等待圆屏确认（需支持开机动画的固件）"
        BleLink.shared.send("#BOOT,\(bootBrand)")
    }
    func bootReply(_ id: Int) {
        bootStatus = id == bootBrand ? "已保存，下次开机生效" : "保存失败，请重新选择"
    }

    @Published var theme: Int {
        didSet {
            ud.set(theme, forKey: "map_theme")
            BleLink.shared.send(MapStyleTransfer.command(theme))
            // 卫星图和普通地图的数据不一样：切换时重新下载
            if oldValue != theme {
                MapSampleReport.shared.failIfWaiting("风格已切换，请重新生成对照图。")
                MapFeeder.shared.resetChunk()
            }
        }
    }
    // iOS v0.2 小地图
    @Published var mapOn: Bool {
        didSet {
            ud.set(mapOn, forKey: "map_on")
            BleLink.shared.send(mapOn ? "#m,1" : "#m,0")
            MapFeeder.shared.resetChunk()
            if mapOn { MapFeeder.shared.resendAll() }
        }
    }
    @Published var buildings: Bool = MapDefaults.buildings() {
        didSet { ud.set(buildings, forKey: "map_buildings"); MapFeeder.shared.resetChunk() }
    }
    @Published private(set) var mapSource: Int { didSet { ud.set(mapSource, forKey: "map_src"); MapFeeder.shared.resetChunk() } }
    /// 走过的路线：1 变灰，2 擦除
    @Published var trail: Int { didSet { ud.set(trail, forKey: "map_trail"); BleLink.shared.send("#r,\(trail)") } }
    /// 清爽模式：小地图保留，去掉红绿灯、电子眼、安全提示、测速牌和行程信息
    @Published var clean: Bool { didSet { ud.set(clean, forKey: "clean_mode"); BleLink.shared.send(clean ? "#w,1" : "#w,0") } }
    /// 圆屏方向（度，0~359，顺时针）。iOS v0.5 起 360° 可调：发 "#R,最接近的90°档,角度"，
    /// 固件 v7.10.1 起解析第二项精确角度；旧固件只认第一项，转到最接近的 90°
    @Published var rotationDeg: Int { didSet { ud.set(rotationDeg, forKey: "screen_rot_deg"); sendRotation() } }
    var rotationMsg: String {
        let d = ((rotationDeg % 360) + 360) % 360
        return "#R,\(((d + 45) / 90) & 3),\(d)"
    }
    private var rotTimer: Timer?
    private var rotLastSent = Date.distantPast
    /// 转圆盘时最多每 0.15 秒发一次（圆屏跟着手指实时转，又不会每一度都发），停下时补发最后的角度
    private func sendRotation() {
        rotTimer?.invalidate()
        let wait = 0.15 - Date().timeIntervalSince(rotLastSent)
        if wait <= 0 {
            rotLastSent = Date()
            BleLink.shared.send(rotationMsg)
            return
        }
        rotTimer = Timer.scheduledTimer(withTimeInterval: wait, repeats: false) { [weak self] _ in
            guard let self = self else { return }
            self.rotLastSent = Date()
            BleLink.shared.send(self.rotationMsg)
        }
    }
    /// 手动亮度（关掉“亮度自动”时用）
    @Published var brightness: Double { didSet { ud.set(brightness, forKey: "bright"); if !brightAuto { sendBrightness() } } }
    /// iOS v0.4 亮度跟着昼夜自动（和安卓版一样：日出后用白天亮度，日落后用夜间亮度）
    @Published var brightAuto: Bool { didSet { ud.set(brightAuto, forKey: "bright_auto"); sendBrightness() } }
    @Published var brightDay: Double { didSet { ud.set(brightDay, forKey: "bright_day"); if brightAuto { sendBrightness() } } }
    @Published var brightNight: Double { didSet { ud.set(brightNight, forKey: "bright_night"); if brightAuto { sendBrightness() } } }
    @Published var brightDesc = ""
    /// 夜间配色：0 自动（按当地日出日落切换），1 一直白天，2 一直夜间（iOS v0.3 起；原来只有开 / 关）
    @Published var nightMode: Int { didSet { ud.set(nightMode, forKey: "night_mode"); applyDayNight(force: true) } }
    @Published var nightDesc = ""                   // 例如“日出 06:12，日落 18:40”
    /// 现在用不用夜间配色（按上面的设置算出来的）；手机上的导航地图、选路线地图也跟着它切换白天 / 夜间样式
    @Published private(set) var paletteNight = false
    /// 规划路线时不走高速（摩托车默认打开）
    @Published var avoidHighway: Bool { didSet { ud.set(avoidHighway, forKey: "avoid_hw") } }
    @Published var curve: Bool { didSet { ud.set(curve, forKey: "map_curve"); BleLink.shared.send(curve ? "#Q,1" : "#Q,0") } }
    @Published var duck: Bool { didSet { ud.set(duck, forKey: "voice_duck") } }
    /// iOS v0.6 导航播报静音（导航画面上的按钮，下次还记得）
    @Published var musicHID: Bool = UserDefaults.standard.object(forKey: "music_hid") as? Bool ?? true {
        didSet {
            ud.set(musicHID, forKey: "music_hid")
            MusicBridge.shared.syncMode()
            MusicBridge.shared.refresh(force: true)
        }
    }
    /// Optional synced lyrics. Disabled on a fresh install and after upgrading existing installs.
    @Published var musicLyrics: Bool = UserDefaults.standard.bool(forKey: "music_lyrics") {
        didSet {
            ud.set(musicLyrics, forKey: "music_lyrics")
            MusicBridge.shared.syncLyrics(force: true)
            MusicBridge.shared.refresh(force: true)
        }
    }
    @Published var progressRing = UserDefaults.standard.object(forKey: "trip_prog") as? Bool ?? true {
        didSet { ud.set(progressRing,forKey:"trip_prog"); BleLink.shared.send(progressRing ? "#M1" : "#M0") }
    }
    @Published var leadMs = UserDefaults.standard.object(forKey: "lead_ms") as? Int ?? 300 {
        didSet { ud.set(leadMs,forKey:"lead_ms"); BleLink.shared.send("#L\(leadMs)") }
    }
    @Published var voiceMuted: Bool { didSet { ud.set(voiceMuted, forKey: "nav_muted") } }

    // Key 设置：用户在 App 里填自己的 key，只存在这台手机上。iOS v0.7 起代码里不再内置任何 key，没填时打开 App 会提醒
    /// 高德 key（iOS 平台，绑定这个 App 的 Bundle ID）：填好后 applyAmapKey() 马上交给高德
    @Published var amapKey: String { didSet { ud.set(amapKey.trimmingCharacters(in: .whitespacesAndNewlines), forKey: "key_amap") } }
    var amapKeyInUse: String { amapKey.trimmingCharacters(in: .whitespacesAndNewlines) }
    /// 高德 key 没填（打开 App 时提醒）
    var keysMissing: Bool { amapKeyInUse.isEmpty }

    /// 把高德 key 交给高德：搜索服务换成新 key 重建，导航用的 key 也一起换
    func applyAmapKey() {
        AMapServices.shared().apiKey = amapKeyInUse
        SearchService.shared.rebuildAPI()
        NaviBridge.shared.rebuildManager()
        MapFeeder.shared.resetChunk()
    }
    /// 界面上显示用：只露出头尾几位
    static func mask(_ k: String) -> String {
        if k.isEmpty { return "没填" }
        if k.count <= 10 { return "已填" }
        return "已填 \(k.prefix(4))…\(k.suffix(4))"
    }

    private init() {
        amapKey = ud.string(forKey: "key_amap") ?? ""
        theme = [1, 0, 5, 11].contains(ud.integer(forKey: "map_theme")) && ud.object(forKey: "map_theme") != nil ? ud.integer(forKey: "map_theme") : 1
        rotationDeg = ud.object(forKey: "screen_rot_deg") as? Int
            ?? ((ud.object(forKey: "screen_rot") as? Int ?? 0) & 3) * 90   // 老版本只存了 0~3 档
        brightness = ud.object(forKey: "bright") as? Double ?? 200
        brightAuto = ud.object(forKey: "bright_auto") as? Bool ?? true
        brightDay = ud.object(forKey: "bright_day") as? Double ?? 255
        brightNight = ud.object(forKey: "bright_night") as? Double ?? 70
        if let m = ud.object(forKey: "night_mode") as? Int { nightMode = m }
        else { nightMode = ud.bool(forKey: "night") ? 2 : 0 }       // 老版本的“夜间配色”开关：开 = 一直夜间
        avoidHighway = ud.object(forKey: "avoid_hw") as? Bool ?? true
        curve = ud.bool(forKey: "map_curve")
        duck = ud.object(forKey: "voice_duck") as? Bool ?? true
        voiceMuted = ud.bool(forKey: "nav_muted")
        mapOn = ud.object(forKey: "map_on") as? Bool ?? true
        mapSource = MapDefaults.source(ud)
        trail = ud.object(forKey: "map_trail") as? Int ?? 1
        clean = ud.bool(forKey: "clean_mode")
    }

    private var brightTimer: Timer?
    private var brightSent = -1
    private var isDayNow = true                         // 最近一次按日出日落算出来的

    /// 现在该用的亮度
    var brightnessInUse: Int { brightAuto ? Int(isDayNow ? brightDay : brightNight) : Int(brightness) }

    private func sendBrightness() {
        brightTimer?.invalidate()                       // 拖滑块时别每一下都发
        brightTimer = Timer.scheduledTimer(withTimeInterval: 0.15, repeats: false) { [weak self] _ in
            self?.applyDayNight(force: true)
        }
    }

    /// 连上圆屏时把所有设置发一遍
    func syncBuildingContinuity() {
        BleLink.shared.send(theme==1 && buildings && mapOn && mapSource==MapFeeder.SRC_AMAP ? "#BC,1":"#BC,0")
    }
    func pushAll() {
        let b = BleLink.shared
        syncBootBrand()
        MusicBridge.shared.syncMode()
        MusicBridge.shared.refresh(force: true)
        applyDayNight(force: true)                      // 亮度、夜间配色
        b.send(MapStyleTransfer.command(theme))
        syncBuildingContinuity()
        b.send(rotationMsg)
        b.send(curve ? "#Q,1" : "#Q,0")
        b.send(mapOn ? "#m,1" : "#m,0")
        b.send("#r,\(trail)")
        b.send(progressRing ? "#M1" : "#M0")
        b.send("#L\(leadMs)")
        b.send(clean ? "#w,1" : "#w,0")
    }

    // MARK: - 昼夜自动切换（和安卓版 Brightness.kt 一样：每分钟按当地日出日落检查一次）
    // 夜间配色（自动模式）和亮度（自动时）都跟着它变

    private var nightSent = -1
    private var dayNightTimer: Timer?

    /// App 启动时调用一次
    func startDayNight() {
        dayNightTimer?.invalidate()
        dayNightTimer = Timer.scheduledTimer(withTimeInterval: 60, repeats: true) { [weak self] _ in self?.applyDayNight() }
        applyDayNight()
    }

    /// 算出现在是白天还是夜间：夜间配色、亮度变了就告诉圆屏
    func applyDayNight(force: Bool = false) {
        let loc = NaviBridge.shared.lastLocation ?? LocationService.shared.location
        let r = SunTimes.isDaytime(lat: loc?.coordinate.latitude, lng: loc?.coordinate.longitude)
        isDayNow = r.day
        let on: Bool
        switch nightMode {
        case 1: on = false; nightDesc = "一直用白天配色"
        case 2: on = true; nightDesc = "一直用夜间配色"
        default:
            on = !r.day
            nightDesc = r.desc + (on ? "（现在是夜间配色）" : "（现在是白天配色）")
        }
        if paletteNight != on { paletteNight = on }
        let v = on ? 1 : 0
        if v != nightSent || force {
            nightSent = v
            BleLink.shared.send("#n,\(v)")
        }
        let lv = brightnessInUse
        brightDesc = brightAuto ? "\(r.desc)，现在用\(r.day ? "白天" : "夜间")亮度 \(lv)" : ""
        if lv != brightSent || force {
            brightSent = lv
            BleLink.shared.send("#b,\(lv)")
        }
    }
}

/// 当天日出、日落时间（NOAA 简化公式，精度在几分钟以内，够切换昼夜配色用；和安卓版一样）
enum SunTimes {
    /// 现在是不是白天；还没有位置时按 6:30~18:30 算白天
    static func isDaytime(lat: Double?, lng: Double?, now: Date = Date()) -> (day: Bool, desc: String) {
        let cal = Calendar.current
        let nowMin = cal.component(.hour, from: now) * 60 + cal.component(.minute, from: now)
        guard let lat = lat, let lng = lng else { return (nowMin >= 390 && nowMin < 1110, "白天 6:30~18:30（还没有定位）") }
        guard let t = sunTimes(lat, lng, now) else { return (true, "极昼/极夜，按白天处理") }
        let (rise, set) = t
        func fmt(_ m: Int) -> String { String(format: "%02d:%02d", m / 60, m % 60) }
        return (nowMin >= rise && nowMin < set, "日出 \(fmt(rise))，日落 \(fmt(set))")
    }

    /// 当天日出、日落的本地时间（从 0 点起的分钟数）
    static func sunTimes(_ latitude: Double, _ longitude: Double, _ date: Date) -> (Int, Int)? {
        let dayOfYear = Double(Calendar.current.ordinality(of: .day, in: .year, for: date) ?? 1)
        let tzHours = Double(TimeZone.current.secondsFromGMT(for: date)) / 3600.0
        let rad = Double.pi / 180
        func calc(_ rising: Bool) -> Double? {
            let lngHour = longitude / 15.0
            let t = dayOfYear + ((rising ? 6.0 : 18.0) - lngHour) / 24.0
            let m = 0.9856 * t - 3.289
            var l = m + 1.916 * sin(m * rad) + 0.020 * sin(2 * m * rad) + 282.634
            l = (l.truncatingRemainder(dividingBy: 360) + 360).truncatingRemainder(dividingBy: 360)
            var ra = atan(0.91764 * tan(l * rad)) / rad
            ra = (ra.truncatingRemainder(dividingBy: 360) + 360).truncatingRemainder(dividingBy: 360)
            let lQuad = floor(l / 90.0) * 90.0, raQuad = floor(ra / 90.0) * 90.0
            ra = (ra + (lQuad - raQuad)) / 15.0
            let sinDec = 0.39782 * sin(l * rad)
            let cosDec = cos(asin(sinDec))
            let cosH = (cos(90.833 * rad) - sinDec * sin(latitude * rad)) / (cosDec * cos(latitude * rad))
            if cosH > 1 || cosH < -1 { return nil }
            var h = rising ? 360 - acos(cosH) / rad : acos(cosH) / rad
            h /= 15.0
            let localT = h + ra - 0.06571 * t - 6.622
            let ut = ((localT - lngHour).truncatingRemainder(dividingBy: 24) + 24).truncatingRemainder(dividingBy: 24)
            return ((ut + tzHours).truncatingRemainder(dividingBy: 24) + 24).truncatingRemainder(dividingBy: 24)
        }
        guard let rise = calc(true), let set = calc(false) else { return nil }
        return (Int(rise * 60), Int(set * 60))
    }
}
