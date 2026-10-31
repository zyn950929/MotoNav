import SwiftUI

/// 版本：跟随 App 版本（见 Config.swift）
///
/// 高德导航界面（AMapNaviDriveView）创建时会调用 UIApplication.shared.delegate.window 判断是不是刘海屏。
/// SwiftUI 默认的 AppDelegate 没有 window，会直接崩溃（iOS v0.1.1 点“开始导航”就闪退的原因），
/// 所以这里补一个 AppDelegate，window 返回当前显示的窗口。
final class AppDelegate: NSObject, UIApplicationDelegate {
    var window: UIWindow? {
        get {
            let wins = UIApplication.shared.connectedScenes
                .compactMap { $0 as? UIWindowScene }
                .flatMap { $0.windows }
            return wins.first { $0.isKeyWindow } ?? wins.first
        }
        set {}
    }
}

@main
struct MotoNavApp: App {
    @AppStorage("ui_appearance") private var appearance=0
    @UIApplicationDelegateAdaptor(AppDelegate.self) private var appDelegate

    init() {
        // 高德：key + 隐私合规声明（高德要求在用任何功能前调用；自己用的 App，直接设为已同意）
        AMapServices.shared().apiKey = Settings.shared.amapKeyInUse     // 用户在 App 里填的（iOS v0.7 起没有内置 key；没填时是空的）
        AMapServices.shared().enableHTTPS = true
        AMapNaviManagerConfig.shared().updatePrivacyShow(.didShow, privacyInfo: .didContain)
        AMapNaviManagerConfig.shared().updatePrivacyAgree(.didAgree)
        AMapSearchAPI.updatePrivacyShow(.didShow, privacyInfo: .didContain)
        AMapSearchAPI.updatePrivacyAgree(.didAgree)
        MAMapView.updatePrivacyShow(.didShow, privacyInfo: .didContain)
        MAMapView.updatePrivacyAgree(.didAgree)

        // 圆屏发来的通知
        BleLink.shared.onNotify = { msg in
            let p = msg.split(separator: ",").map(String.init)
            if p.first == "BOOT", p.count >= 2, let id = Int(p[1]) { Settings.shared.bootReply(id); return }
            if p.count >= 3, p[0] == "T", p[1] == "M" { MusicBridge.shared.handle(p[2]); return }
            if p.first == "T", p.count >= 2 {
                switch p[1] {
                case "W": Weather.shared.request()                    // 滑到了天气页
                case "A": break                                       // 按住接电话：iOS 不允许 App 接电话
                default: break
                }
            }
        }
        BleLink.shared.connect()
        LocationService.shared.start()
        Settings.shared.startDayNight()                 // 夜间配色、亮度按日出日落自动切换（每分钟检查）
        NaviBridge.shared.attach()
    }

    var body: some Scene {
        WindowGroup {
            ContentView()
                .background(AmapCaptureSurface().frame(width: 1, height: 1))
                .preferredColorScheme(AppearanceMode(rawValue:appearance)?.scheme)
                .onReceive(NotificationCenter.default.publisher(for:UIApplication.didBecomeActiveNotification)){_ in
                    BleLink.shared.syncPhoneClock(force:true)
                }
        }
    }
}
