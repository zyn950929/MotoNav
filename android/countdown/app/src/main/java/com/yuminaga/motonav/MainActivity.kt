package com.yuminaga.motonav

import android.content.Context
import androidx.appcompat.app.AlertDialog

import android.Manifest
import android.annotation.SuppressLint
import android.content.Intent
import android.content.pm.PackageManager
import android.content.res.ColorStateList
import android.graphics.Color
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.graphics.drawable.RippleDrawable
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.PowerManager
import android.provider.Settings
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.EditText
import android.widget.FrameLayout
import android.widget.ImageView
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.SeekBar
import android.widget.TextView
import android.widget.Toast
import androidx.appcompat.app.AppCompatActivity

/**
 * MotoNav 手机 App
 *
 * 版本记录
 *   v1.0  接入高德导航组件，蓝牙把转向和距离发给圆屏
 *   v1.1  蓝牙高速模式、不等确认、只发最新一条，降低延迟
 *   v1.2  两次高德数据之间按车速推算距离，倒数更平滑
 *   v1.3  后台常驻服务（锁屏继续工作）、自动连接与重连、心跳、电池优化设置
 *         （配套圆屏固件 v1.3）
 *   v1.3.1 修复编译错误（后台服务启动方式改用系统自带接口）
 *   v1.4  离路口 500 米以外显示直行箭头，500 米以内才显示转向箭头
 *   v1.5  改为始终发送真实转向，远/中/近三阶段画面交给圆屏固件 v1.5 处理
 *   v1.6  1 公里以内距离精确到 1 米，配合圆屏固件 v1.6 让进度环连续前进
 *   v1.6.1 “退出”按钮改为长按才生效，防止误触
 *   v1.7  车速、电子眼限速、车道、中文路名和底部信息（剩余里程/到达时间/红绿灯数）、
 *         圆屏亮度自动（日出日落）+ 手动调节（配套圆屏固件 v1.7）
 *   v1.7.1 修复车道数量不对、测速牌不显示；路名/底部文字缩窄（配套圆屏固件 v1.7.1）；
 *          显示电子眼原始数据
 *   v1.8  转弯箭头角度跟随路口实际角度（锐角弯、缓弯），配套圆屏固件 v1.8
 *   v1.8.1 距离推算上限从 3 秒缩短到 1.2 秒，急刹停车时距离更准
 *         圆屏画成车头朝上的小地图（配套圆屏固件 v2.0）；可在这里关闭
 *   v2.0.1 显示圆屏小地图的帧率（配套圆屏固件 v2.2）
 *   v2.1  可切换圆屏数据线速度 40/60/80MHz，减轻画面撕裂（配套圆屏固件 v2.3）
 *   v2.2  测速提醒提前：按车速留约 20 秒（500~1000 米）；配套圆屏固件 v2.4（远处先显示直行）
 *         显示高德原图和归类后的预览；坐标原点固定（配套圆屏固件 v2.5）
 *   v2.3.1 修复高德返回“权限类型错误”（请求里去掉浏览器标识）
 *   v2.3.2 高德画面更干净：不再把深色细线当道路，去掉小碎块，边缘平滑
 *   v2.4  圆屏“快速刷屏”开关；帧率统计显示更详细（配套圆屏固件 v2.6）
 *   v2.5  小地图配色主题：罪恶都市 / GTA5 / 原版（配套圆屏固件 v2.9）
 *   v2.6  界面重新设计：罪恶都市风深色界面（霓虹粉 + 青色），状态卡片、分段选择按钮、
 *         准备工作完成后自动隐藏，调试信息收进可展开的“调试”区域。功能和以前一样。
 *   v2.7  新 App 图标：霓虹圆屏小地图 + 白色箭头 + 粉色路线（自适应图标，支持安卓 13 主题图标）
 *   v2.8  小地图夜间配色（自动按日出日落 / 固定白天 / 固定夜间）；走过的路线变灰或擦除
 *         （配套圆屏固件 v3.0）
 *   v3.0  自己做的导航界面，代替高德自带组件：搜索页（联想提示、最近去过）、选路线页
 *         （多条路线对比、默认不走高速、模拟导航）、导航页（霓虹风转向卡片、车道、车速、测速、
 *         剩余里程），按 1080×1200 方屏排版；高德原版界面留在调试区备用
 *   v3.1  圆屏小地图底部显示 到达时间 · 剩余里程 · 红绿灯数；前方 300 米内的红绿灯显示距离
 *         （圆屏和手机导航页都有，配套圆屏固件 v3.1）
 *   v3.2  模拟导航补回暂停/继续按钮，并可切换模拟车速
 *   v3.3  红绿灯直接画在圆屏小地图上（随路线一起发送位置），配套圆屏固件 v3.2
 *   v3.4  电子眼（测速点、拍照点）也画在圆屏小地图上，配套圆屏固件 v3.5
 *   v3.5  调试区新增「高德探测」：记录高德手机版的导航通知、界面文字和车机版广播，
 *         用来评估改用高德官方 App 导航（摩托车模式、换语音）的可行性
 *   v3.6  修复：调试区按钮文字被切掉一半；页面滚动时内容钻到手机状态栏下面
 *   v3.7  高德探测：修复通知界面解析失败（图片绘制出错时不再丢掉文字），记录通知样式、
 *         界面上没有文字的控件名和悬浮窗
 *   v3.8  导航语音可选：高德自带 / 手机系统语音（选语音引擎、选声音、调语速音调、试听）
 *   v3.9  修复：语音引擎列表里只看得到 Google 的（清单里补上查看语音引擎的声明）
 *   v4.0  内置 AI 语音（开源 sherpa-onnx）—— v4.1 已撤掉（App 太大）
 *   v4.1  导航语音里加“免费好听的语音”推荐（Google 语音合成、VoxSherpa AI 语音），一键去安装，装好后直接选
 *   v4.2  电子眼按类型（测速、闯红灯、违章拍照、公交道……）换算成图标编号发给圆屏，小地图上画不同的图标（配合固件 v3.8）
 *   v4.3  停车时沿用最后的行驶朝向发给圆屏，小地图不再乱转（配合固件 v4.0：停车/暂停时保持地图画面）
 *   v4.4  导入高德 App 分享的路线（比如摩托车路线）：读出原路线，沿路挑途经点让高德导航照着走，并显示吻合程度
 *   v4.4.1 修复导入分享路线时报“Cleartext HTTP traffic not permitted”（高德跳转给的 http 地址改用 https）
 *   v4.4.2 导入分享路线支持高德摩托车路线；修复手机上卡在高德跳转页（callAPP）读不出路线
 *   v4.4.3 导入失败时显示 App 版本和完整网址，文字可以长按复制
 *   v4.4.4 修复手机上读不出分享路线：从高德跳转页的 shorturl&r= 参数里取路线
 *   v4.5  按分享路线导航时：偏航后只用前方的途经点重新规划（不会被拽回去补点）；原路线画成橙色细线
 *         显示在导航页和圆屏上（配合固件 v4.4）
 *   v4.6  蓝牙请求 2M 模式（大块数据传得更快，配合固件 v4.5）；调试区显示蓝牙模式和实测传输速度
 *   v4.7  巡航模式（首页“巡航模式”按钮，不设目的地也显示小地图、车速、电子眼、限速）；道路安全提示
 *         （急弯、学校、事故多发、注意行人、陡坡等）在圆屏上用图标显示（配合固件 v4.8）
 *   v4.8  清爽模式（小地图设置里的开关，配合固件 v4.9）
 *   v4.8.1 清爽模式改为：小地图保留，只去掉红绿灯、电子眼、安全提示、测速牌和行程信息
 *   v4.9  圆屏显示“重新规划中”、GPS 信号弱、路线路况颜色；途中有更快路线时导航页弹出“换这条”；
 *         选路线页可以加最多 3 个途经点（配合固件 v5.0）
 *   v5.0  来电显示：来电时圆屏整屏显示名字和号码，接听或挂断后回到地图（小地图设置里的开关，配合固件 v5.1）
 *   v5.1  播报时压低音乐：像车机一样，导航播报时音乐自动变小（或暂停），播完恢复（导航语音卡片里选）
 *   v5.3  圆屏触屏接电话：来电时按住圆屏 0.8 秒接听（配合固件 v5.2，需要“接听电话”权限）
 *   v5.4  圆屏天气页：在圆屏上左右滑看天气（高德天气，用导航的 key；配合固件 v5.3）
 *   v5.5  修复：滑到天气页 App 闪退（图片栏编号超出）；新增闪退记录，下次打开在调试信息最上面显示
 *   v5.6  天气页下面一排改成接下来 5 个小时的天气、温度、降雨概率（Open-Meteo，免费不用 key；配合固件 v5.6）
 *   v5.7  调试信息里显示小地图底图用时（配合固件 v5.7 的两核心一起画）
 *   v5.8  调试信息里显示小地图每一步各花多少毫秒（配合固件 v5.8）
 *   v5.9  小地图新增 8 种风格（赛博霓虹、复古纸地图、极简黑白、蓝图、掌机像素、雷达、寻宝罗盘、卫星图）
 *         和“弯道急缓上色”开关（配合固件 v6.1）
 *   v5.9.1 卫星图风格不再受“地图来源”影响，选了就用高德卫星照片
 *   v6.0  风格选择改成全屏卡片：每种风格一张圆屏效果预览图（res/drawable-nodpi/theme_0~10.png），
 *         点一下就换，当前用的那张粉色描边；设置里“风格”一行也显示当前风格的小预览
 *   v6.1  圆屏方向：整个画面转 0°/90°/180°/270°，装车时线朝哪边都行（配合固件 v6.2）；
 *         修复弯道上色开关不起作用（改用 "#Q"）
 *   v6.2  高德 key 改成在 App 里填（首页「Key 设置」，第一次打开自动弹出），保存在手机上；
 *         地图、定位、搜索、导航都用自己填的高德 key。local.properties 不填也能编译（填了就当默认值）。
 *         填 key 的页面直接显示本 App 的包名和 SHA1，点一下复制，方便去高德控制台申请 Android key
 *   v6.3  和 iOS 版一样：搜索目的地直接放在首页（搜索框 + 结果 + 粘贴高德分享路线 + 最近去过），
 *         选好就进选路线页直接规划；巡航模式单独一张卡片。
 *         圆屏方向改成圆盘：按住转动，任意角度，圆屏实时跟着转（配合固件 v6.4 的 "#D角度"；旧固件转到最近的 90°）
 *   v6.4  蓝牙配对验证（配合固件 v6.5）：第一次连接时输入圆屏上的 6 位配对码，之后自动连、全程加密，
 *         别人的手机发不了指令；首页圆屏状态显示配对提示 / “已验证”；圆屏设置里可以“全部忘掉”配对
 *         第一次打开、或打开 App 时还有 key 没填，自动弹出填写页
 *   v6.6  修：有的手机（小米等）上播报时音乐压不住——音乐 App 不理会系统的“压低”请求。
 *         “暂停”模式下，打开「通知使用权」后直接对正在放的音乐按暂停，播完自动接着放；界面上给出提示和设置入口
 *   v6.7  修：正式版 APK 一打开就闪退（打包时的代码优化把 Kotlin 自带的类改坏了）→ build.gradle.kts 里关掉优化
 *   v6.8  搜索改成打完字点“搜索”（或按回车）才搜，不再每打一个字就搜（首页和选路线页都改了）；
 *         填 key 页面按安装包类型显示“调试版 / 发布版安全码 SHA1”，告诉你填到高德哪一格
 *   v6.9  修：点“全部忘掉”后再也连不上（圆屏忘了、手机还记着旧配对）。按钮改成“忘掉其他手机”（配合固件 v6.6，
 *         保留正在用的这台）；检测到旧配对对不上时自动清掉手机上的旧配对，清不掉就提示并一点打开系统蓝牙设置
 *   v7.0  导航语音卡片新增「播报内容：详细 / 简洁」（高德的详细播报 / 简洁播报）
 *   v7.1  安装包瘦身（build.gradle.kts）：只打包 64 位手机的高德底层库，底层库压缩存放
 *   v7.2  减少“慢半拍”：小地图卡片新增「预判」关 / 0.3 秒 / 0.6 秒（默认 0.3），路口距离和圆屏小地图都提前一点显示；
 *         距离推算从每秒 5 次改成 10 次（配合固件 v6.7 的转弯预判）
 *   v7.3  修：换了签名（高德 key 的 SHA1 对不上）后圆屏天气页出不来。高德定位 / 高德天气失败时，
 *         改用手机自己的定位 + Open-Meteo 实况，天气照样显示；调试信息里写明高德失败的原因
 *   v7.4  圆屏方向圆盘：只有按在圆盘上才转动，圆盘外面的空白处照常上下滑动页面
 *   v7.5  修：卸载重装后来电显示悄悄失效（开关默认开着，但电话权限只在拨开关时才要）。
 *         现在「授予权限」和打开 App 时都会一起要电话权限；没给全时开关下面黄字提示，点一下去授权
 *   v7.6  首页圆屏状态显示圆屏电量（配合固件 v6.8，接了电池才有），电量低于 15% 时状态点变黄
 *   v7.7  来电接听提示改成“按住圆屏 0.2 秒接听”（配合固件 v6.9）
 *   v7.8  认圆屏：首页显示连着的是哪块（"MotoNav-3A7F"，配合固件 v7.0），App 优先连上次那块；
 *         圆屏设置里加「换一块」，身边有几块圆屏时用
 *   v7.9  到达特效：到达时把这趟的里程、用时、最高车速发给圆屏（配合固件 v7.1 的庆祝动画）；到达自己加的途经点时圆屏放短特效
 *   v8.0  路程进度条开关（圆屏左边沿一条细弧，走了多少一目了然；配合固件 v7.3）
 *   v8.1  路程进度改成最外圈绿环（走过的变灰，转弯时让位给倒计时环；配合固件 v7.4），只改了说明文字
 *   v8.2  界面整理：首页顶部一张状态卡（圆屏名字、电量、配对提示，导航 / 巡航中多一行）；巡航改成一行；
 *         设置分成「圆屏显示 / 导航语音 / 圆屏设置 / 地图 Key」四组，可折叠，收起时显示一行当前设置；
 *         调试信息、测试按钮、屏幕速度、地图来源、帧率等挪进「开发者选项」，默认隐藏（连点 5 下版本号打开）
 */
const val APP_VERSION = "v8.3.26-single.32-fresh.1"

class MainActivity : AppCompatActivity() {
    // ---------------- 配色（罪恶都市夜景：深紫黑底、霓虹粉、青色） ----------------
    private val BG = Color.parseColor("#0C0A11")
    private val CARD = Color.parseColor("#16131D")
    private val STROKE = Color.parseColor("#262131")
    private val TEXT = Color.parseColor("#F5F2F9")
    private val SUB = Color.parseColor("#958EA4")
    private val PINK = Color.parseColor("#FF4FB4")
    private val PURPLE = Color.parseColor("#8E5CFF")
    private val CYAN = Color.parseColor("#4BDDEB")
    private val GREEN = Color.parseColor("#3DDC84")
    private val AMBER = Color.parseColor("#FFB547")
    private val RED = Color.parseColor("#FF5A6A")

    private val handler = Handler(Looper.getMainLooper())
    private var uiResumed=false
    private val refreshTask=Runnable { refresh() }
    private val refreshers = ArrayList<() -> Unit>()       // 每 0.2 秒刷新一次的界面元素

    private lateinit var pvRaw: ImageView
    private lateinit var pvCls: ImageView
    private var shownPv = -1

    private fun dp(v: Int) = (v * resources.displayMetrics.density).toInt()
    private fun dpf(v: Float) = v * resources.displayMetrics.density

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        CrashLog.install(this)                       // v5.5 闪退记录
        NaviBridge.initPrivacy(this)
        MapFeeder.load(this)
        TrafficLightOverlay.load(this)
        CallWatcher.load(this)                       // v5.0 来电显示开关
        Weather.init(this)                           // v5.4 天气
        Brightness.load(this)
        NaviVoice.load(this)                           // v3.8 导航语音设置

        window.statusBarColor = BG
        window.navigationBarColor = BG
        supportActionBar?.hide()                       // 不要系统自带的标题栏

        buildMainPages(savedInstanceState)

        // 权限已经给过：打开 App 就自动启动后台服务并连接圆屏
        if (hasPermissions()) NavService.start(this)
        refresh()
        // v7.5 来电显示开着、却没有电话权限（比如卸载重装后）：打开 App 时顺便要一下，不然来电显示悄悄失效
        if (savedInstanceState == null && !FusionFirstUse.shouldOpen(this) && hasPermissions() && CallWatcher.enabled &&
            checkSelfPermission(Manifest.permission.READ_PHONE_STATE) != PackageManager.PERMISSION_GRANTED)
            handler.postDelayed({ requestPermissions(CallWatcher.PERMS, 7) }, 800)
        // v6.2 还没填高德 key：第一次打开直接弹出填写页
        // v6.5 打开 App 时还有 key 没填（第一次打开时两个都没填）：直接弹出填写页
        val returningFromSetup=FusionFirstUse.returning(this)
        if (savedInstanceState == null) handler.post {
            if(FusionFirstUse.shouldOpen(this) && !NavigationUi.active())openProviderSetup()
            else if(!returningFromSetup && !Keys.allFilled(this))showKeyDialog(first=true)
        }
    }

    private lateinit var mainPages: MainPagesShell
    private val pageRefreshers = Array(5) { ArrayList<() -> Unit>() }

    private fun buildMainPages(saved: Bundle?) {
        mainPages = MainPagesShell(this, saved, { index -> buildMainPage(index) }) { index ->
            hideKeyboard()
            pageRefreshers[index].forEach { it() }
            if (index == 0 && searchInput?.text.isNullOrBlank()) refreshHomeIdle?.invoke()
        }
        mainPages.addView(activeNavigationCard(),0,LinearLayout.LayoutParams(-1,-2).apply {
            setMargins(dp(16),dp(8),dp(16),0)
        })
        setContentView(mainPages)
        mainPages.showPage(saved?.getInt("main_page_selected", 0)
            ?: intent.getIntExtra(NavigationUi.EXTRA_SETTINGS_PAGE,0).coerceIn(0,4))
        onBackPressedDispatcher.addCallback(this, object : androidx.activity.OnBackPressedCallback(true) {
            override fun handleOnBackPressed() {
                if (mainPages.selectedPage != 0) mainPages.showPage(0)
                else { isEnabled = false; onBackPressedDispatcher.onBackPressed(); isEnabled = true }
            }
        })
    }

    /** Build each page once so switching tabs keeps search results, controls and scroll position. */
    private fun buildMainPage(index: Int): View {
        val firstCallback = refreshers.size
        val content = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(16), dp(16), dp(16), dp(24))
        }
        val titles = listOf("首页搜索", "圆屏显示", "导航语音", "圆屏设置", "地图 Key")
        content.addView(pageHeader(if (index == 0) "MotoNav" else titles[index]))
        fun settingsBody(build: (LinearLayout) -> Unit) {
            val box = card(); build(box); content.addView(wrapMargin(box))
        }
        when (index) {
            0 -> {
                content.addView(homeConnection())
                content.addView(setupCard())
                content.addView(searchCard())
                content.addView(cruiseCard())
                content.addView(rideCard())
            }
            1 -> {
                val summary = label("", 13f, SUB).apply { setPadding(dp(4), 0, 0, dp(14)) }
                content.addView(summary); refreshers += { summary.text = mapSummary() }
                settingsBody { mapSection(it) }
            }
            2 -> {
                val summary = label("", 13f, SUB).apply { setPadding(dp(4), 0, 0, dp(14)) }
                content.addView(summary); refreshers += { summary.text = voiceSummary() }
                settingsBody { voiceSection(it) }
            }
            3 -> {
                content.addView(statusCard())
                settingsBody { screenSection(it) }
                devWrap = section("dev", "🛠", "开发者选项", { "调试信息、测试按钮、屏幕速度、地图来源" }) { devSection(it) }.apply {
                    visibility = if (uiPrefs.getBoolean("dev_mode", false)) View.VISIBLE else View.GONE
                }
                content.addView(devWrap)
                content.addView(exitButton())
                // 版本号放在设置页最底下；连点 5 下打开 / 关闭开发者选项
                content.addView(label("MotoNav " + APP_VERSION.substringBefore("-"), 11f, SUB).apply {
                    gravity = Gravity.CENTER; setPadding(0, dp(16), 0, dp(4)); setOnClickListener { onVersionTap() }
                })
            }
            4 -> settingsBody { keySection(it) }
        }
        val callbacks = refreshers.subList(firstCallback, refreshers.size)
        pageRefreshers[index].addAll(callbacks.toList()); callbacks.clear()
        addPressFeedback(content)
        return content
    }

    private fun activeNavigationCard(): View {
        val box=card()
        box.addView(label("返回导航 ›",16f,CYAN,bold=true))
        box.addView(label("导航继续运行，可在这里调整设置",12f,SUB).apply { setPadding(0,dp(4),0,0) })
        box.contentDescription="返回正在进行的导航"
        box.setOnClickListener {
            if(!NavigationUi.returnToNavigation(this))toast("当前没有正在进行的导航")
        }
        addPressFeedback(box)
        val wrap=wrapMargin(box)
        refreshers += { wrap.visibility=if(NavigationUi.active())View.VISIBLE else View.GONE }
        wrap.visibility=if(NavigationUi.active())View.VISIBLE else View.GONE
        return wrap
    }

    private fun pageHeader(title: String): View = LinearLayout(this).apply {
        orientation = LinearLayout.VERTICAL
        setPadding(dp(4), dp(4), dp(4), dp(16))
        val home = title == "MotoNav"
        addView(label(title, if (home) 30f else 26f, if (home) PINK else TEXT, bold = true).apply {
            if (home) {
                typeface = Typeface.create(Typeface.DEFAULT, Typeface.BOLD_ITALIC)
                letterSpacing = 0.02f
                setShadowLayer(dpf(4f), 0f, 0f, Color.argb(110, 255, 79, 180))
            }
        })
        if (home) addView(label("圆屏摩托导航", 13f, SUB).apply {
            setPadding(dp(2), dp(4), 0, 0); letterSpacing = 0.06f; setOnClickListener { onVersionTap() }
        })
    }

    private fun homeConnection(): View {
        // 一枚胶囊：连接状态点 + 文字，点一下去圆屏设置页
        val row = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(14), 0, dp(14), 0); minimumHeight = dp(44)
            background = GradientDrawable().apply { setColor(CARD); cornerRadius = dpf(22f); setStroke(dp(1), STROKE) }
        }
        val dot = View(this).apply {
            background = GradientDrawable().apply { shape = GradientDrawable.OVAL; setColor(SUB) }
        }
        row.addView(dot, LinearLayout.LayoutParams(dp(8), dp(8)).apply { rightMargin = dp(10) })
        val state = label("", 13f, TEXT).apply { isSingleLine = true; ellipsize = android.text.TextUtils.TruncateAt.END }
        row.addView(state, LinearLayout.LayoutParams(0, -2, 1f))
        row.addView(label("›", 18f, SUB))
        row.setOnClickListener { mainPages.showPage(3) }
        refreshers += {
            val ready = BleLink.isConnected && BleLink.verified
            setDot(dot, if (ready) GREEN else AMBER)
            state.text = if (ready) BleLink.screenName.ifBlank { "圆屏" } + " 已连接" +
                (if (BleLink.screenBatteryPct >= 0) " · ${BleLink.screenBatteryPct}%" else "")
                else if (BleLink.isConnected) "圆屏正在验证配对" else "圆屏未连接"
        }
        return wrapMargin(row)
    }

    override fun onSaveInstanceState(outState: Bundle) {
        if (::mainPages.isInitialized) mainPages.save(outState)
        super.onSaveInstanceState(outState)
    }


    // ================= 页面各部分 =================

    /** 准备工作（权限、电池）：都完成后自动隐藏 */
    private fun setupCard(): View {
        val card = card()
        card.addView(label("出发前准备", 15f, TEXT, bold = true))
        card.addView(label("完成以下准备后，锁屏也能稳定给圆屏发数据", 12f, SUB).apply { setPadding(0, dp(2), 0, dp(10)) })
        val permBtn = pillButton("授予权限", PURPLE) { requestAllPermissions() }
        val batBtn = pillButton("电池设为「不限制」", PURPLE) { requestIgnoreBatteryOptimization() }
        val providerBtn=pillButton("完成首次车机授权", PURPLE) { openProviderSetup() }
        card.addView(providerBtn,lpMatch(bottom=8))
        card.addView(permBtn, lpMatch(bottom = 8))
        card.addView(batBtn, lpMatch())
        refreshers += {
            val p = hasPermissions(); val b = isIgnoringBatteryOptimization()
            permBtn.visibility = if (p) View.GONE else View.VISIBLE
            batBtn.visibility = if (b) View.GONE else View.VISIBLE
            val ready=FusionFirstUse.complete(this)
            providerBtn.visibility=if(ready)View.GONE else View.VISIBLE
            card.visibility = if (p && b && ready) View.GONE else View.VISIBLE
        }
        return wrapMargin(card)
    }

    // ================= v6.3 首页搜索目的地 =================

    private var directRouteMode = LocalRouting.DRIVE
    private var searchInput: EditText? = null
    private var homeKw = ""
    private val homeSearchRun = Runnable { homeSearch(homeKw) }
    private var refreshHomeIdle: (() -> Unit)? = null

    /** 去哪儿：搜索框 + 搜索按钮，下面是搜索结果；没输入时显示“粘贴高德分享的路线”和“最近去过” */
    private fun searchCard(): View {
        val outer = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        // 摩托车路线规划：首页最主要的入口，单独一张渐变描边卡片
        val motorEntry = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL
            setPadding(dp(18), dp(16), dp(16), dp(16))
            background = GradientDrawable(GradientDrawable.Orientation.TL_BR, intArrayOf(0xff2c1631.toInt(), 0xff17202f.toInt())).apply {
                cornerRadius = dpf(20f); setStroke(dp(1), Color.argb(150, 255, 79, 180))
            }
            val col = LinearLayout(this@MainActivity).apply { orientation = LinearLayout.VERTICAL }
            col.addView(label("摩托车路线规划", 19f, TEXT, bold = true))
            col.addView(label("选起终点 · 对比路线 · 常用地点", 12f, SUB).apply { setPadding(0, dp(4), 0, 0) })
            addView(col, LinearLayout.LayoutParams(0, -2, 1f))
            addView(label("›", 26f, PINK))
            contentDescription = "摩托车路线规划，选择起终点并对比路线"
            setOnClickListener { startActivity(Intent(this@MainActivity, MotorWebPlannerActivity::class.java)) }
        }
        outer.addView(wrapMargin(motorEntry))

        val card = card()
        card.addView(label("去哪儿", 18f, TEXT, bold = true).apply { setPadding(0, 0, 0, dp(12)) })
        val row = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL }
        val input = EditText(this).apply {
            hint = "搜索地点、地址"
            textSize = 16f
            setTextColor(TEXT)
            setHintTextColor(SUB)
            setSingleLine(true)
            imeOptions = android.view.inputmethod.EditorInfo.IME_ACTION_SEARCH
            setPadding(dp(16), 0, dp(16), 0)
            background = GradientDrawable().apply { setColor(BG); cornerRadius = dpf(14f); setStroke(dp(1), STROKE) }
        }
        searchInput = input
        val go = pillButton("搜索", PINK) { homeSearchNow() }.apply { textSize = 16f }
        row.addView(input, LinearLayout.LayoutParams(0, dp(50), 1f))
        row.addView(go, LinearLayout.LayoutParams(dp(80), dp(50)).apply { leftMargin = dp(10) })
        card.addView(row)
        directRouteMode = LocalRouting.normalize(getSharedPreferences("motonav", MODE_PRIVATE).getInt("direct_route_mode", LocalRouting.DRIVE))
        card.addView(segmented(listOf(LocalRouting.DRIVE to "驾车", LocalRouting.BICYCLE to "自行车"), { directRouteMode }) {
            directRouteMode = it
            getSharedPreferences("motonav", MODE_PRIVATE).edit().putInt("direct_route_mode", it).apply()
        }, lpMatch(top = 10))

        val results = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        card.addView(results, lpMatch(top = 6))

        // 常用目的地、最近去过、导入分享路线（没输入时显示）
        val idle = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        val hist = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        idle.addView(hist)
        idle.addView(textLink("导入高德分享路线 ›") { showShareImport() }, LinearLayout.LayoutParams(-2, -2).apply { topMargin = dp(16) })
        card.addView(idle)

        fun listHeader(title: String, action: String, onAction: () -> Unit) = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL; setPadding(0, dp(16), 0, dp(2))
            addView(label(title, 14f, SUB, bold = true), LinearLayout.LayoutParams(0, -2, 1f))
            addView(textLink(action) { onAction() })
        }
        fun showHistory() {
            hist.removeAllViews()
            val favorites = FavoritePlaces.load(this)
            hist.addView(listHeader("常用目的地", "管理 ${favorites.size}/50") {
                FavoritePlacesUi.manage(this@MainActivity, { showHistory() }) { p -> openPlan(p.name,p.addr,p.lat,p.lng) }
            })
            if (favorites.isEmpty()) hist.addView(label("在搜索结果里点「收藏」添加", 12f, SUB).apply { setPadding(0, dp(4), 0, 0) })
            favorites.take(4).forEach { p ->
                hist.addView(placeRow(p.name, p.addr) { openPlan(p.name, p.addr, p.lat, p.lng) })
            }
            val h = NavHistory.load(this).take(5)
            if (h.isNotEmpty()) {
                hist.addView(listHeader("最近去过", "清除") { NavHistory.clear(this@MainActivity); showHistory() })
                for (it in h) hist.addView(placeRow(it.name, it.addr) { openPlan(it.name, it.addr, it.lat, it.lng) })
            }
            addPressFeedback(hist)
        }
        refreshHomeIdle = { showHistory() }
        showHistory()

        homeResults = results
        homeIdle = idle
        input.addTextChangedListener(object : android.text.TextWatcher {
            override fun beforeTextChanged(s: CharSequence?, a: Int, b: Int, c: Int) {}
            override fun onTextChanged(s: CharSequence?, a: Int, b: Int, c: Int) {}
            override fun afterTextChanged(e: android.text.Editable?) {
                homeKw = e?.toString()?.trim().orEmpty()
                handler.removeCallbacks(homeSearchRun)
                // v6.8 不再边打边搜：打完点“搜索”或按回车才搜；清空输入框时回到“最近去过”
                if (homeKw.isEmpty()) { results.removeAllViews(); homeTips = emptyList(); homeTipsKw = ""; idle.visibility = View.VISIBLE }
            }
        })
        input.setOnEditorActionListener { _, _, _ ->
            // 回车：这串字已经搜过、有结果 → 选第一个；没搜过 → 现在搜
            val t = homeTips.firstOrNull()
            if (t != null && homeTipsKw == homeKw) pickTip(t) else homeSearchNow()
            true
        }
        outer.addView(wrapMargin(card))
        return outer
    }

    private fun showShareImport(){
        val input=EditText(this).apply {hint="粘贴高德路线分享链接";minLines=2;maxLines=5;setTextColor(TEXT);setHintTextColor(SUB);setPadding(dp(16),dp(12),dp(16),dp(12))}
        val clip=runCatching{(getSystemService(Context.CLIPBOARD_SERVICE) as android.content.ClipboardManager).primaryClip?.getItemAt(0)?.coerceToText(this)?.toString()}.getOrNull()
        if(AmapShare.findUrl(clip)!=null && clip!=null && clip.length<=20000)input.setText(clip)
        val dialog=AlertDialog.Builder(this).setTitle("导入高德分享路线").setView(input).setPositiveButton("读取路线",null).setNegativeButton("取消",null).create()
        dialog.setOnShowListener{dialog.getButton(AlertDialog.BUTTON_POSITIVE).setOnClickListener{
            val raw=input.text.toString().trim()
            if(raw.length>20000 || AmapShare.findUrl(raw)==null){input.error="请粘贴有效的高德路线分享链接";return@setOnClickListener}
            startActivity(Intent(this,RoutePlanActivity::class.java).putExtra(RoutePlanActivity.EXTRA_SHARE,raw)
                .putExtra(SingleEngineDestination.EXTRA_ID,java.util.UUID.randomUUID().toString()))
            dialog.dismiss()
        }}
        dialog.show()
    }

    private var homeResults: LinearLayout? = null
    private var homeIdle: View? = null
    private var homeTips: List<com.amap.api.services.help.Tip> = emptyList()
    private var homeTipsKw = ""

    /** 一行地点：名称 + 地址，点了去选路线 */
    private fun placeRow(name: String, addr: String, save: (() -> Unit)? = null, onClick: () -> Unit): View {
        val box = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(0, dp(10), 0, dp(10))
            setOnClickListener { onClick() }
        }
        box.addView(label(name, 16f, TEXT).apply { isSingleLine = true; ellipsize = android.text.TextUtils.TruncateAt.END })
        if (addr.isNotBlank()) box.addView(label(addr, 12f, SUB).apply {
            isSingleLine = true; ellipsize = android.text.TextUtils.TruncateAt.END; setPadding(0, dp(2), 0, 0)
        })
        val wrap = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        val row = LinearLayout(this).apply { gravity = Gravity.CENTER_VERTICAL }
        row.addView(box, LinearLayout.LayoutParams(0, -2, 1f))
        // 右侧：有收藏按钮就显示按钮，否则显示 › 提示整行可点
        if (save != null) row.addView(textLink("收藏") { save() }, LinearLayout.LayoutParams(-2, -2).apply { leftMargin = dp(8) })
        else row.addView(label("›", 20f, SUB).apply { setPadding(dp(8), 0, dp(2), 0) })
        wrap.addView(row)
        wrap.addView(divider(top = 0))
        return wrap
    }

    private var lastHomeGoAt = 0L
    private fun homeSearchNow() {
        val now = android.os.SystemClock.elapsedRealtime()
        if (now - lastHomeGoAt < 300) return            // 软键盘和实体键盘的回车可能各来一次
        lastHomeGoAt = now
        handler.removeCallbacks(homeSearchRun)
        homeKw = searchInput?.text?.toString()?.trim().orEmpty()
        if (homeKw.isEmpty()) { toast("先输入要去的地方"); return }
        hideKeyboard()
        homeIdle?.visibility = View.GONE
        homeResults?.let { it.removeAllViews(); it.addView(label("搜索中…", 13f, SUB).apply { setPadding(0, dp(10), 0, 0) }) }
        homeSearch(homeKw)
    }

    /** 高德输入提示：附近优先 */
    private fun homeSearch(kw: String) {
        if (kw.isEmpty()) return
        if (!Keys.hasAmap(this)) { showKeyDialog(first = true); return }
        val results = homeResults ?: return
        try {
            val q = com.amap.api.services.help.InputtipsQuery(kw, "")
            q.setCityLimit(false)
            lastKnownLocation()?.let { q.setLocation(com.amap.api.services.core.LatLonPoint(it.first, it.second)) }
            val tips = com.amap.api.services.help.Inputtips(this, q)
            tips.setInputtipsListener(object : com.amap.api.services.help.Inputtips.InputtipsListener {
                override fun onGetInputtips(list: MutableList<com.amap.api.services.help.Tip>?, code: Int) {
                    if (homeKw != kw) return                         // 已经又改了字，这次的结果不要了
                    results.removeAllViews()
                    if (code != 1000) { results.addView(label("搜索失败（错误码 $code）：检查网络和高德 key", 13f, RED).apply { setPadding(0, dp(10), 0, 0) }); return }
                    val l = (list ?: emptyList()).filter { it.point != null }
                    homeTips = l; homeTipsKw = kw
                    if (l.isEmpty()) { results.addView(label("没找到，换个说法试试", 13f, SUB).apply { setPadding(0, dp(10), 0, 0) }); return }
                    for (t in l.take(8)) {
                        val sub = listOf(t.district ?: "", t.address ?: "").filter { it.isNotBlank() }.joinToString(" ")
                        results.addView(placeRow(t.name ?: "", sub, save = {
                            val p = t.point
                            if (p != null) FavoritePlacesUi.save(this@MainActivity,t.name ?: "目的地",sub,p.latitude,p.longitude) { refreshHomeIdle?.invoke() }
                        }) { pickTip(t) })
                    }
                    addPressFeedback(results)
                }
            })
            tips.requestInputtipsAsyn()
        } catch (e: Exception) {
            results.removeAllViews()
            results.addView(label("搜索出错：${e.message}", 13f, RED))
        }
    }

    private fun pickTip(t: com.amap.api.services.help.Tip) {
        val p = t.point ?: return
        val sub = listOf(t.district ?: "", t.address ?: "").filter { it.isNotBlank() }.joinToString(" ")
        openPlan(t.name ?: "目的地", sub, p.latitude, p.longitude)
    }

    /** 选好目的地：打开选路线页，直接开始规划（不再经过搜索页） */
    private fun openPlan(name: String, addr: String, lat: Double, lng: Double) {
        if (!Keys.hasAmap(this)) { showKeyDialog(first = true); return }
        hideKeyboard()
        if(SingleEngineSession.EXPERIMENT && directRouteMode==LocalRouting.DRIVE){
            startActivity(SingleEngineDestination.intent(this,name,addr,lat,lng))
            return
        }
        if(SingleEngineSession.owns)SingleEngineSession.disarm()
        startActivity(Intent(this, RoutePlanActivity::class.java)
            .putExtra(RoutePlanActivity.EXTRA_FROM_HOME, true)
            .putExtra(RoutePlanActivity.EXTRA_MODE, directRouteMode)
            .putExtra(RoutePlanActivity.EXTRA_NAME, name)
            .putExtra(RoutePlanActivity.EXTRA_ADDR, addr)
            .putExtra(RoutePlanActivity.EXTRA_LAT, lat)
            .putExtra(RoutePlanActivity.EXTRA_LNG, lng))
    }

    /** 手机最近一次的位置（只用来让搜索结果附近优先，不准也没关系） */
    @SuppressLint("MissingPermission")
    private fun lastKnownLocation(): Pair<Double, Double>? = try {
        val lm = getSystemService(LOCATION_SERVICE) as android.location.LocationManager
        lm.getProviders(true).mapNotNull { lm.getLastKnownLocation(it) }.maxByOrNull { it.time }?.let { it.latitude to it.longitude }
    } catch (_: Exception) { null }

    private fun hideKeyboard() {
        val v = searchInput ?: return
        v.clearFocus()
        (getSystemService(INPUT_METHOD_SERVICE) as android.view.inputmethod.InputMethodManager).hideSoftInputFromWindow(v.windowToken, 0)
    }

    /** v8.2 巡航模式：一行说明 + 右边按钮（不设目的地，只提醒测速和危险路段，圆屏照常显示小地图） */
    private fun cruiseCard(): View {
        val card = card()
        val row = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL }
        val col = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        col.addView(label("巡航模式", 16f, TEXT, bold = true))
        col.addView(label("不设目的地，提醒测速和危险路段；静止 10 分钟自动结束", 12f, SUB).apply { setPadding(0, dp(4), dp(10), 0) })
        row.addView(col, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        val cruise = pillButton("开始", CYAN) {
            if (NaviBridge.cruising) {
                NaviBridge.stopCruise()
            } else {
                if (!Keys.hasAmap(this@MainActivity)) { showKeyDialog(first = true); return@pillButton }
                if (!hasPermissions()) { requestAllPermissions(); return@pillButton }
                if (!NavService.running) NavService.start(this@MainActivity)
                if(SingleEngineSession.owns)SingleEngineSession.disarm()
                val err = NaviBridge.startCruise(this@MainActivity)
                if (err != null) Toast.makeText(this@MainActivity, "巡航没能开始：$err", Toast.LENGTH_LONG).show()
            }
        }
        row.addView(cruise, LinearLayout.LayoutParams(dp(76), dp(40)))
        card.addView(row)
        refreshers += {
            val on = NaviBridge.cruising
            cruise.text = if (on) "结束" else "开始"
            (cruise.background as? GradientDrawable)?.setColor(if (on) Color.parseColor("#2A7F88") else Color.parseColor("#36B8C6"))
        }
        return wrapMargin(card)
    }

    /** v8.2 状态卡片：圆屏连没连上（大字）、电量、配对提示；导航 / 巡航中多一行；小地图出问题才提示 */
    private fun statusCard(): View {
        val card = card()
        val top = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL }
        val dot = View(this).apply { background = GradientDrawable().apply { shape = GradientDrawable.OVAL; setColor(SUB) } }
        top.addView(dot, LinearLayout.LayoutParams(dp(12), dp(12)).apply { rightMargin = dp(12) })
        val col = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        val title = label("", 18f, TEXT, bold = true).apply { isSingleLine = true; ellipsize = android.text.TextUtils.TruncateAt.END }
        val sub = label("", 12f, SUB).apply { maxLines = 5 }
        col.addView(title); col.addView(sub)
        top.addView(col, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        val bat = TextView(this).apply {
            textSize = 13f; typeface = Typeface.DEFAULT_BOLD; gravity = Gravity.CENTER
            setPadding(dp(10), dp(4), dp(10), dp(4))
            background = GradientDrawable().apply { setColor(BG); cornerRadius = dpf(12f) }
        }
        top.addView(bat, LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply { leftMargin = dp(8) })
        top.setOnClickListener {                          // v6.9 旧配对对不上时：点一下打开系统蓝牙设置
            if (BleLink.needUnbond) try { startActivity(Intent(Settings.ACTION_BLUETOOTH_SETTINGS)) } catch (_: Exception) {}
        }
        card.addView(top)
        val reconnect = textLink("重新连接圆屏") { BleLink.connect(this) }
        card.addView(reconnect, LinearLayout.LayoutParams(ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply { topMargin = dp(4); leftMargin = dp(24) })
        // 导航 / 巡航中
        val navBox = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        navBox.addView(divider(top = 12))
        val navRow = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL; setPadding(0, dp(8), 0, 0) }
        val navDot = View(this).apply { background = GradientDrawable().apply { shape = GradientDrawable.OVAL; setColor(GREEN) } }
        navRow.addView(navDot, LinearLayout.LayoutParams(dp(12), dp(12)).apply { rightMargin = dp(12) })
        val navCol = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        val navTitle = label("", 15f, TEXT, bold = true)
        val navSub = label("", 12f, SUB).apply { maxLines = 2 }
        navCol.addView(navTitle); navCol.addView(navSub)
        navRow.addView(navCol, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        navBox.addView(navRow)
        card.addView(navBox)
        // 小地图出问题时才显示
        val mapWarn = label("", 12f, AMBER).apply { setPadding(dp(24), dp(8), 0, 0); visibility = View.GONE }
        card.addView(mapWarn)
        val offlineLabel=label("",12f,SUB).apply { setPadding(dp(24),dp(6),0,0); maxLines=2 }
        card.addView(offlineLabel)
        refreshers += {
            val conn = BleLink.isConnected
            val searching = BleLink.status.contains("搜索") || BleLink.status.contains("连接…") || BleLink.status.contains("正在")
            setDot(dot, if (conn && BleLink.verified) GREEN else if (conn || searching) AMBER else RED)
            title.text = when {
                conn && BleLink.verified -> BleLink.screenName.ifEmpty { "圆屏" } + " 已连接"
                conn -> "正在确认圆屏配对…"
                searching -> BleLink.status
                else -> "圆屏未连接"
            }
            sub.setTextColor(if (BleLink.needUnbond) AMBER else SUB)
            sub.text = when {
                BleLink.pairHint.isNotEmpty() && !BleLink.verified -> BleLink.pairHint      // v6.4 配对提示
                conn && BleLink.verified -> "已配对验证"
                conn -> "正在验证配对…"
                else -> BleLink.status
            }
            val pct = BleLink.screenBatteryPct
            bat.visibility = if (conn && pct >= 0) View.VISIBLE else View.GONE
            if (pct >= 0) {
                val chg = BleLink.screenBattery.contains("⚡")
                bat.text = (if (chg) "⚡ " else "") + "$pct%"
                bat.setTextColor(if (chg) CYAN else if (pct > 50) GREEN else if (pct >= 15) AMBER else RED)
            }
            reconnect.visibility = if (conn) View.GONE else View.VISIBLE

            val navActive = NaviBridge.lastInfo.startsWith("图标")
            val cruising = NaviBridge.cruising
            navBox.visibility = if (navActive || cruising) View.VISIBLE else View.GONE
            setDot(navDot, if (navActive) PINK else CYAN)
            navTitle.text = if (navActive) "导航中" else "巡航中"
            navSub.text = if (navActive) (NaviBridge.extraInfo.lineSequence().lastOrNull()?.removePrefix("底部：") ?: "") else "提醒测速、电子眼和危险路段"

            offlineLabel.text=MapFeeder.offlineStatus()
            offlineLabel.visibility=if (offlineLabel.text.isEmpty()) View.GONE else View.VISIBLE
            mapWarn.text = MapFeeder.loadingStatus()
            mapWarn.visibility = if (mapWarn.text.isEmpty()) View.GONE else View.VISIBLE
        }
        return wrapMargin(card)
    }

    // ================= v8.2 可折叠的设置分组 =================

    private fun rideCard(): View {
        val box = card()
        val head = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL }
        val col = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        col.addView(label("骑行轨迹",16f,TEXT,bold=true))
        val status = label(RideRecorder.status(),12f,SUB).apply { setPadding(0, dp(4), dp(10), 0) }
        col.addView(status)
        head.addView(col, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        head.addView(label("›", 22f, SUB))
        head.setOnClickListener { startActivity(Intent(this,RideHistoryActivity::class.java)) }
        box.addView(head)
        refreshers += { status.text=RideRecorder.status() }
        return wrapMargin(box)
    }

    private val uiPrefs by lazy { getSharedPreferences("ui", MODE_PRIVATE) }

    /** 一张可折叠卡片：标题 + 一行摘要（当前设置），点标题展开 / 收起；展开状态记在手机上 */
    private fun section(key: String, icon: String, title: String, summary: () -> String,
                        forceOpen: Boolean = false, build: (LinearLayout) -> Unit): View {
        val card = card()
        val head = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER_VERTICAL; setPadding(0, dp(2), 0, dp(2)) }
        head.addView(label(icon, 18f, TEXT), LinearLayout.LayoutParams(dp(32), ViewGroup.LayoutParams.WRAP_CONTENT))
        val col = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        col.addView(label(title, 16f, TEXT, bold = true))
        val sum = label("", 12f, SUB).apply { isSingleLine = true; ellipsize = android.text.TextUtils.TruncateAt.END }
        col.addView(sum)
        head.addView(col, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        // 展开 / 收起箭头放进小圆底里，看得出整行可以点
        val chev = label("", 13f, TEXT).apply {
            gravity = Gravity.CENTER; includeFontPadding = false
            background = GradientDrawable().apply { shape = GradientDrawable.OVAL; setColor(Color.parseColor("#241F2E")) }
        }
        head.addView(chev, LinearLayout.LayoutParams(dp(30), dp(30)).apply { leftMargin = dp(8) })
        val body = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        body.addView(divider(top = 12))
        build(body)
        card.addView(head)
        card.addView(body)
        var open = forceOpen || uiPrefs.getBoolean("open_$key", false)
        fun paint() { body.visibility = if (open) View.VISIBLE else View.GONE; chev.text = if (open) "▴" else "▾" }
        head.setOnClickListener { open = !open; uiPrefs.edit().putBoolean("open_$key", open).apply(); paint() }
        paint()
        refreshers += { sum.text = summary() }
        return wrapMargin(card)
    }

    /** 分组里的小标题 */
    private fun subhead(t: String, top: Int = 12) = label(t, 14f, TEXT).apply { setPadding(0, dp(top), 0, dp(8)) }
    private fun note(t: String) = label(t, 12f, SUB).apply { setPadding(0, dp(6), 0, 0) }

    /** 圆屏显示：小地图开关、风格、夜间配色、清爽模式、行程环、弯道上色、走过的路线、预判 */
    private fun mapSection(card: LinearLayout) {
        card.addView(settingRow("路口红绿灯读秒（实验）", "自动准备路口信息，前方 300 米内显示有效数据",
            toggle({ TrafficLightOverlay.enabled }) { TrafficLightOverlay.enable(this,it) }))
        val lightState=note("")
        card.addView(lightState)
        refreshers += { lightState.text=TrafficLightOverlay.summary() }
        if(packageName=="com.motonav.app.fresh")card.addView(pillButton("查看车机路口信息",PURPLE){
            NavigationUi.openProvider(this)
        },lpMatch(top=4,bottom=8))
        card.addView(settingRow("路口灯显示", "同一快照的左转、直行、右转一起显示，缺项标暂无数据", label("整路口",12f,SUB)))
        card.addView(pillButton("预览路口灯布局（不显示秒数）", STROKE){
            if(BleLink.verified){BleLink.sendEvent("#TLP,1");Toast.makeText(this,"灰色占位预览，5 秒后恢复",Toast.LENGTH_SHORT).show()}
            else Toast.makeText(this,"请先连接圆屏",Toast.LENGTH_SHORT).show()
        },lpMatch(top=4,bottom=8))
        card.addView(note("导航时自动准备前方 300 米内的路口信息，显示可用方向的真实灯色和秒数。缺少的数据不推测，整份数据过期时只保留灰色框架；并非所有路口都有读秒。当前支持实际驾车导航，普通室内模拟、网页摩托车及自行车暂不叠加。首次使用若服务未就绪，可在高级诊断完成初始化。"))
        card.addView(divider())
        card.addView(settingRow("显示小地图", "关掉后圆屏只显示大箭头", toggle({ MapFeeder.enabled }) { MapFeeder.setEnabled(it) }))
        card.addView(divider())
        // v6.0 风格：右边小预览图，点「更换」打开全屏风格卡片
        val themeSub = label("", 12f, SUB)
        val thumb = ImageView(this).apply { scaleType = ImageView.ScaleType.FIT_CENTER }
        val themeCtl = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            addView(thumb, LinearLayout.LayoutParams(dp(46), dp(46)).apply { rightMargin = dp(10) })
            addView(textLink("更换") { showThemePicker() })
        }
        card.addView(settingRow("地图风格", null, themeCtl, themeSub))
        var shownTheme = -1
        refreshers += {
            themeSub.text = MapFeeder.themeName()
            if (shownTheme != MapFeeder.theme) { shownTheme = MapFeeder.theme; setThemeImage(thumb, shownTheme) }
        }
        // v2.8 夜间配色
        card.addView(subhead("夜间配色"))
        card.addView(segmented(listOf(MapFeeder.NIGHT_AUTO to "自动", MapFeeder.NIGHT_OFF to "白天", MapFeeder.NIGHT_ON to "夜间"),
            { MapFeeder.nightMode }) { MapFeeder.setNightMode(it) })
        val nightSub = note("")
        card.addView(nightSub)
        refreshers += {
            nightSub.text = "现在：" + (if (MapFeeder.nightNow) "夜间" else "白天") +
                (if (MapFeeder.nightMode == MapFeeder.NIGHT_AUTO) "（按日出日落自动切换）" else "")
        }
        card.addView(divider(top = 12))
        // v8.0 行程环
        card.addView(settingRow("路程进度环", "最外圈绿环，走过的变灰；快到路口时换成转弯倒计时",
            toggle({ MapFeeder.progress }) { MapFeeder.setProgress(it) }))
        card.addView(divider())
        // v4.8 清爽模式
        card.addView(settingRow("清爽模式", "只留车速和转弯，不显示红绿灯、电子眼等", toggle({ MapFeeder.clean }) { MapFeeder.setClean(it) }))
        card.addView(divider())
        // v5.9 弯道急缓上色
        card.addView(settingRow("弯道急缓上色", "前方路线按弯的急缓涂成绿 / 黄 / 红，跑山路好用",
            toggle({ MapFeeder.curve }) { MapFeeder.setCurve(it) }))
        // v2.8 走过的路线
        card.addView(subhead("走过的路线"))
        card.addView(segmented(listOf(MapFeeder.TRAIL_GREY to "变灰", MapFeeder.TRAIL_ERASE to "擦除"),
            { MapFeeder.trail }) { MapFeeder.setTrail(it) })
        // v7.2 预判提前量
        card.addView(subhead("预判（减少慢半拍）", top = 14))
        card.addView(segmented(listOf(0 to "关", 300 to "0.3 秒", 600 to "0.6 秒"), { MapFeeder.leadMs }) { MapFeeder.setLead(it) })
        card.addView(note("地图和路口距离提前一点显示，抵消定位和蓝牙的延迟。还是慢就选 0.6 秒，提示太早就选小一点"))
    }

    private fun mapSummary(): String =
        if (!MapFeeder.enabled) "小地图已关闭，只显示大箭头"
        else MapFeeder.themeName() + " · " + (if (MapFeeder.nightNow) "夜间" else "白天") + "配色" +
            (if (MapFeeder.clean) " · 清爽" else "") + (if (MapFeeder.progress) " · 进度环" else "")

    /** v3.8 导航语音：高德自带 / 手机系统语音（可选引擎、声音、语速、音调、试听）；v4.1 推荐免费 AI 语音引擎 */
    @Suppress("DEPRECATION")
    private fun voiceSection(card: LinearLayout) {
        val sys = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        card.addView(segmented(listOf(NaviVoice.MODE_AMAP to "高德自带", NaviVoice.MODE_SYSTEM to "手机系统语音"), { NaviVoice.mode }) {
            NaviVoice.setMode(it)
        })
        card.addView(note("切换后下次开始导航生效；导航中可以在导航页点「播报」静音"))
        // v7.0 播报详细程度
        card.addView(subhead("播报内容", top = 16))
        card.addView(segmented(listOf(NaviVoice.BC_DETAIL to "详细", NaviVoice.BC_CONCISE to "简洁"), { NaviVoice.broadcast }) {
            NaviVoice.setBroadcast(it)
        })
        val bcSub = label("", 12f, SUB).apply { setPadding(0, dp(6), 0, 0) }
        card.addView(bcSub)
        refreshers += {
            bcSub.text = (if (NaviVoice.broadcast == NaviVoice.BC_CONCISE)
                "简洁：话少，只在快到路口和关键时刻提醒（熟路用）"
            else "详细：路口前多次提醒，报路名、距离和沿途信息（不熟的路用）")
        }
        // v5.1 播报时音乐怎么办
        card.addView(subhead("播报时的音乐", top = 16))
        card.addView(segmented(listOf(NaviVoice.DUCK_LOWER to "压低", NaviVoice.DUCK_PAUSE to "暂停", NaviVoice.DUCK_OFF to "不管"),
            { NaviVoice.duck }) { NaviVoice.setDuck(it) })
        val duckSub = label("", 12f, SUB).apply { setPadding(0, dp(6), 0, 0) }
        card.addView(duckSub)
        card.addView(pillButton("放着音乐点我试试", STROKE) { NaviVoice.test() }, lpMatch(top = 8, bottom = 4).apply { height = dp(40) })
        // v6.6 有的音乐 App 压不住：提示改用“暂停”，暂停模式没开通知使用权时点这里去开
        val duckTip = label("", 12f, AMBER).apply {
            setPadding(0, dp(6), 0, 0)
            setOnClickListener {
                if (NaviVoice.duck == NaviVoice.DUCK_PAUSE && !NaviVoice.canControlMedia())
                    try { startActivity(Intent("android.settings.ACTION_NOTIFICATION_LISTENER_SETTINGS")) } catch (_: Exception) { toast("没能打开设置页，请到系统设置里搜「通知使用权」") }
            }
        }
        card.addView(duckTip)
        refreshers += { duckSub.text = "播报时音乐${when (NaviVoice.duck) { NaviVoice.DUCK_LOWER -> "自动变小，播完恢复"; NaviVoice.DUCK_PAUSE -> "暂停，播完继续"; else -> "不处理" }}"
            duckTip.text = when {
                NaviVoice.duck == NaviVoice.DUCK_LOWER -> "压不住？有的音乐 App（小米手机上常见）不配合“压低”，改选「暂停」更靠谱"
                NaviVoice.duck == NaviVoice.DUCK_PAUSE && !NaviVoice.canControlMedia() ->
                    "👉 点这里打开 MotoNav 的「通知使用权」：不听话的音乐 App 也能被直接暂停、播完自动接着放"
                else -> ""
            }
            duckTip.visibility = if (duckTip.text.isEmpty()) View.GONE else View.VISIBLE
        }

        card.addView(subhead("圆屏音乐遥控", top = 16))
        val musicHint = label("", 12f, SUB)
        card.addView(musicHint)
        card.addView(pillButton("开启通知使用权", STROKE) {
            try { startActivity(Intent("android.settings.ACTION_NOTIFICATION_LISTENER_SETTINGS")) }
            catch (_: Exception) { toast("请到系统设置里开启 MotoNav 通知使用权") }
        }, lpMatch(top = 8, bottom = 4).apply { height = dp(40) })
        refreshers += { musicHint.text = if (AmapProbe.notifEnabled(this))
            "圆屏左滑查看天气，右滑查看歌曲；音乐页左滑返回，点按上一首、播放/暂停、下一首。上下滑也能查看天气。"
            else "先开启通知使用权，才能显示歌名和控制手机播放器。需要圆屏固件 v7.8.1。" }

        // 语音引擎
        val engineSub = label("", 12f, SUB)
        sys.addView(divider(top = 12))
        sys.addView(settingRow("语音引擎", null, textLink("更换") {
            val list = NaviVoice.engines()
            if (list.isEmpty()) { toast("手机上没找到语音引擎，请先到应用商店安装一个"); return@textLink }
            android.app.AlertDialog.Builder(this, android.app.AlertDialog.THEME_DEVICE_DEFAULT_DARK)
                .setTitle("选择语音引擎")
                .setItems(list.map { it.second }.toTypedArray()) { _, i -> NaviVoice.setEngine(list[i].first) { toast("已切换：${list[i].second}") } }
                .show()
        }, engineSub))
        // 声音
        val voiceSub = label("", 12f, SUB)
        sys.addView(settingRow("声音", null, textLink("选择") {
            val vs = NaviVoice.zhVoices()
            if (vs.isEmpty()) { toast("这个引擎里没有中文声音（或还在准备中），换个引擎试试"); return@textLink }
            val names = arrayOf("引擎默认") + vs.map { NaviVoice.voiceLabel(it) }.toTypedArray()
            android.app.AlertDialog.Builder(this, android.app.AlertDialog.THEME_DEVICE_DEFAULT_DARK)
                .setTitle("选择声音（${vs.size} 个中文声音）")
                .setItems(names) { _, i -> NaviVoice.setVoice(if (i == 0) "" else vs[i - 1].name); NaviVoice.test() }
                .show()
        }, voiceSub))
        // 语速、音调
        fun slider(title: String, get: () -> Float, set: (Float) -> Unit): View {
            val box = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(0, dp(6), 0, 0) }
            val t = label("", 14f, TEXT)
            box.addView(t)
            val sb = SeekBar(this).apply {
                max = 30                                       // 0.5 ~ 2.0，每格 0.05
                progress = ((get() - 0.5f) / 0.05f).toInt()
                progressTintList = ColorStateList.valueOf(PINK)
                thumbTintList = ColorStateList.valueOf(PINK)
                progressBackgroundTintList = ColorStateList.valueOf(STROKE)
                setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                    override fun onProgressChanged(s: SeekBar, p: Int, fromUser: Boolean) { if (fromUser) set(0.5f + p * 0.05f) }
                    override fun onStartTrackingTouch(s: SeekBar) {}
                    override fun onStopTrackingTouch(s: SeekBar) { NaviVoice.test() }
                })
            }
            box.addView(sb, lpMatch(top = 2, bottom = 2))
            refreshers += { t.text = "$title  %.2f×".format(get()) }
            return box
        }
        sys.addView(slider("语速", { NaviVoice.rate }) { NaviVoice.setRate(it) })
        sys.addView(slider("音调", { NaviVoice.pitch }) { NaviVoice.setPitch(it) })
        // 试听 + 系统语音设置
        val btns = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; setPadding(0, dp(10), 0, dp(4)) }
        btns.addView(pillButton("试听", PURPLE) { NaviVoice.test() }, LinearLayout.LayoutParams(0, dp(40), 1f).apply { rightMargin = dp(8) })
        btns.addView(pillButton("系统语音设置", STROKE) {
            try { startActivity(Intent("com.android.settings.TTS_SETTINGS")) }
            catch (_: Exception) { startActivity(Intent(Settings.ACTION_SETTINGS)) }
        }, LinearLayout.LayoutParams(0, dp(40), 1f))
        sys.addView(btns)
        // v4.1 推荐的免费语音（都是独立的语音引擎 App，装好后在上面「更换」里选；MotoNav 本身不变大）
        sys.addView(divider(top = 12))
        sys.addView(label("免费好听的语音", 14f, TEXT).apply { setPadding(0, dp(10), 0, dp(2)) })
        sys.addView(label("装好后回来点上面的「更换」选它，再点「选择」挑声音", 12f, SUB).apply { setPadding(0, 0, 0, dp(6)) })
        for (e in FREE_VOICES) sys.addView(freeVoiceRow(e))
        card.addView(sys)

        refreshers += {
            sys.visibility = if (NaviVoice.mode == NaviVoice.MODE_SYSTEM) View.VISIBLE else View.GONE
            engineSub.text = NaviVoice.status
            voiceSub.text = NaviVoice.currentVoiceLabel()
        }
    }

    private fun voiceSummary(): String =
        (if (NaviVoice.mode == NaviVoice.MODE_AMAP) "高德自带" else "手机系统语音") + " · " +
            (if (NaviVoice.broadcast == NaviVoice.BC_CONCISE) "简洁" else "详细") + "播报 · 音乐" +
            when (NaviVoice.duck) { NaviVoice.DUCK_LOWER -> "压低"; NaviVoice.DUCK_PAUSE -> "暂停"; else -> "不管" }

    private fun toast(t: String) = Toast.makeText(this, t, Toast.LENGTH_SHORT).show()

    // ---------------- v6.0 风格选择：全屏卡片 + 预览图 ----------------

    /** 预览图放在 res/drawable/ 或 res/drawable-nodpi/；找不到就画一个纯色圆代替 */
    private fun setThemeImage(iv: ImageView, t: Int) {
        val id = when (t) {
            0 -> R.drawable.theme_0
            1 -> R.drawable.theme_1
            5 -> R.drawable.theme_5
            MapFeeder.THEME_DRAGON -> R.drawable.theme_11
            else -> {
                val pkgId = resources.getIdentifier("theme_$t", "drawable", "com.yuminaga.motonav")
                if (pkgId != 0) pkgId else resources.getIdentifier("theme_$t", "drawable", packageName)
            }
        }
        if (id != 0) iv.setImageResource(id)
        else iv.setImageDrawable(GradientDrawable().apply { shape = GradientDrawable.OVAL; setColor(STROKE) })
    }

    private fun showThemePicker() {
        val dlg = android.app.Dialog(this, android.R.style.Theme_DeviceDefault_NoActionBar)
        val root = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setBackgroundColor(BG)
            setPadding(dp(16), dp(20), dp(16), dp(12))
        }
        // 标题
        root.addView(label("小地图风格", 22f, TEXT, bold = true))
        root.addView(label("四种独立风格。下图为示意，原版保留高德默认颜色和标注；圆屏需更新配套固件。", 12f, SUB).apply {
            setPadding(0, dp(4), 0, dp(12))
        })
        // 卡片网格（两列）
        val gap = dp(12)
        val itemW = (resources.displayMetrics.widthPixels - dp(16) * 2 - gap) / 2
        val grid = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        val cards = HashMap<Int, LinearLayout>()
        fun styleCard(c: LinearLayout, selected: Boolean) {
            c.background = GradientDrawable().apply {
                setColor(if (selected) Color.parseColor("#2A1830") else CARD)
                cornerRadius = dpf(18f)
                setStroke(if (selected) dp(2) else dp(1), if (selected) PINK else STROKE)
            }
        }
        val order = MapFeeder.THEME_ORDER
        var row: LinearLayout? = null
        order.forEachIndexed { i, t ->
            if (i % 2 == 0) {
                row = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL }
                grid.addView(row, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply { bottomMargin = gap })
            }
            val c = LinearLayout(this).apply {
                orientation = LinearLayout.VERTICAL
                gravity = Gravity.CENTER_HORIZONTAL
                setPadding(dp(10), dp(12), dp(10), dp(12))
                isClickable = true
            }
            val img = ImageView(this).apply { scaleType = ImageView.ScaleType.FIT_CENTER }
            setThemeImage(img, t)
            val imgSize = itemW - dp(36)
            c.addView(img, LinearLayout.LayoutParams(imgSize, imgSize))
            val name = label(MapFeeder.themeName(t), 15f, TEXT, bold = true).apply {
                gravity = Gravity.CENTER; setPadding(0, dp(8), 0, dp(2))
            }
            c.addView(name, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT))
            val desc = label(MapFeeder.themeDesc(t), 11f, SUB).apply { gravity = Gravity.CENTER; maxLines = 3 }
            c.addView(desc, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT))
            styleCard(c, t == MapFeeder.theme)
            c.setOnClickListener {
                val old = MapFeeder.theme
                if (old != t) {
                    MapFeeder.setTheme(t)
                    cards[old]?.let { styleCard(it, false) }
                    styleCard(c, true)
                    toast("已换成：${MapFeeder.themeName(t)}")
                }
            }
            cards[t] = c
            row!!.addView(c, LinearLayout.LayoutParams(itemW, ViewGroup.LayoutParams.MATCH_PARENT).apply {
                if (i % 2 == 0) rightMargin = gap
            })
        }
        val scroll = ScrollView(this).apply { addView(grid) }
        root.addView(scroll, LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, 0, 1f))
        // 底部：完成
        root.addView(pillButton("完成", PINK) { dlg.dismiss() },
            LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(46)).apply { topMargin = dp(10) })
        dlg.setContentView(root)
        dlg.window?.setLayout(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT)
        dlg.show()
    }

    // ---- v4.1 推荐的免费语音引擎 ----
    private class FreeVoice(val name: String, val desc: String, val pkgKey: String, val market: String, val web: String)

    private val FREE_VOICES = listOf(
        FreeVoice("Google 语音合成", "普通话男女声都有，下载语音包后离线可用，最稳定。装好后在「系统语音设置」→ Google 齿轮 → 安装语音数据 → 中文（中国）",
            "com.google.android.tts", "market://details?id=com.google.android.tts",
            "https://play.google.com/store/apps/details?id=com.google.android.tts"),
        FreeVoice("VoxSherpa TTS（AI 语音）", "开源免费的 AI 语音，声音更自然；在它的模型页下载中文模型（女声一般以 zf_ 开头）。手机上现算，可能慢零点几秒",
            "voxsherpa", "market://search?q=VoxSherpa%20TTS", "https://github.com/CodeBySonu95/VoxSherpa-TTS"),
    )

    private fun freeVoiceRow(e: FreeVoice): View {
        val st = label("", 12f, SUB)
        val btn = textLink("安装") {
            try { startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(e.market))) }
            catch (_: Exception) { startActivity(Intent(Intent.ACTION_VIEW, Uri.parse(e.web))) }
        }
        refreshers += {
            val has = NaviVoice.hasEngine(e.pkgKey)
            st.text = (if (has) "✓ 已安装　" else "") + e.desc
            btn.text = if (has) "打开商店" else "安装"
        }
        return settingRow(e.name, null, btn, st)
    }

    /** 圆屏设置：来电显示、亮度、方向、配对的手机、连哪块圆屏 */
    private fun screenSection(card: LinearLayout) {
        val bootSub = label("", 12f, SUB)
        card.addView(settingRow("开机动画 · 摩托车品牌", null, textLink("选择") {
            android.app.AlertDialog.Builder(this, android.app.AlertDialog.THEME_DEVICE_DEFAULT_DARK)
                .setTitle("选择圆屏开机品牌")
                .setSingleChoiceItems(BootBrand.labels, BootBrand.selected) { dialog, which ->
                    BootBrand.select(which)
                    bootSub.text = BootBrand.summary()
                    dialog.dismiss()
                }
                .setNegativeButton("取消", null).show()
        }, bootSub))
        refreshers += { bootSub.text = BootBrand.summary() }
        card.addView(note("适用 1.75 与 1.85B；保存后断电重启圆屏播放，关闭可直接进入原待机画面"))
        card.addView(divider())
        // v5.0 来电显示
        val callSub = label("", 12f, SUB).apply {                // v7.5 权限没给全时黄字提示，点一下去授权
            setOnClickListener {
                if (CallWatcher.enabled && CallWatcher.PERMS.any { checkSelfPermission(it) != PackageManager.PERMISSION_GRANTED })
                    requestPermissions(CallWatcher.PERMS, 7)
            }
        }
        card.addView(settingRow("来电显示", null,
            toggle({ CallWatcher.enabled }) { on ->
                CallWatcher.setEnabled(this, on)
                if (on && CallWatcher.PERMS.any { checkSelfPermission(it) != PackageManager.PERMISSION_GRANTED })
                    requestPermissions(CallWatcher.PERMS, 7)
            }, callSub))
        refreshers += {
            val missing = CallWatcher.enabled && CallWatcher.PERMS.any { checkSelfPermission(it) != PackageManager.PERMISSION_GRANTED }
            callSub.text = if (missing) "⚠ ${CallWatcher.permsText(this)}（点这里授权）"
                else "来电时圆屏显示名字和号码，按住圆屏 0.2 秒接听"   // v7.7
            callSub.setTextColor(if (missing) AMBER else SUB)
        }
        card.addView(divider())
        // 亮度
        var syncing = false
        val autoToggle = toggle({ Brightness.auto }) { if (!syncing) Brightness.changeAuto(it) }
        val brightSub = label("", 12f, SUB)
        card.addView(settingRow("自动亮度", null, autoToggle, brightSub))
        val seek = SeekBar(this).apply {
            max = 250                                   // 实际亮度 = 进度 + 5，范围 5~255
            progress = Brightness.manualLevel - 5
            progressTintList = ColorStateList.valueOf(CYAN)
            thumbTintList = ColorStateList.valueOf(CYAN)
            progressBackgroundTintList = ColorStateList.valueOf(STROKE)
            setOnSeekBarChangeListener(object : SeekBar.OnSeekBarChangeListener {
                override fun onProgressChanged(sb: SeekBar, p: Int, fromUser: Boolean) {
                    if (!fromUser) return
                    syncing = true
                    Brightness.changeManual(p + 5)          // 拖动滑块 = 改为手动亮度
                    syncing = false
                }
                override fun onStartTrackingTouch(sb: SeekBar) {}
                override fun onStopTrackingTouch(sb: SeekBar) {}
            })
        }
        card.addView(seek, lpMatch(top = 4, bottom = 6))
        refreshers += {
            val cur = if (Brightness.current < 0) "—" else "${Brightness.current}"
            brightSub.text = if (Brightness.auto) "当前 $cur · ${Brightness.desc}" else "手动 · 当前 $cur（拖动滑块调节）"
        }
        card.addView(divider())
        // v6.3 圆屏方向：按住圆盘转动
        card.addView(subhead("圆屏方向"))
        val dial = RotDial(this, PINK, CYAN, TEXT, SUB, STROKE)
        dial.setAngle(MapFeeder.screenDeg)
        var lastSendAt = 0L
        dial.onChange = { d, final ->
            val now = android.os.SystemClock.elapsedRealtime()
            if (final || now - lastSendAt >= 70) {           // 拖动时最多每秒十几次，免得蓝牙堵住
                lastSendAt = now
                MapFeeder.setScreenDeg(d, save = final)
            }
        }
        card.addView(dial, lpMatch(bottom = 12))
        val btnRow = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; gravity = Gravity.CENTER }
        fun stepBtn(t: String, act: () -> Unit) = pillButton(t, BG, act).apply { textSize = 17f }
        val nudge = { delta: Int ->
            val d = ((dial.angle + delta) % 360 + 360) % 360
            dial.setAngle(d); MapFeeder.setScreenDeg(d, save = true)
        }
        btnRow.addView(stepBtn("−1°") { nudge(-1) }, LinearLayout.LayoutParams(dp(84), dp(44)).apply { rightMargin = dp(10) })
        btnRow.addView(stepBtn("+1°") { nudge(1) }, LinearLayout.LayoutParams(dp(84), dp(44)).apply { rightMargin = dp(10) })
        btnRow.addView(stepBtn("归零") { dial.setAngle(0); MapFeeder.setScreenDeg(0, save = true) }, LinearLayout.LayoutParams(dp(84), dp(44)))
        card.addView(btnRow)
        card.addView(note("按住圆盘转动，粉色指针 = 画面上方在圆屏上朝的方向，圆屏实时跟着转；转到 0°/90°/180°/270° 附近会吸住"))
        card.addView(divider(top = 14))
        // v6.4 配对验证（固件 v6.5）
        card.addView(settingRow("配对的手机",
            "圆屏只听配对过的手机。加新手机：重启圆屏，2 分钟内连接并输入配对码",
            textLink("忘掉其他手机") {
                if (!BleLink.verified) { toast("先连上圆屏并通过验证再点"); return@textLink }
                android.app.AlertDialog.Builder(this, android.app.AlertDialog.THEME_DEVICE_DEFAULT_DARK)
                    .setTitle("让圆屏忘掉其他手机？")
                    .setMessage("除了现在连着的这台，圆屏记住的其他手机都会被忘掉，它们以后要重新配对。\n\n手机丢了、换了手机时用新手机点一下。")
                    .setPositiveButton("忘掉") { _, _ -> BleLink.forgetOthers(); toast("已让圆屏忘掉其他手机") }
                    .setNegativeButton("取消", null)
                    .show()
            }))
        card.addView(divider())
        // v7.8 身边有几块圆屏：换一块连
        card.addView(settingRow("连哪块圆屏",
            "App 优先连上次那块；身边有几块圆屏、想连另一块时点「换一块」。编号在圆屏首页下方",
            textLink("换一块") { BleLink.switchScreen(this); toast("正在找另一块圆屏…") }))
    }

    /** v6.2 Key 设置：显示两个 key 填没填，点「填写 / 修改」弹出填写页 */
    private fun keySection(card: LinearLayout) {
        val amapSub = label("", 12f, SUB)
        card.addView(settingRow("高德 key", null, textLink("填写 / 修改") { showKeyDialog(first = false) }, amapSub))
        card.addView(divider())
        card.addView(divider())
        val weatherSub = label("和风天气：实况、逐小时、降水、空气、预警", 12f, SUB)
        card.addView(settingRow("和风天气", null, textLink("配置") { showWeatherConfig() }, weatherSub))
        card.addView(textLink("查看天气详情与数据来源") {
            android.app.AlertDialog.Builder(this).setTitle("和风天气")
                .setMessage(Weather.detailText()).setPositiveButton("关闭", null).show()
        }, LinearLayout.LayoutParams(-2, -2).apply { topMargin = dp(8) })
        refreshers += { weatherSub.text = if (Weather.host().isBlank() || Weather.key().isBlank()) "未配置：请填写 API Host 与 API Key" else "已配置 · 仅查看天气时请求，10分钟缓存" }
        refreshers += {
            val a = Keys.amap(this)
            amapSub.text = Keys.mask(a) + (if (a.isEmpty()) "：导航、搜索、定位都用不了" else if (a != Keys.appliedAmap) "（重启 App 后生效）" else "") +
                "\n导航、搜索、定位、普通底图和卫星图用它"
            amapSub.setTextColor(if (a.isEmpty()) RED else SUB)
        }
    }

    private fun showWeatherConfig() {
        val box = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL; setPadding(dp(20), dp(8), dp(20), dp(8)) }
        box.addView(label("和风控制台 → 设置 → API Host；项目 → 凭据 → API Key。普通天气不再请求高德天气或 Open-Meteo。", 13f, SUB))
        val host = EditText(this).apply { hint = "xxxx.qweatherapi.com"; setText(Weather.host()); setSingleLine(true) }
        val key = EditText(this).apply {
            hint = "和风 API Key"; setText(Weather.key()); setSingleLine(true)
            inputType = android.text.InputType.TYPE_CLASS_TEXT or android.text.InputType.TYPE_TEXT_VARIATION_PASSWORD
        }
        box.addView(host); box.addView(key)
        box.addView(label("四类权限：天气预报、分钟降水、空气质量、天气预警。只启用实际使用的服务，按账号额度计费。数据来源：和风天气 QWeather。", 12f, SUB))
        box.addView(textLink("和风天气数据来源说明") { startActivity(Intent(Intent.ACTION_VIEW, android.net.Uri.parse("https://developer.qweather.com/attribution.html"))) })
        val dialog = android.app.AlertDialog.Builder(this).setTitle("和风天气配置").setView(box)
            .setNegativeButton("取消", null).setPositiveButton("保存", null).create()
        dialog.setOnShowListener {
            dialog.getButton(android.app.AlertDialog.BUTTON_POSITIVE).setOnClickListener {
                try {
                    Weather.save(host.text.toString(), key.text.toString().trim())
                    toast("已保存，滑到圆屏天气页查询")
                    dialog.dismiss()
                } catch(e: IllegalArgumentException) { toast(e.message ?: "请检查配置") }
            }
        }
        dialog.show()
    }

    // ================= v8.2 开发者选项（连点 5 下标题下面的版本号打开 / 隐藏） =================

    private var devWrap: View? = null
    private var devTaps = 0
    private var devTapAt = 0L

    private fun onVersionTap() {
        val now = android.os.SystemClock.elapsedRealtime()
        devTaps = if (now - devTapAt < 800) devTaps + 1 else 1
        devTapAt = now
        if (devTaps < 5) return
        devTaps = 0
        val on = !uiPrefs.getBoolean("dev_mode", false)
        uiPrefs.edit().putBoolean("dev_mode", on).apply()
        devWrap?.visibility = if (on) View.VISIBLE else View.GONE
        if (on) mainPages.showPage(3)
        toast(if (on) "已打开开发者选项（圆屏设置页底部）" else "已隐藏开发者选项")
    }

    /** 开发者选项：地图来源、屏幕速度、测试按钮、各种状态和原始数据 */
    private fun devSection(card: LinearLayout) {
        // 原来在首页顶部的实验入口
        card.addView(pillButton("单引擎验证入口（实验）", STROKE) { startActivity(Intent(this, SingleEngineActivity::class.java)) },
            LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,dp(40)).apply { bottomMargin=dp(8) })
        card.addView(pillButton("路口服务初始化 / 高级诊断", STROKE) { NavigationUi.openProvider(this) },
            LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,dp(40)).apply { bottomMargin=dp(8) })
        // 状态（原来首页上的小地图行、帧率）
        val stat = label("", 12f, SUB).apply { setLineSpacing(0f, 1.15f) }
        card.addView(stat)
        refreshers += {
            if (stat.isShown) {
                stat.text = "圆屏：${BleLink.status}" + (if (BleLink.screenFps.isNotEmpty()) "\n小地图 ${BleLink.screenFps} 帧/秒" else "") +
                    "\n小地图：" + (if (!MapFeeder.enabled) "已关闭" else "${MapFeeder.sourceName()} · ${MapFeeder.themeName()}") +
                    "\nApp $APP_VERSION · 后台服务${if (NavService.running) "运行中" else "未运行"}"
            }
        }
        card.addView(subhead("地图来源"))
        card.addView(label("地图来源：高德（普通底图和卫星图）", 13f, SUB))
        card.addView(note("高德底图支持后台服务息屏取图；出发前准备好沿线离线地图，可减少途中等待。可随时选回高德。"))
        card.addView(settingRow("俯视楼块（实验）", "高德建筑轮廓；需要楼块实验固件，取图未完成时继续显示普通底图",
            toggle({MapFeeder.buildingEnabled}){MapFeeder.setBuildingEnabled(it)}))
        card.addView(pillButton("显示 / 隐藏高德取图预览", STROKE) { AmapCaptureHost.togglePreview() },
            LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,dp(40)))
        card.addView(subhead("屏幕数据线速度", top = 14))
        card.addView(segmented(listOf(40 to "40 标准", 60 to "60", 80 to "80"), { BleLink.screenMhz }) { BleLink.send("#q,$it") })
        card.addView(note("越快画面撕裂越轻；花屏就切回 40。切换后圆屏会自动重启"))
        card.addView(settingRow("快速刷屏", "提高帧率；花屏时关掉（圆屏会自动重启）",
            toggle({ BleLink.screenFast != 0 }) { BleLink.send(if (it) "#f,1" else "#f,0") }))
        card.addView(divider(top = 8))
        val testRow = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; setPadding(0, dp(12), 0, dp(10)) }
        testRow.addView(pillButton("测试：左转 160 米", STROKE) { BleLink.send("L,160") }, LinearLayout.LayoutParams(0, dp(40), 1f).apply { rightMargin = dp(8) })
        testRow.addView(pillButton("圆屏清屏", STROKE) { BleLink.send("C") }, LinearLayout.LayoutParams(0, dp(40), 1f))
        card.addView(testRow)
        // v3.5 高德探测 / v3.0 高德原版导航界面
        card.addView(pillButton("高德探测", STROKE) { startActivity(Intent(this@MainActivity, ProbeActivity::class.java)) },
            LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(40)).apply { bottomMargin = dp(10) })
        if(!SingleEngineSession.EXPERIMENT)card.addView(pillButton("用高德原版界面导航", STROKE) { NaviBridge.startNaviComponent(this@MainActivity) },
            LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(40)).apply { bottomMargin = dp(10) })
        val info = label("", 12f, SUB).apply { setLineSpacing(0f, 1.15f); setTextIsSelectable(true) }
        card.addView(info)
        // 高德预览：左 = 原图，右 = 归类上色后
        val pvRow = LinearLayout(this).apply { orientation = LinearLayout.HORIZONTAL; setPadding(0, dp(10), 0, 0) }
        pvRaw = ImageView(this).apply { adjustViewBounds = true }
        pvCls = ImageView(this).apply { adjustViewBounds = true }
        pvRow.addView(pvRaw, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f).apply { rightMargin = dp(4) })
        pvRow.addView(pvCls, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        card.addView(pvRow)
        info.setOnLongClickListener { CrashLog.clear(); toast("已清除闪退记录"); true }   // v5.5
        refreshers += {
            if (info.isShown) {
                val crash = CrashLog.last()
                info.text = (if (crash.isNotEmpty()) "⚠ 上次闪退（截图发出来；长按这里清除）：\n$crash\n\n" else "") +
                    "Key：${Keys.applyInfo}\n" +
                    "圆屏：${BleLink.statsText()}\n${BleLink.screenStats}\n" +
                    (if (BleLink.screenProf.isNotEmpty()) "${BleLink.screenProf}\n" else "") + "\n" +
                    "小地图：${MapFeeder.infoText()}\n\n" +
                    "导航：${NaviBridge.lastInfo}\n" +
                    (if (NaviBridge.extraInfo.isNotEmpty()) "${NaviBridge.extraInfo}\n" else "") +
                    "巡航/安全提示：${NaviBridge.cruiseRaw}\n" +
                    "来电显示：${CallWatcher.lastInfo}（${CallWatcher.permsText(this)}）\n" +
                    "圆屏最后一次按下：${CallWatcher.lastTouch}\n" +
                    "音乐遥控：${MusicBridge.info}\n" +
                    "天气：${Weather.info}\n" +
                    "分享路线：${AmapShare.info}\n" +
                    "播报压低音乐：${NaviVoice.duckInfo}\n" +
                    "语音广播：${NaviVoice.broadcastInfo}\n" +
                    "\n车道原始数据：\n${NaviBridge.laneRaw}\n" +
                    "\n电子眼原始数据：\n${NaviBridge.camRaw}"
            }
        }
    }

    /** v6.2 填 key 的页面：两个输入框 + 申请高德 key 要用的包名、SHA1（点一下复制） */
    private fun showKeyDialog(first: Boolean) {
        val box = LinearLayout(this).apply {
            orientation = LinearLayout.VERTICAL
            setPadding(dp(20), dp(8), dp(20), dp(4))
        }
        fun copyRow(name: String, value: String) = label("$name：$value  ⧉", 12f, CYAN).apply {
            setPadding(0, dp(4), 0, dp(4))
            setOnClickListener {
                val cm = getSystemService(CLIPBOARD_SERVICE) as android.content.ClipboardManager
                cm.setPrimaryClip(android.content.ClipData.newPlainText(name, value))
                toast("已复制$name")
            }
        }
        fun input(hint: String, value: String) = EditText(this).apply {
            setText(value)
            this.hint = hint
            setSingleLine(true)
            textSize = 14f
            setTextColor(TEXT)
            setHintTextColor(SUB)
            typeface = Typeface.MONOSPACE
        }
        if (first) {
            val missing = listOfNotNull(if (!Keys.hasAmap(this)) "高德 key" else null)
            box.addView(label("App 里不带任何 key，要填你自己申请的（都是免费的）。还没填：${missing.joinToString("、")}。" +
                "填好以后可以在底部「地图 Key」页面里改。", 13f, TEXT).apply { setPadding(0, 0, 0, dp(10)) })
        }

        box.addView(label("高德 key（必填）", 15f, TEXT, bold = true))
        box.addView(label("高德开放平台 lbs.amap.com → 控制台 → 应用管理 → 创建应用 → 添加 Key，" +
            "服务平台选「Android 平台」，把下面两项填进去：", 12f, SUB).apply { setPadding(0, dp(4), 0, dp(2)) })
        box.addView(copyRow("包名（PackageName）", packageName))
        // v6.8 按这个安装包是调试版还是正式版，告诉用户填到高德的哪一格
        val debugBuild = (applicationInfo.flags and android.content.pm.ApplicationInfo.FLAG_DEBUGGABLE) != 0
        box.addView(copyRow(if (debugBuild) "调试版安全码 SHA1" else "发布版安全码 SHA1", Keys.signSha1(this)))
        box.addView(label(if (debugBuild) "你现在装的是调试版（Android Studio 点 Run 装的）：填到高德 key 的「调试版安全码 SHA1」"
            else "你现在装的是正式版（用 .jks 打包的 APK）：填到高德 key 的「发布版安全码 SHA1」", 11f, SUB))
        val amapIn = input("粘贴高德 key", Keys.userAmap(this))
        box.addView(amapIn, lpMatch(top = 6, bottom = 14))

        android.app.AlertDialog.Builder(this, android.app.AlertDialog.THEME_DEVICE_DEFAULT_DARK)
            .setTitle(if (first) "填写地图 key" else "Key 设置")
            .setView(ScrollView(this).apply { addView(box) })
            .setNegativeButton(if (first) "稍后再填" else "取消", null)
            .setPositiveButton("保存") { _, _ ->
                val a = amapIn.text.toString().filterNot { it.isWhitespace() }
                val needRestart = Keys.save(this, a)
                MapFeeder.onKeysChanged()
                if (needRestart && Keys.hasAmap(this) && NavigationUi.active()) {
                    toast("Key 已保存，本次导航继续；导航结束后重开 App 生效")
                } else if (needRestart && Keys.hasAmap(this)) {
                    android.app.AlertDialog.Builder(this, android.app.AlertDialog.THEME_DEVICE_DEFAULT_DARK)
                        .setTitle("高德 key 已保存")
                        .setMessage("要重新打开一次 App，导航、搜索才会用上新的 key。")
                        .setPositiveButton("现在重启") { _, _ -> Keys.restartApp(this) }
                        .setNegativeButton("稍后", null)
                        .show()
                } else toast("已保存")
            }
            .show()
    }

    /** 退出：短按提示，长按才退出 */
    private fun exitButton(): View {
        val t = TextView(this).apply {
            text = "长按退出 MotoNav"
            textSize = 13f
            setTextColor(SUB)
            gravity = Gravity.CENTER
            setPadding(0, dp(14), 0, dp(14))
            background = GradientDrawable().apply {
                cornerRadius = dpf(20f)
                setStroke(dp(1), STROKE)
            }
            setOnClickListener { Toast.makeText(this@MainActivity, "防误触：请长按退出", Toast.LENGTH_SHORT).show() }
            setOnLongClickListener {
                NavService.stop(this@MainActivity)
                finishAndRemoveTask()
                true
            }
        }
        return FrameLayout(this).apply {
            setPadding(0, dp(18), 0, 0)
            addView(t, FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT))
        }
    }

    // ================= 小组件 =================

    private fun card(): LinearLayout = LinearLayout(this).apply {
        orientation = LinearLayout.VERTICAL
        setPadding(dp(18), dp(16), dp(18), dp(16))
        background = GradientDrawable().apply {
            setColor(CARD)
            cornerRadius = dpf(20f)
            setStroke(dp(1), STROKE)
        }
    }

    private fun wrapMargin(v: View): View = FrameLayout(this).apply {
        setPadding(0, 0, 0, dp(12))
        addView(v, FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT))
    }

    private fun label(t: String, size: Float, color: Int, bold: Boolean = false) = TextView(this).apply {
        text = t
        textSize = size
        setTextColor(color)
        if (bold) typeface = Typeface.DEFAULT_BOLD
    }

    private fun divider(top: Int = 10): View = View(this).apply {
        setBackgroundColor(STROKE)
        layoutParams = LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, dp(1)).apply { topMargin = dp(top); bottomMargin = dp(2) }
    }

    private fun lpMatch(top: Int = 0, bottom: Int = 0) =
        LinearLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT).apply {
            topMargin = dp(top); bottomMargin = dp(bottom)
        }

    /** 按钮：彩色实心；传 STROKE 时画成描边按钮（次要操作），不再是一块像禁用的灰色 */
    private fun pillButton(t: String, color: Int, onClick: () -> Unit) = TextView(this).apply {
        text = t
        textSize = 15f
        val outline = color == STROKE
        setTextColor(if (outline) TEXT else Color.WHITE)
        typeface = Typeface.DEFAULT_BOLD
        gravity = Gravity.CENTER
        includeFontPadding = false
        setPadding(dp(12), 0, dp(12), 0)               // v3.6 上下不留内边距，按钮高度由外面定，字不会被切掉
        minHeight = dp(40)
        background = GradientDrawable().apply {
            cornerRadius = dpf(14f)
            if (outline) { setColor(Color.TRANSPARENT); setStroke(dp(1), Color.parseColor("#4A425A")) } else setColor(color)
        }
        setOnClickListener { onClick() }
    }

    /** 小按钮（胶囊）：青色字 + 淡青底 + 细边框，一看就是能点的 */
    private fun textLink(t: String, onClick: () -> Unit) = TextView(this).apply {
        text = t
        textSize = 13f
        setTextColor(CYAN)
        typeface = Typeface.DEFAULT_BOLD
        gravity = Gravity.CENTER
        includeFontPadding = false
        setPadding(dp(14), dp(8), dp(14), dp(8))
        minHeight = dp(34)
        background = GradientDrawable().apply {
            setColor(Color.argb(30, 75, 221, 235)); cornerRadius = dpf(17f); setStroke(dp(1), Color.argb(90, 75, 221, 235))
        }
        setOnClickListener { onClick() }
    }

    /** 给所有可点的控件加按下水波纹（不改点击行为），按下时有反馈 */
    private fun addPressFeedback(v: View) {
        if (v.hasOnClickListeners() && v.foreground == null) {
            val radius = (v.background as? GradientDrawable)?.takeIf { it.shape == GradientDrawable.RECTANGLE }?.cornerRadius ?: dpf(12f)
            val mask = GradientDrawable().apply { setColor(Color.WHITE); cornerRadius = radius }
            v.foreground = RippleDrawable(ColorStateList.valueOf(Color.argb(46, 255, 255, 255)), null, mask)
        }
        if (v is ViewGroup) for (i in 0 until v.childCount) addPressFeedback(v.getChildAt(i))
    }

    private fun setDot(v: View, color: Int) {
        (v.background as? GradientDrawable)?.setColor(color)
    }

    /** 设置行：左边标题 + 说明，右边控件 */
    private fun settingRow(title: String, desc: String?, control: View, descView: TextView? = null): View {
        val row = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            gravity = Gravity.CENTER_VERTICAL
            setPadding(0, dp(6), 0, dp(6))
        }
        val col = LinearLayout(this).apply { orientation = LinearLayout.VERTICAL }
        col.addView(label(title, 15f, TEXT))
        if (desc != null) col.addView(label(desc, 12f, SUB))
        if (descView != null) col.addView(descView)
        row.addView(col, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        if (control.layoutParams == null) row.addView(control, LinearLayout.LayoutParams(-2, -2).apply { leftMargin = dp(12) })
        else row.addView(control)
        return row
    }

    /** 开关（自己画的胶囊开关，配色统一） */
    private fun toggle(get: () -> Boolean, onChange: (Boolean) -> Unit): View {
        val track = FrameLayout(this)
        val knob = View(this).apply { background = GradientDrawable().apply { shape = GradientDrawable.OVAL; setColor(Color.WHITE) } }
        track.addView(knob, FrameLayout.LayoutParams(dp(22), dp(22)).apply { topMargin = dp(3) })
        val bg = GradientDrawable().apply { cornerRadius = dpf(14f) }
        track.background = bg
        track.layoutParams = LinearLayout.LayoutParams(dp(50), dp(28)).apply { leftMargin = dp(12) }
        fun paint() {
            val on = get()
            bg.setColor(if (on) PINK else STROKE)
            (knob.layoutParams as FrameLayout.LayoutParams).leftMargin = if (on) dp(25) else dp(3)
            knob.requestLayout()
        }
        track.setOnClickListener { onChange(!get()); paint() }
        paint()
        refreshers += { paint() }
        return track
    }

    /** 分段选择按钮（选中的填充霓虹色） */
    private fun segmented(options: List<Pair<Int, String>>, get: () -> Int, onPick: (Int) -> Unit): View {
        val row = LinearLayout(this).apply {
            orientation = LinearLayout.HORIZONTAL
            setPadding(dp(3), dp(3), dp(3), dp(3))
            background = GradientDrawable().apply { setColor(BG); cornerRadius = dpf(14f); setStroke(dp(1), STROKE) }
        }
        val items = ArrayList<Pair<Int, TextView>>()
        fun paint() {
            val cur = get()
            for ((v, tv) in items) {
                val sel = v == cur
                tv.setTextColor(if (sel) Color.WHITE else SUB)
                tv.typeface = if (sel) Typeface.DEFAULT_BOLD else Typeface.DEFAULT
                tv.background = if (sel) GradientDrawable(GradientDrawable.Orientation.LEFT_RIGHT, intArrayOf(PINK, PURPLE)).apply {
                    cornerRadius = dpf(11f)
                } else null
            }
        }
        for ((v, name) in options) {
            val tv = TextView(this).apply {
                text = name
                textSize = 14f
                gravity = Gravity.CENTER
                setPadding(dp(4), dp(9), dp(4), dp(9))
                setOnClickListener { onPick(v); paint() }
            }
            items += v to tv
            row.addView(tv, LinearLayout.LayoutParams(0, ViewGroup.LayoutParams.WRAP_CONTENT, 1f))
        }
        paint()
        refreshers += { paint() }
        return row
    }

    // ================= 刷新 =================

    private fun refresh() {
        for (r in refreshers) r()
        if (::mainPages.isInitialized) pageRefreshers[mainPages.selectedPage].forEach { it() }
        if (::pvRaw.isInitialized && pvRaw.isShown && MapFeeder.previewVer != shownPv) {
            shownPv = MapFeeder.previewVer
            MapFeeder.previewRaw?.let { pvRaw.setImageBitmap(it) }
            MapFeeder.previewCls?.let { pvCls.setImageBitmap(it) }
        }
        if(uiResumed)handler.postDelayed(refreshTask,300)
    }

    override fun onResume() {
        super.onResume()
        BleLink.resetScanBackoff()
        NavService.updateWakeLock()
        uiResumed=true;handler.removeCallbacks(refreshTask);refresh()
        if(!FusionFirstUse.returning(this))FusionCarUi.closeConsole(this)
        if (searchInput?.text.isNullOrBlank()) refreshHomeIdle?.invoke()     // v6.3 选路线页回来：更新“最近去过”
        if(FusionFirstUse.consumeReturn(this))handler.post {
            if(isFinishing || isDestroyed)return@post
            android.app.AlertDialog.Builder(this)
                .setTitle("首次授权完成了吗？")
                .setMessage("在车机界面阅读使用条款并自行确认，按提示授予定位等权限。看到地图首页后，点“已完成”；以后将直接使用 MotoNav。")
                .setPositiveButton("已完成") { _,_->
                    FusionFirstUse.finish(this)
                    if(!Keys.allFilled(this))showKeyDialog(first=true)
                }
                .setNeutralButton("继续授权") { _,_->openProviderSetup() }
                .setNegativeButton("稍后") { _,_->
                    FusionFirstUse.later(this)
                    if(!Keys.allFilled(this))showKeyDialog(first=true)
                }
                .setCancelable(false).show()
        }
    }

    private fun openProviderSetup() {
        runCatching{FusionFirstUse.open(this)}.onFailure{toast("授权界面暂未打开，可在首页重试")}
    }

    override fun onPause() {
        uiResumed=false;handler.removeCallbacks(refreshTask)
        super.onPause()
    }

    override fun onDestroy() {
        handler.removeCallbacksAndMessages(null)
        super.onDestroy()
    }

    // ================= 权限 =================

    private fun neededPermissions(): List<String> {
        val perms = mutableListOf(
            Manifest.permission.ACCESS_FINE_LOCATION,
            Manifest.permission.ACCESS_COARSE_LOCATION
        )
        if (Build.VERSION.SDK_INT >= 31) {
            perms += Manifest.permission.BLUETOOTH_SCAN
            perms += Manifest.permission.BLUETOOTH_CONNECT
        }
        if (Build.VERSION.SDK_INT >= 33) {
            perms += Manifest.permission.POST_NOTIFICATIONS
        }
        return perms
    }

    /** 启动后台服务必需的权限（通知权限不是必需的） */
    private fun hasPermissions(): Boolean = neededPermissions()
        .filter { it != Manifest.permission.POST_NOTIFICATIONS }
        .all { checkSelfPermission(it) == PackageManager.PERMISSION_GRANTED }

    private fun requestAllPermissions() {
        // v7.5 来电显示开着时，电话相关权限一起要（以前只在拨开关时才要，重装后容易漏掉）
        val p = neededPermissions().toMutableList()
        if (CallWatcher.enabled) p += CallWatcher.PERMS
        requestPermissions(p.toTypedArray(), 1)
    }

    override fun onRequestPermissionsResult(requestCode: Int, permissions: Array<out String>, grantResults: IntArray) {
        super.onRequestPermissionsResult(requestCode, permissions, grantResults)
        if (requestCode == 7) {                                  // v5.0 来电显示的权限
            CallWatcher.start(this)
            Toast.makeText(this, CallWatcher.permsText(this), Toast.LENGTH_LONG).show()
            return
        }
        CallWatcher.start(this)                                  // v7.5 电话权限可能刚给：重新挂上来电监听
        if (hasPermissions()) {
            NavService.start(this)
        } else {
            BleLink.status = "权限没有给全：位置和「附近的设备」都要允许"
        }
    }

    // ================= 电池优化 =================

    private fun isIgnoringBatteryOptimization(): Boolean {
        val pm = getSystemService(POWER_SERVICE) as PowerManager
        return pm.isIgnoringBatteryOptimizations(packageName)
    }

    @SuppressLint("BatteryLife")
    private fun requestIgnoreBatteryOptimization() {
        if (isIgnoringBatteryOptimization()) return
        try {
            startActivity(
                Intent(Settings.ACTION_REQUEST_IGNORE_BATTERY_OPTIMIZATIONS, Uri.parse("package:$packageName"))
            )
        } catch (_: Exception) {
            // 个别系统不支持直接弹窗，就打开电池优化列表让你手动找 MotoNav
            startActivity(Intent(Settings.ACTION_IGNORE_BATTERY_OPTIMIZATION_SETTINGS))
        }
    }
}
