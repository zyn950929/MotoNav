import SwiftUI

@main
struct ExportProbeApp: App {
    var body: some Scene { WindowGroup { ProbeRoot() } }
}

struct ProbeRoot: View {
    @State private var showReport = false
    var body: some View {
        Text("MotoNav export presentation check")
            .sheet(isPresented: $showReport) { ProbeReport() }
            .onAppear { DispatchQueue.main.asyncAfter(deadline: .now()+0.5) { showReport=true } }
    }
}

/// Matches the app's nested troubleshooting sheet and first presentation.
struct ProbeReport: View {
    @State private var request: MapExportRequest?
    @State private var launched = false
    @State private var status = "Preparing the saved timing file"
    var body: some View {
        NavigationView {
            VStack(spacing:20) {
                Text("Map report export").font(.title)
                Text(status)
            }.navigationTitle("MotoNav diagnostics")
        }.sheet(item:$request) { value in
            MapReportExportSheet(request:value) { request=nil }
        }.onAppear {
            guard !launched else {return};launched=true
            do {
                let folder=FileManager.default.urls(for:.documentDirectory,in:.userDomainMask)[0]
                let url=folder.appendingPathComponent("map-timing-probe.json")
                try Data("{\"build\":\"8\",\"events\":[{\"event\":\"session-stop\",\"time\":1}]}".utf8).write(to:url)
                let destination:MapExportRequest.Destination=ProcessInfo.processInfo.arguments.contains("--save") ? .save:.share
                let value=try MapExportRequest(files:[url],destination:destination)
                status="Saved timing JSON is ready"
                DispatchQueue.main.asyncAfter(deadline:.now()+1) { request=value }
            } catch { status="ERROR: "+error.localizedDescription }
        }
    }
}
