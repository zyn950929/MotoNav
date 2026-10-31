import SwiftUI
import MapKit
import CoreLocation

/// 轨迹缩略 / 详情：用已有 Ride.fixes 画 MapKit 折线（无需高德 Key）。
/// `compactMarkers`：列表缩略用小圆点，避免 MKMarker 图钉占满 64pt 缩略图。
/// Keep the system map attribution visible in thumbnails and detail views.
struct RideTrackMapView: UIViewRepresentable {
    let fixes: [RideRecorder.Fix]
    var interactive = false
    var lineWidth: CGFloat = 4
    /// 列表缩略：小圆点；详情可略大。
    var compactMarkers = false

    func makeCoordinator() -> Coord { Coord(compactMarkers: compactMarkers) }

    func makeUIView(context: Context) -> MKMapView {
        let m = MKMapView(frame: .zero)
        m.isZoomEnabled = interactive
        m.isScrollEnabled = interactive
        m.isRotateEnabled = false
        m.isPitchEnabled = false
        m.showsCompass = false
        m.showsScale = false
        m.showsTraffic = false
        m.showsBuildings = false
        m.showsUserLocation = false
        m.pointOfInterestFilter = .excludingAll
        m.delegate = context.coordinator
        context.coordinator.lineWidth = lineWidth
        context.coordinator.compactMarkers = compactMarkers
        return m
    }

    func updateUIView(_ map: MKMapView, context: Context) {
        context.coordinator.lineWidth = lineWidth
        context.coordinator.compactMarkers = compactMarkers
        let stamp="\(fixes.count)|\(fixes.first?.time.timeIntervalSince1970 ?? 0)|\(fixes.last?.time.timeIntervalSince1970 ?? 0)"
        guard context.coordinator.stamp != stamp else{return}
        context.coordinator.stamp=stamp
        map.removeOverlays(map.overlays)
        map.removeAnnotations(map.annotations)
        guard fixes.count >= 2 else {
            if let first = fixes.first {
                map.setRegion(MKCoordinateRegion(
                    center: CLLocationCoordinate2D(latitude: first.lat, longitude: first.lng),
                    span: MKCoordinateSpan(latitudeDelta: 0.02, longitudeDelta: 0.02)), animated: false)
            }
            return
        }
        var coords = fixes.map { CLLocationCoordinate2D(latitude: $0.lat, longitude: $0.lng) }
        let poly = MKPolyline(coordinates: &coords, count: coords.count)
        map.addOverlay(poly)
        if let s = coords.first {
            let a = MKPointAnnotation(); a.coordinate = s; a.title = "起"; map.addAnnotation(a)
        }
        if let e = coords.last {
            let a = MKPointAnnotation(); a.coordinate = e; a.title = "终"; map.addAnnotation(a)
        }
        let pad: CGFloat = compactMarkers ? 6 : 24
        map.setVisibleMapRect(poly.boundingMapRect, edgePadding: UIEdgeInsets(top: pad, left: pad, bottom: pad + (compactMarkers ? 18 : 8), right: pad), animated: false)
    }

    final class Coord: NSObject, MKMapViewDelegate {
        var stamp=""
        var lineWidth: CGFloat = 4
        var compactMarkers = false
        init(compactMarkers: Bool) { self.compactMarkers = compactMarkers }

        func mapView(_ mapView: MKMapView, rendererFor overlay: MKOverlay) -> MKOverlayRenderer {
            if let p = overlay as? MKPolyline {
                let r = MKPolylineRenderer(polyline: p)
                r.strokeColor = UIColor(red: 0, green: 0.478, blue: 1, alpha: 1)
                r.lineWidth = lineWidth
                r.lineCap = .round
                r.lineJoin = .round
                return r
            }
            return MKOverlayRenderer(overlay: overlay)
        }
        func mapView(_ mapView: MKMapView, viewFor annotation: MKAnnotation) -> MKAnnotationView? {
            if annotation is MKUserLocation { return nil }
            let start = annotation.title == "起"
            let fill = start ? UIColor.systemGreen : UIColor.systemRed
            // 自定义小圆点：缩略约 7pt，详情约 11pt（远小于默认 MKMarker 图钉）
            let id = compactMarkers ? "ride-pt-compact" : "ride-pt-detail"
            let v = mapView.dequeueReusableAnnotationView(withIdentifier: id)
                ?? MKAnnotationView(annotation: annotation, reuseIdentifier: id)
            v.annotation = annotation
            let d: CGFloat = compactMarkers ? 7 : 11
            let renderer = UIGraphicsImageRenderer(size: CGSize(width: d + 2, height: d + 2))
            v.image = renderer.image { ctx in
                let c = ctx.cgContext
                c.setFillColor(fill.cgColor)
                c.fillEllipse(in: CGRect(x: 1, y: 1, width: d, height: d))
                c.setStrokeColor(UIColor.white.withAlphaComponent(0.92).cgColor)
                c.setLineWidth(compactMarkers ? 1 : 1.5)
                c.strokeEllipse(in: CGRect(x: 1, y: 1, width: d, height: d))
            }
            v.centerOffset = .zero
            v.canShowCallout = false
            v.displayPriority = .required
            return v
        }
    }
}

struct RideListRow: View {
    let ride: RideRecorder.Ride
    var body: some View {
        HStack(spacing: 12) {
            RideTrackMapView(fixes: ride.fixes, interactive: false, lineWidth: 2.5, compactMarkers: true)
                .frame(width: 64, height: 64)
                .clipShape(RoundedRectangle(cornerRadius: 10, style: .continuous))
            VStack(alignment: .leading, spacing: 3) {
                Text(rideTitle(ride))
                    .font(.body.weight(.semibold))
                    .foregroundColor(AppTheme.text)
                    .lineLimit(1)
                Text(ride.started, style: .date)
                    .font(.caption)
                    .foregroundColor(AppTheme.sub)
                Text("\(NaviBridge.distText(Int(ride.meters))) · \(durationText(ride))")
                    .font(.caption)
                    .foregroundColor(AppTheme.sub)
            }
            Spacer(minLength: 0)
            Image(systemName: "chevron.right")
                .font(.caption.weight(.semibold))
                .foregroundColor(AppTheme.sub.opacity(0.7))
        }
        .padding(.vertical, 4)
    }
}

struct RideFeatureCard: View {
    let ride: RideRecorder.Ride
    var body: some View {
        VStack(alignment: .leading, spacing: 10) {
            RideTrackMapView(fixes: ride.fixes, interactive: false, lineWidth: 3.5, compactMarkers: true)
                .frame(height: 160)
                .clipShape(RoundedRectangle(cornerRadius: 14, style: .continuous))
            HStack {
                Text("导航记录").font(.body.weight(.semibold)).foregroundColor(AppTheme.text)
                Spacer()
                Image(systemName: "chevron.right").font(.caption.weight(.semibold)).foregroundColor(AppTheme.sub)
            }
            Text("\(timeLabel(ride.started)) 出发 · \(NaviBridge.distText(Int(ride.meters))) · \(durationText(ride))")
                .font(.caption)
                .foregroundColor(AppTheme.sub)
        }
        .padding(14)
        .background(RoundedRectangle(cornerRadius: 18).fill(AppTheme.card))
    }
}

struct RideDetailView: View {
    let ride: RideRecorder.Ride
    @ObservedObject var recorder = RideRecorder.shared
    @State private var exportURL: URL?
    @State private var confirmDelete=false
    @Environment(\.dismiss) private var dismiss

    var body: some View {
        ScrollView {
            VStack(alignment: .leading, spacing: 14) {
                RideTrackMapView(fixes: ride.fixes, interactive: true, lineWidth: 5, compactMarkers: false)
                    .frame(height: 280)
                    .clipShape(RoundedRectangle(cornerRadius: 18, style: .continuous))

                CardView {
                    HStack {
                        metric("距离", NaviBridge.distText(Int(ride.meters)))
                        metric("耗时", durationText(ride))
                        metric("平均", avgSpeedText(ride))
                        metric("最高", maxSpeedText(ride))
                    }
                    Divider().opacity(0.35)
                    row("出发", timeLabel(ride.started))
                    row("模式", ride.mode)
                    row("轨迹点", "\(ride.fixes.count)")
                }

                HStack(spacing: 10) {
                    GlassButton(title: "导出 GPX", icon: "square.and.arrow.up", style: .tinted, tint: AppTheme.accent) {
                        do { exportURL = try recorder.export(ride) }
                        catch { recorder.error = error.localizedDescription }
                    }
                    GlassButton(title: "删除", icon: "trash", style: .clear, tint: AppTheme.red) {
                        confirmDelete=true
                    }
                }
                if !recorder.error.isEmpty {
                    Text(recorder.error).font(.caption).foregroundColor(AppTheme.red)
                }
            }
            .padding(16)
        }
        .background(AppTheme.bg.ignoresSafeArea())
        .navigationBarHidden(false)
        .navigationTitle("导航详情")
        .navigationBarTitleDisplayMode(.inline)
        .background(InteractivePopEnabler())
        .alert("删除这条导航记录？",isPresented:$confirmDelete){Button("删除",role:.destructive){recorder.remove(ride);dismiss()};Button("取消",role:.cancel){}}
        .sheet(isPresented: Binding(get: { exportURL != nil }, set: { if !$0 { exportURL = nil } })) {
            if let url = exportURL { ExportSheet(url: url) }
        }
    }

    private func metric(_ title: String, _ value: String) -> some View {
        VStack(spacing: 4) {
            Text(title).font(.caption2).foregroundColor(AppTheme.sub)
            Text(value).font(.subheadline.weight(.bold).monospacedDigit()).foregroundColor(AppTheme.text)
                .lineLimit(1).minimumScaleFactor(0.7)
        }
        .frame(maxWidth: .infinity)
    }

    private func row(_ k: String, _ v: String) -> some View {
        HStack {
            Text(k).foregroundColor(AppTheme.sub)
            Spacer()
            Text(v).foregroundColor(AppTheme.text)
        }
        .font(.subheadline)
    }
}

// MARK: - Ride formatting helpers

func rideTitle(_ ride: RideRecorder.Ride) -> String {
    ride.mode.isEmpty ? "导航记录" : ride.mode
}

func durationText(_ ride: RideRecorder.Ride) -> String {
    let end = ride.ended ?? ride.fixes.last?.time ?? ride.started
    let s = max(0, Int(end.timeIntervalSince(ride.started)))
    let h = s / 3600, m = (s % 3600) / 60, sec = s % 60
    if h > 0 { return String(format: "%d:%02d:%02d", h, m, sec) }
    return String(format: "%02d:%02d", m, sec)
}

func timeLabel(_ date: Date) -> String {
    let cal = Calendar.current
    let f = DateFormatter()
    if cal.isDateInToday(date) {
        f.dateFormat = "今天 HH:mm"
    } else if cal.isDateInYesterday(date) {
        f.dateFormat = "昨天 HH:mm"
    } else {
        f.dateFormat = "M月d日 HH:mm"
    }
    return f.string(from: date)
}

func avgSpeedText(_ ride: RideRecorder.Ride) -> String {
    let end = ride.ended ?? ride.fixes.last?.time ?? ride.started
    let hours = max(end.timeIntervalSince(ride.started) / 3600.0, 1.0 / 3600.0)
    let kmh = (ride.meters / 1000.0) / hours
    return String(format: "%.0f km/h", kmh)
}

func maxSpeedText(_ ride: RideRecorder.Ride) -> String {
    let maxMps = ride.fixes.map(\.speed).max() ?? 0
    return String(format: "%.0f km/h", max(0, maxMps) * 3.6)
}

/// 确保系统侧滑返回手势可用（自定义根导航栏隐藏时偶发被关掉）。
struct InteractivePopEnabler: UIViewControllerRepresentable {
    func makeUIViewController(context: Context) -> UIViewController {
        let c = Controller()
        return c
    }
    func updateUIViewController(_ uiViewController: UIViewController, context: Context) {
        (uiViewController as? Controller)?.enable()
    }
    final class Controller: UIViewController {
        override func viewDidAppear(_ animated: Bool) {
            super.viewDidAppear(animated)
            enable()
        }
        func enable() {
            guard let nav = navigationController else { return }
            nav.interactivePopGestureRecognizer?.isEnabled = true
            nav.interactivePopGestureRecognizer?.delegate = nil
        }
    }
}
