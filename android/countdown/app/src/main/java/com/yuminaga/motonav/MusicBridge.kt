package com.yuminaga.motonav

import android.content.ComponentName
import android.content.Context
import android.media.MediaMetadata
import android.media.session.MediaController
import android.media.session.MediaSessionManager
import android.media.session.PlaybackState
import android.os.Handler
import android.os.Looper
import android.os.SystemClock
import android.view.KeyEvent

/** 通过系统媒体会话同步歌曲、遥控播放，不传输音频。 */
object MusicBridge {
    private val handler = Handler(Looper.getMainLooper())
    private var ctx: Context? = null
    private var controllers = emptyList<MediaController>()
    private var current: MediaController? = null
    private var lastText = ""
    private var lastState = ""
    @Volatile var info = "等待圆屏音乐操作"
    private var operation = 0
    private val callback = object : MediaController.Callback() {
        override fun onMetadataChanged(metadata: MediaMetadata?) { publish() }
        override fun onPlaybackStateChanged(state: PlaybackState?) { publish() }
        override fun onSessionDestroyed() { refresh() }
    }
    fun init(context: Context) {
        if (ctx != null) return
        ctx = context.applicationContext
        handler.post(object : Runnable {
            override fun run() { refresh(); handler.postDelayed(this, 1500) }
        })
    }
    fun refresh(force: Boolean = false) {
        if (Looper.myLooper() != Looper.getMainLooper()) { handler.post { refresh(force) }; return }
        val c = ctx ?: return
        val next = if (!AmapProbe.notifEnabled(c)) emptyList() else try {
            val msm = c.getSystemService(Context.MEDIA_SESSION_SERVICE) as MediaSessionManager
            msm.getActiveSessions(ComponentName(c, AmapNotifyListener::class.java)).filter { it.packageName != c.packageName }
        } catch (_: SecurityException) { emptyList() }
        if (controllers.map { it.sessionToken } != next.map { it.sessionToken }) {
            controllers.forEach { try { it.unregisterCallback(callback) } catch (_: Exception) {} }
            controllers = next
            controllers.forEach { try { it.registerCallback(callback, handler) } catch (_: Exception) {} }
        }
        publish(force)
    }
    private fun publish(force: Boolean = false) {
        val c = ctx ?: return
        current = controllers.firstOrNull { it.playbackState?.state == PlaybackState.STATE_PLAYING }
            ?: controllers.firstOrNull { it.sessionToken == current?.sessionToken } ?: controllers.firstOrNull()
        val mc = current
        val meta = mc?.metadata
        val title = meta?.getString(MediaMetadata.METADATA_KEY_TITLE)?.takeIf { it.isNotBlank() }
            ?: meta?.getString(MediaMetadata.METADATA_KEY_DISPLAY_TITLE)?.takeIf { it.isNotBlank() }
            ?: if (mc != null) "未知歌曲" else if (AmapProbe.notifEnabled(c)) "暂无音乐播放" else "请开启 MotoNav 通知使用权"
        val artist = meta?.getString(MediaMetadata.METADATA_KEY_ARTIST)?.takeIf { it.isNotBlank() }
            ?: meta?.getString(MediaMetadata.METADATA_KEY_ALBUM_ARTIST)?.takeIf { it.isNotBlank() } ?: ""
        val state = mc?.playbackState
        val playing = state?.state == PlaybackState.STATE_PLAYING
        // 部分播放器漏报 actions；允许发标准媒体按键，不按这些位禁用按钮。
        val mask = if (mc != null) 7 else 0
        val status = "#Z,${if (mc != null) 1 else 0},${if (playing) 1 else 0},$mask"
        if (force || lastText != "$title\n$artist") {
            lastText = "$title\n$artist"
            for ((slot, text) in listOf(5 to title, 6 to artist)) {
                val image = TextImage.render(text.take(200), 360, if (slot == 5) 32f else 24f)
                BleLink.sendImage(slot, image?.w ?: 0, image?.h ?: 0, image?.data ?: byteArrayOf())
            }
        }
        if (force || lastState != status) { lastState = status; BleLink.send(status) }
    }
    private fun track(mc: MediaController): String {
        val m = mc.metadata
        return listOf(MediaMetadata.METADATA_KEY_MEDIA_ID, MediaMetadata.METADATA_KEY_TITLE,
            MediaMetadata.METADATA_KEY_ARTIST, MediaMetadata.METADATA_KEY_ALBUM).joinToString("|") { m?.getString(it) ?: "" }
    }

    fun onScreenEvent(action: String, touchInfo: String = "") {
        handler.post {
            if (action == "H") { info = "触摸未命中按钮：$touchInfo"; return@post }
            refresh(action == "Q")
            if (action == "Q") return@post
            if (action !in listOf("P", "N", "T")) return@post
            val mc = current
            if (mc == null) { info = "收到 $action，但没有可控制的播放器"; BleLink.sendEvent("#ZACK,5"); return@post }
            val beforeTrack = track(mc)
            val beforePosition = mc.playbackState?.position ?: -1L
            val beforePlaying = mc.playbackState?.state == PlaybackState.STATE_PLAYING
            val op = ++operation
            val name = when (action) { "P" -> "上一首"; "N" -> "下一首"; else -> "播放/暂停" }
            info = "收到$name · ${mc.packageName} · 触摸 $touchInfo"
            BleLink.sendEvent("#ZACK,2")
            try {
                val code = when (action) {
                    "P" -> KeyEvent.KEYCODE_MEDIA_PREVIOUS
                    "N" -> KeyEvent.KEYCODE_MEDIA_NEXT
                    else -> KeyEvent.KEYCODE_MEDIA_PLAY_PAUSE
                }
                val now = SystemClock.uptimeMillis()
                // 成对发送按下和抬起；发往所选会话，避免控制到别的播放器。
                val down = mc.dispatchMediaButtonEvent(KeyEvent(now, now, KeyEvent.ACTION_DOWN, code, 0))
                val up = mc.dispatchMediaButtonEvent(KeyEvent(now, SystemClock.uptimeMillis(), KeyEvent.ACTION_UP, code, 0))
                if (!down && !up) {
                    when (action) {
                        "P" -> mc.transportControls.skipToPrevious()
                        "N" -> mc.transportControls.skipToNext()
                        "T" -> if (beforePlaying) mc.transportControls.pause() else mc.transportControls.play()
                    }
                    info += " · 会话控制"
                } else info += " · 媒体按键"
                fun check(attempt: Int) {
                    if (operation != op) return
                    val after = mc.playbackState
                    val changed = if (action == "T")
                        after != null && (if (beforePlaying) (after.state == PlaybackState.STATE_PAUSED || after.state == PlaybackState.STATE_STOPPED) else after.state == PlaybackState.STATE_PLAYING)
                    else track(mc) != beforeTrack ||
                        (beforePosition > 2000 && after != null && after.position >= 0 && after.position < beforePosition - 1500)
                    if (changed) {
                        info += " · 播放器已响应"
                        BleLink.sendEvent("#ZACK,3")
                        refresh(true)
                    } else if (attempt < 5) handler.postDelayed({ check(attempt + 1) }, 300)
                    else {
                        info += " · 已发送，但未收到播放器状态变化"
                        BleLink.sendEvent("#ZACK,4")
                        refresh(true)
                    }
                }
                handler.postDelayed({ check(0) }, 300)
            } catch (e: Exception) {
                info += " · 控制失败：${e.javaClass.simpleName}"
                BleLink.sendEvent("#ZACK,4")
                refresh(true)
            }
        }
    }
}
