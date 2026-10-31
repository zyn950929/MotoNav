package com.yuminaga.motonav

import android.content.*
import android.os.*
import android.util.Log
import com.amap.api.navi.AMapNavi
import org.json.JSONObject
import java.io.File

/** Real-route handoff. Never mixes a real-position car light into SDK simulation. */
object CarNavSync {
    private val handler=Handler(Looper.getMainLooper())
    private var context:Context?=null
    private var registered=false
    var requested=false; private set
    var simulation=false; private set
    private var epoch=-1L
    private var requestedAt=0L
    private var expectedLat=Double.NaN
    private var expectedLng=Double.NaN
    private var guideAt=0L
    private var carType=-1
    private var carLat=Double.NaN
    private var carLng=Double.NaN
    private var endLat=Double.NaN
    private var endLng=Double.NaN
    private var carIcon=0
    private var carTurnMetres=-1
    private var guides=0
    private var status="未启动联动"
    private var logAt=0L
    private var carRoad=""
    private var carNextRoad=""
    private var carRemaining=-1
    private var calculating=false
    private var startSent=false
    private val startup=ProviderStartup()
    private var startupTick:Runnable?=null
    private var warmToken=0L
    private val engineState=ProviderEngineState()
    private var syncToken=0L
    private var leaseAt=0L
    private var firstStartAt=0L
    private var webRouteKey=""
    private var routeAnchors:List<DoubleArray> = emptyList()
    private var routeSource="sdk"
    private fun current()=requested && if(webRouteKey.isNotEmpty())
        WebMotorGuide.active && !WebMotorGuide.simulation && WebMotorGuide.route?.key==webRouteKey
        else epoch==NaviBridge.navigationEpoch()
    private fun now()=SystemClock.elapsedRealtime()
    private fun renewLease(c:Context){
        if(syncToken<=0||now()-leaseAt<5000)return
        val temporary=File(c.filesDir,"car-nav-sync-enabled.tmp")
        temporary.writeText("$syncToken\n${now()+15000}")
        check(temporary.renameTo(File(c.filesDir,"car-nav-sync-enabled")))
        leaseAt=now()
    }

    fun start(c:Context) {
        if(registered)return
        context=c.applicationContext
        val r=object:BroadcastReceiver(){override fun onReceive(c:Context,i:Intent){receive(i)}}
        if(Build.VERSION.SDK_INT>=33)context!!.registerReceiver(r,IntentFilter(CarEngineTap.ACTION),"com.motonav.app.fresh.permission.CAR_GUIDANCE",handler,Context.RECEIVER_NOT_EXPORTED)
        else context!!.registerReceiver(r,IntentFilter(CarEngineTap.ACTION),"com.motonav.app.fresh.permission.CAR_GUIDANCE",handler)
        registered=true
    }
    private fun send(i:Intent) {
        val c=context?:return
        i.action="AUTONAVI_STANDARD_BROADCAST_RECV"
        i.setPackage(c.packageName)
        i.putExtra("SOURCE_APP","MotoNav fusion")
        c.sendBroadcast(i)
    }
    private fun muteProvider() {
        // Confirmed in this provider's NaviMuteAction: 10047, int 1 = mute.
        // The navigation and cruise settings are distinct; neither changes system audio.
        send(Intent().putExtra("KEY_TYPE",10047).putExtra("EXTRA_MUTE",1))
        send(Intent().putExtra("KEY_TYPE",10047).putExtra("EXTRA_CASUAL_MUTE",1))
        Log.i("MotoNavCarSync","provider mute requested: navigation=1 cruise=1")
    }
    /** Uses the current selected SDK path, not a stale destination preference. */
    fun wants(c:Context)=TrafficLightOverlay.enabled && c.packageName=="com.motonav.app.fresh" &&
        NaviBridge.directRouteMode!=LocalRouting.BICYCLE && ShareGuide.route?.isTravel!=true
    fun request(c:Context,n:AMapNavi,emulate:Boolean):Boolean {
        stop()
        start(c)
        if(!wants(c))return false
        FusionCarUi.prepare(c)
        muteProvider()
        if(emulate){
            send(Intent().putExtra("KEY_TYPE",10010))
            status="SDK 模拟位置与真实灯源不同，模拟时不显示读秒"
            return false
        }
        val path=try{n.naviPath?.coordList?.toList()}catch(_:Exception){null}?:return false
        if(path.size<2)return false
        requestPath(c,path.map{doubleArrayOf(it.latitude,it.longitude)},intArrayOf())
        return false // SDK retains ordinary GPS navigation; no external position substitution.
    }
    /** Constrain the background car replan; MotoNav keeps the selected motorcycle geometry. */
    fun requestWeb(c:Context,route:WebMotorRoute,emulate:Boolean) {
        stop();start(c)
        routeSource="web-motorcycle"
        if(!TrafficLightOverlay.enabled || c.packageName!="com.motonav.app.fresh"){status="后台路线同步已关闭";return}
        if(emulate){status="网页模拟未同步真实位置灯源";return}
        webRouteKey=route.key
        val track=route.tracker()
        val explicit=route.request.vias.mapNotNull { via ->
            val progress=track.nearestProgress(via.lat,via.lng)
            val point=track.at(progress)
            if(CarSignalAlignment.metres(via.lat,via.lng,point[0],point[1])>100)null
            else track.cum.indexOfFirst{it>=progress}.takeIf{it>=0}
        }.toIntArray()
        FusionCarUi.prepare(c);muteProvider()
        requestPath(c,route.points,explicit)
    }
    private fun requestPath(c:Context,path:List<DoubleArray>,explicit:IntArray) {
        if(path.size<2)return
        val first=path.first();val last=path.last()
        if(!CarSignalAlignment.coordinate(first[0],first[1])||!CarSignalAlignment.coordinate(last[0],last[1]))return
        TrafficPositionMonitor.start(c)
        requested=true;simulation=false;epoch=NaviBridge.navigationEpoch();requestedAt=now()
        syncToken=maxOf(now(),syncToken+1);leaseAt=0;firstStartAt=0;engineState.reset(syncToken)
        renewLease(c)
        expectedLat=last[0];expectedLng=last[1]
        guideAt=0;guides=0
        status="正在准备路口信息"
        calculating=false;startSent=false
        send(Intent().putExtra("KEY_TYPE",10010))
        val i=Intent().putExtra("KEY_TYPE",10007)
            .putExtra("EXTRA_SLAT",first[0]).putExtra("EXTRA_SLON",first[1])
            .putExtra("EXTRA_SNAME","MotoNav 起点")
            .putExtra("EXTRA_DLAT",last[0]).putExtra("EXTRA_DLON",last[1])
            .putExtra("EXTRA_DNAME","MotoNav 同步终点").putExtra("EXTRA_DEV",0)
            .putExtra("EXTRA_M",-1)
        // Preserve the existing SDK selection; web routes prioritize their detours/vias.
        routeAnchors=if(webRouteKey.isNotEmpty()) {
            val turns=WebMotorGuide.route?.takeIf{it.key==webRouteKey}?.steps.orEmpty()
                .filter{it.dir!="S"}.map{it.endIndex}.toIntArray()
            WebRouteAnchors.select(path.toTypedArray(),explicit,turns).toList()
        } else {
            val cumulative=DoubleArray(path.size)
            for(k in 1 until path.size)cumulative[k]=cumulative[k-1]+CarSignalAlignment.metres(path[k-1][0],path[k-1][1],path[k][0],path[k][1])
            if(cumulative.last()>1000)(1..3).map{part->path[cumulative.indexOfFirst{it>=cumulative.last()*part/4.0}.coerceAtLeast(0)]}
            else emptyList()
        }
        val anchors=listOf("F","S","T")
        // 10007 ignores the via fields. 10032 is handled by jb0 and reads all three.
        i.putExtra("KEY_TYPE",if(routeAnchors.isNotEmpty())10032 else 10007)
        routeAnchors.forEachIndexed{a,point->
            i.putExtra("EXTRA_${anchors[a]}MIDLAT",point[0])
                .putExtra("EXTRA_${anchors[a]}MIDLON",point[1])
                .putExtra("EXTRA_${anchors[a]}MIDNAME","MotoNav 路线点${a+1}")
        }
        val token=epoch;val routeKey=webRouteKey;val session=syncToken
        fun valid()=current()&&epoch==token&&webRouteKey==routeKey&&syncToken==session
        startup.begin(now())
        val task=object:Runnable {override fun run(){
            if(!valid())return
            runCatching{renewLease(c)}.onFailure{Log.w("MotoNavCarSync","engine-state lease",it)}
            if(startup.running()&&engineState.stopped(now())){
                if(startup.recoverStoppedEngine(now())){
                    guideAt=0;startSent=false;firstStartAt=0;engineState.reset(syncToken)
                    Log.w("MotoNavCarSync","confirmed native stop; bounded recovery=${startup.recoveries()}")
                }else{
                    status="路口导航连续中断，请重新开始导航"
                    File(c.filesDir,"car-nav-sync-enabled").delete();return
                }
            }
            val acknowledged=startSent&&engineState.realStarted(firstStartAt,now())
            when(startup.next(now(),FusionCarUi.prepared(c,warmToken),acknowledged)){
                ProviderStartup.WARM->{
                    warmToken=now();FusionCarUi.warm(c,warmToken)
                    status="正在初始化路口信息服务"
                }
                ProviderStartup.PLAN->{
                    requestedAt=now();guideAt=0;calculating=true;startSent=false;firstStartAt=0
                    muteProvider();send(i)
                    Log.i("MotoNavCarSync","route-request epoch=$token source=$routeSource points=${path.size} anchors=${routeAnchors.size} attempt=${startup.plans()}")
                }
                ProviderStartup.START->{
                    if(!startSent)firstStartAt=now()
                    startSent=true;muteProvider()
                    // 10032 leaves a planned-route preview. Select that route and
                    // explicitly start it; a generic navigation operation can be lost
                    // while the HMI is still changing from calculation to preview.
                    send(Intent().putExtra("KEY_TYPE",10055).putExtra("EXTRA_CHANGE_ROAD",1)
                        .putExtra("EXTRA_START_NAVI",true))
                    status="等待路口导航确认"
                    Log.i("MotoNavCarSync","start-navigation epoch=$token attempt=${startup.starts()}")
                }
                ProviderStartup.FAILED->{
                    File(c.filesDir,"car-nav-sync-enabled").delete()
                    status="路口服务未确认启动，请检查首次授权或重新开始导航";return
                }
            }
            if(startup.running()&&acknowledged&&status=="等待路口导航确认"){
                status="路口导航已启动，等待前方路口数据"
                Log.i("MotoNavCarSync","navigation-acknowledged epoch=$token realEngine=true guides=$guides")
            }
            handler.postDelayed(this,if(startup.running())1000 else 500)
        }}
        startupTick=task;handler.post(task)
    }

    fun standard(i:Intent){
        if(!current() || i.getIntExtra("KEY_TYPE",-1)!=10019)return
        when(i.getIntExtra("EXTRA_STATE",-1)){
            5 -> {calculating=true;status="正在准备路口信息"}
            6 -> {startup.calculated();status="路口路线已就绪，正在启动导航"}
            7 -> {calculating=false;startup.planFailed();status="路口算路未成功，正在重试"}
            9,12 -> guideAt=0
        }
    }
    fun stop(expectedEpoch:Long?=null) {
        if(expectedEpoch!=null&&(webRouteKey.isNotEmpty()||expectedEpoch!=epoch))return
        val owned=requested
        startupTick?.let{handler.removeCallbacks(it)};startupTick=null;startup.stop()
        TrafficPositionMonitor.stop()
        context?.let{FusionCarUi.finishWarm(it,warmToken)}
        context?.let{File(it.filesDir,"car-nav-sync-enabled").delete();File(it.filesDir,"car-nav-sync-enabled.tmp").delete()}
        engineState.reset(0);firstStartAt=0;leaseAt=0
        requested=false;guideAt=0;status="联动已停止"
        webRouteKey="";routeAnchors=emptyList();routeSource="sdk"
        if(owned)send(Intent().putExtra("KEY_TYPE",10010))
    }
    private fun receive(i:Intent) {
        if(i.getStringExtra("kind")=="engine"){
            if(i.getStringExtra("providerProcess")!=context?.packageName)return
            if(current())engineState.accept(i.getLongExtra("syncToken",0),i.getLongExtra("at",0),
                i.getBooleanExtra("real",false),i.getBooleanExtra("simulation",false),now())
            return
        }
        if(i.hasExtra("kind")&&i.getStringExtra("kind")!="guide")return
        if(!current())return
        if(!startup.acceptsGuide())return
        val stamp=i.getLongExtra("at",0)
        if(stamp<requestedAt || now()-stamp !in 0..2500)return
        guides++
        guideAt=stamp;carType=i.getIntExtra("type",-1)
        carLat=i.getDoubleExtra("lat",Double.NaN);carLng=i.getDoubleExtra("lng",Double.NaN)
        endLat=i.getDoubleExtra("endLat",Double.NaN);endLng=i.getDoubleExtra("endLng",Double.NaN)
        carIcon=i.getIntExtra("icon",0);carTurnMetres=i.getIntExtra("turnMetres",-1)
        carRoad=i.getStringExtra("road").orEmpty();carNextRoad=i.getStringExtra("nextRoad").orEmpty()
        carRemaining=i.getIntExtra("remaining",-1)
        val destination=CarSignalAlignment.metres(expectedLat,expectedLng,endLat,endLng)<=100
        val correctMode=carType==(if(simulation)1 else 0)
        status=when{
            !destination->"正在核对目的地"
            !correctMode->"等待路口信息服务就绪"
            else->"正在核对前方路口"
        }
        if(now()-logAt>=1500){logAt=now();Log.i("MotoNavCarSync",debug().toString())}
    }
    /** Route agreement alone is NOT evidence of a signal location on a web route. */
    fun hasFreshGuide()=guideAt>=requestedAt && guideAt>0 && now()-guideAt in 0..2999
    private fun stoppedGuide():Boolean {
        if(!current() || guideAt<requestedAt || guideAt==0L || carType!=0 ||
            CarSignalAlignment.metres(expectedLat,expectedLng,endLat,endLng)>100 || !TrafficLightOverlay.stoppedAlignmentHeld())return false
        if(!hasFreshGuide())return true
        // A GPS-proven stop at the previously matched junction can outlive the
        // two engines' different deceleration/prediction distances. Road pair,
        // maneuver, destination and total-route proximity must still agree.
        val web=WebMotorGuide.active
        return CarSignalAlignment.stoppedGuideAgreement(carRoad,carNextRoad,carIcon,carTurnMetres,carRemaining,
            if(web)WebMotorSignals.currentRoad().ifEmpty{WebMotorGuide.currentRoad} else NaviBridge.uiCurRoad,
            if(web)WebMotorGuide.nextRoad else NaviBridge.uiNextRoad,
            if(web)WebMotorGuide.direction else NaviBridge.uiDir,
            if(web)WebMotorGuide.turnMeters else NaviBridge.trafficLightGuidanceTurnMetres(),
            if(web)WebMotorGuide.remaining else NaviBridge.uiRemainM)
    }
    fun webRouteAgreement():String {
        if(webRouteKey.isEmpty() || !current())return "inactive"
        if(!hasFreshGuide() && !stoppedGuide())return "stale-guide"
        if(carType!=0)return "mode"
        if(CarSignalAlignment.metres(expectedLat,expectedLng,endLat,endLng)>100)return "destination"
        if(!WebMotorGuide.guidanceFresh() && !TrafficLightOverlay.stoppedAlignmentHeld())return "web-position"
        if(stoppedGuide())return ""
        return CarSignalAlignment.guideDisagreement(carRoad,carNextRoad,carIcon,carTurnMetres,carRemaining,
            WebMotorSignals.currentRoad().ifEmpty{WebMotorGuide.currentRoad},WebMotorGuide.nextRoad,WebMotorGuide.direction,WebMotorGuide.turnMeters,WebMotorGuide.remaining)
    }
    fun webAnchors(key:String)=if(webRouteKey==key)routeAnchors.map{it.clone()} else emptyList()
    fun webSummary():String {
        if(webRouteKey.isEmpty() || !requested)return status
        if(!startup.running())return status
        if(guideAt==0L)return "后台导航已启动，等待路口引导"
        return if(webRouteAgreement().isEmpty())"后台路线当前路段匹配 · ${routeAnchors.size} 个关键点"
            else "后台路线核对中 · 摩托车导航继续"
    }
    fun aligned():Boolean {
        // Web signals are SDK-marked road nodes geometrically matched to the
        // unchanged motorcycle route; a step count/turn is never sufficient.
        if(WebMotorGuide.active)return webRouteKey.isNotEmpty() &&
            webRouteAgreement().isEmpty() && (WebMotorSignals.distance()>=0 || TrafficLightOverlay.stoppedAlignmentHeld())
        if(webRouteKey.isNotEmpty())return false
        if(stoppedGuide())return true
        val position=NaviBridge.trafficLightPosition()
        val direct=CarSignalAlignment.matches(requested,simulation,carType,requestedAt,guideAt,now(),
            expectedLat,expectedLng,endLat,endLng,position[0],position[1],carLat,carLng)
        if(direct)return true
        // Native channel observed 0,0 coordinates. Real navigation only: require
        // matching road pair, maneuver and both segment/whole-route distances.
        return requested&&!simulation&&carType==0&&guideAt>=requestedAt&&now()-guideAt in 0..2999&&
            CarSignalAlignment.metres(expectedLat,expectedLng,endLat,endLng)<=100&&
            !CarSignalAlignment.coordinate(carLat,carLng)&&
            CarSignalAlignment.guideAgreement(carRoad,carNextRoad,carIcon,carTurnMetres,carRemaining,
                NaviBridge.uiCurRoad,NaviBridge.uiNextRoad,NaviBridge.uiDir,NaviBridge.trafficLightGuidanceTurnMetres(),NaviBridge.uiRemainM)
    }
    fun allows(wanted:Int,lightMetres:Int):Boolean {
        if(!aligned())return false
        val ownTurn=if(WebMotorGuide.active)WebMotorGuide.turnMeters else NaviBridge.uiDist
        val held=stoppedGuide()
        // Eligibility was established in the SDK's 300m range. Translation can
        // exceed 300m by the permitted sampling skew; only the offset is needed.
        // Both engines must approach the same maneuver, at a similar distance.
        if(!CarSignalAlignment.movementAgreement(wanted,lightMetres,ownTurn,carTurnMetres,
                CarSignalAlignment.turn(carIcon),held))return false
        return wanted==4 || kotlin.math.abs(carTurnMetres-(if(WebMotorGuide.active)WebMotorGuide.turnMeters else NaviBridge.trafficLightGuidanceTurnMetres()))<=35 || held
    }
    fun summary():String=if(requested&&!startup.running()&&startup.plans()==0&&now()-requestedAt>30000&&guideAt==0L)"路口信息服务暂未就绪，可在高级诊断完成首次初始化" else status
    fun debug()=JSONObject().put("source",routeSource).put("webRouteKey",webRouteKey)
        .put("anchors",org.json.JSONArray(routeAnchors.map{org.json.JSONArray(it.toList())}))
        .put("webAgreement",if(webRouteKey.isNotEmpty())webRouteAgreement() else "")
        .put("webRoad",WebMotorGuide.currentRoad).put("webNextRoad",WebMotorGuide.nextRoad)
        .put("webTurn",WebMotorGuide.direction).put("webTurnMetres",WebMotorGuide.turnMeters).put("webRemaining",WebMotorGuide.remaining)
        .put("requested",requested).put("simulation",simulation).put("epoch",epoch)
        .put("engineReady",context?.let{FusionCarUi.prepared(it,warmToken)}?:false)
        .put("startupPlans",startup.plans()).put("startupStarts",startup.starts()).put("startupAcknowledged",startup.running())
        .put("nativeStateFresh",engineState.fresh(now())).put("nativeReal",engineState.real())
        .put("nativeSimulation",engineState.simulation()).put("startupRecoveries",startup.recoveries())
        .put("guides",guides).put("carType",carType).put("carIcon",carIcon)
        .put("carCoordinatePresent",CarSignalAlignment.coordinate(carLat,carLng))
        .put("carTurnMetres",carTurnMetres).put("guideAgeMs",if(guideAt>0)now()-guideAt else -1)
        .put("carRoad",carRoad).put("carNextRoad",carNextRoad).put("carRemaining",carRemaining)
        .put("sdkRoad",NaviBridge.uiCurRoad).put("sdkNextRoad",NaviBridge.uiNextRoad)
        .put("sdkTurn",NaviBridge.uiDir).put("sdkTurnMetres",NaviBridge.uiDist).put("sdkRemaining",NaviBridge.uiRemainM)
        .put("sdkRawTurnMetres",NaviBridge.trafficLightGuidanceTurnMetres())
        .put("guideDisagreement",CarSignalAlignment.guideDisagreement(carRoad,carNextRoad,carIcon,carTurnMetres,carRemaining,
            NaviBridge.uiCurRoad,NaviBridge.uiNextRoad,NaviBridge.uiDir,NaviBridge.trafficLightGuidanceTurnMetres(),NaviBridge.uiRemainM))
        .put("destinationMatched",CarSignalAlignment.metres(expectedLat,expectedLng,endLat,endLng)<=100)
        .put("positionGap",CarSignalAlignment.metres(NaviBridge.trafficLightPosition()[0],NaviBridge.trafficLightPosition()[1],carLat,carLng).takeIf{it.isFinite()}?:-1)
        .put("aligned",aligned()).put("status",summary())
}
