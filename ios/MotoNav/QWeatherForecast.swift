import Foundation

/// QWeather documents minute-resolution RFC3339 timestamps such as 2024-05-31T03:00Z.
/// Foundation's default ISO8601DateFormatter requires seconds. Preserve the offset while
/// normalizing only minute-resolution values; also accept seconds and fractional seconds.
enum QWeatherForecast {
    struct Hour: Equatable {
        let hour: Int, icon: Int, temperature: Int, probability: Int
        var wire: String { "\(hour),\(icon),\(temperature),\(probability)" }
    }
    struct Forecast {
        let hours: [Hour], note: String?
    }
    static func timestamp(_ raw: String) -> Date? {
        let text = raw.trimmingCharacters(in: .whitespacesAndNewlines)
        let normalized = text.replacingOccurrences(
            of: #"^(\d{4}-\d{2}-\d{2}T\d{2}:\d{2})(Z|[+-]\d{2}:\d{2})$"#,
            with: "$1:00$2", options: .regularExpression)
        let plain = ISO8601DateFormatter()
        if let date = plain.date(from: normalized) { return date }
        let fractional = ISO8601DateFormatter()
        fractional.formatOptions = [.withInternetDateTime, .withFractionalSeconds]
        return fractional.date(from: normalized)
    }
    static func parse(_ response: [String: Any]?, now: Date = Date(), timeZone: TimeZone = .current,
                      icon: (String, Bool) -> Int) -> Forecast {
        guard let values = response?["hours"] as? [[String: Any]] else {
            return Forecast(hours: [], note: "逐小时接口没有返回预报列表")
        }
        var valid: [(Date, Hour)] = [], badTimes = 0, badTemps = 0, past = 0
        var calendar = Calendar(identifier: .gregorian); calendar.timeZone = timeZone
        for item in values {
            guard let date = timestamp(item["forecastTime"] as? String ?? "") else { badTimes += 1; continue }
            guard date > now else { past += 1; continue }
            let temperature = item["temperature"] as? [String: Any] ?? [:]
            let temp = (temperature["value"] as? NSNumber)?.doubleValue ?? Double(temperature["value"] as? String ?? "")
            guard let temp = temp, temp.isFinite, (-100...70).contains(temp) else { badTemps += 1; continue }
            let hour = calendar.component(.hour, from: date)
            let precipitation = item["precipitation"] as? [String: Any] ?? [:]
            let p = (precipitation["probability"] as? NSNumber)?.doubleValue ?? Double(precipitation["probability"] as? String ?? "")
            let probability = p != nil && p!.isFinite && (0...1).contains(p!) ? Int((p! * 100).rounded()) : -1
            let text = (item["condition"] as? [String: Any])?["text"] as? String ?? ""
            valid.append((date, Hour(hour: hour, icon: icon(text, hour < 6 || hour >= 19), temperature: Int(temp.rounded()), probability: probability)))
        }
        // Avoid duplicate forecast times and show the next five hours in chronological order.
        var seen = Set<Date>()
        let hours = valid.sorted { $0.0 < $1.0 }.filter { seen.insert($0.0).inserted }.prefix(5).map { $0.1 }
        var reasons: [String] = []
        if badTimes > 0 { reasons.append("\(badTimes) 条预报时间无效") }
        if badTemps > 0 { reasons.append("\(badTemps) 条温度无效") }
        if hours.isEmpty {
            if values.isEmpty { reasons.append("逐小时列表为空") }
            else if past == values.count { reasons.append("逐小时预报已过期") }
            else if reasons.isEmpty { reasons.append("没有有效的未来逐小时预报") }
        }
        return Forecast(hours: hours, note: reasons.isEmpty ? nil : reasons.joined(separator: "；"))
    }
}

enum QWeatherRequestError: LocalizedError {
    case http(Int), service(String), invalidData
    var errorDescription: String? {
        switch self {
        case .http(let status):
            switch status {
            case 401, 403: return "HTTP \(status)，请检查 Key 和服务权限"
            case 429: return "HTTP 429，请求额度或频率受限"
            default: return "HTTP \(status)"
            }
        case .service(let code): return "和风服务状态 \(code)"
        case .invalidData: return "接口数据格式无效"
        }
    }
}
