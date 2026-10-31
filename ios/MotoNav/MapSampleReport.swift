import SwiftUI
import UIKit

/// Explicitly requested local samples only. No keys, cookies or automatic uploads.
final class MapSampleReport: ObservableObject {
    static let shared=MapSampleReport()
    @Published private(set) var status="在网纹出现的位置生成同一块地图的转换前后对照图。"
    @Published private(set) var files:[URL]=[]
    @Published private(set) var waiting=false
    private let lock=NSLock()
    private var request:(id:UUID,style:Int)?
    func begin() {
        guard [0,1,5].contains(Settings.shared.theme) else {fail("请先选择高德原版、罪恶都市或极简风格。");return}
        lock.lock();request=(UUID(),Settings.shared.theme);lock.unlock()
        files=[];waiting=true;status="正在重新取图，请保持 App 在前台…"
        MapFeeder.shared.requestMapSample()
    }
    func fail(_ message:String) {
        lock.lock();request=nil;lock.unlock()
        waiting=false;status=message
    }
    func failIfWaiting(_ message:String) {if waiting {fail(message)}}
    private func take(style:Int)->UUID? {
        lock.lock();defer{lock.unlock()}
        guard let value=request,value.style==style else {return nil}
        request=nil;return value.id
    }
    func capture(raw:[UInt32],indices:[UInt8],palette:[Int]?=nil,packet:Data,w:Int,h:Int,style:Int) {
        guard let id=take(style:style),raw.count==w*h,indices.count==raw.count else {return}
        let root=FileManager.default.urls(for:.cachesDirectory,in:.userDomainMask)[0]
            .appendingPathComponent("map-samples",isDirectory:true).appendingPathComponent(id.uuidString,isDirectory:true)
        do {
            try FileManager.default.createDirectory(at:root,withIntermediateDirectories:true)
            let source=root.appendingPathComponent("01-source.png"),converted=root.appendingPathComponent("02-converted.png")
            try Self.png(raw,w:w,h:h).write(to:source,options:.atomic)
            let preview:[UInt32]
            if let palette=palette {
                let rgb=palette.map{c -> UInt32 in
                    0xff000000 | UInt32((c>>11&31)*255/31)<<16 | UInt32((c>>5&63)*255/63)<<8 | UInt32((c&31)*255/31)
                }
                preview=indices.map{rgb[Int($0)]}
            } else {
                // Material colours are illustrative; the byte indices are preserved in map.bin.
                let rgb:[UInt32]=[0xfff4edf2,0xff30243e,0xffd6e8d1,0xff75d5ec,0xffc9c0cc,0xffaa9bb1,0xffc9c0cc,0xffaa9bb1]
                preview=indices.map{rgb[min(7,Int($0))]}
            }
            try Self.png(preview,w:w,h:h).write(to:converted,options:.atomic)
            let wire=root.appendingPathComponent("map.bin");try packet.write(to:wire,options:.atomic)
            let info:[String:Any]=["appVersion":APP_VERSION,"build":Bundle.main.object(forInfoDictionaryKey:"CFBundleVersion") ?? "",
                "style":style,"width":w,"height":h,"backgroundLum":MapGeo.backgroundLum(raw),
                "packetBytes":packet.count,"validPacket":RasterPacket.valid(packet),"buildingPacket":RasterPacket.isBuilding(packet),
                "note":"Source and converted base come from the same new capture. Preview material colours are illustrative. No personal Key, cookie or absolute GPS reference is included."]
            let metadata=root.appendingPathComponent("sample.json")
            try JSONSerialization.data(withJSONObject:info,options:[.prettyPrinted,.sortedKeys]).write(to:metadata,options:.atomic)
            DispatchQueue.main.async {
                self.files=[source,converted,metadata,wire];self.waiting=false
                self.status="对照图已生成。地图区域仅保存在本机，分享时由你选择接收位置。"
            }
        } catch {DispatchQueue.main.async{self.fail("生成对照图失败，请重试。")}}
    }
    private static func png(_ pixels:[UInt32],w:Int,h:Int)throws->Data {
        var data=pixels
        let result=data.withUnsafeMutableBytes{buffer -> Data? in
            guard let image=CGContext(data:buffer.baseAddress,width:w,height:h,bitsPerComponent:8,bytesPerRow:w*4,
                space:CGColorSpaceCreateDeviceRGB(),bitmapInfo:CGImageAlphaInfo.premultipliedFirst.rawValue|CGBitmapInfo.byteOrder32Little.rawValue)?.makeImage() else {return nil}
            return UIImage(cgImage:image).pngData()
        }
        guard let result=result else {throw MapProcessingError.invalid("无法生成地图图片")}
        return result
    }
}

struct MapSampleReportView:View {
    @ObservedObject private var report=MapSampleReport.shared
    @ObservedObject private var trace=MapTraceReport.shared
    @Environment(\.dismiss) private var dismiss
    @State private var sharing=false
    @State private var shareItems:[URL]=[]
    var body:some View {
        NavigationView {
            ScrollView {
                VStack(alignment:.leading,spacing:16) {
                    Text("地图显示问题").font(.title2.bold())
                    Text("高速底图衔接").font(.headline)
                    Text("模拟导航前点开始，底图空白后停止并导出。不记录地图画面、坐标或 Key，不会自动上传。")
                        .font(.subheadline).foregroundColor(AppTheme.sub)
                    if trace.recording {
                        Button("停止记录"){trace.finish()}
                    } else {Button("开始记录地图衔接"){trace.begin()}.disabled(trace.exporting)}
                    Text(trace.status).font(.footnote).foregroundColor(AppTheme.sub)
                    if !trace.files.isEmpty {Button("导出衔接记录"){shareItems=trace.files;sharing=true}}
                    Divider()
                    Text("地图颜色与纹理").font(.headline)
                    Text("请在网纹出现的地图区域生成对照图。它们包含地图画面，不含 Key；不会自动上传。")
                        .font(.subheadline).foregroundColor(AppTheme.sub)
                    Button(report.waiting ? "正在取图…":"生成新的对照图"){report.begin()}.disabled(report.waiting)
                    Text(report.status).font(.footnote).foregroundColor(AppTheme.sub)
                    ForEach(Array(report.files.prefix(2).enumerated()),id:\.offset){i,url in
                        if let image=UIImage(contentsOfFile:url.path) {
                            Text(i==0 ? "原始取图（转换前）":"转换后的基础底图（配色示意）").font(.headline)
                            Image(uiImage:image).resizable().scaledToFit().clipShape(RoundedRectangle(cornerRadius:12))
                        }
                    }
                    if !report.files.isEmpty {Button("导出对照图和资料"){shareItems=report.files;sharing=true}}
                }.padding(16)
            }.background(AppTheme.bg).foregroundColor(AppTheme.text)
                .navigationTitle("地图显示问题").navigationBarTitleDisplayMode(.inline)
                .toolbar{ToolbarItem(placement:.confirmationAction){Button("完成"){dismiss()}}}
        }.sheet(isPresented:$sharing){MapSampleShare(items:shareItems)}
    }
}
private struct MapSampleShare:UIViewControllerRepresentable {
    let items:[URL]
    func makeUIViewController(context:Context)->UIActivityViewController {UIActivityViewController(activityItems:items,applicationActivities:nil)}
    func updateUIViewController(_ controller:UIActivityViewController,context:Context){}
}
