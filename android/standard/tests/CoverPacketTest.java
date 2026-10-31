package com.example.motonav;

import java.util.zip.Inflater;

public final class CoverPacketTest {
    public static void main(String[] args) throws Exception {
        int w = 180, h = 180;
        int[] pixels = new int[w * h];
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                // 模拟渐变与图案的专辑封面
                int cx = x - 70, cy = y - 70;
                int r2 = cx * cx + cy * cy;
                if (r2 < 40 * 40) {
                    pixels[y * w + x] = 0xFFFF4500; // 亮橙红
                } else if (r2 < 60 * 60) {
                    pixels[y * w + x] = 0xFF1E90FF; // 道奇蓝
                } else {
                    pixels[y * w + x] = 0xFF181820; // 深色黑底
                }
            }
        }

        CoverPacket.Encoded enc = CoverPacket.encode(pixels, w, h);
        if (enc == null) throw new AssertionError("CoverPacket.encode returned null");
        if (enc.width != w || enc.height != h) throw new AssertionError("Dimensions mismatch");
        if (enc.wireBytes.length >= w * h) {
            throw new AssertionError("Compressed cover bytes should be smaller than raw: " + enc.wireBytes.length);
        }

        byte[] wire = enc.wireBytes;
        int parsedW = (wire[0] & 255) | ((wire[1] & 255) << 8);
        int parsedH = (wire[2] & 255) | ((wire[3] & 255) << 8);
        int flags = (wire[4] & 255) | ((wire[5] & 255) << 8);
        if (parsedW != w || parsedH != h) throw new AssertionError("Parsed dimensions mismatch");
        if ((flags & CoverPacket.FLAG_DEFLATE) == 0 || (flags & CoverPacket.FLAG_PALETTE) == 0) {
            throw new AssertionError("Missing deflate or palette flag");
        }

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

        System.out.printf("PASS: CoverPacketTest (raw RGB=%dB, encoded wire=%dB, ratio=%.1f%%)%n",
            w * h * 4, wire.length, wire.length * 100.0 / (w * h * 4));
    }
}
