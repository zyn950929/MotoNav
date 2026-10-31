package com.yuminaga.motonav;

import java.io.ByteArrayOutputStream;
import java.util.Arrays;
import java.util.zip.Inflater;

public final class RasterCompressionDeltaTest {
    public static void main(String[] args) throws Exception {
        if (!BleSession.confirms("CAP,ZMAP,1", true)) {
            throw new AssertionError("CAP,ZMAP,1 should confirm session when capabilityQuerySent=true");
        }
        if (BleSession.confirms("CAP,ZMAP,1", false)) {
            throw new AssertionError("CAP,ZMAP,1 should not confirm session when capabilityQuerySent=false");
        }

        int w = 1200, h = 1200;
        byte[] refGrid = makeSyntheticMap(w, h, 0, 0, false);
        byte[] refPkt = encodeRaster(101, 1000, 2000, w, h, 2000 | RasterPacket.FLAG_BUILDINGS, refGrid);

        // 1. Legacy fallback when zmapSupported = false
        byte[] legacyWire = RasterPacket.forWire(refPkt, null, false, false);
        if ((u16(legacyWire, 14) & (RasterPacket.FLAG_DEFLATE | RasterPacket.FLAG_DELTA | RasterPacket.FLAG_BUILDINGS)) != 0) {
            throw new AssertionError("Legacy wire should strip experimental flags");
        }

        // 2. Full Deflate compression (Phase 1)
        byte[] fullZWire = RasterPacket.forWire(refPkt, null, true, true);
        int fullFlags = u16(fullZWire, 14);
        if ((fullFlags & RasterPacket.FLAG_DEFLATE) == 0 || (fullFlags & RasterPacket.FLAG_DELTA) != 0) {
            throw new AssertionError("Full compressed packet should set FLAG_DEFLATE and not FLAG_DELTA");
        }
        if (fullZWire.length >= refPkt.length) {
            throw new AssertionError("Deflate packet should be smaller than raw RLE packet");
        }
        byte[] refTiledBuf = new byte[(w * h + 1) / 2];
        byte[] decodedRef = firmwareSimulateDecode(fullZWire, null, 0, 0, refTiledBuf);
        if (!Arrays.equals(refGrid, decodedRef)) {
            throw new AssertionError("Full deflate decoded grid mismatch");
        }

        // 3. Same-origin Base -> Building incremental delta (ox=0, oy=0)
        byte[] bldGrid = makeSyntheticMap(w, h, 0, 0, true);
        byte[] bldPkt = encodeRaster(101, 1000, 2000, w, h, 2000 | RasterPacket.FLAG_BUILDINGS, bldGrid);
        byte[] baseGrid = makeSyntheticMap(w, h, 0, 0, false);
        byte[] basePkt = encodeRaster(101, 1000, 2000, w, h, 2000, baseGrid);
        byte[] baseTiledBuf = new byte[(w * h + 1) / 2];
        firmwareSimulateDecode(RasterPacket.forWire(basePkt, null, true, true), null, 0, 0, baseTiledBuf);
        byte[] bldDeltaWire = RasterPacket.forWire(bldPkt, basePkt, true, true);
        int bldFlags = u16(bldDeltaWire, 14);
        if ((bldFlags & RasterPacket.FLAG_DELTA) == 0 || (bldFlags & RasterPacket.FLAG_DEFLATE) == 0) {
            throw new AssertionError("Building upgrade should use FLAG_DELTA | FLAG_DEFLATE");
        }
        byte[] bldTiledBuf = new byte[(w * h + 1) / 2];
        byte[] decodedBld = firmwareSimulateDecode(bldDeltaWire, baseTiledBuf, 1000, 2000, bldTiledBuf);
        if (!Arrays.equals(bldGrid, decodedBld)) {
            throw new AssertionError("Same-origin building delta decoded grid mismatch");
        }

        // 4. 16x16 Block-aligned shift delta (dx = 448m -> ox = 224px, dy = -448m -> oy = -224px)
        int oxAligned = 224, oyAligned = -224;
        byte[] nextAlignedGrid = makeSyntheticMap(w, h, oxAligned, oyAligned, true);
        int alignedX0 = 1000 + oxAligned * 2, alignedY0 = 2000 - oyAligned * 2;
        byte[] nextAlignedPkt = encodeRaster(101, alignedX0, alignedY0, w, h, 2000 | RasterPacket.FLAG_BUILDINGS, nextAlignedGrid);
        byte[] alignedDeltaWire = RasterPacket.forWire(nextAlignedPkt, bldPkt, true, true);
        byte[] alignedFullWire = RasterPacket.forWire(nextAlignedPkt, null, true, true);
        if ((u16(alignedDeltaWire, 14) & RasterPacket.FLAG_DELTA) == 0) {
            throw new AssertionError("Overlapping shift should use FLAG_DELTA");
        }
        if (alignedDeltaWire.length >= alignedFullWire.length) {
            throw new AssertionError("Delta wire should be smaller than full compressed wire");
        }
        byte[] alignedTiledBuf = new byte[(w * h + 1) / 2];
        byte[] decodedAligned = firmwareSimulateDecode(alignedDeltaWire, bldTiledBuf, 1000, 2000, alignedTiledBuf);
        if (!Arrays.equals(nextAlignedGrid, decodedAligned)) {
            throw new AssertionError("Block-aligned shift delta decoded grid mismatch");
        }

        // 5. Unaligned shift delta (dx = 102m -> ox = 51px, dy = 74m -> oy = -37px)
        int oxOdd = 51, oyOdd = -37;
        byte[] nextOddGrid = makeSyntheticMap(w, h, oxAligned + oxOdd, oyAligned + oyOdd, true);
        int oddX0 = 1000 + (oxAligned + oxOdd) * 2, oddY0 = 2000 - (oyAligned + oyOdd) * 2;
        byte[] nextOddPkt = encodeRaster(101, oddX0, oddY0, w, h, 2000 | RasterPacket.FLAG_BUILDINGS, nextOddGrid);
        byte[] oddDeltaWire = RasterPacket.forWire(nextOddPkt, nextAlignedPkt, true, true);
        if ((u16(oddDeltaWire, 14) & RasterPacket.FLAG_DELTA) == 0) {
            throw new AssertionError("Odd shift should use FLAG_DELTA");
        }
        byte[] oddTiledBuf = new byte[(w * h + 1) / 2];
        byte[] decodedOdd = firmwareSimulateDecode(oddDeltaWire, alignedTiledBuf, alignedX0, alignedY0, oddTiledBuf);
        if (!Arrays.equals(nextOddGrid, decodedOdd)) {
            throw new AssertionError("Odd shift delta decoded grid mismatch");
        }

        System.out.printf("PASS: Raster compression & delta (raw=%dB, fullZ=%dB, bldDelta=%dB, shiftDelta=%dB)%n",
            nextAlignedPkt.length, alignedFullWire.length, bldDeltaWire.length, alignedDeltaWire.length);
    }

    private static byte[] encodeRaster(int frameId, int x0, int y0, int w, int h, int flags, byte[] idx) {
        ByteArrayOutputStream b = new ByteArrayOutputStream(w * h / 6 + 64);
        writeU16(b, frameId);
        writeI32(b, x0);
        writeI32(b, y0);
        writeU16(b, w);
        writeU16(b, h);
        writeU16(b, flags);
        int i = 0, n = w * h;
        while (i < n) {
            int c = idx[i] & 15;
            int run = 1;
            while (i + run < n && run < 271 && (idx[i + run] & 15) == c) run++;
            if (run <= 15) b.write(((run - 1) << 4) | c);
            else {
                b.write((15 << 4) | c);
                b.write(run - 16);
            }
            i += run;
        }
        return b.toByteArray();
    }

    private static byte[] makeSyntheticMap(int w, int h, int worldOx, int worldOy, boolean buildings) {
        byte[] g = new byte[w * h];
        for (int y = 0; y < h; y++) {
            int wy = y + worldOy;
            for (int x = 0; x < w; x++) {
                int wx = x + worldOx;
                int v = 0;
                if (Math.abs(wx % 180) < 6 || Math.abs(wy % 180) < 6) v = 1; // road
                else if (((wx / 90 + wy / 90) & 7) == 0) v = 3; // water
                else if (((wx / 70 - wy / 70) & 7) == 1) v = 2; // park
                else if (buildings && Math.abs(wx % 60) > 12 && Math.abs(wy % 60) > 12 && ((wx / 60 + wy / 60) & 1) == 0) {
                    v = 7; // building
                }
                g[y * w + x] = (byte) v;
            }
        }
        return g;
    }

    private static int rasterNibbleAddr(int w, int x, int y) {
        int bw = (w + 15) >> 4;
        return (((y >> 4) * bw + (x >> 4)) << 8) | ((y & 15) << 4) | (x & 15);
    }

    private static void rasterCopyReference(byte[] dst, byte[] src, int w, int h, int ox, int oy) {
        int bytes = (w * h + 1) / 2;
        if (ox == 0 && oy == 0) {
            System.arraycopy(src, 0, dst, 0, bytes);
            return;
        }
        Arrays.fill(dst, 0, bytes, (byte) 0);
        int xStart = Math.max(0, -ox), xEnd = Math.min(w, w - ox);
        int yStart = Math.max(0, -oy), yEnd = Math.min(h, h - oy);
        if (xStart >= xEnd || yStart >= yEnd) return;
        if ((w & 15) == 0 && (h & 15) == 0 && (ox & 15) == 0 && (oy & 15) == 0) {
            int bw = w >> 4, obx = ox >> 4, oby = oy >> 4;
            int bxStart = xStart >> 4, bxEnd = xEnd >> 4;
            int byStart = yStart >> 4, byEnd = yEnd >> 4;
            for (int by = byStart; by < byEnd; by++) {
                int sby = by + oby;
                for (int bx = bxStart; bx < bxEnd; bx++) {
                    int sbx = bx + obx;
                    System.arraycopy(src, (sby * bw + sbx) * 128, dst, (by * bw + bx) * 128, 128);
                }
            }
            return;
        }
        for (int y = yStart; y < yEnd; y++) {
            int sy = y + oy;
            for (int x = xStart; x < xEnd; x++) {
                int sx = x + ox;
                int sa = rasterNibbleAddr(w, sx, sy);
                int c = ((sa & 1) != 0) ? (src[sa >> 1] & 15) : ((src[sa >> 1] >>> 4) & 15);
                if (c != 0) {
                    int da = rasterNibbleAddr(w, x, y);
                    if ((da & 1) != 0) dst[da >> 1] |= (byte) c;
                    else dst[da >> 1] |= (byte) (c << 4);
                }
            }
        }
    }

    private static byte[] firmwareSimulateDecode(byte[] wire, byte[] refTiledBuf, int refBufX0, int refBufY0, byte[] outTiledBuf) throws Exception {
        int w = u16(wire, 10), h = u16(wire, 12);
        int flags = u16(wire, 14);
        boolean pal = (flags & RasterPacket.FLAG_PALETTE) != 0;
        boolean deflate = (flags & RasterPacket.FLAG_DEFLATE) != 0;
        boolean delta = (flags & RasterPacket.FLAG_DELTA) != 0;
        int hdr = pal ? 48 : (delta ? 24 : 16);

        if (delta) {
            if (refTiledBuf == null) throw new AssertionError("Delta requires reference buffer");
            int refX0 = readI32(wire, 16), refY0 = readI32(wire, 20);
            if (refX0 != refBufX0 || refY0 != refBufY0) {
                throw new AssertionError("Reference coordinates mismatch");
            }
            int mm = (flags & RasterPacket.MM_MASK) > 0 ? (flags & RasterPacket.MM_MASK) : 2000;
            int ox = (readI32(wire, 2) - refX0) * 1000 / mm;
            int oy = (refY0 - readI32(wire, 6)) * 1000 / mm;
            rasterCopyReference(outTiledBuf, refTiledBuf, w, h, ox, oy);
        } else {
            Arrays.fill(outTiledBuf, (byte) 0);
        }

        byte[] rleStream;
        if (deflate) {
            int rawLen = readI32(wire, hdr);
            rleStream = new byte[rawLen];
            Inflater inf = new Inflater();
            inf.setInput(wire, hdr + 4, wire.length - (hdr + 4));
            int got = inf.inflate(rleStream);
            inf.end();
            if (got != rawLen) throw new AssertionError("Inflate length mismatch: " + got + " != " + rawLen);
        } else {
            rleStream = Arrays.copyOfRange(wire, hdr, wire.length);
        }

        int pos = 0, idx = 0, total = w * h;
        while (pos < rleStream.length && idx < total) {
            int b = rleStream[pos++] & 255;
            int c = b & 15;
            int run = (b >>> 4) + 1;
            if ((b >>> 4) == 15) run = 16 + (rleStream[pos++] & 255);
            if (delta && c == 0) {
                idx += run;
                continue;
            }
            while (run-- > 0) {
                int x = idx % w, y = idx / w;
                int a = rasterNibbleAddr(w, x, y);
                if (delta) {
                    if ((a & 1) != 0) outTiledBuf[a >> 1] ^= (byte) c;
                    else outTiledBuf[a >> 1] ^= (byte) (c << 4);
                } else {
                    if ((a & 1) != 0) outTiledBuf[a >> 1] |= (byte) c;
                    else outTiledBuf[a >> 1] |= (byte) (c << 4);
                }
                idx++;
            }
        }
        if (idx != total) throw new AssertionError("RLE total mismatch");

        byte[] outGrid = new byte[total];
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int a = rasterNibbleAddr(w, x, y);
                outGrid[y * w + x] = (byte) (((a & 1) != 0) ? (outTiledBuf[a >> 1] & 15) : ((outTiledBuf[a >> 1] >>> 4) & 15));
            }
        }
        return outGrid;
    }

    private static int u16(byte[] b, int o) {
        return (b[o] & 255) | ((b[o + 1] & 255) << 8);
    }

    private static int readI32(byte[] b, int o) {
        return (b[o] & 255) | ((b[o + 1] & 255) << 8) | ((b[o + 2] & 255) << 16) | ((b[o + 3] & 255) << 24);
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
