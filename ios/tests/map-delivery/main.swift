import Foundation

func check(_ condition:@autoclosure ()->Bool,_ message:String){if !condition(){fatalError(message)}}
let size=1200
var gray=[UInt32](repeating:0xffeeeeee,count:size*size)
for y in 0..<size {for x in 570..<630 {gray[y*size+x]=0xffd0d0d0}}
for y in 100..<300 {gray[y*size+100]=0xff333333}
check(MapGeo.classifyPixel(0xffd0d0d0,248)==MapGeo.PX_ROAD,"mid-gray highway must not disappear")
check(MapGeo.classifyPixel(0xff333333,238)==MapGeo.PX_BLOCK,"dark building outlines must not become roads")
let pixels=MapGeo.classifyAmapBase(gray,w:size,h:size)
check(pixels[600*size+600]==MapGeo.PX_ROAD,"continuous gray highway survives cleanup")
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
try cache.store(packet,frame:frame,source:3,node:node,style:1)
do {try cache.store(empty,frame:frame,source:3,node:node,style:1);fatalError("blank cache overwrite accepted")}catch{}
check(cache.read(frame:frame,source:3,buildings:false,node:node,style:1)==packet,"valid cached map survives rejected blank update")
try? FileManager.default.removeItem(at:folder)
print("PASS gray-highway extraction, blank-frame rejection, preserved cache, 60/80/100/120 km/h handovers and building priority")
