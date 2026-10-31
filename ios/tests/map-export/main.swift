import Foundation
func check(_ value:@autoclosure ()->Bool,_ message:String){precondition(value(),message)}
let root=FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString,isDirectory:true)
try FileManager.default.createDirectory(at:root,withIntermediateDirectories:true)
defer {try? FileManager.default.removeItem(at:root)}
let cache=root.appendingPathComponent("legacy",isDirectory:true),saved=root.appendingPathComponent("saved",isDirectory:true)
try FileManager.default.createDirectory(at:cache,withIntermediateDirectories:true)
try FileManager.default.createDirectory(at:saved,withIntermediateDirectories:true)
let old=cache.appendingPathComponent("map-timing-build7.json")
let json=Data("{\"build\":\"7\",\"events\":[{\"event\":\"session-stop\",\"time\":1}]}".utf8)
try json.write(to:old)
let request=try MapExportRequest(files:[old],destination:.share)
check(request.files==[old],"first presentation carries the actual immutable file URL")
let next=saved.appendingPathComponent("map-timing-build8.json")
try json.write(to:next)
try FileManager.default.setAttributes([.modificationDate:Date(timeIntervalSince1970:100)],ofItemAtPath:old.path)
try FileManager.default.setAttributes([.modificationDate:Date(timeIntervalSince1970:200)],ofItemAtPath:next.path)
check(request.files==[old],"a new recording cannot replace the file in the already presented sheet")
let save=try MapExportRequest(files:[next],destination:.save)
check(save.id != request.id && save.files==[next],"save is a distinct immutable presentation")
check(MapReportFiles.latestTiming(in:[cache,saved])==next,"newest saved record restores after relaunch")
let broken=saved.appendingPathComponent("map-timing-broken.json")
try Data("not JSON".utf8).write(to:broken)
try FileManager.default.setAttributes([.modificationDate:Date(timeIntervalSince1970:300)],ofItemAtPath:broken.path)
check(MapReportFiles.latestTiming(in:[cache,saved])==next,"corrupt newer files do not hide a valid record")
try FileManager.default.removeItem(at:next)
check(MapReportFiles.latestTiming(in:[cache,saved])==old,"Build7 cache records survive a source update/relaunch")
for invalid in [[],[root],[root.appendingPathComponent("missing.json")],[URL(string:"https://example.com/report.json")!]] as [[URL]] {
    do {_ = try MapExportRequest(files:invalid,destination:.share);fatalError("invalid file opened a blank export sheet")}catch{}
}
let empty=root.appendingPathComponent("empty.json");try Data().write(to:empty)
do {_ = try MapExportRequest(files:[empty],destination:.save);fatalError("empty file accepted")}catch{}
print("PASS immutable first export, share/save identity, record recovery across update, corrupt/empty/missing file handling")
