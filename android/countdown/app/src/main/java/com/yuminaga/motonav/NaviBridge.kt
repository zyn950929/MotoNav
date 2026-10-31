package com.yuminaga.motonav

import android.content.Context
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import com.amap.api.location.AMapLocationClient
import com.amap.api.maps.MapsInitializer
import com.amap.api.navi.AMapNavi
import com.amap.api.navi.AMapNaviListener
import com.amap.api.navi.AmapNaviPage
import com.amap.api.navi.AmapNaviParams
import com.amap.api.navi.AmapNaviType
import com.amap.api.navi.AmapPageType
import com.amap.api.navi.INaviInfoCallback
import com.amap.api.navi.NaviSetting
import com.amap.api.navi.model.AMapNaviCameraInfo
import com.amap.api.navi.model.AMapNaviLocation
import com.amap.api.navi.model.NaviInfo
import com.amap.api.services.core.ServiceSettings
import java.lang.reflect.Proxy
import java.util.Calendar
import kotlin.math.max
import kotlin.math.roundToInt

/**
 * 版本：跟随 App 版本，见 MainActivity.kt 里的 APP_VERSION（当前 v4.2）
 *
 * 打开高德导航组件，并在后台“旁听”导航数据，转成指令发给圆屏：
 * - 转向和距离："R,300"（两次高德数据之间按车速推算，倒数平滑，v1.2 起）
 * - v1.7 新增：车速 "#v"、前方测速 "#k"、车道 "#j"、
 *   路名和底部“剩余里程·到达时间·红绿灯数”（渲染成文字图片发送）
 * - v1.7.1 车道数量改用高德给的 laneCount（修复显示一大排直行车道）；
 *   测速牌改为按位置持续跟踪到经过为止；文字图片缩窄，避免碰到圆环；
 *   App 上显示电子眼原始数据，方便排查
 * - v1.8 按路线形状算出路口的实际转弯角度，发给圆屏："L,153,-128"（第三个数是角度，
 *   负数向左、正数向右），锐角弯、缓弯的箭头都跟实际角度一致
 * - v2.0 小地图：定位和路线交给 MapFeeder（下载附近地图、发送位置和路线）
 * - v3.0 自己的导航界面（RoutePlanActivity 选路线、NavActivity 导航）：
 *   这里额外把转向、距离、路名、车道、测速等整理成 ui* 字段给界面显示；
 *   算路结果通过 routeCallback 转给选路线页面。高德原版组件仍保留（调试区可打开）。
 * - v3.1 行程信息 "#e,剩余米,到达时间HHMM,红绿灯数"；前方红绿灯距离 "#g,米"（按路线小段的红绿灯标记计算）
 * - v3.3 路线上所有红绿灯的位置随路线一起发给圆屏，画在小地图上
 * - v3.4 前方电子眼（测速、拍照）的位置发给圆屏 "#c,…"，画在小地图上
 * - v3.8 高德的播报文字（onGetNavigationText）交给 NaviVoice，用手机系统语音念
 * - v4.2 电子眼按类型换算成图标编号（CamIcons）发给圆屏，圆屏按类型画不同的图标（固件 v3.8）
 * - v4.5 按导入的分享路线导航时，偏航重算交给 ShareGuide 再按原路线规划一次
 * - v4.7 巡航模式（startCruise/stopCruise）；道路安全提示（交通设施回调）和电子眼一起发给圆屏
 * - v4.9 重新规划中 "#y"、GPS 信号弱 "#x"、更快路线 "#h"（导航页可一键切换）、路况颜色 "#u"
 */
object NaviBridge {
    @Volatile var lastInfo = "（还没开始导航）"
    @Volatile var extraInfo = ""
    @Volatile var laneRaw = "（还没收到车道信息）"
    @Volatile var camRaw = "（还没收到电子眼信息）"

    // ---- v3.0 给自己的导航界面用（界面每 0.2 秒读一次）----
    @Volatile var uiDir = ""                  // 圆屏方向代码：L/R/SL/SR/U/S/A
    @Volatile var uiAngle: Int? = null        // 路口实际转弯角度
    @Volatile var uiDist = -1                 // 离路口距离（米，已推算平滑）
    @Volatile var uiNextRoad = ""
    @Volatile var uiCurRoad = ""
    @Volatile var uiRemainM = 0
    @Volatile var uiRemainS = 0
    @Volatile var uiLights = 0
    @Volatile var uiSpeed = -1                // 车速 km/h（-1 = 还没有）
    @Volatile var uiCamLimit = 0              // 前方测速限速（0 = 没有）
    @Volatile var uiCamDist = 0
    @Volatile var uiLanes: IntArray = IntArray(0)   // 每条车道两个数：背景码、推荐码（255 = 不推荐）
    @Volatile var uiArrived = false
    // v7.9 到达特效用的行程统计：从开始导航算起骑了多远、多久、最快多少
    private var tripStartAt = 0L
    private var tripMeters = 0.0
    private var tripMaxKmh = 0
    private var tripLat = Double.NaN; private var tripLng = Double.NaN
    /** v7.9 这次导航的途经点是不是用户自己加的（按高德分享路线走时，途经点是 App 自动挑的，到了不放特效） */
    @Volatile var directRouteMode = LocalRouting.DRIVE
    @Volatile var realWaypoints = false
    @Volatile var uiHasInfo = false           // 已经收到过导航信息
    @Volatile var uiLightDist = -1            // v3.1 前方红绿灯距离（米，-1 = 300 米内没有）
    /** 算路结果回调（选路线页面设置）：成功与否、高德给的结果对象 */
    private val routeResults = RouteResultSlot<(Boolean, Any?) -> Unit>()
    fun listenForRoute(owner: Any, callback: (Boolean, Any?) -> Unit) = routeResults.set(owner,callback)
    fun releaseRoute(owner: Any) = routeResults.clear(owner)
    fun ownsRoute(owner: Any) = routeResults.owns(owner)
    @Volatile private var usingComponent = false   // v3.8 正在用高德原版界面（它自己播报，系统语音不再重复念）
    private var lastSent = ""
    private var navi: AMapNavi? = null
    private var listener: AMapNaviListener? = null

    private val handler = Handler(Looper.getMainLooper())
    private const val TICK_MS = 100L          // 推算频率：每秒 10 次（v7.2 原来 5 次，数字倒数更顺）
    /** v7.2 预判提前量（秒）：显示“再过一点点”的距离，抵消 GPS 和传输的延迟（和小地图共用一个设置） */
    private fun leadS() = MapFeeder.leadMs / 1000.0
    private const val MAX_PREDICT_S = 1.2     // v1.8.1：超过 1.2 秒没新数据就不再推算（原 3 秒），减少急停时的误差
    // v2.2 测速提前提醒：按车速留出约 20 秒反应时间，最少 500 米，最多 1000 米
    //     （60km/h ≈ 500 米，120km/h ≈ 670 米）
    private fun cameraRange(): Int = (speedKmhNow / 3.6 * 20).toInt().coerceIn(500, 1000)
    @Volatile private var speedKmhNow = 0
    private const val CAMERA_TIMEOUT_MS = 90_000L   // 90 秒没有任何更新就不再显示测速牌

    // ---- 推算用的状态（只在主线程里读写）----
    private var active = false
    private val arrivalSession = ArrivalPolicy()
    private var curDir = ""
    private var curAngle: Int? = null          // v1.8 当前路口的实际转弯角度（度），算不出来时为 null
    private var angleKey = ""                  // 缓存：哪条路线的哪个路段
    private var lastRouteKey = ""              // v2.0 已发给小地图的路线
    private var curStep = -1
    private var baseDist = 0
    private var baseAt = 0L
    private var speed = 0.0                   // 估算车速（米/秒），用于距离推算
    private var shownDir = ""
    private var shownDist = -1
    private var lastUpdateAt = 0L
    private var lastGap = 0.0

    // ---- v1.7 行车信息 ----
    private var lastSpeedMsg = ""
    private var lastCamMsg = ""
    private var lastCamAt = 0L
    // v1.7.1 正在跟踪的测速点
    private var camLimit = 0
    private var camLat = 0.0
    private var camLng = 0.0
    private var camBaseDist = 0
    private var camMinDist = Int.MAX_VALUE
    private var myLat = Double.NaN
    private var myLng = Double.NaN
    fun trafficLightPosition()=doubleArrayOf(myLat,myLng)
    fun trafficLightContextActive()=active && uiHasInfo && !uiArrived && !rerouting && !gpsWeakSent &&
        arrivalSession.acceptsGuidance(arrivalSession.generation()) && !navigationIsSimulation()
    private var newLaneApi = false
    private var lastLaneMsg = ""
    private var lastRoadText = ""
    private var lastBottomText = ""
    // v3.1 行程信息、前方红绿灯
    private var lastTripMsg = ""
    private var lastLightMsg = ""
    private var lastCamMapMsg = ""           // v3.4 小地图上的电子眼
    private var lightBase = -1                // 最近一次算出的前方红绿灯距离（米，-1 = 没有）
    private var lightAt = 0L
    private var lightApproachKey = ""
    private const val LIGHT_SHOW_M = 300      // 300 米以内才在圆屏上显示

    private fun now() = SystemClock.elapsedRealtime()

    /** Route-link traffic-light distance only; turn distance is not a substitute. */
    fun trafficLightOverlayDistance(): Int =
        if (TrafficSignalPolicy.isNear(
            active && arrivalSession.acceptsGuidance(arrivalSession.generation()),
            uiHasInfo, uiArrived,
            rerouting || (gpsWeakSent && !arrivalSession.isSimulation(arrivalSession.generation())),
            uiLightDist, lastUpdateAt, now()
        )) uiLightDist else -1

    // The signal-bearing route link identifies the approach independently of
    // transient arrow/distance changes. Empty means no reliable link was found.
    fun trafficLightApproachKey() = lightApproachKey
    // Alignment compares received guidance, never the deliberately advanced UI.
    fun trafficLightGuidanceTurnMetres() = baseDist

    /** Diagnostic values only. Does not modify navigation, GPS, or light snapshots. */
    fun trafficLightOverlayDebug() = org.json.JSONObject()
        .put("active",active).put("hasInfo",uiHasInfo).put("arrived",uiArrived)
        .put("sessionGuidance",arrivalSession.acceptsGuidance(arrivalSession.generation()))
        .put("simulation",arrivalSession.isSimulation(arrivalSession.generation()))
        .put("rerouting",rerouting).put("gpsWeak",gpsWeakSent)
        .put("guidanceAgeMs",now()-lastUpdateAt).put("rawMetres",uiLightDist)
        .put("eligibleMetres",trafficLightOverlayDistance())
        .put("approachKey",lightApproachKey).put("rawTurnMetres",baseDist)

    /** 高德要求：使用前先声明隐私政策已告知并同意（自己用的 App，直接设为同意） */
    fun initPrivacy(context: Context) {
        val c = context.applicationContext
        NaviSetting.updatePrivacyShow(c, true, true)
        NaviSetting.updatePrivacyAgree(c, true)
        MapsInitializer.updatePrivacyShow(c, true, true)
        MapsInitializer.updatePrivacyAgree(c, true)
        AMapLocationClient.updatePrivacyShow(c, true, true)
        AMapLocationClient.updatePrivacyAgree(c, true)
        ServiceSettings.updatePrivacyShow(c, true, true)
        ServiceSettings.updatePrivacyAgree(c, true)
    }

    /** v3.0 准备一次新的导航：拿到高德导航对象，挂上我们的“旁听”回调，清空上次的状态 */
    fun attach(context: Context): AMapNavi? {
        if(SingleEngineSession.owns) return null // Never construct the second engine while the native-only session owns guidance.
        routeResults.reset()
        CarNavSync.stop()
        WebMotorGuide.stop()
        val c = context.applicationContext
        endCruiseQuietly()                                                       // v4.7 开始导航前先结束巡航
        // Close the previous trip BEFORE calculating/arming a new one. Resetting
        // only our UI leaves the SDK emulator running and a later start may fail.
        arrivalSession.cancel(arrivalSession.generation())
        listener?.let { old -> try { navi?.removeAMapNaviListener(old) } catch (_: Exception) {} }
        listener=null
        try { navi?.stopNavi() } catch (_: Exception) {}
        resetState()
        val epoch = arrivalSession.prepare()
        android.util.Log.i("MotoNavLifecycle","prepare epoch=$epoch")
        try {
            val n = AMapNavi.getInstance(c)
            listener?.let { old -> try { navi?.removeAMapNaviListener(old) } catch (_: Exception) {} }
            val l = makeNaviListener(epoch)
            n.addAMapNaviListener(l)
            navi = n
            listener = l
        } catch (e: Exception) {
            arrivalSession.cancel(epoch)
            lastInfo = "导航初始化失败：${e.message}"
            return null
        }
        MapFeeder.onNaviStart()
        usingComponent = false
        return navi
    }

    /** v3.0 退出自己的导航界面：停止导航，圆屏回到待机 */
    fun navigationEpoch(): Long = arrivalSession.generation()
    fun hasActiveNavigation(): Boolean = !cruising && !uiArrived && arrivalSession.acceptsGuidance(arrivalSession.generation())
    fun navigationIsSimulation(): Boolean = arrivalSession.isSimulation(arrivalSession.generation())

    fun exitNavi(expectedEpoch: Long = arrivalSession.generation()) {
        android.util.Log.i("MotoNavLifecycle","exit-request epoch=$expectedEpoch current=${arrivalSession.generation()}")
        if (!arrivalSession.isCurrent(expectedEpoch)) return
        val epoch = expectedEpoch
        CarNavSync.stop(epoch)
        // stopNavi may synchronously emit onEndEmulatorNavi. Close before asking the SDK to stop.
        arrivalSession.cancel(epoch)
        try { navi?.stopNavi() } catch (_: Exception) {}
        ShareGuide.stop()                                                        // v4.5
        handler.post { if (arrivalSession.isCurrent(epoch)) resetState() }
        finish("C", "已退出导航", epoch)
    }

    /** Arm before startNavi so its synchronous callbacks belong to a running trip. */
    fun beginNavigation(emulate: Boolean): Boolean {
        if (SingleEngineSession.owns) return false
        val started=arrivalSession.begin(arrivalSession.generation(),emulate)
        if(started) android.util.Log.i("MotoNavArrival","begin session=${arrivalSession.generation()} simulation=$emulate")
        return started
    }

    fun navigationStartFailed(reason:String="导航启动失败") {
        android.util.Log.i("MotoNavLifecycle","start-failed epoch=${arrivalSession.generation()} reason=$reason")
        lastInfo=reason
        arrivalSession.cancel(arrivalSession.generation())
        RideRecorder.stop()
        active=false
        handler.removeCallbacks(tick)
    }

    /** Close SDK callbacks before the independent web-motor tracker owns BLE and map output. */
    fun prepareWebMotorNavigation() {
        CarNavSync.stop()
        arrivalSession.cancel(arrivalSession.generation())
        endCruiseQuietly()
        try { navi?.stopNavi() } catch (_: Exception) {}
        ShareGuide.stop()
        RideRecorder.stop()
        routeResults.reset()
        resetState()
        // Invalidate already queued SDK finish/reset jobs from the previous session as well.
        val closedEpoch = arrivalSession.prepare()
        arrivalSession.cancel(closedEpoch)
    }

    fun releaseForSingleEngine() {
        prepareWebMotorNavigation()
        handler.removeCallbacks(tick)
        AMapNavi.destroy()
        navi=null; listener=null
        check(AMapNavi.isDestroyed() && !AMapNavi.isNaviStarted()) { "原导航引擎尚未释放，未启动单引擎实验" }
    }

    fun preparedRouteDiagnostics()=org.json.JSONObject().put("points",runCatching{navi?.naviPath?.coordList?.size?:0}.getOrDefault(0)).put("mode",directRouteMode)
    fun engineAllocatedForDiagnostics()=!AMapNavi.isDestroyed()
    fun engineRunningForDiagnostics()=AMapNavi.isNaviStarted()

    /** 打开高德原版导航组件：起点默认是“我的位置”，在组件里搜索目的地 */
    fun startNaviComponent(context: Context) {
        if(SingleEngineSession.owns){NavigationUi.returnToNavigation(context);return}
        val c = context.applicationContext
        if (attach(c) == null) return
        usingComponent = true
        lastInfo = "导航组件已打开，请搜索目的地"
        val params = AmapNaviParams(null, null, null, AmapNaviType.DRIVER, AmapPageType.ROUTE)
        AmapNaviPage.getInstance().showRouteActivity(c, params, makePageCallback())
    }

    /** 高德转向图标编号 → 圆屏方向代码 */
    fun iconToDir(icon: Int): String = when (icon) {
        2, 6, 21 -> "L"     // 左转、左后方、环岛左转
        3, 7, 22 -> "R"     // 右转、右后方、环岛右转
        4, 65 -> "SL"       // 左前方、靠左
        5, 66 -> "SR"       // 右前方、靠右
        8, 19, 24 -> "U"    // 掉头
        15 -> "A"           // 到达目的地
        else -> "S"         // 直行及其他
    }

    // ================= v1.8：路口实际转弯角度 =================

    /**
     * 用路线坐标算当前路段终点（即下一个路口）的转弯角度：
     * 进路口前 30 米的方向 → 出路口后 30 米的方向，两者之差。负数 = 向左，正数 = 向右。
     * 只对左转/右转/左前/右前计算；算不出来或方向对不上时返回 null（圆屏用默认角度）。
     */
    private fun turnAngle(step: Int, icon: Int, dir: String): Int? {
        if (dir != "L" && dir != "R" && dir != "SL" && dir != "SR") return null
        val fallback = when (icon) { 6 -> -135; 7 -> 135; else -> null }   // 左后方/右后方
        try {
            val steps = navi?.naviPath?.steps ?: return fallback
            if (step < 0 || step + 1 >= steps.size) return fallback
            val inC = steps[step].coords ?: return fallback
            val outC = steps[step + 1].coords ?: return fallback
            if (inC.size < 2 || outC.size < 2) return fallback
            val j = inC.last()
            // 往回走 30 米找进路口方向的起点
            var a = inC[inC.size - 2]
            var acc = 0.0
            for (i in inC.size - 1 downTo 1) {
                acc += distM(inC[i].latitude, inC[i].longitude, inC[i - 1].latitude, inC[i - 1].longitude)
                a = inC[i - 1]
                if (acc >= 30) break
            }
            // 往前走 30 米找出路口方向的终点
            var b = outC[1]
            acc = 0.0
            for (i in 0 until outC.size - 1) {
                acc += distM(outC[i].latitude, outC[i].longitude, outC[i + 1].latitude, outC[i + 1].longitude)
                b = outC[i + 1]
                if (acc >= 30) break
            }
            val bin = bearing(a.latitude, a.longitude, j.latitude, j.longitude)
            val bout = bearing(j.latitude, j.longitude, b.latitude, b.longitude)
            var t = bout - bin
            while (t > 180) t -= 360
            while (t <= -180) t += 360
            val left = dir == "L" || dir == "SL"
            // 方向对不上（比如高德说左转，几何上却是右转）就不用
            if (left && t > -10) return fallback
            if (!left && t < 10) return fallback
            val mag = Math.abs(t).coerceIn(20.0, 150.0)
            val r = (Math.round(mag / 5) * 5).toInt()          // 取到 5 度，避免箭头抖动
            return if (left) -r else r
        } catch (_: Exception) {
            return fallback
        }
    }

    /** 方位角：0 = 正北，顺时针为正（度） */
    private fun bearing(lat1: Double, lng1: Double, lat2: Double, lng2: Double): Double {
        val k = Math.PI / 180
        val x = (lng2 - lng1) * Math.cos((lat1 + lat2) / 2 * k)
        val y = lat2 - lat1
        return Math.toDegrees(Math.atan2(x, y))
    }

    /** 距离取整：1 公里以上精确到 100 米，1 公里以内精确到 1 米 */
    private fun roundDist(d: Int): Int = if (d >= 1000) (d + 50) / 100 * 100 else d

    private fun resetState() {
        android.util.Log.i("MotoNavLifecycle","reset epoch=${arrivalSession.generation()} via="+
            Throwable().stackTrace.drop(1).take(4).joinToString(" > "){it.className+"."+it.methodName})
        handler.removeCallbacks(tick)
        tripStartAt = 0L; tripMeters = 0.0; tripMaxKmh = 0; tripLat = Double.NaN; tripLng = Double.NaN   // v7.9
        active = false
        curDir = ""; curStep = -1; curAngle = null; angleKey = ""; lastRouteKey = ""
        baseDist = 0; baseAt = 0L; speed = 0.0
        shownDir = ""; shownDist = -1
        lastUpdateAt = 0L; lastGap = 0.0
        lastSent = ""
        lastSpeedMsg = ""; lastCamMsg = ""; lastCamAt = 0L; lastLaneMsg = ""
        camLimit = 0; camMinDist = Int.MAX_VALUE
        lastRoadText = ""; lastBottomText = ""
        extraInfo = ""
        uiDir = ""; uiAngle = null; uiDist = -1; uiNextRoad = ""; uiCurRoad = ""
        uiRemainM = 0; uiRemainS = 0; uiLights = 0; uiCamLimit = 0; uiCamDist = 0
        uiLanes = IntArray(0); uiArrived = false; uiHasInfo = false
        lastTripMsg = ""; lastLightMsg = ""; lightBase = -1; lightAt = 0L; uiLightDist = -1
        rerouting = false; gpsWeakSent = false; lastTrafficMsg = ""; lastTrafficAt = 0L    // v4.9
        suggestNewId = null; uiSuggestMin = 0
        lastCamMapMsg = ""
        navCamPts = emptyList(); cruiseCamPts = emptyList(); facPts.clear()      // v4.7
    }

    // ================= 转向和距离 =================

    /** 高德给了一次导航信息（切到主线程处理） */
    private fun onNaviInfo(info: NaviInfo, epoch: Long) {
        val icon = info.iconType
        val raw = info.curStepRetainDistance
        val step = info.curStep
        val nextRoad = info.nextRoadName ?: ""
        val curRoad = info.currentRoadName ?: ""
        val remainM = info.pathRetainDistance
        val remainS = info.pathRetainTime
        val lights = info.routeRemainLightCount
        handler.post {
            if (!arrivalSession.acceptsGuidance(epoch)) return@post
            val t = now()
            if (lastUpdateAt != 0L) {
                lastGap = (t - lastUpdateAt) / 1000.0
                if (step == curStep && lastGap in 0.2..5.0 && raw <= baseDist) {
                    val v = (baseDist - raw) / lastGap
                    if (v in 0.0..70.0) speed = if (speed == 0.0) v else speed * 0.5 + v * 0.5
                }
            }
            lastUpdateAt = t
            if(icon==15 && curDir!="A") android.util.Log.i("MotoNavArrival","destination-ahead session=$epoch remaining=$remainM stepDistance=$raw wire=S")
            curDir = iconToDir(icon)
            val path = navi?.naviPath
            val key = "${path?.pathid}:$step:$icon"
            // v2.0 路线变化（新规划、偏航重算）时把整条路线发给小地图
            if (path != null) {
                val pk = "${path.pathid}:${path.allLength}"
                if (pk != lastRouteKey) {
                    try {
                        val cl = path.coordList
                        if (cl != null && cl.size >= 2 &&
                            MapFeeder.onRoute(pk, cl.map { doubleArrayOf(it.latitude, it.longitude) }, routeLights())) {
                            lastRouteKey = pk
                            lastTrafficAt = 0L; pushTraffic()                  // v4.9 新路线：路况也重新发
                        }
                    } catch (_: Exception) {}
                }
            }
            if (key != angleKey) {
                angleKey = key
                curAngle = turnAngle(step, icon, curDir)
            }
            curStep = step
            baseDist = raw
            baseAt = t
            lastInfo = "图标 $icon → $curDir（角度 ${curAngle ?: "无"}），还有 $raw 米，下一条路：$nextRoad\n" +
                "高德数据间隔：${"%.1f".format(lastGap)} 秒"
            pushNav(curDir, max(0, (raw - speed * leadS()).roundToInt()), fromRealData = true)   // v7.2 真实数据也加提前量
            updateTexts(curDir, curRoad, nextRoad, remainM, remainS, lights)
            uiAngle = curAngle; uiNextRoad = nextRoad; uiCurRoad = curRoad          // v3.0
            lightBase = nextLightDist(step, Refl.int(info, "getCurLink"), raw) ?: -1  // v3.1
            lightAt = t
            pushLight(lightBase)
            uiRemainM = remainM; uiRemainS = remainS; uiLights = lights; uiHasInfo = true
            if (!active) {
                active = true
                if (tripStartAt == 0L) tripStartAt = SystemClock.elapsedRealtime()   // v7.9
                handler.postDelayed(tick, TICK_MS)
            }
        }
    }

    /** 两次真实数据之间按车速推算距离；顺便检查测速提示是否过期 */
    private val tick = object : Runnable {
        override fun run() {
            if (!active || !arrivalSession.acceptsGuidance(arrivalSession.generation())) return
            val elapsed = (now() - baseAt) / 1000.0
            if (speed > 0.5 && elapsed < MAX_PREDICT_S) {
                val est = max(0, (baseDist - speed * (elapsed + leadS())).roundToInt())        // v7.2 加提前量
                pushNav(curDir, est, fromRealData = false)
                if (lightBase >= 0) pushLight(max(0, (lightBase - speed * ((now() - lightAt) / 1000.0 + leadS())).roundToInt()))
            }
            trackCamera()
            if (now() - lastTrafficAt > 60_000) pushTraffic()                      // v4.9 每分钟刷新一次路况
            handler.postDelayed(this, TICK_MS)
        }
    }

    private fun pushNav(dir: String, dist: Int, fromRealData: Boolean) {
        if (!arrivalSession.acceptsGuidance(arrivalSession.generation())) return
        val d = roundDist(dist)
        // 同一方向下，推算值不让数字往回跳（真实数据来了允许小幅修正）
        if (dir == shownDir && shownDist >= 0 && d > shownDist) {
            if (!fromRealData || d - shownDist < 20) return
        }
        shownDir = dir
        shownDist = d
        uiDir = dir; uiDist = d                                                  // v3.0
        // Keep the destination flag on the phone, but reserve wire A,0 for actual completion.
        val wireDir=ArrivalPolicy.guidanceDirection(dir)
        send(if (curAngle != null) "$wireDir,$d,$curAngle" else "$wireDir,$d")
    }

    private fun send(msg: String) {
        if (msg != lastSent) {
            lastSent = msg
            BleLink.send(msg)
        }
    }

    /** v7.9 累计行程：两次定位之间的直线距离（一次跳太远的当成定位漂移，不算） */
    private fun addTrip(lat: Double, lng: Double, kmh: Int) {
        if (!tripLat.isNaN()) {
            val r = FloatArray(1)
            android.location.Location.distanceBetween(tripLat, tripLng, lat, lng, r)
            if (r[0] < 300f) tripMeters += r[0]
        }
        tripLat = lat; tripLng = lng
        if (kmh in 1..250 && kmh > tripMaxKmh) tripMaxKmh = kmh
    }

    private fun finish(msg: String, info: String, epoch: Long = arrivalSession.generation()) {
        android.util.Log.i("MotoNavLifecycle","finish-request epoch=$epoch current=${arrivalSession.generation()} msg=$msg reason=$info")
        handler.post {
            if (!arrivalSession.isCurrent(epoch)) return@post
            val arrived=msg.startsWith("A")
            if (arrived) {
                if (!arrivalSession.arrive(epoch)) return@post
                android.util.Log.i("MotoNavArrival","arrived session=$epoch event=$info wire=A,0")
                uiArrived=true
                if (tripStartAt>0L) {
                    val min=((SystemClock.elapsedRealtime()-tripStartAt)/60000L).toInt()
                    BleLink.sendEvent("#O${tripMeters.roundToInt()}/$min/$tripMaxKmh")
                }
            } else arrivalSession.cancel(epoch)
            CarNavSync.stop(epoch)
            RideRecorder.stop()
            ShareGuide.stop()
            active = false
            handler.removeCallbacks(tick)
            lastInfo = info
            shownDir = ""; shownDist = -1
            send(msg)
        }
    }

    // ================= v1.7：路名和底部信息（文字图片） =================

    private fun updateTexts(dir: String, curRoad: String, nextRoad: String,
                            remainM: Int, remainS: Int, lights: Int) {
        val road = when {
            dir == "A" -> "前方为目的地"
            dir == "S" && curRoad.isNotBlank() -> "沿 $curRoad 直行"
            nextRoad.isNotBlank() -> "进入 $nextRoad"
            else -> ""
        }
        if (road != lastRoadText) {
            lastRoadText = road
            sendText(0, road, ROAD_MAX_W, 26f, 18f)
        }

        val km = if (remainM >= 1000) "%.1fkm".format(remainM / 1000.0) else "${remainM}m"
        val cal = Calendar.getInstance().apply { add(Calendar.SECOND, remainS) }
        val eta = "%02d:%02d".format(cal.get(Calendar.HOUR_OF_DAY), cal.get(Calendar.MINUTE))
        val bottom = buildString {
            append("$km · ${eta}到达")
            if (lights > 0) append(" · ${lights}个红绿灯")
        }
        if (bottom != lastBottomText) {
            lastBottomText = bottom
            sendText(1, bottom, BOTTOM_MAX_W, 19f, 15f)
        }
        // 圆屏按实际米数推进行程环；底部文字继续由原来的格式控制精度。
        val rm = max(0, remainM)
        val trip = "#e,$rm,${cal.get(Calendar.HOUR_OF_DAY) * 100 + cal.get(Calendar.MINUTE)},${max(0, lights)}"
        if (trip != lastTripMsg) {
            lastTripMsg = trip
            BleLink.send(trip)
        }
        extraInfo = "路名：$road\n底部：$bottom"
    }

    // ================= v3.1：前方红绿灯 =================

    /**
     * 算到前方下一个红绿灯还有多少米（1 公里内找不到返回 null）。
     * 高德路线由“路段(step) → 小段(link)”组成，每个 link 标明终点有没有红绿灯；
     * 当前 link 剩余长度 = 当前路段剩余距离 − 本路段后面几个 link 的长度。
     * 不同版本 SDK 字段名可能不同，全部用反射读，读不到就不显示。
     */
    private fun nextLightDist(step: Int, curLink: Int?, stepRemain: Int): Int? {
        lightApproachKey=""
        if (curLink == null || curLink < 0) return null
        try {
            val steps = navi?.naviPath?.steps ?: return null
            if (step < 0 || step >= steps.size) return null
            fun links(i: Int): List<*>? = Refl.call(steps[i], "getLinks").second as? List<*>
            fun len(l: Any?): Int = Refl.int(l, "getLength") ?: 0
            fun light(l: Any?): Boolean = Refl.call(l, "getTrafficLights").second == true
            var ls = links(step) ?: return null
            if (curLink >= ls.size) return null
            var after = 0
            for (k in curLink + 1 until ls.size) after += len(ls[k])
            var acc = max(0, stepRemain - after)
            if (light(ls[curLink])) {
                lightApproachKey="${navigationEpoch()}:${navi?.naviPath?.pathid}:$step:$curLink"
                return acc
            }
            var s = step
            var k = curLink + 1
            while (acc <= 1000) {
                if (k >= ls.size) {
                    s++
                    if (s >= steps.size) return null
                    ls = links(s) ?: return null
                    k = 0
                    continue
                }
                acc += len(ls[k])
                if (light(ls[k])) {
                    lightApproachKey="${navigationEpoch()}:${navi?.naviPath?.pathid}:$s:$k"
                    return acc
                }
                k++
            }
        } catch (_: Exception) {}
        return null
    }

    /** v3.3 整条路线上所有红绿灯的位置（纬度, 经度）：有红绿灯的小段，取它的终点 */
    private fun routeLights(): List<DoubleArray> {
        val out = ArrayList<DoubleArray>()
        try {
            val steps = navi?.naviPath?.steps ?: return out
            for (st in steps) {
                val ls = Refl.call(st, "getLinks").second as? List<*> ?: continue
                for (l in ls) {
                    if (Refl.call(l, "getTrafficLights").second != true) continue
                    val cs = Refl.call(l, "getCoords").second as? List<*> ?: continue
                    val last = cs.lastOrNull() ?: continue
                    val la = Refl.call(last, "getLatitude").second as? Double ?: continue
                    val lo = Refl.call(last, "getLongitude").second as? Double ?: continue
                    out.add(doubleArrayOf(la, lo))
                }
            }
        } catch (_: Exception) {}
        return out
    }

    private fun pushLight(d: Int) {
        val show = d in 0..LIGHT_SHOW_M
        uiLightDist = if (show) d else -1
        val m = if (show) "#g,${(d + 5) / 10 * 10}" else "#g"
        if (m != lastLightMsg) {
            lastLightMsg = m
            BleLink.send(m)
        }
    }

    // 文字图片最大宽度（像素）：配合圆屏固件 v1.7.1 的排版，保证不碰到外圈
    private const val ROAD_MAX_W = 290
    private const val BOTTOM_MAX_W = 240

    private fun sendText(slot: Int, text: String, maxW: Int, size: Float, minSize: Float) {
        val img = TextImage.render(text, maxW, size, minSize)
        if (img == null) BleLink.sendImage(slot, 0, 0, ByteArray(0))
        else BleLink.sendImage(slot, img.w, img.h, img.data)
    }

    // ================= v1.7：车速 =================

    private fun onLocation(loc: AMapNaviLocation, epoch: Long) {
        val kmh = loc.speed.roundToInt().coerceAtLeast(0)
        speedKmhNow = kmh
        uiSpeed = kmh                                                            // v3.0
        val c = loc.coord
        val receivedAt = now()
        val receivedWall = System.currentTimeMillis()
        val fixStamp = loc.time ?: 0L
        val idleSpeed = loc.speed.toDouble()
        val idleLat = c?.latitude ?: Double.NaN
        val idleLng = c?.longitude ?: Double.NaN
        val idleAccuracy = loc.accuracy.toDouble()
        if (c != null) Brightness.updateLocation(c.latitude, c.longitude)
        handler.post {
            if (!arrivalSession.isOpen(epoch)) return@post
            if(active && !navigationIsSimulation())TrafficLightOverlay.positionFix(
                idleLat,idleLng,idleAccuracy,idleSpeed/3.6,fixStamp,receivedWall,receivedAt)
            if (cruising) cruiseIdle.onFix(receivedAt, fixStamp, idleSpeed, idleLat, idleLng, idleAccuracy)
            RideRecorder.onFix(idleLat,idleLng,fixStamp,receivedWall,idleAccuracy,idleSpeed)
            if (c != null) {
                myLat = c.latitude; myLng = c.longitude
                if (active) addTrip(c.latitude, c.longitude, kmh)                 // v7.9
                ShareGuide.onLocation(c.latitude, c.longitude)                    // v4.5
                MapFeeder.onLocation(c.latitude, c.longitude, loc.bearing, kmh)   // v2.0 小地图
            }
            val m = "#v,$kmh"
            if (m != lastSpeedMsg) {
                lastSpeedMsg = m
                BleLink.send(m)
            }
        }
    }

    // ================= v1.7：电子眼测速 =================

    private fun onCameras(arr: Array<*>?) {
        var best: AMapNaviCameraInfo? = null
        val all = StringBuilder()
        arr?.forEach { item ->
            if (item is AMapNaviCameraInfo) {
                if (all.length < 240) all.append("[类型${item.cameraType}(${CamIcons.name(item.cameraType)}) 限速${item.cameraSpeed} ${item.cameraDistance}米] ")
                if (item.cameraSpeed > 0 && item.cameraDistance in 0..cameraRange()) {
                    val b = best
                    if (b == null || item.cameraDistance < b.cameraDistance) best = item
                }
            }
        }
        val b = best
        val lim = b?.cameraSpeed ?: 0
        val dist = b?.cameraDistance ?: 0
        val lat = b?.y ?: 0.0
        val lng = b?.x ?: 0.0
        val n = arr?.size ?: 0
        // v3.4 前方 1 公里内的电子眼位置发给圆屏，画在小地图上："#c,坐标系,x,y,限速,图标,..."
        // v4.7 和道路安全提示合在一起发（pushCamMap），最多 10 个
        val pts = ArrayList<MapPt>()
        arr?.forEach { item ->
            if (item is AMapNaviCameraInfo && item.cameraDistance in 0..1000 && item.x != 0.0) {
                val p = MapFeeder.toFrame(item.y, item.x) ?: return@forEach
                pts.add(MapPt(p[0], p[1], max(0, item.cameraSpeed), CamIcons.iconOf(item.cameraType), item.cameraDistance))
            }
        }
        handler.post { navCamPts = pts; pushCamMap() }
        camRaw = "${n}个：$all（${java.text.SimpleDateFormat("HH:mm:ss", java.util.Locale.CHINA).format(java.util.Date())}）"
        handler.post {
            lastCamAt = now()
            if (b != null) {
                // 新的（或同一个）测速点：开始跟踪
                if (lim != camLimit || camLat != lat || camLng != lng) camMinDist = Int.MAX_VALUE   // 换了一个测速点
                camLimit = lim; camLat = lat; camLng = lng
                camBaseDist = dist
                updateCamDist(dist)
            } else if (n == 0) {
                clearCamera()      // 高德明确告诉我们前方没有电子眼
            }
            // 列表里有电子眼但都不在 300 米内 / 没有限速：保持当前正在跟踪的那个（如果有）
        }
    }

    /** 每 200 毫秒：用当前位置算到测速点的直线距离，经过之后自动消失 */
    private fun trackCamera() {
        if (camLimit <= 0) return
        if (now() - lastCamAt > CAMERA_TIMEOUT_MS) { clearCamera(); return }
        if (camLat == 0.0 || myLat.isNaN()) return
        val d = distM(myLat, myLng, camLat, camLng).roundToInt()
        if (d > cameraRange() + 150) { clearCamera(); return }
        // 离测速点最近时不到 60 米，之后又远离 30 米以上 → 已经经过
        if (camMinDist < 60 && d > camMinDist + 30) { clearCamera(); return }
        updateCamDist(d)
    }

    private fun updateCamDist(d: Int) {
        if (d < camMinDist) camMinDist = d
        uiCamLimit = camLimit; uiCamDist = d                                     // v3.0
        val m = "#k,$camLimit,${(d + 5) / 10 * 10}"
        if (m != lastCamMsg) {
            lastCamMsg = m
            BleLink.send(m)
        }
    }

    private fun clearCamera() {
        camLimit = 0
        uiCamLimit = 0                                                           // v3.0
        camMinDist = Int.MAX_VALUE
        if (lastCamMsg != "#k") {
            lastCamMsg = "#k"
            BleLink.send("#k")
        }
    }

    /** 两点间距离（米），短距离用平面近似足够 */
    private fun distM(lat1: Double, lng1: Double, lat2: Double, lng2: Double): Double {
        val k = Math.PI / 180
        val x = (lng2 - lng1) * k * Math.cos((lat1 + lat2) / 2 * k)
        val y = (lat2 - lat1) * k
        return Math.sqrt(x * x + y * y) * 6_371_000.0
    }

    // ================= v1.7：车道 =================

    private fun toIntList(v: Any?): List<Int> = when (v) {
        is IntArray -> v.toList()
        is ByteArray -> v.map { it.toInt() and 0xFF }
        is CharArray -> v.map { it.code }
        else -> emptyList()
    }

    /** 用反射读字段（不同版本 SDK 字段名可能略有不同，读不到就返回 null） */
    private fun fieldOf(o: Any, name: String): Any? {
        var c: Class<*>? = o.javaClass
        while (c != null) {
            try {
                val f = c.getDeclaredField(name)
                f.isAccessible = true
                return f.get(o)
            } catch (_: NoSuchFieldException) {
                c = c.superclass
            } catch (_: Exception) {
                return null
            }
        }
        return null
    }

    private fun onLaneInfo(args: Array<out Any?>?) {
        if (args == null || args.isEmpty()) return
        var bg: List<Int> = emptyList()
        var fr: List<Int> = emptyList()
        var count: Int? = null
        val api: String
        if (args.size >= 3) {
            // 旧版回调：showLaneInfo(AMapLaneInfo[] 每条车道一个, byte[] 背景, byte[] 推荐)
            // 新版回调也会同时触发；只要收到过新版，就不再用旧版数据
            if (newLaneApi) return
            api = "旧"
            bg = toIntList(args[1])
            fr = toIntList(args[2])
            val arr = args[0] as? Array<*>
            if (arr != null && arr.isNotEmpty()) count = arr.size
        } else {
            // 新版回调：showLaneInfo(AMapLaneInfo)，laneCount = 车道数量
            val obj = args[0] ?: return
            newLaneApi = true
            api = "新"
            bg = toIntList(fieldOf(obj, "backgroundLane"))
            fr = toIntList(fieldOf(obj, "frontLane"))
            count = (fieldOf(obj, "laneCount") as? Number)?.toInt()
        }
        // 没有车道数时：数到第一个 255（无效）为止
        var n = count ?: bg.indexOfFirst { it == 255 }.let { if (it < 0) bg.size else it }
        n = n.coerceIn(0, minOf(8, bg.size))
        val sb = StringBuilder()
        for (i in 0 until n) {
            val b = bg[i]
            val f = fr.getOrElse(i) { 255 }
            sb.append(if (b in 0..14) Integer.toHexString(b) else "f")
            sb.append(if (f in 0..14) Integer.toHexString(f) else "f")
        }
        val m = if (n > 0) "#j,$sb" else "#j"
        uiLanes = IntArray(n * 2) { k -> if (k % 2 == 0) bg[k / 2] else fr.getOrElse(k / 2) { 255 } }   // v3.0
        laneRaw = "${api}接口 车道数${count ?: "?"}：背景 ${bg.take(10)}，推荐 ${fr.take(10)}（编码 $sb）"
        handler.post {
            if (m != lastLaneMsg) {
                lastLaneMsg = m
                BleLink.send(m)
            }
        }
    }

    private fun onLaneHide() {
        uiLanes = IntArray(0)                                                    // v3.0
        handler.post {
            if (lastLaneMsg != "#j") {
                lastLaneMsg = "#j"
                BleLink.send("#j")
            }
        }
    }

    // ---- 下面用“动态代理”只实现我们关心的几个回调，其余回调自动忽略 ----

    private fun makeNaviListener(epoch: Long): AMapNaviListener =
        Proxy.newProxyInstance(
            AMapNaviListener::class.java.classLoader,
            arrayOf(AMapNaviListener::class.java)
        ) { proxy, method, args ->
            try {
                if (method.name !in listOf("hashCode","equals","toString") && !arrivalSession.isOpen(epoch))
                    return@newProxyInstance defaultValue(method.returnType)
                when (method.name) {
                    "onStartNavi" -> if (usingComponent) {
                        val type = (args?.getOrNull(0) as? Number)?.toInt() ?: 0
                        handler.post {
                            if (arrivalSession.begin(epoch,type!=1)) RideRecorder.start("导航","导航组件",type!=1)
                        }
                    }
                    "onNaviInfoUpdate" -> (args?.getOrNull(0) as? NaviInfo)?.let { onNaviInfo(it,epoch) }
                    "onLocationChange" -> (args?.getOrNull(0) as? AMapNaviLocation)?.let { onLocation(it,epoch) }
                    "updateCameraInfo" -> onCameras(args?.getOrNull(0) as? Array<*>)
                    "showLaneInfo" -> onLaneInfo(args)
                    "hideLaneInfo" -> onLaneHide()
                    "onArriveDestination" -> finish("A,0", "已到达目的地",epoch)
                    "onEndEmulatorNavi" -> if (arrivalSession.isSimulation(epoch)) finish("A,0", "模拟导航结束",epoch)
                    // v7.9 到达自己加的途经点：圆屏放 3 秒短特效
                    "onArrivedWayPoint" -> if (realWaypoints) BleLink.sendEvent("#J")
                    // v3.8 “手机系统语音”模式：高德把播报文字交给我们念；v5.1 高德自己念时用它来压低音乐
                    "onGetNavigationText" -> NaviVoice.onNaviText(args?.lastOrNull { it is String } as? String, usingComponent)
                    // v3.0 算路结果交给选路线页面
                    "onCalculateRouteSuccess" -> { val cb = routeResults.current(); val a = args?.getOrNull(0); handler.post { if (arrivalSession.isOpen(epoch)) { if(cb!=null && routeResults.current()===cb)cb(true,a); setRerouting(false); ShareGuide.onCalcDone(true) } } }
                    "onCalculateRouteFailure" -> { val cb = routeResults.current(); val a = args?.getOrNull(0); handler.post { if (arrivalSession.isOpen(epoch)) { if(cb!=null && routeResults.current()===cb)cb(false,a); setRerouting(false); ShareGuide.onCalcDone(false) } } }
                    // v4.5 偏航：按分享路线导航时，高德重算完我们再按原路线重新规划；v4.9 圆屏显示“重新规划中”
                    "onReCalculateRouteForYaw" -> handler.post { setRerouting(true); ShareGuide.onYaw() }
                    "onReCalculateRouteForTrafficJam" -> handler.post { setRerouting(true) }
                    // v4.9 GPS 信号弱 / 更快路线 / 路况变化
                    "onGpsSignalWeak" -> (args?.getOrNull(0) as? Boolean)?.let { w -> handler.post { setGpsWeak(w) } }
                    "onSuggestChangePath" -> { val a = args; handler.post { onSuggestChangePath(a) } }
                    "onTrafficStatusUpdate" -> handler.post { lastTrafficAt = 0L; pushTraffic() }
                    "hashCode" -> return@newProxyInstance System.identityHashCode(proxy)
                    "equals" -> return@newProxyInstance (proxy === args?.getOrNull(0))
                    "toString" -> return@newProxyInstance "MotoNavListener"
                    // v4.7 交通设施（道路安全提示）、巡航模式的电子眼：不同 SDK 版本方法名不一样，按名字认
                    else -> if (isCruiseCallback(method.name)) { val a = args; handler.post { onCruiseCallback(method.name, a) } }
                }
            } catch (e: Exception) {
                extraInfo = "处理 ${method.name} 出错：${e.message}"
            }
            defaultValue(method.returnType)
        } as AMapNaviListener

    private fun makePageCallback(epoch: Long = arrivalSession.generation()): INaviInfoCallback =
        Proxy.newProxyInstance(
            INaviInfoCallback::class.java.classLoader,
            arrayOf(INaviInfoCallback::class.java)
        ) { proxy, method, args ->
            when (method.name) {
                "onExitPage" -> {
                    arrivalSession.cancel(epoch)
                    handler.post { if (arrivalSession.isCurrent(epoch)) resetState() }
                    finish("C", "已退出导航",epoch)
                }
                "hashCode" -> return@newProxyInstance System.identityHashCode(proxy)
                "equals" -> return@newProxyInstance (proxy === args?.getOrNull(0))
                "toString" -> return@newProxyInstance "MotoNavPageCallback"
            }
            defaultValue(method.returnType)
        } as INaviInfoCallback

    // ================= v4.9 重新规划中 / GPS 信号弱 / 更快路线 / 路况颜色 =================

    private var rerouting = false
    private var gpsWeakSent = false
    private var lastTrafficMsg = ""
    private var lastTrafficAt = 0L
    private var suggestNewId: Any? = null              // 高德建议的更快路线编号（Long 或 Int）
    @Volatile var uiSuggestMin = 0                     // 给导航页显示：更快路线能省几分钟（0 = 没有）
    private val suggestTimeout = Runnable { clearSuggest() }

    /** 重新规划中：圆屏显示 REROUTING（只在导航进行中） */
    fun setRerouting(on: Boolean) {
        if (on && !active) return
        if (on == rerouting) return
        rerouting = on
        BleLink.send(if (on) "#y,1" else "#y,0")
    }

    private fun setGpsWeak(w: Boolean) {
        if (w == gpsWeakSent) return
        gpsWeakSent = w
        BleLink.send(if (w) "#x,1" else "#x,0")
        extraInfo = if (w) "GPS 信号弱" else "GPS 信号恢复"
    }

    /** 高德发现更快的路线：onSuggestChangePath(新路线编号, 旧路线编号, 省的秒数, …) */
    private fun onSuggestChangePath(args: Array<out Any?>?) {
        val a = args ?: return
        val newId = a.getOrNull(0) ?: return
        val save = (a.getOrNull(2) as? Number)?.toInt() ?: 0
        val min = max(1, (save + 30) / 60)
        suggestNewId = newId
        uiSuggestMin = min
        BleLink.send("#h,$min")
        extraInfo = "发现更快路线，能省约 $min 分钟（新路线 $newId）"
        handler.removeCallbacks(suggestTimeout)
        handler.postDelayed(suggestTimeout, 30_000)
    }

    /** 导航页上点了“换这条”：切到高德建议的更快路线 */
    fun acceptSuggest(): Boolean {
        val id = suggestNewId ?: return false
        val n = navi ?: return false
        val lid = (id as? Number)?.toLong() ?: return false
        val ok = Refl.call(n, "selectMainPathID", lid).first || Refl.call(n, "selectMainPathID", lid.toInt()).first
        extraInfo = if (ok) "已换到更快的路线" else "没能切换路线（这个 SDK 版本可能不支持）"
        clearSuggest()
        return ok
    }

    fun clearSuggest() {
        handler.removeCallbacks(suggestTimeout)
        if (suggestNewId == null && uiSuggestMin == 0) return
        suggestNewId = null; uiSuggestMin = 0
        BleLink.send("#h")
    }

    /**
     * 路况：从高德路线上读出每一段的拥堵状态（2 缓行、3 拥堵、4 严重拥堵），
     * 只发不畅通的路段："#u,起点米,终点米,状态,..."（米 = 离路线起点）
     */
    private fun pushTraffic() {
        lastTrafficAt = now()
        val path = navi?.naviPath ?: return
        val list = Refl.call(path, "getTrafficStatuses").second as? List<*> ?: return
        val segs = ArrayList<IntArray>()
        var pos = 0
        for (o in list) {
            val len = Refl.int(o, "getLength") ?: continue
            val st = Refl.int(o, "getStatus") ?: 0
            if (st >= 2 && len > 0) {
                val last = segs.lastOrNull()
                if (last != null && last[2] == st && last[1] >= pos - 5) last[1] = pos + len
                else segs.add(intArrayOf(pos, pos + len, st))
            }
            pos += len
        }
        val sb = StringBuilder("#u")
        for (sg in segs) {
            val part = ",${sg[0]},${sg[1]},${sg[2]}"
            if (sb.length + part.length > 480) break
            sb.append(part)
        }
        val m = sb.toString()
        if (m != lastTrafficMsg) { lastTrafficMsg = m; BleLink.send(m) }
    }

    // ================= v4.7 巡航模式 + 道路安全提示 =================

    @Volatile var cruising = false
    @Volatile var cruiseRaw = "（还没收到巡航/交通设施数据）"
    private class MapPt(val x: Int, val y: Int, val spd: Int, val icon: Int, val dist: Int, val at: Long = 0L)
    private var navCamPts: List<MapPt> = emptyList()           // 导航中的电子眼（updateCameraInfo）
    private var cruiseCamPts: List<MapPt> = emptyList()        // 巡航中的电子眼
    private val facPts = LinkedHashMap<String, MapPt>()        // 道路安全提示（按位置合并，20 秒没更新就去掉）
    private var aimlessListener: Any? = null
    private val cruiseIdle = CruiseIdleTimer()

    private val cruiseTick = object : Runnable {
        override fun run() {
            if (!cruising) return
            val t = now()
            if (cruiseIdle.shouldStop(t)) {
                stopCruise()
                lastInfo = "连续10分钟未移动，已自动退出巡航"
                cruiseRaw = lastInfo
                return
            }
            trackCamera()
            if (facPts.values.removeAll { t - it.at > 20_000 }) pushCamMap()
            if (cruiseCamPts.isNotEmpty() && cruiseCamPts[0].at != 0L && t - cruiseCamPts[0].at > 20_000) { cruiseCamPts = emptyList(); pushCamMap() }
            handler.postDelayed(this, 300)
        }
    }

    /** 开始巡航（不设目的地）：返回 null = 成功，否则是出错原因 */
    fun startCruise(context: Context): String? {
        val n = attach(context) ?: return lastInfo
        // Ride planning leaves travel-view mode on the reused singleton.
        directRouteMode=LocalRouting.DRIVE
        n.setIsNaviTravelView(false)
        // 新版 SDK 的巡航回调在单独的 AimlessModeListener 里：有就挂上
        try {
            val cls = Class.forName("com.amap.api.navi.AimlessModeListener")
            val l = Proxy.newProxyInstance(cls.classLoader, arrayOf(cls)) { proxy, m, a ->
                when (m.name) {
                    "hashCode" -> return@newProxyInstance System.identityHashCode(proxy)
                    "equals" -> return@newProxyInstance (proxy === a?.getOrNull(0))
                    "toString" -> return@newProxyInstance "MotoNavCruise"
                    else -> handler.post { onCruiseCallback(m.name, a) }
                }
                defaultValue(m.returnType)
            }
            if (Refl.call(n, "addAimlessModeListener", l).first) aimlessListener = l
        } catch (_: Throwable) { }
        // 3 = 电子眼和特殊道路设施都播报
        if (!Refl.call(n, "startAimlessMode", 3).first) return "这个版本的高德 SDK 不支持巡航模式"
        cruiseIdle.start(now())
        cruising = true
        RideRecorder.start("巡航","巡航")
        cruiseRaw = "巡航已开始，等高德发来数据…"
        lastInfo = "巡航中"
        BleLink.send("#z,1")
        handler.removeCallbacks(cruiseTick)
        handler.postDelayed(cruiseTick, 300)
        return null
    }

    /** 结束巡航，圆屏回到首页 */
    fun stopCruise() {
        if (!cruising) return
        val epoch=arrivalSession.generation()
        endCruiseQuietly()
        handler.post { if (arrivalSession.isCurrent(epoch)) resetState() }
        BleLink.send("C")
        lastInfo = "巡航已结束"
    }

    private fun endCruiseQuietly() {
        if (!cruising) return
        arrivalSession.cancel(arrivalSession.generation())
        cruising = false
        RideRecorder.stop()
        cruiseIdle.stop()
        handler.removeCallbacks(cruiseTick)
        Refl.call(navi, "stopAimlessMode")
        aimlessListener?.let { Refl.call(navi, "removeAimlessModeListener", it) }
        aimlessListener = null
        BleLink.send("#z,0")
    }

    private fun isCruiseCallback(name: String): Boolean {
        val n = name.lowercase()
        return "facility" in n || "aimless" in n || "eleccamera" in n
    }

    private fun num(o: Any?, vararg names: String): Double? {
        for (nm in names) { val r = Refl.call(o, nm); if (r.first && r.second is Number) return (r.second as Number).toDouble() }
        return null
    }

    /** 巡航电子眼 / 交通设施回调：取出位置、类型、限速、距离，合并后发给圆屏 */
    private fun onCruiseCallback(name: String, args: Array<out Any?>?) {
        val items = ArrayList<Any>()
        var isList = false
        args?.forEach { a ->
            when (a) {
                is Array<*> -> { isList = true; a.forEach { if (it != null) items.add(it) } }
                is Collection<*> -> { isList = true; a.forEach { if (it != null) items.add(it) } }
                null, is Number, is String, is Boolean -> {}
                else -> items.add(a)
            }
        }
        val camCb = "camera" in name.lowercase()
        if (items.isEmpty()) {
            if (camCb && isList) { cruiseCamPts = emptyList(); pushCamMap() }
            if (isList && !camCb) { facPts.clear(); pushCamMap() }
            cruiseRaw = "$name：0 个（${clock()}）"
            return
        }
        val t = now()
        val raw = StringBuilder()
        val cams = ArrayList<MapPt>()
        var best: MapPt? = null; var bestLat = 0.0; var bestLng = 0.0
        if (isList && !camCb) facPts.clear()
        for (o in items) {
            val type = num(o, "getBroadcastType", "getType", "getCameraType")?.toInt() ?: -1
            var lng = num(o, "getCoorX", "getLongitude", "getX")
            var lat = num(o, "getCoorY", "getLatitude", "getY")
            if (lat == null || lng == null || lat == 0.0) {
                val c = Refl.call(o, "getCoord").second ?: Refl.call(o, "getCoordinate").second
                lat = num(c, "getLatitude"); lng = num(c, "getLongitude")
            }
            val spd = max(0, num(o, "getLimitSpeed", "getSpeed", "getCameraSpeed")?.toInt() ?: 0)
            val dist = num(o, "getDistance", "getCameraDistance")?.toInt() ?: -1
            val asCam = camCb || FacTypes.isCamera(type)
            if (raw.length < 260) raw.append("[${if (asCam) "眼" else FacTypes.name(type)} 类型$type ${if (spd > 0) "限速$spd " else ""}${dist}米] ")
            if (lat == null || lng == null || lat == 0.0 || dist > 1000) continue
            val p = MapFeeder.toFrame(lat, lng) ?: continue
            if (asCam) {
                val mp = MapPt(p[0], p[1], spd, if (spd > 0) 2 else 1, dist, t)
                cams.add(mp)
                if (spd > 0 && dist in 0..cameraRange() && (best == null || dist < best.dist)) { best = mp; bestLat = lat; bestLng = lng }
            } else {
                facPts["${p[0] / 5},${p[1] / 5},$type"] = MapPt(p[0], p[1], 0, FacTypes.iconOf(type), dist, t)
            }
        }
        if (camCb || cams.isNotEmpty()) {
            if (cruising) cruiseCamPts = cams
            // 巡航中也显示测速牌（导航中由 updateCameraInfo 负责）
            val b = best
            if (cruising && b != null) {
                lastCamAt = now()
                if (b.spd != camLimit || camLat != bestLat || camLng != bestLng) camMinDist = Int.MAX_VALUE
                camLimit = b.spd; camLat = bestLat; camLng = bestLng
                camBaseDist = b.dist
                updateCamDist(b.dist)
            }
        }
        cruiseRaw = "$name：${items.size} 个 $raw（${clock()}）"
        pushCamMap()
    }

    private fun clock() = java.text.SimpleDateFormat("HH:mm:ss", java.util.Locale.CHINA).format(java.util.Date())

    /** 电子眼 + 道路安全提示合在一起发给圆屏（最近的 10 个） */
    private fun pushCamMap() {
        val fid = MapFeeder.frameId()
        val all = (navCamPts + cruiseCamPts + facPts.values).sortedBy { if (it.dist < 0) 99_999 else it.dist }.take(10)
        val sb = StringBuilder()
        for (p in all) sb.append(",${p.x},${p.y},${p.spd},${p.icon}")
        val m = if (all.isNotEmpty() && fid != null) "#c,$fid$sb" else "#c"
        if (m != lastCamMapMsg) { lastCamMapMsg = m; BleLink.send(m) }
    }

    private fun defaultValue(t: Class<*>): Any? = when (t) {
        java.lang.Boolean.TYPE -> false
        Integer.TYPE -> 0
        java.lang.Long.TYPE -> 0L
        java.lang.Float.TYPE -> 0f
        java.lang.Double.TYPE -> 0.0
        else -> null
    }
}


/**
 * v4.2 电子眼类型 → 圆屏图标编号（和固件 v3.8 的 CAM_ICONS 顺序一致）：
 *   0 电子眼(未知)  1 监控  2 测速  3 闯红灯  4 违章拍照  5 公交道  6 应急车道  7 非机动车道
 *   8 区间起点  9 区间终点  10 流动测速  11 ETC  12 礼让行人  13 违法变道  14 违停  15 禁止鸣笛
 *
 * 高德类型编号以手机上实际装的 SDK 为准：用反射读 com.amap.api.navi.enums.CameraType 的常量名再翻译，
 * 读不到就用常见编号表。这个版本的 SDK 没有单独的“礼让行人”“违法变道”类型（一般归在“违章拍照”里），
 * 以后 SDK 加了新类型，常量名里带关键词的会自动对上图标。
 */
object CamIcons {
    private val NAMES = arrayOf(
        "电子眼", "监控", "测速", "闯红灯", "违章拍照", "公交道", "应急车道", "非机动车道",
        "区间起点", "区间终点", "流动测速", "ETC", "礼让行人", "违法变道", "违停", "禁止鸣笛"
    )

    /** 反射读不到时用的常见编号：高德类型 → 图标 */
    private val FALLBACK = mapOf(0 to 2, 1 to 1, 2 to 3, 3 to 4, 4 to 5, 5 to 6, 6 to 7, 8 to 8, 9 to 9, 10 to 10, 11 to 11)

    /** 常量名 → 图标（先精确，再按关键词） */
    private fun iconOfField(field: String): Int? {
        val f = field.uppercase()
        val exact = mapOf(
            "SPEED" to 2, "SURVEILLANCE" to 1, "TRAFFICLIGHT" to 3, "BREAKRULE" to 4, "BUSWAY" to 5,
            "EMERGENCY" to 6, "BICYCLE" to 7, "INTERVALVELOCITYSTART" to 8, "INTERVALVELOCITYEND" to 9,
            "FLOWSPEED" to 10, "ETC" to 11
        )
        exact[f]?.let { return it }
        return when {
            "PEDESTRIAN" in f || "ZEBRA" in f || "COURTESY" in f || "YIELD" in f -> 12
            "LANECHANGE" in f || "CHANGELANE" in f || "SOLIDLINE" in f -> 13
            "INTERVAL" in f && "START" in f -> 8
            "INTERVAL" in f && "END" in f -> 9
            "FLOW" in f || "MOBILE" in f -> 10
            "SPEED" in f || "VELOCITY" in f -> 2
            "LIGHT" in f -> 3
            "PARK" in f -> 14
            "HONK" in f || "WHISTLE" in f -> 15
            "BUS" in f -> 5
            "EMERGENCY" in f -> 6
            "BICYCLE" in f || "NONMOTOR" in f -> 7
            "ETC" in f -> 11
            "BREAK" in f || "ILLEGAL" in f || "VIOLAT" in f -> 4
            "SURVEIL" in f || "MONITOR" in f -> 1
            else -> null
        }
    }

    /** 从 SDK 里读出来的 高德类型 → 图标 */
    private val fromSdk: Map<Int, Int> by lazy {
        val m = HashMap<Int, Int>()
        try {
            val c = Class.forName("com.amap.api.navi.enums.CameraType")
            for (f in c.fields) {
                if (!java.lang.reflect.Modifier.isStatic(f.modifiers) || f.type != Integer.TYPE) continue
                val icon = iconOfField(f.name) ?: continue
                val v = f.getInt(null)
                if (!m.containsKey(v)) m[v] = icon
            }
        } catch (_: Throwable) { }
        m
    }

    /** 高德类型 → 图标编号（不认识的 = 0 通用电子眼） */
    fun iconOf(type: Int): Int = fromSdk[type] ?: FALLBACK[type] ?: 0

    /** 高德类型的中文名（调试用） */
    fun name(type: Int): String = NAMES[iconOf(type)]
}

/**
 * v4.7 交通设施（道路安全提示）类型 → 圆屏图标（16~25，和固件 v4.8 的 CAM_ICONS 一致）：
 *   16 急弯  17 学校  18 事故多发  19 注意行人  20 陡坡  21 村庄  22 窄桥  23 落石  24 隧道  25 注意（其他）
 * 编号以手机上装的高德 SDK 为准：反射读几个可能的类型常量类，按常量名里的关键词对应；读不到就都显示“注意”，
 * 调试区会显示原始类型编号，方便以后补对应关系。
 */
object FacTypes {
    private val CLASSES = listOf(
        "com.amap.api.navi.enums.TrafficFacilityType", "com.amap.api.navi.enums.AMapNaviTrafficFacilityType",
        "com.amap.api.navi.enums.AimLessModeTrafficFacilityType", "com.amap.api.navi.enums.BroadcastType",
        "com.amap.api.navi.enums.FacilityType"
    )
    private val names: Map<Int, String> by lazy {
        val m = HashMap<Int, String>()
        for (cn in CLASSES) {
            try {
                val c = Class.forName(cn)
                for (f in c.fields) {
                    if (!java.lang.reflect.Modifier.isStatic(f.modifiers) || f.type != Integer.TYPE) continue
                    val v = f.getInt(null)
                    if (!m.containsKey(v)) m[v] = f.name.uppercase()
                }
            } catch (_: Throwable) { }
        }
        m
    }

    fun isCamera(type: Int): Boolean {
        val n = names[type] ?: return false
        return "CAMERA" in n || "SPEED" in n || "MONITOR" in n || "SURVEIL" in n || "ELEC" in n
    }

    fun iconOf(type: Int): Int {
        val n = names[type] ?: return 25
        return when {
            "CURVE" in n || "BEND" in n || "TURN" in n || "WINDING" in n || "SHARP" in n -> 16
            "SCHOOL" in n || "CHILD" in n -> 17
            "ACCIDENT" in n -> 18
            "PEDESTRIAN" in n || "ZEBRA" in n || "WALK" in n -> 19
            "SLOPE" in n || "DOWNHILL" in n || "STEEP" in n || "HILL" in n -> 20
            "VILLAGE" in n -> 21
            "BRIDGE" in n -> 22
            "ROCK" in n || "FALL" in n -> 23
            "TUNNEL" in n -> 24
            else -> 25
        }
    }

    private val ZH = arrayOf("急弯", "学校", "事故多发", "注意行人", "陡坡", "村庄", "窄桥", "落石", "隧道", "注意")
    fun name(type: Int): String {
        val n = names[type]
        return ZH[iconOf(type) - 16] + if (n != null) "($n)" else ""
    }
}
