import SwiftUI

/// 版本：跟随 App 版本（见 Config.swift，iOS v0.7 新增）
///
/// 地图与天气服务配置页：Key 只保存在这台手机上。
/// 缺少地图所需 Key 时自动提示；和风天气可选，保存后即可使用。
struct KeySetupView: View {
    @ObservedObject var settings = Settings.shared
    @Environment(\.dismiss) private var dismiss
    @State private var amapIn = Settings.shared.amapKey
    @State private var copied = false

    private var bundleID: String { Bundle.main.bundleIdentifier ?? "" }

    var body: some View {
        NavigationView {
            ScrollView {
                VStack(alignment: .leading, spacing: 16) {
                    Text("高德用于搜索、导航、普通底图和卫星图；和风天气用于圆屏天气。请使用自己的服务账号 Key，只保存在这台手机上。")
                        .font(.subheadline).foregroundColor(ContentView.sub)

                    // 高德
                    CardView {
                        HStack {
                            Text("高德 Key").foregroundColor(AppTheme.text).bold()
                            Text("必填").font(.caption).foregroundColor(ContentView.pink)
                            Spacer()
                            Text(Settings.mask(settings.amapKeyInUse)).font(.caption).foregroundColor(ContentView.sub)
                        }
                        Text("不填的话搜索、导航、巡航都用不了").font(.caption).foregroundColor(ContentView.sub)
                        step(1, "打开高德开放平台，用手机号注册并完成实名认证")
                        step(2, "控制台 →「应用管理」→「我的应用」→「创建新应用」，再点「添加 Key」")
                        step(3, "服务平台选「iOS 平台」，安全码（Bundle ID）填下面这个：")
                        HStack {
                            Text(bundleID).font(.system(.callout, design: .monospaced)).foregroundColor(ContentView.cyan)
                                .textSelection(.enabled)
                            Spacer()
                            Button(copied ? "已复制" : "复制") {
                                UIPasteboard.general.string = bundleID
                                copied = true
                            }
                            .font(.subheadline.bold()).foregroundColor(ContentView.pink)
                        }
                        .padding(10)
                        .background(RoundedRectangle(cornerRadius: 10).fill(ContentView.bg))
                        step(4, "把生成的 Key 复制，粘贴到下面")
                        keyField("粘贴高德 Key", text: $amapIn)
                        Link(destination: URL(string: "https://console.amap.com/dev/key/app")!) {
                            Label("打开高德开放平台", systemImage: "safari").font(.subheadline).foregroundColor(ContentView.cyan)
                        }
                    }

                    CardView {
                        WeatherSettingsView()
                    }

                    PillButton(title: "保存地图 Key", color: ContentView.pink) { save() }
                    Button("以后再说") { dismiss() }
                        .font(.subheadline).foregroundColor(ContentView.sub)
                        .frame(maxWidth: .infinity)
                }
                .padding(16)
            }
            .background(ContentView.bg.ignoresSafeArea())
            .navigationTitle("Key 设置")
            .navigationBarTitleDisplayMode(.inline)
        }
        
    }

    private func step(_ n: Int, _ t: String) -> some View {
        HStack(alignment: .firstTextBaseline, spacing: 8) {
            Text("\(n)").font(.caption.bold()).foregroundColor(.black)
                .frame(width: 18, height: 18).background(Circle().fill(ContentView.cyan))
            Text(t).font(.footnote).foregroundColor(AppTheme.text).fixedSize(horizontal: false, vertical: true)
        }
    }

    private func keyField(_ placeholder: String, text: Binding<String>) -> some View {
        HStack {
            TextField(placeholder, text: text)
                .textFieldStyle(.plain)
                .foregroundColor(AppTheme.text)
                .disableAutocorrection(true)
                .textInputAutocapitalization(.never)
            Button("粘贴") { if let s = UIPasteboard.general.string { text.wrappedValue = s.trimmingCharacters(in: .whitespacesAndNewlines) } }
                .font(.subheadline.bold()).foregroundColor(ContentView.cyan)
        }
        .padding(10)
        .background(RoundedRectangle(cornerRadius: 10).fill(ContentView.bg))
        .overlay(RoundedRectangle(cornerRadius: 10).stroke(ContentView.stroke, lineWidth: 1))
    }

    private func save() {
        let oldAmap = settings.amapKeyInUse
        settings.amapKey = amapIn
        if settings.amapKeyInUse != oldAmap { settings.applyAmapKey() }     // 马上生效，不用重启
        dismiss()
    }
}
