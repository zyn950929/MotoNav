package com.yuminaga.motonav

import android.graphics.Bitmap
import android.os.Handler
import android.os.Looper
import java.util.concurrent.Executors

/**
 * 负责复杂路口放大图的缩放、转码与 BLE 推送管理。
 */
object CrossFeeder {
    private const val TARGET_MAX_W = 256
    private const val TARGET_MAX_H = 160

    private val worker = Executors.newSingleThreadExecutor()
    private val main = Handler(Looper.getMainLooper())
    private var crossId = 0
    private var revision = 0L
    @Volatile var isShowing = false
        private set

    fun sendCross(bitmap: Bitmap) {
        if (!BleLink.crossSupported || bitmap.isRecycled) return
        val expectedRevision = ++revision
        isShowing = true
        val origW = bitmap.width
        val origH = bitmap.height
        if (origW <= 0 || origH <= 0) return

        // 计算等比缩放尺寸
        val scale = minOf(TARGET_MAX_W.toFloat() / origW, TARGET_MAX_H.toFloat() / origH, 1.0f)
        val dstW = ((origW * scale).toInt().coerceIn(64, TARGET_MAX_W) and -2) // 偶数宽
        val dstH = (origH * scale).toInt().coerceIn(40, TARGET_MAX_H)

        val scaled = Bitmap.createScaledBitmap(bitmap, dstW, dstH, true)
        val argb = IntArray(dstW * dstH)
        scaled.getPixels(argb, 0, dstW, 0, 0, dstW, dstH)
        if (scaled !== bitmap) scaled.recycle()

        worker.execute {
            val encoded = CrossPacket.encode(argb, dstW, dstH) ?: return@execute
            main.post {
                if (!isShowing || revision != expectedRevision || !BleLink.crossSupported) return@post
                crossId = (crossId + 1) and 0xFF
                BleLink.sendBulk(CrossPacket.TYPE_CROSS, crossId, encoded.wireBytes)
                BleLink.send("#X,1")
            }
        }
    }

    fun hideCross() {
        revision++
        isShowing = false
        BleLink.cancelBulk(CrossPacket.TYPE_CROSS)
        if (BleLink.crossSupported) BleLink.send("#X,0")
    }
}
