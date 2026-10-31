package com.yuminaga.motonav;

import java.util.Arrays;
import java.util.zip.Inflater;

public final class CrossPacketTest {
    public static void main(String[] args) throws Exception {
        int w = 240, h = 140;
        int[] pixels = new int[w * h];
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                // 模拟立交桥与天空分歧场景
                if (y < 40) pixels[y * w + x] = 0xFF5B92E5; // 天空蓝
                else if (Math.abs(x - w / 2) < 20 || (y > 80 && Math.abs(x - (w / 2 + (y - 80))) < 15)) {
                    pixels[y * w + x] = 0xFFFFFFFF; // 道路标线白
                } else if (y > 40 && y < 120 && Math.abs(x - w / 2) < 60) {
                    pixels[y * w + x] = 0xFF4A4A4A; // 沥青路面深灰
                } else {
                    pixels[y * w + x] = 0xFF2D5E2D; // 绿化带深绿
                }
            }
        }

        CrossPacket.Encoded enc = CrossPacket.encode(pixels, w, h);
        if (enc == null) throw new AssertionError("CrossPacket.encode returned null");
        if (enc.width != w || enc.height != h) throw new AssertionError("Dimensions mismatch");
        if (enc.wireBytes.length >= w * h) {
            throw new AssertionError("Compressed cross bytes should be much smaller than raw RGB: " + enc.wireBytes.length);
        }

        byte[] wire = enc.wireBytes;
        int parsedW = (wire[0] & 255) | ((wire[1] & 255) << 8);
        int parsedH = (wire[2] & 255) | ((wire[3] & 255) << 8);
        int flags = (wire[4] & 255) | ((wire[5] & 255) << 8);
        if (parsedW != w || parsedH != h) throw new AssertionError("Parsed dimensions mismatch");
        if ((flags & CrossPacket.FLAG_DEFLATE) == 0 || (flags & CrossPacket.FLAG_PALETTE) == 0) {
            throw new AssertionError("Missing deflate or palette flag");
        }

        // 解压 Deflate 并还原 4-bit RLE
        int rawLen = (wire[38] & 255) | ((wire[39] & 255) << 8) | ((wire[40] & 255) << 16) | ((wire[41] & 255) << 24);
        byte[] rle = new byte[rawLen];
        Inflater inf = new Inflater();
        inf.setInput(wire, 42, wire.length - 42);
        int got = inf.inflate(rle);
        inf.end();
        if (got != rawLen) throw new AssertionError("Inflated length mismatch: " + got + " != " + rawLen);

        int pos = 0, pxCount = 0;
        byte[] decodedIdx = new byte[w * h];
        while (pos < rle.length && pxCount < w * h) {
            int b = rle[pos++] & 255;
            int c = b & 15;
            int run = (b >> 4) + 1;
            if ((b >> 4) == 15 && pos < rle.length) run = 16 + (rle[pos++] & 255);
            for (int k = 0; k < run && pxCount < w * h; k++) {
                decodedIdx[pxCount++] = (byte) c;
            }
        }
        if (pxCount != w * h) throw new AssertionError("Decoded pixels count mismatch: " + pxCount);

        System.out.printf("PASS: CrossPacketTest (raw RGB=%dB, encoded wire=%dB, ratio=%.1f%%)%n",
            w * h * 4, wire.length, wire.length * 100.0 / (w * h * 4));
    }
}
