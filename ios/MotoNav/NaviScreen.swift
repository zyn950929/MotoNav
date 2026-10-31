import SwiftUI
import ObjectiveC

/// 版本：跟随 App 版本（见 Config.swift）
///
/// 导航时手机上显示的高德原版导航界面（地图、路线、转向提示都由高德画）。
/// 我们的 NaviBridge 同时在旁边“旁听”导航数据发给圆屏。
/// iOS v0.5.2：
/// - 高德界面自带的“关闭”按钮 = 退出导航；“更多”按钮弹出菜单（看全程 / 回到跟随、路况开关、退出导航）
/// - 地图跟着夜间配色切换白天 / 夜间样式
/// - 到达目的地后显示 4 秒“已到达”，然后自动关掉（圆屏同时显示到达，4 秒后回到待机）
/// iOS v0.6：播报静音按钮（下次还记得）；模拟导航暂停 / 继续、车速 40/60/100/150；
/// “退出导航”要长按（防误触），高德界面的“关闭”按钮先问一句
/// 整合版（v0.15.11-tpms）：修复底部遮挡——高德 showUIElements=false 隐藏顶卡/底栏/logo/主页按钮，
/// 顶部转向卡 / 到达时刻由本页自绘（数据来自 NaviBridge uiDir/uiDist/uiRoad/uiRemainS），
/// 路况光柱保留高德原生（实测 showUIElements=false 下 mapShowTraffic 光柱仍显示），
/// 底部只剩我们一套控件；高德“主页/路况开关”仍在“更多”菜单

/// 接住高德导航界面的按钮回调（和 NaviBridge 一样在运行时登记协议，见 CLAUDE.md 高德坑第 1 条）
final class DriveViewController: NSObject, ObservableObject {
    weak var view: AMapNaviDriveView?
    var onClose: (() -> Void)?
    @Published var showMore = false

    var asDelegate: AMapNaviDriveViewDelegate? {
        class_addProtocol(DriveViewController.self, AMapNaviDriveViewDelegate.self)
        return (self as AnyObject) as? AMapNaviDriveViewDelegate
    }

    @objc(driveViewCloseButtonClicked:)
    func closeTapped(_ v: AMapNaviDriveView) { onClose?() }

    @objc(driveViewMoreButtonClicked:)
    func moreTapped(_ v: AMapNaviDriveView) { showMore = true }

    var isOverview: Bool { view?.showMode == .overview }
    func toggleOverview() {
        guard let v = view else { return }
        v.showMode = v.showMode == .overview ? .carPositionLocked : .overview
    }
    var trafficOn: Bool { view?.mapShowTraffic ?? true }
    func toggleTraffic() { view?.mapShowTraffic.toggle() }
}

struct AMapDriveViewRep: UIViewRepresentable {
    let controller: DriveViewController
    let night: Bool

    func makeUIView(context: Context) -> AMapNaviDriveView {
        let v = AMapNaviDriveView(frame: .zero)
        v.delegate = controller.asDelegate
        v.mapViewModeType = night ? .night : .day
        // 整合版：高德自带 UI（顶卡/底栏/logo/主页按钮/光柱）全部隐藏，改由 NaviScreen 自绘
        v.showUIElements = false
        v.screenAnchor = CGPoint(x: 0.5, y: 0.72)       // 仅 showUIElements=NO 时有效：车标偏下，前方视野更大
        controller.view = v
        AMapNaviDriveManager.sharedInstance().addDataRepresentative(v)
        return v
    }

    func updateUIView(_ v: AMapNaviDriveView, context: Context) {
        let mode: AMapNaviViewMapModeType = night ? .night : .day
        if v.mapViewModeType != mode { v.mapViewModeType = mode }
    }

    static func dismantleUIView(_ uiView: AMapNaviDriveView, coordinator: ()) {
        AMapNaviDriveManager.sharedInstance().removeDataRepresentative(uiView)
    }
}

struct NaviScreen: View {
    @ObservedObject var navi = NaviBridge.shared
    @ObservedObject var settings = Settings.shared
    @StateObject private var drive = DriveViewController()
    @Environment(\.dismiss) private var dismiss
    @State private var confirmClose = false
    @State private var holding = false                  // 正在长按“退出导航”
    @State private var holdHint = false

    var body: some View {
        ZStack(alignment: .bottom) {
            AMapDriveViewRep(controller: drive, night: settings.paletteNight).ignoresSafeArea()
            if !navi.showArrived {
                topOverlay
                    .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .top)
                VStack(spacing: 10) {
                    controlRow
                    exitButton
                }
            }
            if navi.showArrived { arrivedCard }
        }
        .onAppear { drive.onClose = { confirmClose = true } }
        .alert("退出导航？", isPresented: $confirmClose) {
            Button("退出导航", role: .destructive) { exitNavi() }
            Button("继续导航", role: .cancel) {}
        }
        .onChange(of: navi.navigating) { now in
            if !now && !navi.showArrived { dismiss() }          // 退出导航：马上关；到达：等“已到达”显示完
        }
        .onChange(of: navi.showArrived) { now in
            if !now && !navi.navigating { dismiss() }           // “已到达”显示完了
        }
        .confirmationDialog("导航选项", isPresented: $drive.showMore, titleVisibility: .hidden) {
            Button(drive.isOverview ? "回到车头跟随" : "看全程") { drive.toggleOverview() }
            Button(drive.trafficOn ? "关掉路况颜色" : "显示路况颜色") { drive.toggleTraffic() }
            Button("返回首页修改设置") { dismiss() }
            Button("退出导航", role: .destructive) { exitNavi() }
            Button("取消", role: .cancel) {}
        }
    }

    private func exitNavi() {
        navi.stop()
        dismiss()
    }

    /// 顶部浮层：自绘转向卡（整合高德顶卡+底栏"到达"）+ 更快路线提示
    private var topOverlay: some View {
        VStack(spacing: 10) {
            turnCard
            if navi.uiSuggestMin > 0 {                  // iOS v0.2 高德发现更快的路线
                HStack {
                    Text("发现更快路线，能省约 \(navi.uiSuggestMin) 分钟")
                        .font(.subheadline).foregroundColor(AppTheme.text)
                    Spacer()
                    GlassChip(title: "换这条", tint: ContentView.cyan, foreground: ContentView.cyan, compact: true) { navi.acceptSuggest() }
                    GlassChip(title: "不用", foreground: AppTheme.sub, compact: true) { navi.clearSuggest() }
                }
                .padding(14)
                .liquidGlass(RoundedRectangle(cornerRadius: 20))
            }
        }
        .padding(.horizontal, 16)
        .padding(.top, 8)
    }

    /// 自绘转向卡：冷淡科技深色半透明 HUD；下一转向用系统蓝块强调
    private var turnCard: some View {
        HStack(spacing: 14) {
            Image(systemName: Self.dirSymbol(navi.uiDir))
                .font(.system(size: 22, weight: .bold))
                .foregroundColor(.white)
                .frame(width: 44, height: 44)
                .background(RoundedRectangle(cornerRadius: 12, style: .continuous).fill(AppTheme.accent))
            VStack(alignment: .leading, spacing: 2) {
                Text(navi.uiDist >= 0
                     ? "\(Self.distText(navi.uiDist)) \(navi.uiRoad.isEmpty ? Self.dirWord(navi.uiDir) : "进入")"
                     : Self.dirWord(navi.uiDir))
                    .font(.headline).foregroundColor(.white)
                Text(navi.uiRoad.isEmpty ? " " : navi.uiRoad)
                    .font(.footnote).foregroundColor(AppTheme.hudSub)
            }
            Spacer()
            VStack(alignment: .trailing, spacing: 2) {
                Text(Self.etaText(navi.uiRemainS))
                    .font(.subheadline.weight(.medium)).foregroundColor(.white)
                Text(Self.remainText(navi.uiRemainM, navi.uiRemainS))
                    .font(.caption2).foregroundColor(AppTheme.hudSub)
            }
        }
        .padding(14)
        .background(RoundedRectangle(cornerRadius: 18, style: .continuous).fill(AppTheme.hudCard))
        .overlay(RoundedRectangle(cornerRadius: 18, style: .continuous).stroke(Color.white.opacity(0.10)))
    }

    private static func dirSymbol(_ d: String) -> String {
        switch d {
        case "L": return "arrow.turn.up.left"
        case "R": return "arrow.turn.up.right"
        case "SL": return "arrow.up.left"
        case "SR": return "arrow.up.right"
        case "U": return "arrow.uturn.left"
        case "A": return "mappin.and.ellipse"
        default: return "arrow.up"
        }
    }

    private static func dirWord(_ d: String) -> String {
        switch d {
        case "L": return "左转"
        case "R": return "右转"
        case "SL": return "左前方"
        case "SR": return "右前方"
        case "U": return "掉头"
        case "A": return "到达终点"
        default: return "直行"
        }
    }

    private static func distText(_ m: Int) -> String {
        m >= 1000 ? String(format: "%.1f 公里", Double(m) / 1000) : "\(m) 米"
    }

    private static func etaText(_ remainS: Int) -> String {
        guard remainS > 0 else { return "—" }
        let f = DateFormatter()
        f.dateFormat = "HH:mm"
        return f.string(from: Date().addingTimeInterval(TimeInterval(remainS))) + " 到达"
    }

    private static func remainText(_ m: Int, _ s: Int) -> String {
        guard m > 0 || s > 0 else { return " " }
        return String(format: "剩余 %.1f km · %d 分", Double(m) / 1000, max(1, s / 60))
    }

    /// 静音、（模拟导航时）暂停 / 继续、车速
    private var controlRow: some View {
        HStack(spacing: 10) {
            chip(settings.voiceMuted ? "speaker.slash.fill" : "speaker.wave.2.fill",
                 settings.voiceMuted ? "已静音" : "播报", on: settings.voiceMuted) {
                settings.voiceMuted.toggle()
                if settings.voiceMuted { Speech.shared.stopNow() }
            }
            if navi.emulating {
                chip(navi.emuPaused ? "play.fill" : "pause.fill", navi.emuPaused ? "继续" : "暂停", on: navi.emuPaused) { navi.toggleEmuPause() }
                chip("speedometer", "\(navi.emuSpeed) km/h", on: false) { navi.cycleEmuSpeed() }
                    .fixedSize()                       // 不被同行挤压截断（实测 "60…" 截断）
            }
            Spacer()
            chip("gearshape", "设置", on: false) { dismiss() }
        }
        .padding(.horizontal, 24)
    }

    /// 导航浮层控制芯片：选中态系统蓝
    private func chip(_ icon: String, _ title: String, on: Bool, _ action: @escaping () -> Void) -> some View {
        GlassChip(title: title, icon: icon, tint: on ? AppTheme.accent : nil, foreground: on ? AppTheme.accent : nil, action: action)
    }

    /// 长按 0.6 秒才退出；灰底进度 + 红字「退出导航」（冷淡科技 destructive）
    private var exitButton: some View {
        ZStack(alignment: .leading) {
            Capsule().fill(AppTheme.secondaryFill.opacity(0.92))
                .liquidGlass(Capsule(), tint: nil, interactive: true)
            GeometryReader { g in
                Capsule().fill(AppTheme.red.opacity(0.55))
                    .frame(width: holding ? g.size.width : 0)
                    .animation(holding ? .linear(duration: 0.6) : .easeOut(duration: 0.15), value: holding)
            }
            HStack(spacing: 4) {
                Text(holdHint ? "要长按才能退出" : (holding ? "松手取消" : "长按"))
                    .foregroundColor(AppTheme.text)
                if !holdHint && !holding {
                    Text("退出导航").foregroundColor(AppTheme.red)
                }
            }
            .font(.headline)
            .frame(maxWidth: .infinity)
        }
        .frame(height: 48)
        .clipShape(Capsule())
        .contentShape(Capsule())
        .onTapGesture {
            holdHint = true
            DispatchQueue.main.asyncAfter(deadline: .now() + 1.5) { holdHint = false }
        }
        .onLongPressGesture(minimumDuration: 0.6, pressing: { p in holding = p }) {
            holding = false
            exitNavi()
        }
        .padding(.horizontal, 24)
        .padding(.bottom, 24)
    }

    /// 到达目的地：显示 4 秒后自动关掉（点一下马上关）
    private var arrivedCard: some View {
        VStack(spacing: 8) {
            Image(systemName: "flag.checkered").font(.system(size: 40, weight: .bold)).foregroundColor(AppTheme.accent)
            Text("已到达目的地").font(.title2.bold()).foregroundColor(AppTheme.text)
            Text("马上回到主界面").font(.footnote).foregroundColor(AppTheme.sub)
        }
        .padding(28)
        .frame(maxWidth: .infinity)
        .liquidGlass(RoundedRectangle(cornerRadius: 24))
        .padding(24)
        .frame(maxHeight: .infinity)
        .contentShape(Rectangle())
        .onTapGesture { exitNavi() }
    }
}
