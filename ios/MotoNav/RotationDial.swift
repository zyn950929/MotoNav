import SwiftUI

/// 版本：跟随 App 版本（见 Config.swift，iOS v0.5.1 新增）
///
/// 圆屏方向调节圆盘：按住圆盘任意位置转动来调角度（顺时针为正），粉色指针 = 画面上方在圆屏上朝的方向，
/// 顶上外侧的小三角 = 0°。转到 0° / 90° / 180° / 270° 附近（±3°）会吸住并震一下，每过 15° 轻震一下；
/// 下面的按钮可以 ±1° 微调、归零。
struct RotationDial: View {
    @Binding var degrees: Int
    var size: CGFloat = 220

    @State private var lastFinger: Double?          // 上一次手指所在的角度
    @State private var accum = 0.0                  // 这次按住以来累计转了多少度
    @State private var startDeg = 0
    @State private var dragging = false

    var body: some View {
        VStack(spacing: 14) {
            ZStack {
                Circle().fill(ContentView.bg)
                Circle().stroke(dragging ? ContentView.pink : ContentView.stroke, lineWidth: 2)
                // 跟着角度转的部分：刻度 + 指针
                ZStack {
                    ForEach(0..<72, id: \.self) { i in
                        let major = i % 18 == 0, mid = i % 6 == 0
                        let h: CGFloat = major ? 16 : (mid ? 11 : 6)
                        Capsule()
                            .fill(major ? ContentView.pink : (mid ? Color.white.opacity(0.75) : ContentView.sub.opacity(0.45)))
                            .frame(width: major ? 3 : 1.5, height: h)
                            .offset(y: -(size / 2 - h / 2 - 6))
                            .rotationEffect(.degrees(Double(i) * 5))
                    }
                    Capsule()
                        .fill(ContentView.pink)
                        .frame(width: 5, height: size * 0.17)
                        .offset(y: -size * 0.255)
                    Triangle()
                        .fill(ContentView.pink)
                        .frame(width: 20, height: 18)
                        .offset(y: -size * 0.36)
                }
                .rotationEffect(.degrees(Double(degrees)))
                // 固定的部分：中间的度数
                VStack(spacing: 0) {
                    Text("\(degrees)°").font(.system(size: 30, weight: .bold, design: .rounded)).foregroundColor(.white)
                    Text(dragging ? "转动中" : "按住转动").font(.caption2).foregroundColor(ContentView.sub)
                }
                .frame(width: size * 0.36, height: size * 0.36)
                .background(Circle().fill(ContentView.card))
            }
            .frame(width: size, height: size)
            .contentShape(Circle())
            .highPriorityGesture(                        // 手势挂在圆盘本身上：手指坐标以圆盘左上角为原点
                DragGesture(minimumDistance: 0)
                    .onChanged { v in onDrag(v.location) }
                    .onEnded { _ in lastFinger = nil; dragging = false }
            )
            .overlay(alignment: .top) {                  // 0° 的位置
                Triangle()
                    .rotation(.degrees(180))
                    .fill(ContentView.cyan)
                    .frame(width: 14, height: 10)
                    .offset(y: -14)
            }
            .padding(.top, 14)

            HStack(spacing: 10) {
                dialButton("−1°") { set((degrees + 359) % 360) }
                dialButton("+1°") { set((degrees + 1) % 360) }
                dialButton("归零") { set(0) }
            }
        }
        .frame(maxWidth: .infinity)
    }

    /// 手指位置 → 角度（0 = 正上方，顺时针为正）
    private func fingerAngle(_ p: CGPoint) -> Double? {
        let dx = p.x - size / 2, dy = p.y - size / 2
        if dx * dx + dy * dy < 15 * 15 { return nil }        // 太靠近圆心，角度不稳，不算
        return atan2(dx, -dy) * 180 / .pi
    }

    private func onDrag(_ p: CGPoint) {
        guard let a = fingerAngle(p) else { return }
        if let last = lastFinger {
            var d = a - last
            if d > 180 { d -= 360 }
            if d < -180 { d += 360 }
            accum += d
        } else {
            startDeg = degrees; accum = 0; dragging = true
        }
        lastFinger = a
        var n = Int((Double(startDeg) + accum).rounded())
        n = ((n % 360) + 360) % 360
        for s in [0, 90, 180, 270, 360] where abs(n - s) <= 3 { n = s % 360 }   // 吸附到 90° 的倍数
        set(n)
    }

    private func set(_ n: Int) {
        if n == degrees { return }
        if n % 90 == 0 { UIImpactFeedbackGenerator(style: .medium).impactOccurred() }
        else if n % 15 == 0 { UISelectionFeedbackGenerator().selectionChanged() }
        degrees = n
    }

    private func dialButton(_ t: String, _ action: @escaping () -> Void) -> some View {
        Button(action: action) {
            Text(t).font(.subheadline.bold()).foregroundColor(.white)
                .frame(minWidth: 64, minHeight: 34)
                .background(Capsule().fill(ContentView.bg))
                .overlay(Capsule().stroke(ContentView.stroke, lineWidth: 1))
        }
        .buttonStyle(.plain)
    }
}

/// 尖朝上的三角形
struct Triangle: Shape {
    func path(in r: CGRect) -> Path {
        var p = Path()
        p.move(to: CGPoint(x: r.midX, y: r.minY))
        p.addLine(to: CGPoint(x: r.maxX, y: r.maxY))
        p.addLine(to: CGPoint(x: r.minX, y: r.maxY))
        p.closeSubpath()
        return p
    }
}
