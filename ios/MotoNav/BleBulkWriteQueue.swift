import Foundation

/// Keep one ATT write in flight; replacing a map must not resurrect its obsolete fragments.
struct BleBulkWriteQueue {
    struct Part { let type: Int, id: Int, data: Data; var attempts = 0 }
    enum Result: Equatable { case sent, retry, obsolete, failed }
    private var parts: [Part] = []
    private var latest: [Int: Int] = [:]
    private var acknowledged: [Int: Int] = [:]
    private(set) var inFlight: Part?
    var hasData: Bool { inFlight != nil || !parts.isEmpty }
    func contains(type:Int,id:Int)->Bool {
        (inFlight?.type == type && inFlight?.id == id) || parts.contains{$0.type==type && $0.id==id}
    }
    var canStart: Bool { inFlight == nil && !parts.isEmpty }
    var queuedCount: Int { parts.count }
    func payloadRemaining(type:Int,id:Int)->Int {
        let queued=parts.filter{$0.type==type && $0.id==id}.reduce(0){$0+max(0,$1.data.count-15)}
        return queued + (inFlight.map{$0.type==type && $0.id==id ? max(0,$0.data.count-15):0} ?? 0)
    }
    var peek:Part? {canStart ? parts.first:nil}
    mutating func replace(type: Int, id: Int, packets: [Data], prioritize: Bool = false) {
        parts.removeAll { $0.type == type }
        latest[type] = id; acknowledged.removeValue(forKey: type)
        let incoming=packets.map { Part(type: type, id: id, data: $0) }
        if prioritize {parts.insert(contentsOf:incoming,at:0)} else {parts.append(contentsOf:incoming)}
    }
    mutating func next() -> Part? {
        guard canStart else { return nil }
        var part = parts.removeFirst(); part.attempts += 1; inFlight = part
        return part
    }
    mutating func complete(success: Bool) -> Result {
        guard let part = inFlight else { return .obsolete }
        inFlight = nil
        if success || acknowledged[part.type] == part.id { return .sent }
        guard latest[part.type] == part.id else { return .obsolete }
        if part.attempts < 3 { parts.insert(part, at: 0); return .retry }
        parts.removeAll { $0.type == part.type && $0.id == part.id }
        return .failed
    }
    mutating func acknowledge(type: Int, id: Int) {
        acknowledged[type] = id
        parts.removeAll { $0.type == type && $0.id == id }
        // An application K can arrive before the ATT callback; still wait for that callback.
    }
    mutating func reset() { parts.removeAll(); latest.removeAll(); acknowledged.removeAll(); inFlight = nil }
    /// Keep the ATT callback slot occupied, but retire all queued packets of these kinds.
    mutating func cancel(types:Set<Int>) {
        parts.removeAll{types.contains($0.type)}
        for type in types {latest.removeValue(forKey:type);acknowledged.removeValue(forKey:type)}
    }
}

enum BleOutboundPriority {
    enum Choice: Equatable { case command, bulk, image, idle }
    static func choose(commandReady: Bool, bulkReady: Bool, bulkPending: Bool, imageReady: Bool, favorBulk:Bool=false) -> Choice {
        if commandReady && !(favorBulk && bulkReady) { return .command }
        if bulkReady { return .bulk }
        if !bulkPending && imageReady { return .image }
        return .idle
    }
}

/// Bound outstanding commands without paying an ATT round trip for every map fragment.
/// First/last fragments and failed retries always validate the transport with a response.
struct BleBulkBarrier {
    private var commands = 0
    func requiresResponse(seq:Int,parts:Int,retry:Bool)->Bool {
        seq == 0 || seq == parts-1 || retry || commands>=4
    }
    mutating func needsResponse(seq:Int,parts:Int,retry:Bool,commandReady:Bool)->Bool {
        // Backpressure is a reason to wait for readiness, not a new ATT barrier.
        // The caller checks readiness before dequeueing a no-response fragment.
        if requiresResponse(seq:seq,parts:parts,retry:retry) {return true}
        commands+=1;return false
    }
    static func canStart(response:Bool,canWithout:Bool)->Bool {response || canWithout}
    mutating func reset() { commands = 0 }
}


struct MapTraceBuffer {
    static let capacity=2048
    private(set) var events:[[String:Any]]=[]
    private(set) var dropped=0
    private static let allowed:Set<String>=["kmh","ready","sentCovered","confirmedCovered","serial","baseReady","needsCoverage","nextBlock","buildings","source","type","id","bytes","seq","parts","attempt","response","commandsQueued","bulkQueued","transferPending","outsideView","size","seconds","components","pixels","maxWrite","payload","elapsed","awaitAck","viewportOffsetM","preparedOffsetM","decodeValid","rate","probe","canWithout","needsResponse","style","beforeBytes"]
    mutating func append(name:String,fields:[String:Any],seconds:Double) {
        let safeName=name.range(of:"^[a-z][a-z0-9-]{0,39}$",options:.regularExpression) != nil ? name:"event"
        var e:[String:Any]=["event":safeName,"time":max(0,seconds)]
        for (key,value) in fields where Self.allowed.contains(key) {
            if let n=value as? NSNumber,n.doubleValue.isFinite {e[key]=n}
        }
        if events.count>=Self.capacity {events.removeFirst();dropped+=1}
        events.append(e)
    }
    var document:[String:Any] {["events":events,"dropped":dropped,"capacity":Self.capacity]}
}


/// A last-fragment duplicate asks the production receiver to repeat a lost K,
/// without inventing an acknowledgement or re-sending the complete raster.
struct BleMapReceipt {
    struct Pending {let id:Int,tail:Data;var wireDone:Double?=nil;var probes=0}
    private(set) var current:Pending?
    mutating func begin(id:Int,tail:Data) {current=Pending(id:id,tail:tail)}
    mutating func wireFinished(id:Int,at now:Double)->Bool {
        guard var p=current,p.id==id,p.wireDone==nil else {return false}
        p.wireDone=now;current=p;return true
    }
    func blocks(id:Int,now:Double)->Bool {
        guard let p=current,p.id==id else {return false}
        return true // Explicit receipt or bounded expiry releases the handoff.
    }
    var waiting:Bool {current?.wireDone != nil}
    mutating func probe(now:Double)->Pending? {
        guard var p=current,let done=p.wireDone,p.probes<2,now-done>=0.3*Double(p.probes+1),now-done<2 else {return nil}
        p.probes+=1;current=p;return p
    }
    mutating func expire(now:Double)->Int? {
        guard let p=current,let done=p.wireDone,now-done>=2 else {return nil}
        current=nil;return p.id
    }
    mutating func acknowledge(id:Int) {if current?.id==id {current=nil}}
    mutating func reset() {current=nil}
}

struct BleMapWireMeter {
    private var transfer:(id:Int,bytes:Int,at:Double,tailIssued:Bool)?
    private(set) var rate:Double?
    mutating func begin(id:Int,bytes:Int,at:Double) {transfer=(id,bytes,at,false)}
    mutating func tailIssued(id:Int) {if transfer?.id==id {transfer?.tailIssued=true}}
    mutating func finish(id:Int,at now:Double)->(bytes:Int,seconds:Double)? {
        guard let p=transfer,p.id==id,now>p.at else {return nil}
        transfer=nil
        // An already-decoded raster can acknowledge the first duplicate fragment.
        // That confirms delivery, but cannot measure a whole raster's throughput.
        guard p.tailIssued else {return nil}
        let seconds=now-p.at,sample=Double(p.bytes)/seconds
        rate=rate.map{$0*0.65+sample*0.35} ?? sample
        return (p.bytes,seconds)
    }
    mutating func cancel() {transfer=nil}
    mutating func reset() {transfer=nil;rate=nil}
}
