import SwiftUI

/// 版本：跟随 App 版本（见 Config.swift）
///
/// 自动白夜模式（和安卓版对齐后新增）：
/// - 界面的语义色不写死，而是浅色 / 深色两套值，用 UIColor 的 dynamic Provider 包起来
/// - 跟随系统深浅色自动切换；用户在「设置 → 外观」里也可以强制浅色或深色
///
/// iOS v0.15.21-tpms 冷淡科技（Apple-like cool muted）：
/// - 中性灰阶为主；点缀色仅系统蓝（Light `#007AFF` / Dark `#0A84FF`）
/// - 去掉粉 / 青双品牌色与霓虹 HUD；玻璃材质保留，染色克制
/// - 卡片 / 列表 / 选项卡：无灰色描边，靠间距与填充分层
/// - 按钮：Primary = 系统蓝实心；Secondary = 灰底；Destructive = 系统红字
extension Color {
    /// 浅色 / 深色两套值，跟随当前界面样式自动切换
    static func dynamic(light: UIColor, dark: UIColor) -> Color {
        Color(UIColor { traits in
            traits.userInterfaceStyle == .dark ? dark : light
        })
    }
}

/// 语义色板：界面里只用这些名字，不直接写死颜色
enum AppTheme {
    // MARK: 页面（冷淡科技 · 中性灰）
    /// 页面背景
    static let bg = Color.dynamic(
        light: UIColor(red: 0.953, green: 0.953, blue: 0.973, alpha: 1),   // #F3F3F8
        dark:  UIColor(red: 0.000, green: 0.000, blue: 0.000, alpha: 1))   // #000000
    /// 卡片（内容层：保持实色，玻璃只用在控制层）
    static let card = Color.dynamic(
        light: UIColor(red: 1, green: 1, blue: 1, alpha: 1),               // #FFFFFF
        dark:  UIColor(red: 0.110, green: 0.110, blue: 0.118, alpha: 1))   // #1C1C1E
    /// 卡片内嵌的浅底（输入框、路线卡片的凹槽）
    static let sunken = Color.dynamic(
        light: UIColor(red: 0.949, green: 0.949, blue: 0.969, alpha: 1),   // #F2F2F7
        dark:  UIColor(red: 0.173, green: 0.173, blue: 0.180, alpha: 1))   // #2C2C2E
    /// 描边
    static let stroke = Color.dynamic(
        light: UIColor(red: 0.780, green: 0.780, blue: 0.800, alpha: 1),   // #C7C7CC
        dark:  UIColor(red: 0.235, green: 0.235, blue: 0.263, alpha: 1))   // #3C3C43
    /// Secondary 按钮 / 未选 chip 底
    static let secondaryFill = Color.dynamic(
        light: UIColor(red: 0.898, green: 0.898, blue: 0.918, alpha: 1),   // #E5E5EA
        dark:  UIColor(red: 0.227, green: 0.227, blue: 0.235, alpha: 1))   // #3A3A3C

    // MARK: 文字
    /// 主文字
    static let text = Color.dynamic(
        light: UIColor(red: 0.000, green: 0.000, blue: 0.000, alpha: 1),   // #000000
        dark:  UIColor(red: 1.000, green: 1.000, blue: 1.000, alpha: 1))   // #FFFFFF
    /// 次要文字
    static let sub = Color.dynamic(
        light: UIColor(red: 0.557, green: 0.557, blue: 0.576, alpha: 1),   // #8E8E93
        dark:  UIColor(red: 0.557, green: 0.557, blue: 0.576, alpha: 1))   // #8E8E93
    /// 导航条选中软底
    static let selCapsule = Color.dynamic(
        light: UIColor(red: 0.000, green: 0.478, blue: 1.000, alpha: 0.12),
        dark:  UIColor(red: 0.039, green: 0.518, blue: 1.000, alpha: 0.18))

    // MARK: 强调色（单一系统蓝；pink/cyan 保留别名，兼容旧引用）
    /// 系统蓝（主强调）
    static let accent = Color.dynamic(
        light: UIColor(red: 0.000, green: 0.478, blue: 1.000, alpha: 1),   // #007AFF
        dark:  UIColor(red: 0.039, green: 0.518, blue: 1.000, alpha: 1))   // #0A84FF
    /// 选中 chip 淡底
    static let accentSoft = Color.dynamic(
        light: UIColor(red: 0.000, green: 0.478, blue: 1.000, alpha: 0.14),
        dark:  UIColor(red: 0.039, green: 0.518, blue: 1.000, alpha: 0.22))

    /// 历史别名 → 系统蓝（不再是粉）
    static let pink = accent
    /// 历史别名 → 系统蓝（次要强调也统一，忌青/粉并置）
    static let cyan = accent

    // MARK: Tab 选中态：四 Tab 同一系统蓝
    static let tabAccents: [Color] = [accent, accent, accent, accent]

    /// 标题（冷淡科技：实色字重，无粉青渐变）
    static let titleGradient = LinearGradient(
        colors: [text, text], startPoint: .leading, endPoint: .trailing)

    // MARK: 状态色（系统语义）
    static let green = Color.dynamic(
        light: UIColor(red: 0.204, green: 0.780, blue: 0.349, alpha: 1),   // #34C759
        dark:  UIColor(red: 0.188, green: 0.820, blue: 0.345, alpha: 1))   // #30D158
    static let amber = Color.dynamic(
        light: UIColor(red: 1.000, green: 0.584, blue: 0.000, alpha: 1),   // #FF9500
        dark:  UIColor(red: 1.000, green: 0.624, blue: 0.039, alpha: 1))   // #FF9F0A
    static let red = Color.dynamic(
        light: UIColor(red: 1.000, green: 0.231, blue: 0.188, alpha: 1),   // #FF3B30
        dark:  UIColor(red: 1.000, green: 0.271, blue: 0.227, alpha: 1))   // #FF453A
    /// 保留但降饱和，避免当品牌色用
    static let purple = Color.dynamic(
        light: UIColor(red: 0.686, green: 0.322, blue: 0.871, alpha: 1),
        dark:  UIColor(red: 0.749, green: 0.353, blue: 0.949, alpha: 1))
    static let orange = amber

    // MARK: 导航 HUD
    /// 转向卡深色半透明底
    static let hudCard = Color.black.opacity(0.78)
    /// HUD 次要字
    static let hudSub = Color.white.opacity(0.62)
}

/// 外观模式：0 跟随系统（默认），1 始终浅色，2 始终深色
enum AppearanceMode: Int, CaseIterable {
    case system = 0
    case light = 1
    case dark = 2

    var name: String {
        switch self {
        case .system: return "跟随系统"
        case .light: return "浅色"
        case .dark: return "深色"
        }
    }

    /// MotoNavApp 里 .preferredColorScheme 用
    var scheme: ColorScheme? {
        switch self {
        case .system: return nil
        case .light: return .light
        case .dark: return .dark
        }
    }
}

// MARK: - 液态玻璃（iOS 26 Liquid Glass；旧系统降级为毛玻璃材质）

extension View {
    @ViewBuilder func liquidGlass<S:Shape>(_ shape:S,tint:Color?=nil,interactive:Bool=false)->some View {
        #if compiler(>=6.2)
        if #available(iOS 26.0,*) {self.glassEffect(.regular,in:shape)}
        else {self.background(shape.fill(.ultraThinMaterial))}
        #else
        self.background(shape.fill(.ultraThinMaterial))
        #endif
    }
    func tabBarLiquidGlass<S:Shape>(_ shape:S)->some View {self.liquidGlass(shape)}
}

// MARK: - 冷淡科技按钮
//
// - Primary (prominent)：系统蓝实心 + 白字
// - Secondary (tinted / clear)：灰底 + 主文字
// - Destructive：透明底 + 系统红字
// - Disabled：低对比灰

/// 按钮浓淡：clear 次要 / tinted 次操作 / prominent 主 CTA / destructive 危险
enum GlassButtonStyle {
    case clear
    case tinted
    case prominent
    case destructive
}

/// 统一按钮：冷淡科技实心 / 灰底胶囊
struct GlassButton: View {
    let title: String
    var icon: String? = nil
    var style: GlassButtonStyle = .clear
    var tint: Color = AppTheme.accent
    var enabled = true
    var compact = false
    let action: () -> Void

    private var foreground: Color {
        switch style {
        case .prominent: return .white
        case .destructive: return AppTheme.red
        default: return AppTheme.text
        }
    }

    private var fill: Color {
        switch style {
        case .prominent: return tint
        case .destructive: return Color.clear
        case .tinted: return AppTheme.secondaryFill
        case .clear: return AppTheme.secondaryFill.opacity(0.72)
        }
    }

    var body: some View {
        Button(action: action) {
            Group {
                if let icon {
                    Label(title, systemImage: icon)
                } else {
                    Text(title)
                }
            }
            .font(compact ? .subheadline.weight(.semibold) : .headline)
            .foregroundStyle(foreground)
            .lineLimit(1)
            .minimumScaleFactor(0.8)
            .padding(.horizontal, compact ? 12 : 18)
            .padding(.vertical, compact ? 8 : 12)
            .frame(minHeight: compact ? 32 : 46)
            .frame(maxWidth: (style == .prominent || (style == .tinted && !compact)) ? .infinity : nil)
            .background(Capsule().fill(fill))
            .contentShape(Capsule())
        }
        .buttonStyle(.plain)
        .disabled(!enabled)
        .opacity(enabled ? 1 : 0.38)
    }
}

/// 小芯片：选中 = 蓝软底 + 蓝字；默认 = 灰底
struct GlassChip: View {
    let title: String
    var icon: String? = nil
    var tint: Color? = nil
    var foreground: Color? = nil
    var enabled = true
    var compact = false
    let action: () -> Void

    private var isAccented: Bool { tint != nil || foreground != nil }

    private var labelColor: Color {
        if let foreground { return foreground }
        if tint != nil { return AppTheme.accent }
        return AppTheme.text
    }

    private var chipFill: Color {
        if tint != nil { return AppTheme.accentSoft }
        return AppTheme.secondaryFill
    }

    var body: some View {
        Button(action: action) {
            Group {
                if let icon {
                    Label(title, systemImage: icon)
                } else {
                    Text(title)
                }
            }
            .font(compact ? .caption.weight(.semibold) : .subheadline.weight(.semibold))
            .foregroundStyle(labelColor)
            .lineLimit(1)
            .minimumScaleFactor(0.75)
            .padding(.horizontal, compact ? 9 : 12)
            .padding(.vertical, compact ? 5 : 8)
            .background(Capsule().fill(chipFill))
            .contentShape(Capsule())
        }
        .buttonStyle(.plain)
        .disabled(!enabled)
        .opacity(enabled ? 1 : 0.38)
    }
}
