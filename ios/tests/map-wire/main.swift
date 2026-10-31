import Foundation
func check(_ c:@autoclosure ()->Bool,_ m:String){precondition(c(),m)}
// Production barrier makes its response choice before dequeueing and respects
// no-response credits, even while the with-response path remains available.
var barrier=BleBulkBarrier()
check(barrier.requiresResponse(seq:0,parts:30,retry:false),"first fragment validates ATT")
check(!barrier.requiresResponse(seq:1,parts:30,retry:false),"middle fragments are batched")
check(!BleBulkBarrier.canStart(response:false,canWithout:false),"backpressure blocks a middle write")
check(BleBulkBarrier.canStart(response:true,canWithout:false),"a scheduled barrier may use the ATT slot")
var q=BleBulkWriteQueue();q.replace(type:3,id:1,packets:[Data([1]),Data([2])])
let before=q.queuedCount
check(q.peek?.data==Data([1]) && q.queuedCount==before,"peek does not consume the blocked fragment")
for i in 1...4 {check(!barrier.needsResponse(seq:i,parts:30,retry:false,commandReady:true),"bounded command batch")}
check(barrier.requiresResponse(seq:5,parts:30,retry:false),"validate after four commands")
barrier.reset()
q.replace(type:2,id:1,packets:[Data([9])],prioritize:true)
check(q.next()?.type==2,"navigation route precedes pending map fragments without replacing the map")
_ = q.complete(success:true)
check(q.next()?.type==3,"map resumes after the independent route")
check(BleOutboundPriority.choose(commandReady:true,bulkReady:true,bulkPending:true,imageReady:true,favorBulk:true) == .bulk,"repeated position commands leave a bulk opportunity")
var receipt=BleMapReceipt();receipt.begin(id:4,tail:Data([99]))
check(receipt.blocks(id:4,now:0),"unconfirmed raster blocks replacement")
check(receipt.wireFinished(id:4,at:10),"wire completion starts receipt wait")
check(!receipt.wireFinished(id:4,at:10.2),"a tail probe cannot restart its own receipt timeout")
check(receipt.probe(now:10.1)==nil,"no premature probe")
check(receipt.probe(now:10.31)?.tail==Data([99]),"probe preserves exact last fragment")
check(receipt.blocks(id:4,now:10.4),"ATT completion is not a device map receipt")
receipt.acknowledge(id:3);check(receipt.blocks(id:4,now:10.5),"an old K cannot release the new raster")
receipt.acknowledge(id:4);check(!receipt.blocks(id:4,now:10.5),"real matching K releases the raster")
receipt.begin(id:5,tail:Data([98]));_ = receipt.wireFinished(id:5,at:20)
check(receipt.probe(now:20.31) != nil && receipt.probe(now:20.61) != nil && receipt.probe(now:21)==nil,"at most two small receipt probes")
check(receipt.expire(now:21.9)==nil && receipt.expire(now:22.1)==5,"receipt wait is bounded without synthesizing success")
var meter=BleMapWireMeter();meter.begin(id:1,bytes:112461,at:10.1493)
let sample=meter.finish(id:1,at:42.3957)!
check(abs(sample.seconds-32.2464)<0.001 && (meter.rate ?? 0)<3500,"replay the actual first-map wire duration")
check(meter.finish(id:1,at:42.7)==nil,"tail probes cannot inflate measured throughput")
let n=1200
var idx=[UInt8](repeating:0,count:n*n)
for y in 0..<n {for x in 590..<610 {idx[y*n+x]=1}}
for y in 40..<400 {for x in 40..<450 {idx[y*n+x]=(x+y)%2==0 ? 4:5}}
for y in 500..<850 {for x in 40..<450 {idx[y*n+x]=(x+y)%2==0 ? 6:7}}
let full=MapGeo.encodeRaster(frameId:1,x0:-1200,y0:1200,w:n,h:n,mmPerPx:2000|RasterPacket.flag,idx:idx)
// Busy roofs exceed production packet capacity in this synthetic checker. Use
// larger stripes while retaining redundant runs after roof removal.
if !RasterPacket.valid(full) {
    for y in 40..<400 {for x in 40..<450 {idx[y*n+x]=(x/5+y/5)%2==0 ? 4:5}}
    for y in 500..<850 {for x in 40..<450 {idx[y*n+x]=(x/5+y/5)%2==0 ? 6:7}}
}
let roofs=MapGeo.encodeRaster(frameId:1,x0:-1200,y0:1200,w:n,h:n,mmPerPx:2000|RasterPacket.flag,idx:idx)
check(RasterPacket.valid(roofs),"valid detailed fixture")
let base=MapDeliveryPolicy.compactBase(roofs)
check(RasterPacket.valid(base) && !RasterPacket.isBuilding(base),"compact road packet remains wire-compatible")
let decoded=RasterPacket.decode(base)!
for i in idx.indices {check(decoded[i]==(idx[i]>3 ? idx[i]&2:idx[i]),"roof removal preserves underlying park/ground and every road pixel")}
check(base.count<roofs.count/4,"roof removal must merge RLE runs, not merely relabel colours at the old byte count")
check(!MapDeliveryPolicy.roofsFit(kmh:120,bytes:127289,rate:3600),"actual slow link cannot block roads with full roofs")
check(MapDeliveryPolicy.roofsFit(kmh:0,bytes:127289,rate:3600),"stationary detail remains available")
check(MapDeliveryPolicy.roofsFit(kmh:120,bytes:127289,rate:40000),"sufficient measured bandwidth allows highway detail")
check(!MapDeliveryPolicy.mayPrepareRoofs(kmh:120,rate:3600),"deferred roofs do not burn extra GPU passes")
print("PASS credit-aware batching, route/position fairness, real K receipt gates, bounded probes, measured wire rate, compact base and optional roofs")
print("Compact map fixture: full=\(roofs.count), base=\(base.count)")

// Recorded ATT delays are replayed with explicit synthetic ready/not-ready
// windows; the original log does not expose actual no-response credit timings.
let fixtureURL=URL(fileURLWithPath:"ios/tests/map-wire/observed-delays.json")
let fixture=try! JSONSerialization.jsonObject(with:Data(contentsOf:fixtureURL)) as! [String:Any]
let delays=fixture["rttSeconds"] as! [Double]
func replay(bytes:Int,legacy:Bool)->Double {
    let parts=(bytes+484)/485
    var queue=BleBulkWriteQueue(),pace=BleWritePacer(),batch=BleBulkBarrier()
    queue.replace(type:3,id:1,packets:(0..<parts).map{Data([UInt8($0&255),UInt8($0>>8)])})
    var now=0.0,ack:Double?,responseIndex=0,received:[Int]=[]
    while queue.hasData {
        if let due=ack,now>=due {
            let p=queue.inFlight!;received.append(Int(p.data[0])|Int(p.data[1])<<8)
            _ = queue.complete(success:true);batch.reset();ack=nil
        }
        // An explicit transient credit pattern, shared by both versions.
        let canWithout=Int(now*1000)%80<20
        if let head=queue.peek {
            let seq=Int(head.data[0])|Int(head.data[1])<<8
            let required=batch.requiresResponse(seq:seq,parts:parts,retry:false) || (legacy && !canWithout)
            if BleBulkBarrier.canStart(response:required,canWithout:canWithout),pace.take(now:now) {
                _ = queue.next()
                if required {
                    ack=now+delays[responseIndex%delays.count];responseIndex+=1
                } else {
                    check(canWithout,"never issue a no-response write while CoreBluetooth reports backpressure")
                    _ = batch.needsResponse(seq:seq,parts:parts,retry:false,commandReady:true)
                    received.append(seq);_ = queue.complete(success:true)
                }
            }
        }
        now+=0.001;precondition(now<180,"replay stalled")
    }
    check(received==Array(0..<parts),"transient backpressure preserves every ordered fragment")
    return now
}
let oldFull=replay(bytes:112461,legacy:true),newBase=replay(bytes:38304,legacy:false)
check(newBase<oldFull/2,"small initial roads plus readiness-aware batching reduce the modeled transfer budget")
print("PASS measured RTT replay under explicit credit model: legacy full=\(oldFull)s, compact base=\(newBase)s (modeled, not a phone measurement)")
