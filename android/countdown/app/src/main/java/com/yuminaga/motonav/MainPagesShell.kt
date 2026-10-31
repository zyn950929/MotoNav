package com.yuminaga.motonav

import android.graphics.Canvas
import android.graphics.Paint
import android.graphics.Path
import android.graphics.RectF
import android.os.Bundle
import android.view.Gravity
import android.view.View
import android.view.ViewGroup
import android.widget.FrameLayout
import android.widget.LinearLayout
import android.widget.ScrollView
import android.widget.TextView
import androidx.appcompat.app.AppCompatActivity
import androidx.core.view.ViewCompat
import androidx.core.view.WindowCompat
import androidx.core.view.WindowInsetsCompat
import kotlin.math.cos
import kotlin.math.sin

/** Five independent, lazily created pages; bottom navigation never scrolls away. */
class MainPagesShell(private val activity:AppCompatActivity, saved:Bundle?,
                     private val build:(Int)->View, private val onSelected:(Int)->Unit):LinearLayout(activity) {
    private val S=NavStyle
    private fun dp(v:Int)=S.dpi(activity,v)
    private val titles=listOf("首页搜索","圆屏显示","导航语音","圆屏设置","地图 Key")
    private val host=FrameLayout(activity)
    private val pages=arrayOfNulls<ScrollView>(5)
    private val restoreY=saved?.getIntArray("main_page_scroll")?.takeIf{it.size==5} ?: IntArray(5)
    private val tabs=ArrayList<LinearLayout>()
    private val icons=ArrayList<PageIcon>()
    private val labels=ArrayList<TextView>()
    private val indicators=ArrayList<FrameLayout>()
    var selectedPage=0;private set

    init {
        orientation=VERTICAL;setBackgroundColor(S.BG)
        WindowCompat.setDecorFitsSystemWindows(activity.window,false)
        addView(host,LayoutParams(-1,0,1f))
        val navigation=LinearLayout(activity).apply {
            orientation=HORIZONTAL;setPadding(dp(5),dp(5),dp(5),dp(5));setBackgroundColor(S.CARD_SOLID)
            elevation=S.dp(activity,10f)
        }
        titles.forEachIndexed { index,title ->
            val tab=LinearLayout(activity).apply {
                orientation=VERTICAL;gravity=Gravity.CENTER;isFocusable=true
                contentDescription=title;minimumHeight=dp(58)
                setOnClickListener{showPage(index)}
            }
            val indicator=FrameLayout(activity)
            val icon=PageIcon(index)
            indicator.addView(icon,FrameLayout.LayoutParams(dp(23),dp(23),Gravity.CENTER))
            tab.addView(indicator,LayoutParams(dp(44),dp(29)))
            val label=TextView(activity).apply {
                text=title;textSize=11.5f;setTextColor(S.SUB);gravity=Gravity.CENTER
                isSingleLine=true;includeFontPadding=false;setPadding(0,dp(4),0,0)
            }
            tab.addView(label,LayoutParams(-1,-2))
            navigation.addView(tab,LayoutParams(0,dp(58),1f))
            tabs+=tab;icons+=icon;labels+=label;indicators+=indicator
        }
        // 底部导航栏上方一条细分隔线
        addView(View(activity).apply{setBackgroundColor(S.STROKE)},LayoutParams(-1,maxOf(1,dp(1)/2)))
        addView(navigation,LayoutParams(-1,-2))
        ViewCompat.setOnApplyWindowInsetsListener(this){view,insets->
            val bars=insets.getInsets(WindowInsetsCompat.Type.systemBars() or WindowInsetsCompat.Type.displayCutout())
            val ime=insets.getInsets(WindowInsetsCompat.Type.ime())
            view.setPadding(bars.left,bars.top,bars.right,maxOf(bars.bottom,ime.bottom));insets
        }
    }

    fun showPage(index:Int) {
        if(index !in 0..4)return
        pages[selectedPage]?.visibility=GONE
        selectedPage=index
        val page=pages[index] ?: ScrollView(activity).apply {
            id=View.generateViewId();isFillViewport=true;clipToPadding=true
            addView(build(index),FrameLayout.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT,ViewGroup.LayoutParams.WRAP_CONTENT))
            host.addView(this,FrameLayout.LayoutParams(-1,-1));pages[index]=this
            post{scrollTo(0,restoreY[index])}
        }
        page.visibility=VISIBLE
        tabs.forEachIndexed{i,tab->
            val selected=i==index;tab.isSelected=selected
            labels[i].setTextColor(if(selected)S.PINK else S.SUB)
            labels[i].typeface=if(selected)S.BOLD else S.MEDIUM
            indicators[i].background=if(selected)S.round(activity,0xFF351E38.toInt(),14f) else null
            icons[i].active=selected
        }
        onSelected(index)
    }

    fun save(out:Bundle) {
        out.putInt("main_page_selected",selectedPage)
        out.putIntArray("main_page_scroll",IntArray(5){pages[it]?.scrollY ?: restoreY[it]})
    }

    private inner class PageIcon(private val index:Int):View(activity) {
        var active=false
            set(value){if(field!=value){field=value;invalidate()}}
        private val paint=Paint(Paint.ANTI_ALIAS_FLAG).apply {
            style=Paint.Style.STROKE;strokeWidth=1.8f;strokeCap=Paint.Cap.ROUND;strokeJoin=Paint.Join.ROUND
        }
        override fun onDraw(canvas:Canvas) {
            super.onDraw(canvas)
            canvas.save();canvas.scale(width/24f,height/24f);paint.color=if(active)S.PINK else S.SUB
            fun line(x1:Float,y1:Float,x2:Float,y2:Float)=canvas.drawLine(x1,y1,x2,y2,paint)
            when(index) {
                0->{
                    canvas.drawPath(Path().apply{moveTo(3f,10f);lineTo(12f,3f);lineTo(21f,10f)},paint)
                    canvas.drawPath(Path().apply{moveTo(6f,9f);lineTo(6f,21f);lineTo(18f,21f);lineTo(18f,9f)},paint)
                    canvas.drawPath(Path().apply{moveTo(10f,21f);lineTo(10f,14f);lineTo(14f,14f);lineTo(14f,21f)},paint)
                }
                1->{
                    canvas.drawCircle(12f,12f,9f,paint)
                    canvas.drawPath(Path().apply{moveTo(7f,16f);lineTo(10f,13f);lineTo(14f,14f);lineTo(17f,8f)},paint)
                    canvas.drawCircle(17f,8f,1f,paint)
                }
                2->{
                    canvas.drawPath(Path().apply{moveTo(4f,9f);lineTo(8f,9f);lineTo(12f,5f);lineTo(12f,19f);lineTo(8f,15f);lineTo(4f,15f);close()},paint)
                    canvas.drawArc(RectF(10f,7f,20f,17f),-55f,110f,false,paint)
                    canvas.drawArc(RectF(7f,3f,24f,21f),-50f,100f,false,paint)
                }
                3->{
                    canvas.drawCircle(12f,12f,6.5f,paint);canvas.drawCircle(12f,12f,2.5f,paint)
                    repeat(8){val a=it*Math.PI/4;line((12+6.5*cos(a)).toFloat(),(12+6.5*sin(a)).toFloat(),(12+9*cos(a)).toFloat(),(12+9*sin(a)).toFloat())}
                }
                4->{
                    canvas.drawCircle(8f,8f,4.5f,paint);line(11f,11f,21f,21f)
                    line(16f,16f,19f,13f);line(19f,19f,22f,16f)
                }
            }
            canvas.restore()
        }
    }
}
