import SwiftUI
import UIKit
import CoreLocation
import ObjectiveC

/// A separate north-up map renderer. It never changes the route preview/navigation camera.
final class AmapCaptureHost: NSObject {
    static let shared = AmapCaptureHost()
    struct Shot {
        let pixels: [UInt32], withoutBuildings: [UInt32]?, width: Int, height: Int
        let left: Double, top: Double, right: Double, bottom: Double
        let north: Double, south: Double
    }
    private weak var container: UIView?
    private var map: MAMapView?
    private var serial = 0
    private var pending: ((Result<Shot, Error>) -> Void)?
    private var started = Date.distantPast
    private var reference: [UInt32]?
    private var referenceProjection: [Double]?
    private var emptyBuildingChecks = 0
    private var timer: Timer?
    private var observer: NSObjectProtocol?
    private var satelliteCapture=false
    private var mpp: Double { satelliteCapture ? 3.5 : 2.0 }

    func attach(_ view: UIView) {
        container = view
        #if DEBUG
        MapCaptureCheck.startIfRequested()
        #endif
        if observer == nil {
            observer = NotificationCenter.default.addObserver(forName: UIApplication.willResignActiveNotification, object: nil, queue: .main) { [weak self] _ in self?.cancel() }
        }
    }
    func detach(_ view: UIView) {
        if container === view { cancel(); map?.removeFromSuperview(); map = nil; container = nil }
    }
    private func ensureMap(size: CGFloat) throws -> MAMapView {
        guard UIApplication.shared.applicationState == .active, let view = container, view.window != nil else {
            throw MapProcessingError.invalid("高德截图需保持 App 在前台；后台使用已离线的地图")
        }
        guard !Settings.shared.amapKeyInUse.isEmpty else { throw MapProcessingError.invalid("请先填写高德 iOS Key") }
        let m: MAMapView
        if let existing = map { m = existing } else {
            m = MAMapView(frame: CGRect(x: 0, y: 0, width: size, height: size))
            m.contentScaleFactor = 1; m.layer.contentsScale = 1
            m.mapType = .standard
            m.isShowsLabels = false; m.isShowsBuildings = false; m.isShowsIndoorMap = false
            m.showsUserLocation = false; m.isShowTraffic = false; m.showsCompass = false; m.showsScale = false
            m.isZoomEnabled = false; m.isScrollEnabled = false; m.isRotateEnabled = false; m.isRotateCameraEnabled = false
            m.isAccessibilityElement = false; m.accessibilityElementsHidden = true
            class_addProtocol(AmapCaptureHost.self, MAMapViewDelegate.self)
            m.delegate = (self as AnyObject) as? MAMapViewDelegate
            view.addSubview(m); map = m
        }
        m.transform = .identity
        // Extraction uses a light land/road palette, regardless of the phone UI theme.
        m.overrideUserInterfaceStyle = .light
        m.bounds = CGRect(x: 0, y: 0, width: size, height: size)
        m.center = CGPoint(x: size / 2, y: size / 2)
        // GLKit uses the displayed geometry for its drawable. A one-point transform produces
        // transparent snapshots even when the SDK reports state=1. Keep actual render geometry.
        m.layoutIfNeeded()
        m.renderringDisabled = false
        m.isAllowDecreaseFrame = false
        #if DEBUG
        if ProcessInfo.processInfo.arguments.contains("--map-capture-check"),
           !ProcessInfo.processInfo.arguments.contains("--map-capture-hidden"), let window = view.window {
            window.addSubview(m)
            m.frame = CGRect(x: 0, y: 0, width: size, height: size)
        }
        #endif
        return m
    }
    @objc(mapViewDidFailLoadingMap:withError:)
    func mapLoadFailed(_ map: MAMapView, error: NSError) {
        finish(.failure(MapProcessingError.invalid("高德地图加载失败：\(error.domain) \(error.code)")))
    }
    func capture(_ frame: MapGeo.Frame, x: Int, y: Int, size n: Int, satellite: Bool = false, style: Int = 1, completion: @escaping (Result<Shot, Error>) -> Void) {
        precondition(Thread.isMainThread)
        guard pending == nil else { completion(.failure(MapProcessingError.invalid("高德截图正在处理上一块"))); return }
        do {
            let m = try ensureMap(size: CGFloat(AmapCapturePolicy.renderSize(for: n)))
            satelliteCapture=satellite
            m.mapType = satellite ? .satellite : .standard
            m.isShowsLabels = style == 0
            pending = completion; reference = nil; referenceProjection = nil; emptyBuildingChecks = 0; started = Date(); serial += 1
            let id = serial
            m.isShowsBuildings = false; m.rotationDegree = 0; m.cameraDegree = n == 600 ? 0.05 : 0
            let margin = n == 600 ? 20.0 : 100.0
            let north = frame.lat(Double(y) + margin), south = frame.lat(Double(y) - Double(n)*self.mpp - margin)
            let west = frame.lng(Double(x) - margin), east = frame.lng(Double(x) + Double(n)*self.mpp + margin)
            let nw = MAMapPointForCoordinate(CLLocationCoordinate2D(latitude: north, longitude: west))
            let se = MAMapPointForCoordinate(CLLocationCoordinate2D(latitude: south, longitude: east))
            let rect = MAMapRectMake(min(nw.x, se.x), min(nw.y, se.y), abs(se.x - nw.x), abs(se.y - nw.y))
            // The SDK's internal viewport catches up on a render tick after UIView resizing.
            // Fitting immediately uses the old 720-point viewport and drops building zoom below 17.
            DispatchQueue.main.asyncAfter(deadline: .now() + 0.35) {
                guard self.serial == id, self.pending != nil else { return }
                m.setVisibleMapRect(rect, animated: false)
                m.cameraDegree = n == 600 ? 0.05 : 0
                DispatchQueue.main.asyncAfter(deadline: .now() + 1) { self.snapshot(id, frame: frame, x: x, y: y, n: n) }
            }
            timer = Timer.scheduledTimer(withTimeInterval: n == 600 ? 24 : 40, repeats: false) { [weak self] _ in
                guard let self = self, self.serial == id else { return }
                self.finish(.failure(MapProcessingError.invalid("高德取图超时，保留已有地图")))
            }
        } catch { completion(.failure(error)) }
    }
    private func snapshot(_ id: Int, frame: MapGeo.Frame, x: Int, y: Int, n: Int) {
        guard serial == id, pending != nil, let m = map else { return }
        guard UIApplication.shared.applicationState == .active else { cancel(); return }
        m.takeSnapshot(in: m.bounds, timeoutInterval: 3) { [weak self] image, state in
            DispatchQueue.main.async {
                guard let self = self, self.serial == id, self.pending != nil else { return }
                guard state == 1, let cg = image?.cgImage else {
                    DispatchQueue.main.asyncAfter(deadline: .now() + 0.8) { self.snapshot(id, frame: frame, x: x, y: y, n: n) }; return
                }
                #if DEBUG
                MapCaptureCheck.record(image: image, map: m, tile: n, buildings: self.reference != nil)
                #endif
                do {
                    let width = min(cg.width, 2880), height = min(cg.height, 2880)
                    guard width <= 5000, height <= 5000, width > 0, height > 0,
                          abs(m.rotationDegree) < 0.1, abs(m.cameraDegree) < 0.1 else { throw MapProcessingError.invalid("高德截图尺寸或方向异常") }
                    if n == 600, m.zoomLevel < 17 { throw MapProcessingError.invalid("建筑取图缩放级别不足") }
                    let north = frame.lat(Double(y)), south = frame.lat(Double(y) - Double(n)*self.mpp)
                    let tl = m.convert(CLLocationCoordinate2D(latitude: north, longitude: frame.lng(Double(x))), toPointTo: m)
                    let tr = m.convert(CLLocationCoordinate2D(latitude: north, longitude: frame.lng(Double(x) + Double(n)*self.mpp)), toPointTo: m)
                    let bl = m.convert(CLLocationCoordinate2D(latitude: south, longitude: frame.lng(Double(x))), toPointTo: m)
                    let br = m.convert(CLLocationCoordinate2D(latitude: south, longitude: frame.lng(Double(x) + Double(n)*self.mpp)), toPointTo: m)
                    let sx = Double(width) / Double(m.bounds.width), sy = Double(height) / Double(m.bounds.height)
                    guard abs(tl.y - tr.y) * sy <= 2, abs(tl.x - bl.x) * sx <= 2,
                          abs(bl.y - br.y) * sy <= 2, abs(tr.x - br.x) * sx <= 2,
                          Double(tr.x - tl.x) * sx >= Double(n - 2), Double(bl.y - tl.y) * sy >= Double(n - 2) else { throw MapProcessingError.invalid("高德截图投影未对齐或精度不足") }
                    var pixels = [UInt32](repeating: 0, count: width * height)
                    let ok = pixels.withUnsafeMutableBytes { raw -> Bool in
                        guard let context = CGContext(data: raw.baseAddress, width: width, height: height, bitsPerComponent: 8, bytesPerRow: width * 4, space: CGColorSpaceCreateDeviceRGB(), bitmapInfo: CGImageAlphaInfo.premultipliedFirst.rawValue | CGBitmapInfo.byteOrder32Little.rawValue) else { return false }
                        context.draw(cg, in: CGRect(x: 0, y: 0, width: width, height: height)); return true
                    }
                    guard ok else { throw MapProcessingError.invalid("高德截图像素解码失败") }
                    // SDK state=1 can precede the first GPU frame. Do not publish a transparent image.
                    if stride(from: 0, to: pixels.count, by: 97).contains(where: { pixels[$0] >> 24 < 250 }) {
                        DispatchQueue.main.asyncAfter(deadline: .now() + 0.8) { self.snapshot(id, frame: frame, x: x, y: y, n: n) }; return
                    }
                    let projection = [Double(tl.x) * sx, Double(tl.y) * sy, Double(tr.x) * sx, Double(bl.y) * sy]
                    // Both shots share exactly the same camera. Only the SDK building layer changes.
                    if n == 600, self.reference == nil {
                        self.reference = pixels; self.referenceProjection = projection; m.isShowsBuildings = true
                        DispatchQueue.main.asyncAfter(deadline: .now() + 1.2) { self.snapshot(id, frame: frame, x: x, y: y, n: n) }; return
                    }
                    if n == 600, let reference = self.reference {
                        guard reference.count == pixels.count, let original = self.referenceProjection, AmapCapturePolicy.aligned(original, projection) else { throw MapProcessingError.invalid("建筑两次截图尺寸或相机发生变化") }
                        let changed = zip(reference, pixels).contains { pair in
                            AmapBuildings.colorDifference(pair.0, pair.1) >= 4
                        }
                        if !changed && self.emptyBuildingChecks < 2 {
                            self.emptyBuildingChecks += 1
                            DispatchQueue.main.asyncAfter(deadline: .now() + 1.2) { self.snapshot(id, frame: frame, x: x, y: y, n: n) }; return
                        }
                    }
                    self.finish(.success(Shot(pixels: pixels, withoutBuildings: self.reference, width: width, height: height,
                                             left: Double(tl.x) * sx, top: Double(tl.y) * sy, right: Double(tr.x) * sx, bottom: Double(bl.y) * sy, north: north, south: south)))
                } catch { self.finish(.failure(error)) }
            }
        }
    }
    private func finish(_ result: Result<Shot, Error>) {
        guard let done = pending else { return }
        pending = nil; serial += 1; reference = nil; referenceProjection = nil; timer?.invalidate(); timer = nil
        if UIApplication.shared.applicationState == .active { map?.isShowsBuildings = false }
        map?.renderringDisabled = true; map?.isAllowDecreaseFrame = true
        #if DEBUG
        if ProcessInfo.processInfo.arguments.contains("--map-capture-check"), let m = map, let view = container { view.addSubview(m) }
        #endif
        done(result)
    }
    func cancel() { finish(.failure(MapProcessingError.cancelled)) }
}

#if DEBUG
/// Explicit developer launch only; captures SDK output without changing navigation or saved settings.
private enum MapCaptureCheck {
    private static var started = false
    private static var enabled: Bool { ProcessInfo.processInfo.arguments.contains("--map-capture-check") }
    private static let folder = FileManager.default.urls(for: .cachesDirectory, in: .userDomainMask)[0].appendingPathComponent("map-capture-check")
    static func record(image: UIImage?, map: MAMapView, tile: Int, buildings: Bool) {
        guard enabled else { return }
        try? FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
        let name = tile == 1200 ? "base" : (buildings ? "buildings-on" : "buildings-off")
        let info: [String: Any] = ["zoom": map.zoomLevel, "camera": map.cameraDegree, "bounds": NSCoder.string(for: map.bounds), "frame": NSCoder.string(for: map.frame), "imageWidth": image?.cgImage?.width ?? 0, "imageHeight": image?.cgImage?.height ?? 0]
        if let data = try? JSONSerialization.data(withJSONObject: info, options: [.prettyPrinted, .sortedKeys]) { try? data.write(to: folder.appendingPathComponent(name + ".json"), options: .atomic) }
        if let png = image?.pngData() { try? png.write(to: folder.appendingPathComponent(name + ".png"), options: .atomic) }
    }
    static func startIfRequested() {
        guard enabled, !started else { return }
        let args = ProcessInfo.processInfo.arguments
        guard let index = args.firstIndex(of: "--map-capture-check"), args.count >= index + 6,
              let lat = Double(args[index+1]), let lng = Double(args[index+2]), let id = Int(args[index+3]),
              let cx = Double(args[index+4]), let cy = Double(args[index+5]) else { return }
        started = true
        try? FileManager.default.removeItem(at: folder)
        DispatchQueue.main.asyncAfter(deadline: .now() + 2) {
            DispatchQueue.global(qos: .userInitiated).async {
                var messages = [String]()
                let result = Result { try AmapSdkRaster.build(MapGeo.Frame(refLat: lat, refLng: lng, id: id), cx: cx, cy: cy, includeBuildings: true, shouldCancel: { false }, onBuildingStatus: { messages.append($0) }).bytes }
                try? FileManager.default.createDirectory(at: folder, withIntermediateDirectories: true)
                switch result {
                case .success(let packet):
                    try? packet.write(to: folder.appendingPathComponent("map.bin"), options: .atomic)
                    messages.append("buildingFlag=\(RasterPacket.isBuilding(packet))")
                case .failure(let error): messages.append(error.localizedDescription)
                }
                try? messages.joined(separator: "\n").write(to: folder.appendingPathComponent("result.txt"), atomically: true, encoding: .utf8)
            }
        }
    }
}
#endif

struct AmapCaptureSurface: UIViewRepresentable {
    func makeUIView(context: Context) -> UIView {
        let view = UIView(frame: CGRect(x: 0, y: 0, width: 1, height: 1))
        view.isUserInteractionEnabled = false; view.accessibilityElementsHidden = true
        AmapCaptureHost.shared.attach(view); return view
    }
    func updateUIView(_ uiView: UIView, context: Context) {}
    static func dismantleUIView(_ uiView: UIView, coordinator: ()) { AmapCaptureHost.shared.detach(uiView) }
}
