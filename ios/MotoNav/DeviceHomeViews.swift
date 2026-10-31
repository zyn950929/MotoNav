import SwiftUI

/// 圆屏 Tab：连接卡（产品正视 PNG）+ 圆屏显示 / 圆屏设置入口 + 胎压全字段。
struct DeviceConnectionCard<DisplayDest: View, SystemDest: View>: View {
    @ObservedObject var ble = BleLink.shared
    @ObservedObject var navi = NaviBridge.shared
    @ObservedObject var map = MapFeeder.shared
    @ObservedObject var settings = Settings.shared
    @ObservedObject private var connectAnim = ConnectAnimationCoordinator.shared
    @ViewBuilder var displayDestination: () -> DisplayDest
    @ViewBuilder var systemDestination: () -> SystemDest

    var body: some View {
        CardView {
            HStack(alignment: .top, spacing: 12) {
                VStack(alignment: .leading, spacing: 8) {
                    Text("导航屏")
                        .font(.caption)
                        .foregroundColor(AppTheme.sub)
                    Text(ble.screenName.isEmpty ? "MotoNav" : ble.screenName)
                        .font(.system(size: 28, weight: .bold))
                        .foregroundColor(AppTheme.text)
                        .lineLimit(1)
                        .minimumScaleFactor(0.7)
                    Text("圆屏导航")
                        .font(.caption)
                        .foregroundColor(AppTheme.sub)
                    HStack(spacing: 6) {
                        Circle()
                            .fill(ble.isReady ? AppTheme.green : AppTheme.amber)
                            .frame(width: 8, height: 8)
                        Text(ble.connectionSummary)
                            .font(.subheadline.weight(.medium))
                            .foregroundColor(ble.isReady ? AppTheme.green : AppTheme.sub)
                            .lineLimit(1)
                    }
                    .padding(.top, 2)
                    VStack(alignment: .leading, spacing: 2) {
                        Text("设备电量").font(.caption2).foregroundColor(AppTheme.sub)
                        Text(ble.screenBattery.map { "\($0)%" } ?? "—")
                            .font(.subheadline.weight(.semibold).monospacedDigit())
                            .foregroundColor(AppTheme.text)
                    }
                    .padding(.top, 4)
                    GlassChip(
                        title: ble.isReady ? "断开" : "连接",
                        icon: ble.isReady ? "bolt.slash" : "bolt.fill",
                        tint: ble.isReady ? nil : AppTheme.accent,
                        compact: true
                    ) {
                        if ble.isReady { ble.disconnect() }
                        else { connectAnim.connectManually() }
                    }
                    .padding(.top, 4)
                }
                .frame(maxWidth: .infinity, alignment: .leading)

                ZStack(alignment: .bottom) {
                    Image("GroundShadow")
                        .resizable()
                        .scaledToFit()
                        .frame(width: 150, height: 34)
                        .offset(y: 8)
                        .opacity(0.75)
                    // 3/4 角产品图（mockup C：银壳 + 导航屏，等比无椭圆压扁）
                    Image("DevicePuckHero")
                        .resizable()
                        .scaledToFit()
                        .frame(width: 152, height: 170)
                }
                .frame(width: 156, height: 186)
            }

            if navi.navigating {
                Text("导航中：\(navi.uiDir) \(navi.uiDist) 米 · \(navi.uiSpeed) km/h")
                    .font(.footnote)
                    .foregroundColor(AppTheme.accent)
            }
            Text(Settings.themeName(settings.theme)).font(.caption2).foregroundColor(AppTheme.sub)

            Divider().opacity(0.35)

            HStack(spacing: 0) {
                NavigationLink(destination: displayDestination()) {
                    deviceQuickLinkLabel("圆屏显示", "sun.max")
                }
                .buttonStyle(.plain)
                Divider().frame(height: 28).opacity(0.35)
                NavigationLink(destination: systemDestination()) {
                    deviceQuickLinkLabel("圆屏设置", "gearshape")
                }
                .buttonStyle(.plain)
            }
        }
    }

    private func deviceQuickLinkLabel(_ title: String, _ icon: String) -> some View {
        HStack(spacing: 6) {
            Image(systemName: icon).font(.system(size: 14, weight: .semibold))
            Text(title).font(.subheadline.weight(.medium))
        }
        .foregroundColor(AppTheme.accent)
        .frame(maxWidth: .infinity)
        .padding(.vertical, 6)
        .contentShape(Rectangle())
    }
}

