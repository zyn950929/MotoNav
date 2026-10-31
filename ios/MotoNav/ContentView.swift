import SwiftUI
import CoreLocation
import AVFoundation

/// 版本：跟随 App 版本（见 Config.swift）
///
/// 主界面：圆屏连接状态、搜索目的地 / 导入高德分享路线、选路线、开始导航、巡航、圆屏设置、Key 设置、调试信息
/// （罪恶都市风深色界面）
struct ContentView: View {
    @ObservedObject var ble = BleLink.shared
    @ObservedObject var navi = NaviBridge.shared
    @ObservedObject var independent = WebMotorSession.shared
    @ObservedObject var search = SearchService.shared
    @ObservedObject var settings = Settings.shared
    @ObservedObject var weather = Weather.shared
    @ObservedObject var map = MapFeeder.shared
    @ObservedObject var history = History.shared
    @ObservedObject var favorites = Favorites.shared
    @State private var page = 0
    @State private var showWebMotor = false
    @State private var independentMode: IndependentMode = .motorcycle
    @State private var showRides = false
    @AppStorage("voice_rate") private var voiceRate = Double(AVSpeechUtteranceDefaultSpeechRate)
    @AppStorage("voice_pitch") private var voicePitch = 1.0
    @AppStorage("voice_id") private var voiceID = ""

    @State private var query = ""
    @State private var dest: SearchService.Place?
    @State private var showNavi = false
    @State private var showDebug = false
    @State private var showKeys = false                 // iOS v0.7 填写地图 Key 页面
    private static var keyPrompted = false              // 这次打开 App 已经提醒过填 key 了
    @State private var shareBusy = false
    @State private var shareStatus = ""
    @State private var pendingShare=""
    @State private var shareImportID=UUID()
    @State private var showShareVerification=false
    @State private var showWaypointEditor=false
    @State private var cruiseNote = ""
    @State private var addingVia = false               // 正在搜途经点

    static let bg = Color(red: 0.055, green: 0.04, blue: 0.08)
    static let card = Color(red: 0.10, green: 0.08, blue: 0.14)
    static let stroke = Color(red: 0.17, green: 0.14, blue: 0.22)
    static let pink = Color(red: 1.0, green: 0.31, blue: 0.71)
    static let cyan = Color(red: 0.26, green: 0.88, blue: 0.94)
    static let sub = Color(red: 0.62, green: 0.58, blue: 0.67)

    var body: some View {
        VStack(spacing: 0) {
            activeNavigationBanner
            ZStack {
                ForEach(0..<5) { index in
                    ScrollView {
                        VStack(alignment: .leading, spacing: 16) {
                            header
                            if index == 0 {
                                cruiseCard
                                statusCard
                                HStack {
                                    Button("摩托车路线规划") { if !independent.running {independent.arrived=false}; independentMode = .motorcycle; showWebMotor = true }.foregroundColor(Self.pink)
                                    Spacer()
                                    Button("自行车路线规划") { if !independent.running {independent.arrived=false}; independentMode = .bicycle; showWebMotor = true }.foregroundColor(Self.cyan)
                                }
                                destCard
                                favoritesCard
                                Button("骑行轨迹") { showRides = true }.foregroundColor(Self.cyan)
                            } else if index == 1 { displayCard }
                            else if index == 2 { voiceCard }
                            else if index == 3 { statusCard; settingsCard; debugCard }
                            else { keyCard; Text(weather.detail).foregroundColor(Self.sub) }
                        }.padding(16)
                    }.opacity(page == index ? 1 : 0).allowsHitTesting(page == index).accessibilityHidden(page != index)
                }
            }
            HStack(spacing: 0) {
                ForEach(0..<5) { index in
                    Button { hideKeyboard(); page = index } label: {
                        VStack(spacing: 4) {
                            Image(systemName: ["magnifyingglass", "map", "speaker.wave.2", "gearshape", "key"][index])
                            Text(["首页搜索", "圆屏显示", "导航语音", "圆屏设置", "地图 Key"][index]).font(.system(size: 10))
                        }.frame(maxWidth: .infinity).padding(.vertical, 10).foregroundColor(page == index ? Self.pink : Self.sub)
                    }
                }
            }.background(Self.card)
        }
        .background(ContentView.bg.ignoresSafeArea())
        .fullScreenCover(isPresented: $showWebMotor) { WebMotorView(mode:independentMode) }
        .sheet(isPresented: $showRides) { NavigationView { RideHistoryView() } }
        .fullScreenCover(isPresented: $showNavi) { NaviScreen() }
        .sheet(isPresented: $showKeys) { KeySetupView() }
        .sheet(isPresented:$showShareVerification){
            ShareVerificationView(page:AmapShare.findUrl(pendingShare).flatMap{URL(string:$0)}){
                let text=pendingShare
                DispatchQueue.main.asyncAfter(deadline:.now()+0.35){importShare(text)}
            }
        }
        .sheet(isPresented:$showWaypointEditor){
            RouteWaypointEditor(points:navi.vias.map{MotorPoint(lat:$0.lat,lng:$0.lng,name:$0.name)},near:nil){navi.replaceVias($0)}
        }
        .onAppear {                                    // iOS v0.7 打开 App 时，高德或高德 key 没填就提醒一次
            if !ContentView.keyPrompted && settings.keysMissing {
                ContentView.keyPrompted = true
                showKeys = true
            }
        }
        .onOpenURL { url in                            // iOS v0.4 高德里“分享到 MotoNav”：motonav://share?t=分享的文字
            guard url.scheme == "motonav",
                  let t = URLComponents(url: url, resolvingAgainstBaseURL: false)?.queryItems?.first(where: { $0.name == "t" })?.value
            else { return }
            importShare(t)
        }
    }

    private var activeNavigationBanner: some View {
        Group {
            if navi.navigating || independent.running {
                HStack {
                    VStack(alignment:.leading,spacing:3) {
                        Text("导航继续运行中").bold()
                        Text(independent.running ? independent.instruction : "\(navi.uiDir) · \(navi.uiDist) 米")
                            .font(.caption).lineLimit(1)
                    }
                    Spacer()
                    Button("回到导航") {
                        if independent.running { showWebMotor=true } else { showNavi=true }
                    }.foregroundColor(Self.pink)
                }.padding(12).background(Self.card).foregroundColor(.white)
            }
        }
    }

    // MARK: 标题
    private var header: some View {
        VStack(alignment: .leading, spacing: 2) {
            Text("MotoNav")
                .font(.system(size: 34, weight: .heavy, design: .rounded))
                .italic()
                .foregroundStyle(LinearGradient(colors: [ContentView.pink, ContentView.cyan], startPoint: .leading, endPoint: .trailing))
            Text("圆屏导航 · \(APP_VERSION)").font(.caption).foregroundColor(ContentView.sub)
        }
        .padding(.top, 8)
    }

    // MARK: 圆屏状态
    private var statusCard: some View {
        CardView {
            HStack {
                Circle().fill(ble.isReady ? Color.green : Color.orange).frame(width: 10, height: 10)
                Text("圆屏").foregroundColor(.white).bold()
                Spacer()
                Button(ble.isReady ? "断开" : "连接") { ble.isReady ? ble.disconnect() : ble.connect() }
                    .foregroundColor(ContentView.cyan)
            }
            Text(ble.status).font(.footnote).foregroundColor(ContentView.sub)
            // iOS v0.6 圆屏画小地图时每 2 秒回报一次帧率
            if ble.isReady, Date().timeIntervalSince(ble.statAt) < 10, !ble.screenFps.isEmpty {
                Text("小地图 \(ble.screenFps) 帧/秒").font(.footnote).foregroundColor(ContentView.sub)
            }
            if navi.navigating {
                Text("导航中：\(navi.uiDir) \(navi.uiDist) 米 · \(navi.uiSpeed) km/h").font(.footnote).foregroundColor(ContentView.cyan)
                if navi.uiRemainM > 0 {
                    Text("剩余 \(NaviBridge.distText(navi.uiRemainM)) · 约 \(NaviBridge.durText(navi.uiRemainS)) · \(ContentView.etaText(navi.uiRemainS)) 到达")
                        .font(.footnote).foregroundColor(.white)
                }
                Button("回到导航画面") { showNavi = true }.foregroundColor(ContentView.pink)
            }
            Text("小地图：\(map.shortStatus) · 高德 · \(Settings.themeName(settings.theme))")
                .font(.caption).foregroundColor(ContentView.sub)
        }
    }

    // MARK: 去哪儿
    private var destCard: some View {
        CardView {
            Text("去哪儿").foregroundColor(.white).bold()
            HStack(spacing: 10) {
                // 只在按键盘上的“搜索”或右边按钮时才查高德（边打字边查太费额度）
                TextField(addingVia ? "搜索途经点（比如加油站）" : "搜索地点、地址", text: $query)
                    .textFieldStyle(.plain)
                    .padding(12)
                    .background(RoundedRectangle(cornerRadius: 12).fill(ContentView.bg))
                    .foregroundColor(.white)
                    .submitLabel(.search)
                    .onSubmit { if !needAmapKey() { search.search(query) } }
                    .onChange(of: query) { if $0.isEmpty { search.clear() } }
                Button("搜索") { hideKeyboard(); if !needAmapKey() { search.search(query) } }
                    .font(.headline)
                    .foregroundColor(.white)
                    .padding(.horizontal, 16).padding(.vertical, 12)
                    .background(RoundedRectangle(cornerRadius: 12).fill(ContentView.pink))
            }
            // iOS v0.3 导入高德 App 分享的路线（比如高德里规划好的摩托车路线）
            if !addingVia {
            Button { pasteShare() } label: {
                HStack(spacing: 6) {
                    Image(systemName: "doc.on.clipboard")
                    Text(shareBusy ? "正在读取分享的路线…" : "粘贴高德分享的路线")
                }
                .font(.subheadline).foregroundColor(Color.orange)
            }
            .disabled(shareBusy)
            Text("在高德里规划好路线 → 分享 → 选 MotoNav；或者复制链接后点上面这行").font(.caption2).foregroundColor(ContentView.sub)
            }
            if !shareStatus.isEmpty { Text(shareStatus).font(.footnote).foregroundColor(ContentView.sub) }
            if !pendingShare.isEmpty && !shareBusy {
                Button("网页验证后重试"){showShareVerification=true}.foregroundColor(Self.cyan)
            }
            if addingVia{Button("搜索附近加油站"){query="加油站";search.search(query)}.foregroundColor(Self.cyan)}
            if !search.status.isEmpty { Text(search.status).font(.footnote).foregroundColor(ContentView.sub) }
            ForEach(search.results.prefix(8)) { p in
                Button { choose(p) } label: {
                    HStack(alignment: .center, spacing: 8) {
                        VStack(alignment: .leading, spacing: 2) {
                            Text(p.name).foregroundColor(.white)
                            Text(p.address).font(.caption).foregroundColor(ContentView.sub).lineLimit(1)
                        }
                        Spacer(minLength: 4)
                        if let d = distanceTo(p) { Text(d).font(.caption).foregroundColor(ContentView.cyan) }
                    }
                    .frame(maxWidth: .infinity, alignment: .leading)
                    .padding(.vertical, 6)
                }
                Divider().background(ContentView.stroke)
            }
            // iOS v0.4 最近去过（还没选目的地、或者正在选途经点时显示）
            if search.results.isEmpty && (dest == nil || addingVia) && !history.places.isEmpty {
                Text(addingVia ? "从最近去过里选途经点" : "最近去过").font(.caption).foregroundColor(ContentView.sub).padding(.top, 4)
                ForEach(history.places.prefix(8)) { p in
                    Button { choose(p) } label: {
                        HStack(alignment: .center, spacing: 8) {
                            VStack(alignment: .leading, spacing: 2) {
                                Text(p.name).foregroundColor(.white)
                                if !p.address.isEmpty { Text(p.address).font(.caption).foregroundColor(ContentView.sub).lineLimit(1) }
                            }
                            Spacer(minLength: 4)
                            if let d = distanceTo(p) { Text(d).font(.caption).foregroundColor(ContentView.cyan) }
                        }
                        .frame(maxWidth: .infinity, alignment: .leading)
                        .padding(.vertical, 4)
                    }
                    Divider().background(ContentView.stroke)
                }
                Button("清除记录") { history.clear() }.font(.caption).foregroundColor(ContentView.sub)
            }
            if let d = dest {
                HStack {
                    Text("目的地：\(d.name)").font(.footnote).foregroundColor(.white)
                    Button("收藏") { favorites.add(d) }.foregroundColor(Self.cyan)
                    Spacer()
                    if !navi.navigating {
                        Button("换目的地") {
                            dest = nil; query = ""; addingVia = false; shareStatus = ""
                            search.clear()
                            navi.clearPlan()
                        }
                        .font(.caption).foregroundColor(ContentView.cyan)
                    }
                }
                if let sr = navi.shareRoute {
                    Text("按高德分享的\(sr.kind)路线走（原路线 \(NaviBridge.distText(Int(sr.total)))）").font(.caption).foregroundColor(Color.orange)
                    if !navi.shareMatch.isEmpty { Text(navi.shareMatch).font(.caption).foregroundColor(ContentView.sub) }
                }
                if navi.shareRoute == nil {                 // 途经点（分享的路线自带途经点）
                    HStack(spacing: 12) {
                        Text(navi.vias.isEmpty ? "途经：无" : "途经：" + navi.vias.map { $0.name }.joined(separator: " → "))
                            .font(.caption).foregroundColor(ContentView.sub).lineLimit(2)
                        Spacer()
                        if addingVia {
                            Button("取消") { addingVia = false; query = d.name; search.clear() }
                                .font(.caption).foregroundColor(ContentView.sub)
                        } else {
                            Button(navi.vias.isEmpty ? "+ 途经点":"编辑途经点") { showWaypointEditor=true }
                                .font(.caption).foregroundColor(navi.vias.count >= 3 ? ContentView.sub : ContentView.cyan)
                                .disabled(navi.navigating)
                            if !navi.vias.isEmpty {
                                Button("清除") { navi.clearVias() }.font(.caption).foregroundColor(ContentView.sub)
                            }
                        }
                    }
                    if addingVia {
                        Text("在上面搜索途经点（比如加油站），或者从最近去过里选；最多 3 个，按顺序经过")
                            .font(.caption2).foregroundColor(Color.orange)
                    }
                }
                Picker("模拟速度",selection:$navi.preferredSimulationSpeed){ForEach(NavigationSpeed.values,id:\.self){Text("\($0) km/h").tag($0)}}.pickerStyle(.segmented)
                Toggle("不走高速", isOn: $settings.avoidHighway)
                    .foregroundColor(.white).tint(ContentView.pink)
                    .onChange(of: settings.avoidHighway) { _ in navi.replan() }
                if navi.calculating || navi.routeOptions.isEmpty {
                    Text(navi.calculating ? "正在规划路线…" : navi.info).font(.footnote).foregroundColor(ContentView.sub)
                }
                if navi.planFailed && !navi.calculating {   // iOS v0.6 规划失败 / 超时
                    Button { navi.replan() } label: {
                        Label("重新规划", systemImage: "arrow.clockwise").font(.subheadline.bold()).foregroundColor(ContentView.cyan)
                    }
                }
                if !navi.routeOptions.isEmpty {
                    RouteMapView(routes: navi.routeOptions, selected: navi.selectedRouteID,
                                 ghost: navi.shareRoute?.pts.map { CLLocationCoordinate2D(latitude: $0.lat, longitude: $0.lng) } ?? [],
                                 dest: navi.destCoord,
                                 vias: navi.vias.map { CLLocationCoordinate2D(latitude: $0.lat, longitude: $0.lng) },
                                 night: settings.paletteNight)
                        .frame(height: 230)
                        .clipShape(RoundedRectangle(cornerRadius: 14))
                    HStack(spacing: 8) {
                        ForEach(navi.routeOptions) { o in
                            RouteCard(option: o, selected: o.id == navi.selectedRouteID)
                                .onTapGesture { navi.select(routeID: o.id) }
                        }
                    }
                }
                HStack(spacing: 12) {
                    PillButton(title: "开始导航", color: ContentView.pink, enabled: navi.routeReady) {
                        navi.start(emulate: false); showNavi = true
                    }
                    PillButton(title: "模拟导航", color: Color.purple, enabled: navi.routeReady) {
                        navi.start(emulate: true); showNavi = true
                    }
                }
            }
        }
    }

    /// 离现在位置的直线距离（搜索结果、最近去过右边显示）
    private func distanceTo(_ p: SearchService.Place) -> String? {
        guard let me = navi.myPosition() else { return nil }
        let d = Int(NaviBridge.distM(me.lat, me.lng, p.lat, p.lng))
        return d >= 1000 ? String(format: "%.1f 公里", Double(d) / 1000) : "\(d) 米"
    }

    /// 剩余秒数 → 到达时间“18:52”
    static func etaText(_ remainS: Int) -> String {
        let f = DateFormatter(); f.dateFormat = "HH:mm"
        return f.string(from: Date().addingTimeInterval(TimeInterval(remainS)))
    }

    /// 点了搜索结果或最近去过：选目的地，或者（正在选途经点时）加一个途经点
    private func choose(_ p: SearchService.Place) {
        guard !navi.navigating else {shareStatus="导航仍在运行；如需更换目的地，请先结束导航";return}
        guard !WebMotorSession.shared.running else { shareStatus = "请先结束网页导航"; return }
        search.clear()
        hideKeyboard()
        if addingVia {
            addingVia = false
            query = dest?.name ?? ""
            navi.addVia(name: p.name, lat: p.lat, lng: p.lng)
            return
        }
        dest = p
        query = p.name                                  // 只改文字，不会再触发搜索
        shareStatus = ""
        History.shared.add(p)
        if needAmapKey() { return }
        navi.plan(toLat: p.lat, lng: p.lng)
    }

    /// 读剪贴板里的高德分享链接，还原路线后按它规划
    private func pasteShare() {
        guard let t = UIPasteboard.general.string, AmapShare.findUrl(t) != nil else {
            shareStatus = "剪贴板里没有高德的分享链接：先在高德里打开路线 → 分享 → 复制链接"
            return
        }
        importShare(t)
    }

    /// 导入高德分享的路线（剪贴板粘贴，或者从高德“分享到 MotoNav”过来）
    private func importShare(_ t: String) {
        guard AmapShare.findUrl(t) != nil else { shareStatus = "分享的内容里没有高德链接"; return }
        if navi.navigating || WebMotorSession.shared.running { shareStatus = "正在导航：先退出导航，再导入分享的路线"; return }
        addingVia = false
        hideKeyboard()
        shareBusy = true
        shareStatus = "正在读取高德分享的路线…"
        pendingShare=t
        let sequence=UUID();shareImportID=sequence
        Task { @MainActor in
            defer{if shareImportID==sequence{shareBusy=false}}
            do{
                let r=try await AmapWebSession.loadShare(t)
                guard shareImportID==sequence,!navi.navigating,!WebMotorSession.shared.running else{shareStatus="导航状态已改变，请结束后重新导入";return}
                shareStatus="已读取：\(r.kind)路线 \(NaviBridge.distText(r.distance>0 ? r.distance:Int(r.total)))"+
                    (r.restored ? "":"（高德未确认原样还原，请核对地图）")
                pendingShare=""
                let name=r.endName.isEmpty ? "分享的目的地":r.endName
                dest=SearchService.Place(name:name,address:"高德分享的\(r.kind)路线",lat:r.endLat,lng:r.endLng)
                History.shared.add(SearchService.Place(name:name,address:"",lat:r.endLat,lng:r.endLng))
                query=name;search.clear()
                if r.kind=="自行车" || r.kind=="摩托车"{
                    navi.clearPlan();WebMotorSession.shared.route=MotorRoutes.fromShare(r)
                    independentMode=r.kind=="自行车" ? .bicycle:.motorcycle;showWebMotor=true
                }else if !needAmapKey(){navi.plan(toLat:r.endLat,lng:r.endLng,share:r)}
            }catch{
                guard shareImportID==sequence else{return}
                shareStatus="读取失败：\(error.localizedDescription)；可完成网页验证后重试"
            }
        }
    }

    // MARK: 巡航
    private var cruiseCard: some View {
        CardView {
            HStack {
                Text("巡航模式").foregroundColor(.white).bold()
                Spacer()
                if navi.cruising { Text("进行中 · \(navi.uiSpeed) km/h").font(.caption).foregroundColor(ContentView.cyan) }
            }
            Text("不设目的地：骑车时提醒前方测速、电子眼和急弯、学校、事故多发等道路安全提示，圆屏照常显示小地图")
                .font(.caption).foregroundColor(ContentView.sub)
            if navi.cruising { Text(navi.info).font(.footnote).foregroundColor(ContentView.sub) }
            PillButton(title: navi.cruising ? "结束巡航" : "开始巡航", color: navi.cruising ? Color.gray : ContentView.cyan.opacity(0.8),
                       enabled: !navi.navigating && !WebMotorSession.shared.running) {
                cruiseNote = ""
                if navi.cruising { navi.stopCruise() }
                else if !needAmapKey(), let err = navi.startCruise() { cruiseNote = err }
            }
            if !cruiseNote.isEmpty { Text(cruiseNote).font(.caption).foregroundColor(Color.orange) }
        }
    }

    // MARK: 圆屏设置
    private var settingsCard: some View {
        CardView {
            Text("圆屏设置").foregroundColor(.white).bold()
            Picker("开机动画", selection: $settings.bootBrand) {
                ForEach(Settings.bootBrands.indices, id: \.self) { i in Text(Settings.bootBrands[i]).tag(i) }
            }.tint(ContentView.cyan)
            Text(settings.bootStatus).font(.caption).foregroundColor(ContentView.sub)
            Toggle("蓝牙媒体按键（QQ 音乐等）", isOn: $settings.musicHID).foregroundColor(.white).tint(ContentView.pink)
            if settings.musicHID {
                Text(ble.mediaHIDSupported
                     ? (ble.mediaHIDReady && ble.mediaHIDMode ? "媒体按键已就绪" : ble.mediaAMSControls > 0 && ble.mediaHIDMode ? "AMS 媒体控制已就绪，HID 当前不可用" : "请在 iPhone 设置 → 蓝牙中配对 MotoNav；按圆屏配对码提示完成")
                     : "需要圆屏 HID 固件 v7.9.0 或更新版，连接后自动同步")
                    .font(.caption).foregroundColor(ContentView.sub)
                if !ble.mediaSongInfo.isEmpty {
                    Text(ble.mediaSongInfo).font(.caption).foregroundColor(ContentView.cyan)
                }
                Toggle("显示逐句歌词", isOn: $settings.musicLyrics).foregroundColor(.white).tint(ContentView.pink)
                Text("默认关闭，上方歌名、下方歌手。开启后按歌曲信息从 LRCLIB 匹配歌词，歌词显示在歌手下方；未匹配到时保留歌名和歌手。")
                    .font(.caption).foregroundColor(ContentView.sub)
                if !settings.musicLyrics && ble.mediaLyricsActive { Text("正在关闭圆屏歌词…").font(.caption).foregroundColor(ContentView.sub) }
                if settings.musicLyrics {
                    Text(ble.mediaLyricsSupported ? ble.mediaLyricsStatus : "需要圆屏固件 v7.11.0 或更新版")
                        .font(.caption).foregroundColor(ContentView.cyan)
                }
                Text("先在网易云或 QQ 音乐开始播放，再用圆屏控制。新固件通过 iPhone AMS 显示歌名、歌手和状态，信息取决于当前播放器提供的内容。HID SENT 表示按键已发送。")
                    .font(.caption).foregroundColor(ContentView.sub)
            } else {
                Button("授权 Apple Music 圆屏控制") { MusicBridge.shared.authorize() }
                    .foregroundColor(ContentView.cyan)
            }
            Text("关闭蓝牙媒体按键后，改用系统音乐 App 控制。不读取来电号码。巡航连续停车 10 分钟自动结束。")
                .font(.caption).foregroundColor(ContentView.sub)
            Text("圆屏方向").foregroundColor(.white).padding(.top, 6)
            RotationDial(degrees: $settings.rotationDeg)      // iOS v0.5.1 按住圆盘转动调角度
            Text("按住圆盘转动（顺时针为正），粉色指针是画面上方在圆屏上朝的方向，圆屏会跟着实时转。转到 0°/90°/180°/270° 附近会吸住。任意角度要圆屏固件 v7.10.1；旧固件会转到最接近的 90°")
                .font(.caption2).foregroundColor(ContentView.sub)
            // iOS v0.6 屏幕刷新速度、快速刷屏（圆屏存起来并自动重启；当前值来自圆屏的帧率回报）
            Text("刷新速度").foregroundColor(.white).padding(.top, 6)
            Picker("刷新速度", selection: Binding(get: { ble.screenMHz }, set: { ble.setScreenMHz($0) })) {
                Text("40 标准").tag(40); Text("60").tag(60); Text("80").tag(80)
            }
            .pickerStyle(.segmented)
            .disabled(!ble.isReady)
            Toggle("快速刷屏", isOn: Binding(get: { ble.screenFast == 1 }, set: { ble.setScreenFast($0) }))
                .foregroundColor(.white).tint(ContentView.pink)
                .disabled(!ble.isReady)
            Text(ble.screenMHz < 0
                 ? "越快画面撕裂越轻；花屏就切回 40、关掉快速刷屏。切换后圆屏会自动重启。现在的值要等圆屏显示过小地图才知道"
                 : "越快画面撕裂越轻；花屏就切回 40、关掉快速刷屏。切换后圆屏会自动重启")
                .font(.caption2).foregroundColor(ContentView.sub)
            Toggle("亮度跟着昼夜自动调", isOn: $settings.brightAuto).foregroundColor(.white).tint(ContentView.pink)
            if settings.brightAuto {
                HStack {
                    Text("白天").foregroundColor(.white)
                    Slider(value: $settings.brightDay, in: 5...255, step: 5).tint(ContentView.pink)
                    Text("\(Int(settings.brightDay))").font(.caption).foregroundColor(ContentView.sub).frame(width: 34)
                }
                HStack {
                    Text("夜间").foregroundColor(.white)
                    Slider(value: $settings.brightNight, in: 5...255, step: 5).tint(ContentView.pink)
                    Text("\(Int(settings.brightNight))").font(.caption).foregroundColor(ContentView.sub).frame(width: 34)
                }
                if !settings.brightDesc.isEmpty { Text(settings.brightDesc).font(.caption2).foregroundColor(ContentView.sub) }
            } else {
                HStack {
                    Text("亮度").foregroundColor(.white)
                    Slider(value: $settings.brightness, in: 5...255, step: 5).tint(ContentView.pink)
                    Text("\(Int(settings.brightness))").font(.caption).foregroundColor(ContentView.sub).frame(width: 34)
                }
            }
        }
    }

    private var displayCard: some View {
        CardView {
            Text("圆屏显示").foregroundColor(.white).bold()
            Toggle("路程进度环", isOn: $settings.progressRing).foregroundColor(.white).tint(Self.pink)
            Picker("预判", selection: $settings.leadMs) {
                Text("关闭").tag(0); Text("0.3秒").tag(300); Text("0.6秒").tag(600)
            }.pickerStyle(.segmented)
            Text("地图状态：\(map.shortStatus)").font(.caption).foregroundColor(Self.sub)
            HStack {
                Text("风格").foregroundColor(.white)
                Spacer()
                Picker("风格", selection: $settings.theme) {
                    ForEach(Settings.themeOrder, id: \.self) { t in Text(Settings.themeName(t)).tag(t) }
                }
                .tint(ContentView.cyan)
            }
            Text(Settings.themeDesc(settings.theme)).font(.caption2).foregroundColor(ContentView.sub)
            Toggle("小地图", isOn: $settings.mapOn).foregroundColor(.white).tint(ContentView.pink)
            if settings.mapOn {
                Text("地图来源：高德（普通底图和卫星图）").font(.caption).foregroundColor(ContentView.sub)
                if settings.mapSource == MapFeeder.SRC_AMAP && settings.theme == 1 {
                    Toggle("俯视建筑轮廓", isOn: $settings.buildings).foregroundColor(.white).tint(ContentView.pink)
                    Text(ble.buildingsSupported ? "圆屏支持建筑图层，默认提取高德建筑俯视轮廓" : "正在查询圆屏建筑能力；确认前先显示普通底图")
                        .font(.caption2).foregroundColor(ContentView.sub)
                    if ble.isReady && !ble.buildingsSupported {
                        Button("重新查询建筑能力") { ble.retryBuildingCapability() }
                    }
                    if !map.buildingInfo.isEmpty { Text(map.buildingInfo).font(.caption).foregroundColor(ContentView.sub) }
                }
                Text(map.offlineStatus).font(.caption).foregroundColor(ContentView.sub)
                Text("选定路线后，前台自动下载沿途地图；锁屏使用已保存地图包。请等下载完成再出发。")
                    .font(.caption2).foregroundColor(ContentView.sub)
                Text("走过的路线").foregroundColor(.white)
                Picker("走过的路线", selection: $settings.trail) {
                    Text("变灰").tag(1); Text("擦除").tag(2)
                }
                .pickerStyle(.segmented)
                Toggle("清爽模式", isOn: $settings.clean).foregroundColor(.white).tint(ContentView.pink)
                Text("清爽模式：地图保留，去掉红绿灯、电子眼、安全提示、测速牌和行程信息").font(.caption2).foregroundColor(ContentView.sub)
            }
            Text("夜间配色").foregroundColor(.white)
            Picker("夜间配色", selection: $settings.nightMode) {
                Text("自动（日出日落）").tag(0); Text("白天").tag(1); Text("夜间").tag(2)
            }
            .pickerStyle(.segmented)
            if !settings.nightDesc.isEmpty { Text(settings.nightDesc).font(.caption2).foregroundColor(ContentView.sub) }
            Toggle("弯道急缓上色", isOn: $settings.curve).foregroundColor(.white).tint(ContentView.pink)
        }
    }
    private var favoritesCard: some View {
        CardView {
            Text("常用目的地").foregroundColor(.white).bold()
            if favorites.places.isEmpty { Text("选择目的地后点收藏").foregroundColor(Self.sub) }
            ForEach(favorites.places) { place in
                HStack {
                    Button(place.name) { choose(place.searchPlace) }.foregroundColor(Self.cyan)
                    Spacer()
                    Button("移除") { favorites.remove(place) }.foregroundColor(Self.sub)
                }
            }
        }
    }
    private var voiceCard: some View {
        CardView {
            Text("导航语音").foregroundColor(.white).bold()
            Toggle("导航静音", isOn: $settings.voiceMuted)
            Toggle("播报时压低音乐", isOn: $settings.duck)
            Picker("系统音色", selection: $voiceID) {
                Text("自动优选中文").tag("")
                ForEach(AVSpeechSynthesisVoice.speechVoices().filter { $0.language.hasPrefix("zh") }, id: \.identifier) { voice in
                    Text(voice.name).tag(voice.identifier)
                }
            }
            Text("语速"); Slider(value: $voiceRate, in: 0.3...0.6)
            Text("音调"); Slider(value: $voicePitch, in: 0.7...1.4)
            Button("试听播报") { Speech.shared.test() }
        }.foregroundColor(.white).tint(Self.pink)
    }

    // MARK: Key 设置
    private var keyCard: some View {
        CardView {
            Text("Key 设置").foregroundColor(.white).bold()
            Text("高德 Key：\(Settings.mask(settings.amapKeyInUse))（搜索、导航、巡航用）")
                .font(.footnote).foregroundColor(settings.amapKeyInUse.isEmpty ? Color.orange : ContentView.sub)
            Text("和风天气：\(weather.host.isEmpty || weather.key.isEmpty ? "未配置（可选）" : "已配置")")
                .font(.footnote).foregroundColor(ContentView.sub)
            Button(settings.keysMissing ? "去填写" : "修改") { showKeys = true }
                .foregroundColor(ContentView.cyan)
        }
    }

    /// 没填高德 key：弹出填写页，返回 true（调用的地方就不往下做了）
    private func needAmapKey() -> Bool {
        if settings.amapKeyInUse.isEmpty { hideKeyboard(); showKeys = true; return true }
        return false
    }

    // MARK: 调试信息
    private var debugCard: some View {
        CardView {
            Button(showDebug ? "收起调试信息" : "调试信息") { showDebug.toggle() }.foregroundColor(ContentView.cyan)
            if showDebug {
                Text("""
                圆屏：\(ble.status)
                已发送 \(ble.sentCount) 条，最近：\(ble.lastSent)
                \(ble.screenStats)
                导航：\(navi.info)
                路名：\(navi.uiRoad)
                \(navi.laneRaw)
                电子眼：\(navi.camRaw)
                巡航：\(navi.cruiseRaw)
                \(navi.extraInfo)
                小地图：\(map.infoText)
                地图数据累计发送 \(ble.bulkKB) KB
                天气：\(weather.info)
                """)
                .font(.caption.monospaced())
                .foregroundColor(ContentView.sub)
                .textSelection(.enabled)
            }
        }
    }
}

// MARK: - 小组件

struct CardView<Content: View>: View {
    @ViewBuilder var content: Content
    var body: some View {
        VStack(alignment: .leading, spacing: 10) { content }
            .padding(16)
            .frame(maxWidth: .infinity, alignment: .leading)
            .background(RoundedRectangle(cornerRadius: 18).fill(ContentView.card))
            .overlay(RoundedRectangle(cornerRadius: 18).stroke(ContentView.stroke, lineWidth: 1))
    }
}

/// 一条路线的对比卡片：标签、时间、距离 · 红绿灯 · 收费
struct RouteCard: View {
    let option: NaviBridge.RouteOption
    let selected: Bool
    var body: some View {
        VStack(alignment: .leading, spacing: 3) {
            Text(option.label).font(.caption).foregroundColor(selected ? ContentView.pink : ContentView.sub).lineLimit(1)
            Text(NaviBridge.durText(option.timeS)).font(.headline).foregroundColor(.white).lineLimit(1).minimumScaleFactor(0.7)
            Text(NaviBridge.distText(option.length) + (option.lights > 0 ? " · \(option.lights)灯" : "") + (option.toll > 0 ? " · ¥\(option.toll)" : ""))
                .font(.caption2).foregroundColor(ContentView.sub).lineLimit(1).minimumScaleFactor(0.7)
        }
        .padding(10)
        .frame(maxWidth: .infinity, alignment: .leading)
        .background(RoundedRectangle(cornerRadius: 12).fill(ContentView.bg))
        .overlay(RoundedRectangle(cornerRadius: 12).stroke(selected ? ContentView.pink : ContentView.stroke, lineWidth: selected ? 2 : 1))
        .contentShape(Rectangle())
    }
}

struct PillButton: View {
    let title: String
    let color: Color
    var enabled = true
    let action: () -> Void
    var body: some View {
        Button(action: action) {
            Text(title).font(.headline).foregroundColor(.white)
                .frame(maxWidth: .infinity, minHeight: 46)
                .background(Capsule().fill(enabled ? color : Color.gray.opacity(0.4)))
        }
        .disabled(!enabled)
    }
}

/// 收起键盘
func hideKeyboard() {
    UIApplication.shared.sendAction(#selector(UIResponder.resignFirstResponder), to: nil, from: nil, for: nil)
}
