import SwiftUI

/// 「我的」顶层分组：通用 / 导航语音 / 关于（圆屏与 TPMS 已迁到圆屏 Tab）。
enum SettingsHubItem: String, Identifiable, CaseIterable {
    case general, voice, about
    var id: String { rawValue }
    var title: String {
        switch self {
        case .general: return "通用"
        case .voice: return "导航语音"
        case .about: return "关于"
        }
    }
    var subtitle: String {
        switch self {
        case .general: return "外观与key"
        case .voice: return "语音与播报"
        case .about: return "版本与使用说明"
        }
    }
    var icon: String {
        switch self {
        case .general: return "gearshape"
        case .voice: return "speaker.wave.2"
        case .about: return "info.circle"
        }
    }
}

struct SettingsHubList<Destination: View>: View {
    @ViewBuilder var destination: (SettingsHubItem) -> Destination
    var body: some View {
        VStack(spacing: 0) {
            ForEach(Array(SettingsHubItem.allCases.enumerated()), id: \.element.id) { idx, item in
                NavigationLink(destination: destination(item)) {
                    HStack(spacing: 12) {
                        Image(systemName: item.icon)
                            .font(.system(size: 17, weight: .semibold))
                            .foregroundColor(AppTheme.accent)
                            .frame(width: 28)
                        VStack(alignment: .leading, spacing: 2) {
                            Text(item.title)
                                .font(.body.weight(.semibold))
                                .foregroundColor(AppTheme.text)
                            Text(item.subtitle)
                                .font(.caption)
                                .foregroundColor(AppTheme.sub)
                        }
                        Spacer()
                        Image(systemName: "chevron.right")
                            .font(.caption.weight(.semibold))
                            .foregroundColor(AppTheme.sub.opacity(0.7))
                    }
                    .padding(.horizontal, 16)
                    .padding(.vertical, 14)
                    .contentShape(Rectangle())
                }
                .buttonStyle(.plain)
                if idx < SettingsHubItem.allCases.count - 1 {
                    Divider().padding(.leading, 56).opacity(0.35)
                }
            }
        }
        .background(RoundedRectangle(cornerRadius: 18).fill(AppTheme.card))
    }
}
