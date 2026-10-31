import SwiftUI
import CoreLocation
import AVFoundation
import UIKit

/// 版本：跟随 App 版本（见 Config.swift）
///
/// 主界面：圆屏连接状态、搜索目的地 / 导入高德分享路线、选路线、开始导航、巡航、圆屏设置、Key 设置
/// 0.16.0 UI alignment: four tabs, semantic colours, device card and grouped settings.
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
    @ObservedObject private var rides=RideRecorder.shared
    @ObservedObject private var connectAnim=ConnectAnimationCoordinator.shared
    @AppStorage("ui_appearance") private var appearance=0
    @AppStorage("ui_tab_placement") private var tabPlacement=0
    @Environment(\.horizontalSizeClass) private var hSizeClass
    @State private var uiStarted=false
    @State private var page = 0
    @State private var showWebMotor = false
    @State private var independentMode: IndependentMode = .motorcycle
    @State private var showRides = false
    @State private var showScreens=false
    @State private var confirmForgetPhones=false
    @AppStorage("voice_rate") private var voiceRate = Double(AVSpeechUtteranceDefaultSpeechRate)
    @AppStorage("voice_pitch") private var voicePitch = 1.0
    @AppStorage("voice_id") private var voiceID = ""

    @State private var query = ""
    @State private var dest: SearchService.Place?
    @State private var showNavi = false
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

    static let bg=AppTheme.bg,card=AppTheme.card,stroke=AppTheme.stroke
    static let pink=AppTheme.accent,cyan=AppTheme.accent,sub=AppTheme.sub

    var body:some View {
        GeometryReader { geo in
            let wide=TabLayout.isWide(size:geo.size,hSizeClass:hSizeClass)
            let rail=TabLayout.useSideRail(size:geo.size,hSizeClass:hSizeClass,placement:tabPlacement)
            NavigationView {
                ZStack(alignment:.bottom) {
                    if rail {
                        HStack(spacing:0) {
                            VStack(spacing:0){activeNavigationBanner;pages(wide:wide)}
                            rightRail(systemEdge:0)
                        }
                    } else {
                        VStack(spacing:0){activeNavigationBanner;pages(wide:wide)}
                        bottomBar
                    }
                }.background(Self.bg.ignoresSafeArea()).navigationBarHidden(true)
            }.navigationViewStyle(.stack).id(page)
        }
        .fullScreenCover(isPresented: $showWebMotor) { WebMotorView(mode:independentMode) }
        .sheet(isPresented: $showRides) { NavigationView { RideHistoryView() } }
        .sheet(isPresented:$showScreens) { screenPicker }
        .confirmationDialog("让圆屏忘掉其他手机？",isPresented:$confirmForgetPhones,titleVisibility:.visible) {
            Button("忘掉其他手机",role:.destructive){ble.forgetOtherPhones()}
            Button("取消",role:.cancel){}
        } message: {Text("当前手机保留配对，其他手机以后需要重新配对。")}
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
        .overlay { if connectAnim.isPresented {ConnectSheetView().transition(.opacity).zIndex(50)} }
        .animation(.easeOut(duration:0.35),value:connectAnim.isPresented)
        .onAppear {
            softenSegmentedChrome()
            if !uiStarted {
                uiStarted=true
                if !settings.keysMissing && !ble.isReady {connectAnim.connectAutomatically()}
            }                                    // iOS v0.7 打开 App 时，高德或高德 key 没填就提醒一次
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
                }.padding(12).background(Self.card).foregroundColor(AppTheme.text)
            }
        }
    }

    private var screenPicker:some View {
        NavigationView {
            List {
                Section {
                    Text("选择圆屏，编号可在圆屏首页查看。").font(.subheadline).foregroundColor(Self.sub)
                    if ble.availableScreens.isEmpty {Text("正在搜索附近圆屏…")}
                    ForEach(ble.availableScreens){screen in
                        Button(screen.name){ble.selectScreen(screen.id);showScreens=false;connectAnim.connectAutomatically()}
                    }
                }
            }.navigationTitle("选择圆屏").toolbar {
                ToolbarItem(placement:.cancellationAction){Button("取消"){showScreens=false}}
                ToolbarItem(placement:.confirmationAction){Button("重新搜索"){ble.beginScreenSelection()}}
            }
        }.onAppear{ble.beginScreenSelection()}.onDisappear{ble.endScreenSelection()}
    }

    private var statusCard: some View {
        CardView {
            HStack {
                Circle().fill(ble.isReady ? Color.green : Color.orange).frame(width: 10, height: 10)
                Text("圆屏").foregroundColor(AppTheme.text).bold()
                Spacer()
                Button(ble.isReady ? "断开" : "连接") { if ble.isReady {ble.disconnect()}else{connectAnim.connectManually()} }
                    .foregroundColor(ContentView.cyan)
            }
            Text(ble.connectionSummary).font(.footnote).foregroundColor(ContentView.sub)
            Button("换一块圆屏"){showScreens=true}.foregroundColor(Self.cyan)
            Button("忘掉其他手机",role:.destructive){confirmForgetPhones=true}.disabled(!ble.isReady || !ble.verified)
            if navi.navigating {
                Text("导航中：\(navi.uiDir) \(navi.uiDist) 米 · \(navi.uiSpeed) km/h").font(.footnote).foregroundColor(ContentView.cyan)
                if navi.uiRemainM > 0 {
                    Text("剩余 \(NaviBridge.distText(navi.uiRemainM)) · 约 \(NaviBridge.durText(navi.uiRemainS)) · \(ContentView.etaText(navi.uiRemainS)) 到达")
                        .font(.footnote).foregroundColor(AppTheme.text)
                }
                Button("回到导航画面") { showNavi = true }.foregroundColor(ContentView.pink)
            }
        }
    }

    private func pages(wide: Bool) -> some View {
        ScrollView {
            Group {
                if page == 0 && wide {
                    // Duo / 宽屏：左主（规划）右辅（连接摘要 + 轨迹快切）
                    HStack(alignment: .top, spacing: 16) {
                        VStack(alignment: .leading, spacing: 16) {
                            header
                            cruiseCard
                            routeButtons
                            destCard
                        }
                        VStack(alignment: .leading, spacing: 16) {
                            statusCard
                            favoritesCard
                            GlassChip(title: "导航轨迹", icon: "location.north.line", tint: Self.cyan) { page = 1 }
                            GlassChip(title: "常用地点", icon: "star", tint: Self.pink, compact: true) { page = 1 }
                        }
                    }
                } else {
                    VStack(alignment: .leading, spacing: 16) {
                        if page == 0 {
                            header
                            cruiseCard
                            routeButtons
                            destCard
                        } else if page == 1 {
                            ridesTabContent
                        } else if page == 2 {
                            deviceTabContent
                        } else {
                            settingsTabContent
                        }
                    }
                }
            // 内容滚过悬浮玻璃条后方，留白 ≈ 条高 + 安全区
            }.padding(.horizontal, 16).padding(.top, 16).padding(.bottom, 128)
        }
        .frame(maxWidth: wide ? 1080 : .infinity)
        .frame(maxWidth: .infinity)
    }

    private var ridesTabContent: some View {
        VStack(alignment: .leading, spacing: 14) {
            HStack(alignment: .top) {
                VStack(alignment: .leading, spacing: 4) {
                    Text("轨迹")
                        .font(.system(size: 28, weight: .heavy, design: .rounded))
                        .foregroundColor(AppTheme.text)
                    Text(monthRideSummary)
                        .font(.caption)
                        .foregroundColor(AppTheme.sub)
                }
                Spacer()
                GlassChip(title: "全部", icon: "list.bullet", tint: Self.cyan, compact: true) { showRides = true }
            }
            .padding(.top, 8)

            if let active = rides.active {
                CardView {
                    Text("记录中 · \(active.mode) · \(Int(active.meters)) 米")
                        .foregroundColor(AppTheme.accent).bold()
                    Text("真实导航或巡航中自动写入轨迹点")
                        .font(.caption).foregroundColor(AppTheme.sub)
                }
            }

            if let latest = rides.rides.first {
                NavigationLink(destination: RideDetailView(ride: latest)) {
                    RideFeatureCard(ride: latest)
                }
                .buttonStyle(.plain)
            } else if rides.active == nil {
                CardView {
                    Text("还没有导航记录")
                        .foregroundColor(AppTheme.text).bold()
                    Text("真实导航或巡航开始时自动记录；模拟导航不记录。")
                        .font(.caption).foregroundColor(AppTheme.sub)
                }
            }

            if rides.rides.count > 1 {
                Text("更早的导航")
                    .font(.subheadline.weight(.semibold))
                    .foregroundColor(AppTheme.sub)
                    .padding(.top, 4)
                let earlier = Array(rides.rides.dropFirst().prefix(12))
                VStack(spacing: 0) {
                    ForEach(Array(earlier.enumerated()), id: \.element.id) { idx, ride in
                        NavigationLink(destination: RideDetailView(ride: ride)) {
                            RideListRow(ride: ride)
                                .padding(.horizontal, 12)
                                .padding(.vertical, 8)
                        }
                        .buttonStyle(.plain)
                        if idx < earlier.count - 1 {
                            Divider().padding(.leading, 88).opacity(0.35)
                        }
                    }
                }
                .background(RoundedRectangle(cornerRadius: 18).fill(AppTheme.card))
            }

            favoritesCard
            historyCard
        }
    }

    private var monthRideSummary: String {
        let cal = Calendar.current
        let monthRides = rides.rides.filter { cal.isDate($0.started, equalTo: Date(), toGranularity: .month) }
        let meters = monthRides.reduce(0.0) { $0 + $1.meters }
        let km = meters / 1000.0
        return String(format: "本月 %.1f km · %d 次导航", km, monthRides.count)
    }

    private var deviceTabContent: some View {
        VStack(alignment: .leading, spacing: 16) {
            HStack {
                Text("圆屏")
                    .font(.system(size: 28, weight: .heavy, design: .rounded))
                    .foregroundColor(AppTheme.text)
                Spacer()
            }
            .padding(.top, 8)
            DeviceConnectionCard(
                displayDestination: { secondaryPage(title: "圆屏显示") { displayCard } },
                systemDestination: { secondaryPage(title: "圆屏设置") { settingsCard } }
            )
            connectionManagementCard
        }
    }

    private var settingsTabContent: some View {
        VStack(alignment: .leading, spacing: 16) {
            sectionTitle("我的", subtitle: "通用 · 导航语音 · 关于")
            SettingsHubList { item in
                secondaryPage(title: item.title) {
                    switch item {
                    case .general:
                        appearanceCard
                        layoutCard
                        keyCard
                    case .voice:
                        voiceCard

                    case .about:
                        CardView {
                            Text("关于").foregroundColor(AppTheme.text).bold()
                            Text(APP_VERSION).font(.footnote).foregroundColor(AppTheme.sub)
                            Text("AI 辅助开发 · 仅供非商业使用")
                                .font(.caption).foregroundColor(AppTheme.sub)
                        }
                        Link("项目与使用说明",destination:URL(string:"https://github.com/zyn950929/MotoNav")!).font(.subheadline)
                    }
                }
            }
        }
    }

    private func secondaryPage<Content: View>(title: String, @ViewBuilder content: () -> Content) -> some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 16) {
                content()
            }
            .padding(.horizontal, 16)
            .padding(.top, 12)
            .padding(.bottom, 128)
        }
        .background(AppTheme.bg.ignoresSafeArea())
        .navigationBarHidden(false)
        .navigationTitle(title)
        .navigationBarTitleDisplayMode(.inline)
        .background(InteractivePopEnabler())
    }

    private func softenSegmentedChrome() {
        let app = UISegmentedControl.appearance()
        app.selectedSegmentTintColor = UIColor { t in
            t.userInterfaceStyle == .dark
                ? UIColor(red: 0.039, green: 0.518, blue: 1.000, alpha: 0.28)
                : UIColor(red: 0.000, green: 0.478, blue: 1.000, alpha: 0.16)
        }
        app.backgroundColor = UIColor { t in
            t.userInterfaceStyle == .dark
                ? UIColor(red: 0.173, green: 0.173, blue: 0.180, alpha: 1)
                : UIColor(red: 0.949, green: 0.949, blue: 0.969, alpha: 1)
        }
        app.setDividerImage(UIImage(), forLeftSegmentState: .normal, rightSegmentState: .normal, barMetrics: .default)
        app.setTitleTextAttributes([.foregroundColor: UIColor.label], for: .normal)
        app.setTitleTextAttributes([.foregroundColor: UIColor.label], for: .selected)
    }

    private func sectionTitle(_ title: String, subtitle: String) -> some View {
        VStack(alignment: .leading, spacing: 2) {
            Text(title)
                .font(.system(size: 28, weight: .heavy, design: .rounded))
                .foregroundColor(AppTheme.text)
            Text(subtitle).font(.caption).foregroundColor(ContentView.sub)
        }
        .padding(.top, 8)
    }

    private var routeButtons: some View {
        HStack(spacing: 10) {
            GlassButton(title: "摩托车路线规划", style: .tinted, tint: Self.pink, compact: true) {
                if !independent.running { independent.arrived = false }
                independentMode = .motorcycle; showWebMotor = true
            }
            GlassButton(title: "自行车路线规划", style: .tinted, tint: Self.cyan, compact: true) {
                if !independent.running { independent.arrived = false }
                independentMode = .bicycle; showWebMotor = true
            }
        }
    }

    private var header: some View {
        VStack(alignment: .leading, spacing: 2) {
            Text("MotoNav")
                .font(.system(size: 34, weight: .heavy, design: .default))
                .foregroundColor(AppTheme.text)
            Text("圆屏导航 · \(APP_VERSION)").font(.caption).foregroundColor(ContentView.sub)
        }
        .padding(.top, 8)
    }

    private var historyCard: some View {
        CardView {
            Text("最近去过").foregroundColor(AppTheme.text).bold()
            if history.places.isEmpty {
                Text("搜索并选择目的地后会出现在这里").foregroundColor(Self.sub)
            } else {
                ForEach(history.places.prefix(12)) { p in
                    Button {
                        page = 0
                        choose(p)
                    } label: {
                        HStack(alignment: .center, spacing: 8) {
                            VStack(alignment: .leading, spacing: 2) {
                                Text(p.name).foregroundColor(AppTheme.text)
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
                GlassChip(title: "清除记录", foreground: ContentView.sub, compact: true) { history.clear() }
            }
        }
    }

    private var appearanceCard: some View {
        CardView {
            Text("外观").foregroundColor(AppTheme.text).bold()
            Picker("外观", selection: $appearance) {
                Text("跟随系统").tag(0); Text("浅色").tag(1); Text("深色").tag(2)
            }
            .pickerStyle(.segmented)
            Text("跟随系统时，界面随手机的深色模式自动切换浅色 / 深色；也可以一直用浅色或深色。圆屏小地图的昼夜由「圆屏 → 夜间配色」控制。")
                .font(.caption2).foregroundColor(AppTheme.sub)
        }
    }

    private var connectionManagementCard:some View {
        CardView {
            Text("连接管理").font(.headline).foregroundColor(AppTheme.text)
            Text(ble.connectionSummary).font(.subheadline).foregroundColor(Self.sub)
            HStack {
                GlassChip(title:"换一块圆屏",icon:"arrow.triangle.2.circlepath",tint:Self.cyan){showScreens=true}
                Spacer()
                GlassChip(title:"忘掉其他手机",foreground:AppTheme.red,enabled:ble.isReady && ble.verified,compact:true){confirmForgetPhones=true}
            }
            Text("更换圆屏时按设备名称选择，并按圆屏提示完成配对。").font(.caption).foregroundColor(Self.sub)
        }
    }
    private var layoutCard:some View {
        CardView {
            Text("导航栏").font(.headline).foregroundColor(AppTheme.text)
            Picker("导航栏位置",selection:$tabPlacement) {
                Text("自动").tag(0);Text("底部").tag(1);Text("右侧").tag(2)
            }.pickerStyle(.segmented)
            Text("自动模式在宽屏横向布局中使用侧栏。").font(.caption).foregroundColor(Self.sub)
        }
    }

    // MARK: 去哪儿
    private var destCard: some View {
        CardView {
            Text("去哪儿").foregroundColor(AppTheme.text).bold()
            HStack(spacing: 10) {
                // 只在按键盘上的“搜索”或右边按钮时才查高德（边打字边查太费额度）
                TextField(addingVia ? "搜索途经点（比如加油站）" : "搜索地点、地址", text: $query)
                    .textFieldStyle(.plain)
                    .padding(12)
                    .background(RoundedRectangle(cornerRadius: 12).fill(ContentView.bg))
                    .foregroundColor(AppTheme.text)
                    .submitLabel(.search)
                    .onSubmit { if !needAmapKey() { search.search(query) } }
                    .onChange(of: query) { if $0.isEmpty { search.clear() } }
                Button("搜索") { hideKeyboard(); if !needAmapKey() { search.search(query) } }
                    .font(.headline)
                    .foregroundColor(AppTheme.text)
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
            if !shareStatus.isEmpty { Text(Self.userMessage(shareStatus)).font(.footnote).foregroundColor(ContentView.sub) }
            if !pendingShare.isEmpty && !shareBusy {
                Button("网页验证后重试"){showShareVerification=true}.foregroundColor(Self.cyan)
            }
            if addingVia{Button("搜索附近加油站"){query="加油站";search.searchFuel()}.foregroundColor(Self.cyan)}
            if !search.status.isEmpty { Text(Self.userMessage(search.status)).font(.footnote).foregroundColor(ContentView.sub) }
            ForEach(search.results.prefix(8)) { p in
                Button { choose(p) } label: {
                    HStack(alignment: .center, spacing: 8) {
                        VStack(alignment: .leading, spacing: 2) {
                            Text(p.name).foregroundColor(AppTheme.text)
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
                                Text(p.name).foregroundColor(AppTheme.text)
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
                    Text("目的地：\(d.name)").font(.footnote).foregroundColor(AppTheme.text)
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
                    .foregroundColor(AppTheme.text).tint(ContentView.pink)
                    .onChange(of: settings.avoidHighway) { _ in navi.replan() }
                if navi.calculating || navi.routeOptions.isEmpty {
                    Text(navi.calculating ? "正在规划路线…" : Self.userMessage(navi.info)).font(.footnote).foregroundColor(ContentView.sub)
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
                    PillButton(title: "模拟导航", color: AppTheme.accent, enabled: navi.routeReady) {
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
                shareStatus="已读取：\(r.kind)路线 \(NaviBridge.distText(r.distance>0 ? r.distance:Int(r.total)))" +
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
                Text("巡航模式").foregroundColor(AppTheme.text).bold()
                Spacer()
                if navi.cruising { Text("进行中 · \(navi.uiSpeed) km/h").font(.caption).foregroundColor(ContentView.cyan) }
            }
            Text("不设目的地，也能查看地图和道路提醒")
                .font(.caption).foregroundColor(ContentView.sub)
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
            Text("圆屏设置").foregroundColor(AppTheme.text).bold()
            Picker("开机动画", selection: $settings.bootBrand) {
                ForEach(Settings.bootBrands.indices, id: \.self) { i in Text(Settings.bootBrands[i]).tag(i) }
            }.tint(ContentView.cyan)
            Text(settings.bootStatus).font(.caption).foregroundColor(ContentView.sub)
            Toggle("音乐控制", isOn: $settings.musicHID).foregroundColor(AppTheme.text).tint(ContentView.pink)
            if settings.musicHID {
                Text(ble.mediaHIDSupported
                     ? (ble.mediaHIDReady && ble.mediaHIDMode ? "媒体按键已就绪" : ble.mediaAMSControls > 0 && ble.mediaHIDMode ? "音乐控制已就绪" : "请在 iPhone 设置 → 蓝牙中配对 MotoNav；按圆屏配对码提示完成")
                     : "请连接圆屏并更新到支持音乐控制的固件")
                    .font(.caption).foregroundColor(ContentView.sub)
                if !ble.mediaSongInfo.isEmpty {
                    Text(ble.mediaSongInfo).font(.caption).foregroundColor(ContentView.cyan)
                }
                Toggle("显示逐句歌词", isOn: $settings.musicLyrics).foregroundColor(AppTheme.text).tint(ContentView.pink)
                Text("开启后自动匹配歌词，未匹配到时显示歌名和歌手。")
                    .font(.caption).foregroundColor(ContentView.sub)
                if !settings.musicLyrics && ble.mediaLyricsActive { Text("正在关闭圆屏歌词…").font(.caption).foregroundColor(ContentView.sub) }
                if settings.musicLyrics {
                    Text(ble.mediaLyricsSupported ? "播放音乐后自动匹配歌词" : "请更新圆屏固件以显示歌词")
                        .font(.caption).foregroundColor(ContentView.cyan)
                }
                Text("先在音乐 App 中播放，再用圆屏切歌或暂停。歌名和歌手由播放器提供。")
                    .font(.caption).foregroundColor(ContentView.sub)
            } else {
                Button("授权 Apple Music 圆屏控制") { MusicBridge.shared.authorize() }
                    .foregroundColor(ContentView.cyan)
            }
            Text("巡航连续停车 10 分钟后自动结束。")
                .font(.caption).foregroundColor(ContentView.sub)
            Text("圆屏方向").foregroundColor(AppTheme.text).padding(.top, 6)
            RotationDial(degrees: $settings.rotationDeg)      // iOS v0.5.1 按住圆盘转动调角度
            Text("拖动圆盘调整方向，粉色指针代表画面上方。")
                .font(.caption2).foregroundColor(ContentView.sub)
            Toggle("亮度跟着昼夜自动调", isOn: $settings.brightAuto).foregroundColor(AppTheme.text).tint(ContentView.pink)
            if settings.brightAuto {
                HStack {
                    Text("白天").foregroundColor(AppTheme.text)
                    Slider(value: $settings.brightDay, in: 5...255, step: 5).tint(ContentView.pink)
                    Text("\(Int(settings.brightDay))").font(.caption).foregroundColor(ContentView.sub).frame(width: 34)
                }
                HStack {
                    Text("夜间").foregroundColor(AppTheme.text)
                    Slider(value: $settings.brightNight, in: 5...255, step: 5).tint(ContentView.pink)
                    Text("\(Int(settings.brightNight))").font(.caption).foregroundColor(ContentView.sub).frame(width: 34)
                }
                if !settings.brightDesc.isEmpty { Text(settings.brightDesc).font(.caption2).foregroundColor(ContentView.sub) }
            } else {
                HStack {
                    Text("亮度").foregroundColor(AppTheme.text)
                    Slider(value: $settings.brightness, in: 5...255, step: 5).tint(ContentView.pink)
                    Text("\(Int(settings.brightness))").font(.caption).foregroundColor(ContentView.sub).frame(width: 34)
                }
            }
        }
    }

    private var displayCard: some View {
        CardView {
            Text("圆屏显示").foregroundColor(AppTheme.text).bold()
            Toggle("路程进度环", isOn: $settings.progressRing).foregroundColor(AppTheme.text).tint(Self.pink)
            Picker("预判", selection: $settings.leadMs) {
                Text("关闭").tag(0); Text("0.3秒").tag(300); Text("0.6秒").tag(600)
            }.pickerStyle(.segmented)
            HStack {
                Text("风格").foregroundColor(AppTheme.text)
                Spacer()
                Picker("风格", selection: $settings.theme) {
                    ForEach(Settings.themeOrder, id: \.self) { t in Text(Settings.themeName(t)).tag(t) }
                }
                .tint(ContentView.cyan)
            }
            Text(Settings.themeDesc(settings.theme)).font(.caption2).foregroundColor(ContentView.sub)
            Toggle("小地图", isOn: $settings.mapOn).foregroundColor(AppTheme.text).tint(ContentView.pink)
            if settings.mapOn {
                if settings.mapSource == MapFeeder.SRC_AMAP && settings.theme == 1 {
                    Toggle("俯视建筑轮廓", isOn: $settings.buildings).foregroundColor(AppTheme.text).tint(ContentView.pink)
                    if ble.isReady && !ble.buildingsSupported {
                        Text("当前圆屏暂不支持建筑轮廓，将显示普通地图。")
                            .font(.caption).foregroundColor(Self.sub)
                    }
                }
                Text(Self.userMessage(map.offlineStatus)).font(.caption).foregroundColor(ContentView.sub)
                Text("选定路线后，前台自动下载沿途地图；锁屏使用已保存地图包。请等下载完成再出发。")
                    .font(.caption2).foregroundColor(ContentView.sub)
                Text("走过的路线").foregroundColor(AppTheme.text)
                Picker("走过的路线", selection: $settings.trail) {
                    Text("变灰").tag(1); Text("擦除").tag(2)
                }
                .pickerStyle(.segmented)
                Toggle("清爽模式", isOn: $settings.clean).foregroundColor(AppTheme.text).tint(ContentView.pink)
                Text("仅保留地图和路线，隐藏其他提示").font(.caption2).foregroundColor(ContentView.sub)
            }
            Text("夜间配色").foregroundColor(AppTheme.text)
            Picker("夜间配色", selection: $settings.nightMode) {
                Text("自动（日出日落）").tag(0); Text("白天").tag(1); Text("夜间").tag(2)
            }
            .pickerStyle(.segmented)
            if !settings.nightDesc.isEmpty { Text(settings.nightDesc).font(.caption2).foregroundColor(ContentView.sub) }
            Toggle("弯道急缓上色", isOn: $settings.curve).foregroundColor(AppTheme.text).tint(ContentView.pink)
        }
    }
    private var favoritesCard: some View {
        CardView {
            Text("常用目的地").foregroundColor(AppTheme.text).bold()
            if favorites.places.isEmpty { Text("选择目的地后点收藏").foregroundColor(Self.sub) }
            ForEach(favorites.places) { place in
                HStack {
                    Button(place.name) { page=0; choose(place.searchPlace) }.foregroundColor(Self.cyan)
                    Spacer()
                    Button("移除") { favorites.remove(place) }.foregroundColor(Self.sub)
                }
            }
        }
    }
    private var voiceCard: some View {
        CardView {
            Text("导航语音").foregroundColor(AppTheme.text).bold()
            Toggle("导航静音", isOn: $settings.voiceMuted)
            Toggle("播报时压低音乐", isOn: $settings.duck)
            Picker("系统音色", selection: $voiceID) {
                Text("自动优选中文").tag("")
                ForEach(Self.zhSpeechVoices, id: \.identifier) { voice in
                    Text(voice.name).tag(voice.identifier)
                }
            }
            Text("语速"); Slider(value: $voiceRate, in: 0.3...0.6)
            Text("音调"); Slider(value: $voicePitch, in: 0.7...1.4)
            Button("试听播报") { Speech.shared.test() }
        }.foregroundColor(AppTheme.text).tint(Self.pink)
    }

    private static let zhSpeechVoices=AVSpeechSynthesisVoice.speechVoices().filter{$0.language.hasPrefix("zh")}

    // MARK: Key 设置
    private var keyCard: some View {
        CardView {
            Text("Key 设置").foregroundColor(AppTheme.text).bold()
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

    /// Product-facing recovery hints; raw SDK, URL and transport details stay out of the interface.
    static func userMessage(_ raw:String)->String {
        let value=raw.trimmingCharacters(in:.whitespacesAndNewlines)
        if value.range(of:#"https?://|NSURLError|Error Domain=|AMapError|HTTP|\bSDK\b|\bGCJ|\bWGS|数据包|毫秒|字节|0x[0-9a-fA-F]+"#,options:.regularExpression) != nil {
            if value.contains("登录") || value.contains("验证") { return "请完成高德登录或验证后重试" }
            if value.contains("地图") { return "地图暂未准备完成，请保持 App 在前台后重试" }
            return "暂时未能完成，请检查网络和服务配置后重试"
        }
        return value.replacingOccurrences(of:#"（-?\d+）"#,with:"",options:.regularExpression)
    }

    // MARK: 底部导航条（对齐 Apple Music：悬浮液态玻璃胶囊，内容从后方穿过时实时透色）
    private var bottomBar: some View {
        HStack(spacing: 0) {
            ForEach(0..<TabSpec.count, id: \.self) { i in tabButton(i) }
        }
        .padding(.horizontal, 10)
        .padding(.top, 8)
        .padding(.bottom, 6)
        .tabBarLiquidGlass(Capsule())
        .padding(.horizontal, 18)
        .padding(.bottom, 10)
    }

    private func tabButton(_ i: Int) -> some View {
        let selected = page == i
        let accent = AppTheme.tabAccents[i]
        return Button {
            hideKeyboard()
            page = i
        } label: {
            VStack(spacing: 2) {
                ZStack {
                    // Apple Music：选中态是图标后的软色圆晕，不是整格白胶囊（白胶囊会盖死玻璃折射）
                    if selected {
                        Circle()
                            .fill(accent.opacity(0.18))
                            .frame(width: 44, height: 32)
                    }
                    Image(systemName: selected ? TabSpec.fillIcons[i] : TabSpec.icons[i])
                        .font(.system(size: 20, weight: .semibold))
                        .foregroundColor(selected ? accent : AppTheme.sub.opacity(0.72))
                        .frame(width: 44, height: 28)
                }
                Text(TabSpec.titles[i])
                    .font(.system(size: 10, weight: selected ? .semibold : .medium))
                    .foregroundColor(selected ? accent : AppTheme.sub.opacity(0.72))
                    .lineLimit(1).minimumScaleFactor(0.75)
            }
            .frame(maxWidth: .infinity)
            .padding(.vertical, 2)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
    }

    // MARK: 右侧竖栏（Duo 外屏：同系液态玻璃竖条）
    private func rightRail(systemEdge: CGFloat) -> some View {
        VStack(spacing: 6) {
            Spacer(minLength: systemEdge > 0 ? 170 : 12)
            ForEach(0..<TabSpec.count, id: \.self) { i in railButton(i) }
            Spacer(minLength: 12)
        }
        .padding(.vertical, 10)
        .frame(width: max(66, systemEdge))
        .tabBarLiquidGlass(Capsule())
        .padding(.vertical, 8).padding(.trailing, 6)
    }

    private func railButton(_ i: Int) -> some View {
        let selected = page == i
        let accent = AppTheme.tabAccents[i]
        return Button {
            hideKeyboard()
            page = i
        } label: {
            VStack(spacing: 2) {
                ZStack {
                    if selected {
                        Circle()
                            .fill(accent.opacity(0.18))
                            .frame(width: 46, height: 34)
                    }
                    Image(systemName: selected ? TabSpec.fillIcons[i] : TabSpec.icons[i])
                        .font(.system(size: 20, weight: .semibold))
                        .foregroundColor(selected ? accent : AppTheme.sub.opacity(0.72))
                        .frame(width: 46, height: 30)
                }
                Text(TabSpec.titles[i])
                    .font(.system(size: 10, weight: selected ? .semibold : .medium))
                    .foregroundColor(selected ? accent : AppTheme.sub.opacity(0.72))
                    .lineLimit(1).minimumScaleFactor(0.6)
            }
            .frame(width: 58)
            .contentShape(Rectangle())
        }
        .buttonStyle(.plain)
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

    }
}

/// 一条路线的对比卡片：标签、时间、距离 · 红绿灯 · 收费
struct RouteCard: View {
    let option: NaviBridge.RouteOption
    let selected: Bool
    var body: some View {
        VStack(alignment: .leading, spacing: 3) {
            Text(option.label).font(.caption).foregroundColor(selected ? ContentView.pink : ContentView.sub).lineLimit(1)
            Text(NaviBridge.durText(option.timeS)).font(.headline).foregroundColor(AppTheme.text).lineLimit(1).minimumScaleFactor(0.7)
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

enum TabSpec {
    static let count=4
    static let titles=["导航","轨迹","圆屏","我的"]
    static let icons=["location.north","map","circle.circle","person"]
    static let fillIcons=["location.north.fill","map.fill","circle.circle.fill","person.fill"]
}
enum TabLayout {
    static func isWide(size:CGSize,hSizeClass:UserInterfaceSizeClass?)->Bool {size.width>=700}
    static func useSideRail(size:CGSize,hSizeClass:UserInterfaceSizeClass?,placement:Int)->Bool {
        if placement==1{return false};if placement==2{return true}
        return size.width>=700 && size.width>size.height
    }
}
