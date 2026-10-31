#pragma once
#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define RASTER_FLAG_PALETTE   0x8000
#define RASTER_FLAG_BUILDINGS 0x4000
#define RASTER_FLAG_DEFLATE   0x2000
#define RASTER_FLAG_DELTA     0x1000
#define RASTER_MM_MASK        0x0FFF

// 校验完整游程后才允许写入备用底图；坏包不会使当前底图失效。
struct RasterHeader {
  uint16_t frame, width, height, mm;
  int32_t x0, y0;
  int32_t refX0, refY0;
  uint32_t dataStart, dataEnd;
  int blocksWide;
  size_t bytes;
  bool palette;
  bool buildings;
  bool deflate;
  bool delta;
};
static inline uint16_t rasterU16(const uint8_t *p) { return (uint16_t)(p[0] | ((uint16_t)p[1] << 8)); }
static inline uint32_t rasterU32(const uint8_t *p) {
  return (uint32_t)p[0] | ((uint32_t)p[1]<<8) | ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
static inline int32_t rasterI32(const uint8_t *p) { return (int32_t)rasterU32(p); }
static inline bool rasterHeaderCheck(const uint8_t *p, uint32_t len, RasterHeader &h, uint32_t maxPixels, size_t capacity) {
  if (!p || len<16) return false;
  h.frame=rasterU16(p); h.x0=rasterI32(p+2); h.y0=rasterI32(p+6);
  h.width=rasterU16(p+10); h.height=rasterU16(p+12);
  uint16_t flags=rasterU16(p+14);
  h.palette=(flags&RASTER_FLAG_PALETTE)!=0;
  h.buildings=(flags&RASTER_FLAG_BUILDINGS)!=0;
  h.deflate=(flags&RASTER_FLAG_DEFLATE)!=0;
  h.delta=(flags&RASTER_FLAG_DELTA)!=0;
  h.mm=flags&RASTER_MM_MASK;
  h.refX0=h.x0; h.refY0=h.y0;
  if (h.palette && (h.buildings || h.delta)) return false;
  uint32_t n=(uint32_t)h.width*h.height;
  if (!h.width || !h.height || !h.mm || n>maxPixels) return false;
  h.blocksWide=(h.width+15)/16;
  h.bytes=(size_t)h.blocksWide*((h.height+15)/16)*128;
  h.dataStart=h.palette ? 48 : (h.delta ? 24 : 16);
  if (h.bytes>capacity || len<h.dataStart) return false;
  if (h.delta) {
    h.refX0=rasterI32(p+16); h.refY0=rasterI32(p+20);
    int64_t dx=(int64_t)h.x0-h.refX0, dy=(int64_t)h.refY0-h.y0;
    if ((dx*1000)%h.mm!=0 || (dy*1000)%h.mm!=0) return false;
  }
  return true;
}
static inline bool rasterCheck(const uint8_t *p, uint32_t len, RasterHeader &h, uint32_t maxPixels, size_t capacity) {
  if (!rasterHeaderCheck(p,len,h,maxPixels,capacity) || h.deflate) return false;
  uint32_t n=(uint32_t)h.width*h.height;
  uint32_t pos=h.dataStart, count=0;
  uint8_t maxColor=h.palette ? 15 : ((h.buildings || h.delta) ? 7 : 3);
  while (pos<len && count<n) {
    uint8_t b=p[pos++]; uint32_t run=(b>>4)+1;
    if ((b>>4)==15) { if (pos>=len) return false; run=16+p[pos++]; }
    if ((b&15)>maxColor || run>n-count) return false;
    count+=run;
  }
  if (count!=n) return false;
  h.dataEnd=pos;
  return true;
}
static inline void rasterCopyReference(uint8_t *out, const uint8_t *ref, const RasterHeader &h) {
  int ox=(int)(((int64_t)h.x0-h.refX0)*1000/h.mm);
  int oy=(int)(((int64_t)h.refY0-h.y0)*1000/h.mm);
  if (ox==0 && oy==0) {
    memcpy(out,ref,h.bytes);
    return;
  }
  memset(out,0,h.bytes);
  int xStart=ox<0 ? -ox : 0;
  int xEnd=ox>0 ? (int)h.width-ox : (int)h.width;
  int yStart=oy<0 ? -oy : 0;
  int yEnd=oy>0 ? (int)h.height-oy : (int)h.height;
  if (xStart>=xEnd || yStart>=yEnd) return;
  if ((ox&15)==0 && (oy&15)==0 && (h.width&15)==0 && (h.height&15)==0) {
    int bxStart=xStart>>4, bxEnd=xEnd>>4, byStart=yStart>>4, byEnd=yEnd>>4;
    int bxOff=ox>>4, byOff=oy>>4;
    for (int by=byStart;by<byEnd;by++) {
      for (int bx=bxStart;bx<bxEnd;bx++) {
        memcpy(out+(((size_t)by*h.blocksWide+bx)<<7),
               ref+(((size_t)(by+byOff)*h.blocksWide+(bx+bxOff))<<7),128);
      }
    }
    return;
  }
  for (int y=yStart;y<yEnd;y++) {
    int ry=y+oy;
    uint32_t outRow=((uint32_t)(y>>4)*h.blocksWide<<8)|((y&15)<<4);
    uint32_t refRow=((uint32_t)(ry>>4)*h.blocksWide<<8)|((ry&15)<<4);
    for (int x=xStart;x<xEnd;x++) {
      int rx=x+ox;
      uint32_t refA=refRow+(((uint32_t)(rx>>4))<<8)+(rx&15);
      uint8_t c=(ref[refA>>1]>>((refA&1)<<2))&15;
      if (c) {
        uint32_t outA=outRow+(((uint32_t)(x>>4))<<8)+(x&15);
        out[outA>>1]|=(outA&1) ? (c<<4) : c;
      }
    }
  }
}
static inline void rasterDecode(const uint8_t *p, const RasterHeader &h, uint8_t *out, const uint8_t *ref = nullptr) {
  if (h.delta && ref) rasterCopyReference(out,ref,h);
  else memset(out,0,h.bytes);
  uint32_t pos=h.dataStart, i=0, n=(uint32_t)h.width*h.height;
  int x=0,y=0;
  while (i<n) {
    uint8_t b=p[pos++], color=b&15; uint32_t run=(b>>4)+1;
    if ((b>>4)==15) run=16+p[pos++];
    if (color) for (uint32_t j=0;j<run;j++) {
      uint32_t a=((uint32_t)((y>>4)*h.blocksWide+(x>>4))<<8)|((y&15)<<4)|(x&15);
      out[a>>1]^=(a&1) ? color<<4 : color;
      if (++x==h.width) { x=0; y++; }
    } else { x+=run; while (x>=h.width) { x-=h.width; y++; } }
    i+=run;
  }
  if (h.delta && !h.palette && !h.buildings) {
    for (size_t k=0;k<h.bytes;k++) out[k]&=0x33;
  }
}
static inline bool rasterRead(const uint8_t *pixels, const RasterHeader &h, int x, int y, uint8_t &color) {
  if (!pixels || (uint32_t)x>=h.width || (uint32_t)y>=h.height) return false;
  uint32_t k=((uint32_t)((y>>4)*h.blocksWide+(x>>4))<<8)|((y&15)<<4)|(x&15);
  uint8_t b=pixels[k>>1]; color=(k&1) ? b>>4 : b&15;
  return true;
}
