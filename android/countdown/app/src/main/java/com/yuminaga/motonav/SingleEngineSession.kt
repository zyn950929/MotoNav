package com.yuminaga.motonav

import android.content.*
import android.os.*
import android.util.Log
import org.json.JSONObject
import java.io.File
import java.util.Calendar
import kotlin.math.roundToInt

/** Experimental sole guidance owner. Never calls AMapNavi.getInstance/startNavi. */
object SingleEngineSession {
    const val EXPERIMENT = true
    private val handler=Handler(Looper.getMainLooper())
    private var context:Context?=null
    private var receiver:BroadcastReceiver?=null
    var owns=false;private set
    var nativeRunning=false;private set
    var simulation=false;private set
    var direction="S";private set
    var turnMetres=-1;private set
    var road="";private set
    var nextRoad="";private set
    var speed=0;private set
    var remaining=0;private set
    var remainingSeconds=0;private set
    var lanes=IntArray(0);private set
    var lights=0;private set
    var cameraLimit=0;private set
    var cameraDistance=0;private set
    var guideCount=0;private set
    var positionCount=0;private set
    var laneCount=0;private set
    var routePoints=0;private set
    var status="未启动";private set
    var lat=Double.NaN;private set
    var lng=Double.NaN;private set
    private var armedAt=0L
    private var engineAt=0L
    private var guideAt=0L
    private var fixAt=0L
    var bearing=0f;private set
    private var segment=-1
    private var turnBase=0
    private var lastRoad=""
    private var lastFooter=""
    private var lastLane=""
    private var route:List<DoubleArray> = emptyList()
    private var routeId=""
    private var routeSent=false
    private var shapeVerified=false
    private var nativeRouteStatus="尚未读取"
    private var currentEndLat=Double.NaN
    private var currentEndLng=Double.NaN
    private var pendingShape:JSONObject?=null
    private var routeLights:List<DoubleArray> = emptyList()
    private var signalGeometry:NativeSignalGeometry?=null
    private var signalIndex=-1
    private var signalMetres=-1
    private var signalFixAt=0L
    private var clearRouteAtPose=true
    private var lastMute:Int?=null
    private var askedRouteInfo=false
    private var routeInfoRequests=0
    private var routeInfoAt=0L
    private var routeInfoReplies=0
    private var lastVoiceKey=""
    private var gpsSpeedAt=0L
    private val lifecycle=SingleEngineLifecycle()
    val arrived get()=lifecycle.arrived()
    val rerouting get()=lifecycle.rerouting()
    private var minimumShapeAt=0L
    private var tripFinishedAt=0L
    private var tripStart=0L
    private var tripMetres=0.0
    private var tripMax=0
    private var tripLat=Double.NaN
    private var tripLng=Double.NaN
    private var routeChanges=0
    private var arrivalCount=0
    private val startup=ProviderStartup()
    private var pendingPlan:Intent?=null
    private var warmToken=0L
    private var targetLat=Double.NaN
    private var targetLng=Double.NaN
    private var targetMatches=false
    private var planStartLat=0.0
    private var planStartLng=0.0
    var waypoints:List<MotorPoint> = emptyList();private set
    private var targetName="目的地"
    private var targetAddress=""
    var choices:List<NativeRouteChoice> = emptyList();private set
    var selectedChoice=0;private set
    val choosing get()=startup.choosing()
    private var selectOnStart=false
    private var choiceRequestedAt=0L
    private var choiceReads=0
    private var previewRoute:List<DoubleArray> = emptyList()
    private var previewId=""
    private var previewSelected=-1
    private var expectedRouteId=""
    private var choiceConfirmedAt=0L
    private var requestSimulation=false
    var simulationSpeedKmh=60;private set
    var appliedSimulationSpeed=0;private set
    var simulationSpeedPending=false;private set
    var simulationSpeedStatus="";private set
    private var speedRequest=""
    private var speedRequestedAt=0L
    private var simulationDisplay:NativeSimulationDisplay?=null
    private var previewReadStamp=0L
    var sharedRoute:AmapShare.Route?=null;private set
    private var sharedMatch:SharedRouteComparison.Result?=null
    private var matchPreview=""
    private val shareWorker=java.util.concurrent.Executors.newSingleThreadExecutor()
    private var shareCheck:java.util.concurrent.Future<*>?=null
    val sharedNeedsConfirmation get()=sharedRoute?.endpointsOnly==true || sharedMatch?.needsConfirmation()==true
    val sharedRouteNote get()=sharedRoute?.let { r ->
        when {
            r.endpointsOnly -> "仅导入${r.kind}起终点，当前彩色路线为重新规划的路线"
            sharedMatch==null -> "正在核对原分享线路与当前路线…"
            else -> "高德${r.kind}分享 · 金色虚线为原线路，粉色为导航线\n线路匹配约 ${((sharedMatch?.coverage ?: 0.0)*100).toInt()}%"+
                if(sharedNeedsConfirmation)" · 存在变化，请核对后再开始" else ""
        }
    }.orEmpty()
    val waypointSearchCenter get()=previewRoute.firstOrNull()?.let{MotorPoint(it[0],it[1])}
        ?: if(CarSignalAlignment.coordinate(planStartLat,planStartLng))MotorPoint(planStartLat,planStartLng) else null
    val destinationTitle get()=targetName
    val choiceReady get()=choosing&&choices.isNotEmpty()&&previewSelected==selectedChoice&&previewRoute.isNotEmpty() && (sharedRoute==null || sharedRoute?.endpointsOnly==true || (sharedMatch!=null && matchPreview==previewId))
    private fun now()=SystemClock.elapsedRealtime()
    fun arm(c:Context) {
        if(owns)return
        context=c.applicationContext
        simulationSpeedKmh=SimulationSpeedPolicy.normalize(c.getSharedPreferences("motonav",0).getInt("single_simulation_speed",60))
        WebMotorGuide.stop()
        runCatching{NaviBridge.releaseForSingleEngine()}.onFailure{
            status="原导航引擎未完全退出，请重开 App 后测试";return
        }
        CarNavSync.stop()
        NavService.start(c)
        TrafficLightOverlay.start(c);TrafficLightOverlay.enable(c,true)
        owns=true;nativeRunning=false;armedAt=now();engineAt=0;guideAt=0;fixAt=0
        resetTrip()
        guideCount=0;positionCount=0;laneCount=0;routePoints=0;route=emptyList();routeSent=false
        shapeVerified=false;pendingShape=null;nativeRouteStatus="等待唯一导航引擎";routeLights=emptyList()
        currentEndLat=Double.NaN;currentEndLng=Double.NaN
        signalGeometry=null;signalIndex=-1;signalMetres=-1;signalFixAt=0
        lat=Double.NaN;lng=Double.NaN;segment=-1;lastRoad="";lastFooter="";lastLane=""
        clearRouteAtPose=true;lastMute=null;askedRouteInfo=false;routeInfoRequests=0;routeInfoAt=0;routeInfoReplies=0;lastVoiceKey=""
        File(c.filesDir,"single-engine-enabled").writeText(armedAt.toString())
        File(c.filesDir,"single-engine-route.json").delete()
        val r=object:BroadcastReceiver(){override fun onReceive(c:Context,i:Intent){receive(i)}}
        if(Build.VERSION.SDK_INT>=33)context!!.registerReceiver(r,IntentFilter(CarEngineTap.ACTION),
            "com.motonav.app.fresh.permission.CAR_GUIDANCE",handler,Context.RECEIVER_NOT_EXPORTED)
        else context!!.registerReceiver(r,IntentFilter(CarEngineTap.ACTION),"com.motonav.app.fresh.permission.CAR_GUIDANCE",handler)
        receiver=r
        MapFeeder.clearSingleEngineRoute();MapFeeder.onNaviStart()
        BleLink.send("C");BleLink.send("#z,0");BleLink.send("#j");BleLink.send("#k");BleLink.send("#c");BleLink.send("#g")
        TrafficPositionMonitor.start(c)
        status="已关闭 MotoNav 导航引擎，请在内置导航选择目的地"
        NavService.updateWakeLock()
        handler.post(tick)
        Log.i("MotoNavSingleEngine","armed; SDK navigation released; native engine is sole guidance owner")
    }
    fun disarm(stopNative:Boolean=true) {
        if(!owns)return
        shareCheck?.cancel(true);shareCheck=null;sharedRoute=null;sharedMatch=null;matchPreview=""
        queueSimulationStop();clearSimulationSpeed()
        owns=false;nativeRunning=false;handler.removeCallbacks(tick)
        receiver?.let{runCatching{context?.unregisterReceiver(it)}};receiver=null
        context?.let{c->
            File(c.filesDir,"single-engine-enabled").delete()
            File(c.filesDir,"single-engine-preview-enabled").delete()
            File(c.filesDir,"single-engine-simulation-command.json").delete()
            if(stopNative)c.sendBroadcast(Intent("AUTONAVI_STANDARD_BROADCAST_RECV").setPackage(c.packageName).putExtra("KEY_TYPE",10010))
        }
        TrafficPositionMonitor.stop();BleLink.send("C");BleLink.clearTrafficLight();BleLink.sendTrafficApproach(0)
        RideRecorder.stop();lifecycle.cancel();MapFeeder.clearSingleEngineRoute()
        BleLink.send("#j");BleLink.send("#k");BleLink.send("#c");BleLink.send("#y,0");BleLink.send("#x,0")
        shapeVerified=false;routeSent=false;pendingShape=null;signalGeometry=null;simulationDisplay=null
        status="已结束单引擎实验"
        waypoints=emptyList()
        startup.stop();pendingPlan=null;choices=emptyList();previewRoute=emptyList();previewId="";selectOnStart=false;requestSimulation=false;simulation=false
    }
    fun setSimulationSpeed(kmh:Int):Boolean{
        if(!SimulationSpeedPolicy.allowed(kmh)||(!choosing&&!requestSimulation))return false
        if(simulationSpeedPending)return false
        simulationSpeedKmh=kmh;simulationSpeedStatus=""
        if(nativeRunning&&simulation){
            val c=context?:return false
            speedRequest="speed:${now()}";speedRequestedAt=now();simulationSpeedPending=true
            val command=JSONObject().put("owner",armedAt.toString()).put("sequence","$armedAt:$choiceConfirmedAt")
                .put("request",speedRequest).put("at",speedRequestedAt).put("speed",kmh)
            val tmp=File(c.filesDir,"single-engine-simulation-speed.tmp")
            tmp.writeText(command.toString());check(tmp.renameTo(File(c.filesDir,"single-engine-simulation-speed.json")))
        }else context?.getSharedPreferences("motonav",0)?.edit()?.putInt("single_simulation_speed",kmh)?.apply()
        return true
    }
    private fun clearSimulationSpeed(){
        simulationSpeedPending=false;speedRequest="";appliedSimulationSpeed=0;simulationSpeedStatus=""
        context?.let{c->listOf("single-engine-simulation-speed.json","single-engine-simulation-speed-ack.json").forEach{File(c.filesDir,it).delete()}}
    }
    private fun readSimulationSpeedAck(){
        if(!simulationSpeedPending)return
        context?.let{c->runCatching{
            val f=File(c.filesDir,"single-engine-simulation-speed-ack.json")
            if(!f.isFile||f.length()>2048)return@runCatching
            val ack=JSONObject(f.readText())
            if(ack.optString("sequence")!="$armedAt:$choiceConfirmedAt"||ack.optString("request")!=speedRequest)return@runCatching
            simulationSpeedPending=false
            if(ack.optBoolean("applied")&&ack.optInt("speed")==simulationSpeedKmh){
                appliedSimulationSpeed=simulationSpeedKmh;simulationSpeedStatus=""
                c.getSharedPreferences("motonav",0).edit().putInt("single_simulation_speed",simulationSpeedKmh).apply()
            }else{simulationSpeedStatus="速度调整未成功";simulationSpeedKmh=SimulationSpeedPolicy.normalize(appliedSimulationSpeed)}
            f.delete()
        }}
        if(simulationSpeedPending&&now()-speedRequestedAt>7000){
            simulationSpeedPending=false;simulationSpeedStatus="速度调整未确认";simulationSpeedKmh=SimulationSpeedPolicy.normalize(appliedSimulationSpeed)
        }
    }
    private fun queueSimulationStop(){
        if(!requestSimulation||choiceConfirmedAt<=0)return
        context?.let{c->
            val data=JSONObject().put("sequence","$armedAt:$choiceConfirmedAt").put("at",now())
            val temp=File(c.filesDir,"single-engine-simulation-stop.tmp")
            temp.writeText(data.toString());check(temp.renameTo(File(c.filesDir,"single-engine-simulation-stop.json")))
        }
    }
    /** Requests only the native engine's route, with optional explicit native vias. */
    fun plan(c:Context,fromLat:Double,fromLng:Double,toLat:Double,toLng:Double,vias:List<DoubleArray> = emptyList(),choose:Boolean=false) {
        require(((fromLat==0.0&&fromLng==0.0)||CarSignalAlignment.coordinate(fromLat,fromLng))&&CarSignalAlignment.coordinate(toLat,toLng)&&vias.size<=3&&vias.all{it.size>=2&&CarSignalAlignment.coordinate(it[0],it[1])})
        queueSimulationStop();clearSimulationSpeed()
        shareCheck?.cancel(true);shareCheck=null;sharedRoute=null;sharedMatch=null;matchPreview=""
        arm(c);if(!owns)return
        resetTrip();TrafficPositionMonitor.start(c)
        planStartLat=fromLat;planStartLng=fromLng;waypoints=emptyList()
        targetLat=toLat;targetLng=toLng;targetMatches=false;targetName="目的地";targetAddress=""
        armedAt=now();File(c.filesDir,"single-engine-enabled").writeText(armedAt.toString())
        pendingShape=null;shapeVerified=false;routeSent=false;routePoints=0;route=emptyList();routeLights=emptyList()
        signalGeometry=null;signalIndex=-1;signalMetres=-1;signalFixAt=0
        askedRouteInfo=false;routeInfoRequests=0;routeInfoReplies=0;routeInfoAt=0;MapFeeder.clearSingleEngineRoute()
        choices=emptyList();selectedChoice=0;previewSelected=-1;previewRoute=emptyList();previewId="";expectedRouteId=""
        selectOnStart=choose;choiceRequestedAt=0;choiceReads=0;choiceConfirmedAt=0;previewReadStamp=0
        requestSimulation=false;File(c.filesDir,"single-engine-simulation-command.json").delete()
        simulationDisplay=null
        val marker=File(c.filesDir,"single-engine-preview-enabled")
        if(choose)marker.writeText(armedAt.toString()) else marker.delete()
        File(c.filesDir,"single-engine-preview.json").delete()
        // Use the native route preview (10032), retaining any via points.
        // The bundled HMI auto-start countdown is disabled by native_preview.py.
        val i=Intent().putExtra("KEY_TYPE",if(choose||vias.isNotEmpty())10032 else 10007).putExtra("EXTRA_SLAT",fromLat).putExtra("EXTRA_SLON",fromLng)
            .putExtra("EXTRA_SNAME","MotoNav 起点").putExtra("EXTRA_DLAT",toLat).putExtra("EXTRA_DLON",toLng)
            .putExtra("EXTRA_DNAME","MotoNav 终点").putExtra("EXTRA_DEV",0).putExtra("EXTRA_M",-1)
        vias.forEachIndexed{k,p->val name=listOf("F","S","T")[k]
            i.putExtra("EXTRA_${name}MIDLAT",p[0]).putExtra("EXTRA_${name}MIDLON",p[1]).putExtra("EXTRA_${name}MIDNAME","途经点${k+1}")}
        pendingPlan=i;startup.begin(now(),choose)
        status=if(choose)"正在规划备选路线" else "正在准备导航"
    }
    @Suppress("MissingPermission")
    fun planTo(c:Context,toLat:Double,toLng:Double,name:String="目的地",address:String=""){
        val lm=c.getSystemService(Context.LOCATION_SERVICE) as android.location.LocationManager
        val fix=runCatching{lm.getProviders(true).mapNotNull{lm.getLastKnownLocation(it)}.maxByOrNull{it.time}}.getOrNull()
        if(fix==null||System.currentTimeMillis()-fix.time !in 0..30000){
            // The native engine resolves its own current position for a zero start.
            plan(c,0.0,0.0,toLat,toLng,choose=true)
        }else{
            val p=MapGeo.wgsToGcj(fix.latitude,fix.longitude);plan(c,p[0],p[1],toLat,toLng,choose=true)
        }
        targetName=name.ifBlank{"目的地"};targetAddress=address
    }
    fun planShared(c:Context,route:AmapShare.Route){
        require(route.kind=="驾车" || route.kind=="摩托车")
        val anchors=if(route.endpointsOnly)emptyList() else RouteAnchorSelector.select(route.pts.toTypedArray(),intArrayOf()).map{route.pts[it]}
        plan(c,route.startLat,route.startLng,route.endLat,route.endLng,anchors,choose=true)
        sharedRoute=route;targetName=route.endName.ifBlank{"分享目的地"};targetAddress="高德${route.kind}分享"
    }
    fun setWaypoints(c:Context,points:List<MotorPoint>):Boolean{
        if(!choosing||points.size>RouteWaypointEditor.MAX||points.any{!CarSignalAlignment.coordinate(it.lat,it.lng)})return false
        if(points==waypoints)return true
        val name=targetName;val address=targetAddress;val shared=sharedRoute
        val constraints=if(points.isEmpty()&&shared!=null&&!shared.endpointsOnly)
            RouteAnchorSelector.select(shared.pts.toTypedArray(),intArrayOf()).map{shared.pts[it]}
            else points.map{doubleArrayOf(it.lat,it.lng)}
        plan(c,planStartLat,planStartLng,targetLat,targetLng,constraints,choose=true)
        waypoints=points.toList();targetName=name;targetAddress=address;sharedRoute=shared
        return true
    }
    private fun compareSharedPreview(){
        val shared=sharedRoute?:return
        if(shared.endpointsOnly)return
        val stamp=armedAt;val id=previewId;val points=previewRoute
        sharedMatch=null;matchPreview="";shareCheck?.cancel(true)
        shareCheck=shareWorker.submit{
            runCatching{SharedRouteComparison.compare(shared.pts.toTypedArray(),points.toTypedArray())}.onSuccess{result->
                handler.post{if(owns && armedAt==stamp && previewId==id && sharedRoute===shared){sharedMatch=result;matchPreview=id}}
            }.onFailure{if(it !is java.util.concurrent.CancellationException)Log.w("MotoNavShare","Shared geometry comparison failed",it)}
        }
    }
    fun selectChoice(index:Int){
        if(!choosing||index !in choices.indices)return
        shareCheck?.cancel(true);sharedMatch=null;matchPreview=""
        selectedChoice=index;previewRoute=emptyList();previewId="";previewSelected=-1;previewReadStamp=0
        command(Intent().putExtra("KEY_TYPE",10055).putExtra("EXTRA_CHANGE_ROAD",index+1).putExtra("EXTRA_START_NAVI",false))
        // Re-export after selection, including a repeat selection of the same native path.
        context?.let{File(it.filesDir,"single-engine-preview-enabled").writeText("$armedAt:$index:${now()}")}
        status="正在切换至方案 ${index+1}"
    }
    fun startChoice(simulate:Boolean=false,acceptSharedChange:Boolean=false):Boolean{
        if(!choiceReady || (sharedNeedsConfirmation && !acceptSharedChange))return false
        requestSimulation=simulate
        expectedRouteId=previewId;choiceConfirmedAt=now();nativeRunning=false;guideAt=0;targetMatches=false
        context?.let{File(it.filesDir,"single-engine-preview-enabled").delete()}
        startup.confirm(now());status="正在启动方案 ${selectedChoice+1}"
        return true
    }
    private fun acceptChoices(raw:String){
        if(!choosing||!startup.calculatedRoute()||choiceRequestedAt==0L)return
        runCatching{NativeRouteChoice.parse(raw,targetLat,targetLng)}.onSuccess{list->
            if(list.isNotEmpty()){
                choices=list;status="请选择路线，再开始导航"
            }
        }
    }
    private fun readPreview(){
        if(!choosing||!startup.calculatedRoute())return
        val c=context?:return
        runCatching{
            val file=File(c.filesDir,"single-engine-preview.json");if(file.length() !in 1..4_000_000)return
            if(previewRoute.isNotEmpty()&&previewReadStamp==file.lastModified())return
            val o=JSONObject(file.readText());if(o.getLong("at")<armedAt)return
            if(o.optInt("selected",-1)!=selectedChoice)return
            val a=o.getJSONArray("points");if(a.length() !in 2..40000)return
            val points=List(a.length()){k->val p=a.getJSONArray(k);doubleArrayOf(p.getDouble(0),p.getDouble(1))}
            if(!points.all{CarSignalAlignment.coordinate(it[0],it[1])})return
            val end=points.last();if(CarSignalAlignment.metres(end[0],end[1],targetLat,targetLng)>100)return
            val meta=choices.getOrNull(selectedChoice)?:return
            if(kotlin.math.abs(o.getInt("metres")-meta.metres)>5)return
            previewRoute=points;previewId=o.getString("id");previewSelected=selectedChoice;previewReadStamp=file.lastModified()
            compareSharedPreview()
            status="已选择方案 ${selectedChoice+1}，可开始导航"
        }
    }
    fun displayGeometry()=if(choosing)previewRoute else geometry()
    fun displayGeometryKey()=if(choosing&&previewRoute.isNotEmpty())"preview:$armedAt:$previewId" else geometryKey()
    fun saveDestination(a:android.app.Activity){
        val la=if(CarSignalAlignment.coordinate(targetLat,targetLng))targetLat else currentEndLat
        val lo=if(CarSignalAlignment.coordinate(targetLat,targetLng))targetLng else currentEndLng
        if(!CarSignalAlignment.coordinate(la,lo)){
            android.widget.Toast.makeText(a,"请先选择目的地并开始导航",android.widget.Toast.LENGTH_SHORT).show();return
        }
        FavoritePlacesUi.save(a,targetName,targetAddress,la,lo)
    }
    fun standard(i:Intent){
        if(!owns)return
        if(i.getIntExtra("KEY_TYPE",-1)==10056){
            val raw=i.getStringExtra("EXTRA_ROAD_INFO")?:return
            if(raw.length<=4_000_000){
                context?.let{File(it.filesDir,"single-engine-standard-route.json").writeText(raw)}
                routeInfoReplies++
                acceptChoices(raw)
                Log.i("MotoNavSingleEngine","standard-route-info bytes=${raw.length}")
            }
            return
        }
        if(pendingPlan==null||i.getIntExtra("KEY_TYPE",-1)!=10019)return
        when(i.getIntExtra("EXTRA_STATE",-1)){6->startup.calculated();7->startup.planFailed()}
    }
    private fun command(i:Intent){context?.let{c->c.sendBroadcast(i.setAction("AUTONAVI_STANDARD_BROADCAST_RECV").setPackage(c.packageName))}}
    fun approachKey()=if(owns&&nativeRunning&&signalIndex>=0)"native:$routeId:light:$signalIndex" else ""
    fun signalDistance()=if(owns&&nativeRunning&&now()-signalFixAt in 0..5000)signalMetres else -1
    private fun receive(i:Intent) {
        if(!owns)return
        val t=i.getLongExtra("at",0)
        if(t<armedAt || now()-t !in 0..2500)return
        when(i.getStringExtra("kind")){
            "nativeStatus"->{nativeStatus(i.getIntExtra("code",-1),t)}
            "nativeRouteStatus"->nativeRouteStatus=i.getStringExtra("state").orEmpty()
            "routeInfo"->{
                routeInfoReplies++
                if(choosing)runCatching{
                    val f=File(context!!.filesDir,"single-engine-route-info.json")
                    if(f.length() in 1..4_000_000){val o=JSONObject(f.readText());if(o.optLong("at")>=choiceRequestedAt)acceptChoices(o.getString("json"))}
                    readPreview()
                }
            }
            "previewRoute"->readPreview()
            "engine"->{
                if(i.getStringExtra("providerProcess")!=context?.packageName)return
                engineAt=t;simulation=i.getBooleanExtra("simulation",false)
                val active=lifecycle.open()&&(if(requestSimulation)simulation else i.getBooleanExtra("real",false)||simulation)
                if(active&&!nativeRunning)lastMute=null
                if(nativeRunning&&!active){
                    BleLink.send("C");BleLink.clearTrafficLight();BleLink.sendTrafficApproach(0);MapFeeder.clearSingleEngineRoute()
                    shapeVerified=false;routeSent=false;pendingShape=null;signalGeometry=null;signalIndex=-1;signalMetres=-1
                }
                if(nativeRunning&&!active)RideRecorder.stop()
                nativeRunning=active
                if(!active&&lifecycle.open()&&!choosing)status="等待内置导航开始；MotoNav 自身导航未运行"
            }
            "guide"->{
                if(!lifecycle.open()||choosing)return
                guideAt=t;guideCount++;turnBase=i.getIntExtra("turnMetres",0).coerceAtLeast(0);turnMetres=turnBase
                direction=NaviBridge.iconToDir(i.getIntExtra("icon",0)).let{if(it=="A")"S" else it}
                road=i.getStringExtra("road").orEmpty();nextRoad=i.getStringExtra("nextRoad").orEmpty()
                if(now()-gpsSpeedAt>2500)speed=i.getDoubleExtra("speed",0.0).roundToInt().coerceIn(0,250)
                remaining=i.getIntExtra("remaining",0).coerceAtLeast(0)
                remainingSeconds=i.getIntExtra("remainingSeconds",0).coerceAtLeast(0);segment=i.getIntExtra("segment",-1)
                targetMatches=CarSignalAlignment.metres(targetLat,targetLng,i.getDoubleExtra("endLat",Double.NaN),i.getDoubleExtra("endLng",Double.NaN))<=100
                currentEndLat=i.getDoubleExtra("endLat",Double.NaN);currentEndLng=i.getDoubleExtra("endLng",Double.NaN)
                verifyShape()
                if(simulation)simulationDisplay?.guide(segment,turnBase,remaining,t)
                val ca=i.getIntExtra("cameraDistance",-1);val limit=i.getIntExtra("cameraSpeed",0)
                cameraDistance=ca.coerceAtLeast(0);cameraLimit=limit.coerceAtLeast(0);lights=i.getIntExtra("lights",0).coerceAtLeast(0)
                BleLink.send(if(ca>=0&&limit>0)"#k,$limit,$ca" else "#k")
                val la=i.getDoubleExtra("lat",Double.NaN);val lo=i.getDoubleExtra("lng",Double.NaN)
                if(CarSignalAlignment.coordinate(la,lo))pose(la,lo,i.getDoubleExtra("bearing",0.0).toFloat())
                if(!rerouting)status="单引擎${if(simulation)"模拟" else "导航"} · 已接收 $guideCount 次引导"
                Log.i("MotoNavSingleEngine",debug().toString())
            }
            "position"->{
                // Native protocol position provenance is recorded for coordinate verification.
                // Real navigation uses the explicit WGS84→GCJ02 GPS path below.
                positionCount++
                if(simulation) {
                    val la=i.getDoubleExtra("lat",Double.NaN);val lo=i.getDoubleExtra("lng",Double.NaN)
                    if(CarSignalAlignment.coordinate(la,lo))pose(la,lo,i.getDoubleExtra("bearing",0.0).toFloat())
                }
            }
            "lane"->{
                val b=i.getIntArrayExtra("back")?:IntArray(0);val f=i.getIntArrayExtra("front")?:IntArray(0)
                val n=b.indexOfFirst{it==255}.let{if(it<0)b.size else it}.coerceIn(0,8)
                lanes=IntArray(n*2){k->if(k%2==0)b[k/2] else f.getOrElse(k/2){255}}
                val text=(0 until n).joinToString(""){k->
                    val bg=b[k];val front=f.getOrElse(k){255}
                    (if(bg in 0..14)bg.toString(16) else "f")+(if(front in 0..14)front.toString(16) else "f")
                }
                val msg=if(text.isEmpty())"#j" else "#j,$text"
                if(msg!=lastLane){BleLink.send(msg);lastLane=msg};laneCount++
            }
            "route"->{
                if(!lifecycle.open()||rerouting)return
                val c=context?:return
                val f=File(c.filesDir,"single-engine-route.json")
                if(f.length() !in 1..4_000_000)return
                runCatching{
                    val o=JSONObject(f.readText());val a=o.getJSONArray("points")
                    if(o.getLong("at")<maxOf(armedAt,minimumShapeAt)||a.length() !in 2..40000)return
                    routePoints=a.length();pendingShape=o;shapeVerified=false;routeSent=false
                    verifyShape()
                }
            }
        }
    }
    private fun verifyShape(){
        val o=pendingShape?:return
        if(!nativeRunning||rerouting||!lifecycle.open()||pendingPlan!=null||!CarSignalAlignment.coordinate(currentEndLat,currentEndLng))return
        runCatching{
            val a=o.getJSONArray("points")
            val points=List(a.length()){k->val p=a.getJSONArray(k);doubleArrayOf(p.getDouble(0),p.getDouble(1))}
            require(points.size in 2..40000&&points.all{CarSignalAlignment.coordinate(it[0],it[1])})
            val end=points.last();val offset=CarSignalAlignment.metres(end[0],end[1],currentEndLat,currentEndLng)
            require(offset<=250){"native destination mismatch: $offset"}
            var length=0.0
            for(k in 1 until points.size)length+=CarSignalAlignment.metres(points[k-1][0],points[k-1][1],points[k][0],points[k][1])
            val reported=o.optInt("metres",0)
            require(reported>0&&kotlin.math.abs(length-reported)<=maxOf(300.0,reported*.12)){"native length mismatch: $length/$reported"}
            val nextId=o.getString("id")
            if(expectedRouteId.isNotEmpty()&&nextId!=expectedRouteId&&routeId.isEmpty()){
                lifecycle.cancel();nativeRunning=false;pendingPlan=null;startup.stop()
                command(Intent().putExtra("KEY_TYPE",10010));TrafficPositionMonitor.stop();RideRecorder.stop()
                nativeRouteStatus="启动路线与所选方案不同，请重新选择";status=nativeRouteStatus;return
            }
            if(routeId.isNotEmpty()&&nextId!=routeId)routeChanges++
            route=points;routeId=nextId;shapeVerified=true;pendingShape=null
            if(simulation){
                fun ints(name:String)=o.optJSONArray(name)?.let{v->IntArray(v.length()){v.optInt(it)}}?:IntArray(0)
                simulationDisplay=NativeSimulationDisplay(points.toTypedArray(),reported,ints("segmentStarts"),ints("segmentMetres"))
                simulationDisplay?.guide(segment,turnBase,remaining,guideAt)
            }
            BleLink.send("#y,0")
            routeLights=o.optJSONArray("lights")?.let{ls->List(ls.length()){k->val p=ls.getJSONArray(k);doubleArrayOf(p.getDouble(0),p.getDouble(1))}.filter{CarSignalAlignment.coordinate(it[0],it[1])}}?:emptyList()
            signalGeometry=NativeSignalGeometry(points.toTypedArray(),routeLights.toTypedArray())
            signalIndex=-1;signalMetres=-1;signalFixAt=0
            nativeRouteStatus="已核验当前导航路线：${points.size} 点"
            Log.i("MotoNavSingleEngine","native-shape-verified id=$routeId points=${points.size} length=${length.roundToInt()} metres=$reported endOffset=${offset.roundToInt()}")
        }.onFailure{nativeRouteStatus="路线核验等待：${it.message}"}
    }
    fun geometry()=if(shapeVerified)route else emptyList()
    fun geometryKey()=if(shapeVerified)"$armedAt:$routeId" else ""
    fun instruction()="${when(direction){"L"->"左转";"R"->"右转";"U"->"掉头";"SL"->"靠左";"SR"->"靠右";else->"直行"}}${if(nextRoad.isNotBlank())"进入$nextRoad" else road}"
    fun rawFix(la:Double,lo:Double,heading:Float,kmh:Int,accuracy:Double,stamp:Long,mock:Boolean){
        if(owns&&!simulation&&nativeRunning&&lifecycle.open()){
            // A missing GPS speed is not a measured stop. Keep the engine's
            // current speed until a valid GPS speed (including zero) replaces it.
            if(kmh>=0){speed=kmh.coerceIn(0,250);gpsSpeedAt=now()}
            pose(la,lo,heading)
            if(lifecycle.running()&&accuracy<=40){
                if(CarSignalAlignment.coordinate(tripLat,tripLng)){
                    val d=CarSignalAlignment.metres(tripLat,tripLng,la,lo);if(d<100)tripMetres+=d
                }
                tripLat=la;tripLng=lo;tripMax=maxOf(tripMax,speed)
            }
            signalGeometry?.locate(la,lo,accuracy)?.let{signalIndex=it.index;signalMetres=it.metres;signalFixAt=now()}
            if(!mock)RideRecorder.onFix(la,lo,stamp,System.currentTimeMillis(),accuracy,speed.toDouble())
        }
    }
    private fun resetTrip(){
        lifecycle.reset();minimumShapeAt=0;tripStart=0;tripFinishedAt=0;tripMetres=0.0;tripMax=0
        tripLat=Double.NaN;tripLng=Double.NaN;routeChanges=0;arrivalCount=0
        gpsSpeedAt=0;speed=0;remaining=0;remainingSeconds=0;turnMetres=-1;turnBase=0
        lanes=IntArray(0);lastLane="";lastRoad="";lastFooter="";lastVoiceKey=""
        currentEndLat=Double.NaN;currentEndLng=Double.NaN;routeId=""
        BleLink.send("#j");BleLink.send("#k");BleLink.send("#c");BleLink.send("#y,0")
    }
    private fun nativeStatus(code:Int,stamp:Long){
        // Ignore terminal events from a route being replaced during startup.
        if(pendingPlan!=null||!lifecycle.running())return
        val completed=lifecycle.event(code)
        if(completed){
            arrivalCount++;tripFinishedAt=now();nativeRunning=false;speed=0;remaining=0;remainingSeconds=0
            lanes=IntArray(0);signalGeometry=null;signalIndex=-1;signalMetres=-1
            RideRecorder.stop();TrafficPositionMonitor.stop();pendingPlan=null;startup.stop()
            BleLink.clearTrafficLight();BleLink.sendTrafficApproach(0)
            BleLink.send("#j");BleLink.send("#k");BleLink.send("#y,0");BleLink.send("#x,0")
            val minutes=if(tripStart>0)((now()-tripStart)/60000).toInt() else 0
            BleLink.sendEvent("#O${tripMetres.roundToInt()}/$minutes/$tripMax")
            BleLink.send("A,0");status="已到达目的地"
            Log.i("MotoNavSingleEngine","arrival-confirmed epoch=$armedAt event=$code wire=A,0")
        }else if(code==307||code==305){
            minimumShapeAt=stamp;pendingShape=null;shapeVerified=false;routeSent=false
            signalGeometry=null;signalIndex=-1;signalMetres=-1;lanes=IntArray(0);lastLane=""
            MapFeeder.clearSingleEngineRoute();BleLink.send("#j");BleLink.send("#c");BleLink.send("#k")
            BleLink.clearTrafficLight();BleLink.sendTrafficApproach(0)
            BleLink.send(if(rerouting)"#y,1" else "#y,0")
            status=if(rerouting)"偏离路线，正在重新规划" else "重算完成，正在更新路线"
        }else if(code==306){
            BleLink.send("#y,0");status="重算暂未成功，等待导航引擎恢复"
        }else if(code==308||code==9){
            nativeRunning=false;RideRecorder.stop();TrafficPositionMonitor.stop()
            BleLink.send("C");BleLink.send("#y,0");BleLink.send("#j");BleLink.clearTrafficLight();BleLink.sendTrafficApproach(0)
            MapFeeder.clearSingleEngineRoute();shapeVerified=false;routeSent=false;signalGeometry=null
            status="导航已结束"
        }
    }
    fun tripSummary():String {
        val minutes=if(tripStart>0)((tripFinishedAt.takeIf{it>0}?:now())-tripStart)/60000 else 0
        return "本次 %.1f 公里 · %d 分钟 · 最高 %d km/h".format(tripMetres/1000.0,minutes,tripMax)
    }
    private fun pose(la:Double,lo:Double,heading:Float){
        if(!CarSignalAlignment.coordinate(la,lo))return
        lat=la;lng=lo;if(heading.isFinite()&&(speed>=3||fixAt==0L))bearing=heading;fixAt=now()
        Brightness.updateLocation(la,lo);MapFeeder.onLocation(la,lo,bearing,speed)
        if(clearRouteAtPose){clearRouteAtPose=false;MapFeeder.clearSingleEngineRoute()}
    }
    private fun text(slot:Int,value:String,width:Int,size:Float){
        TextImage.render(value,width,size,15f)?.let{BleLink.sendImage(slot,it.w,it.h,it.data)}
    }
    private val tick=object:Runnable{override fun run(){
        if(!owns)return
        readSimulationSpeedAck()
        if(choosing&&startup.calculatedRoute()){
            if(choices.isEmpty()&&choiceReads<8&&now()-choiceRequestedAt>=2000){
                choiceRequestedAt=now();choiceReads++
                command(Intent().putExtra("KEY_TYPE",10056).putExtra("EXTRA_TYPE",0))
            }
            readPreview()
            if(choices.isEmpty()&&choiceReads>=8&&now()-choiceRequestedAt>=3000){
                startup.stop();pendingPlan=null;context?.let{File(it.filesDir,"single-engine-preview-enabled").delete()}
                status="未取得备选路线，请重新搜索目的地"
            }
        }
        if(nativeRunning&&pendingPlan==null&&guideCount>0&&routeInfoRequests<3&&now()-routeInfoAt>=10000){
            routeInfoAt=now()
            command(Intent().putExtra("KEY_TYPE",10056).putExtra("EXTRA_TYPE",routeInfoRequests))
            routeInfoRequests++;askedRouteInfo=true
            Log.i("MotoNavSingleEngine","request-route-info type=${routeInfoRequests-1}")
        }
        val mute=if(NaviVoice.muted||NaviVoice.mode==NaviVoice.MODE_SYSTEM)1 else 0
        if(mute!=lastMute){command(Intent().putExtra("KEY_TYPE",10047).putExtra("EXTRA_MUTE",mute));lastMute=mute}
        pendingPlan?.let{plan->context?.let{c->
            when(startup.next(now(),FusionCarUi.prepared(c,warmToken),nativeRunning&&guideAt>armedAt&&targetMatches&&(!requestSimulation||simulation))){
                ProviderStartup.WARM->{warmToken=now();FusionCarUi.warm(c,warmToken)}
                ProviderStartup.PLAN->{
                    choices=emptyList();previewRoute=emptyList();previewId="";previewSelected=-1;choiceReads=0;choiceRequestedAt=0
                    command(Intent().putExtra("KEY_TYPE",10010));command(plan);status="正在由单引擎规划路线"
                }
                ProviderStartup.START->{
                    if(requestSimulation){
                        // Provider-process UI thread consumes this private, owner-scoped
                        // request. No mock system GPS and no SDK navigation instance.
                        val data=JSONObject().put("owner",armedAt.toString()).put("sequence",choiceConfirmedAt.toString())
                            .put("at",now()).put("pathId",expectedRouteId.toLong()).put("speed",simulationSpeedKmh)
                            .put("request","start:$armedAt:$choiceConfirmedAt")
                        speedRequest=data.getString("request");speedRequestedAt=now();simulationSpeedPending=true
                        val tmp=File(c.filesDir,"single-engine-simulation-command.tmp")
                        tmp.writeText(data.toString());check(tmp.renameTo(File(c.filesDir,"single-engine-simulation-command.json")))
                    }else command(if(selectOnStart)Intent().putExtra("KEY_TYPE",10055).putExtra("EXTRA_CHANGE_ROAD",selectedChoice+1).putExtra("EXTRA_START_NAVI",true) else Intent().putExtra("KEY_TYPE",10009))
                    status=if(requestSimulation)"正在启动所选路线的模拟导航" else "正在启动单引擎导航"
                }
                ProviderStartup.FAILED->{
                    lifecycle.cancel();nativeRunning=false;pendingPlan=null
                    command(Intent().putExtra("KEY_TYPE",10010));TrafficPositionMonitor.stop();RideRecorder.stop()
                    File(c.filesDir,"single-engine-preview-enabled").delete()
                    File(c.filesDir,"single-engine-simulation-command.json").delete()
                    MapFeeder.clearSingleEngineRoute();shapeVerified=false;routeSent=false
                    BleLink.send("C");BleLink.clearTrafficLight();BleLink.sendTrafficApproach(0)
                    status="导航引擎启动未确认，请重新选择目的地"
                }
            }
            if(startup.running())pendingPlan=null
        }}
        if(pendingPlan==null&&nativeRunning&&guideAt>=armedAt&&lifecycle.begin(simulation)){
            tripStart=now();RideRecorder.start("导航","单引擎导航",simulation)
        }
        if(lifecycle.running()&&now()-engineAt>3500){
            nativeRunning=false;status="导航引擎状态中断，等待恢复";BleLink.send("#x,1")
        }
        if(nativeRunning&&simulation&&shapeVerified&&!rerouting){
            simulationDisplay?.sample(now())?.let{p->speed=p.speedKmh;pose(p.lat,p.lng,p.bearing)}
        }
        if(nativeRunning&&shapeVerified&&!routeSent&&fixAt>0)
            routeSent=MapFeeder.onRoute("native:$armedAt:$routeId",route,routeLights)
        if(nativeRunning&&lifecycle.running()&&!rerouting&&guideAt>=armedAt&&now()-guideAt<15000){
            val age=((now()-guideAt)/1000.0).coerceIn(0.0,1.2)
            turnMetres=(turnBase-age*speed/3.6).roundToInt().coerceAtLeast(0)
            BleLink.send("#x,0");BleLink.send("#v,$speed");BleLink.send("$direction,$turnMetres")
            val instruction="${when(direction){"L"->"左转";"R"->"右转";"U"->"掉头";"SL"->"靠左";"SR"->"靠右";else->"直行"}}${if(nextRoad.isNotBlank())"进入$nextRoad" else road}"
            if(instruction!=lastRoad){text(0,instruction,290,26f);lastRoad=instruction}
            val cal=Calendar.getInstance().apply{add(Calendar.SECOND,remainingSeconds)}
            val hhmm=cal.get(Calendar.HOUR_OF_DAY)*100+cal.get(Calendar.MINUTE)
            BleLink.send("#e,$remaining,$hhmm,$lights")
            val footer="${"%.1f".format(remaining/1000.0)}km · ${"%02d:%02d".format(hhmm/100,hhmm%100)}到达"
            if(footer!=lastFooter){text(1,footer,240,19f);lastFooter=footer}
            if(NaviVoice.mode==NaviVoice.MODE_SYSTEM&&!NaviVoice.muted){
                val bucket=when{turnMetres<=30->0;turnMetres<=100->1;turnMetres<=300->2;turnMetres<=800->3;else->4}
                val key="$routeId:$segment:$road:$nextRoad:$bucket"
                if(key!=lastVoiceKey){lastVoiceKey=key;NaviVoice.onNaviText("前方${turnMetres}米，${instruction()}")}
            }
        }else if(nativeRunning&&lifecycle.running()&&!rerouting)BleLink.send("#x,1")
        handler.postDelayed(this,250)
    }}
    fun debug()=JSONObject().put("owns",owns).put("nativeRunning",nativeRunning).put("simulation",simulation)
        .put("sdkEngineAllocated",NaviBridge.engineAllocatedForDiagnostics()).put("sdkNavigation",NaviBridge.engineRunningForDiagnostics())
        .put("guides",guideCount).put("positionCallbacks",positionCount)
        .put("laneCallbacks",laneCount).put("routePoints",routePoints).put("shapeVerified",shapeVerified)
        .put("nativeRouteStatus",nativeRouteStatus).put("routeSent",routeSent).put("nativeMapLights",routeLights.size)
        .put("signalDistance",signalDistance()).put("signalIndex",signalIndex).put("segment",segment)
        .put("routeInfoRequests",routeInfoRequests).put("routeInfoReplies",routeInfoReplies)
        .put("direction",direction).put("turnMetres",turnMetres).put("remaining",remaining)
        .put("speedKmh",speed).put("bearing",bearing)
        .put("arrived",arrived).put("arrivalCount",arrivalCount).put("rerouting",rerouting).put("routeChanges",routeChanges)
        .put("routeId",routeId).put("status",status).put("tripMetres",tripMetres.roundToInt())
        .put("choosing",choosing).put("choices",org.json.JSONArray(choices.map{JSONObject().put("index",it.index).put("metres",it.metres).put("seconds",it.seconds).put("roads",it.roads)}))
        .put("selectedChoice",selectedChoice).put("previewId",previewId).put("previewSelected",previewSelected).put("choiceReady",choiceReady).put("expectedRouteId",expectedRouteId)
        .put("simulationSpeedSelected",simulationSpeedKmh).put("simulationSpeedApplied",appliedSimulationSpeed)
        .put("simulationSpeedPending",simulationSpeedPending).put("simulationSpeedStatus",simulationSpeedStatus)
        .put("manualWaypoints",org.json.JSONArray(waypoints.map{JSONObject().put("name",it.name).put("lat",it.lat).put("lng",it.lng)}))
        .put("requestedSimulation",requestSimulation).put("sharedKind",sharedRoute?.kind).put("sharedNote",sharedRouteNote).put("sharedNeedsConfirmation",sharedNeedsConfirmation)
        .put("positionAge",if(fixAt==0L)-1 else now()-fixAt).put("road",road).put("nextRoad",nextRoad)
}
