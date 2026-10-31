import SwiftUI

/// AirPods 风格连接底栏：3D 圆盘 + 状态 + 可选电量。
struct ConnectSheetView: View {
    @ObservedObject var coord = ConnectAnimationCoordinator.shared
    @ObservedObject var ble = BleLink.shared

    var body: some View {
        ZStack(alignment: .bottom) {
            Color.black.opacity(0.45)
                .ignoresSafeArea()
                .onTapGesture { /* 连接中不点空白关闭 */ }

            VStack(spacing: 0) {
                HStack {
                    Spacer(minLength: 36)
                    Text(coord.deviceTitle)
                        .font(.system(size: 20, weight: .semibold))
                        .foregroundColor(Color(white: 0.1))
                    Spacer(minLength: 0)
                    Button {
                        coord.dismiss()
                    } label: {
                        Image(systemName: "xmark")
                            .font(.system(size: 11, weight: .bold))
                            .foregroundColor(Color(white: 0.35))
                            .frame(width: 30, height: 30)
                            .background(Circle().fill(Color(white: 0.92)))
                    }
                    .buttonStyle(.plain)
                }
                .padding(.horizontal, 22)
                .padding(.top, 18)

                ConnectPuckSceneView(
                    spinning: coord.phase == .spinning,
                    spinSeconds: coord.spinSeconds
                )
                .frame(width: 260, height: 260)
                .padding(.top, 4)

                if coord.showBattery {
                    VStack(spacing: 4) {
                        HStack(spacing: 6) {
                            batteryGlyph
                            if ble.screenCharging { Image(systemName: "bolt.fill")
                                .font(.system(size: 11, weight: .semibold))
                                .foregroundColor(Color(white: 0.15)) }
                        }
                        Text(batteryLabel)
                            .font(.system(size: 28, weight: .semibold))
                            .foregroundColor(Color(white: 0.1))
                    }
                    .padding(.top, 4)
                    .transition(.opacity.combined(with: .move(edge: .bottom)))
                }

                Text(connectStatusLabel)
                    .font(.system(size: 15, weight: .medium))
                    .foregroundColor(coord.phase == .connected
                                     ? Color(red: 0.2, green: 0.55, blue: 0.3)
                                     : Color(white: 0.28))
                    .padding(.top, 8)

                HStack(spacing: 6) {
                    Image(systemName: "bolt.fill")
                        .font(.system(size: 10))
                    Text(coord.footerHint)
                        .font(.system(size: 12, weight: .medium))
                }
                .foregroundColor(Color(white: 0.55))
                .padding(.bottom, 22)
            }
            .frame(maxWidth: 390)
            .background(
                RoundedRectangle(cornerRadius: 38, style: .continuous)
                    .fill(Color.white)
                    .shadow(color: .black.opacity(0.28), radius: 24, y: 12)
            )
            .padding(.horizontal, 10)
            .padding(.bottom, 10)
        }
        .animation(.easeOut(duration: 0.35), value: coord.showBattery)
        .animation(.easeOut(duration: 0.35), value: coord.phase)
    }

    private var connectStatusLabel: String {
        switch coord.phase {
        case .connected: return "已连接圆屏"
        case .spinning, .idle: return "正在连接圆屏…"
        }
    }

    private var batteryLabel: String {
        if let p = ble.screenBattery { return "\(p)%" }
        return "—"
    }

    private var batteryGlyph: some View {
        let pct = CGFloat(ble.screenBattery ?? 100) / 100
        return ZStack(alignment: .leading) {
            RoundedRectangle(cornerRadius: 3, style: .continuous)
                .stroke(Color(white: 0.12), lineWidth: 1.5)
                .frame(width: 28, height: 12)
            RoundedRectangle(cornerRadius: 1.5, style: .continuous)
                .fill(Color(red: 0.2, green: 0.78, blue: 0.35))
                .frame(width: max(2, 22 * pct), height: 6)
                .padding(.leading, 3)
        }
        .overlay(alignment: .trailing) {
            Capsule().fill(Color(white: 0.12)).frame(width: 2, height: 5)
                .offset(x: 4)
        }
    }
}
