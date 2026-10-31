package com.yuminaga.motonav

import android.annotation.SuppressLint
import android.bluetooth.BluetoothAdapter
import android.bluetooth.BluetoothDevice
import android.bluetooth.BluetoothGatt
import android.bluetooth.BluetoothGattCallback
import android.bluetooth.BluetoothGattCharacteristic
import android.bluetooth.BluetoothGattDescriptor
import android.bluetooth.BluetoothManager
import android.bluetooth.BluetoothProfile
import android.bluetooth.BluetoothStatusCodes
import android.bluetooth.le.ScanCallback
import android.bluetooth.le.ScanFilter
import android.bluetooth.le.ScanResult
import android.bluetooth.le.ScanSettings
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.os.Build
import android.os.Handler
import android.os.Looper
import android.os.ParcelUuid
import android.os.SystemClock
import java.util.UUID
import android.util.Log
import com.yuminaga.motonav.BleSession.Phase

/**
 * 版本：跟随 App 版本，见 MainActivity.kt 里的 APP_VERSION（当前 v5.7；v5.7 显示底图用时；v4.6 请求蓝牙 2M 模式、统计实际传输速度；v5.5 图片栏扩到 8 个，修复天气页闪退）
 *
 * 负责搜索圆屏、连接、发送指令（和圆屏固件里的名字、UUID 一一对应）。
 *
 * 发送机制（v1.7 重写）：
 * - 所有消息进一个发送队列，同一时间只有一条在路上，发完立刻发下一条。
 * - 同类状态消息（导航、车速、测速、车道、亮度）在队列里只保留最新一条，旧的直接被替换，
 *   所以圆屏永远收到最新数据，不会积压。
 * - 文字图片（路名、底部信息）按大数据包分片，每一片都必须送达，不会被替换。
 * - 连上后协商大数据包（MTU 517），重连后自动补发最近的全部状态和图片。
 * - 自动连接、锁屏也能重连、每 2 秒心跳（v1.3 起）。
 * - v2.0 新增“大块数据”通道（地图块、导航路线，几十 KB）：单独排队，优先级低于导航等状态消息；
 *   订阅圆屏的通知通道，圆屏收齐一份大块数据后会回 "K,类型,编号"。
 * - v6.4 配对验证（配合固件 v6.5）：第一次连接时手机弹出配对框，输入圆屏上的 6 位数字；
 *   圆屏验证通过后发 "U,1"，收到后把所有状态重发一遍（验证前发的会被圆屏拒收）。
 *   监听手机的配对状态，在界面上提示该做什么；forgetOthers() 让圆屏忘掉别的手机（v6.9）
 * - v7.8 认圆屏：固件 v7.0 起每块圆屏蓝牙名不同（"MotoNav-3A7F"，后 4 位是它的蓝牙地址）。
 *   App 记住上次验证通过的那块，搜索时先等它 4 秒；4 秒内没出现才连别的（新圆屏也能连上）。
 *   switchScreen() 换一块：这次搜索跳过现在这块，连附近另一块
 * - v6.9 手机记着旧配对、圆屏却忘了 → 连上就断：自动检测，能删就自动删掉手机上的旧配对，删不掉就提示去系统设置
 */
@SuppressLint("MissingPermission")
object BleLink {
    private val SERVICE_UUID: UUID = UUID.fromString("6e400001-b5a3-f393-e0a9-e50e24dcca9e")
    private val RX_UUID: UUID = UUID.fromString("6e400002-b5a3-f393-e0a9-e50e24dcca9e")
    private val TX_UUID: UUID = UUID.fromString("6e400003-b5a3-f393-e0a9-e50e24dcca9e")   // v2.0 圆屏 → 手机
    private val CCCD_UUID: UUID = UUID.fromString("00002902-0000-1000-8000-00805f9b34fb")
    private const val BULK_HEADER = 15

    private const val PREFER_MS = 4000L          // v7.8 先等上次那块圆屏这么久
    private const val HEARTBEAT_MS = 2000L
    private const val MAX_WRITE = 500          // 圆屏单个数据包最多收 512 字节，留点余量
    private const val IMG_HEADER = 10

    @Volatile var status = "未连接"
    @Volatile var verified = false               // v6.4 圆屏说这台手机通过了配对验证（"U,1"）
    @Volatile var pairHint = ""                  // v6.4 配对过程的提示（给界面看）
    @Volatile var screenBattery = ""             // v7.6 圆屏电量（固件 v6.8 起，接了电池才有），例 "85%⚡"
    @Volatile var screenBatteryPct = -1
    @Volatile var screenName = ""                // v7.8 连着的圆屏蓝牙名，例 "MotoNav-3A7F"（旧固件都叫 "MotoNav"）
    private var lastAddr: String? = null         // v7.8 上次验证通过的圆屏地址（存在手机上）
    private var skipAddr = ""                    // v7.8 换圆屏时这次搜索跳过的地址
    private var candidate: ScanResult? = null    // v7.8 搜到的“别的圆屏”里信号最强的
    private var scanStartAt = 0L
    @Volatile var needUnbond = false             // v6.9 手机记着配对、圆屏却不认了：要到系统蓝牙设置里取消配对
    private var staleCount = 0                   // v6.9 “手机有配对记录、却没通过验证就断开”连续几次
    @Volatile private var gatt: BluetoothGatt? = null
    private val session = BleSession<BluetoothGatt>()
    private var triedDirect = false
    private var capabilityQuerySent = false
    private var clockSyncElapsed=0L
    private var clockSyncWall=0L
    private var clockSyncOffset=Int.MIN_VALUE

    private fun trace(message: String) { Log.i("MotoNavBLE", message) }

    // Callbacks can arrive after close(), including for the same physical device.
    // Always dispatch first, then check the exact GATT instance on the main thread.
    private fun onCurrent(g: BluetoothGatt, block: () -> Unit) {
        handler.post { if (gatt === g && session.owns(g)) block() }
    }

    private fun closeCurrent() {
        val old = gatt
        gatt = null
        old?.let { session.clear(it) }
        ready = false; verified = false; buildingsSupported = false
        capabilityQuerySent = false; rxChar = null; mtu = 23
        phyText = "?"; burstStart = 0L
        synchronized(lock) { writing = false; queue.clear(); bulkQ.clear() }
        try { old?.disconnect() } catch (_: Exception) {}
        try { old?.close() } catch (_: Exception) {}
    }

    private fun failConnection(g: BluetoothGatt, reason: String) {
        if (gatt !== g || !session.owns(g)) return
        trace("retry phase=${session.phase()} reason=$reason")
        closeCurrent()
        status = "$reason，稍后自动重连"
        scheduleRetry()
    }
    private var rxChar: BluetoothGattCharacteristic? = null
    private var appContext: Context? = null
    private var scanning = false
    private var auto = false
    private val handler = Handler(Looper.getMainLooper())

    @Volatile var mtu = 23
    @Volatile private var ready = false        // 服务和大数据包都协商好了，可以发送

    // ---- 发送队列 ----
    private class Out(val key: String?, var data: ByteArray, val signal: TrafficSignalWrite? = null, val expiry: Long = Long.MAX_VALUE)
    private val lock = Any()
    private val queue = ArrayDeque<Out>()
    private class BulkPkt(val type: Int, val data: ByteArray)
    private val bulkQ = ArrayDeque<BulkPkt>()          // v2.0 大块数据
    @Volatile var bulkSentKB = 0
    // v4.6 传输速度统计：一批大块数据从开始发到发完，算平均速度
    @Volatile var phyText = "?"                       // 蓝牙模式：1M / 2M
    @Volatile var lastRateKBs = 0.0                   // 最近一批的速度（KB/秒）
    @Volatile var bestRateKBs = 0.0                   // 最快的一批
    @Volatile var lastBurstKB = 0                     // 最近一批多大
    private var burstStart = 0L
    private var burstBytes = 0L
    @Volatile var buildingsSupported = false
    @Volatile var screenStats = ""                    // v2.0.1 圆屏发来的帧率统计
    @Volatile var screenProf = ""                     // v5.8 每帧各步骤用时
    @Volatile var screenFps = ""                      // v2.6 给新界面用：帧率、屏幕速度、快速刷屏
    @Volatile var screenMhz = -1
    @Volatile var screenFast = -1
    private var writing = false
    private var writeStartedAt = 0L
    private var lastWriteAt = 0L

    // ---- 重连后补发用：最近的状态消息和文字图片 ----
    private val lastState = LinkedHashMap<String, String>()
    private class Img(val w: Int, val h: Int, val data: ByteArray)
    private val lastImg = HashMap<Int, Img?>()
    private val imgId = IntArray(12)               // v5.5 4 → 8：天气用到了图片栏 4，原来只有 0~3 会闪退

    // ---- 给界面看的统计 ----
    @Volatile var sentCount = 0
    @Volatile var lastSentMsg = ""
    @Volatile var lastSentAt = 0L
    @Volatile var imgSentCount = 0

    private fun now() = SystemClock.elapsedRealtime()

    private fun adapter(): BluetoothAdapter? =
        (appContext?.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager)?.adapter

    val isConnected: Boolean get() = ready

    // ================= 连接 =================

    fun startAuto(context: Context) {
        appContext = context.applicationContext
        registerBondWatch(context.applicationContext)
        if (auto) return
        auto = true
        handler.removeCallbacks(heartbeat)
        handler.postDelayed(heartbeat, HEARTBEAT_MS)
        connect(context)
    }

    fun stopAuto() {
        auto = false
        handler.removeCallbacksAndMessages(null)
        stopScan()
        closeCurrent()
        triedDirect = false
        status = "已停止"
    }

    fun connect(context: Context) {
        appContext = context.applicationContext
        try {
            val ad = adapter()
            if (ad == null || !ad.isEnabled) {
                status = "请先打开手机蓝牙"
                scheduleRetry()
                return
            }
            if (gatt != null || scanning) return
            // HID/system Bluetooth may already hold the link, so the screen no longer
            // advertises. Attach our navigation GATT client directly before scanning.
            if (!triedDirect) {
                val last = lastAddr().takeIf { it.isNotEmpty() && it != skipAddr }
                val known = last?.let { runCatching { ad.getRemoteDevice(it) }.getOrNull() }
                    ?: (appContext?.getSystemService(Context.BLUETOOTH_SERVICE) as? BluetoothManager)
                        ?.getConnectedDevices(BluetoothProfile.GATT)
                        ?.filter { it.address != skipAddr && (it.name == "MotoNav" || it.name?.startsWith("MotoNav-") == true) }
                        ?.singleOrNull()
                if (known != null) {
                    triedDirect = true
                    connectDevice(known, known.name ?: "MotoNav")
                    return
                }
            }
            triedDirect = false // A failed direct attempt gets a complete scan before trying again.
            val scanner = ad.bluetoothLeScanner ?: run { status = "无法扫描"; scheduleRetry(); return }
            val filters = listOf(ScanFilter.Builder().setServiceUuid(ParcelUuid(SERVICE_UUID)).build())
            // 优化功耗：初次扫描使用 BALANCED 均衡模式，多次未找到则降级为 LOW_POWER 低功耗模式，杜绝 100% 满负荷发热
            val scanMode = if (scanRetryCount < 2) ScanSettings.SCAN_MODE_BALANCED else ScanSettings.SCAN_MODE_LOW_POWER
            val settings = ScanSettings.Builder().setScanMode(scanMode).build()
            scanning = true
            status = "正在搜索圆屏…"
            candidate = null
            scanStartAt = now()
            scanner.startScan(filters, settings, scanCallback)
            val timeoutMs = if (scanRetryCount < 2) 8000L else 6000L
            handler.postDelayed(scanTimeout, timeoutMs)
            handler.postDelayed(pickCandidate, PREFER_MS)          // v7.8
        } catch (e: SecurityException) {
            scanning = false
            status = "缺少蓝牙权限，请先点「1. 授予权限」"
        }
    }

    private val scanTimeout = Runnable {
        if (scanning) {
            stopScan()
            skipAddr = ""                                          // v7.8 附近只有一块：下次也连它
            status = "没找到圆屏（确认已通电），稍后自动重试"
            scheduleRetry()
        }
    }

    private var scanRetryCount = 0

    fun resetScanBackoff() {
        scanRetryCount = 0
        if (auto && !scanning && gatt == null) {
            handler.removeCallbacks(retryRunnable)
            handler.post(retryRunnable)
        }
    }

    private val retryRunnable = Runnable { appContext?.let { connect(it) } }

    private fun scheduleRetry() {
        if (!auto) return
        handler.removeCallbacks(retryRunnable)
        scanRetryCount = (scanRetryCount + 1).coerceAtMost(4)
        // 指数退避 (Exponential Backoff)：5s -> 15s -> 30s -> 60s，未通电待机时大幅节省手机电量
        val delay = when {
            scanRetryCount <= 1 -> 5000L
            scanRetryCount == 2 -> 15000L
            scanRetryCount == 3 -> 30000L
            else -> 60000L
        }
        handler.postDelayed(retryRunnable, delay)
    }

    private fun stopScan() {
        handler.removeCallbacks(scanTimeout)
        handler.removeCallbacks(pickCandidate)
        if (!scanning) return
        scanning = false
        try { adapter()?.bluetoothLeScanner?.stopScan(scanCallback) } catch (_: Exception) {}
    }

    private val scanCallback = object : ScanCallback() {
        override fun onScanResult(callbackType: Int, result: ScanResult) { handler.post { handleScanResult(result) } }

        private fun handleScanResult(result: ScanResult) {
            if (!scanning) return
            // v7.8 优先连上次那块；别的圆屏先记着，等 4 秒还没见到上次那块再连
            val addr = result.device.address
            if (addr == skipAddr) return
            val last = lastAddr()
            if (last.isEmpty() || addr == last) { connectTo(result); return }
            val c = candidate
            if (c == null || result.rssi > c.rssi) candidate = result
            if (now() - scanStartAt >= PREFER_MS) candidate?.let { connectTo(it) }
        }

        override fun onScanFailed(errorCode: Int) {
            handler.post {
                if (scanning) {
                    stopScan()
                    status = "扫描失败（$errorCode），稍后自动重试"
                    scheduleRetry()
                }
            }
        }
    }

    /** v7.8 等够了还没见到上次那块：连信号最强的另一块 */
    private val pickCandidate = Runnable { if (scanning) candidate?.let { connectTo(it) } }

    private fun connectTo(r: ScanResult) {
        if (!scanning || gatt != null) return
        connectDevice(r.device, r.scanRecord?.deviceName ?: r.device.name ?: "MotoNav")
    }

    private fun connectDevice(device: BluetoothDevice, name: String) {
        stopScan()
        candidate = null
        handler.removeCallbacks(retryRunnable)
        screenName = name
        verified = false; pairHint = ""; needUnbond = false
        status = "正在连接 $screenName…"
        trace("connect direct=$triedDirect")
        try {
            val connection = device.connectGatt(appContext, false, gattCallback, BluetoothDevice.TRANSPORT_LE)
            if (connection == null) {
                status = "无法创建蓝牙连接，稍后重试"; scheduleRetry(); return
            }
            gatt = connection
            session.begin(connection, now())
        } catch (_: Exception) {
            status = "蓝牙连接启动失败，请检查蓝牙权限"; scheduleRetry()
        }
    }

    private fun prefs() = appContext?.getSharedPreferences("ble", Context.MODE_PRIVATE)
    private fun lastAddr(): String {
        if (lastAddr == null) lastAddr = prefs()?.getString("last", "").orEmpty()
        return lastAddr.orEmpty()
    }
    private fun saveLast(addr: String) {
        lastAddr = addr
        prefs()?.edit()?.putString("last", addr)?.apply()
    }

    /** v7.8 换一块圆屏：断开现在这块，这次搜索跳过它，连附近另一块（另一块验证通过后就记住新的） */
    fun switchScreen(c: Context) {
        appContext = c.applicationContext
        val cur = gatt?.device?.address ?: lastAddr()
        skipAddr = cur
        saveLast("")
        stopScan()
        handler.removeCallbacks(retryRunnable)
        closeCurrent()
        triedDirect = false
        screenName = ""; screenBattery = ""; screenBatteryPct = -1
        pairHint = ""; staleCount = 0; needUnbond = false
        connect(c)
    }

    /** Subscribe first: firmware sends its U,1 confirmation only in the first few seconds. */
    private fun enableNotify(g: BluetoothGatt) {
        if (!session.owns(g) || session.phase() != Phase.NOTIFY) return
        val tx = g.getService(SERVICE_UUID)?.getCharacteristic(TX_UUID)
        val d = tx?.getDescriptor(CCCD_UUID)
        if (tx == null || d == null) { failConnection(g, "圆屏缺少通知服务"); return }
        try {
            if (!g.setCharacteristicNotification(tx, true)) {
                failConnection(g, "无法开启圆屏通知"); return
            }
            val ok = if (Build.VERSION.SDK_INT >= 33) {
                g.writeDescriptor(d, BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE) == BluetoothStatusCodes.SUCCESS
            } else {
                @Suppress("DEPRECATION")
                d.value = BluetoothGattDescriptor.ENABLE_NOTIFICATION_VALUE
                @Suppress("DEPRECATION")
                g.writeDescriptor(d)
            }
            // Android may still be completing service discovery. Retry within the same bounded stage.
            if (!ok) handler.postDelayed({ if (session.owns(g)) enableNotify(g) }, 300)
        } catch (_: Exception) { failConnection(g, "圆屏通知订阅失败") }
    }

    private fun markVerified() {
        if (verified) return
        scanRetryCount = 0 // Reset after authentication, not every failed connection attempt.
        verified = true; pairHint = ""; staleCount = 0; needUnbond = false
        gatt?.device?.address?.let { saveLast(it); skipAddr = "" }
        trace("authenticated")
        val g = gatt ?: return
        if (ready) {
            session.move(g, Phase.AUTH, Phase.READY, now(), 0)
            status = "已连接（高速模式，数据包 ${maxWrite()} 字节）"
            resendAll()
            sendEvent("#HM,0")
        }
    }

    private fun onNotify(bytes: ByteArray?) {
        val msg = bytes?.toString(Charsets.UTF_8)?.trim() ?: return
        if (BleSession.confirms(msg, capabilityQuerySent)) markVerified()
        if(msg=="CAP,HID,1") { handler.post { sendEvent("#HM,0") }; return }
        if(msg=="CAP,BLD,1") {
            handler.post { if(!buildingsSupported) { buildingsSupported=true;MapFeeder.onBuildingCapability() } }
            return
        }
        val p = msg.split(',')
        if (p.size >= 3 && p[0] == "T" && p[1] == "M") { MusicBridge.onScreenEvent(p[2], p.drop(3).joinToString(",")); return }
        if (p.size >= 4 && p[0] == "S") {               // v2.0.1 圆屏帧率统计
            val mhz = if (p.size >= 5) "，屏幕速度 ${p[4]}MHz" else ""
            screenFps = p[1]
            if (p.size >= 5) screenMhz = p[4].toIntOrNull() ?: -1
            if (p.size >= 8) screenFast = p[7].toIntOrNull() ?: -1
            val more = if (p.size >= 8) "\n其中画地图 ${p[5]} 毫秒，画边框和信息 ${p[6]} 毫秒；快速刷屏${if (p[7] == "1") "开" else "关"}" else ""
            val ras = if (p.size >= 10 && p[8] != "0") "\n画地图里，高德底图占 ${p[8]} 毫秒（${p[9]} 个核心一起画）" else ""   // v5.7
            screenStats = "小地图 ${p[1]} 帧/秒（画一帧 ${p[2]} 毫秒，推到屏幕 ${p[3]} 毫秒$mhz）$more$ras"
            return
        }
        if (p.size >= 2 && p[0] == "T") { CallWatcher.onScreenTouch(p); return }   // v5.3 圆屏触屏接听
        if (p.size >= 3 && p[0] == "V") {                // v7.6 圆屏电量：V,百分比,充电,USB,mV
            val pct = p[1].toIntOrNull() ?: -1
            screenBatteryPct = pct
            screenBattery = if (pct < 0) "" else "$pct%" + (if (p[2] == "1") "⚡充电中" else "")
            return
        }
        if (p.size >= 2 && p[0] == "BOOT") {
            val id = p[1].toIntOrNull()
            if (id == -1) BootBrand.onSaveError() else id?.let { BootBrand.onReply(it) }
            return
        }
        if (p.size >= 2 && p[0] == "U") return
        if (p.size >= 12 && p[0] == "Q") {             // v5.8 圆屏每帧各步骤用时（0.1 毫秒）
            val names = listOf("底图", "道路", "路线", "红绿灯/电子眼/车头", "边缘渐暗", "北标/电子眼图标",
                "外圈", "车速/测速牌", "车道", "底部箭头和距离", "提示")
            screenProf = "各步骤用时（毫秒/帧）：" + names.indices.joinToString("，") { i ->
                "${names[i]} ${"%.1f".format((p[i + 1].toIntOrNull() ?: 0) / 10f)}"
            }
            return
        }
        if (p.size >= 3 && p[0] == "K") {
            val t = p[1].toIntOrNull() ?: return
            val id = p[2].toIntOrNull() ?: return
            MapFeeder.onAck(t, id)
        }
    }

    private val gattCallback = object : BluetoothGattCallback() {
        override fun onConnectionStateChange(g: BluetoothGatt, st: Int, newState: Int) = onCurrent(g) {
            trace("connection status=$st state=$newState")
            if (st != BluetoothGatt.GATT_SUCCESS || newState == BluetoothProfile.STATE_DISCONNECTED) {
                // Ordinary timeouts and missed callbacks do not prove the stored pairing is stale.
                if (st == 5 || st == 15) checkStaleBond(g)
                failConnection(g, "连接断开（$st）")
            } else if (newState == BluetoothProfile.STATE_CONNECTED &&
                session.move(g, Phase.CONNECTING, Phase.SERVICES, now(), 12000)) {
                screenBattery = ""; screenBatteryPct = -1
                status = "蓝牙已连接，正在读取圆屏服务…"
                try {
                    g.requestConnectionPriority(BluetoothGatt.CONNECTION_PRIORITY_HIGH)
                    if (!g.discoverServices()) failConnection(g, "无法读取圆屏服务")
                } catch (_: Exception) { failConnection(g, "读取圆屏服务失败") }
            }
        }

        override fun onServicesDiscovered(g: BluetoothGatt, st: Int) = onCurrent(g) {
            if (session.phase() != Phase.SERVICES) return@onCurrent
            trace("services status=$st")
            if (st != BluetoothGatt.GATT_SUCCESS) {
                failConnection(g, "读取圆屏服务失败（$st）"); return@onCurrent
            }
            rxChar = g.getService(SERVICE_UUID)?.getCharacteristic(RX_UUID)
            if (rxChar == null) { failConnection(g, "未找到圆屏导航服务"); return@onCurrent }
            val pairing = try { g.device.bondState != BluetoothDevice.BOND_BONDED } catch (_: Exception) { true }
            session.move(g, Phase.SERVICES, Phase.NOTIFY, now(), if (pairing) 120000 else 12000)
            status = "蓝牙已连接，正在开启圆屏通知…"
            enableNotify(g)
        }

        override fun onDescriptorWrite(g: BluetoothGatt, d: BluetoothGattDescriptor, st: Int) = onCurrent(g) {
            if (d.uuid != CCCD_UUID || d.characteristic.uuid != TX_UUID || session.phase() != Phase.NOTIFY) return@onCurrent
            trace("notify status=$st")
            if (st != BluetoothGatt.GATT_SUCCESS) {
                failConnection(g, "圆屏通知订阅失败（$st）"); return@onCurrent
            }
            session.move(g, Phase.NOTIFY, Phase.MTU, now(), 3000)
            status = "圆屏通知已开启，正在协商数据包…"
            try { if (!g.requestMtu(517)) onLinkReady(g) }
            catch (_: Exception) { onLinkReady(g) }
        }

        override fun onMtuChanged(g: BluetoothGatt, newMtu: Int, st: Int) = onCurrent(g) {
            trace("mtu status=$st size=$newMtu")
            if (st == BluetoothGatt.GATT_SUCCESS && newMtu >= 23) mtu = newMtu
            try {
                g.setPreferredPhy(BluetoothDevice.PHY_LE_2M_MASK, BluetoothDevice.PHY_LE_2M_MASK, BluetoothDevice.PHY_OPTION_NO_PREFERRED)
            } catch (_: Exception) {}
            if (session.phase() == Phase.MTU) onLinkReady(g)
        }

        override fun onPhyUpdate(g: BluetoothGatt, txPhy: Int, rxPhy: Int, st: Int) = onCurrent(g) {
            if (st == BluetoothGatt.GATT_SUCCESS) phyText = phyName(txPhy)
        }
        override fun onPhyRead(g: BluetoothGatt, txPhy: Int, rxPhy: Int, st: Int) = onCurrent(g) {
            if (st == BluetoothGatt.GATT_SUCCESS) phyText = phyName(txPhy)
        }

        @Deprecated("Android 13 以下用这个")
        override fun onCharacteristicChanged(g: BluetoothGatt, c: BluetoothGattCharacteristic) {
            @Suppress("DEPRECATION") val value = c.value?.copyOf()
            if (c.uuid == TX_UUID) onCurrent(g) { onNotify(value) }
        }
        override fun onCharacteristicChanged(g: BluetoothGatt, c: BluetoothGattCharacteristic, value: ByteArray) {
            val copy = value.copyOf()
            if (c.uuid == TX_UUID) onCurrent(g) { onNotify(copy) }
        }

        override fun onCharacteristicWrite(g: BluetoothGatt, c: BluetoothGattCharacteristic, st: Int) = onCurrent(g) {
            if (c.uuid != RX_UUID) return@onCurrent
            if (st != BluetoothGatt.GATT_SUCCESS) {
                failConnection(g, "圆屏数据发送失败（$st）"); return@onCurrent
            }
            synchronized(lock) { writing = false }
            pump()
        }
    }

    private fun phyName(p: Int) = when (p) { 1 -> "1M"; 2 -> "2M"; 3 -> "Coded"; else -> "?" }

    /** 连接完全就绪：补发最近的全部状态和图片 */
    private fun onLinkReady(g: BluetoothGatt) {
        if (!session.owns(g) || ready || session.phase() != Phase.MTU) return
        val pairing = try { g.device.bondState != BluetoothDevice.BOND_BONDED } catch (_: Exception) { true }
        session.move(g, Phase.MTU, if (verified) Phase.READY else Phase.AUTH, now(), if (verified) 0 else if (pairing) 120000 else 20000)
        ready = true
        try { g.readPhy() } catch (_: Exception) {}
        status = if (verified) "已连接（高速模式，数据包 ${maxWrite()} 字节）" else "导航通道已连接，正在确认圆屏配对…"
        trace("channel ready verified=$verified mtu=$mtu")
        // #GB? is a read-only query. Firmware only notifies CAP,BLD,1 after encrypted,
        // authenticated pairing. This recovers a U,1 missed while system HID held the link.
        capabilityQuerySent = true
        send("#GB?")
        if (verified) resendAll()
    }

    /** 补发最近的全部状态、图片、地图（连上时 / v6.4 配对验证通过时） */
    private fun resendAll() {
        if (!ready) return
        synchronized(lock) {
            queue.clear()
            bulkQ.clear()
            writing = false
            // 亮度放最前面，然后是导航和其他状态
            lastState["#b"]?.let { enqueue("#b", it.toByteArray()) }
            for ((k, v) in lastState) if (k != "#b") enqueue(k, v.toByteArray())
            for ((slot, img) in lastImg) enqueueImage(slot, img)
        }
        pump()
        MusicBridge.refresh(true)
        BootBrand.sync()
        syncClock(true)
        MapFeeder.onLinkReady()            // v2.0 地图块和路线重发
    }

    /** This command is never cached/replayed: a reconnect must use the current wall clock. */
    private fun syncClock(force:Boolean){
        if(!ready||!verified)return
        val elapsed=now();val wall=System.currentTimeMillis()
        val offset=java.util.TimeZone.getDefault().getOffset(wall)/1000
        if(!force && elapsed-clockSyncElapsed<60000 && offset==clockSyncOffset &&
            kotlin.math.abs((wall-clockSyncWall)-(elapsed-clockSyncElapsed))<2000)return
        val wire=PhoneClockWire.encode(wall,offset)?:return
        synchronized(lock){enqueue("CLOCK",wire.toByteArray(Charsets.UTF_8))}
        clockSyncElapsed=elapsed;clockSyncWall=wall;clockSyncOffset=offset
        pump()
    }

    private fun maxWrite(): Int = minOf(mtu - 3, MAX_WRITE)

    // ================= 心跳 =================

    private val heartbeat = object : Runnable {
        override fun run() {
            val connection = gatt
            if (connection != null && session.expired(now())) {
                if (session.phase() == Phase.MTU) onLinkReady(connection)
                else failConnection(connection, when (session.phase()) {
                    Phase.CONNECTING -> "连接圆屏超时"
                    Phase.SERVICES -> "读取圆屏服务超时"
                    Phase.NOTIFY -> "开启圆屏通知超时"
                    else -> "等待圆屏配对确认超时"
                })
            }
            if (ready && !verified) { capabilityQuerySent = true; send("#GB?") }
            syncClock(false)
            val idle = synchronized(lock) { queue.isEmpty() && bulkQ.isEmpty() && !writing }
            if (ready && idle && now() - lastWriteAt > HEARTBEAT_MS - 500) send("P")
            if (auto) handler.postDelayed(this, HEARTBEAT_MS)
        }
    }

    // ================= 发送 =================

    /** 文字消息的类型：同类型在队列里只保留最新一条 */
    private fun keyOf(msg: String): String = when {
        msg == "#GB?" -> "#GB?"
        // v6.3 "#D角度"、v7.2 "#L毫秒" 这类没有逗号的：按前两个字符算同一类（只留最新的）
        msg.startsWith("#") -> if (msg.contains(',')) msg.substringBefore(',') else msg.take(2)
        msg == "P" -> "P"
        else -> "NAV"                      // 导航方向和清屏 C
    }

    /** 发送一条文字消息（导航、车速、测速、车道、亮度、心跳） */
    fun send(msg: String) {
        val k = keyOf(msg)
        synchronized(lock) {
            if (msg == "C") {
                // 清屏：圆屏会清掉所有状态，这边也忘掉（亮度除外），重连后不再补发旧数据
                val b = lastState["#b"]
                lastState.clear()
                if (b != null) lastState["#b"] = b
                lastImg.clear()
                queue.removeAll { it.key != null && it.key.startsWith("B") }
            }
            if (k != "P" && k != "#GB?") lastState[k] = msg
            enqueue(k, msg.toByteArray(Charsets.UTF_8))
        }
        pump()
    }

    /** v7.9 发一次性的事件（到达统计、途经点）：不记进“重连补发”，免得重连后又放一遍特效 */
    fun sendEvent(msg: String) {
        synchronized(lock) { enqueue("EV" + msg.take(2), msg.toByteArray(Charsets.UTF_8)) }   // 先后顺序无所谓：圆屏放特效 0.9 秒后才显示统计
        pump()
    }

    // Transient lights never enter lastState/reconnect replay. Recompute TTL at write.
    fun sendTrafficLight(packet: TrafficSignalWrite) {
        if (!ready || !verified) return
        synchronized(lock) {
            queue.removeAll { it.key == "TL" }
            queue.addLast(Out("TL", byteArrayOf(), packet))
        }
        pump()
    }
    fun clearTrafficLight() {
        synchronized(lock) {
            queue.removeAll { it.key == "TL" }
            if(ready && verified) queue.addLast(Out("TL", "#TL,0".toByteArray(), expiry=now()+400))
        }
        pump()
    }
    // Transient approach frame only; no phase, countdown, persistence or replay.
    fun sendTrafficApproach(id:Int) {
        synchronized(lock) {
            queue.removeAll { it.key == "TLA" }
            if(ready && verified) queue.addLast(Out("TLA", "#TLA,$id,${if(id>0)1500 else 0}".toByteArray(), expiry=now()+400))
        }
        pump()
    }

    /** 发送一张文字图片；w = 0 表示清空这一栏 */
    fun clearWeather() {
        synchronized(lock) {
            lastState.remove("#W"); lastState.remove("#WX")
            for (slot in listOf(3, 4, 7, 8, 9, 10, 11)) lastImg.remove(slot)
            queue.removeAll { it.key == "#W" || it.key == "#WX" }
        }
        sendEvent("#WXRESET")
    }

    fun sendImage(slot: Int, w: Int, h: Int, data: ByteArray) {
        val img = Img(w, h, data)
        synchronized(lock) {
            lastImg[slot] = img
            enqueueImage(slot, img)
        }
        pump()
    }

    /**
     * v2.0 发送一份大块数据（地图块/路线）。同类型还没发完的旧数据直接作废。
     * 每片：'M' 类型 编号 总片数(2) 片序号(2) 总字节(4) 偏移(4) + 数据
     */
    fun sendBulk(type: Int, id: Int, data: ByteArray) {
        synchronized(lock) {
            bulkQ.removeAll { it.type == type }
            if (!ready) return
            val payload = maxWrite() - BULK_HEADER
            if (payload < 100) return                  // 没协商到大数据包就不发
            val parts = (data.size + payload - 1) / payload
            if (parts > 4000) return
            for (p in 0 until parts) {
                val off = p * payload
                val len = minOf(payload, data.size - off)
                val h = byteArrayOf(
                    'M'.code.toByte(), type.toByte(), id.toByte(),
                    parts.toByte(), (parts shr 8).toByte(), p.toByte(), (p shr 8).toByte(),
                    data.size.toByte(), (data.size shr 8).toByte(), (data.size shr 16).toByte(), (data.size shr 24).toByte(),
                    off.toByte(), (off shr 8).toByte(), (off shr 16).toByte(), (off shr 24).toByte()
                )
                bulkQ.addLast(BulkPkt(type, h + data.copyOfRange(off, off + len)))
            }
            bulkSentKB += data.size / 1024
        }
        pump()
    }

    /** Cancel queued map fragments while leaving route and position delivery intact. */
    fun cancelBulk(vararg types: Int) {
        synchronized(lock) { bulkQ.removeAll { it.type in types } }
    }

    fun bulkPending(): Boolean = synchronized(lock) { bulkQ.isNotEmpty() }

    /** Submission progress only; firmware ACK is the sole proof of a received map. */
    fun bulkProgress(type: Int, id: Int): Int? = synchronized(lock) {
        val packet = bulkQ.firstOrNull { it.type==type && it.data.size>=15 && (it.data[2].toInt() and 255)==(id and 255) }?.data
            ?: return@synchronized null
        val count = (packet[3].toInt() and 255) or ((packet[4].toInt() and 255) shl 8)
        val next = (packet[5].toInt() and 255) or ((packet[6].toInt() and 255) shl 8)
        if (count<=0) null else (next*100/count).coerceIn(0,99)
    }

    /** 把图片切片放进队列（需持有 lock） */
    private fun enqueueImage(slot: Int, img: Img?) {
        // 同一栏还没发出去的旧图片片段全部作废
        queue.removeAll { it.key != null && it.key.startsWith("B$slot:") }
        if (img == null) return
        val id = (++imgId[slot]) and 0xFF
        val payload = maxWrite() - IMG_HEADER
        if (img.w == 0 || img.data.isEmpty()) {
            queue.addLast(Out("B$slot:$id:0", header(slot, id, 0, 1, 0, 0, 0)))
            return
        }
        if (payload < 16) return                       // 没协商到大数据包，就不发图片
        val parts = (img.data.size + payload - 1) / payload
        if (parts > 32) return
        for (p in 0 until parts) {
            val off = p * payload
            val len = minOf(payload, img.data.size - off)
            val pkt = header(slot, id, p, parts, img.w, img.h, off) + img.data.copyOfRange(off, off + len)
            queue.addLast(Out("B$slot:$id:$p", pkt))
        }
        imgSentCount++
    }

    private fun header(slot: Int, id: Int, part: Int, parts: Int, w: Int, h: Int, off: Int): ByteArray =
        byteArrayOf(
            'B'.code.toByte(), slot.toByte(), id.toByte(), part.toByte(), parts.toByte(),
            (w and 0xFF).toByte(), (w shr 8).toByte(), h.toByte(),
            (off and 0xFF).toByte(), (off shr 8).toByte()
        )

    /** 放进队列：同类型的状态消息直接替换原位置的旧消息（需持有 lock） */
    private fun enqueue(key: String, data: ByteArray) {
        val existing = queue.firstOrNull { it.key == key }
        if (existing != null) {
            existing.data = data
        } else {
            queue.addLast(Out(key, data))
        }
        while (queue.size > 120) queue.removeFirst()
    }

    /** 如果空闲，就发队列里的下一条 */
    private fun pump() {
        synchronized(lock) {
            if (!ready) return
            val stuck = now() - writeStartedAt > 1000
            if (writing && !stuck) return
            queue.removeAll { it.expiry <= now() || it.signal?.expired(now()) == true }
            // 导航等状态消息优先，空闲时再发地图大块数据
            val next = if (verified) queue.firstOrNull() else queue.firstOrNull { it.key == "#GB?" || it.key == "P" }
            if (!verified && next == null) return
            if (next == null) {
                val b = bulkQ.firstOrNull() ?: run { writing = false; return }
                if (writeNow(b.data)) {
                    bulkQ.removeFirst()
                    writing = true
                    writeStartedAt = now()
                    lastWriteAt = writeStartedAt
                    // v4.6 速度统计：一批大块数据从第一片到最后一片
                    if (burstStart == 0L) { burstStart = writeStartedAt; burstBytes = 0L }
                    burstBytes += b.data.size
                    if (bulkQ.isEmpty()) {
                        val dt = (writeStartedAt - burstStart) / 1000.0
                        if (burstBytes >= 8 * 1024 && dt > 0.05) {
                            lastRateKBs = burstBytes / 1024.0 / dt
                            lastBurstKB = (burstBytes / 1024).toInt()
                            if (lastRateKBs > bestRateKBs) bestRateKBs = lastRateKBs
                        }
                        burstStart = 0L
                    }
                } else {
                    writing = false
                    handler.postDelayed({ pump() }, 30)
                }
                return
            }
            val wire = if(next.signal != null) {
                val encoded=next.signal.encode(now())
                if(encoded==null){queue.removeFirst();handler.post { pump() };return}
                encoded.toByteArray(Charsets.UTF_8)
            } else next.data
            if (writeNow(wire)) {
                queue.removeFirst()
                writing = true
                writeStartedAt = now()
                lastWriteAt = writeStartedAt
                if (next.key != "P" && next.key?.startsWith("B") != true) {
                    sentCount++
                    lastSentMsg = String(wire, Charsets.UTF_8)
                    lastSentAt = writeStartedAt
                }
            } else {
                writing = false
                handler.postDelayed({ pump() }, 30)   // 蓝牙忙，稍后重试
            }
        }
    }

    private fun writeNow(bytes: ByteArray): Boolean {
        val g = gatt ?: return false
        val c = rxChar ?: return false
        return try {
            if (Build.VERSION.SDK_INT >= 33) {
                g.writeCharacteristic(c, bytes, BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE) ==
                    BluetoothStatusCodes.SUCCESS
            } else {
                @Suppress("DEPRECATION")
                c.value = bytes
                c.writeType = BluetoothGattCharacteristic.WRITE_TYPE_NO_RESPONSE
                @Suppress("DEPRECATION")
                g.writeCharacteristic(c)
            }
        } catch (_: SecurityException) {
            status = "缺少蓝牙权限"
            false
        }
    }

    /** 给界面显示的统计文字 */
    fun statsText(): String {
        if (sentCount == 0) return "还没发送过指令"
        val ago = (now() - lastSentAt) / 1000.0
        val q = synchronized(lock) { queue.size }
        val bq = synchronized(lock) { bulkQ.size }
        return "已发送 $sentCount 条，文字图片 $imgSentCount 张，队列 $q，地图待发 $bq 片（累计 $bulkSentKB KB）\n" +
            "最近发送：$lastSentMsg（${"%.1f".format(ago)} 秒前）\n" +
            speedText()
    }

    /** v4.6 蓝牙模式和实测传输速度 */
    fun speedText(): String {
        val r = if (lastRateKBs > 0) "最近一批 $lastBurstKB KB，${"%.0f".format(lastRateKBs)} KB/秒（最快 ${"%.0f".format(bestRateKBs)} KB/秒）"
                else "还没测到（发一次地图块或路线后显示）"
        return "蓝牙 $phyText 模式，数据包 ${maxWrite()} 字节；$r"
    }

    // ================= v6.4 配对 =================

    private var bondWatching = false
    private val bondReceiver = object : BroadcastReceiver() {
        override fun onReceive(c: Context, i: Intent) {
            val dev: BluetoothDevice? = if (Build.VERSION.SDK_INT >= 33)
                i.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE, BluetoothDevice::class.java)
            else @Suppress("DEPRECATION") i.getParcelableExtra(BluetoothDevice.EXTRA_DEVICE)
            val mine = gatt?.device ?: return
            if (dev == null || dev.address != mine.address) return
            val prev = i.getIntExtra(BluetoothDevice.EXTRA_PREVIOUS_BOND_STATE, BluetoothDevice.ERROR)
            when (i.getIntExtra(BluetoothDevice.EXTRA_BOND_STATE, BluetoothDevice.ERROR)) {
                BluetoothDevice.BOND_BONDING ->
                    pairHint = "正在配对：在手机弹出的配对框里输入圆屏上显示的 6 位数字（没弹出就下拉通知栏看看）"
                BluetoothDevice.BOND_BONDED -> {
                    pairHint = "配对成功，正在验证…"
                    if (ready && !verified) { capabilityQuerySent = true; send("#GB?") }
            syncClock(false)
                }
                BluetoothDevice.BOND_NONE -> if (prev == BluetoothDevice.BOND_BONDING)
                    pairHint = "配对没成功。圆屏显示 LOCKED 的话：重启圆屏，2 分钟内再连"
            }
        }
    }

    private fun registerBondWatch(c: Context) {
        if (bondWatching) return
        bondWatching = true
        val f = IntentFilter(BluetoothDevice.ACTION_BOND_STATE_CHANGED)
        try {
            if (Build.VERSION.SDK_INT >= 33) c.registerReceiver(bondReceiver, f, Context.RECEIVER_EXPORTED)
            else c.registerReceiver(bondReceiver, f)
        } catch (_: Exception) { bondWatching = false }
    }

    /** v6.9 让圆屏忘掉“别的”手机（固件 v6.6 起保留正在连着的这台；旧的 v6.5 固件会全部忘掉） */
    fun forgetOthers() {
        send("#UNPAIR")
        pairHint = ""
    }

    /**
     * v6.9 手机上还记着和圆屏的配对，圆屏却已经忘了（比如用 v6.5 固件点过“全部忘掉”、或者重新刷了固件）：
     * 手机拿旧钥匙去加密，圆屏不认，连上就断。连续两次这样，就先试着自动删掉手机上的旧配对；
     * 删不掉（新安卓不让 App 删）就提示到系统蓝牙设置里手动“取消配对”。
     */
    private fun checkStaleBond(g: BluetoothGatt) {
        if (verified) { staleCount = 0; return }
        val dev = g.device ?: return
        val bonded = try { dev.bondState == BluetoothDevice.BOND_BONDED } catch (_: SecurityException) { false }
        if (!bonded) { staleCount = 0; return }
        staleCount++
        if (staleCount < 2) return
        val removed = try { dev.javaClass.getMethod("removeBond").invoke(dev) as? Boolean ?: false } catch (_: Throwable) { false }
        if (removed) {
            needUnbond = false
            staleCount = 0
            pairHint = "圆屏不认识这台手机的旧配对了，已自动清掉，正在重连：手机弹出配对框时输入圆屏上的 6 位数字"
        } else {
            needUnbond = true
            pairHint = "圆屏不认识这台手机的旧配对了：点这里打开系统蓝牙设置，在已配对设备里对 ${screenName.ifEmpty { "MotoNav" }} 点「取消配对 / 忽略」，" +
                "再回来会自动重连，输入圆屏上的配对码"
        }
    }

}
