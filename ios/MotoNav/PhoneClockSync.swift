import Foundation

/// UTC seconds and the phone's actual timezone offset. No wall time is replayed from a BLE cache.
struct PhoneClockSync {
    private var lastWall: Double?
    private var lastUptime=0.0
    private var lastOffset=Int.min
    private var warmupRemaining=0
    mutating func message(wall: Double, uptime: Double, offset: Int, force: Bool = false) -> String? {
        guard wall.isFinite,uptime.isFinite,wall>=1577836800,wall<4102444800,abs(offset)<=50400 else{return nil}
        let initial=lastWall == nil || force
        let interval=warmupRemaining>0 ? 2.0 : 60.0
        if !force,let previous=lastWall,uptime>=lastUptime,uptime-lastUptime<60,
           offset==lastOffset,abs((wall-previous)-(uptime-lastUptime))<2,uptime-lastUptime<interval {return nil}
        if initial {warmupRemaining=2} else if warmupRemaining>0 {warmupRemaining-=1}
        lastWall=wall;lastUptime=uptime;lastOffset=offset
        return "#TIME,\(Int64(wall.rounded(.down))),\(offset)"
    }
}

enum NavigationSpeed {
    static let values=[60,100,120]
    static func next(_ value: Int) -> Int {
        guard let i=values.firstIndex(of:value) else{return values[0]}
        return values[(i+1)%values.count]
    }
}
