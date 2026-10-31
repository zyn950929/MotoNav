import SwiftUI
import CoreLocation

final class Favorites: ObservableObject {
    static let shared = Favorites()
    struct Place: Codable, Identifiable {
        var id = UUID(); var name: String; var address: String; var lat: Double; var lng: Double
        var searchPlace: SearchService.Place { .init(name: name, address: address, lat: lat, lng: lng) }
    }
    @Published private(set) var places: [Place] = []
    private init() {
        if let data = UserDefaults.standard.data(forKey: "favorite_places") { places = (try? JSONDecoder().decode([Place].self, from: data)) ?? [] }
    }
    func add(_ p: SearchService.Place) {
        guard !places.contains(where: { NaviBridge.distM($0.lat, $0.lng, p.lat, p.lng) < 50 }) else { return }
        places.insert(Place(name: p.name, address: p.address, lat: p.lat, lng: p.lng), at: 0); save()
    }
    func remove(_ p: Place) { places.removeAll { $0.id == p.id }; save() }
    private func save() { if let data = try? JSONEncoder().encode(places) { UserDefaults.standard.set(data, forKey: "favorite_places") } }
}

/// WGS84 fixes only. Simulation is excluded; each accepted fix is atomically checkpointed.
final class RideRecorder: ObservableObject {
    static let shared = RideRecorder()
    struct Fix: Codable { let lat: Double; let lng: Double; let time: Date; let speed: Double }
    struct Ride: Codable, Identifiable {
        var id = UUID(); let started: Date; var ended: Date?; var mode: String; var fixes: [Fix] = []; var meters = 0.0
    }
    @Published private(set) var rides: [Ride] = []
    @Published private(set) var active: Ride?
    @Published var error = ""
    private let root = FileManager.default.urls(for: .documentDirectory, in: .userDomainMask)[0].appendingPathComponent("Rides", isDirectory: true)
    private init() {
        do {
            try FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
            for file in try FileManager.default.contentsOfDirectory(at: root, includingPropertiesForKeys: nil) where file.pathExtension == "json" {
                if var ride = try? JSONDecoder().decode(Ride.self, from: Data(contentsOf: file)) {
                    if ride.ended == nil { ride.ended = ride.fixes.last?.time ?? ride.started; try write(ride) }
                    rides.append(ride)
                }
            }
            rides.sort { $0.started > $1.started }
        } catch { self.error = "骑行记录读取失败：\(error.localizedDescription)" }
    }
    private func write(_ ride: Ride) throws {
        try JSONEncoder().encode(ride).write(to: root.appendingPathComponent(ride.id.uuidString + ".json"), options: [.atomic, .completeFileProtectionUntilFirstUserAuthentication])
    }
    func start(_ mode: String) {
        finish(); active = Ride(started: Date(), mode: mode); checkpoint()
    }
    func fix(_ l: CLLocation) {
        guard var ride = active, l.horizontalAccuracy >= 0, l.horizontalAccuracy <= 60,
              abs(l.timestamp.timeIntervalSinceNow) < 15 else { return }
        if let last = ride.fixes.last {
            guard l.timestamp > last.time else { return }
            let distance = CLLocation(latitude: last.lat, longitude: last.lng).distance(from: l)
            guard distance / l.timestamp.timeIntervalSince(last.time) < 90 else { return }
            guard distance >= 3 || l.timestamp.timeIntervalSince(last.time) >= 10 else { return }
            ride.meters += distance
        }
        ride.fixes.append(Fix(lat: l.coordinate.latitude, lng: l.coordinate.longitude, time: l.timestamp, speed: max(0, l.speed)))
        active = ride; checkpoint()
    }
    private func checkpoint() {
        guard let ride = active else { return }
        do { try write(ride) } catch { self.error = "轨迹保存失败：\(error.localizedDescription)" }
    }
    func finish() {
        guard var ride = active else { return }; ride.ended = Date(); active = nil
        do { try write(ride); rides.insert(ride, at: 0) } catch { self.error = "轨迹保存失败：\(error.localizedDescription)" }
    }
    func remove(_ ride: Ride) {
        do { try FileManager.default.removeItem(at: root.appendingPathComponent(ride.id.uuidString + ".json")); rides.removeAll { $0.id == ride.id } }
        catch { self.error = error.localizedDescription }
    }
    func export(_ ride: Ride) throws -> URL {
        let formatter = ISO8601DateFormatter()
        let points = ride.fixes.map { "<trkpt lat=\"\($0.lat)\" lon=\"\($0.lng)\"><time>\(formatter.string(from: $0.time))</time></trkpt>" }.joined()
        let xml = "<?xml version=\"1.0\" encoding=\"UTF-8\"?><gpx version=\"1.1\" creator=\"MotoNav\" xmlns=\"http://www.topografix.com/GPX/1/1\"><trk><name>MotoNav</name><trkseg>\(points)</trkseg></trk></gpx>"
        let url = FileManager.default.temporaryDirectory.appendingPathComponent("MotoNav-\(ride.id).gpx")
        try Data(xml.utf8).write(to: url, options: .atomic); return url
    }
}

struct RideHistoryView:View {
    @ObservedObject var recorder=RideRecorder.shared
    var body:some View {
        List {
            if let active=recorder.active {Text("记录中 · \(active.mode) · \(Int(active.meters)) 米")}
            if !recorder.error.isEmpty {Text(ContentView.userMessage(recorder.error)).foregroundColor(AppTheme.red)}
            ForEach(recorder.rides){ride in
                NavigationLink(destination:RideDetailView(ride:ride)){RideListRow(ride:ride)}
            }
            if recorder.rides.isEmpty && recorder.active == nil {Text("真实导航或巡航开始时自动记录；模拟导航不记录。")}
        }.navigationTitle("导航轨迹")
    }
}
struct ExportSheet: UIViewControllerRepresentable {
    let url: URL
    func makeUIViewController(context: Context) -> UIActivityViewController { UIActivityViewController(activityItems: [url], applicationActivities: nil) }
    func updateUIViewController(_ uiViewController: UIActivityViewController, context: Context) {}
}
