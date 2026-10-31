package com.yuminaga.motonav

import android.graphics.Bitmap
import android.content.Intent
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.graphics.drawable.GradientDrawable
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.text.SpannableString
import android.text.Spanned
import android.text.TextUtils
import android.text.style.AbsoluteSizeSpan
import android.util.TypedValue
import android.view.Gravity
import android.view.MotionEvent
import android.view.View
import android.view.WindowManager
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import androidx.activity.OnBackPressedCallback
import androidx.appcompat.app.AlertDialog
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import androidx.core.view.WindowInsetsControllerCompat
import com.amap.api.maps.AMap
import com.amap.api.maps.CameraUpdateFactory
import com.amap.api.maps.MapView
import com.amap.api.maps.model.BitmapDescriptorFactory
import com.amap.api.maps.model.CameraPosition
import com.amap.api.maps.model.LatLng
import com.amap.api.maps.model.LatLngBounds
import com.amap.api.maps.model.Marker
import com.amap.api.maps.model.MarkerOptions
import com.amap.api.maps.model.Polyline
import com.amap.api.maps.model.PolylineOptions
import java.util.Calendar

/** Web motorcycle guidance, using the same cards, typography and arrow as the driving page. */
class MotorWebNavActivity : AppCompatActivity() {
    private val S=NavStyle
    private fun dp(v:Int)=S.dpi(this,v)
    private val handler=Handler(Looper.getMainLooper())
    private lateinit var root:FrameLayout
    private lateinit var map:MapView
    private lateinit var turnCard:LinearLayout
    private lateinit var arrowBox:FrameLayout
    private lateinit var arrow:TurnArrowView
    private lateinit var distanceText:TextView
    private lateinit var roadText:TextView
    private lateinit var statusText:TextView
    private lateinit var speedBox:LinearLayout
    private lateinit var signalBadge:TrafficLightBadge
    private lateinit var speedNumber:TextView
    private lateinit var bottomCard:LinearLayout
    private lateinit var remainText:TextView
    private lateinit var etaText:TextView
    private lateinit var overviewButton:TextView
    private lateinit var muteButton:TextView
    private lateinit var planButton:TextView
    private lateinit var settingsButton:TextView
    private var providerButton:TextView?=null
    private lateinit var arrivedCard:LinearLayout
    private var pauseButton:TextView?=null
    private var simSpeedButton:TextView?=null
    private var poly:Polyline?=null
    private var marker:Marker?=null
    private var shownRoute=""
    private var follow=true
    private var nearShown:Boolean?=null
    private var mapNight:Boolean?=null
    private val poseUpdates=MapPoseUpdates()
    private var uiResumed=false
    private var insetTop=0
    private var insetBottom=0

    override fun onCreate(savedInstanceState:Bundle?) {
        super.onCreate(savedInstanceState);supportActionBar?.hide()
        if(WebMotorGuide.route==null){finish();return}
        window.addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON)
        WindowCompat.setDecorFitsSystemWindows(window,false)
        WindowInsetsControllerCompat(window,window.decorView).apply {
            hide(WindowInsetsCompat.Type.statusBars())
            systemBarsBehavior=WindowInsetsControllerCompat.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
        }
        window.navigationBarColor=Color.TRANSPARENT
        root=FrameLayout(this).apply{setBackgroundColor(S.BG)}
        map=MapView(this);map.onCreate(savedInstanceState)
        map.map.uiSettings.apply{isZoomControlsEnabled=false;isCompassEnabled=false;isScaleControlsEnabled=true}
        map.map.setOnMapTouchListener{event->
            if(event.actionMasked==MotionEvent.ACTION_MOVE && follow){follow=false;styleControls()}
        }
        root.addView(map,FrameLayout.LayoutParams(-1,-1))
        root.addView(View(this).apply{
            background=GradientDrawable(GradientDrawable.Orientation.TOP_BOTTOM,
                intArrayOf(Color.parseColor("#990E0A14"),Color.TRANSPARENT))
            importantForAccessibility=View.IMPORTANT_FOR_ACCESSIBILITY_NO
        },FrameLayout.LayoutParams(-1,dp(170),Gravity.TOP))
        buildTurnCard();buildSpeed();buildBottomCard();buildArrived()
        setContentView(root)
        ViewCompat.setOnApplyWindowInsetsListener(root){_,insets->
            val bars=insets.getInsets(WindowInsetsCompat.Type.systemBars() or WindowInsetsCompat.Type.displayCutout())
            insetTop=bars.top;insetBottom=bars.bottom
            (turnCard.layoutParams as FrameLayout.LayoutParams).apply{
                leftMargin=bars.left+dp(10);rightMargin=bars.right+dp(10);topMargin=insetTop+dp(8)
            }
            (bottomCard.layoutParams as FrameLayout.LayoutParams).apply{
                leftMargin=bars.left+dp(10);rightMargin=bars.right+dp(10);bottomMargin=insetBottom+dp(8)
            }
            turnCard.requestLayout();bottomCard.requestLayout();positionSide();insets
        }
        turnCard.addOnLayoutChangeListener{_,_,_,_,_,_,_,_,_->positionSide()}
        root.addOnLayoutChangeListener{_,l,t,r,b,oldL,oldT,oldR,oldB->
            if(r-l!=oldR-oldL || b-t!=oldB-oldT){poseUpdates.invalidateCamera();if(!follow)root.post{overview()}}
        }
        onBackPressedDispatcher.addCallback(this,object:OnBackPressedCallback(true){
            override fun handleOnBackPressed(){NavigationUi.openSettings(this@MotorWebNavActivity,0)}
        })
        if(intent.hasExtra(NavigationUi.EXTRA_SETTINGS_PAGE))root.post {
            NavigationUi.openSettings(this,intent.getIntExtra(NavigationUi.EXTRA_SETTINGS_PAGE,1))
        }
    }

    override fun onNewIntent(intent:Intent) {
        super.onNewIntent(intent)
        if(intent.hasExtra(NavigationUi.EXTRA_SETTINGS_PAGE))
            NavigationUi.openSettings(this,intent.getIntExtra(NavigationUi.EXTRA_SETTINGS_PAGE,1))
    }

    private fun text(size:Float,color:Int=S.TEXT,bold:Boolean=false)=TextView(this).apply{
        textSize=size;setTextColor(color);includeFontPadding=false;typeface=if(bold)S.BOLD else S.MEDIUM
    }
    private fun buildTurnCard(){
        turnCard=LinearLayout(this).apply{
            orientation=LinearLayout.VERTICAL;background=S.round(this@MotorWebNavActivity,S.CARD,22f,S.STROKE)
            elevation=S.dp(this@MotorWebNavActivity,10f);setPadding(dp(10),dp(10),dp(14),dp(10))
        }
        val row=LinearLayout(this).apply{gravity=Gravity.CENTER_VERTICAL}
        arrowBox=FrameLayout(this).apply{background=S.gradient(this@MotorWebNavActivity,18f)}
        arrow=TurnArrowView(this)
        arrowBox.addView(arrow,FrameLayout.LayoutParams(-1,-1).apply{setMargins(dp(7),dp(7),dp(7),dp(7))})
        row.addView(arrowBox,LinearLayout.LayoutParams(dp(72),dp(72)))
        val column=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL;setPadding(dp(14),0,0,0)}
        distanceText=text(40f).apply{typeface=S.NUM;isSingleLine=true}
        roadText=text(17f).apply{maxLines=2;ellipsize=TextUtils.TruncateAt.END;setPadding(0,dp(2),0,0)}
        column.addView(distanceText);column.addView(roadText)
        row.addView(column,LinearLayout.LayoutParams(0,-2,1f));turnCard.addView(row)
        statusText=text(11f,S.SUB).apply{
            isSingleLine=true;ellipsize=TextUtils.TruncateAt.END;setPadding(dp(2),dp(8),0,0);minHeight=dp(24)
            contentDescription="导航和圆屏状态，点按查看详情";setOnClickListener{showStatusDetails()}
        }
        turnCard.addView(statusText)
        root.addView(turnCard,FrameLayout.LayoutParams(-1,-2,Gravity.TOP).apply{setMargins(dp(10),dp(8),dp(10),0)})
    }
    private fun buildSpeed(){
        speedBox=LinearLayout(this).apply{
            orientation=LinearLayout.VERTICAL;gravity=Gravity.CENTER
            background=GradientDrawable().apply{shape=GradientDrawable.OVAL;setColor(S.CARD);setStroke(dp(2),S.STROKE)}
            elevation=S.dp(this@MotorWebNavActivity,8f)
        }
        speedNumber=text(24f).apply{typeface=S.NUM;gravity=Gravity.CENTER;text="--"}
        speedBox.addView(speedNumber)
        speedBox.addView(text(9f,S.SUB).apply{text="km/h";gravity=Gravity.CENTER})
        root.addView(speedBox,FrameLayout.LayoutParams(dp(62),dp(62),Gravity.TOP or Gravity.START).apply{
            leftMargin=dp(12);topMargin=dp(146)
        })
        signalBadge=TrafficLightBadge(this)
        root.addView(signalBadge,FrameLayout.LayoutParams(-2,-2,Gravity.TOP or Gravity.START).apply{
            leftMargin=dp(12);topMargin=dp(218)
        })
        planButton=circle("规划",S.CYAN).apply{
            contentDescription="返回路线规划，保持导航";setOnClickListener{finish()}
        }
        root.addView(planButton,FrameLayout.LayoutParams(dp(46),dp(46),Gravity.TOP or Gravity.END).apply{
            rightMargin=dp(12);topMargin=dp(146)
        })
        settingsButton=circle("设置",S.CYAN).apply{
            contentDescription="返回设置，导航继续";setOnClickListener{NavigationUi.openSettings(this@MotorWebNavActivity)}
        }
        root.addView(settingsButton,FrameLayout.LayoutParams(dp(46),dp(46),Gravity.TOP or Gravity.END).apply{
            rightMargin=dp(12);topMargin=dp(204)
        })
        if(packageName=="com.motonav.app.fresh") {
            providerButton=circle("车机",S.CYAN).apply{
                contentDescription="查看车机路口信息，导航继续"
                setOnClickListener{NavigationUi.openProvider(this@MotorWebNavActivity)}
            }
            root.addView(providerButton,FrameLayout.LayoutParams(dp(46),dp(46),Gravity.TOP or Gravity.END).apply{
                rightMargin=dp(12);topMargin=dp(262)
            })
        }
    }
    private fun positionSide(){
        if(!::turnCard.isInitialized || !::speedBox.isInitialized)return
        val top=if(turnCard.bottom>0)turnCard.bottom+dp(12) else insetTop+dp(146)
        (speedBox.layoutParams as FrameLayout.LayoutParams).topMargin=top
        (planButton.layoutParams as FrameLayout.LayoutParams).topMargin=top
        (settingsButton.layoutParams as FrameLayout.LayoutParams).topMargin=top+dp(58)
        providerButton?.let{(it.layoutParams as FrameLayout.LayoutParams).topMargin=top+dp(116);it.requestLayout()}
        (signalBadge.layoutParams as FrameLayout.LayoutParams).topMargin=top+dp(72)
        signalBadge.requestLayout();speedBox.requestLayout();planButton.requestLayout();settingsButton.requestLayout()
    }
    private fun circle(value:String,color:Int)=TextView(this).apply{
        text=value;textSize=12f;setTextColor(color);typeface=S.BOLD;gravity=Gravity.CENTER
        background=GradientDrawable().apply{shape=GradientDrawable.OVAL;setColor(S.CARD2)}
        elevation=S.dp(this@MotorWebNavActivity,2f)
    }
    private fun buildBottomCard(){
        bottomCard=LinearLayout(this).apply{
            orientation=LinearLayout.VERTICAL;background=S.round(this@MotorWebNavActivity,S.CARD,22f,S.STROKE)
            elevation=S.dp(this@MotorWebNavActivity,10f);setPadding(dp(14),dp(10),dp(10),dp(10))
        }
        val row=LinearLayout(this).apply{gravity=Gravity.CENTER_VERTICAL}
        val summary=LinearLayout(this).apply{orientation=LinearLayout.VERTICAL}
        remainText=text(18f,S.TEXT,true).apply{
            isSingleLine=true;ellipsize=TextUtils.TruncateAt.END
            setAutoSizeTextTypeUniformWithConfiguration(12,18,1,TypedValue.COMPLEX_UNIT_SP)
        }
        etaText=text(12f,S.SUB).apply{isSingleLine=true;ellipsize=TextUtils.TruncateAt.END;setPadding(0,dp(5),0,0)}
        summary.addView(remainText);summary.addView(etaText)
        row.addView(summary,LinearLayout.LayoutParams(0,-2,1f))
        overviewButton=circle("全览",S.CYAN).apply{
            contentDescription="全览路线或跟随当前位置"
            setOnClickListener{follow=!follow;poseUpdates.invalidateCamera();if(!follow)overview();styleControls()}
        }
        muteButton=circle("播报",S.TEXT).apply{
            contentDescription="切换导航语音"
            setOnClickListener{
                NaviVoice.muted=!NaviVoice.muted
                getSharedPreferences("motonav",0).edit().putBoolean("nav_muted",NaviVoice.muted).apply()
                styleControls()
            }
        }
        val exit=circle("✕",S.RED).apply{
            textSize=18f;contentDescription="退出导航，长按可直接退出"
            setOnClickListener{confirmExit()};setOnLongClickListener{WebMotorGuide.stop();finish();true}
        }
        for(button in listOf(overviewButton,muteButton,exit)){
            row.addView(button,LinearLayout.LayoutParams(dp(46),dp(46)).apply{leftMargin=dp(6)})
        }
        bottomCard.addView(row)
        if(WebMotorGuide.simulation){
            bottomCard.addView(View(this).apply{setBackgroundColor(S.STROKE)},LinearLayout.LayoutParams(-1,dp(1)).apply{
                topMargin=dp(10);bottomMargin=dp(6)
            })
            val simRow=LinearLayout(this).apply{gravity=Gravity.CENTER_VERTICAL}
            simRow.addView(text(12f,S.SUB).apply{text="模拟导航";gravity=Gravity.CENTER_VERTICAL},LinearLayout.LayoutParams(0,dp(36),1f))
            pauseButton=chip("暂停",S.AMBER).apply{setOnClickListener{WebMotorGuide.togglePause();styleControls()}}
            simSpeedButton=chip("60 km/h",S.CYAN).apply{
                setOnClickListener{
                    WebMotorGuide.simulationKmh=when(WebMotorGuide.simulationKmh){60->100;100->120;else->60}
                    styleControls()
                }
            }
            simRow.addView(pauseButton,LinearLayout.LayoutParams(dp(70),dp(36)))
            simRow.addView(simSpeedButton,LinearLayout.LayoutParams(dp(96),dp(36)).apply{leftMargin=dp(8)})
            bottomCard.addView(simRow)
        }
        root.addView(bottomCard,FrameLayout.LayoutParams(-1,-2,Gravity.BOTTOM).apply{setMargins(dp(10),0,dp(10),dp(8))})
        styleControls()
    }
    private fun chip(value:String,color:Int)=text(13f,color,true).apply{
        text=value;gravity=Gravity.CENTER;background=S.round(this@MotorWebNavActivity,S.CARD2,12f)
    }
    private fun buildArrived(){
        arrivedCard=LinearLayout(this).apply{
            orientation=LinearLayout.VERTICAL;gravity=Gravity.CENTER
            background=S.round(this@MotorWebNavActivity,S.CARD_SOLID,24f,S.PINK,2f)
            elevation=S.dp(this@MotorWebNavActivity,14f);setPadding(dp(24),dp(20),dp(24),dp(20));visibility=View.GONE
        }
        arrivedCard.addView(text(26f,S.PINK,true).apply{text="已到达目的地";gravity=Gravity.CENTER})
        arrivedCard.addView(text(15f).apply{
            text=WebMotorGuide.route?.request?.end?.name.orEmpty();gravity=Gravity.CENTER
            maxLines=2;setPadding(0,dp(8),0,dp(14))
        })
        arrivedCard.addView(chip("完成导航",S.CYAN).apply{setOnClickListener{WebMotorGuide.stop();finish()}},
            LinearLayout.LayoutParams(-1,dp(44)))
        root.addView(arrivedCard,FrameLayout.LayoutParams(-2,-2,Gravity.CENTER).apply{leftMargin=dp(24);rightMargin=dp(24)})
    }
    private fun styleControls(){
        overviewButton.text=if(follow)"全览" else "跟随";overviewButton.setTextColor(if(follow)S.CYAN else S.PINK)
        muteButton.text=if(NaviVoice.muted)"静音" else "播报";muteButton.setTextColor(if(NaviVoice.muted)S.SUB else S.TEXT)
        pauseButton?.text=if(WebMotorGuide.paused)"继续" else "暂停"
        pauseButton?.setTextColor(if(WebMotorGuide.paused)S.GREEN else S.AMBER)
        simSpeedButton?.text="${WebMotorGuide.simulationKmh} km/h"
    }
    private fun overview(){
        val r=WebMotorGuide.route ?:return
        val bounds=LatLngBounds.Builder();r.points.forEach{bounds.include(LatLng(it[0],it[1]))}
        map.post{
            runCatching{map.map.moveCamera(CameraUpdateFactory.newLatLngBoundsRect(bounds.build(),dp(28),dp(28),
                turnCard.bottom+dp(20),(root.height-bottomCard.top).coerceAtLeast(dp(100))+dp(20)))}
        }
    }
    private fun carBitmap():Bitmap{
        val size=dp(42);val image=Bitmap.createBitmap(size,size,Bitmap.Config.ARGB_8888)
        val canvas=Canvas(image);val paint=Paint(Paint.ANTI_ALIAS_FLAG)
        val path=Path().apply{
            moveTo(size*.5f,size*.08f);lineTo(size*.86f,size*.9f);lineTo(size*.5f,size*.7f);lineTo(size*.14f,size*.9f);close()
        }
        paint.style=Paint.Style.STROKE;paint.color=S.PINK;paint.strokeWidth=size*.08f;canvas.drawPath(path,paint)
        paint.style=Paint.Style.FILL;paint.color=Color.WHITE;canvas.drawPath(path,paint);return image
    }
    private val refresh=object:Runnable{
        override fun run(){
            if(!uiResumed)return
            val route=WebMotorGuide.route
            if(route==null){finish();return}
            val g=WebMotorGuide;arrow.dir=g.direction;arrow.angle=null
            val(number,unit)=S.distParts(g.turnMeters)
            val span=SpannableString("$number $unit")
            span.setSpan(AbsoluteSizeSpan(dp(17)),number.length,span.length,Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
            distanceText.text=if(g.arrived)"已到达" else span;roadText.text=g.instruction
            val near=g.direction!="S" && g.turnMeters in 0..100 && !g.arrived
            if(near!=nearShown){
                nearShown=near
                arrowBox.background=if(near)GradientDrawable(GradientDrawable.Orientation.LEFT_RIGHT,
                    intArrayOf(S.AMBER,Color.parseColor("#FF7A3D"))).apply{cornerRadius=S.dp(this@MotorWebNavActivity,18f)}
                    else S.gradient(this@MotorWebNavActivity,18f)
            }
            val normal=g.status.startsWith("网页摩托车")
            val state=if(normal){if(g.simulation){if(g.paused)"模拟已暂停" else "模拟导航"}else"摩托车导航"}else g.status
            statusText.text="$state · ${if(BleLink.verified)"圆屏已连接" else "圆屏未连接"}"+
                if(CarNavSync.requested)"\n${CarNavSync.webSummary()}" else ""
            statusText.setTextColor(if(!normal && !g.arrived)S.AMBER else S.SUB)
            speedNumber.text=if(g.lat.isNaN())"--" else "${g.speed}"
            remainText.text=if(g.arrived)"行程完成" else "${S.distText(g.remaining)} · ${S.durText(g.remainingS)}"
            val cal=Calendar.getInstance().apply{add(Calendar.SECOND,g.remainingS)}
            etaText.text=if(g.arrived)route.request.end.name else
                "%02d:%02d 预计到达".format(cal.get(Calendar.HOUR_OF_DAY),cal.get(Calendar.MINUTE))
            arrivedCard.visibility=if(g.arrived)View.VISIBLE else View.GONE
            if(mapNight!=MapFeeder.nightNow){mapNight=MapFeeder.nightNow;map.map.mapType=if(mapNight==true)AMap.MAP_TYPE_NIGHT else AMap.MAP_TYPE_NORMAL}
            if(route.key!=shownRoute){
                shownRoute=route.key;poly?.remove()
                poly=map.map.addPolyline(PolylineOptions().addAll(route.points.map{LatLng(it[0],it[1])})
                    .width(dp(7).toFloat()).color(S.PINK).zIndex(1f))
                poseUpdates.invalidateCamera();if(!follow || g.lat.isNaN())overview()
            }
            if(!g.lat.isNaN()){
                val pose=LatLng(g.lat,g.lng)
                val positionChanged=poseUpdates.positionChanged(g.lat,g.lng)
                val targetBearing = if(follow) 0f else 360f-g.bearing
                if(marker==null){
                    marker=map.map.addMarker(MarkerOptions().position(pose).anchor(.5f,.5f)
                        .icon(BitmapDescriptorFactory.fromBitmap(carBitmap()))
                        .apply{setFlat(false)}.rotateAngle(targetBearing).zIndex(5f).title("当前位置"))
                } else {
                    if(positionChanged) marker?.position=pose
                    if(marker?.rotateAngle != targetBearing) marker?.rotateAngle=targetBearing
                }
                if(follow && poseUpdates.cameraChanged(g.lat,g.lng,g.bearing)){
                    map.map.animateCamera(CameraUpdateFactory.newCameraPosition(CameraPosition(pose,17f,0f,g.bearing)), 500L, null)
                }
            }
            styleControls();if(uiResumed)handler.postDelayed(this,500)
        }
    }
    private fun showStatusDetails(){
        AlertDialog.Builder(this).setTitle("摩托车导航状态")
            .setMessage("${WebMotorGuide.status}\n${CarNavSync.webSummary()}\n${WebMotorSignals.status}\n${MapFeeder.loadingStatus()}\n\n此实验使用网页摩托车路线。后台车机通过最多三个关键点约束算路，仍可能不同。只有路网信号灯节点与网页路线、行驶方向匹配，且车机前方路口也一致时才显示读秒。缺少节点或数据时隐藏。车道、电子眼和道路限速未提供，预计到达时间按路线进度估算。")
            .setPositiveButton("知道了",null).show()
    }
    private fun confirmExit(){
        AlertDialog.Builder(this).setMessage("结束这次摩托车导航？")
            .setPositiveButton("结束"){_,_->WebMotorGuide.stop();finish()}.setNegativeButton("继续导航",null).show()
    }
    override fun onResume(){
        super.onResume();FusionCarUi.closeConsole(this)
        if(::map.isInitialized){map.onResume();uiResumed=true;handler.removeCallbacks(refresh);handler.post(refresh)}
    }
    override fun onPause(){
        uiResumed=false;handler.removeCallbacks(refresh)
        if(::map.isInitialized)map.onPause();super.onPause()
    }
    override fun onSaveInstanceState(out:Bundle){if(::map.isInitialized)map.onSaveInstanceState(out);super.onSaveInstanceState(out)}
    override fun onDestroy(){handler.removeCallbacksAndMessages(null);if(::map.isInitialized)map.onDestroy();super.onDestroy()}
}
