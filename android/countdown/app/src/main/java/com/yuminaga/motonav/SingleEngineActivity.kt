package com.yuminaga.motonav

import android.content.Intent
import android.graphics.*
import android.os.*
import android.view.*
import android.widget.*
import androidx.activity.OnBackPressedCallback
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import com.amap.api.maps.*
import com.amap.api.maps.model.*
import java.util.Calendar

/** MotoNav cards and map rendering, without NaviView or an SDK navigation instance. */
class SingleEngineActivity:AppCompatActivity(){
    private val s=NavStyle
    private val handler=Handler(Looper.getMainLooper())
    private lateinit var map:MapView
    private lateinit var arrow:TurnArrowView
    private lateinit var distance:TextView
    private lateinit var road:TextView
    private lateinit var state:TextView
    private lateinit var summary:TextView
    private lateinit var lanes:LaneView
    private lateinit var followButton:TextView
    private lateinit var muteButton:TextView
    private lateinit var choiceBox:LinearLayout
    private lateinit var choiceScroll:ScrollView
    private lateinit var startChoice:TextView
    private lateinit var simulateChoice:TextView
    private lateinit var navigationButtons:LinearLayout
    private lateinit var headRow:LinearLayout
    private lateinit var planTitle:TextView
    private lateinit var viaSummary:TextView
    private lateinit var choiceActions:LinearLayout
    private lateinit var speedRow:LinearLayout
    private lateinit var speedLabel:TextView
    private val speedButtons=linkedMapOf<Int,TextView>()
    private var speedUiKey=""
    private var choiceUiRoutes:List<NativeRouteChoice>?=null
    private var choiceUiSelected=-1
    private var choiceUiChoosing=false
    private var shareGhost:Polyline?=null
    private var ghostKey=""
    private var ghostSource:AmapShare.Route?=null
    private var cachedGhostKey=""
    private lateinit var shareNote:TextView
    private var polyline:Polyline?=null
    private var startMarker:Marker?=null
    private var endMarker:Marker?=null
    private val viaMarkers=ArrayList<Marker>()
    private var position:Marker?=null
    private var routeKey=""
    private var follow=true
    private val poseUpdates=MapPoseUpdates()
    private var resumed=false
    private var night:Boolean?=null
    private var pendingDestination:SingleEngineDestination.Request?=null
    private var destinationDialog:AlertDialog?=null
    private var activityGeneration=0
    companion object {
        private const val STATE_PENDING="single_pending_destination"
        private var current=java.lang.ref.WeakReference<SingleEngineActivity>(null)
        private var generations=0
        private var acceptedRequests=0
        private var ignoredReplays=0
        fun destinationDiagnostics()=org.json.JSONObject()
            .put("generation",current.get()?.activityGeneration ?: -1).put("accepted",acceptedRequests)
            .put("ignoredReplays",ignoredReplays).put("pending",current.get()?.pendingDestination!=null)
            .put("dialogShowing",current.get()?.destinationDialog?.isShowing==true)
        fun recreateForCheck():Boolean {
            val activity=current.get() ?: return false
            if(!activity.resumed)return false
            activity.recreate();return true
        }
    }
    private fun dp(n:Int)=s.dpi(this,n)
    private fun text(size:Float,color:Int=s.TEXT)=TextView(this).apply{textSize=size;setTextColor(color);typeface=s.MEDIUM;includeFontPadding=false}
    private fun control(title:String,action:()->Unit)=text(13f,s.CYAN).apply{
        this.text=title;gravity=Gravity.CENTER;setPadding(dp(6),0,dp(6),0)
        background=s.round(this@SingleEngineActivity,s.CARD2,12f);setOnClickListener{action()}
    }
    private fun card()=LinearLayout(this).apply{
        orientation=LinearLayout.VERTICAL;background=s.round(this@SingleEngineActivity,s.CARD,20f,s.STROKE)
        setPadding(dp(12),dp(10),dp(12),dp(10))
    }
    override fun onCreate(savedInstanceState:Bundle?){
        SingleEngineDestination.restore(savedInstanceState)
        current=java.lang.ref.WeakReference(this);activityGeneration=++generations
        super.onCreate(savedInstanceState);supportActionBar?.hide();window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        val root=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL;setBackgroundColor(s.BG);setPadding(dp(8),dp(8),dp(8),dp(8))}
        val head=card()
        planTitle=text(22f).apply{text="驾车路线规划";maxLines=2};head.addView(planTitle)
        val row=LinearLayout(this).apply{gravity=Gravity.CENTER_VERTICAL};headRow=row
        arrow=TurnArrowView(this).apply{background=s.gradient(this@SingleEngineActivity,16f)}
        row.addView(arrow,LinearLayout.LayoutParams(dp(68),dp(68)))
        val col=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL;setPadding(dp(14),0,0,0)}
        distance=text(34f).apply{typeface=s.NUM};road=text(17f).apply{maxLines=2}
        col.addView(distance);col.addView(road);row.addView(col,LinearLayout.LayoutParams(0,-2,1f));head.addView(row)
        lanes=LaneView(this);head.addView(lanes,LinearLayout.LayoutParams(-1,dp(32)).apply{topMargin=dp(6)})
        state=text(11f,s.SUB).apply{
            setPadding(0,dp(7),0,0)
            setOnClickListener{AlertDialog.Builder(this@SingleEngineActivity).setTitle("导航状态").setMessage(SingleEngineSession.status+"\n"+MapFeeder.loadingStatus()).setPositiveButton("知道了",null).show()}
            // 原始调试数据只在开发者选项打开时长按可看
            setOnLongClickListener{
                if(!getSharedPreferences("ui",MODE_PRIVATE).getBoolean("dev_mode",false))return@setOnLongClickListener false
                AlertDialog.Builder(this@SingleEngineActivity).setTitle("单引擎验证").setMessage(SingleEngineSession.debug().toString(2)).setPositiveButton("关闭",null).show();true
            }
        }
        head.addView(state)
        viaSummary=control("添加途经点（最多3个）"){
            RouteWaypointEditor.show(this,SingleEngineSession.waypoints,SingleEngineSession.waypointSearchCenter){points->
                if(!SingleEngineSession.setWaypoints(this,points))Toast.makeText(this,"请在路线规划时修改途经点",Toast.LENGTH_SHORT).show()
            }
        }.apply{contentDescription="编辑驾车途经点";visibility=View.GONE}
        head.addView(viaSummary,LinearLayout.LayoutParams(-1,dp(36)).apply{topMargin=dp(8)})
        root.addView(head)
        val mapBox=FrameLayout(this)
        map=MapView(this);map.onCreate(savedInstanceState)
        map.map.uiSettings.apply{isZoomControlsEnabled=false;isCompassEnabled=false;isScaleControlsEnabled=true}
        map.map.setOnMapTouchListener{if(it.actionMasked==MotionEvent.ACTION_MOVE){follow=false;followButton.text="跟随"}}
        mapBox.addView(map,FrameLayout.LayoutParams(-1,-1))
        mapBox.addView(TrafficLightBadge(this),FrameLayout.LayoutParams(-2,-2,Gravity.TOP or Gravity.START).apply{setMargins(dp(8),dp(8),dp(8),0)})
        root.addView(mapBox,LinearLayout.LayoutParams(-1,0,1f).apply{topMargin=dp(8);bottomMargin=dp(8)})
        val foot=card();shareNote=text(12f,s.AMBER).apply{maxLines=3;setPadding(0,0,0,dp(8));visibility=View.GONE};foot.addView(shareNote);summary=text(17f).apply{setPadding(0,0,0,dp(10))};foot.addView(summary)
        choiceBox=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL}
        choiceScroll=ScrollView(this).apply{isFillViewport=false;addView(choiceBox)}
        val heightDp=resources.displayMetrics.heightPixels/resources.displayMetrics.density
        foot.addView(choiceScroll,LinearLayout.LayoutParams(-1,dp(when{heightDp>=800->326;heightDp>=700->214;else->132})))
        speedRow=LinearLayout(this).apply{gravity=Gravity.CENTER_VERTICAL}
        speedLabel=text(11f,s.SUB).apply{text="模拟速度";maxLines=2}
        speedRow.addView(speedLabel,LinearLayout.LayoutParams(dp(65),-2))
        listOf(60,100,120).forEach{kmh->
            val button=control("$kmh km/h"){
                if(!SingleEngineSession.setSimulationSpeed(kmh))Toast.makeText(this,"请等待速度调整完成",Toast.LENGTH_SHORT).show()
            }
            speedButtons[kmh]=button
            speedRow.addView(button,LinearLayout.LayoutParams(0,dp(36),1f).apply{leftMargin=dp(5)})
        }
        foot.addView(speedRow,LinearLayout.LayoutParams(-1,-2).apply{bottomMargin=dp(6)})
        startChoice=control("开始导航"){
            beginChoice(false)
        }.apply{background=s.gradient(this@SingleEngineActivity,12f);setTextColor(Color.WHITE);typeface=s.BOLD}
        simulateChoice=control("模拟导航"){
            beginChoice(true)
        }
        choiceActions=LinearLayout(this).apply{
            addView(control("取消"){SingleEngineSession.disarm();finish()},LinearLayout.LayoutParams(dp(52),dp(46)).apply{rightMargin=dp(7)})
            addView(simulateChoice,LinearLayout.LayoutParams(0,dp(46),1f).apply{rightMargin=dp(7)})
            addView(startChoice,LinearLayout.LayoutParams(0,dp(46),1f))
        }
        foot.addView(choiceActions,LinearLayout.LayoutParams(-1,-2).apply{topMargin=dp(8);bottomMargin=dp(4)})
        val buttons=LinearLayout(this);navigationButtons=buttons
        followButton=control("全览"){follow=!follow;poseUpdates.invalidateCamera();if(!follow)overview()}
        muteButton=control("播报"){NaviVoice.muted=!NaviVoice.muted;getSharedPreferences("motonav",0).edit().putBoolean("nav_muted",NaviVoice.muted).apply()}
        val settings=control("设置"){NavigationUi.openSettings(this)}
        val exit=control("退出"){AlertDialog.Builder(this).setMessage("结束这次导航？").setPositiveButton("结束"){_,_->SingleEngineSession.disarm();finish()}.setNegativeButton("继续导航",null).show()}
        listOf(followButton,muteButton,settings,exit).forEach{buttons.addView(it,LinearLayout.LayoutParams(0,dp(42),1f).apply{setMargins(dp(3),0,dp(3),0)})};foot.addView(buttons)
        val more=LinearLayout(this)
        more.addView(control("收藏到常用地点"){SingleEngineSession.saveDestination(this)},LinearLayout.LayoutParams(0,dp(36),1f))
        more.addView(control("搜索目的地"){startActivity(Intent(this,MainActivity::class.java))},LinearLayout.LayoutParams(0,dp(36),1f))
        more.addView(control("查看数据源"){FusionCarUi.openConsole(this,"com.yuminaga.motonav.SingleEngineActivity")},LinearLayout.LayoutParams(0,dp(36),1f))
        foot.addView(more,LinearLayout.LayoutParams(-1,-2).apply{topMargin=dp(6)});root.addView(foot)
        setContentView(root)
        onBackPressedDispatcher.addCallback(this,object:OnBackPressedCallback(true){override fun handleOnBackPressed(){
            if(SingleEngineSession.choosing){SingleEngineSession.disarm();finish()}
            else NavigationUi.openSettings(this@SingleEngineActivity,0)
        }})
        SingleEngineSession.arm(this)
        val pending=SingleEngineDestination.decode(savedInstanceState?.getBundle(STATE_PENDING))
        if(pending!=null){SingleEngineDestination.clear(intent);handleDestination(pending)} else consumeDestination(intent)
    }
    override fun onNewIntent(intent:Intent){
        super.onNewIntent(intent);setIntent(intent)
        SingleEngineSession.arm(this)
        consumeDestination(intent)
    }
    private fun consumeDestination(i:Intent){
        val request=SingleEngineDestination.decode(i.extras)
        // Android retains its own launch Intent; clearing these extras alone is insufficient.
        SingleEngineDestination.clear(i)
        if(request==null)return
        if(!SingleEngineDestination.claim(request)){
            ignoredReplays++
            android.util.Log.i("MotoNavLifecycle","destination-replay-ignored")
            return
        }
        acceptedRequests++
        handleDestination(request)
    }
    private fun handleDestination(request:SingleEngineDestination.Request){
        destinationDialog?.dismiss();destinationDialog=null;pendingDestination=null
        fun plan(){
            follow=false
            if(request.shareToken.isEmpty())SingleEngineSession.planTo(this,request.lat,request.lng,request.name,request.address)
            else runCatching{SharedRouteStore.load(this,request.shareToken)}
                .onSuccess{SingleEngineSession.planShared(this,it)}
                .onFailure{Toast.makeText(this,it.message ?: "分享路线已失效，请重新导入",Toast.LENGTH_LONG).show()}
        }
        if(!SingleEngineSession.nativeRunning){plan();return}
        pendingDestination=request
        destinationDialog=AlertDialog.Builder(this).setTitle(request.name)
            .setMessage("结束当前导航，规划前往这里的备选路线？")
            .setPositiveButton("规划路线"){_,_->
                if(pendingDestination?.id==request.id){pendingDestination=null;destinationDialog=null;plan()}
            }.setNegativeButton("继续当前导航"){_,_->pendingDestination=null;destinationDialog=null}
            .setOnCancelListener{pendingDestination=null;destinationDialog=null}.show()
    }
    private fun beginChoice(simulate:Boolean){
        val g=SingleEngineSession
        if(!g.choiceReady){Toast.makeText(this,"请等待路线加载和线路核对完成",Toast.LENGTH_SHORT).show();return}
        val preview=g.displayGeometryKey()
        fun start(){
            if(g.displayGeometryKey()!=preview){Toast.makeText(this,"路线已变化，请重新核对",Toast.LENGTH_SHORT).show();return}
            if(g.startChoice(simulate,true)){follow=true;poseUpdates.invalidateCamera()}
        }
        if(g.sharedNeedsConfirmation)AlertDialog.Builder(this).setTitle("当前导航路线与分享线路有变化")
            .setMessage(g.sharedRouteNote+"\n实际导航将沿粉色路线行驶，请先核对地图。")
            .setPositiveButton("使用粉色路线"){_,_->start()}.setNegativeButton("继续核对",null).show()
        else start()
    }
    private fun overview(){
        val points=SingleEngineSession.displayGeometry();if(points.size<2)return
        val bounds=LatLngBounds.Builder();points.forEach{bounds.include(LatLng(it[0],it[1]))}
        map.post{runCatching{map.map.moveCamera(CameraUpdateFactory.newLatLngBounds(bounds.build(),dp(26)))}}
    }
    private fun icon():Bitmap{
        val image=Bitmap.createBitmap(dp(38),dp(38),Bitmap.Config.ARGB_8888);val canvas=Canvas(image);val n=image.width.toFloat()
        val shape=Path().apply{moveTo(n*.5f,n*.08f);lineTo(n*.86f,n*.9f);lineTo(n*.5f,n*.7f);lineTo(n*.14f,n*.9f);close()}
        canvas.drawPath(shape,Paint(Paint.ANTI_ALIAS_FLAG).apply{style=Paint.Style.STROKE;strokeWidth=n*.08f;color=s.PINK})
        canvas.drawPath(shape,Paint(Paint.ANTI_ALIAS_FLAG).apply{color=Color.WHITE});return image
    }
    private val refresh=object:Runnable{override fun run(){
        val g=SingleEngineSession;val(number,unit)=s.distParts(g.turnMetres)
        headRow.visibility=if(g.choosing)View.GONE else View.VISIBLE
        planTitle.visibility=if(g.choosing)View.VISIBLE else View.GONE
        viaSummary.visibility=if(g.choosing)View.VISIBLE else View.GONE
        if(g.choosing){
            val title=RouteWaypointEditor.summary(g.waypoints)
            if(viaSummary.text.toString()!=title)viaSummary.text=title
        }
        planTitle.text=if(g.destinationTitle=="目的地")"驾车路线规划" else "前往 ${g.destinationTitle}"
        navigationButtons.visibility=if(g.choosing)View.GONE else View.VISIBLE
        choiceScroll.visibility=if(g.choosing)View.VISIBLE else View.GONE
        choiceActions.visibility=if(g.choosing)View.VISIBLE else View.GONE
        speedRow.visibility=if(g.choosing||(g.simulation&&!g.arrived))View.VISIBLE else View.GONE
        val speedKey="${g.simulationSpeedKmh}:${g.simulationSpeedPending}:${g.simulationSpeedStatus}"
        if(speedUiKey!=speedKey){
            speedUiKey=speedKey
            speedLabel.text=if(g.simulationSpeedPending)"正在调整" else g.simulationSpeedStatus.ifBlank{"模拟速度"}
            speedButtons.forEach{(kmh,button)->
                val selected=kmh==g.simulationSpeedKmh
                button.setTextColor(if(selected)s.PINK else s.CYAN)
                button.background=s.round(this@SingleEngineActivity,s.CARD2,12f,if(selected)s.PINK else s.STROKE)
                button.isEnabled=!g.simulationSpeedPending;button.alpha=if(g.simulationSpeedPending).5f else 1f
            }
        }
        startChoice.isEnabled=g.choiceReady;startChoice.alpha=if(g.choiceReady)1f else .45f
        simulateChoice.isEnabled=g.choiceReady;simulateChoice.alpha=startChoice.alpha
        startChoice.text=if(g.choices.isEmpty())"规划中…" else if(g.choiceReady)"开始导航" else "加载路线中…"
        val currentChoices=g.choices
        if(choiceUiChoosing!=g.choosing||choiceUiSelected!=g.selectedChoice||choiceUiRoutes!=currentChoices){
            choiceUiChoosing=g.choosing;choiceUiSelected=g.selectedChoice;choiceUiRoutes=currentChoices
            choiceBox.removeAllViews()
            g.choices.forEach{q->
                val selected=q.index==g.selectedChoice
                val item=LinearLayout(this@SingleEngineActivity).apply{
                    orientation=LinearLayout.VERTICAL;setPadding(dp(12),dp(10),dp(12),dp(10))
                    background=s.round(this@SingleEngineActivity,if(selected)s.CARD2 else s.CARD,12f,if(selected)s.CYAN else s.STROKE)
                    setOnClickListener{if(q.index!=g.selectedChoice)g.selectChoice(q.index)}
                }
                item.addView(text(17f,if(selected)s.CYAN else s.TEXT).apply{text="${if(selected)"✓ " else ""}方案 ${q.index+1}　${s.durText(q.seconds)} · ${s.distText(q.metres)}"})
                item.addView(text(12f,s.SUB).apply{text=q.roads.ifBlank{"按地图所示路线行驶"};maxLines=2;setPadding(0,dp(5),0,0)})
                item.addView(text(11f,s.SUB).apply{text="${q.lights} 个红绿灯 · ${if(q.tolls==0)"无收费" else "收费 ${q.tolls} 元"}";setPadding(0,dp(5),0,0)})
                choiceBox.addView(item,LinearLayout.LayoutParams(-1,-2).apply{bottomMargin=dp(7)})
            }
        }
        distance.text=if(g.arrived)"已到达" else "$number $unit";road.text=if(g.arrived)"本次导航已完成" else if(g.rerouting)"正在重新规划路线" else g.instruction();arrow.dir=if(g.arrived)"A" else g.direction;arrow.angle=null
        lanes.lanes=g.lanes;lanes.visibility=if(g.choosing||g.lanes.isEmpty())View.GONE else View.VISIBLE
        state.text=if(g.choosing)g.status else "${if(g.arrived)"已到达目的地" else if(g.rerouting)"正在重算" else if(g.nativeRunning)if(g.simulation)"模拟导航" else "单引擎导航" else g.status} · ${g.speed} km/h · ${if(BleLink.verified)"圆屏已连接" else "圆屏未连接"}"+if(g.nativeRunning&&g.geometry().isEmpty())" · 正在读取路线" else ""
        val cal=Calendar.getInstance().apply{add(Calendar.SECOND,g.remainingSeconds)}
        summary.text=if(g.choosing)if(g.choices.isEmpty())"正在规划驾车路线" else "${g.choices.size} 条路线 · 请选择" else if(g.arrived)g.tripSummary() else "${s.distText(g.remaining)} · ${s.durText(g.remainingSeconds)} · %02d:%02d 到达".format(cal.get(Calendar.HOUR_OF_DAY),cal.get(Calendar.MINUTE))
        followButton.text=if(follow)"全览" else "跟随";muteButton.text=if(NaviVoice.muted)"静音" else "播报"
        shareNote.text=g.sharedRouteNote;shareNote.visibility=if(g.choosing && g.sharedRoute!=null)View.VISIBLE else View.GONE
        val original=g.sharedRoute
        if(original!==ghostSource){
            ghostSource=original
            cachedGhostKey=if(original!=null)original.sourceUrl+":"+original.pts.hashCode() else ""
        }
        val newGhost=if(g.choosing && original!=null && !original.endpointsOnly)cachedGhostKey else ""
        if(newGhost!=ghostKey){
            ghostKey=newGhost;shareGhost?.remove();shareGhost=null
            if(newGhost.isNotEmpty() && original!=null)shareGhost=map.map.addPolyline(PolylineOptions()
                .addAll(original.pts.map{LatLng(it[0],it[1])}).width(dp(3).toFloat()).color(s.AMBER).setDottedLine(true).zIndex(-1f))
        }
        val key=g.displayGeometryKey()
        if(night!=MapFeeder.nightNow){night=MapFeeder.nightNow;map.map.mapType=if(night==true)AMap.MAP_TYPE_NIGHT else AMap.MAP_TYPE_NORMAL}
        if(key.isEmpty()&&routeKey.isNotEmpty()){polyline?.remove();polyline=null;startMarker?.remove();startMarker=null;endMarker?.remove();endMarker=null;viaMarkers.forEach{it.remove()};viaMarkers.clear();routeKey=""}
        if(key.isNotEmpty()&&key!=routeKey){
            routeKey=key;polyline?.remove();startMarker?.remove();endMarker?.remove();startMarker=null;endMarker=null
            viaMarkers.forEach{it.remove()};viaMarkers.clear()
            val points=g.displayGeometry().map{LatLng(it[0],it[1])}
            polyline=map.map.addPolyline(PolylineOptions().addAll(points).width(dp(7).toFloat()).color(s.PINK))
            if(g.choosing){
                g.waypoints.forEachIndexed{i,point->viaMarkers.add(map.map.addMarker(MarkerOptions()
                    .position(LatLng(point.lat,point.lng)).title("途经${i+1} · ${point.name}")
                    .icon(BitmapDescriptorFactory.defaultMarker(BitmapDescriptorFactory.HUE_ORANGE))))}
                startMarker=map.map.addMarker(MarkerOptions().position(points.first()).title("起点").icon(BitmapDescriptorFactory.defaultMarker(BitmapDescriptorFactory.HUE_AZURE)))
                endMarker=map.map.addMarker(MarkerOptions().position(points.last()).title(g.destinationTitle).icon(BitmapDescriptorFactory.defaultMarker(BitmapDescriptorFactory.HUE_RED)))
            }
            if(g.choosing||!follow||g.lat.isNaN())overview()
        }
        if(!g.lat.isNaN()){
            val p=LatLng(g.lat,g.lng)
            val positionChanged=poseUpdates.positionChanged(g.lat,g.lng)
            val targetBearing = if(follow) 0f else 360f-g.bearing
            if(position==null){
                position=map.map.addMarker(MarkerOptions().position(p).anchor(.5f,.5f)
                    .icon(BitmapDescriptorFactory.fromBitmap(icon()))
                    .setFlat(false)
                    .rotateAngle(targetBearing).zIndex(5f).title("当前位置"))
            } else {
                if(positionChanged) position?.position=p
                if(position?.rotateAngle != targetBearing) position?.rotateAngle=targetBearing
            }
            if(!g.choosing&&follow&&poseUpdates.cameraChanged(g.lat,g.lng,g.bearing)){
                map.map.animateCamera(CameraUpdateFactory.newCameraPosition(CameraPosition(p,17f,0f,g.bearing)), 500L, null)
            }
        }
        if(resumed)handler.postDelayed(this,500)
    }}
    override fun onResume(){super.onResume();FusionCarUi.closeConsole(this);map.onResume();resumed=true;handler.post(refresh)}
    override fun onPause(){resumed=false;handler.removeCallbacks(refresh);map.onPause();super.onPause()}
    override fun onSaveInstanceState(outState:Bundle){
        SingleEngineDestination.save(outState)
        pendingDestination?.let{outState.putBundle(STATE_PENDING,it.state())}
        map.onSaveInstanceState(outState);super.onSaveInstanceState(outState)
    }
    override fun onDestroy(){
        if(current.get()===this)current.clear()
        destinationDialog?.dismiss();destinationDialog=null
        handler.removeCallbacksAndMessages(null);map.onDestroy();super.onDestroy()
    }
}
