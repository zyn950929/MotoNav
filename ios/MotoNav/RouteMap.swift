import SwiftUI
import CoreLocation
import ObjectiveC

/// 版本：跟随 App 版本（见 Config.swift，iOS v0.3 新增）
///
/// 选路线时的小地图预览（高德地图，夜间样式）：
/// 选中的路线粉色粗线、其他路线灰色、导入的高德分享原路线橙色虚线，自动缩放到能看全所有路线。
/// iOS v0.4：目的地红色大头针、途经点紫色大头针。iOS v0.5.2：地图跟着夜间配色切换白天 / 夜间样式。
/// 放在可以上下滑的主界面里，所以地图本身不能拖动，只用来看。
struct RouteMapView: UIViewRepresentable {
    let routes: [NaviBridge.RouteOption]
    let selected: Int
    let ghost: [CLLocationCoordinate2D]
    var dest: CLLocationCoordinate2D? = nil
    var vias: [CLLocationCoordinate2D] = []
    var night = true                                // iOS v0.5.2 跟着夜间配色：夜间用深色地图，白天用浅色地图

    func makeCoordinator() -> RouteMapDelegate { RouteMapDelegate() }

    func makeUIView(context: Context) -> MAMapView {
        let m = MAMapView(frame: .zero)
        m.mapType = night ? .standardNight : .standard
        m.showsUserLocation = true
        m.isZoomEnabled = false
        m.isScrollEnabled = false
        m.isRotateEnabled = false
        m.isRotateCameraEnabled = false
        m.showsCompass = false
        m.showsScale = false
        m.delegate = context.coordinator.asDelegate
        return m
    }

    func updateUIView(_ m: MAMapView, context: Context) {
        let type: MAMapType = night ? .standardNight : .standard
        if m.mapType != type { m.mapType = type }
        // 内容没变就不重画
        let key = routes.map { "\($0.id):\($0.coords.count)" }.joined(separator: ",") + "|\(selected)|\(ghost.count)" +
            "|\(dest.map { "\($0.latitude),\($0.longitude)" } ?? "")|" + vias.map { "\($0.latitude),\($0.longitude)" }.joined(separator: ";")
        if context.coordinator.key == key { return }
        context.coordinator.key = key
        if let old = m.overlays { m.removeOverlays(old) }
        let oldPins = (m.annotations ?? []).compactMap { $0 as? MAPointAnnotation }
        if !oldPins.isEmpty { m.removeAnnotations(oldPins) }
        var pins: [MAPointAnnotation] = []
        for (i, v) in vias.enumerated() {
            let a = MAPointAnnotation(); a.coordinate = v; a.title = "途经 \(i + 1)"; pins.append(a)
        }
        if let d = dest { let a = MAPointAnnotation(); a.coordinate = d; a.title = "目的地"; pins.append(a) }
        if !pins.isEmpty { m.addAnnotations(pins) }
        var lines: [MAPolyline] = []
        func add(_ coords: [CLLocationCoordinate2D], _ kind: String) {
            if coords.count < 2 { return }
            var c = coords
            if let l = MAPolyline(coordinates: &c, count: UInt(c.count)) { l.title = kind; lines.append(l) }
        }
        add(ghost, "ghost")
        for r in routes where r.id != selected { add(r.coords, "alt") }
        if let s = routes.first(where: { $0.id == selected }) { add(s.coords, "sel") }    // 选中的最后画（在最上面）
        if lines.isEmpty { return }
        m.addOverlays(lines)
        m.showOverlays(lines, edgePadding: UIEdgeInsets(top: 28, left: 28, bottom: 28, right: 28), animated: false)
    }
}

/// 地图回调：决定每条线画成什么样。和 NaviBridge 一样在运行时登记协议（见 CLAUDE.md 高德坑第 1 条）
final class RouteMapDelegate: NSObject {
    var key = ""

    var asDelegate: MAMapViewDelegate? {
        class_addProtocol(RouteMapDelegate.self, MAMapViewDelegate.self)
        return (self as AnyObject) as? MAMapViewDelegate
    }

    /// 大头针：目的地红色、途经点紫色；自己的位置用地图默认的蓝点
    @objc(mapView:viewForAnnotation:)
    func annotationView(_ m: MAMapView, annotation: MAAnnotation) -> MAAnnotationView? {
        guard let a = annotation as? MAPointAnnotation else { return nil }
        let id = "pin"
        let v = (m.dequeueReusableAnnotationView(withIdentifier: id) as? MAPinAnnotationView) ?? MAPinAnnotationView(annotation: a, reuseIdentifier: id)
        v?.annotation = a
        v?.pinColor = a.title == "目的地" ? .red : .purple
        v?.canShowCallout = false
        return v
    }

    @objc(mapView:rendererForOverlay:)
    func renderer(_ m: MAMapView, overlay: MAOverlay) -> MAOverlayRenderer? {
        guard let pl = overlay as? MAPolyline, let r = MAPolylineRenderer(polyline: pl) else { return nil }
        r.lineJoinType = kMALineJoinRound
        r.lineCapType = kMALineCapRound
        switch pl.title {
        case "sel":
            r.strokeColor = UIColor(red: 1, green: 0.31, blue: 0.71, alpha: 1)
            r.lineWidth = 8
        case "ghost":
            r.strokeColor = UIColor(red: 1, green: 0.62, blue: 0.15, alpha: 0.95)
            r.lineWidth = 4
            r.lineDashType = kMALineDashTypeSquare
        default:
            r.strokeColor = UIColor(white: 0.6, alpha: 0.7)
            r.lineWidth = 6
        }
        return r
    }
}
