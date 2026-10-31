import Foundation

var checks = 0
func check(_ value: @autoclosure () -> Bool, _ label: String) { precondition(value(), label); checks += 1 }
let payloads = [Data([1]), Data([2]), Data([3])]
var writer = BleBulkWriteQueue()
writer.replace(type: 3, id: 1, packets: payloads)
let first = writer.next()!
check(first.data == payloads[0] && writer.next() == nil, "Only one ATT write may be in flight")
check(writer.complete(success: false) == .retry, "Failed fragment retries before later fragments")
check(writer.next()?.data == first.data, "Retry preserves exact bytes")
check(writer.complete(success: true) == .sent, "ATT response releases the next fragment")
check(writer.next()?.data == payloads[1], "No fragment skipped after recovery")
writer.replace(type: 3, id: 2, packets: [Data([9])])
check(writer.complete(success: false) == .obsolete, "Late failure cannot resurrect superseded map")
check(writer.next()?.id == 2, "Replacement begins after outstanding callback")
writer.acknowledge(type: 3, id: 2)
check(writer.hasData && !writer.canStart, "Early application K still waits for outstanding ATT callback")
check(writer.complete(success: false) == .sent && !writer.hasData, "Application acknowledgement prevents unnecessary retransmission")

writer.replace(type: 3, id: 3, packets: payloads)
writer.replace(type: 2, id: 8, packets: [Data([7])])
for attempt in 1...3 {
    check(writer.next()?.attempts == attempt, "ATT retries remain bounded")
    _ = writer.complete(success: false)
}
check(writer.next()?.type == 2, "Permanent map error keeps independent route transfer")
_ = writer.complete(success: true)
check(!writer.hasData, "Failed map queue empties so application can retry complete map")
writer.replace(type: 3, id: 4, packets: payloads)
_ = writer.next()
writer.replace(type: 3, id: 5, packets: [Data([8])])
writer.acknowledge(type: 3, id: 4)
_ = writer.complete(success: true)
check(writer.next()?.id == 5, "Old K cannot clear a newer map")
writer.reset()
check(!writer.hasData && writer.inFlight == nil, "Disconnect clears outstanding writes")

// Continuous decorative images and 50 position/turn commands per second must not starve a map.
// Each ATT response takes 35ms; commands can be served while awaiting the response.
writer.replace(type: 3, id: 6, packets: (0..<60).map { Data([UInt8($0)]) })
var pace = BleWritePacer(), time = 0.0, responseAt: Double?, nextCommand = 0.0
var commandQueued = false, commands = 0, delivered: [UInt8] = [], imageWrites = 0
while writer.hasData {
    if time >= nextCommand { commandQueued = true; nextCommand += 0.02 }
    if let deadline = responseAt, time >= deadline {
        delivered.append(writer.inFlight!.data[0]); _ = writer.complete(success: true); responseAt = nil
    }
    let choice = BleOutboundPriority.choose(commandReady: commandQueued, bulkReady: writer.canStart,
        bulkPending: writer.hasData, imageReady: true)
    if choice != .idle, pace.take(now: time) {
        switch choice {
        case .command: commandQueued = false; commands += 1
        case .bulk: _ = writer.next(); responseAt = time + 0.035
        case .image: imageWrites += 1
        case .idle: break
        }
    }
    time += 0.001
    precondition(time < 4, "Map deadline missed")
}
check(delivered == Array(UInt8(0)..<UInt8(60)), "Every fragment delivered in order despite continuous image traffic")
check(commands > 100 && !commandQueued, "Position and turn commands remain live during ATT transfer")
check(time < 3, "60-fragment map finishes within viewport handoff time")
check(imageWrites <= 1, "Decorative images yield during map transfer")

// Device measurements show ~100ms ATT round trips. Batch four commands between barriers
// so a 60-fragment map still arrives before the previous viewport expires at highway speed.
writer.reset(); writer.replace(type: 3, id: 7, packets: (0..<60).map { Data([UInt8($0)]) })
pace.reset(); time = 0; responseAt = nil; nextCommand = 0; commandQueued = false
delivered = []; commands = 0
var barrier = BleBulkBarrier(), outstanding = 0, peakOutstanding = 0
while writer.hasData {
    if time >= nextCommand { commandQueued = true; nextCommand += 0.02 }
    if let deadline = responseAt, time >= deadline {
        delivered.append(writer.inFlight!.data[0]); _ = writer.complete(success: true)
        barrier.reset(); responseAt = nil; outstanding = 0
    }
    let choice = BleOutboundPriority.choose(commandReady: commandQueued, bulkReady: writer.canStart,
        bulkPending: writer.hasData, imageReady: true)
    if choice != .idle, pace.take(now: time) {
        switch choice {
        case .command: commandQueued = false; commands += 1
        case .bulk:
            let part = writer.next()!, seq = Int(part.data[0])
            let respond = barrier.needsResponse(seq: seq, parts: 60, retry: false, commandReady: true)
            outstanding += 1; peakOutstanding = max(peakOutstanding, outstanding)
            if respond { responseAt = time + 0.1 }
            else { delivered.append(part.data[0]); _ = writer.complete(success: true) }
        case .image, .idle: break
        }
    }
    time += 0.001; precondition(time < 4, "Batched map misses deadline")
}
check(delivered == Array(UInt8(0)..<UInt8(60)), "Batched fragments preserve order and content")
check(peakOutstanding <= 5 && time < 2, "Realistic 100ms round trips keep map under two seconds with bounded bursts")
check(commands > 50, "Navigation remains responsive during confirmed batches")
barrier.reset()
check(barrier.needsResponse(seq: 0, parts: 60, retry: false, commandReady: true), "Validate first fragment before command burst")
check(barrier.needsResponse(seq: 59, parts: 60, retry: false, commandReady: true), "Last fragment waits for actual transport response")
check(barrier.needsResponse(seq: 5, parts: 60, retry: true, commandReady: true), "Failed barrier retry cannot silently become unconfirmed")
check(barrier.needsResponse(seq: 5, parts: 60, retry: false, commandReady: false), "Host backpressure forces a response barrier")
print("BLE bulk: \(checks) checks passed; map delivery \(String(format: "%.2f", time))s, \(commands) navigation commands")
var clockWriter=BleBulkWriteQueue()
clockWriter.replace(type:3,id:1,packets:[Data([1]),Data([2])])
let held=clockWriter.next()!
let clockWire=Data("#TIME,1793433600,28800".utf8)
clockWriter.replace(type:0,id:1,packets:[clockWire],prioritize:true)
check(clockWriter.next()==nil && clockWriter.inFlight?.type==held.type,"Clock cannot overlap the in-flight ATT map write")
_ = clockWriter.complete(success:true)
let clockPart=clockWriter.next()!
check(clockPart.type==0 && clockPart.data==clockWire && clockWire.count>20,"Unfragmented clock command receives the next acknowledged ATT slot")
_ = clockWriter.complete(success:true)
check(clockWriter.next()?.data==Data([2]),"Map transfer resumes after the clock acknowledgement")
print("PASS clock ATT serialization, priority and complete message above the default 20-byte limit")
