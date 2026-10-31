package com.yuminaga.motonav

import android.content.*
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.os.*
import android.util.Log
import android.view.Gravity
import android.view.View
import android.widget.TextView
import android.text.SpannableStringBuilder
import android.text.Spanned
import android.text.style.ForegroundColorSpan
import org.json.JSONArray
import org.json.JSONObject

/** Only receives light snapshots. Never starts/changes either application's route. */
object TrafficLightOverlay {
    private val handler=Handler(Looper.getMainLooper())
    private val junction=TrafficJunctionState()
    private val outputDelay=TrafficSignalOutputDelay()
    private val stopped=TrafficStopContext()
    private var rawFixAt=0L
    private var context:Context?=null
    private var receiver:BroadcastReceiver?=null
    private var wasVerified=false
    private var sent=false
    private var gateWasOpen=false
    private var approachKey=""
    private var frameId=1
    private var frameOpened=false
    private var frameSent=false
    private var frameAt=0L
    var enabled=false; private set
    var wantedDirection=0; private set
    var packets=0; private set
    private var upstreamAt=0L
    var upstreamDirections:List<Int> = emptyList();private set
    var message="尚未启动"; private set
    private fun now()=SystemClock.elapsedRealtime()
    fun load(c:Context) {
        context=c.applicationContext
        val prefs=c.getSharedPreferences("motonav",Context.MODE_PRIVATE)
        enabled=prefs.getBoolean("traffic_light_overlay",false)
        // All lights in one upstream snapshot are displayed; no movement selector.
    }
    fun enable(c:Context,value:Boolean) {
        if(context==null)load(c)
        enabled=value
        c.getSharedPreferences("motonav",Context.MODE_PRIVATE).edit().putBoolean("traffic_light_overlay",value).apply()
        clear(if(value)"等待路口读秒数据" else "已关闭")
    }
    fun start(c:Context) {
        if(receiver!=null)return
        load(c)
        CarNavSync.start(c)
        val r=object:BroadcastReceiver(){override fun onReceive(ctx:Context,i:Intent){receive(i)}}
        val filter=IntentFilter("AUTONAVI_STANDARD_BROADCAST_SEND")
        if(Build.VERSION.SDK_INT>=33)c.applicationContext.registerReceiver(r,filter,Context.RECEIVER_EXPORTED)
        else c.applicationContext.registerReceiver(r,filter)
        receiver=r;wasVerified=BleLink.verified
        message=if(enabled)"等待路口读秒数据" else "已关闭"
        handler.post(tick)
    }
    fun stop() {
        receiver?.let{try{context?.unregisterReceiver(it)}catch(_:Exception){}}
        receiver=null;handler.removeCallbacks(tick);clear("监听已停止")
    }
    private fun clear(reason:String) {
        stopped.reset()
        outputDelay.reset()
        frameOpened=false;frameSent=false;BleLink.sendTrafficApproach(0)
        junction.clear();message=reason
        BleLink.clearTrafficLight();sent=false
    }
    private fun clearPhase(reason:String){
        junction.clear();outputDelay.reset();BleLink.clearTrafficLight();sent=false;message=reason
    }
    fun positionFix(lat:Double,lng:Double,accuracy:Double,speedMps:Double,stamp:Long,wall:Long,elapsed:Long,raw:Boolean=false){
        if(!raw && rawFixAt>0 && elapsed-rawFixAt in 0..2999)return
        if(raw)rawFixAt=elapsed
        stopped.fix(lat,lng,accuracy,speedMps,stamp,wall,elapsed)
    }
    private fun contextActive()=if(SingleEngineSession.owns)SingleEngineSession.nativeRunning else if(WebMotorGuide.active)WebMotorGuide.signalContextActive() else NaviBridge.trafficLightContextActive()
    fun stoppedAlignmentHeld()=contextActive() && stopped.holds(overlayKey(),now())
    private fun receive(i:Intent) {
        SingleEngineSession.standard(i)
        CarNavSync.standard(i)
        if(!enabled)return
        val listSource=i.hasExtra("lightsData")
        try {
            if(i.getIntExtra("KEY_TYPE",-1)!=60073)return
            packets++
            upstreamAt=now()
            val json=i.getStringExtra("lightsData")
            if(i.getBooleanExtra("clearLights",false) || i.getBooleanExtra("EXTRA_CLEAR_LIGHTS",false) ||
               (i.hasExtra("lightsCount") && i.getIntExtra("lightsCount",-1)==0)) {
                // Empty camera callbacks must not clear a fresh navigation snapshot,
                // and an empty navigation callback must not clear camera directions.
                junction.accept(json!=null,emptyArray(),now())
                if(junction.snapshots(now()).isEmpty()) {
                    // Source absence expires colour/seconds, not a known junction's geometry.
                    // The ordinary gate/stop lease still controls the neutral frame.
                    outputDelay.reset();BleLink.clearTrafficLight();sent=false
                }
                upstreamDirections=emptyList()
                Log.i("MotoNavLightOverlay",JSONObject().put("event","source-clear")
                    .put("format",if(json!=null)"camera-list" else "navigation-single")
                    .put("remainingDirections",JSONArray(junction.snapshots(now()).map{it.dir})).toString())
                message="当前来源无灯数据，保留另一路有效快照"
                return
            }
            val lights=if(json!=null) {
                require(json.length<=16000)
                val a=JSONArray(json);require(a.length()<=32)
                Array(a.length()){idx-> val o=a.getJSONObject(idx)
                    TrafficSignalState.Light(o.getInt("status"),o.getInt("countdown"),TrafficSignalPolicy.cameraDirection(o.getInt("dir")),o.optInt("waitNum",0),o.optInt("showType",0))}
            }else arrayOf(TrafficSignalState.Light(i.getIntExtra("trafficLightStatus",-999),
                i.getIntExtra("redLightCountDownSeconds",-1),TrafficSignalPolicy.standardDirection(i.getIntExtra("dir",-1)),i.getIntExtra("waitRound",0),0))
            val eligible=updateGate()
            upstreamDirections=lights.map{it.dir}
            fun record(matched:Boolean) {
                Log.i("MotoNavLightOverlay",JSONObject().put("event","upstream").put("count",lights.size)
                    .put("packet",packets).put("near",gateWasOpen).put("eligible",eligible)
                    .put("wanted",wantedDirection).put("directions",JSONArray(lights.map{it.dir}))
                    .put("format",if(json!=null)"camera-list" else "navigation-single")
                    .put("rawDir",i.getIntExtra("dir",-1)).put("carAligned",CarNavSync.aligned())
                    .put("lights",JSONArray(lights.map{JSONObject().put("dir",it.dir).put("status",it.status).put("seconds",it.seconds)}))
                    .put("gate",debug())
                    .put("matched",matched).toString())
            }
            if(!eligible){
                // Even during alignment jitter, a contradictory incoming phase
                // must never leave the old colour on screen for the grace period.
                val old=junction.snapshots(now())
                if(lights.any{fresh->old.any{it.dir==fresh.dir && it.status!=fresh.status}})
                    clearPhase("灯色变化但路口尚未对齐，等待可靠快照")
                record(false);message="等待可靠的路口信息";return
            }
            // Keep source snapshots separate. A legacy single-light packet must
            // never erase/merge a fresh multi-light junction from the list callback.
            require(junction.accept(json!=null,lights,now()))
            val matched=junction.snapshots(now()).isNotEmpty()
            record(matched)
            message=if(matched)"已收到前方路口信息" else "当前路口暂无可用读秒"
        } catch(e:Exception){
            junction.reject(listSource)
            val remaining=junction.snapshots(now())
            if(remaining.isEmpty()){
                outputDelay.reset();BleLink.clearTrafficLight();sent=false
            }
            // The two callback formats have independent freshness/phase clocks.
            // Invalid legacy fields must not wipe a valid multi-light snapshot.
            message="当前来源灯数据无效，保留另一路有效快照"
            Log.w("MotoNavLightOverlay",JSONObject().put("event","source-invalid")
                .put("format",if(listSource)"camera-list" else "navigation-single")
                .put("error",e.javaClass.simpleName)
                .put("remainingDirections",JSONArray(remaining.map{it.dir})).toString())
        }
    }
    private fun directDistance()=if(SingleEngineSession.owns)SingleEngineSession.signalDistance() else if(WebMotorGuide.active)WebMotorSignals.distance() else NaviBridge.trafficLightOverlayDistance()
    private fun overlayDistance():Int {
        val direct=directDistance()
        return if(direct>=0)direct else if(contextActive())stopped.frameDistance(overlayKey(),now()) else -1
    }
    private fun overlayKey()=if(SingleEngineSession.owns)SingleEngineSession.approachKey() else if(WebMotorGuide.active)WebMotorSignals.approachKey() else NaviBridge.trafficLightApproachKey()
    private fun overlayTurn()=if(WebMotorGuide.active)WebMotorGuide.direction else NaviBridge.uiDir
    private fun overlayTurnMetres()=if(WebMotorGuide.active)WebMotorGuide.turnMeters else NaviBridge.uiDist
    private fun updateGate():Boolean {
        if(SingleEngineSession.owns) return singleEngineGate()
        if(!contextActive())stopped.clear()
        val metres=overlayDistance()
        val near=enabled && metres>=0
        val wanted=if(near)TrafficSignalPolicy.desiredMovement(overlayTurn(),overlayTurnMetres(),metres) else 0
        val key=overlayKey()
        if(key!=approachKey) {
            stopped.clear()
            Log.i("MotoNavLightOverlay",JSONObject().put("event","approach-change")
                .put("previous",approachKey).put("current",key).put("metres",metres).toString())
            outputDelay.reset()
            frameOpened=false;frameSent=false;frameId=if(frameId==Int.MAX_VALUE)1 else frameId+1
            BleLink.sendTrafficApproach(0)
            // A new approach must wait for a new upstream snapshot, not replay a previous light.
            junction.clear();BleLink.clearTrafficLight();sent=false;approachKey=key
        }
        if(near!=gateWasOpen || wanted!=wantedDirection) {
            gateWasOpen=near;wantedDirection=wanted
            Log.i("MotoNavLightOverlay",JSONObject().put("event","proximity").put("visibleRange",near)
                .put("metres",metres).put("wanted",wanted).put("turn",overlayTurn())
                .put("turnMetres",overlayTurnMetres()).put("approachKey",key).toString())
        }
        // Movement is used only to check that both engines approach the same signal;
        // it does not select which of that junction's lights will be displayed.
        val positionEvidence=directDistance()>=0 || stoppedAlignmentHeld()
        val eligible=near && positionEvidence && wanted>0 && CarNavSync.allows(wanted,metres)
        if(eligible && CarNavSync.hasFreshGuide() && directDistance()>=0)stopped.remember(key,metres,now())
        // A fresh contradictory car route invalidates remembered alignment immediately.
        if(!eligible && near && CarNavSync.hasFreshGuide() && positionEvidence)stopped.invalidateAlignment()
        return eligible
    }
    private fun singleEngineGate():Boolean {
        val key=SingleEngineSession.approachKey()
        if(key!=approachKey) {
            junction.clear();outputDelay.reset();BleLink.clearTrafficLight();sent=false
            frameOpened=false;frameSent=false;frameId=if(frameId==Int.MAX_VALUE)1 else frameId+1
            BleLink.sendTrafficApproach(0);approachKey=key
        }
        // A single engine supplies both guidance and lights. Do not compare it
        // against a stopped SDK route, or invent a second route position.
        val metres=overlayDistance()
        gateWasOpen=enabled && SingleEngineSession.nativeRunning && key.isNotEmpty() && metres in 0..300
        if(gateWasOpen&&directDistance()>=0)stopped.remember(key,metres,now())
        wantedDirection=TrafficSignalPolicy.desiredMovement(SingleEngineSession.direction,SingleEngineSession.turnMetres,0)
        return gateWasOpen
    }
    fun snapshots():Array<TrafficSignalPacket>{
        if(!updateGate())return emptyArray()
        return junction.snapshots(now())
    }
    fun frameVisible()=enabled && gateWasOpen && frameOpened && approachKey.isNotEmpty()
    private val tick=object:Runnable {override fun run(){
        val verified=BleLink.verified
        if(verified!=wasVerified){clear("蓝牙状态变化，等待新的灯数据");wasVerified=verified}
        val s=snapshots()
        if(s.isNotEmpty() && verified)frameOpened=true
        // Keep only neutral geometry during a missing/stale snapshot. The phase
        // itself still expires after 3s; a new link/stop/disconnect closes both.
        val keepFrame=verified && enabled && gateWasOpen && frameOpened && approachKey.isNotEmpty()
        if(keepFrame && (!frameSent || now()-frameAt>=500)) {
            BleLink.sendTrafficApproach(frameId);frameSent=true;frameAt=now()
        } else if(!keepFrame && frameSent) {
            BleLink.sendTrafficApproach(0);frameSent=false
        }
        if(s.isNotEmpty() && verified) {
            outputDelay.reset();BleLink.sendTrafficLight(TrafficSignalBatchPacket(s,now()));sent=true
        } else if(sent && outputDelay.due(now())) {
            Log.i("MotoNavLightOverlay",JSONObject().put("event","output-clear")
                .put("reason","alignment/snapshot unavailable for 1000ms")
                .put("car",CarNavSync.debug()).toString())
            BleLink.clearTrafficLight();sent=false;outputDelay.reset()
        } else if(!sent)outputDelay.reset()
        if(receiver!=null)handler.postDelayed(this,250)
    }}
    fun summary():String {
        if(!enabled)return "已关闭"
        val s=snapshots()
        if(SingleEngineSession.owns)return if(s.isNotEmpty()) "单引擎读秒 · "+s.joinToString("　"){TrafficSignalPolicy.directionLabel(it.dir)+" "+phaseText(it)} else "等待当前导航引擎的真实灯数据"
        if(s.isNotEmpty())return "当前路口 · 前方 ${overlayDistance()} 米\n"+
            listOf(1,4,2).joinToString("　"){dir->
                val p=s.firstOrNull{it.dir==dir}
                "${TrafficSignalPolicy.directionLabel(dir)} ${p?.let{phaseText(it)}?:"暂无数据"}"
            }
        val reason=when {
            !gateWasOpen -> "等待前方 300 米内的可靠信号灯距离"
            !CarNavSync.aligned() -> CarNavSync.summary()
            wantedDirection==0 -> "通过方向尚不确定，已隐藏灯卡片"
            else -> "当前路口暂无灯快照，已隐藏"
        }
        return reason
    }
    fun debug()=JSONObject().put("directDistance",directDistance()).put("contextDistance",overlayDistance())
        .put("stoppedAlignment",stoppedAlignmentHeld()).put("frameOpened",frameOpened)
        .put("frameSent",frameSent).put("frameId",frameId).put("near",gateWasOpen)
        .put("phaseCount",junction.snapshots(now()).size).put("approachKey",approachKey)
        .put("packets",packets).put("upstreamAgeMs",if(upstreamAt>0)now()-upstreamAt else -1)
        .put("message",message)
    fun color(status:Int)=when(status){1->Color.rgb(255,88,91);4->Color.rgb(80,225,143);-1->Color.rgb(255,210,65);else->Color.LTGRAY}
    fun phaseText(p:TrafficSignalPacket):String{
        val phase=when(p.status){1->"红";4->"绿";-1->"黄";else->"未知"}
        val seconds=p.seconds(now())
        val known=p.status==1||p.status==4||p.status== -1
        return "$phase ${if(known&&seconds>0)"${seconds}s" else "—"}"+(if(p.wait>0)" · 等${p.wait}轮" else "")
    }
}

/** Compact label, lifecycle-bound updates; no system overlay permission or extra service. */
class TrafficLightBadge(c:Context):TextView(c) {
    private val handler=Handler(Looper.getMainLooper())
    private fun dp(v:Int)=(v*resources.displayMetrics.density+0.5f).toInt()
    init {
        gravity=Gravity.START;typeface=Typeface.DEFAULT_BOLD;textSize=13f;setTextColor(Color.WHITE)
        setPadding(dp(10),dp(7),dp(10),dp(7));visibility=View.GONE
        elevation=dp(6).toFloat()
        background=GradientDrawable().apply{setColor(0xee171722.toInt());cornerRadius=dp(13).toFloat();setStroke(dp(1),Color.GRAY)}
    }
    private val refresh=object:Runnable{override fun run(){
        val s=TrafficLightOverlay.snapshots()
        val visible=s.isNotEmpty() || TrafficLightOverlay.frameVisible()
        visibility=if(visible)View.VISIBLE else View.GONE
        if(visible) {
            val b=SpannableStringBuilder("当前路口\n")
            val dirs=if(s.any{it.dir==0})listOf(1,4,2,0) else listOf(1,4,2)
            dirs.forEachIndexed{index,dir->
                val p=s.firstOrNull{it.dir==dir};val start=b.length
                b.append(TrafficSignalPolicy.directionLabel(dir)).append("  ")
                    .append(p?.let{TrafficLightOverlay.phaseText(it)}?:"暂无数据")
                b.setSpan(ForegroundColorSpan(p?.let{TrafficLightOverlay.color(it.status)}?:Color.GRAY),start,b.length,Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
                if(index<dirs.lastIndex)b.append('\n')
            }
            text=b;contentDescription=b.toString()
        }
        handler.postDelayed(this,200)
    }}
    override fun onAttachedToWindow(){super.onAttachedToWindow();handler.post(refresh)}
    override fun onDetachedFromWindow(){handler.removeCallbacks(refresh);super.onDetachedFromWindow()}
}
