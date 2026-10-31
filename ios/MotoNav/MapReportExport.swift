import Foundation

/// The sheet and its files have one immutable identity, including the first presentation.
struct MapExportRequest: Identifiable {
    enum Destination { case share, save }
    let id = UUID()
    let files: [URL]
    let destination: Destination
    init(files: [URL], destination: Destination) throws {
        guard !files.isEmpty else { throw MapReportExportError.unavailable }
        for file in files {
            guard file.isFileURL, FileManager.default.isReadableFile(atPath: file.path),
                  let values = try? file.resourceValues(forKeys: [.isRegularFileKey, .fileSizeKey]),
                  values.isRegularFile == true, (values.fileSize ?? 0) > 0 else {
                throw MapReportExportError.unavailable
            }
        }
        self.files = files; self.destination = destination
    }
}

enum MapReportExportError: LocalizedError {
    case unavailable
    var errorDescription: String? { "记录文件暂不可用，请停止记录并等待保存完成后重试。" }
}

enum MapReportFiles {
    /// Recovers Build7 cache records as well as new persistent records after an update/relaunch.
    static func latestTiming(in folders: [URL]) -> URL? {
        let manager = FileManager.default
        let files = folders.flatMap { folder in
            (try? manager.contentsOfDirectory(at: folder, includingPropertiesForKeys: [.contentModificationDateKey, .fileSizeKey, .isRegularFileKey])) ?? []
        }.filter { $0.lastPathComponent.hasPrefix("map-timing-") && $0.pathExtension == "json" }
        let sorted = files.sorted {
            let a = (try? $0.resourceValues(forKeys: [.contentModificationDateKey]).contentModificationDate) ?? .distantPast
            let b = (try? $1.resourceValues(forKeys: [.contentModificationDateKey]).contentModificationDate) ?? .distantPast
            return a > b
        }
        for file in sorted {
            guard let values = try? file.resourceValues(forKeys: [.isRegularFileKey, .fileSizeKey]),
                  values.isRegularFile == true, let size = values.fileSize, size > 0, size < 4*1024*1024,
                  let data = try? Data(contentsOf: file),
                  let document = (try? JSONSerialization.jsonObject(with: data)) as? [String: Any],
                  document["events"] is [[String: Any]] else { continue }
            return file
        }
        return nil
    }
}

#if canImport(UIKit) && canImport(SwiftUI)
import UIKit
import SwiftUI

struct MapReportExportSheet: UIViewControllerRepresentable {
    let request: MapExportRequest
    let completion: () -> Void
    func makeCoordinator() -> Coordinator { Coordinator(completion: completion) }
    func makeUIViewController(context: Context) -> UIViewController {
        switch request.destination {
        case .share:
            let controller = UIActivityViewController(activityItems: request.files, applicationActivities: nil)
            controller.completionWithItemsHandler = { _, _, _, _ in completion() }
            if let popover = controller.popoverPresentationController {
                popover.sourceView = controller.view
                popover.sourceRect = CGRect(x: controller.view.bounds.midX, y: controller.view.bounds.midY, width: 1, height: 1)
            }
            return controller
        case .save:
            let controller = UIDocumentPickerViewController(forExporting: request.files, asCopy: true)
            controller.delegate = context.coordinator
            return controller
        }
    }
    func updateUIViewController(_ controller: UIViewController, context: Context) {}
    final class Coordinator: NSObject, UIDocumentPickerDelegate {
        let completion: () -> Void
        init(completion: @escaping () -> Void) { self.completion = completion }
        func documentPickerWasCancelled(_ controller: UIDocumentPickerViewController) { completion() }
        func documentPicker(_ controller: UIDocumentPickerViewController, didPickDocumentsAt urls: [URL]) { completion() }
    }
}
#endif
