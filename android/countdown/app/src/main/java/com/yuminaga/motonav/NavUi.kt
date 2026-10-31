package com.yuminaga.motonav

import android.content.Context
import android.graphics.Canvas
import android.graphics.Color
import android.graphics.LinearGradient
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.graphics.Shader
import android.graphics.Typeface
import android.graphics.drawable.GradientDrawable
import android.view.View
import kotlin.math.cos
import kotlin.math.max
import kotlin.math.min
import kotlin.math.sin

/**
 * 版本：跟随 App 版本，见 MainActivity.kt 里的 APP_VERSION（v3.0 新增）
 *
 * 自己的导航界面（选路线页、导航页）共用的配色和小部件：
 * - NavStyle：罪恶都市风配色（和主界面一致）、圆角卡片、渐变按钮
 * - TurnArrowView：转向箭头（角度跟随路口实际角度，和圆屏一样）
 * - LaneView：车道指示
 */
object NavStyle {
    val BG = Color.parseColor("#0C0A11")
    val CARD = Color.parseColor("#F216131D")        // 浮在地图上的卡片（略透明）
    val CARD_SOLID = Color.parseColor("#16131D")
    val CARD2 = Color.parseColor("#251C33")
    val STROKE = Color.parseColor("#3A2E4C")
    val TEXT = Color.parseColor("#F3EEF8")
    val SUB = Color.parseColor("#A79DB6")
    val PINK = Color.parseColor("#FF4FB4")
    val PURPLE = Color.parseColor("#8E5CFF")
    val CYAN = Color.parseColor("#43E1F0")
    val AMBER = Color.parseColor("#FFB547")
    val RED = Color.parseColor("#FF4D5E")
    val GREEN = Color.parseColor("#3DDC84")

    val NUM: Typeface = Typeface.create("sans-serif-condensed", Typeface.BOLD)
    val BOLD: Typeface = Typeface.create("sans-serif", Typeface.BOLD)
    val MEDIUM: Typeface = Typeface.create("sans-serif-medium", Typeface.NORMAL)

    fun dp(ctx: Context, v: Float) = v * ctx.resources.displayMetrics.density
    fun dpi(ctx: Context, v: Int) = (v * ctx.resources.displayMetrics.density + 0.5f).toInt()

    /** 圆角卡片背景 */
    fun round(ctx: Context, color: Int, radiusDp: Float, strokeColor: Int = 0, strokeDp: Float = 1f) =
        GradientDrawable().apply {
            setColor(color)
            cornerRadius = dp(ctx, radiusDp)
            if (strokeColor != 0) setStroke(max(1, dp(ctx, strokeDp).toInt()), strokeColor)
        }

    /** 粉→紫渐变按钮背景 */
    fun gradient(ctx: Context, radiusDp: Float) = GradientDrawable(
        GradientDrawable.Orientation.LEFT_RIGHT, intArrayOf(PINK, PURPLE)
    ).apply { cornerRadius = dp(ctx, radiusDp) }

    /** 秒 → “1小时5分” / “25分钟” */
    fun durText(s: Int): String {
        val m = (s + 59) / 60
        return if (m >= 60) "${m / 60}小时${if (m % 60 > 0) "${m % 60}分" else ""}" else "${max(1, m)}分钟"
    }

    /** 米 → “12.3公里” / “800米” */
    fun distText(m: Int): String = when {
        m >= 100_000 -> "${m / 1000}公里"
        m >= 1000 -> "%.1f公里".format(m / 1000.0)
        else -> "${m}米"
    }

    /** 转向距离：数字和单位分开（大数字 + 小单位） */
    fun distParts(m: Int): Pair<String, String> = when {
        m < 0 -> "--" to ""
        m >= 10_000 -> "${m / 1000}" to "公里"
        m >= 1000 -> "%.1f".format(m / 1000.0) to "公里"
        m >= 100 -> "${(m + 5) / 10 * 10}" to "米"
        else -> "$m" to "米"
    }
}

/** 转向箭头：白色粗线 + 箭头，深色描边；角度跟随路口实际角度 */
class TurnArrowView(ctx: Context) : View(ctx) {
    var dir = "S"
        set(v) { if (field != v) { field = v; invalidate() } }
    var angle: Int? = null
        set(v) { if (field != v) { field = v; invalidate() } }
    var color = Color.WHITE
        set(v) { if (field != v) { field = v; invalidate() } }

    private val line = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE; strokeCap = Paint.Cap.ROUND; strokeJoin = Paint.Join.ROUND
    }
    private val fill = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL; strokeJoin = Paint.Join.ROUND }

    override fun onDraw(c: Canvas) {
        val s = min(width, height).toFloat()
        if (s <= 0) return
        // 1. 在 0~1 的方格里描述图形：一条折线 + 末端箭头
        val pts = ArrayList<FloatArray>()
        var hx = 0f; var hy = 0f; var hdx = 0f; var hdy = -1f
        var ring = false
        when (dir) {
            "A" -> ring = true
            "U" -> {
                pts += floatArrayOf(0.64f, 0.95f); pts += floatArrayOf(0.64f, 0.42f)
                // 上方半圆弧
                for (i in 1..10) {
                    val a = Math.PI * i / 10
                    pts += floatArrayOf((0.49f + 0.15f * cos(a)).toFloat(), (0.42f - 0.15f * sin(a)).toFloat())
                }
                pts += floatArrayOf(0.34f, 0.58f)
                hx = 0.34f; hy = 0.58f; hdx = 0f; hdy = 1f
            }
            "L", "R", "SL", "SR" -> {
                val def = when (dir) { "L" -> -90; "R" -> 90; "SL" -> -45; else -> 45 }
                val a = Math.toRadians((angle ?: def).toDouble())
                val dx = sin(a).toFloat(); val dy = -cos(a).toFloat()
                // 锐角弯（超过 90 度）：拐点抬高、出口加长，免得箭头和竖线挤在一起
                val sharp = max(0f, (Math.abs(Math.toDegrees(a)).toFloat() - 90f) / 60f)
                val jx = 0.5f; val jy = 0.55f - 0.3f * sharp
                val len = 0.32f + 0.08f * sharp
                pts += floatArrayOf(jx, jy + 0.43f + 0.1f * sharp); pts += floatArrayOf(jx, jy)
                hx = jx + dx * len; hy = jy + dy * len; hdx = dx; hdy = dy
                pts += floatArrayOf(hx, hy)
            }
            else -> {
                pts += floatArrayOf(0.5f, 0.95f); pts += floatArrayOf(0.5f, 0.3f)
                hx = 0.5f; hy = 0.3f; hdx = 0f; hdy = -1f
            }
        }
        val hl = 0.2f                        // 箭头长度
        val hw = 0.19f                       // 箭头半宽
        val nx = -hdy; val ny = hdx
        val tip = floatArrayOf(hx + hdx * hl, hy + hdy * hl)
        val b1 = floatArrayOf(hx + nx * hw, hy + ny * hw)
        val b2 = floatArrayOf(hx - nx * hw, hy - ny * hw)
        // 2. 算外框，缩放到控件中间
        var x0 = 9f; var y0 = 9f; var x1 = -9f; var y1 = -9f
        val all = if (ring) listOf(floatArrayOf(0.15f, 0.1f), floatArrayOf(0.85f, 0.9f)) else pts + listOf(tip, b1, b2)
        for (p in all) { x0 = min(x0, p[0]); x1 = max(x1, p[0]); y0 = min(y0, p[1]); y1 = max(y1, p[1]) }
        val sw = 0.13f
        val k = 0.86f * s / max(x1 - x0 + sw, y1 - y0 + sw)
        val ox = (width - (x1 + x0) * k) / 2; val oy = (height - (y1 + y0) * k) / 2
        fun X(v: Float) = ox + v * k
        fun Y(v: Float) = oy + v * k

        for (pass in 0..1) {
            val col = if (pass == 0) Color.parseColor("#CC0A0610") else color
            val extra = if (pass == 0) 0.07f * k else 0f
            line.color = col; fill.color = col
            line.strokeWidth = sw * k + extra
            if (ring) {
                // 目的地：圆环 + 中心点
                c.drawCircle(X(0.5f), Y(0.5f), 0.3f * k, line)
                fill.style = Paint.Style.FILL
                c.drawCircle(X(0.5f), Y(0.5f), 0.11f * k + extra / 2, fill)
                continue
            }
            val path = Path()
            path.moveTo(X(pts[0][0]), Y(pts[0][1]))
            for (i in 1 until pts.size) path.lineTo(X(pts[i][0]), Y(pts[i][1]))
            c.drawPath(path, line)
            val h = Path()
            h.moveTo(X(tip[0]), Y(tip[1])); h.lineTo(X(b1[0]), Y(b1[1])); h.lineTo(X(b2[0]), Y(b2[1])); h.close()
            if (pass == 0) { fill.style = Paint.Style.FILL_AND_STROKE; fill.strokeWidth = extra } else fill.style = Paint.Style.FILL
            c.drawPath(h, fill)
        }
    }
}

/** 车道指示：每条车道一个圆角格子，推荐车道白色，其他灰色 */
class LaneView(ctx: Context) : View(ctx) {
    var lanes: IntArray = IntArray(0)          // 两个一组：背景码、推荐码（255 = 不推荐）
        set(v) { if (!field.contentEquals(v)) { field = v; requestLayout(); invalidate() } }

    private val p = Paint(Paint.ANTI_ALIAS_FLAG).apply {
        style = Paint.Style.STROKE; strokeCap = Paint.Cap.ROUND; strokeJoin = Paint.Join.ROUND
    }
    private val f = Paint(Paint.ANTI_ALIAS_FLAG).apply { style = Paint.Style.FILL }
    private val box = Paint(Paint.ANTI_ALIAS_FLAG)

    /** 高德车道方向码 → 方向位：1 直行，2 左转，4 右转，8 左掉头，16 右掉头（和圆屏固件同一张表） */
    private fun flags(code: Int): Int = when (code) {
        0 -> 1; 1 -> 2; 2 -> 1 or 2; 3 -> 4; 4 -> 1 or 4; 5 -> 8; 6 -> 2 or 4; 7 -> 1 or 2 or 4
        8 -> 16; 9 -> 1 or 8; 10 -> 1 or 16; 11 -> 2 or 8; 12 -> 4 or 16; else -> 1
    }

    override fun onMeasure(wSpec: Int, hSpec: Int) {
        val h = MeasureSpec.getSize(hSpec)
        val n = lanes.size / 2
        val cell = h * 0.78f
        val want = (n * cell + (n - 1).coerceAtLeast(0) * h * 0.08f).toInt()
        setMeasuredDimension(min(want, MeasureSpec.getSize(wSpec)).coerceAtLeast(0), h)
    }

    override fun onDraw(c: Canvas) {
        val n = lanes.size / 2
        if (n == 0) return
        val h = height.toFloat()
        val gap = h * 0.08f
        val cw = min(h * 0.78f, (width - gap * (n - 1)) / n)
        val r = RectF()
        for (i in 0 until n) {
            val bg = lanes[2 * i]; val fr = lanes[2 * i + 1]
            val rec = fr in 0..14
            val x = i * (cw + gap)
            r.set(x, 0f, x + cw, h)
            box.color = if (rec) Color.parseColor("#33FFFFFF") else Color.parseColor("#14FFFFFF")
            c.drawRoundRect(r, h * 0.18f, h * 0.18f, box)
            val col = if (rec) Color.WHITE else Color.parseColor("#66FFFFFF")
            glyph(c, x + cw / 2, h, cw, flags(if (bg in 0..14) bg else 0), col)
        }
    }

    private fun glyph(c: Canvas, cx: Float, h: Float, w: Float, fl: Int, col: Int) {
        p.color = col; f.color = col
        p.strokeWidth = w * 0.11f
        val base = h * 0.86f; val mid = h * 0.55f; val top = h * 0.2f
        val hs = w * 0.17f
        c.drawLine(cx, base, cx, mid, p)
        if (fl and 1 != 0) { c.drawLine(cx, mid, cx, top + hs, p); head(c, cx, top, 0f, -1f, hs) }
        val arm = w * 0.27f
        val ty = if (fl and 1 != 0) h * 0.6f else h * 0.42f
        if (fl and (2 or 4 or 8 or 16) != 0) c.drawLine(cx, base, cx, ty, p)
        if (fl and 2 != 0) { c.drawLine(cx, ty, cx - arm + hs, ty, p); head(c, cx - arm, ty, -1f, 0f, hs) }
        if (fl and 4 != 0) { c.drawLine(cx, ty, cx + arm - hs, ty, p); head(c, cx + arm, ty, 1f, 0f, hs) }
        if (fl and 8 != 0) {       // 左掉头：向上、向左弯、向下
            val path = Path(); path.moveTo(cx, ty); path.quadTo(cx, top, cx - arm * 0.6f, top); path.quadTo(cx - arm, top, cx - arm, ty - hs)
            c.drawPath(path, p); head(c, cx - arm, ty, 0f, 1f, hs)
        }
        if (fl and 16 != 0) {
            val path = Path(); path.moveTo(cx, ty); path.quadTo(cx, top, cx + arm * 0.6f, top); path.quadTo(cx + arm, top, cx + arm, ty - hs)
            c.drawPath(path, p); head(c, cx + arm, ty, 0f, 1f, hs)
        }
    }

    /** 箭头：尖端在 (x, y)，方向 (dx, dy) */
    private fun head(c: Canvas, x: Float, y: Float, dx: Float, dy: Float, s: Float) {
        val bx = x - dx * s; val by = y - dy * s
        val nx = -dy * s * 0.8f; val ny = dx * s * 0.8f
        val path = Path(); path.moveTo(x, y); path.lineTo(bx + nx, by + ny); path.lineTo(bx - nx, by - ny); path.close()
        c.drawPath(path, f)
    }
}

/** 小工具：圆角渐变描边（选中的路线卡片用） */
object GlowBorder {
    fun make(ctx: Context, radiusDp: Float): GradientDrawable = GradientDrawable().apply {
        setColor(NavStyle.CARD2)
        cornerRadius = NavStyle.dp(ctx, radiusDp)
        setStroke(NavStyle.dpi(ctx, 2), NavStyle.PINK)
    }
}

/** 文字渐变（标题用） */
fun gradientShader(width: Float): Shader =
    LinearGradient(0f, 0f, width, 0f, NavStyle.PINK, NavStyle.CYAN, Shader.TileMode.CLAMP)

/**
 * 反射调用：高德 SDK 不同版本里有些方法名字/参数不一样，用这个“试着调用”，
 * 找不到就返回 null，不会让 App 崩溃，也不会因为 SDK 版本差一点就编译不过。
 */
object Refl {
    private fun box(c: Class<*>): Class<*> = when (c) {
        java.lang.Boolean.TYPE -> java.lang.Boolean::class.java
        Integer.TYPE -> Integer::class.java
        java.lang.Long.TYPE -> java.lang.Long::class.java
        java.lang.Float.TYPE -> java.lang.Float::class.java
        java.lang.Double.TYPE -> java.lang.Double::class.java
        else -> c
    }

    /** 调用 obj 上名为 name、参数个数和类型对得上的方法；成功返回 Pair(true, 返回值) */
    fun call(obj: Any?, name: String, vararg args: Any?): Pair<Boolean, Any?> {
        if (obj == null) return false to null
        for (m in obj.javaClass.methods) {
            if (m.name != name || m.parameterTypes.size != args.size) continue
            var ok = true
            for (i in args.indices) {
                val a = args[i] ?: continue
                if (!box(m.parameterTypes[i]).isAssignableFrom(a.javaClass)) { ok = false; break }
            }
            if (!ok) continue
            return try { true to m.invoke(obj, *args) } catch (_: Exception) { false to null }
        }
        return false to null
    }

    fun int(obj: Any?, vararg names: String): Int? {
        for (n in names) { val r = call(obj, n); if (r.first && r.second is Number) return (r.second as Number).toInt() }
        return null
    }

    fun str(obj: Any?, vararg names: String): String? {
        for (n in names) { val r = call(obj, n); if (r.first && r.second is String) return r.second as String }
        return null
    }
}
