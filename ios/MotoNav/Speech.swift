import AVFoundation

/// 版本：跟随 App 版本（见 Config.swift）
///
/// 导航播报：高德把播报文字交给我们，用 iPhone 自带的语音念出来。
/// 播报时把音乐压低（“压低音乐”开关），念完 0.4 秒后恢复；连着蓝牙耳机/头盔时从耳机里出声。
final class Speech: NSObject, AVSpeechSynthesizerDelegate {
    static let shared = Speech()
    private let synth = AVSpeechSynthesizer()
    private var automaticVoice: AVSpeechSynthesisVoice?
    private var busy = 0
    private var releaseWork: DispatchWorkItem?
    private(set) var isSpeaking = false

    override init() {
        super.init()
        synth.delegate = self
        automaticVoice = Self.preferredChineseVoice()
    }

    /// 只自动选择普通话。用 identifier 稳定排序，避免同级音色随枚举顺序切换。
    private static func preferredChineseVoice() -> AVSpeechSynthesisVoice? {
        let voices = AVSpeechSynthesisVoice.speechVoices()
            .filter { $0.language == "zh-CN" }
            .sorted { $0.identifier < $1.identifier }
        if #available(iOS 16.0, *) {
            if let premium = voices.first(where: { $0.quality == .premium }) {
                return premium
            }
        }
        return voices.first(where: { $0.quality == .enhanced })
            ?? AVSpeechSynthesisVoice(language: "zh-CN")
    }

    private func selectedVoice() -> AVSpeechSynthesisVoice? {
        let id = UserDefaults.standard.string(forKey: "voice_id") ?? ""
        if !id.isEmpty, let selected = AVSpeechSynthesisVoice(identifier: id) {
            return selected
        }
        // 每次自动播报前刷新，下载／删除系统音色后无需重装或修改设置。
        // 不把自动结果写入 voice_id，以便以后继续自动升级音色。
        automaticVoice = Self.preferredChineseVoice()
        return automaticVoice
    }

    private func configureSession(active: Bool) {
        let s = AVAudioSession.sharedInstance()
        do {
            if active {
                let opts: AVAudioSession.CategoryOptions = Settings.shared.duck
                    ? [.duckOthers, .interruptSpokenAudioAndMixWithOthers]
                    : [.mixWithOthers]
                try s.setCategory(.playback, mode: .voicePrompt, options: opts)
                try s.setActive(true)
            } else {
                try s.setActive(false, options: .notifyOthersOnDeactivation)   // 让音乐恢复音量
            }
        } catch {
            print("音频设置失败：\(error)")
        }
    }

    func speak(_ text: String) {
        if Settings.shared.voiceMuted { return }        // iOS v0.6 导航画面上点了静音
        let t = text.trimmingCharacters(in: .whitespacesAndNewlines)
        if t.isEmpty { return }
        releaseWork?.cancel()
        if busy == 0 { configureSession(active: true) }
        busy += 1
        isSpeaking = true
        let u = AVSpeechUtterance(string: t)
        u.voice = selectedVoice()
        u.rate = Float(UserDefaults.standard.object(forKey: "voice_rate") as? Double ?? Double(AVSpeechUtteranceDefaultSpeechRate))
        u.pitchMultiplier = Float(UserDefaults.standard.object(forKey: "voice_pitch") as? Double ?? 1.0)
        synth.speak(u)
    }

    private func finished() {
        busy = max(0, busy - 1)
        if busy == 0 {
            let w = DispatchWorkItem { [weak self] in
                guard let self = self, self.busy == 0 else { return }
                self.isSpeaking = false
                self.configureSession(active: false)
            }
            releaseWork = w
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.4, execute: w)
        }
    }

    func speechSynthesizer(_ s: AVSpeechSynthesizer, didFinish u: AVSpeechUtterance) { DispatchQueue.main.async { self.finished() } }
    func speechSynthesizer(_ s: AVSpeechSynthesizer, didCancel u: AVSpeechUtterance) { DispatchQueue.main.async { self.finished() } }

    /// 马上停止正在念的（点了静音）
    func stopNow() { synth.stopSpeaking(at: .immediate) }

    /// 试听（不管静音开关）
    func test() {
        let m = Settings.shared.voiceMuted
        Settings.shared.voiceMuted = false
        speak("前方三百米右转，进入人民南路。注意，前方有测速，限速六十。")
        Settings.shared.voiceMuted = m
    }
}
