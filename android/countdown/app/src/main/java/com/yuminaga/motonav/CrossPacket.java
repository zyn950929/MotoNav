package com.yuminaga.motonav;

import java.io.ByteArrayOutputStream;
import java.util.Arrays;
import java.util.zip.Deflater;

/**
 * 复杂路口放大图（Junction View / CrossImage）压缩打包器：
 * - 提取 16 色 RGB565 调色板
 * - 4-bit 像素索引 RLE 压缩
 * - Deflate 二次压缩
 * - 输出符合 BLE 大块数据传输通道（TYPE_CROSS = 5）的极简二进制包（通常仅 1~2.5 KB）。
 */
public final class CrossPacket {
    public static final int TYPE_CROSS = 5;
    public static final int FLAG_PALETTE = 0x8000;
    public static final int FLAG_DEFLATE = 0x2000;

    public static final class Encoded {
        public final int width;
        public final int height;
        public final byte[] wireBytes;

        public Encoded(int width, int height, byte[] wireBytes) {
            this.width = width;
            this.height = height;
            this.wireBytes = wireBytes;
        }
    }

    private CrossPacket() {}

    /** 将 32 位 ARGB 像素数组量化为 16 色 RGB565 调色板及 4-bit 颜色索引，并进行 RLE + Deflate 压缩。 */
    public static Encoded encode(int[] argb, int w, int h) {
        if (argb == null || w <= 0 || h <= 0 || argb.length < w * h) return null;
        int n = w * h;

        // 1. 提取 16 色代表色（均匀区间聚类采样）
        int[] palRgb888 = selectPalette16(argb, n);
        int[] palRgb565 = new int[16];
        for (int i = 0; i < 16; i++) {
            palRgb565[i] = rgb888To565(palRgb888[i]);
        }

        // 2. 将每个像素映射到最近的调色板索引 (0~15)
        byte[] idx = new byte[n];
        for (int i = 0; i < n; i++) {
            idx[i] = (byte) findNearestColor(argb[i], palRgb888);
        }

        // 3. 4-bit RLE 压缩
        ByteArrayOutputStream rleOut = new ByteArrayOutputStream(n / 4 + 64);
        int pos = 0;
        while (pos < n) {
            int c = idx[pos] & 15;
            int run = 1;
            while (pos + run < n && run < 271 && (idx[pos + run] & 15) == c) run++;
            if (run <= 15) {
                rleOut.write(((run - 1) << 4) | c);
            } else {
                rleOut.write((15 << 4) | c);
                rleOut.write(run - 16);
            }
            pos += run;
        }
        byte[] rleBytes = rleOut.toByteArray();

        // 4. Deflate 二次压缩
        Deflater deflater = new Deflater(Deflater.BEST_COMPRESSION);
        deflater.setInput(rleBytes);
        deflater.finish();
        ByteArrayOutputStream defOut = new ByteArrayOutputStream(rleBytes.length);
        byte[] buf = new byte[1024];
        while (!deflater.finished()) {
            int count = deflater.deflate(buf);
            if (count <= 0) break;
            defOut.write(buf, 0, count);
        }
        deflater.end();
        byte[] compressed = defOut.toByteArray();

        // 5. 组装数据包头
        // 0..1: u16 w, 2..3: u16 h, 4..5: u16 flags, 6..37: 16x u16 pal, 38..41: u32 rawRleLen, 42..: compressed
        ByteArrayOutputStream wire = new ByteArrayOutputStream(42 + compressed.length);
        writeU16(wire, w);
        writeU16(wire, h);
        writeU16(wire, FLAG_PALETTE | FLAG_DEFLATE);
        for (int i = 0; i < 16; i++) {
            writeU16(wire, palRgb565[i]);
        }
        writeI32(wire, rleBytes.length);
        wire.write(compressed, 0, compressed.length);

        return new Encoded(w, h, wire.toByteArray());
    }

    private static int rgb888To565(int c) {
        int r = (c >> 16) & 255;
        int g = (c >> 8) & 255;
        int b = c & 255;
        return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3);
    }

    private static int findNearestColor(int c, int[] palette) {
        int r = (c >> 16) & 255;
        int g = (c >> 8) & 255;
        int b = c & 255;
        int bestIdx = 0;
        int bestDist = Integer.MAX_VALUE;
        for (int i = 0; i < palette.length; i++) {
            int pr = (palette[i] >> 16) & 255;
            int pg = (palette[i] >> 8) & 255;
            int pb = palette[i] & 255;
            int dr = r - pr, dg = g - pg, db = b - pb;
            int dist = dr * dr * 2 + dg * dg * 4 + db * db * 3; // 考虑人眼对绿色更敏感的权重
            if (dist < bestDist) {
                bestDist = dist;
                bestIdx = i;
            }
        }
        return bestIdx;
    }

    private static int[] selectPalette16(int[] argb, int n) {
        // 统计颜色直方图（粗粒度 4x4x4 桶）
        int[] buckets = new int[4096];
        int step = Math.max(1, n / 5000); // 采样最多 5000 个像素加速调色板生成
        for (int i = 0; i < n; i += step) {
            int c = argb[i];
            int r = (c >> 20) & 15;
            int g = (c >> 12) & 15;
            int b = (c >> 4) & 15;
            buckets[(r << 8) | (g << 4) | b]++;
        }

        // 挑选频率最高的 16 个代表桶
        int[] topBuckets = new int[16];
        int[] topCounts = new int[16];
        Arrays.fill(topBuckets, -1);
        for (int b = 0; b < 4096; b++) {
            int count = buckets[b];
            if (count == 0) continue;
            for (int i = 0; i < 16; i++) {
                if (count > topCounts[i]) {
                    for (int j = 15; j > i; j--) {
                        topCounts[j] = topCounts[j - 1];
                        topBuckets[j] = topBuckets[j - 1];
                    }
                    topCounts[i] = count;
                    topBuckets[i] = b;
                    break;
                }
            }
        }

        int[] palette = new int[16];
        for (int i = 0; i < 16; i++) {
            int b = topBuckets[i];
            if (b >= 0) {
                int r = ((b >> 8) & 15) * 17;
                int g = ((b >> 4) & 15) * 17;
                int bl = (b & 15) * 17;
                palette[i] = (r << 16) | (g << 8) | bl;
            } else {
                palette[i] = (i * 17 << 16) | (i * 17 << 8) | (i * 17);
            }
        }
        return palette;
    }

    private static void writeU16(ByteArrayOutputStream out, int v) {
        out.write(v & 255);
        out.write((v >>> 8) & 255);
    }

    private static void writeI32(ByteArrayOutputStream out, int v) {
        out.write(v & 255);
        out.write((v >>> 8) & 255);
        out.write((v >>> 16) & 255);
        out.write((v >>> 24) & 255);
    }
}
