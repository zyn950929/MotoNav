import Foundation
import Combine

/// AirPods 式连接动画：每次手动/自动连接（含断开后再连）都弹出，不只首次。
final class ConnectAnimationCoordinator: ObservableObject {
    static let shared = ConnectAnimationCoordinator()

    enum Kind { case first, manual, auto }

    @Published var isPresented = false
    @Published var kind: Kind = .manual
    @Published var phase: Phase = .idle
    @Published var statusText = "正在连接圆屏…"
    @Published var showBattery = false

    enum Phase { case idle, spinning, connected }

    let spinSeconds: TimeInterval = 3.2
    private var spinStartedAt: Date?
    private var readyWatch: AnyCancellable?
    private var timeoutWork: DispatchWorkItem?
    private var finishWork: DispatchWorkItem?
    private var everConnected = false

    var deviceTitle: String {
        BleLink.shared.screenName.isEmpty ? "MotoNav" : BleLink.shared.screenName
    }

    var footerHint: String {
        switch kind {
        case .first: return "首次配对"
        case .manual: return "手动连接"
        case .auto: return "自动重连"
        }
    }

    /// 用户点「连接」：断开后再连也整段重播动画
    func connectManually() {
        kind = everConnected ? .manual : .first
        presentAndConnect(forceRestart: true)
    }

    /// App 启动 / 静默重连：同样弹动画
    func connectAutomatically() {
        kind = everConnected ? .auto : .first
        presentAndConnect(forceRestart: true)
    }

    /// 兼容旧设置页入口（不再用于跳过动画）
    func resetFirstFlag() { everConnected = false }

    func dismiss() {
        timeoutWork?.cancel()
        finishWork?.cancel()
        readyWatch?.cancel()
        readyWatch = nil
        isPresented = false
        phase = .idle
        showBattery = false
        statusText = "正在连接圆屏…"
    }

    private func presentAndConnect(forceRestart: Bool) {
        if isPresented && !forceRestart {
            BleLink.shared.connect()
            return
        }
        timeoutWork?.cancel()
        finishWork?.cancel()
        readyWatch?.cancel()
        isPresented = true
        phase = .spinning
        showBattery = false
        statusText = kind == .auto ? "正在重新连接圆屏…" : "正在连接圆屏…"
        spinStartedAt = Date()
        BleLink.shared.connect()

        readyWatch = Publishers.CombineLatest(BleLink.shared.$isReady,BleLink.shared.$verified)
            .receive(on: DispatchQueue.main)
            .sink { [weak self] ready,verified in
                guard let self, self.isPresented, ready,verified else { return }
                self.scheduleConnected()
            }
        if BleLink.shared.isReady && BleLink.shared.verified { scheduleConnected() }
        let timeout=DispatchWorkItem{[weak self] in self?.dismiss()}
        timeoutWork=timeout
        DispatchQueue.main.asyncAfter(deadline:.now()+20,execute:timeout)
    }

    private func scheduleConnected() {
        let started = spinStartedAt ?? Date()
        let remain = max(0, spinSeconds - Date().timeIntervalSince(started))
        finishWork?.cancel()
        let work = DispatchWorkItem { [weak self] in
            guard let self, self.isPresented,BleLink.shared.isReady,BleLink.shared.verified else { return }
            self.phase = .connected
            self.statusText = self.kind == .auto ? "已重新连接圆屏" : "已连接圆屏"
            self.showBattery = true
            self.everConnected = true
            DispatchQueue.main.asyncAfter(deadline: .now() + 1.2) { [weak self] in
                guard let self, self.isPresented, self.phase == .connected else { return }
                self.dismiss()
            }
        }
        finishWork = work
        DispatchQueue.main.asyncAfter(deadline: .now() + remain, execute: work)
    }
}
