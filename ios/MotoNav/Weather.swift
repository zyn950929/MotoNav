import Foundation
import CoreLocation
import SwiftUI

/// Same endpoints, units and screen protocol as Android QWeatherClient.
final class Weather: ObservableObject {
    static let shared = Weather()
    @Published var info = "在圆屏切换到天气页查询"
    @Published var detail = "尚未查询天气"
    var host: String { UserDefaults.standard.string(forKey: "qw_host") ?? "" }
    var key: String { UserDefaults.standard.string(forKey: "qw_key") ?? "" }
    private var generation = 0
    private var busy = false
    private var lastAt = Date.distantPast
    private var cacheLifetime: TimeInterval = 600
    private var retryAt = Date.distantPast
    private var messages: [String] = []
    private var images: [(Int, TextImage.Img?)] = []
    private let geocoder = CLGeocoder()

    var displaySummary:String {
        if info.contains("失败") || info.contains("HTTP") || info.contains("Error") || info.contains("无效") { return "天气暂未更新，请检查配置或稍后重试" }
        return String(info.split(separator:"；",maxSplits:1).first ?? "")
    }
    func save(host: String, key: String) throws {
        let normalized = host.trimmingCharacters(in: .whitespacesAndNewlines)
            .replacingOccurrences(of: "https://", with: "").trimmingCharacters(in: CharacterSet(charactersIn: "/")).lowercased()
        guard normalized.range(of: #"^[a-z0-9-]+(?:\.[a-z0-9-]+)*\.qweatherapi\.com$"#, options: .regularExpression) != nil,
              !key.isEmpty, !key.contains(where: { $0.isWhitespace }) else {
            throw NSError(domain: "Weather", code: 1, userInfo: [NSLocalizedDescriptionKey: "请填写 xxxx.qweatherapi.com API Host 和有效 API Key"])
        }
        UserDefaults.standard.set(normalized, forKey: "qw_host")
        UserDefaults.standard.set(key, forKey: "qw_key")
        generation += 1; busy = false; messages = []; images = []; lastAt = .distantPast; retryAt = .distantPast
        info = "配置已保存，请切换到天气页查询"; detail = info
        clearScreen()
    }
    private func clearScreen() {
        BleLink.shared.clearWeather()
    }
    func request(force: Bool = false) {
        guard !host.isEmpty, !key.isEmpty else { info = "请先配置和风天气"; clearScreen(); return }
        if !force && !messages.isEmpty && Date().timeIntervalSince(lastAt) < cacheLifetime { push(); return }
        guard !busy, Date() >= retryAt else { return }
        guard let loc = LocationService.shared.location, abs(loc.timestamp.timeIntervalSinceNow) < 120,
              loc.horizontalAccuracy >= 0 else { info = "等待手机定位，请稍后重试"; LocationService.shared.start(); return }
        busy = true; info = "正在查天气…"
        generation += 1
        let id = generation, accountHost = host, accountKey = key
        geocoder.reverseGeocodeLocation(loc, preferredLocale: Locale(identifier: "zh_CN")) { [weak self] marks, _ in
            DispatchQueue.main.async {
                guard let self = self, self.generation == id else { return }
                let p = marks?.first
                let place = p?.subLocality ?? p?.locality ?? p?.administrativeArea ?? "当前位置"
                Task { await self.fetch(loc, place: place, host: accountHost, key: accountKey, id: id) }
            }
        }
    }
    @MainActor private func fetch(_ loc: CLLocation, place: String, host: String, key: String, id: Int) async {
        let point = String(format: "%.2f/%.2f", locale: Locale(identifier: "en_US_POSIX"), loc.coordinate.latitude, loc.coordinate.longitude)
        let location = String(format: "%.2f%%2C%.2f", locale: Locale(identifier: "en_US_POSIX"), loc.coordinate.longitude, loc.coordinate.latitude)
        let paths = ["/weather/v1/current/\(point)?lang=zh&localTime=true", "/weather/v1/hourly/\(point)?hours=6&lang=zh&localTime=true", "/v7/minutely/5m?location=\(location)&lang=zh", "/airquality/v1/current/\(point)?lang=zh", "/weatheralert/v1/current/\(point)?lang=zh&localTime=true"]
        var results: [[String: Any]?] = [], notes: [String] = []
        let labels = ["实况", "逐小时", "降水", "空气", "预警"]
        for (i, path) in paths.enumerated() {
            do {
                let json = try await QWeatherHTTP.get(host: host, key: key, path: path)
                guard generation == id else { return }
                results.append(json)
            } catch {
                guard generation == id else { return }
                notes.append("\(labels[i])：\(error.localizedDescription)"); results.append(nil)
                if i == 0 { busy = false; retryAt = Date().addingTimeInterval(30); info = notes.joined(separator: "；"); clearScreen(); return }
            }
        }
        guard generation == id else { return }
        busy = false
        do { try build(results, place: place, notes: notes) }
        catch { info = "天气数据无效"; retryAt = Date().addingTimeInterval(30); clearScreen() }
    }
    private func build(_ r: [[String: Any]?], place: String, notes: [String]) throws {
        func dict(_ v: Any?) -> [String: Any] { v as? [String: Any] ?? [:] }
        func number(_ v: Any?) -> Double? {
            let n = (v as? NSNumber)?.doubleValue ?? Double(v as? String ?? "")
            return n?.isFinite == true ? n : nil
        }
        func integer(_ v: Any?, fallback: Int = -1) -> Int {
            guard let n = number(v), abs(n) < 1_000_000 else { return fallback }; return Int(n.rounded())
        }
        let now = r[0] ?? [:], text = dict(now["condition"])["text"] as? String ?? ""
        guard let temp = number(dict(now["temperature"])["value"]), (-100...70).contains(temp) else { throw URLError(.cannotParseResponse) }
        let night = Calendar.current.component(.hour, from: Date()) < 6 || Calendar.current.component(.hour, from: Date()) >= 19
        let wind = dict(now["wind"]), direction = integer(dict(wind["direction"])["degree"])
        let windName = direction >= 0 && direction < 360 ? ["北", "东北", "东", "东南", "南", "西南", "西", "西北"][((direction + 22) / 45) % 8] + "风" : "风向未知"
        let humidity = number(now["humidity"])
        let windText = windName + " \(integer(wind["scale"]))级 · " + (humidity != nil && (0...1).contains(humidity!) ? "湿度 \(Int((humidity! * 100).rounded()))%" : "湿度未知")
        let referenceTime = Date()
        let forecast = QWeatherForecast.parse(r[1], now: referenceTime, icon: { Self.icon($0, night: $1) })
        let hours = forecast.hours.map { $0.wire }
        let allNotes = notes + (forecast.note.map { [$0] } ?? [])
        cacheLifetime = hours.isEmpty ? 30 : 600
        let rain = r[2]?["summary"] as? String ?? "降水预报暂不可用"
        let bars = (r[2]?["minutely"] as? [[String: Any]] ?? []).prefix(24).map { item -> Int in
            guard let n = number(item["precip"]), n >= 0 else { return -1 }; return Int(min(9999, (n * 100).rounded()))
        }
        let indexes = r[3]?["indexes"] as? [[String: Any]] ?? []
        let index = indexes.first { $0["code"] as? String == "cn-mee" } ?? indexes.first ?? [:]
        let aqi = integer(index["aqi"])
        let air = aqi >= 0 ? "\(index["name"] as? String ?? "AQI") \(index["aqiDisplay"] as? String ?? String(aqi)) · \(index["category"] as? String ?? "")" : "空气质量暂不可用"
        let alerts = (r[4]?["alerts"] as? [[String: Any]] ?? []).filter {
            dict($0["messageType"])["code"] as? String != "cancel" && (QWeatherForecast.timestamp($0["expireTime"] as? String ?? "") ?? .distantFuture) > Date()
        }
        func level(_ a: [String: Any]) -> Int { ["minor": 1, "moderate": 2, "severe": 3, "extreme": 4][a["severity"] as? String ?? ""] ?? 1 }
        let top = alerts.max { level($0) < level($1) }
        let confirmed = r[4]?["alerts"] is [[String: Any]] || dict(r[4]?["metadata"])["zeroResult"] as? Bool == true
        let title = top?["headline"] as? String ?? (confirmed ? "当前位置暂无有效预警" : "预警信息暂不可用")
        let body = top?["description"] as? String ?? "", advice = top?["instruction"] as? String ?? ""
        let f = DateFormatter(); f.dateFormat = "HH:mm"
        // Stable Java-compatible hash: the firmware uses it to distinguish warnings.
        var hash: Int32 = 0
        for c in title.utf16 { hash = hash &* 31 &+ Int32(c) }
        messages = ["#WX,\(aqi),\(confirmed ? alerts.count : -1),\(top.map(level) ?? 0),\(bars.count),\(hash & 0x7fffffff)" + bars.map { ",\($0)" }.joined(),
                    "#W,\(Self.icon(text, night: night)),\(Int(temp.rounded())),\(f.string(from: Date())),H,\(hours.count)" + hours.map { ",\($0)" }.joined()]
        images = [(3, "\(place) · \(text)"), (4, windText), (7, rain), (8, air), (9, title), (10, String(body.prefix(60))), (11, String(advice.prefix(60)))].map {
            ($0.0, TextImage.render($0.1, maxW: 330, size: 24, minSize: 18))
        }
        var sources = ["和风天气 QWeather · https://www.qweather.com/", "https://developer.qweather.com/attribution.html"]
        for item in r { sources += dict(item?["metadata"])["attributions"] as? [String] ?? [] }
        var detailParts:[String] = ["\(place) · \(text) \(Int(temp.rounded()))°", windText, rain, air]
        let warningDetails:[String] = alerts.map { alert in
            let fields:[String] = [alert["headline"],alert["description"],alert["instruction"]].compactMap{$0 as? String}
            return fields.joined(separator:"\n")
        }
        detailParts.append(contentsOf:warningDetails)
        detailParts.append(title)
        detailParts.append(contentsOf:allNotes)
        detailParts.append(contentsOf:Array(Set(sources)).sorted())
        detailParts.append("预警可能延迟，请以官方最新发布为准。更新：\(f.string(from: Date()))")
        detail = detailParts.joined(separator:"\n\n")
        info = "\(place) \(text) \(Int(temp.rounded()))°；逐小时 \(hours.count) 个" + (allNotes.isEmpty ? "" : "；" + allNotes.joined(separator: "；"))
        lastAt = Date(); push()
    }
    private func push() {
        for (slot, im) in images { BleLink.shared.sendImage(slot: slot, w: im?.w ?? 0, h: im?.h ?? 0, data: im?.data ?? Data()) }
        for message in messages { BleLink.shared.send(message) }
    }
    static func icon(_ text: String, night: Bool) -> Int {
        if text.contains("雷") { return 5 }
        if text.contains("冰雹") || (text.contains("雪") && text.contains("雨")) { return 8 }
        if text.contains("雪") { return 6 }
        if text.contains("暴雨") || text.contains("大雨") { return 4 }
        if text.contains("雨") { return 3 }
        if text.contains("雾") || text.contains("霾") { return 7 }
        if text.contains("沙") || text.contains("尘") { return 9 }
        if text.contains("阴") { return 2 }
        if text.contains("云") { return night ? 11 : 1 }
        if text.contains("晴") { return night ? 10 : 0 }
        return 2
    }
}

/// Disallow redirects so an account credential cannot be forwarded to another host.
final class QWeatherHTTP: NSObject, URLSessionTaskDelegate {
    static let shared = QWeatherHTTP()
    private lazy var session = URLSession(configuration: .ephemeral, delegate: self, delegateQueue: nil)
    func urlSession(_ session: URLSession, task: URLSessionTask, willPerformHTTPRedirection response: HTTPURLResponse, newRequest request: URLRequest, completionHandler: @escaping (URLRequest?) -> Void) { completionHandler(nil) }
    static func get(host: String, key: String, path: String) async throws -> [String: Any] {
        var request = URLRequest(url: URL(string: "https://\(host)\(path)")!, timeoutInterval: 15)
        request.setValue(key, forHTTPHeaderField: "X-QW-Api-Key")
        let (data, response) = try await shared.session.data(for: request)
        let status = (response as? HTTPURLResponse)?.statusCode ?? 0
        guard status == 200 else { throw QWeatherRequestError.http(status) }
        guard let json = try JSONSerialization.jsonObject(with: data) as? [String: Any] else { throw QWeatherRequestError.invalidData }
        if let code = json["code"], String(describing: code) != "200" {
            let safeCode = String(describing: code).filter { $0.isNumber }.prefix(8)
            throw QWeatherRequestError.service(safeCode.isEmpty ? "未知" : String(safeCode))
        }
        return json
    }
}

struct WeatherSettingsView: View {
    @ObservedObject private var weather = Weather.shared
    @State private var host = Weather.shared.host
    @State private var key = Weather.shared.key
    @State private var error = ""
    var body: some View {
        DisclosureGroup("和风天气 · 可选配置") {
            Text("填写 API Host 和 API Key 后，可在圆屏查询当前天气和逐小时预报。")
                .font(.caption).foregroundColor(ContentView.sub)
            TextField("API Host：xxxx.qweatherapi.com", text: $host).textInputAutocapitalization(.never).autocorrectionDisabled()
            SecureField("API Key", text: $key).textInputAutocapitalization(.never).autocorrectionDisabled()
            Button("保存天气配置") {
                do { try weather.save(host: host, key: key.trimmingCharacters(in: .whitespacesAndNewlines)); error = "" }
                catch { self.error = error.localizedDescription }
            }
            if !error.isEmpty { Text(error).foregroundColor(.orange) }
            Button("刷新天气") { weather.request(force: true) }
            Text(weather.displaySummary).font(.caption)
            Link("和风天气来源说明", destination: URL(string: "https://developer.qweather.com/attribution.html")!)
        }.tint(ContentView.cyan)
    }
}

extension Array {
    subscript(safe i: Int) -> Element? { indices.contains(i) ? self[i] : nil }
}
