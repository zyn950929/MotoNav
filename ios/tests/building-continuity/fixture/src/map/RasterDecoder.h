#pragma once
#include "../../RasterBuildingCarry.h"
bool rasterBuildingCarryOn=false; // Explicit App opt-in; old clients keep prior behaviour.
bool rasterCarriesBuildings=false;
// Carry/clear can alter the rendered pixels relative to the sender's XOR reference.
bool rasterDeltaBaseClean=false;
uint32_t rasterBuildingCarryEpoch=0;
// 位图地图及罪恶都市轮廓解码。由 MotoNav.ino 按依赖顺序包含，仅编译一次。
// v2.5 解析图片地图：u16 坐标系, i32 左上角 x, i32 左上角 y, u16 宽, u16 高, u16 每像素毫米, 然后是压缩的颜色编号
static inline uint16_t blendF(uint16_t a, uint16_t b, uint32_t t);   // v6.1 后面定义
bool rasHasPal = false;                    // v6.1 卫星图自带调色板
uint16_t rasPal[16];

// 罪恶都市局部边界几何；其它风格仍读取原来的 4 位材质图。
uint16_t *rasterVice = nullptr;
size_t rasterAllocatedBytes = RASTER_BUF_BYTES;
bool rasterViceValid = false;
tinfl_decompressor *viceInflater = nullptr;
uint16_t viceEdgePal[7][257];
uint16_t vicePalKey[4] = {0xffff, 0xffff, 0xffff, 0xffff};
// 覆盖率按像素方形与半平面的相交面积计算。利用镜像对称，只需保存九个角度的表。
int8_t viceNX[64], viceNY[64];
int32_t viceOffsets[2][32];
uint16_t viceCoverage[9][257];
bool rasterViceFine = false;
uint8_t viceBaseShift = 13;
uint8_t rasterViceVersion = 0;
const uint16_t *vicePatchData = nullptr;
uint16_t vicePatchCount = 0;

void initViceSampling() {
  static bool ready = false;
  if (ready) return;
  for (int i = 0; i < 64; i++) {
    float a = i * (2.0f * PI / 64.0f);
    viceNX[i] = (int8_t)lroundf(cosf(a) * 127);
    viceNY[i] = (int8_t)lroundf(sinf(a) * 127);
  }
  const float offsets[16] = {0.0f, 0.0625f, 0.125f, 0.1875f,
    0.25f, 0.3125f, 0.375f, 0.4375f, 0.5f, 0.5625f,
    0.625f, 0.6875f, 0.8125f, 1.0f, 1.25f, 1.625f};
  for (int i = 0; i < 32; i++) {
    viceOffsets[0][i] = (i < 16 ? i : i - 32) * 4064;
    viceOffsets[1][i] = (int32_t)lroundf(offsets[i & 15] * (i & 16 ? -32512.0f : 32512.0f));
  }
  for (int angle = 0; angle <= 8; angle++) {
    float a = angle * (PI / 32.0f);
    float major = cosf(a), minor = sinf(a);
    for (int i = 0; i <= 256; i++) {
      float distance = (i - 128) * (1.5f / 256.0f);
      float t = distance + (major + minor) * 0.5f;
      float coverage;
      if (t <= 0) coverage = 0;
      else if (t >= major + minor) coverage = 1;
      else if (minor < 0.00001f) coverage = t / major;
      else if (t < minor) coverage = t * t / (2 * major * minor);
      else if (t <= major) coverage = (t - minor * 0.5f) / major;
      else { float tail = major + minor - t; coverage = 1 - tail * tail / (2 * major * minor); }
      viceCoverage[angle][i] = (uint16_t)max(0, min(256, (int)lroundf(coverage * 256.0f)));
    }
  }
  ready = true;
}

static inline uint8_t viceCoverageAngle(float nx, float ny) {
  int angle = (int)lroundf(atan2f(fabsf(ny), fabsf(nx)) * (32.0f / PI));
  if (angle > 8) angle = 16 - angle;
  return (uint8_t)max(0, min(8, angle));
}


void parseViceGeometry(const uint8_t *p, uint32_t len, uint32_t pos, int bw, int bh) {
  rasterViceValid = false;
  vicePatchCount = 0; vicePatchData = nullptr;
  if (pos <= len && len - pos >= 8 && p[pos] == 'V' && p[pos + 1] == 'C' && p[pos + 2] == 'E' &&
      p[pos + 3] >= 1 && p[pos + 3] <= 3) {
    uint8_t version = p[pos + 3];
    uint32_t packed = (uint32_t)rd32(p + pos + 4);
    uint32_t header = version == 3 ? 12 : 8;
    if (len - pos < header) return;
    uint16_t patchCount = version == 3 ? rdu16(p + pos + 8) : 0;
    if (patchCount > 2047 || (version == 3 && rdu16(p + pos + 10) != 0)) return;
    size_t cellsBytes = (size_t)bw * bh * 512;
    size_t needed = cellsBytes + (size_t)patchCount * 8;
    if (packed == len - pos - header && needed <= RASTER_BUF_BYTES * 4 + 2047 * 8) {
      if (rasterAllocatedBytes != needed) {
        free(rasterBuf);
        rasterBuf = (uint8_t *)ps_malloc(needed);
        rasterAllocatedBytes = rasterBuf ? needed : 0;
      }
      rasterVice = (uint16_t *)rasterBuf;
      if (!viceInflater) viceInflater = (tinfl_decompressor *)malloc(sizeof(tinfl_decompressor));
      if (rasterVice && rasterAllocatedBytes == needed && viceInflater) {
        tinfl_init(viceInflater);
        size_t inBytes = packed, outBytes = needed;
        tinfl_status result = tinfl_decompress(viceInflater, p + pos + header, &inBytes,
            (uint8_t *)rasterVice, (uint8_t *)rasterVice, &outBytes,
            TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
        rasterViceValid = result == TINFL_STATUS_DONE && inBytes == packed && outBytes == needed;
        const uint16_t *vertices = (const uint16_t *)(rasterBuf + cellsBytes);
        if (rasterViceValid) {
          for (uint32_t i = 0; i < (uint32_t)patchCount * 4; i++) {
            uint16_t v = vertices[i];
            if ((v & 15) + ((v >> 4) & 15) + ((v >> 8) & 15) + (v >> 12) != 15) {
              rasterViceValid = false; break;
            }
          }
        }
        if (rasterViceValid) {
          rasterViceVersion = version;
          rasterViceFine = version >= 2;
          viceBaseShift = rasterViceFine ? 14 : 13;
          vicePatchCount = patchCount; vicePatchData = patchCount ? vertices : nullptr;
        }
      }
    }
  }
  if (!rasterViceValid) rasterVice = nullptr;
}

void prepareVicePalette(const uint16_t *pal) {
  static bool initialized = false;
  initViceSampling();
  bool changed = !initialized;
  for (int i = 0; i < 4; i++) if (vicePalKey[i] != pal[i]) changed = true;
  if (!changed) return;
  const uint8_t pairA[7] = {0, 0, 0, 0, 1, 1, 2};
  const uint8_t pairB[7] = {0, 1, 2, 3, 2, 3, 3};
  for (int p = 1; p <= 6; p++) for (int a = 0; a <= 256; a++)
    viceEdgePal[p][a] = blendF(pal[pairA[p]], pal[pairB[p]], a);
  memcpy(vicePalKey, pal, sizeof(vicePalKey));
  initialized = true;
}

void parseRasterLegacy(const uint8_t *p, uint32_t len) {
  rasterDeltaBaseClean=false;
  rasterValid = false;
  rasterViceValid = false;
  if (len < 16) return;
  int frame = rdu16(p);
  int32_t x0 = rd32(p + 2), y0 = rd32(p + 6);
  int w = rdu16(p + 10), h = rdu16(p + 12), mm = rdu16(p + 14);
  bool hasPal = (mm & 0x8000) != 0;
  mm &= 0x7FFF;
  uint16_t palNew[16];
  if (hasPal) {
    if (len < 48) return;
    for (int i = 0; i < 16; i++) palNew[i] = rdu16(p + 16 + 2 * i);
  }
  uint32_t n = (uint32_t)w * h;
  if (w == 0 || h == 0 || n > RASTER_MAX_PX || mm == 0) return;
  int bw = (w + 15) >> 4, bh = (h + 15) >> 4;
  size_t legacyBytes = (size_t)bw * bh * 128;
  if (legacyBytes > RASTER_BUF_BYTES) return;
  uint32_t dataStart = hasPal ? 48 : 16;
  // 先检查普通图的游程长度，找到扩展的位置；不另存一份解压后的普通图。
  uint32_t pos = dataStart, i = 0;
  while (pos < len && i < n) {
    uint8_t b = p[pos++], r = b >> 4;
    uint32_t run = r + 1;
    if (r == 15) { if (pos >= len) return; run = 16 + p[pos++]; }
    if (run > n - i) return;
    i += run;
  }
  if (i != n) return;
  if (!hasPal) parseViceGeometry(p, len, pos, bw, bh);
  if (!rasterViceValid) {
    // 不带扩展的地图、卫星图或扩展解码失败，回到原来的 4 位格式。
    if (!rasterBuf || rasterAllocatedBytes != RASTER_BUF_BYTES) {
      free(rasterBuf);
      rasterBuf = (uint8_t *)ps_malloc(RASTER_BUF_BYTES);
      rasterAllocatedBytes = rasterBuf ? RASTER_BUF_BYTES : 0;
    }
    rasterVice = nullptr;
    if (!rasterBuf) return;
    memset(rasterBuf, 0, legacyBytes);
    pos = dataStart; i = 0;
    int x = 0, y = 0;
    while (i < n) {
      uint8_t b = p[pos++], c = b & 15, r = b >> 4;
      uint32_t run = r + 1;
      if (r == 15) run = 16 + p[pos++];
      if (c) {
        for (uint32_t k = 0; k < run; k++) {
          uint32_t a = ((uint32_t)((y >> 4) * bw + (x >> 4)) << 8) | ((y & 15) << 4) | (x & 15);
          rasterBuf[a >> 1] |= (a & 1) ? (c << 4) : c;
          if (++x == w) { x = 0; y++; }
        }
      } else {
        x += run;
        while (x >= w) { x -= w; y++; }
      }
      i += run;
    }
  }
  rasBW = bw;
  rasX0 = x0; rasY0 = y0; rasW = w; rasH = h; rasMpp = mm / 1000.0f; rasterFrame = frame;
  rasHasPal = hasPal;
  if (hasPal) memcpy(rasPal, palNew, sizeof(rasPal));
  rasterValid = true;
  rasterAt = millis();
}

// 普通地图先在备用缓冲解码，成功后交换；上一块保留，用来补齐新块的覆盖边缘。
bool parseRaster(const uint8_t *p, uint32_t len) {
  RasterHeader probe;
  const uint8_t *src = p;
  uint32_t srcLen = len;
  uint8_t *unpacked = nullptr;
  if (rasterHeaderCheck(p, len, probe, RASTER_MAX_PX, RASTER_BUF_BYTES) && probe.deflate) {
    if (len < probe.dataStart + 6) return false;
    uint32_t rawRleLen = rasterU32(p + probe.dataStart);
    uint32_t compLen = len - probe.dataStart - 4;
    if (rawRleLen == 0 || probe.dataStart + rawRleLen > RASTER_MAX_BYTES) return false;
    unpacked = (uint8_t *)ps_malloc(probe.dataStart + rawRleLen);
    if (!unpacked) return false;
    if (!viceInflater) viceInflater = (tinfl_decompressor *)malloc(sizeof(tinfl_decompressor));
    if (!viceInflater) { free(unpacked); return false; }
    memcpy(unpacked, p, probe.dataStart);
    uint16_t cleanFlags = rasterU16(p + 14) & ~RASTER_FLAG_DEFLATE;
    unpacked[14] = (uint8_t)(cleanFlags & 0xFF);
    unpacked[15] = (uint8_t)(cleanFlags >> 8);
    tinfl_init(viceInflater);
    size_t inBytes = compLen, outBytes = rawRleLen;
    tinfl_status status = tinfl_decompress(viceInflater, p + probe.dataStart + 4, &inBytes,
        unpacked + probe.dataStart, unpacked + probe.dataStart, &outBytes,
        TINFL_FLAG_PARSE_ZLIB_HEADER | TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    if (status != TINFL_STATUS_DONE || inBytes != compLen || outBytes != rawRleLen) {
      free(unpacked);
      return false;
    }
    src = unpacked;
    srcLen = probe.dataStart + rawRleLen;
  }
  RasterHeader next;
  if (!rasterCheck(src,srcLen,next,RASTER_MAX_PX,RASTER_BUF_BYTES)) {
    if (unpacked) free(unpacked);
    return false;
  }
  if (next.dataEnd != srcLen) {
    if (unpacked) { free(unpacked); return false; }
    // 保留既有轮廓扩展的兼容路径；新 App 只发送无扩展的普通底图。
    if (len-next.dataEnd<8 || p[next.dataEnd]!='V' || p[next.dataEnd+1]!='C' || p[next.dataEnd+2]!='E') return false;
    rasterBackValid=false;
    parseRasterLegacy(p,len);
    return rasterValid;
  }
  if (next.delta) {
    bool canDelta = rasterDeltaBaseClean && rasterValid && !rasterViceValid && !rasHasPal &&
        rasterFrame == next.frame && rasW == next.width && rasH == next.height &&
        (uint16_t)lroundf(rasMpp * 1000) == next.mm &&
        rasX0 == next.refX0 && rasY0 == next.refY0 &&
        rasterAllocatedBytes == RASTER_BUF_BYTES && rasterBuf != nullptr;
    if (!canDelta) {
      if (unpacked) free(unpacked);
      return false;
    }
  }
  if (!rasterBackBuf) rasterBackBuf=(uint8_t *)ps_malloc(RASTER_BUF_BYTES);
  if (!rasterBackBuf) {
    if (unpacked) free(unpacked);
    return false;
  }
  rasterDecode(src,next,rasterBackBuf,next.delta ? rasterBuf : nullptr);
  if (unpacked) free(unpacked);
  RasterHeader old = {};
  old.frame=rasterFrame; old.x0=rasX0; old.y0=rasY0; old.width=rasW; old.height=rasH;
  old.mm=(uint16_t)lroundf(rasMpp*1000); old.blocksWide=rasBW;
  bool keep=rasterValid && !rasterViceValid && !rasHasPal && !next.palette &&
      rasterFrame==next.frame && old.mm==next.mm && rasterAllocatedBytes==RASTER_BUF_BYTES;
  old.palette=rasHasPal;old.buildings=rasterCarriesBuildings;
  old.bytes=(size_t)old.blocksWide*((old.height+15)/16)*128;
  rasterDeltaBaseClean=true;
  if(keep && rasterBuildingCarryOn && themeIdx==1 && rasterCarryRoofs(rasterBuf,old,rasterBackBuf,next)>0)
    rasterDeltaBaseClean=false;
  uint8_t *oldPixels=rasterBuf;
  rasterBuf=rasterBackBuf;
  rasterBackBuf=oldPixels;
  if (rasterAllocatedBytes!=RASTER_BUF_BYTES) { free(rasterBackBuf); rasterBackBuf=nullptr; keep=false; }
  rasterAllocatedBytes=RASTER_BUF_BYTES;
  rasterBackHeader=old; rasterBackValid=keep;
  rasterVice=nullptr; rasterViceValid=false; vicePatchData=nullptr; vicePatchCount=0;
  rasBW=next.blocksWide; rasX0=next.x0; rasY0=next.y0; rasW=next.width; rasH=next.height;
  rasMpp=next.mm/1000.0f; rasterFrame=next.frame; rasHasPal=next.palette;
  if (next.palette) for (int i=0;i<16;i++) rasPal[i]=rasterU16(p+16+2*i);
  rasterValid=true; rasterAt=millis();
  rasterCarriesBuildings=next.buildings;
  return true;
}

void setRasterBuildingCarry(bool on) {
  rasterBuildingCarryEpoch=mediaConnectionEpoch;
  rasterBuildingCarryOn=on;
  if(on) return;
  if(rasterValid && !rasHasPal && !rasterViceValid) {
    RasterHeader h={};h.bytes=(size_t)rasBW*((rasH+15)/16)*128;
    if(h.bytes<=RASTER_BUF_BYTES) rasterClearRoofs(rasterBuf,h);
    rasterCarriesBuildings=false;
    rasterDeltaBaseClean=false;
  }
  if(rasterBackValid) rasterClearRoofs(rasterBackBuf,rasterBackHeader);
  navDirty=true;
}
