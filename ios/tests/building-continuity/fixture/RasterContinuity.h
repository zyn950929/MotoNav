#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

// 校验完整游程后才允许写入备用底图；坏包不会使当前底图失效。
struct RasterHeader {
  uint16_t frame, width, height, mm;
  int32_t x0, y0;
  uint32_t dataStart, dataEnd;
  int blocksWide;
  size_t bytes;
  bool palette;
  bool buildings;
};
static inline uint16_t rasterU16(const uint8_t *p) { return (uint16_t)(p[0] | ((uint16_t)p[1] << 8)); }
static inline int32_t rasterI32(const uint8_t *p) {
  return (int32_t)((uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24));
}
static inline bool rasterCheck(const uint8_t *p, uint32_t len, RasterHeader &h, uint32_t maxPixels, size_t capacity) {
  if (!p || len<16) return false;
  h.frame=rasterU16(p); h.x0=rasterI32(p+2); h.y0=rasterI32(p+6);
  h.width=rasterU16(p+10); h.height=rasterU16(p+12);
  uint16_t flags=rasterU16(p+14); h.palette=(flags&0x8000)!=0; h.buildings=(flags&0x4000)!=0; h.mm=flags&0x3fff;
  if (h.palette && h.buildings) return false;
  uint32_t n=(uint32_t)h.width*h.height;
  if (!h.width || !h.height || !h.mm || n>maxPixels) return false;
  h.blocksWide=(h.width+15)/16;
  h.bytes=(size_t)h.blocksWide*((h.height+15)/16)*128;
  h.dataStart=h.palette ? 48 : 16;
  if (h.bytes>capacity || len<h.dataStart) return false;
  uint32_t pos=h.dataStart, count=0;
  while (pos<len && count<n) {
    uint8_t b=p[pos++]; uint32_t run=(b>>4)+1;
    if ((b>>4)==15) { if (pos>=len) return false; run=16+p[pos++]; }
    if ((!h.palette && (b&15)>(h.buildings ? 7 : 3)) || run>n-count) return false;
    count+=run;
  }
  if (count!=n) return false;
  h.dataEnd=pos;
  return true;
}
static inline void rasterDecode(const uint8_t *p, const RasterHeader &h, uint8_t *out) {
  memset(out,0,h.bytes);
  uint32_t pos=h.dataStart, i=0, n=(uint32_t)h.width*h.height;
  int x=0,y=0;
  while (i<n) {
    uint8_t b=p[pos++], color=b&15; uint32_t run=(b>>4)+1;
    if ((b>>4)==15) run=16+p[pos++];
    if (color) for (uint32_t j=0;j<run;j++) {
      uint32_t a=((uint32_t)((y>>4)*h.blocksWide+(x>>4))<<8)|((y&15)<<4)|(x&15);
      out[a>>1]|=(a&1) ? color<<4 : color;
      if (++x==h.width) { x=0; y++; }
    } else { x+=run; while (x>=h.width) { x-=h.width; y++; } }
    i+=run;
  }
}
static inline bool rasterRead(const uint8_t *pixels, const RasterHeader &h, int x, int y, uint8_t &color) {
  if (!pixels || (uint32_t)x>=h.width || (uint32_t)y>=h.height) return false;
  uint32_t k=((uint32_t)((y>>4)*h.blocksWide+(x>>4))<<8)|((y&15)<<4)|(x&15);
  uint8_t b=pixels[k>>1]; color=(k&1) ? b>>4 : b&15;
  return true;
}
