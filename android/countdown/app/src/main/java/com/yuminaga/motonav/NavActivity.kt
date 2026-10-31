package com.yuminaga.motonav

import android.content.Context
import android.content.Intent
import android.graphics.Bitmap
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.Paint
import android.graphics.Path
import android.graphics.drawable.GradientDrawable
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.text.SpannableString
import android.text.Spanned
import android.text.TextUtils
import android.text.style.AbsoluteSizeSpan
import android.view.Gravity
import android.view.View
import android.view.WindowInsets
import android.view.WindowInsetsController
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.TextView
import android.widget.Toast
import androidx.activity.OnBackPressedCallback
import androidx.activity.ComponentActivity
import com.amap.api.navi.AMapNavi
import com.amap.api.navi.AMapNaviView
import com.amap.api.navi.AMapNaviViewListener
import com.amap.api.navi.AMapNaviViewOptions
import java.lang.reflect.Proxy
import java.util.Calendar

/**
 * 版本：跟随 App 版本，见 MainActivity.kt 里的 APP_VERSION（v3.0 新增）
 *
 * 导航页（代替高德组件的导航界面）：
 * - 底图用高德导航地图（隐藏高德自带的按钮和面板），白天/夜间跟小地图的夜间配色设置走
 * - 上面是自己画的霓虹风界面：顶部转向卡片（箭头角度跟随路口、距离、路名、车道），
 *   左侧车速和测速牌，底部剩余里程/时间/到达时间 + 全览、静音、退出
 * - 按 1080×1200 这种接近正方形的屏幕排版：隐藏状态栏，卡片都尽量扁，中间留给地图
 * - 长按退出结束导航；设置按钮和系统返回只切换页面，导航继续运行
 * - v3.2 模拟导航时底栏多出“暂停/继续”和“模拟车速”（40/60/100/150 循环）两个按钮
 * 导航数据全部来自 NaviBridge（同一份数据也在发给圆屏）。
 */
// AMap inflates its navigation UI from a separate resource APK. An AppCompat
// inflater would substitute widgets that require our 0x80 resource package,
// which is deliberately absent from the SDK's private AssetManager.
class NavActivity : ComponentActivity() {
    private val S = NavStyle
    private fun dp(v: Int) = S.dpi(this, v)
    private val handler = Handler(Looper.getMainLooper())

    private lateinit var naviView: AMapNaviView
    private var navi: AMapNavi? = null
    private var emulate = false
    private var destName = ""

    private lateinit var root: FrameLayout
    private lateinit var turnCard: LinearLayout
    private lateinit var mapStatus: TextView
    private lateinit var arrowBox: FrameLayout
    private lateinit var arrow: TurnArrowView
    private lateinit var distText: TextView
    private lateinit var roadText: TextView
    private lateinit var laneView: LaneView
    private lateinit var laneBox: FrameLayout
    private lateinit var sideCol: LinearLayout
    private lateinit var speedNum: TextView
    private lateinit var speedBox: LinearLayout
    private lateinit var camBox: LinearLayout
    private lateinit var camSign: TextView
    private lateinit var camDist: TextView
    private lateinit var lightChip: TextView
    private lateinit var bottomBar: LinearLayout
    private lateinit var remainText: TextView
    private lateinit var etaText: TextView
    private lateinit var overviewBtn: TextView
    private lateinit var muteBtn: TextView
    private lateinit var arrivedCard: LinearLayout
    private lateinit var settingsBtn: TextView
    private var providerBtn:TextView?=null

    private var overview = false
    private var muted = false
    // v3.2 模拟导航：暂停 / 继续，车速可调
    private var paused = false
    private val emuSpeeds = intArrayOf(40, 60, 100, 150)
    private var emuIdx = 1
    private var pauseBtn: TextView? = null
    private var emuSpeedBtn: TextView? = null
    private var exited = false
    private var navigationEpoch = -1L
    private var redirectedToSingle=false
    private var arrivedAt = 0L
    private var nearShown: Boolean? = null

    @Suppress("DEPRECATION")
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        // A navigation view allocates the SDK engine inside its constructor.
        // Redirect before constructing it when the native-only trip owns guidance.
        if(SingleEngineSession.owns){
            redirectedToSingle=true;NavigationUi.returnToNavigation(this);finish();return
        }
        emulate = if(NaviBridge.hasActiveNavigation())NaviBridge.navigationIsSimulation()
            else savedInstanceState?.getBoolean("nav_emulate") ?: intent.getBooleanExtra("emu", false)
        destName = savedInstanceState?.getString("nav_destination") ?: intent.getStringExtra("dest") ?: ""
        muted = getSharedPreferences("motonav", Context.MODE_PRIVATE).getBoolean("nav_muted", false)

        // 全屏：隐藏状态栏（方屏省下一条），地图铺到底
        window.navigationBarColor = Color.TRANSPARENT
        if (Build.VERSION.SDK_INT >= 30) {
            window.setDecorFitsSystemWindows(false)
        } else {
            window.decorView.systemUiVisibility = View.SYSTEM_UI_FLAG_LAYOUT_STABLE or
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN or View.SYSTEM_UI_FLAG_FULLSCREEN
        }

        root = FrameLayout(this).apply { setBackgroundColor(S.BG) }
        naviView = AMapNaviView(this, makeOptions())
        naviView.onCreate(savedInstanceState)
        root.addView(naviView, FrameLayout.LayoutParams(-1, -1))
        try { naviView.setAMapNaviViewListener(makeViewListener()) } catch (_: Throwable) {}
        // v4.5 按导入的分享路线导航：原路线在手机地图上画成橙色细线（导航路线盖在上面，分开的地方才看得到）
        ShareGuide.route?.takeIf { !it.endpointsOnly }?.let { r ->
            try {
                naviView.map?.addPolyline(com.amap.api.maps.model.PolylineOptions()
                    .addAll(r.pts.map { com.amap.api.maps.model.LatLng(it[0], it[1]) })
                    .width(S.dp(this, 5f)).color(Color.parseColor("#E6FF9500")).zIndex(1f))
            } catch (_: Throwable) {}
        }

        // 顶部一条渐暗，让卡片和地图分开
        root.addView(View(this).apply {
            background = GradientDrawable(GradientDrawable.Orientation.TOP_BOTTOM,
                intArrayOf(Color.parseColor("#990E0A14"), Color.TRANSPARENT))
        }, FrameLayout.LayoutParams(-1, dp(150)))

        buildTurnCard()
        mapStatus = TextView(this).apply { textSize=11f; setTextColor(S.SUB); maxLines=3; setPadding(0,dp(6),0,0) }
        turnCard.addView(mapStatus)
        buildSide()
        settingsBtn = circleBtn("设置",12f,S.CYAN).apply {
            contentDescription="返回设置，导航继续"
            setOnClickListener { NavigationUi.openSettings(this@NavActivity) }
        }
        root.addView(settingsBtn,FrameLayout.LayoutParams(dp(46),dp(46),Gravity.TOP or Gravity.END).apply {
            rightMargin=dp(12);topMargin=dp(160)
        })
        if(packageName=="com.motonav.app.fresh") {
            providerBtn=circleBtn("车机",12f,S.CYAN).apply {
                contentDescription="查看车机路口信息，导航继续"
                setOnClickListener{NavigationUi.openProvider(this@NavActivity)}
            }
            root.addView(providerBtn,FrameLayout.LayoutParams(dp(46),dp(46),Gravity.TOP or Gravity.END).apply{
                rightMargin=dp(12);topMargin=dp(218)
            })
        }
        buildBottomBar()
        buildArrived()
        setContentView(root)
        // ComponentActivity does not eagerly install a DecorView. Access the
        // window insets controller only after this view is attached.
        if (Build.VERSION.SDK_INT >= 30) root.post {
            root.windowInsetsController?.let {
                it.hide(WindowInsets.Type.statusBars())
                it.systemBarsBehavior = WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE
            }
        }

        root.setOnApplyWindowInsetsListener { _, ins ->
            var top: Int; var bottom: Int
            if (Build.VERSION.SDK_INT >= 30) {
                val sb = ins.getInsets(WindowInsets.Type.systemBars() or WindowInsets.Type.displayCutout())
                top = sb.top; bottom = sb.bottom
            } else { top = ins.systemWindowInsetTop; bottom = ins.systemWindowInsetBottom }
            (turnCard.layoutParams as FrameLayout.LayoutParams).topMargin = top + dp(8)
            (bottomBar.layoutParams as FrameLayout.LayoutParams).bottomMargin = bottom + dp(8)
            turnCard.requestLayout(); bottomBar.requestLayout()
            ins
        }
        turnCard.addOnLayoutChangeListener { _, _, _, _, b, _, _, _, _ ->
            (sideCol.layoutParams as FrameLayout.LayoutParams).topMargin = b + dp(10)
            sideCol.requestLayout()
            (settingsBtn.layoutParams as FrameLayout.LayoutParams).topMargin=b+dp(10)
            settingsBtn.requestLayout()
            providerBtn?.let{(it.layoutParams as FrameLayout.LayoutParams).topMargin=b+dp(68);it.requestLayout()}
        }

        onBackPressedDispatcher.addCallback(this, object : OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                if(NaviBridge.hasActiveNavigation())NavigationUi.openSettings(this@NavActivity,0)
                else exitNav()
            }
        })

        if(NaviBridge.hasActiveNavigation())resumeNavigationPage() else startNavi()
        handler.post(refresh)
        if(intent.hasExtra(NavigationUi.EXTRA_SETTINGS_PAGE) && NaviBridge.hasActiveNavigation())
            root.post { NavigationUi.openSettings(this,intent.getIntExtra(NavigationUi.EXTRA_SETTINGS_PAGE,1)) }
    }

    // ================= 高德导航地图 =================

    override fun onNewIntent(intent: Intent) {
        super.onNewIntent(intent)
        if(intent.hasExtra(NavigationUi.EXTRA_SETTINGS_PAGE) && NaviBridge.hasActiveNavigation()) {
            NavigationUi.openSettings(this,intent.getIntExtra(NavigationUi.EXTRA_SETTINGS_PAGE,1))
            return
        }
        setIntent(intent)
        // singleTask reuses this page after planning a new trip. onCreate does
        // not run again, so arm/start the newly prepared navigation session here.
        if (navigationEpoch != NaviBridge.navigationEpoch()) {
            // Keep the SDK navigation view alive: its destruction can tear down
            // the shared navigation engine after a replacement trip has started.
            emulate=intent.getBooleanExtra("emu",false)
            destName=intent.getStringExtra("dest") ?: ""
            exited=false;paused=false;arrivedAt=0L;nearShown=null;overview=false;emuIdx=1
            arrivedCard.visibility=View.GONE
            (arrivedCard.getChildAt(1) as? TextView)?.text=destName
            root.removeView(bottomBar);pauseBtn=null;emuSpeedBtn=null
            buildBottomBar();root.requestApplyInsets()
            if(NaviBridge.hasActiveNavigation())resumeNavigationPage() else startNavi()
        }
    }

    private fun resumeNavigationPage() {
        navigationEpoch=NaviBridge.navigationEpoch()
        navi=AMapNavi.getInstance(applicationContext)
        NaviVoice.applyTo(navi!!)
        android.util.Log.i("MotoNavLifecycle","resume-page epoch=$navigationEpoch")
    }

    private fun makeOptions(): AMapNaviViewOptions {
        val o = AMapNaviViewOptions()
        o.setLayoutVisible(false)              // 不要高德自带的按钮和面板，界面我们自己画
        o.setAutoLockCar(true)                 // 拖动地图 6 秒后自动回到车头
        o.setTilt(30)
        // 下面这些不同版本 SDK 可能没有，用“试着调用”的方式，没有就跳过
        Refl.call(o, "setTrafficLine", true)
        Refl.call(o, "setTrafficLayerEnabled", true)
        Refl.call(o, "setAutoChangeZoom", true)
        Refl.call(o, "setPointToCenter", 0.5, 0.64)     // 车的位置偏下，前方的路多看一点
        Refl.call(o, "setLaneInfoShow", false)
        Refl.call(o, "setModeCrossDisplayShow", false)
        Refl.call(o, "setRealCrossDisplay", false)
        Refl.call(o, "setTrafficBarEnabled", false)
        Refl.call(o, "setCompassEnabled", false)
        Refl.call(o, "setSettingMenuEnabled", false)
        Refl.call(o, "setCarBitmap", carBitmap())
        setNightStyle(o, MapFeeder.nightNow)
        return o
    }

    /** 夜间地图：新版本 SDK 用 setMapStyle(MapStyle.NIGHT, null)，老版本用 setNaviNight(true) */
    private fun setNightStyle(o: AMapNaviViewOptions, night: Boolean) {
        val names = listOf("com.amap.api.navi.enums.MapStyle", "com.amap.api.navi.MapStyle", "com.amap.api.navi.view.MapStyle")
        for (n in names) {
            try {
                val cls = Class.forName(n)
                val v = cls.enumConstants?.firstOrNull { (it as Enum<*>).name == (if (night) "NIGHT" else "NORMAL") } ?: continue
                for (m in o.javaClass.methods) {
                    if (m.name == "setMapStyle" && m.parameterTypes.size == 2 && m.parameterTypes[0] == cls) {
                        m.invoke(o, v, null); return
                    }
                }
            } catch (_: Throwable) {}
        }
        Refl.call(o, "setNaviNight", night)
        Refl.call(o, "setAutoNaviViewNightMode", false)
    }

    /** 地图上的车：白色箭头，粉色描边 */
    private fun carBitmap(): Bitmap {
        val s = dp(46)
        val b = Bitmap.createBitmap(s, s, Bitmap.Config.ARGB_8888)
        val c = Canvas(b)
        val p = Paint(Paint.ANTI_ALIAS_FLAG)
        val path = Path().apply {
            moveTo(s * 0.5f, s * 0.08f); lineTo(s * 0.86f, s * 0.9f); lineTo(s * 0.5f, s * 0.7f); lineTo(s * 0.14f, s * 0.9f); close()
        }
        p.style = Paint.Style.STROKE; p.strokeJoin = Paint.Join.ROUND
        p.color = Color.parseColor("#66FF4FB4"); p.strokeWidth = s * 0.16f; c.drawPath(path, p)
        p.color = S.PINK; p.strokeWidth = s * 0.07f; c.drawPath(path, p)
        p.style = Paint.Style.FILL; p.color = Color.WHITE; c.drawPath(path, p)
        return b
    }

    private fun startNavi() {
        try {
            navigationEpoch=NaviBridge.navigationEpoch()
            val n = AMapNavi.getInstance(applicationContext)
            navi = n
            // 高德自带语音播报（静音时关掉）
            NaviVoice.load(this)                         // v3.8 按“导航语音”设置决定谁来播报
            NaviVoice.muted = muted
            NaviVoice.applyTo(n)
            ShareGuide.naviType = if (emulate) 2 else 1               // v4.5
            Refl.call(n, "setMultipleRouteNaviMode", ShareGuide.route?.isTravel != true && NaviBridge.directRouteMode != LocalRouting.BICYCLE)
            if (emulate) n.setEmulatorNaviSpeed(emuSpeeds[emuIdx])
            if (!NaviBridge.beginNavigation(emulate)) {
                Toast.makeText(this,"导航会话已结束，请返回重新规划路线",Toast.LENGTH_LONG).show()
                return
            }
            CarNavSync.request(this,n,emulate)
            val started=n.startNavi(if (emulate) 2 else 1)
            android.util.Log.i("MotoNavLifecycle","start-result epoch=$navigationEpoch started=$started")
            if (started) {
                RideRecorder.start("导航",destName,emulate)
            } else {
                NaviBridge.navigationStartFailed("SDK startNavi returned false")
                Toast.makeText(this,"导航启动失败，请返回重新规划路线",Toast.LENGTH_LONG).show()
            }
        } catch (e: Exception) {
            NaviBridge.navigationStartFailed("startNavi exception: ${e.javaClass.simpleName}")
            Toast.makeText(this, "导航启动失败：${e.message}", Toast.LENGTH_LONG).show()
        }
    }

    /** 只关心“地图是否锁定在车头”，其余回调忽略 */
    private fun makeViewListener(): AMapNaviViewListener =
        Proxy.newProxyInstance(AMapNaviViewListener::class.java.classLoader, arrayOf(AMapNaviViewListener::class.java)) { proxy, m, args ->
            when (m.name) {
                "onLockMap" -> {
                    val locked = args?.getOrNull(0) as? Boolean ?: true
                    handler.post { if (locked) { overview = false; styleOverview() } }
                }
                "hashCode" -> return@newProxyInstance System.identityHashCode(proxy)
                "equals" -> return@newProxyInstance (proxy === args?.getOrNull(0))
                "toString" -> return@newProxyInstance "MotoNavViewListener"
            }
            when (m.returnType) {
                java.lang.Boolean.TYPE -> false
                Integer.TYPE -> 0
                else -> null
            }
        } as AMapNaviViewListener

    // ================= 界面 =================

    private fun buildTurnCard() {
        turnCard = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            background = S.round(this@NavActivity, S.CARD, 22f, S.STROKE)
            elevation = S.dp(this@NavActivity, 10f)
            setPadding(dp(10), dp(10), dp(14), dp(10))
        }
        val row = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL }
        arrowBox = FrameLayout(this).apply { background = S.gradient(this@NavActivity, 18f) }
        arrow = TurnArrowView(this)
        arrowBox.addView(arrow, FrameLayout.LayoutParams(-1, -1).apply { setMargins(dp(6), dp(6), dp(6), dp(6)) })
        row.addView(arrowBox, LinearLayout.LayoutParams(dp(72), dp(72)))
        val col = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(14), 0, 0, 0) }
        distText = TextView(this).apply {
            setTextColor(S.TEXT); typeface = S.NUM; textSize = 40f; includeFontPadding = false; isSingleLine = true
        }
        roadText = TextView(this).apply {
            setTextColor(S.TEXT); typeface = S.MEDIUM; textSize = 17f; isSingleLine = true
            ellipsize = TextUtils.TruncateAt.END; setPadding(0, dp(2), 0, 0)
            text = "正在开始导航…"
        }
        col.addView(distText); col.addView(roadText)
        row.addView(col, LinearLayout.LayoutParams(0, -2, 1f))
        turnCard.addView(row)
        laneView = LaneView(this)
        laneBox = FrameLayout(this).apply { visibility = View.GONE; setPadding(0, dp(8), 0, 0) }
        laneBox.addView(laneView, FrameLayout.LayoutParams(-2, dp(30), Gravity.CENTER_HORIZONTAL))
        turnCard.addView(laneBox, LinearLayout.LayoutParams(-1, -2))
        root.addView(turnCard, FrameLayout.LayoutParams(-1, -2, Gravity.TOP).apply {
            leftMargin = dp(10); rightMargin = dp(10); topMargin = dp(10)
        })
    }

    private fun buildSide() {
        sideCol = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; gravity = Gravity.CENTER_HORIZONTAL }
        // 车速
        speedBox = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL; gravity = Gravity.CENTER
            background = GradientDrawable().apply { shape = GradientDrawable.OVAL; setColor(S.CARD); setStroke(dp(2), S.STROKE) }
            elevation = S.dp(this@NavActivity, 8f)
        }
        speedNum = TextView(this).apply {
            setTextColor(S.TEXT); typeface = S.NUM; textSize = 22f; includeFontPadding = false; gravity = Gravity.CENTER; text = "--"
        }
        speedBox.addView(speedNum)
        speedBox.addView(TextView(this).apply { text = "km/h"; textSize = 9f; setTextColor(S.SUB); gravity = Gravity.CENTER })
        sideCol.addView(speedBox, LinearLayout.LayoutParams(dp(60), dp(60)))
        // 测速牌
        camBox = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; gravity = Gravity.CENTER_HORIZONTAL; visibility = View.GONE }
        camSign = TextView(this).apply {
            setTextColor(Color.BLACK); typeface = S.NUM; textSize = 20f; gravity = Gravity.CENTER; includeFontPadding = false
            background = GradientDrawable().apply { shape = GradientDrawable.OVAL; setColor(Color.WHITE); setStroke(dp(5), S.RED) }
            elevation = S.dp(this@NavActivity, 8f)
        }
        camDist = TextView(this).apply {
            setTextColor(Color.WHITE); textSize = 12f; typeface = S.BOLD; gravity = Gravity.CENTER
            background = S.round(this@NavActivity, S.CARD, 9f); setPadding(dp(6), dp(1), dp(6), dp(1))
        }
        camBox.addView(camSign, LinearLayout.LayoutParams(dp(54), dp(54)))
        camBox.addView(camDist, LinearLayout.LayoutParams(-2, -2).apply { topMargin = dp(4) })
        sideCol.addView(camBox, LinearLayout.LayoutParams(-2, -2).apply { topMargin = dp(10) })
        // v3.1 前方红绿灯
        lightChip = TextView(this).apply {
            textSize = 12f; typeface = S.BOLD; setTextColor(Color.WHITE); gravity = Gravity.CENTER
            background = S.round(this@NavActivity, S.CARD, 12f, S.STROKE)
            setPadding(dp(8), dp(4), dp(8), dp(4)); visibility = View.GONE
        }
        sideCol.addView(lightChip, LinearLayout.LayoutParams(-2, -2).apply { topMargin = dp(10) })
        sideCol.addView(TrafficLightBadge(this), LinearLayout.LayoutParams(-2,-2).apply { topMargin=dp(10) })
        root.addView(sideCol, FrameLayout.LayoutParams(-2, -2, Gravity.TOP or Gravity.START).apply {
            leftMargin = dp(12); topMargin = dp(140)
        })
    }

    private fun circleBtn(t: String, size: Float, color: Int) = TextView(this).apply {
        text = t; textSize = size; setTextColor(color); gravity = Gravity.CENTER; typeface = S.BOLD
        background = GradientDrawable().apply { shape = GradientDrawable.OVAL; setColor(S.CARD2) }
    }

    private fun buildBottomBar() {
        bottomBar = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL
            background = S.round(this@NavActivity, S.CARD, 22f, S.STROKE)
            elevation = S.dp(this@NavActivity, 10f)
            setPadding(dp(16), dp(8), dp(8), dp(8))
        }
        val col = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        remainText = TextView(this).apply {
            setTextColor(S.TEXT); typeface = S.BOLD; textSize = 18f; isSingleLine = true; ellipsize = TextUtils.TruncateAt.END
        }
        etaText = TextView(this).apply {
            setTextColor(S.SUB); textSize = 12.5f; isSingleLine = true; ellipsize = TextUtils.TruncateAt.END
        }
        col.addView(remainText); col.addView(etaText)
        bottomBar.addView(col, LinearLayout.LayoutParams(0, -2, 1f))
        overviewBtn = circleBtn("全览", 12f, S.CYAN).apply { setOnClickListener { toggleOverview() } }
        muteBtn = circleBtn("", 12f, S.TEXT).apply { setOnClickListener { toggleMute() } }
        val exitBtn = circleBtn("✕", 18f, S.RED).apply {
            setOnClickListener { Toast.makeText(this@NavActivity, "长按退出导航", Toast.LENGTH_SHORT).show() }
            setOnLongClickListener { exitNav(); true }
        }
        // v3.2 模拟导航时多两个按钮：暂停/继续、模拟车速
        val btns = ArrayList<TextView>()
        if (emulate) {
            pauseBtn = circleBtn("暂停", 12f, S.AMBER).apply { setOnClickListener { togglePause() } }
            emuSpeedBtn = circleBtn("${emuSpeeds[emuIdx]}", 13f, S.CYAN).apply { setOnClickListener { cycleEmuSpeed() } }
            btns += pauseBtn!!; btns += emuSpeedBtn!!
        }
        btns += overviewBtn; btns += muteBtn; btns += exitBtn
        val bs = if (emulate) dp(42) else dp(46)
        for (b in btns) bottomBar.addView(b, LinearLayout.LayoutParams(bs, bs).apply { leftMargin = dp(if (emulate) 5 else 6) })
        root.addView(bottomBar, FrameLayout.LayoutParams(-1, -2, Gravity.BOTTOM).apply {
            leftMargin = dp(10); rightMargin = dp(10); bottomMargin = dp(10)
        })
        styleOverview(); styleMute()
    }

    private fun buildArrived() {
        arrivedCard = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL; gravity = Gravity.CENTER
            background = S.round(this@NavActivity, S.CARD_SOLID, 24f, S.PINK, 2f)
            elevation = S.dp(this@NavActivity, 14f)
            setPadding(dp(24), dp(20), dp(24), dp(20))
            visibility = View.GONE
        }
        arrivedCard.addView(TextView(this).apply { text = "已到达"; textSize = 26f; typeface = S.BOLD; setTextColor(S.PINK); gravity = Gravity.CENTER })
        arrivedCard.addView(TextView(this).apply {
            text = destName; textSize = 15f; setTextColor(S.TEXT); gravity = Gravity.CENTER; setPadding(0, dp(6), 0, 0)
        })
        root.addView(arrivedCard, FrameLayout.LayoutParams(-2, -2, Gravity.CENTER))
        buildSuggest()
    }

    // ---- v4.9 更快路线提示：高德发现更快的路时弹出，点“换这条”切换（30 秒没点自动收起） ----
    private lateinit var suggestCard: LinearLayout
    private lateinit var suggestText: TextView
    private fun buildSuggest() {
        suggestCard = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL
            background = S.round(this@NavActivity, S.CARD_SOLID, 18f, S.GREEN, 2f)
            elevation = S.dp(this@NavActivity, 12f)
            setPadding(dp(16), dp(10), dp(10), dp(10))
            visibility = View.GONE
        }
        suggestText = TextView(this).apply { textSize = 15f; setTextColor(S.TEXT); typeface = S.MEDIUM }
        suggestCard.addView(suggestText, LinearLayout.LayoutParams(0, -2, 1f))
        val yes = TextView(this).apply {
            text = "换这条"; textSize = 15f; typeface = S.BOLD; setTextColor(Color.WHITE); gravity = Gravity.CENTER
            background = S.round(this@NavActivity, S.GREEN, 14f); setPadding(dp(14), dp(8), dp(14), dp(8))
            setOnClickListener {
                val ok = NaviBridge.acceptSuggest()
                if (!ok) Toast.makeText(this@NavActivity, "没能切换路线", Toast.LENGTH_SHORT).show()
            }
        }
        val no = TextView(this).apply {
            text = "不用"; textSize = 15f; setTextColor(S.SUB); gravity = Gravity.CENTER
            setPadding(dp(12), dp(8), dp(8), dp(8))
            setOnClickListener { NaviBridge.clearSuggest() }
        }
        suggestCard.addView(yes); suggestCard.addView(no)
        root.addView(suggestCard, FrameLayout.LayoutParams(-1, -2, Gravity.BOTTOM).apply {
            leftMargin = dp(12); rightMargin = dp(12); bottomMargin = dp(96)
        })
    }

    private fun styleOverview() {
        overviewBtn.text = if (overview) "跟随" else "全览"
        overviewBtn.setTextColor(if (overview) S.PINK else S.CYAN)
    }

    private fun styleMute() {
        muteBtn.text = if (muted) "静音" else "播报"
        muteBtn.setTextColor(if (muted) S.SUB else S.TEXT)
    }

    private fun toggleOverview() {
        overview = !overview
        try { if (overview) naviView.displayOverview() else naviView.recoverLockMode() } catch (_: Throwable) {}
        styleOverview()
    }

    private fun togglePause() {
        paused = !paused
        Refl.call(navi, if (paused) "pauseNavi" else "resumeNavi")
        pauseBtn?.text = if (paused) "继续" else "暂停"
        pauseBtn?.setTextColor(if (paused) S.GREEN else S.AMBER)
    }

    /** 模拟车速：40 → 60 → 100 → 150 km/h 循环 */
    private fun cycleEmuSpeed() {
        emuIdx = (emuIdx + 1) % emuSpeeds.size
        try { navi?.setEmulatorNaviSpeed(emuSpeeds[emuIdx]) } catch (_: Exception) {}
        emuSpeedBtn?.text = "${emuSpeeds[emuIdx]}"
        Toast.makeText(this, "模拟车速 ${emuSpeeds[emuIdx]} km/h", Toast.LENGTH_SHORT).show()
    }

    private fun toggleMute() {
        muted = !muted
        getSharedPreferences("motonav", Context.MODE_PRIVATE).edit().putBoolean("nav_muted", muted).apply()
        val n = navi
        NaviVoice.muted = muted
        NaviVoice.applyTo(n)
        styleMute()
        Toast.makeText(this, if (muted) "语音播报已关闭" else "语音播报已打开", Toast.LENGTH_SHORT).show()
    }

    private fun exitNav() {
        if (exited) return
        exited = true
        NaviBridge.exitNavi(navigationEpoch)
        finish()
    }

    // ================= 每 0.2 秒刷新 =================

    private val refresh = object : Runnable {
        override fun run() {
            update()
            handler.postDelayed(this, 200)
        }
    }

    private fun update() {
        mapStatus.text = MapFeeder.loadingStatus() + "\n" +
            if(CarNavSync.requested)CarNavSync.summary() else if(emulate)
                (if(TrafficLightOverlay.enabled)"模拟导航 · 实时灯读秒不参与模拟" else "模拟导航 · 不记录轨迹") else RideRecorder.status()
        val B = NaviBridge
        // v4.9 更快路线提示
        if (B.uiSuggestMin > 0) {
            suggestText.text = "发现更快的路线，能省约 ${B.uiSuggestMin} 分钟"
            suggestCard.visibility = View.VISIBLE
        } else suggestCard.visibility = View.GONE
        // 到达
        if (B.uiArrived && !exited) {
            if (arrivedAt == 0L) { arrivedAt = SystemClock.elapsedRealtime(); arrivedCard.visibility = View.VISIBLE }
            else if (SystemClock.elapsedRealtime() - arrivedAt > 4000) { exitNav(); return }
        }
        if (B.uiHasInfo) {
            val dir = B.uiDir.ifEmpty { "S" }
            arrow.dir = dir
            arrow.angle = B.uiAngle
            val near = dir != "S" && B.uiDist in 0..100
            if (near != nearShown) {
                nearShown = near
                arrowBox.background = if (near) GradientDrawable(GradientDrawable.Orientation.LEFT_RIGHT,
                    intArrayOf(S.AMBER, Color.parseColor("#FF7A3D"))).apply { cornerRadius = S.dp(this@NavActivity, 18f) }
                else S.gradient(this, 18f)
            }
            val (num, unit) = S.distParts(B.uiDist)
            val sp = SpannableString("$num $unit")
            sp.setSpan(AbsoluteSizeSpan(dp(17)), num.length, sp.length, Spanned.SPAN_EXCLUSIVE_EXCLUSIVE)
            distText.text = sp
            roadText.text = when {
                dir == "A" -> "前方终点 · ${destName.ifBlank { "目的地" }}"
                dir == "S" && B.uiCurRoad.isNotBlank() -> "沿 ${B.uiCurRoad} 直行"
                B.uiNextRoad.isNotBlank() -> "进入 ${B.uiNextRoad}"
                else -> "前方路口"
            }
            remainText.text = "${S.distText(B.uiRemainM)} · ${S.durText(B.uiRemainS)}"
            val cal = Calendar.getInstance().apply { add(Calendar.SECOND, B.uiRemainS) }
            etaText.text = "%02d:%02d 到达".format(cal.get(Calendar.HOUR_OF_DAY), cal.get(Calendar.MINUTE)) +
                (if (B.uiLights > 0) " · ${B.uiLights}个红绿灯" else "") + (if (emulate) " · 模拟" else "")
        } else {
            distText.text = ""
            remainText.text = if (emulate) "模拟导航准备中…" else "等待定位…"
            etaText.text = destName
        }
        // 车道
        val lanes = B.uiLanes
        laneView.lanes = lanes
        laneBox.visibility = if (lanes.isNotEmpty()) View.VISIBLE else View.GONE
        // 车速 + 测速
        val sp = B.uiSpeed
        val lim = B.uiCamLimit
        val over = lim > 0 && sp > lim
        speedNum.text = if (sp >= 0) "$sp" else "--"
        speedNum.setTextColor(if (over) S.RED else S.TEXT)
        (speedBox.background as? GradientDrawable)?.setStroke(dp(2), if (over) S.RED else S.STROKE)
        val ld = B.uiLightDist
        if (ld >= 0) { lightChip.visibility = View.VISIBLE; lightChip.text = "红绿灯\n${(ld + 5) / 10 * 10}米" }
        else lightChip.visibility = View.GONE
        if (lim > 0) {
            camBox.visibility = View.VISIBLE
            camSign.text = "$lim"
            camSign.textSize = if (lim >= 100) 17f else 20f
            camDist.text = "${(B.uiCamDist + 5) / 10 * 10}米"
        } else camBox.visibility = View.GONE
    }

    // ================= 生命周期 =================

    override fun onResume() {
        super.onResume();if(redirectedToSingle)return
        naviView.onResume()
        FusionCarUi.closeConsole(this)
        muted=getSharedPreferences("motonav",Context.MODE_PRIVATE).getBoolean("nav_muted",false)
        NaviVoice.muted=muted
        navi?.let { NaviVoice.applyTo(it) }
        styleMute()
    }
    override fun onPause() { super.onPause(); if(::naviView.isInitialized)naviView.onPause() }
    override fun onSaveInstanceState(outState: Bundle) {
        outState.putBoolean("nav_emulate",emulate);outState.putString("nav_destination",destName)
        super.onSaveInstanceState(outState);if(::naviView.isInitialized)naviView.onSaveInstanceState(outState)
    }
    override fun onDestroy() {
        handler.removeCallbacksAndMessages(null)
        if (!redirectedToSingle && isFinishing && !exited) { exited = true; NaviBridge.exitNavi(navigationEpoch) }
        if(::naviView.isInitialized)naviView.onDestroy()
        super.onDestroy()
    }
}
