package com.yuminaga.motonav

import android.content.Context
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import com.amap.api.location.AMapLocationClient
import com.amap.api.location.AMapLocationClientOption
import java.util.Calendar
import java.util.concurrent.Executors
import kotlin.math.roundToInt

/** Independent motorcycle guidance; SDK road-node queries never drive its progress or voice. */
object WebMotorGuide {
    @Volatile var route: WebMotorRoute? = null; private set
    @Volatile var active = false; private set
    @Volatile var arrived = false; private set
    @Volatile var simulation = false; private set
    @Volatile var paused = false; private set
    @Volatile var speed = 0; private set
    @Volatile var simulationKmh = 60
    @Volatile var status = "尚未开始网页导航"; private set
    @Volatile var instruction = "等待定位"; private set
    @Volatile var direction = "S"; private set
    @Volatile var turnMeters = 0; private set
    @Volatile var remaining = 0; private set
    @Volatile var remainingS = 0; private set
    @Volatile var lat = Double.NaN; private set
    @Volatile var lng = Double.NaN; private set
    @Volatile var bearing = 0f; private set
    @Volatile var currentRoad=""; private set
    @Volatile var nextRoad=""; private set
    var routeProgress=0.0;private set
    private var offRoute=false
    private var appContext:Context?=null
    fun guidanceFresh()=mapGuidanceFresh() && !simulation
    fun signalContextActive()=active && !arrived && !simulation && hasFix && !offRoute && !rerouting && !requestInFlight
    fun mapGuidanceFresh()=active && !arrived && hasFix && !offRoute && !rerouting &&
        !requestInFlight && SystemClock.elapsedRealtime()-lastFixAt in 0..3000
    private fun syncCarRoute(chosen:WebMotorRoute) {
        appContext?.let{c->runCatching{
            CarNavSync.requestWeb(c,chosen,simulation)
            if(c.packageName=="com.motonav.app.fresh"){
                val anchors=CarNavSync.webAnchors(chosen.key).ifEmpty{
                    RouteAnchorSelector.select(chosen.points.toTypedArray(),intArrayOf()).map{chosen.points[it]}
                }
                WebMotorSignals.load(c,chosen,anchors)
            } else WebMotorSignals.stop()
        }
            .onFailure{android.util.Log.w("MotoNavCarSync","Web route sync failed; motorcycle guidance continues",it)}}
    }
    private var tracker: MotorTrack? = null
    private var location: AMapLocationClient? = null
    private val main = Handler(Looper.getMainLooper())
    private val worker = Executors.newSingleThreadExecutor()
    private var generation = 0
    private var requestInFlight = false
    private var lastRetryAt = -30_000L
    private var rerouting = false
    private var routeSent = ""
    private var lastFixAt = 0L
    private var lastFixStamp = 0L
    private var startedAt = 0L
    private var tripM = 0.0
    private var tripMax = 0
    private var lastLat = Double.NaN
    private var lastLng = Double.NaN
    private var simProgress = 0.0
    private var lastSimAt = 0L
    private var lastVoice = ""
    private var roadSent = ""
    private var footerSent = ""
    private var hasFix = false
    private var progress = 0.0
    private var viaProgress: List<Double> = emptyList()

    fun start(context: Context, chosen: WebMotorRoute, emulate: Boolean) {
        if(SingleEngineSession.owns)SingleEngineSession.disarm()
        stop(false)
        if(SingleEngineSession.EXPERIMENT){
            runCatching{NaviBridge.releaseForSingleEngine()}.onFailure{
                android.widget.Toast.makeText(context,"原导航引擎尚未退出，请重新打开 App 后开始",android.widget.Toast.LENGTH_LONG).show();return
            }
        }else NaviBridge.prepareWebMotorNavigation()
        generation++
        appContext=context.applicationContext;currentRoad="";nextRoad="";offRoute=false;routeProgress=0.0
        route = chosen; tracker = chosen.tracker(); active = true; arrived = false
        simulation = emulate; paused = false; simProgress = 0.0; lastSimAt = 0
        lat = Double.NaN; lng = Double.NaN; hasFix = false; progress = 0.0
        speed = 0; tripM = 0.0; tripMax = 0; lastLat = Double.NaN; lastLng = Double.NaN
        startedAt = SystemClock.elapsedRealtime(); lastFixAt = startedAt; lastFixStamp = 0L
        remaining = chosen.distance; remainingS = chosen.seconds
        instruction = "等待定位"; status = if (emulate) "网页摩托车模拟导航" else "正在获取高德定位…"
        routeSent = ""; roadSent = ""; footerSent = ""; lastVoice = ""; lastRetryAt = -30_000L
        requestInFlight = false; rerouting = false
        viaProgress = chosen.request.vias.map { tracker!!.nearestProgress(it.lat, it.lng) }
        MapFeeder.onNaviStart(); MapFeeder.setGhost(null)
        BleLink.send("#z,0"); BleLink.send("#j"); BleLink.send("#k"); BleLink.send("#g"); BleLink.send("#c")
        BleLink.send("#y,0"); BleLink.send("#x,0")
        RideRecorder.start("网页摩托车", chosen.request.end.name, emulate)
        NaviVoice.load(context)
        NaviVoice.muted = context.getSharedPreferences("motonav",0).getBoolean("nav_muted", false)
        if (!emulate) {
            val epoch = generation
            location = AMapLocationClient(context.applicationContext).also { client ->
                client.setLocationOption(AMapLocationClientOption().apply {
                    locationMode = AMapLocationClientOption.AMapLocationMode.Hight_Accuracy
                    interval = 1000L; isNeedAddress = false; isLocationCacheEnable = false
                    isOffset = true
                })
                client.setLocationListener { fix -> main.post {
                    if (!active || generation != epoch) return@post
                    if (fix == null || fix.errorCode != 0) { status = "定位失败：${fix?.errorInfo ?: "无数据"}"; return@post }
                    val age = System.currentTimeMillis() - fix.time
                    if (fix.time <= 0 || age !in -2000..15000 || fix.time <= lastFixStamp) { status = "定位数据过期，等待新定位"; return@post }
                    lastFixStamp = fix.time
                    if (!fix.coordType.equals("GCJ02", true)) { status = "定位坐标系不匹配，等待高德坐标"; return@post }
                    offer(fix.latitude, fix.longitude, fix.accuracy.toDouble(), fix.speed.toDouble(), fix.bearing, fix.time)
                } }
                client.startLocation()
            }
        }
        syncCarRoute(chosen)
        NavService.updateWakeLock()
        main.post(tick)
    }

    private val tick = object : Runnable {
        override fun run() {
            if (!active) return
            WebMotorSignals.pushCameras()
            val t = SystemClock.elapsedRealtime()
            if (simulation && !paused) {
                val track = tracker ?: return
                val dt = if (lastSimAt == 0L) 0.0 else ((t-lastSimAt)/1000.0).coerceIn(0.0,2.0)
                simProgress = (simProgress + simulationKmh/3.6*dt).coerceAtMost(track.total)
                val p = track.at(simProgress); val q = track.at((simProgress+5).coerceAtMost(track.total))
                val heading = if (simProgress < track.total) MotorTrack.bearing(p[0],p[1],q[0],q[1]) else bearing
                offer(p[0],p[1],3.0,if (simProgress < track.total) simulationKmh/3.6 else 0.0,heading,System.currentTimeMillis())
                lastSimAt=t
            }
            if (!simulation && t-lastFixAt>10000) {
                status = "GPS 信号弱，导航进度已暂停，等待新定位"
                BleLink.send("#x,1")
            }
            main.postDelayed(this,1000)
        }
    }

    fun togglePause() { paused=!paused; lastSimAt=0 }

    private fun offer(la: Double, lo: Double, accuracy: Double, mps: Double, heading: Float, wall: Long) {
        if (!active) return
        val track = tracker ?: return
        val result = track.update(la,lo,accuracy,mps,SystemClock.elapsedRealtime())
        if (result == null) { status = "定位精度不足或位置跳变，等待有效定位"; return }
        offRoute=result.offRoute
        lat=la; lng=lo; bearing=heading; lastFixAt=SystemClock.elapsedRealtime(); hasFix=true
        if(!simulation)TrafficLightOverlay.positionFix(la,lo,accuracy,mps,wall,System.currentTimeMillis(),lastFixAt)
        speed=(mps*3.6).roundToInt().coerceIn(0,250); tripMax=maxOf(tripMax,speed)
        if (!lastLat.isNaN()) {
            val d=MotorTrack.distance(lastLat,lastLng,la,lo)
            if (d<300) tripM+=d
        }
        lastLat=la; lastLng=lo; progress=result.progress;routeProgress=progress
        if (!simulation) RideRecorder.onFix(la,lo,wall,System.currentTimeMillis(),accuracy,speed.toDouble())
        Brightness.updateLocation(la,lo)
        MapFeeder.onLocation(la,lo,heading,speed)
        val r=route ?: return
        val mapLights=WebMotorSignals.mapLights()
        val mapKey="${r.key}:lights:${mapLights.size}"
        if (routeSent!=mapKey && MapFeeder.onRoute(r.key,r.points,mapLights,mapLights.size.toString())) routeSent=mapKey
        val stepIndex=r.steps.indexOfFirst { track.cum[it.endIndex] > result.progress + 2 }.let { if (it<0) r.steps.lastIndex else it }
        val step=r.steps[stepIndex]
        val stepLength=track.cum[step.endIndex]-track.cum[step.startIndex]
        val fraction=if (stepLength>0) ((track.cum[step.endIndex]-result.progress)/stepLength).coerceIn(0.0,1.0) else 0.0
        turnMeters=(step.distance*fraction).roundToInt()
        remaining=turnMeters+r.steps.drop(stepIndex+1).sumOf { it.distance }
        remainingS=(step.duration*fraction).roundToInt()+r.steps.drop(stepIndex+1).sumOf { it.duration }
        direction=step.dir
        currentRoad=step.road
        nextRoad=r.steps.getOrNull(stepIndex+1)?.road.orEmpty()
        instruction=if (step.action.isNotBlank()) step.action + if (nextRoad.isNotBlank()) "进入$nextRoad" else ""
            else "沿${step.road.ifBlank { "当前道路" }}行驶"
        val lights=r.steps.drop(stepIndex).sumOf { it.lights }
        val signalDistance=WebMotorSignals.distance()
        BleLink.send(if(signalDistance>=0)"#g,${(signalDistance+5)/10*10}" else "#g")
        status=when {
            requestInFlight -> "偏航，正在重新规划摩托车路线…"
            result.offRoute -> "已偏离路线 · 距路线 ${result.offset.roundToInt()} 米"
            simulation -> "网页摩托车模拟 · ${simulationKmh} km/h"
            else -> "网页摩托车导航 · 定位精度 ${accuracy.roundToInt()} 米"
        }
        BleLink.send("#x,0"); BleLink.send("#v,$speed")
        if (!result.offRoute) {
            BleLink.send("$direction,$turnMeters")
            val text=instruction.take(160)
            if (text!=roadSent) { roadSent=text; sendText(0,text,290,26f,18f) }
            val calendar=Calendar.getInstance().apply { add(Calendar.SECOND,remainingS) }
            val hhmm=calendar.get(Calendar.HOUR_OF_DAY)*100+calendar.get(Calendar.MINUTE)
            BleLink.send("#e,$remaining,$hhmm,$lights")
            val foot="${"%.1f".format(remaining/1000.0)}km · ${"%02d:%02d".format(hhmm/100,hhmm%100)}到达"
            if (foot!=footerSent) { footerSent=foot; sendText(1,foot,240,19f,15f) }
            val bucket=when { turnMeters<=30 -> 0; turnMeters<=100 -> 1; turnMeters<=300 -> 2; turnMeters<=800 -> 3; else -> 4 }
            val voice="${r.key}:$stepIndex:$bucket"
            if (voice!=lastVoice) {
                lastVoice=voice
                if (!NaviVoice.muted) NaviVoice.speak(if (bucket<4) "前方${turnMeters}米，$instruction" else instruction)
            }
        }
        if (result.arrived) complete()
        else if (!simulation && result.offRoute) reroute()
        else if (!result.offRoute && !requestInFlight && rerouting) { rerouting=false; BleLink.send("#y,0");syncCarRoute(r) }
    }

    private fun sendText(slot: Int, text: String, width: Int, size: Float, min: Float) {
        val img=TextImage.render(text,width,size,min)
        if (img!=null) BleLink.sendImage(slot,img.w,img.h,img.data)
    }

    /** Reroutes use the same motorcycle endpoint and retain only the upcoming explicit via points. */
    fun reroute() {
        if (!active || simulation || !hasFix || requestInFlight) return
        val t=SystemClock.elapsedRealtime()
        if (t-lastRetryAt<30000) return
        lastRetryAt=t; requestInFlight=true; rerouting=true
        CarNavSync.stop();WebMotorSignals.stop()
        val epoch=generation
        val old=route ?: return
        val remainingVias=old.request.vias.filterIndexed { i, _ -> progress < viaProgress[i]+30 }
        val request=old.request.copy(start=MotorPoint(lat,lng,"当前位置"), vias=remainingVias)
        status="正在重新规划摩托车路线…"; BleLink.send("#y,1")
        if (!NaviVoice.muted) NaviVoice.speak("已偏离路线，重新规划摩托车路线")
        worker.execute {
            val result=runCatching { WebMotorRoutes.load(request).first() }
            main.post {
                if (!active || epoch!=generation) return@post
                requestInFlight=false
                result.onSuccess { next ->
                    route=next; tracker=next.tracker(); routeSent=""; lastVoice=""; progress=0.0
                    viaProgress=next.request.vias.map { tracker!!.nearestProgress(it.lat,it.lng) }
                    rerouting=false; BleLink.send("#y,0")
                    remaining=next.distance; remainingS=next.seconds
                    hasFix=false;lastFixAt=0;currentRoad="";nextRoad="";routeProgress=0.0
                    syncCarRoute(next)
                    status="摩托车路线已重算，等待新定位"
                }.onFailure {
                    status="摩托车重算失败：${it.message}；30 秒后再试。请停稳后查看路线。"
                    // Keep rerouting indicator while off route; no driving fallback.
                }
            }
        }
    }

    private fun complete() {
        if (!active || arrived) return
        CarNavSync.stop();WebMotorSignals.stop()
        arrived=true; active=false; remaining=0; turnMeters=0; status="已到达目的地"
        instruction="已到达目的地"; direction="A"
        location?.stopLocation(); location?.onDestroy(); location=null
        main.removeCallbacks(tick); RideRecorder.stop()
        BleLink.send("#y,0"); BleLink.send("#x,0")
        val minutes=((SystemClock.elapsedRealtime()-startedAt)/60000).toInt()
        BleLink.sendEvent("#O${tripM.roundToInt()}/$minutes/$tripMax")
        BleLink.send("A,0") // Only this confirmed terminal event emits the arrival command.
        if (!NaviVoice.muted) NaviVoice.speak("已到达目的地")
    }

    fun stop(sendCancel: Boolean = true) {
        val hadSession=route!=null
        WebMotorSignals.stop()
        generation++; active=false; requestInFlight=false; hasFix=false; arrived=false
        main.removeCallbacks(tick)
        runCatching { location?.stopLocation(); location?.onDestroy() }; location=null
        if (hadSession) {CarNavSync.stop();RideRecorder.stop()}
        currentRoad="";nextRoad="";offRoute=false;appContext=null
        route=null; tracker=null; status="网页导航已退出"
        if (hadSession && sendCancel) { BleLink.send("#y,0"); BleLink.send("#x,0"); BleLink.send("C") }
    }
}
