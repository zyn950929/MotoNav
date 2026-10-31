package com.yuminaga.motonav;

import java.io.ByteArrayOutputStream;
import java.util.Arrays;
import java.util.zip.Deflater;

/** 离线包只接收完整的 1200×1200、2 米普通地图，传输时支持 Deflate 压缩与重叠区 XOR 差分编码。 */
public final class RasterPacket {
    public static final int FLAG_PALETTE = 0x8000;
    public static final int FLAG_BUILDINGS = AmapBuildings.FLAG;
    public static final int FLAG_DEFLATE = 0x2000;
    public static final int FLAG_DELTA = 0x1000;
    public static final int MM_MASK = 0x0FFF;

    public static boolean isBuilding(byte[] b) {
        return b != null && b.length >= 16 && (u16(b, 14) & (FLAG_PALETTE | FLAG_BUILDINGS | MM_MASK)) == (2000 | FLAG_BUILDINGS);
    }

    public static boolean isDeflated(byte[] b) {
        return b != null && b.length >= 16 && (u16(b, 14) & FLAG_DEFLATE) != 0;
    }

    public static boolean isDelta(byte[] b) {
        return b != null && b.length >= 24 && (u16(b, 14) & FLAG_DELTA) != 0;
    }

    /** 连接旧固件时保留完整范围，将屋顶退回地面，绝不发送它不认识的标志。 */
    public static byte[] forLegacy(byte[] b) {
        if (!isBuilding(b) || isDeflated(b) || isDelta(b)) return b;
        byte[] out = b.clone();
        out[14] = (byte) 2000;
        out[15] = (byte) (2000 >>> 8);
        for (int p = 16; p < out.length; ) {
            int v = out[p] & 255;
            if ((v & 15) > 3) out[p] = (byte) ((v & 240) | (v & 2));
            p += ((v >>> 4) == 15) ? 2 : 1;
        }
        return out;
    }

    public static boolean hasPalette(byte[] b) {
        return b != null && b.length >= 48 && (u16(b, 14) & FLAG_PALETTE) != 0;
    }

    private static int u16(byte[] b, int p) {
        return (b[p] & 255) | ((b[p + 1] & 255) << 8);
    }

    private static int i32(byte[] b, int p) {
        return u16(b, p) | (u16(b, p + 2) << 16);
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

    public static boolean valid(byte[] b, int frame, int x, int y) {
        if (b == null || b.length < 17 || b.length > 480000 || u16(b, 0) != frame || i32(b, 2) != x || i32(b, 6) != y ||
            u16(b, 10) != 1200 || u16(b, 12) != 1200 ||
            (u16(b, 14) != 2000 && u16(b, 14) != (2000 | FLAG_BUILDINGS) && u16(b, 14) != (2000 | FLAG_PALETTE))) return false;
        if ((u16(b, 14) & FLAG_PALETTE) != 0 && !hasPalette(b)) return false;
        int pos = hasPalette(b) ? 48 : 16, pixels = 0;
        while (pos < b.length && pixels < 1440000) {
            int v = b[pos++] & 255;
            if ((v & 15) > (hasPalette(b) ? 15 : isBuilding(b) ? 7 : 3)) return false;
            int run = (v >> 4) + 1;
            if ((v >> 4) == 15) {
                if (pos == b.length) return false;
                run = 16 + (b[pos++] & 255);
            }
            if (run > 1440000 - pixels) return false;
            pixels += run;
        }
        return pixels == 1440000 && pos == b.length;
    }

    private static byte[] decodeRle(byte[] b, int totalPixels) {
        int pos = hasPalette(b) ? 48 : 16;
        if (b.length <= pos) return null;
        byte[] pixels = new byte[totalPixels];
        int k = 0;
        while (pos < b.length && k < totalPixels) {
            int v = b[pos++] & 255;
            int run = (v >>> 4) + 1;
            if (run == 16) {
                if (pos >= b.length) return null;
                run += b[pos++] & 255;
            }
            if (run > totalPixels - k) return null;
            byte c = (byte) (v & 15);
            if (c != 0) Arrays.fill(pixels, k, k + run, c);
            k += run;
        }
        return (k == totalPixels && pos == b.length) ? pixels : null;
    }

    private static byte[] encodeRleStream(byte[] idx) {
        ByteArrayOutputStream b = new ByteArrayOutputStream(idx.length / 8 + 64);
        int i = 0, n = idx.length;
        while (i < n) {
            int c = idx[i] & 15;
            int run = 1;
            while (i + run < n && run < 271 && (idx[i + run] & 15) == c) run++;
            if (run <= 15) {
                b.write(((run - 1) << 4) | c);
            } else {
                b.write((15 << 4) | c);
                b.write(run - 16);
            }
            i += run;
        }
        return b.toByteArray();
    }

    private static byte[] deflateBytes(byte[] input, int offset, int length) {
        Deflater deflater = new Deflater(Deflater.BEST_COMPRESSION);
        try {
            deflater.setInput(input, offset, length);
            deflater.finish();
            ByteArrayOutputStream out = new ByteArrayOutputStream(Math.max(256, length / 3));
            byte[] buf = new byte[4096];
            while (!deflater.finished()) {
                int n = deflater.deflate(buf);
                if (n <= 0) break;
                out.write(buf, 0, n);
            }
            return deflater.finished() ? out.toByteArray() : null;
        } finally {
            deflater.end();
        }
    }

    /** 将任意合法栅格包（含普通底图、楼块底图、原版 16 色底图、卫星图）的 RLE 数据段用 Deflate 二次压缩。 */
    public static byte[] compressFull(byte[] b) {
        if (b == null || isDeflated(b) || isDelta(b)) return b;
        int dataStart = hasPalette(b) ? 48 : 16;
        if (b.length <= dataStart) return b;
        int rleLen = b.length - dataStart;
        byte[] compressed = deflateBytes(b, dataStart, rleLen);
        if (compressed == null || dataStart + 4 + compressed.length >= b.length) return b;
        ByteArrayOutputStream out = new ByteArrayOutputStream(dataStart + 4 + compressed.length);
        out.write(b, 0, 14);
        writeU16(out, u16(b, 14) | FLAG_DEFLATE);
        if (dataStart > 16) out.write(b, 16, dataStart - 16);
        writeI32(out, rleLen);
        out.write(compressed, 0, compressed.length);
        return out.toByteArray();
    }

    /** 对新旧两块同坐标系底图的重叠区域做 XOR 差分并 Deflate 压缩（50%~100% 重叠区归零）。 */
    public static byte[] encodeDelta(byte[] current, byte[] reference) {
        if (current == null || reference == null) return null;
        if (hasPalette(current) || hasPalette(reference) || isDeflated(current) || isDeflated(reference) ||
            isDelta(current) || isDelta(reference)) return null;
        if (current.length < 17 || reference.length < 17) return null;
        int frame = u16(current, 0);
        if (frame != u16(reference, 0)) return null;
        int w = u16(current, 10), h = u16(current, 12);
        if (w <= 0 || h <= 0 || w != u16(reference, 10) || h != u16(reference, 12) || (long) w * h > 1690000L) return null;
        int mm = u16(current, 14) & MM_MASK;
        if (mm <= 0 || mm != (u16(reference, 14) & MM_MASK)) return null;
        int x0 = i32(current, 2), y0 = i32(current, 6);
        int refX0 = i32(reference, 2), refY0 = i32(reference, 6);
        long dx = (long) x0 - refX0, dy = (long) refY0 - y0;
        if ((dx * 1000L) % mm != 0 || (dy * 1000L) % mm != 0) return null;
        int ox = (int) ((dx * 1000L) / mm), oy = (int) ((dy * 1000L) / mm);
        if (Math.abs(ox) >= w || Math.abs(oy) >= h) return null;
        int totalPixels = w * h;
        byte[] curPx = decodeRle(current, totalPixels);
        byte[] refPx = decodeRle(reference, totalPixels);
        if (curPx == null || refPx == null) return null;
        int xStart = Math.max(0, -ox), xEnd = Math.min(w, w - ox);
        int yStart = Math.max(0, -oy), yEnd = Math.min(h, h - oy);
        if (xStart >= xEnd || yStart >= yEnd) return null;
        for (int y = yStart; y < yEnd; y++) {
            int curRow = y * w;
            int refRow = (y + oy) * w + ox;
            for (int x = xStart; x < xEnd; x++) {
                curPx[curRow + x] = (byte) ((curPx[curRow + x] ^ refPx[refRow + x]) & 15);
            }
        }
        byte[] deltaRle = encodeRleStream(curPx);
        byte[] compressed = deflateBytes(deltaRle, 0, deltaRle.length);
        if (compressed == null) return null;
        int flags = (u16(current, 14) & (FLAG_BUILDINGS | MM_MASK)) | FLAG_DELTA | FLAG_DEFLATE;
        ByteArrayOutputStream out = new ByteArrayOutputStream(24 + 4 + compressed.length);
        writeU16(out, frame);
        writeI32(out, x0);
        writeI32(out, y0);
        writeU16(out, w);
        writeU16(out, h);
        writeU16(out, flags);
        writeI32(out, refX0);
        writeI32(out, refY0);
        writeI32(out, deltaRle.length);
        out.write(compressed, 0, compressed.length);
        return out.toByteArray();
    }

    /** 根据圆屏能力自动选择：Delta+Deflate（最优） -> Full+Deflate -> 旧版裸 RLE。 */
    public static byte[] forWire(byte[] b, byte[] confirmedReference, boolean buildingsSupported, boolean zmapSupported) {
        if (b == null) return null;
        byte[] plain = (!hasPalette(b) && !buildingsSupported) ? forLegacy(b) : b;
        if (!zmapSupported) return plain;
        byte[] full = compressFull(plain);
        if (confirmedReference != null && !hasPalette(plain)) {
            byte[] refPlain = (!hasPalette(confirmedReference) && !buildingsSupported) ? forLegacy(confirmedReference) : confirmedReference;
            byte[] delta = encodeDelta(plain, refPlain);
            if (delta != null && delta.length < full.length) return delta;
        }
        return full;
    }
}
