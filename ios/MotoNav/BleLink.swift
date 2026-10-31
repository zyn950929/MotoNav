import Foundation
import CoreBluetooth
import UIKit

/// 版本：跟随 App 版本（见 Config.swift 的 APP_VERSION）
///
/// 和圆屏的蓝牙连接（低功耗蓝牙）。协议和安卓版完全一样：
/// - 收件箱 RX：手机写文字指令（"R,120"、"#v,42"…）、文字图片（'B' 开头的二进制包）、
///   小地图大块数据（'M' 开头，iOS v0.2 起：地图块、路线，导航消息优先，空闲时再发）
/// - 发件箱 TX：圆屏通知手机（"T,W" 要天气、"S,…" 帧率统计、"K,类型,编号" 大块数据收齐…）
/// - 同一类状态消息在队列里只留最新一条；连上后把所有状态补发一遍
final class BleLink: NSObject, ObservableObject {
    static let shared = BleLink()

    private let serviceUUID = CBUUID(string: "6e400001-b5a3-f393-e0a9-e50e24dcca9e")
    private let rxUUID = CBUUID(string: "6e400002-b5a3-f393-e0a9-e50e24dcca9e")
    private let txUUID = CBUUID(string: "6e400003-b5a3-f393-e0a9-e50e24dcca9e")
    private let MAX_WRITE = 500
    private let IMG_HEADER = 10
    private let BULK_HEADER = 15

    @Published var status = "未连接"
    struct ScreenChoice:Identifiable { let id:UUID;let name:String }
    @Published private(set) var screenName=""
    @Published private(set) var screenBattery:Int?
    @Published private(set) var screenCharging=false
    @Published private(set) var availableScreens:[ScreenChoice]=[]
    private var screenChoices:[UUID:CBPeripheral]=[:]
    private var choosingScreen=false
    @Published private(set) var mediaLyricsSupported = false
    @Published private(set) var mediaLyricsActive = false
    @Published private(set) var mediaAMSControls = 0
    @Published var mediaLyricsStatus = ""
    @Published var mediaSongInfo = ""
    @Published private(set) var mediaHIDSupported = false
    @Published private(set) var mediaHIDReady = false
    @Published private(set) var mediaHIDMode = false
    @Published private(set) var verified = false
    private var buildingProbe = BleCapabilityProbe()
    private var buildingQueries: Int { buildingProbe.attempts }
    private var txNotifyReady = false
    @Published private(set) var buildingsSupported = false
    @Published var isReady = false
    @Published var sentCount = 0
    @Published var lastSent = ""
    @Published var screenStats = ""
    // iOS v0.6 圆屏每 2 秒回报的 "S,帧率,画一帧,推屏,屏幕MHz,画地图,画边框,快速刷屏,底图,核心数"（只在显示小地图时才发）
    @Published var screenFps = ""
    @Published var screenMHz = -1                   // -1 = 还不知道
    @Published var screenFast = -1
    @Published var statAt = Date.distantPast

    private var central: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var rxChar: CBCharacteristic?
    private var txChar: CBCharacteristic?
    private var wantConnected = true
    private var handshake=BleHandshakeClock()
    private var connectionRetries=0
    private var retryAfter=0.0

    // 发送队列
    private struct Out { let key: String?; var data: Data }
    private var queue: [Out] = []
    private var bulkQ = BleBulkWriteQueue()
    private var bulkWriteSerial = 0
    private var bulkBarrier = BleBulkBarrier()
    private var lastState: [String: String] = [:]          // 文字状态（重连后补发）
    private var stateOrder: [String] = []
    private var lastImg: [Int: (w: Int, h: Int, data: Data)] = [:]
    private var imgId = [Int](repeating: 0, count: 13)
    private var phoneClock=PhoneClockSync()
    private var clockWriteID=0
    private var clockObserver:NSObjectProtocol?
    private var heartbeat: Timer?
    private var lastWriteAt = Date.distantPast
    private var writePacer = BleWritePacer()
    private var pumpScheduled = false

    /// 圆屏发来的通知（主线程回调）
    var onNotify: ((String) -> Void)?

    override init() {
        super.init()
        central = CBCentralManager(delegate: self, queue: .main,
                                   options: [CBCentralManagerOptionRestoreIdentifierKey: "motonav.central"])
        clockObserver=NotificationCenter.default.addObserver(forName:UIApplication.significantTimeChangeNotification,object:nil,queue:.main){[weak self] _ in self?.syncPhoneClock(force:true)}
        heartbeat = Timer.scheduledTimer(withTimeInterval: 2.0, repeats: true) { [weak self] _ in
            self?.syncPhoneClock()
            self?.checkConnectionSetup()
            self?.requestBuildingCapability()
            MusicBridge.shared.syncLyrics()
            MusicBridge.shared.pollMetadata()
            guard let self = self, self.isReady, self.queue.isEmpty, !self.bulkQ.hasData,
                  Date().timeIntervalSince(self.lastWriteAt) > 1.5 else { return }
            self.send("P")
        }
    }

    var bluetoothAvailable:Bool {central.state == .poweredOn}

    var connectionSummary:String {
        if isReady {
            if !verified { return "请按圆屏提示完成配对" }
            let name=screenName.isEmpty ? "圆屏":screenName
            return name+" 已连接"+(screenBattery.map{" · \($0)%"+(screenCharging ? " 充电中":"")} ?? "")
        }
        if status.contains("权限") { return "请在 iPhone 设置中允许蓝牙访问" }
        if status.contains("没打开") { return "请打开 iPhone 蓝牙" }
        if status.contains("搜索") { return "正在搜索圆屏…" }
        if status.contains("连接") && !status.contains("未连接") { return "正在连接圆屏…" }
        if status.contains("不可用") || status.contains("找不到") { return "圆屏暂不可用，请检查电源后重试" }
        return "圆屏未连接"
    }

    // MARK: - 连接

    func beginScreenSelection() {
        choosingScreen=true;availableScreens=[];screenChoices=[:]
        if let p=peripheral {addScreenChoice(p)}
        guard central.state == .poweredOn else{return}
        central.scanForPeripherals(withServices:[serviceUUID],options:nil)
    }
    func endScreenSelection() {
        guard choosingScreen else{return}
        choosingScreen=false;central.stopScan()
        if wantConnected && (peripheral == nil || peripheral?.state == .disconnected) {connect()}
    }
    private func addScreenChoice(_ p:CBPeripheral) {
        screenChoices[p.identifier]=p
        if !availableScreens.contains(where:{$0.id==p.identifier}) {
            availableScreens.append(ScreenChoice(id:p.identifier,name:p.name ?? "MotoNav 圆屏"))
        }
    }
    func selectScreen(_ id:UUID) {
        guard central.state == .poweredOn,let chosen=screenChoices[id] else{return}
        choosingScreen=false;central.stopScan()
        if peripheral === chosen,chosen.state == .connected {return}
        if let old=peripheral {central.cancelPeripheralConnection(old)}
        peripheral=nil;wantConnected=true;connectionRetries=0;retryAfter=0
        UserDefaults.standard.set(id.uuidString,forKey:"ble_peripheral")
        attach(chosen)
    }
    func forgetOtherPhones() {
        guard isReady,verified else{return}
        sendOnce("#UNPAIR")
    }

    func connect() {
        connectionRetries=0;retryAfter=0
        wantConnected = true
        guard central.state == .poweredOn else { status = "蓝牙没打开"; return }
        if let p = peripheral,p.state == .connected {
            if !isReady { discover(p) };return
        }
        if let p = peripheral,p.state == .connecting { return }
        // 先找系统已经连着的（比如 App 重启后）
        let known = central.retrieveConnectedPeripherals(withServices: [serviceUUID])
        if let p = known.first { attach(p); return }
        if let idStr = UserDefaults.standard.string(forKey: "ble_peripheral"), let id = UUID(uuidString: idStr),
           let p = central.retrievePeripherals(withIdentifiers: [id]).first {
            attach(p)                                   // 直接连上次那块（不用扫描，圆屏开机就自动连上）
            return
        }
        status = "正在搜索圆屏…"
        central.scanForPeripherals(withServices: [serviceUUID], options: nil)
    }

    func disconnect() {
        wantConnected = false
        handshake.begin(.idle,now:ProcessInfo.processInfo.systemUptime)
        central.stopScan()
        if let p = peripheral { central.cancelPeripheralConnection(p) }
    }

    private func attach(_ p: CBPeripheral) {
        isReady = false; txNotifyReady = false
        screenName=p.name ?? "MotoNav";screenBattery=nil;screenCharging=false
        buildingsSupported = false
        mediaHIDSupported = false; mediaHIDReady = false; mediaHIDMode = false; mediaLyricsSupported = false; mediaLyricsActive = false; mediaAMSControls = 0; verified = false; buildingProbe.reset()
        MusicBridge.shared.resetAMS()
        peripheral = p
        p.delegate = self
        rxChar=nil;txChar=nil
        if p.state == .connected { discover(p);return }
        status = "正在连接…"
        handshake.begin(.connecting,now:ProcessInfo.processInfo.systemUptime)
        central.connect(p, options: nil)
    }

    private func discover(_ p: CBPeripheral) {
        guard p === peripheral else { return }
        status="已连接，正在读取圆屏服务…"
        handshake.begin(.services,now:ProcessInfo.processInfo.systemUptime)
        p.discoverServices([serviceUUID])
    }
    private func checkConnectionSetup() {
        guard wantConnected,central.state == .poweredOn,handshake.expired(now:ProcessInfo.processInfo.systemUptime) else { return }
        connectionRetries += 1
        retryAfter=ProcessInfo.processInfo.systemUptime+(connectionRetries>=3 ? 30 : 2)
        if connectionRetries>=3 { status="圆屏暂不可用，稍后自动重试；也可点连接重试" }
        else { status="连接步骤超时，正在重新连接（\(connectionRetries)/3）" }
        if let p=peripheral {central.cancelPeripheralConnection(p)}
    }
    private func retryConnection(_ p:CBPeripheral) {
        let delay=max(0,retryAfter-ProcessInfo.processInfo.systemUptime)
        DispatchQueue.main.asyncAfter(deadline:.now()+delay){[weak self] in
            guard let self=self,self.wantConnected,!self.choosingScreen,self.central.state == .poweredOn,self.peripheral === p,p.state == .disconnected else{return}
            self.attach(p)
        }
    }

    private func requestBuildingCapability() {
        guard buildingProbe.next(ready: isReady && txNotifyReady, supported: buildingsSupported, now: Date().timeIntervalSinceReferenceDate) else { return }
        sendOnce("#GB?")
    }

    func retryBuildingCapability() { buildingProbe.reset(); requestBuildingCapability() }

    var maxWrite: Int {
        guard let p = peripheral else { return 20 }
        let response = rxChar?.properties.contains(.write) == true ? p.maximumWriteValueLength(for: .withResponse) : MAX_WRITE
        return min(min(p.maximumWriteValueLength(for: .withoutResponse), response), MAX_WRITE)
    }

    func syncPhoneClock(force:Bool=false) {
        guard isReady,verified else{return}
        let now=Date()
        if let message=phoneClock.message(wall:now.timeIntervalSince1970,uptime:ProcessInfo.processInfo.systemUptime,
            offset:TimeZone.current.secondsFromGMT(for:now),force:force){sendClock(message)}
    }
    private func sendClock(_ message:String) {
        guard isReady,let p=peripheral,let rx=rxChar else {return}
        let data=Data(message.utf8)
        // #TIME is 22 bytes in UTC+8, exceeding a default 20-byte command write.
        // Share the existing one-in-flight ATT queue so this write is acknowledged
        // and never overlaps a map's acknowledged write. It is not an M packet.
        if rx.properties.contains(.write),data.count<=p.maximumWriteValueLength(for:.withResponse) {
            clockWriteID=(clockWriteID+1)&255
            bulkQ.replace(type:0,id:clockWriteID,packets:[data],prioritize:true)
            pump()
        } else if data.count<=p.maximumWriteValueLength(for:.withoutResponse) {sendOnce(message)}
    }

    // MARK: - 发送

    private func keyOf(_ msg: String) -> String {
        if msg.hasPrefix("#") { return String(msg.split(separator: ",", maxSplits: 1).first ?? "") }
        if msg == "P" { return "P" }
        return "NAV"                                    // 导航方向、到达 A、清屏 C
    }

    /// 发一条文字消息（同类型只留最新）
    func send(_ msg: String) {
        if msg.hasPrefix("#TIME,"){sendClock(msg);return}
        let k = keyOf(msg)
        if msg == "C" {                                 // 清屏：状态全部忘掉（亮度、风格、方向这些设置除外）
            let keep: Set<String> = ["#b", "#t", "#R", "#Q", "#n", "#m", "#r", "#w"]
            lastState = lastState.filter { keep.contains($0.key) }
            stateOrder = stateOrder.filter { keep.contains($0) }
            lastImg.removeAll()
            queue.removeAll { $0.key?.hasPrefix("B") == true }
        }
        if k != "P" {
            if lastState[k] == nil { stateOrder.append(k) }
            lastState[k] = msg
        }
        enqueue(k, Data(msg.utf8))
        pump()
    }

    /// 只发一次、不记下来重连后补发（屏幕速度 #q、快速刷屏 #f：圆屏收到会存起来并重启，补发会造成反复重启）
    func sendOnce(_ msg: String) {
        enqueue(keyOf(msg), Data(msg.utf8))
        pump()
    }

    /// 圆屏屏幕数据线速度 40 / 60 / 80 MHz（圆屏保存后自动重启）
    func setScreenMHz(_ v: Int) {
        guard [40, 60, 80].contains(v) else { return }
        screenMHz = v
        sendOnce("#q,\(v)")
    }

    /// 快速刷屏开 / 关（圆屏保存后自动重启）
    func setScreenFast(_ on: Bool) {
        screenFast = on ? 1 : 0
        sendOnce(on ? "#f,1" : "#f,0")
    }

    /// Drop cached weather before resetting the screen; reconnect must not replay stale data.
    func clearWeather() {
        let slots = [3, 4, 7, 8, 9, 10, 11]
        for key in ["#W", "#WX"] { lastState.removeValue(forKey: key) }
        stateOrder.removeAll { $0 == "#W" || $0 == "#WX" }
        for slot in slots { lastImg.removeValue(forKey: slot) }
        queue.removeAll { out in
            guard let key = out.key else { return false }
            return key == "#W" || key == "#WX" || slots.contains { key.hasPrefix("B\($0):") }
        }
        for slot in slots { enqueueImage(slot: slot, w: 0, h: 0, data: Data()) }
        sendOnce("#WXRESET")
    }

    /// Remove queued/cached lyrics before an OFF command; no media-mode command here.
    func sendLyricsMode(_ message: String) {
        if message == "#ML,0" {
            queue.removeAll { $0.key == "#LY" || $0.key?.hasPrefix("B12:") == true }
            lastState.removeValue(forKey: "#LY"); stateOrder.removeAll { $0 == "#LY" }
        }
        send(message)
        if message == "#ML,0" { sendImage(slot: 12, w: 0, h: 0, data: Data()) }
    }

    /// 发一张文字图片（4 位灰度，两个像素一个字节）；w = 0 表示清空这一栏
    func sendImage(slot: Int, w: Int, h: Int, data: Data) {
        guard imgId.indices.contains(slot) else { return }
        lastImg[slot] = (w, h, data)
        enqueueImage(slot: slot, w: w, h: h, data: data)
        pump()
    }

    /// 发一份小地图大块数据（地图块 / 路线）。同类型还没发完的旧数据直接作废。
    /// 每片：'M' 类型 编号 总片数(2) 片序号(2) 总字节(4) 偏移(4) + 数据
    func sendBulk(type: Int, id: Int, data: Data) {
        guard isReady else { return }
        let payload = maxWrite - BULK_HEADER
        if payload < 100 { return }                     // 没协商到大数据包就不发
        let parts = (data.count + payload - 1) / payload
        if parts == 0 || parts > 4000 { return }
        let total = data.count
        var packets: [Data] = []
        for p in 0..<parts {
            let off = p * payload
            let len = min(payload, total - off)
            var pkt = Data([UInt8(ascii: "M"), UInt8(type), UInt8(id & 0xFF),
                            UInt8(parts & 0xFF), UInt8((parts >> 8) & 0xFF), UInt8(p & 0xFF), UInt8((p >> 8) & 0xFF),
                            UInt8(total & 0xFF), UInt8((total >> 8) & 0xFF), UInt8((total >> 16) & 0xFF), UInt8((total >> 24) & 0xFF),
                            UInt8(off & 0xFF), UInt8((off >> 8) & 0xFF), UInt8((off >> 16) & 0xFF), UInt8((off >> 24) & 0xFF)])
            pkt.append(data.subdata(in: off..<(off + len)))
            packets.append(pkt)
        }
        bulkQ.replace(type: type, id: id, packets: packets)
        bulkKB += total / 1024
        pump()
    }

    /// 还有大块数据没发完
    var bulkPending: Bool { bulkQ.hasData }
    func bulkPending(type:Int,id:Int)->Bool {bulkQ.contains(type:type,id:id)}
    func cancelMapTransfers() {bulkQ.cancel(types:[1,3])}
    @Published var bulkKB = 0                         // 累计发了多少 KB 地图数据（调试用）

    private func enqueue(_ key: String, _ data: Data) {
        if let i = queue.firstIndex(where: { $0.key == key }) { queue[i].data = data }
        else { queue.append(Out(key: key, data: data)) }
    }

    private func header(_ slot: Int, _ id: Int, _ part: Int, _ parts: Int, _ w: Int, _ h: Int, _ off: Int) -> Data {
        Data([UInt8(ascii: "B"), UInt8(slot), UInt8(id & 0xFF), UInt8(part), UInt8(parts),
              UInt8(w & 0xFF), UInt8((w >> 8) & 0xFF), UInt8(h & 0xFF),
              UInt8(off & 0xFF), UInt8((off >> 8) & 0xFF)])
    }

    private func enqueueImage(slot: Int, w: Int, h: Int, data: Data) {
        queue.removeAll { $0.key?.hasPrefix("B\(slot):") == true }
        imgId[slot] = (imgId[slot] + 1) & 0xFF
        let id = imgId[slot]
        if w == 0 || data.isEmpty {
            queue.append(Out(key: "B\(slot):\(id):0", data: header(slot, id, 0, 1, 0, 0, 0)))
            return
        }
        let payload = maxWrite - IMG_HEADER
        if payload < 16 { return }
        let parts = (data.count + payload - 1) / payload
        if parts > 32 { return }
        for p in 0..<parts {
            let off = p * payload
            let len = min(payload, data.count - off)
            var pkt = header(slot, id, p, parts, w, h, off)
            pkt.append(data.subdata(in: off..<(off + len)))
            queue.append(Out(key: "B\(slot):\(id):\(p)", data: pkt))
        }
    }

    /// Position/turn commands retain priority. Maps use one acknowledged ATT write at a time;
    /// decorative text bitmaps yield until the map transfer finishes.
    private func pump() {
        guard isReady, let p = peripheral, let rx = rxChar else { return }
        let responses = rx.properties.contains(.write)
        while true {
            let command = queue.firstIndex { $0.key?.hasPrefix("B") != true }
            let choice = BleOutboundPriority.choose(commandReady: command != nil && p.canSendWriteWithoutResponse,
                bulkReady: bulkQ.canStart && (responses || p.canSendWriteWithoutResponse),
                bulkPending: bulkQ.hasData, imageReady: !queue.isEmpty && p.canSendWriteWithoutResponse)
            if choice == .idle { return }
            let now = ProcessInfo.processInfo.systemUptime
            guard writePacer.take(now: now) else {
                if !pumpScheduled {
                    pumpScheduled = true
                    DispatchQueue.main.asyncAfter(deadline: .now() + writePacer.delay(now: now)) { [weak self] in
                        guard let self = self else { return }
                        self.pumpScheduled = false; self.pump()
                    }
                }
                return
            }
            if choice == .bulk, let part = bulkQ.next() {
                let clockCommand=part.type == 0
                let seq = clockCommand ? 0 : Int(part.data[5]) | Int(part.data[6]) << 8
                let parts = clockCommand ? 1 : Int(part.data[3]) | Int(part.data[4]) << 8
                let response = responses && (clockCommand || bulkBarrier.needsResponse(seq: seq, parts: parts,
                    retry: part.attempts > 1, commandReady: p.canSendWriteWithoutResponse))
                traceBulk(part, event: "bulk-write", response: response)
                if response {
                    bulkWriteSerial += 1; let serial = bulkWriteSerial
                    p.writeValue(part.data, for: rx, type: .withResponse)
                    DispatchQueue.main.asyncAfter(deadline: .now() + 5) { [weak self] in
                        guard let self = self, self.isReady, self.bulkWriteSerial == serial, self.bulkQ.inFlight != nil else { return }
                        MapPipelineTrace.event("bulk-write-timeout", ["type": part.type, "id": part.id])
                        self.isReady = false; self.central.cancelPeripheralConnection(p)
                    }
                } else {
                    p.writeValue(part.data, for: rx, type: .withoutResponse)
                    _ = bulkQ.complete(success: true)
                }
            } else {
                let o = queue.remove(at: choice == .command ? command! : 0)
                p.writeValue(o.data, for: rx, type: .withoutResponse)
                sentCount += 1
                if o.key?.hasPrefix("B") != true, o.key != "P" { lastSent = String(data: o.data, encoding: .utf8) ?? "" }
            }
            lastWriteAt = Date()
        }
    }

    private func traceBulk(_ part: BleBulkWriteQueue.Part, event: String, response: Bool = true) {
        guard part.type != 0 else {return}
        guard part.data.count >= BULK_HEADER else { return }
        let seq = Int(part.data[5]) | Int(part.data[6]) << 8
        let count = Int(part.data[3]) | Int(part.data[4]) << 8
        if seq == 0 || seq == count - 1 || part.attempts > 1 {
            MapPipelineTrace.event(event, ["type": part.type, "id": part.id, "seq": seq, "parts": count,
                "attempt": part.attempts, "response": response, "commandsQueued": queue.count, "bulkQueued": bulkQ.queuedCount])
        }
    }

    /// 连上后：设置类消息放最前，再补发其他状态和文字图片
    private func onLinkReady() {
        guard !isReady, txNotifyReady else { return }
        isReady = true
        handshake.begin(.ready,now:ProcessInfo.processInfo.systemUptime)
        connectionRetries=0
        writePacer.reset()
        status = "已连接（数据包 \(maxWrite) 字节）"
        queue.removeAll()
        phoneClock=PhoneClockSync()
        bulkQ.reset(); bulkBarrier.reset(); bulkWriteSerial += 1
        if let b = lastState["#b"] { enqueue("#b", Data(b.utf8)) }
        for k in stateOrder where k != "#b" { if let v = lastState[k] { enqueue(k, Data(v.utf8)) } }
        for (slot, img) in lastImg { enqueueImage(slot: slot, w: img.w, h: img.h, data: img.data) }
        pump()
        Settings.shared.pushAll()
        requestBuildingCapability()
        MapFeeder.shared.resendAll()                    // 圆屏可能重启过：地图块和路线重发
    }
}

// MARK: - CoreBluetooth 回调

extension BleLink: CBCentralManagerDelegate, CBPeripheralDelegate {
    func centralManagerDidUpdateState(_ c: CBCentralManager) {
        if c.state != .poweredOn {
            isReady = false; txNotifyReady = false; buildingsSupported = false; mediaHIDSupported = false; mediaHIDReady = false; mediaHIDMode = false; mediaLyricsSupported = false; mediaLyricsActive = false; mediaAMSControls = 0; verified = false; buildingProbe.reset()
        MusicBridge.shared.resetAMS()
        }
        switch c.state {
        case .poweredOn: if wantConnected { connect() }
        case .unauthorized: status = "没有蓝牙权限：到“设置 → MotoNav”里打开蓝牙"
        case .poweredOff: status = "蓝牙没打开"
        default: status = "蓝牙不可用"
        }
    }

    func centralManager(_ c: CBCentralManager, willRestoreState dict: [String: Any]) {
        if let ps = dict[CBCentralManagerRestoredStatePeripheralsKey] as? [CBPeripheral], let p = ps.first {
            peripheral = p
            p.delegate = self
        }
    }

    func centralManager(_ c: CBCentralManager, didDiscover p: CBPeripheral, advertisementData: [String: Any], rssi RSSI: NSNumber) {
        if choosingScreen {addScreenChoice(p);return}
        guard wantConnected else{return}
        if let current=peripheral,current.state == .connected || current.state == .connecting {return}
        c.stopScan()
        attach(p)
    }

    func centralManager(_ c: CBCentralManager, didConnect p: CBPeripheral) {
        guard p === peripheral else { return }
        status = "已连接，正在读取服务…"
        UserDefaults.standard.set(p.identifier.uuidString, forKey: "ble_peripheral")
        discover(p)
    }

    func centralManager(_ c: CBCentralManager, didFailToConnect p: CBPeripheral, error: Error?) {
        guard p === peripheral else { return }
        status = "连接失败，重试中…"
        mediaHIDSupported = false; mediaHIDReady = false; mediaHIDMode = false; mediaLyricsSupported = false; mediaLyricsActive = false; mediaAMSControls = 0; verified = false; buildingProbe.reset(); txNotifyReady = false
        MusicBridge.shared.resetAMS()
        isReady = false
        connectionRetries+=1
        retryAfter=ProcessInfo.processInfo.systemUptime+(connectionRetries>=3 ? 30 : 2)
        handshake.begin(.idle,now:ProcessInfo.processInfo.systemUptime)
        if connectionRetries>=3 {status="圆屏暂不可用，稍后自动重试"}
        if wantConnected { retryConnection(p) }
    }

    func centralManager(_ c: CBCentralManager, didDisconnectPeripheral p: CBPeripheral, error: Error?) {
        guard p === peripheral else { return }
        screenBattery=nil;screenCharging=false
        isReady = false
        bulkQ.reset(); bulkBarrier.reset(); bulkWriteSerial += 1
        buildingsSupported = false
        mediaHIDSupported = false; mediaHIDReady = false; mediaHIDMode = false; mediaLyricsSupported = false; mediaLyricsActive = false; mediaAMSControls = 0; verified = false; buildingProbe.reset(); txNotifyReady = false
        MusicBridge.shared.resetAMS()
        MapFeeder.shared.onBuildingCapability()
        rxChar = nil;txChar=nil
        status = wantConnected ? "断开了，等圆屏回来自动重连…" : "未连接"
        handshake.begin(.idle,now:ProcessInfo.processInfo.systemUptime)
        if wantConnected { retryConnection(p) }
    }

    func peripheral(_ p: CBPeripheral, didDiscoverServices error: Error?) {
        guard p === peripheral else { return }
        guard let s = p.services?.first(where: { $0.uuid == serviceUUID }) else { status = "找不到圆屏的服务"; return }
        p.discoverCharacteristics([rxUUID, txUUID], for: s)
    }

    func peripheral(_ p: CBPeripheral, didDiscoverCharacteristicsFor s: CBService, error: Error?) {
        guard p === peripheral else { return }
        handshake.begin(.notifications,now:ProcessInfo.processInfo.systemUptime)
        for ch in s.characteristics ?? [] {
            if ch.uuid == rxUUID { rxChar = ch }
            if ch.uuid == txUUID {
                txChar=ch
                txNotifyReady = ch.isNotifying
                if !ch.isNotifying { p.setNotifyValue(true, for: ch) }
            }
        }
        if rxChar != nil {
            if txNotifyReady { onLinkReady() } else { status = "正在打开圆屏通知通道…" }
        } else { status = "找不到圆屏的收件通道" }
    }

    func peripheral(_ p: CBPeripheral, didUpdateNotificationStateFor ch: CBCharacteristic, error: Error?) {
        guard p === peripheral else { return }
        guard ch === txChar else { return }
        txNotifyReady = error == nil && ch.isNotifying
        guard txNotifyReady else { isReady = false; status = "圆屏通知通道未就绪，请重新连接"; return }
        if rxChar != nil { onLinkReady() }
    }

    func peripheralIsReady(toSendWriteWithoutResponse p: CBPeripheral) { pump() }

    func peripheral(_ p: CBPeripheral, didWriteValueFor ch: CBCharacteristic, error: Error?) {
        guard p === peripheral, ch === rxChar, let part = bulkQ.inFlight else { return }
        traceBulk(part, event: error == nil ? "bulk-write-response" : "bulk-write-error")
        bulkBarrier.reset()
        _ = bulkQ.complete(success: error == nil)
        pump()
    }

    func peripheral(_ p: CBPeripheral, didUpdateValueFor ch: CBCharacteristic, error: Error?) {
        guard p === peripheral else { return }
        guard ch === txChar,error == nil, let d = ch.value else { return }
        if d.first == 0xa6 { MusicBridge.shared.receiveAMS(d); return }
        guard let raw = String(data: d, encoding: .utf8) else { return }
        let s = raw.trimmingCharacters(in: .whitespacesAndNewlines)
        if BleAuthenticatedReply.accepts(s), !verified {
            verified = true; syncPhoneClock(force:true); buildingProbe.reset(); requestBuildingCapability(); Settings.shared.pushAll(); MapFeeder.shared.resendAll()
        }
        if s == "U,1" {return}
        if s == "CAP,HID,1" {
            mediaHIDSupported = true
            sendOnce("#AMS?")
            MusicBridge.shared.syncMode(); MusicBridge.shared.refresh(force: true); return
        }
        if s == "CAP,LYR,1" {
            mediaLyricsSupported = true
            MusicBridge.shared.syncMode(); MusicBridge.shared.refresh(force: true); return
        }
        if s == "CAP,BLD,1" {
            if !buildingsSupported { buildingsSupported = true; MapFeeder.shared.onBuildingCapability() }
            return
        }
        let parts = s.split(separator: ",").map(String.init)
        if parts.first == "V",parts.count>=3 {
            screenBattery=Int(parts[1]).flatMap{(0...100).contains($0) ? $0:nil}
            screenCharging=parts[2]=="1"
            return
        }
        if parts.first == "ML", parts.count == 2, ["0", "1"].contains(parts[1]) {
            mediaLyricsActive = parts[1] == "1"
            MusicBridge.shared.lyricsReply(mediaLyricsActive); return
        }
        if parts.first == "AMC", parts.count == 2, let mask = Int(parts[1]) {
            mediaAMSControls = mask & 7; return
        }
        if parts.first == "HID", parts.count == 3 {
            mediaHIDMode = parts[1] == "1"; mediaHIDReady = parts[2] == "1"
            MusicBridge.shared.refresh(force: true); return
        }
        if parts.first == "K", parts.count >= 3, let t = Int(parts[1]), let id = Int(parts[2]) {
            bulkQ.acknowledge(type: t, id: id)
            MapFeeder.shared.onAck(type: t, id: id)
            return
        }
        if parts.first == "S", parts.count >= 4 {
            screenFps = parts[1]
            statAt = Date()
            var t = "小地图 \(parts[1]) 帧/秒（画一帧 \(parts[2]) 毫秒，推屏 \(parts[3]) 毫秒"
            if parts.count >= 5, let m = Int(parts[4]) { screenMHz = m; t += "，屏幕速度 \(m)MHz" }
            t += "）"
            if parts.count >= 8 {
                screenFast = Int(parts[7]) ?? -1
                t += "\n其中画地图 \(parts[5]) 毫秒，画边框和信息 \(parts[6]) 毫秒；快速刷屏\(parts[7] == "1" ? "开" : "关")"
            }
            if parts.count >= 10, parts[8] != "0" { t += "\n画地图里，高德底图占 \(parts[8]) 毫秒（\(parts[9]) 个核心一起画）" }
            screenStats = t
            return
        }
        onNotify?(s)
    }
}
