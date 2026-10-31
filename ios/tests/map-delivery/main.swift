import Foundation

func check(_ condition:@autoclosure ()->Bool,_ message:String){if !condition(){fatalError(message)}}
let size=1200
var gray=[UInt32](repeating:0xffeeeeee,count:size*size)
for y in 0..<size {for x in 570..<630 {gray[y*size+x]=0xffd0d0d0}}
for y in 100..<300 {gray[y*size+100]=0xff333333}
check(MapGeo.classifyPixel(0xffd0d0d0,248)==MapGeo.PX_BLOCK,"gray alone cannot distinguish roofs from roads")
check(MapGeo.classifyPixel(0xff333333,238)==MapGeo.PX_BLOCK,"dark building outlines must not become roads")
let pixels=MapGeo.classifyAmapBase(gray,w:size,h:size)
check(pixels[600*size+600]==MapGeo.PX_ROAD,"continuous gray highway survives cleanup")
// Gray apartments previously became repeated black road shapes, even with buildings disabled.
var city=gray
for y in stride(from:80,to:1100,by:55) {for x in stride(from:80,to:500,by:55) {
    for dy in 0..<25 {for dx in 0..<30 {city[(y+dy)*size+x+dx]=0xffc0c0c0}}
}}
let cityPixels=MapGeo.classifyAmapBase(city,w:size,h:size)
check(cityPixels[90*size+90]==MapGeo.PX_BLOCK,"gray apartment interiors cannot become black roads")
check(cityPixels[600*size+600]==MapGeo.PX_ROAD,"gray highway beside apartments remains visible")
var mesh=[UInt32](repeating:0xffeeeeee,count:size*size)
for y in 100..<1100 {for x in 100..<1100 where (x%40<5 || y%40<5) {mesh[y*size+x]=0xffc0c0c0}}
let meshPixels=MapGeo.classifyAmapBase(mesh,w:size,h:size)
check(meshPixels[400*size+400]==MapGeo.PX_BLOCK,"two-dimensional gray texture cannot become a black mesh")
var diagonal=[UInt32](repeating:0xffeeeeee,count:size*size)
for y in 0..<size {for x in max(0,y-12)...min(size-1,y+12) {diagonal[y*size+x]=0xffd0d0d0}}
check(MapGeo.classifyAmapBase(diagonal,w:size,h:size)[600*size+600]==MapGeo.PX_ROAD,"diagonal gray highway remains visible")
let packet=MapGeo.encodeRaster(frameId:1,x0:-1200,y0:1200,w:size,h:size,mmPerPx:2000,idx:pixels)
let empty=MapGeo.encodeRaster(frameId:1,x0:-1200,y0:1200,w:size,h:size,mmPerPx:2000,idx:[UInt8](repeating:0,count:size*size))
check(RasterPacket.valid(empty),"empty but structurally valid packet reproduces the old bug")
check(!RasterPacket.hasMapContent(empty),"blank packet cannot replace a usable map")
check(RasterPacket.hasMapContent(packet),"gray highway packet remains eligible for delivery")
check(!RasterPacket.hasMapContent(Data(packet.dropLast())),"truncated packet rejected")
let plan=try RouteMapPlan([(0.0,0.0),(0.0,18000.0)])
for speed in [60,80,100,120] {
    for meters in stride(from:0.0,through:18000.0,by:Double(speed)/3.6) {
        let index=plan.displayTarget(progress:meters,x:0,y:meters,kmh:speed)
        let node=plan.nodes[index]
        check(MapCoveragePolicy.covers(x:0,y:meters,cx:node.x,cy:node.y,kmh:speed),"cached highway handover covers viewport")
    }
    check(!MapCoveragePolicy.mayCaptureBuildings(kmh:speed),"high-speed building passes yield to road delivery")
}
check(MapCoveragePolicy.mayCaptureBuildings(kmh:20),"building preparation resumes at low speed")
let folder=FileManager.default.temporaryDirectory.appendingPathComponent(UUID().uuidString)
let cache=RouteMapCache(root:folder)
let frame=MapGeo.Frame(refLat:30,refLng:104,id:1)
let node=plan.nodes[0]
let oldCache=folder.appendingPathComponent("styles-v1-1/v1-3-base/30.0_104.0/0_0.bin")
try FileManager.default.createDirectory(at:oldCache.deletingLastPathComponent(),withIntermediateDirectories:true)
try packet.write(to:oldCache)
check(cache.read(frame:frame,source:3,buildings:false,node:node,style:1)==nil,"old over-broad gray classification cache must not be reused")
try cache.store(packet,frame:frame,source:3,node:node,style:1)
check(!FileManager.default.fileExists(atPath:oldCache.path),"obsolete gray-map cache must release quota for its replacement")
do {try cache.store(empty,frame:frame,source:3,node:node,style:1);fatalError("blank cache overwrite accepted")}catch{}
check(cache.read(frame:frame,source:3,buildings:false,node:node,style:1)==packet,"valid cached map survives rejected blank update")
try? FileManager.default.removeItem(at:folder)
print("PASS gray and diagonal highways, apartment/mesh rejection, blank-frame protection, cache and 60/80/100/120 km/h handovers")
