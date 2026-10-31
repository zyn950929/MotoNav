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
// Real diagnostic colours, without geography or a user's map image.
// The measured land mode is 242; the second land fill is about 248.
// Their slight interpolation variation must never acquire the road material.
let paleLand: [UInt32] = [0xfff8f9fb, 0xfff9f9fb, 0xfff8f7fc, 0xfff9f8fd,
                         0xfff7f8fa, 0xfff8faf9, 0xffedf4fa]
for bg in [238, 242, 248, 252] {
    for color in paleLand { check(MapGeo.classifyPixel(color,bg)==MapGeo.PX_BLOCK,"pale land cannot become a white road") }
    for color:UInt32 in [0xffffffff,0xfffeffff] {check(MapGeo.classifyPixel(color,bg)==MapGeo.PX_ROAD,"Android near-white pavement threshold stays stable")}
    check(MapGeo.classifyPixel(0xffffdf70,bg)==MapGeo.PX_ROAD,"yellow main road remains")
    check(MapGeo.classifyPixel(0xff80d8f5,bg)==MapGeo.PX_WATER,"blue river remains")
    check(MapGeo.classifyPixel(0xffa0d890,bg)==MapGeo.PX_PARK,"park remains")
}
var pale=[UInt32](repeating:0xffedf4fa,count:size*size)
for y in 150..<1050 {for x in 150..<1050 {
    // Periodic variation represents resampling around a secondary land fill,
    // not the earlier unrelated dark-gray roof regression.
    pale[y*size+x] = paleLand[(x%7+y%11)%6]
}}
for y in 0..<size {for x in 580..<600 {pale[y*size+x]=0xffffffff}}
check(MapGeo.backgroundLum(pale)==242,"reproduce measured 242 background mode")
let palePixels=MapGeo.classifyAmapBase(pale,w:size,h:size)
for y in 170..<1030 {for x in 170..<1030 where x<575 || x>605 {
    check(palePixels[y*size+x]==MapGeo.PX_BLOCK,"no mesh anywhere inside the secondary land fill after cleanup and dilation")
}}
check(palePixels[600*size+590]==MapGeo.PX_ROAD,"continuous actual white road remains beside pale land")
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
let oldCache=folder.appendingPathComponent("styles-v2-1/v1-3-base/30.0_104.0/0_0.bin")
try FileManager.default.createDirectory(at:oldCache.deletingLastPathComponent(),withIntermediateDirectories:true)
try packet.write(to:oldCache)
check(cache.read(frame:frame,source:3,buildings:false,node:node,style:1)==nil,"old over-broad gray classification cache must not be reused")
try cache.store(packet,frame:frame,source:3,node:node,style:1)
check(!FileManager.default.fileExists(atPath:oldCache.path),"obsolete gray-map cache must release quota for its replacement")
do {try cache.store(empty,frame:frame,source:3,node:node,style:1);fatalError("blank cache overwrite accepted")}catch{}
check(cache.read(frame:frame,source:3,buildings:false,node:node,style:1)==packet,"valid cached map survives rejected blank update")
try? FileManager.default.removeItem(at:folder)
print("PASS real-sample pale-land colours, resampling-mesh rejection, near-white roads, gray highways, old cache rejection and 60/80/100/120 km/h handovers")


// The earlier coverage test assumed maps appeared instantly. Include actual capture
// and wire delay, and hold the original displayed map until the wire finishes.
func simulateHandoffs(speed:Int,captureDelay:Double,wireDelay:Double,early:Bool)->Int {
    let plan=try! RouteMapPlan([(0,0),(0,18000)])
    var buffer=MapHandoffBuffer(), displayed=plan.nodes[0], sent=displayed
    var capture:(node:RouteMapPlan.Node,at:Double)?
    var wire:(node:RouteMapPlan.Node,at:Double)?
    var blanks=0
    for tick in 0...Int(15000/(Double(speed)/3.6)*10) {
        let now=Double(tick)/10,meters=now*Double(speed)/3.6
        if let job=wire,now>=job.at {displayed=job.node;wire=nil}
        if let job=capture,now>=job.at {
            buffer.stage(.init(bytes:Data([1]),cx:job.node.x,cy:job.node.y,sat:false,type:3,info:"",source:3,style:1));capture=nil
        }
        if let p=buffer.take(x:0,y:meters,kmh:speed,style:1,transferPending:wire != nil) {
            sent = .init(x:p.cx,y:p.cy,distance:0)
            wire=(sent,now+wireDelay)
        }
        let displayIndex=plan.displayTarget(progress:meters,x:0,y:meters,kmh:speed)
        let refresh=MapCoveragePolicy.needsRefresh(x:0,y:meters,cx:sent.x,cy:sent.y,bearing:0,kmh:speed)
        let index=early && refresh ? plan.preparationTarget(progress:meters,kmh:speed) : displayIndex
        let next=plan.nodes[index]
        if capture==nil,buffer.pending==nil,abs(next.y-sent.y)>1 {
            capture=(next,now+captureDelay)
        }
        if !MapCoveragePolicy.covers(x:0,y:meters,cx:displayed.x,cy:displayed.y,kmh:max(120,speed)) {blanks+=1}
    }
    return blanks
}
for speed in [60,80,100,120] {
    for latency in [(4.0,1.0),(8.0,3.0)] {
        check(simulateHandoffs(speed:speed,captureDelay:latency.0,wireDelay:latency.1,early:true)==0,"timed capture + BLE handoff covers the display")
    }
}
check(simulateHandoffs(speed:120,captureDelay:8,wireDelay:3,early:false)>0,"reproduce old late-capture blank at highway speed")
var staged=MapHandoffBuffer()
staged.stage(.init(bytes:Data([9]),cx:0,cy:600,sat:false,type:3,info:"",source:3,style:1))
check(staged.take(x:0,y:0,kmh:120,style:1,transferPending:false)==nil,"early map must wait until its viewport is usable")
check(staged.pending != nil,"early result must not be discarded")
check(staged.take(x:0,y:300,kmh:120,style:1,transferPending:true)==nil,"new map cannot replace an unfinished transfer")
check(staged.take(x:0,y:300,kmh:120,style:1,transferPending:false)?.bytes==Data([9]),"staged map sends after current transfer finishes")
staged.stage(.init(bytes:Data([9]),cx:0,cy:600,sat:false,type:3,info:"",source:3,style:1))
check(staged.take(x:0,y:300,kmh:120,style:0,transferPending:false)==nil && staged.pending==nil,"style change invalidates prepared map")
var base=[Bool](repeating:false,count:100),complete=base,available=[Bool](repeating:true,count:100)
for i in 0...3 {base[i]=true}
let roofs=RouteMapPreparationPolicy.next(base:base,complete:complete,display:0,target:1,buildings:true,kmh:120,available:available)
check(roofs?.index==1 && roofs?.buildings==true,"nearby future buildings prepare before 96 distant base blocks, including highway speed")
base[2]=false
let urgent=RouteMapPreparationPolicy.next(base:base,complete:complete,display:0,target:1,buildings:true,kmh:120,available:available)
check(urgent?.index==2 && urgent?.buildings==false,"missing upcoming roads always precede building passes")
available[2]=false
check(RouteMapPreparationPolicy.next(base:base,complete:complete,display:0,target:1,buildings:true,kmh:120,available:available)?.buildings==false,"failed roof/base retry must not block other road preparation")
base[2]=true
available[1]=false
check(RouteMapPreparationPolicy.next(base:base,complete:complete,display:0,target:1,buildings:true,kmh:120,available:available)?.index != 1,"roof cooldown is local rather than stopping all downloads")
print("PASS delayed 60/80/100/120 km/h handoffs; old blank reproduced; staged-map ATT protection; near-route building priority")
