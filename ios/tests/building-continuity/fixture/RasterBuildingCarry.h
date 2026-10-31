#pragma once
#include "RasterContinuity.h"

// Merge once at packet decode time, using the two existing raster buffers.
// No extra BLE payload, framebuffer allocation or per-render sample is needed.
static inline uint32_t rasterCarryRoofs(const uint8_t *oldPixels,const RasterHeader &old,
                                       uint8_t *pixels,RasterHeader &next) {
  if(!oldPixels || !pixels || oldPixels==pixels || old.palette || next.palette || !old.buildings ||
     old.frame!=next.frame || !old.mm || old.mm!=next.mm) return 0;
  const int64_t dx=((int64_t)next.x0-old.x0)*1000,dy=((int64_t)old.y0-next.y0)*1000;
  if(dx%old.mm || dy%old.mm) return 0; // Never round a half-pixel shift into a ghost outline.
  const int64_t ox=dx/old.mm,oy=dy/old.mm;
  if(ox>=old.width || ox<=-(int64_t)next.width || oy>=old.height || oy<=-(int64_t)next.height) return 0;
  const int left=ox<0 ? (int)-ox:0,top=oy<0 ? (int)-oy:0;
  const int right=(int)((int64_t)old.width-ox<next.width ? (int64_t)old.width-ox:next.width);
  const int bottom=(int)((int64_t)old.height-oy<next.height ? (int64_t)old.height-oy:next.height);
  uint32_t retained=0;
  for(int y=top;y<bottom;y++) {
    const int sy=y+(int)oy;
    const uint32_t row=((uint32_t)(y>>4)*next.blocksWide)<<8;
    const uint32_t sourceRow=((uint32_t)(sy>>4)*old.blocksWide)<<8;
    for(int x=left;x<right;x++) {
      const uint32_t a=row+((uint32_t)(x>>4)<<8)+((y&15)<<4)+(x&15);
      uint8_t &target=pixels[a>>1];
      const uint8_t ground=(a&1) ? target>>4:target&15;
      if(ground!=0 && ground!=2) continue; // Current roads, water and new roofs always win.
      const int sx=x+(int)ox;
      const uint32_t b=sourceRow+((uint32_t)(sx>>4)<<8)+((sy&15)<<4)+(sx&15);
      const uint8_t roof=(b&1) ? oldPixels[b>>1]>>4:oldPixels[b>>1]&15;
      if(roof<4 || roof>7) continue;
      const uint8_t color=4|(roof&1)|(ground&2);
      target=(a&1) ? (target&15)|(color<<4):(target&240)|color;
      retained++;
    }
  }
  next.buildings=next.buildings || retained>0;
  return retained;
}
static inline void rasterClearRoofs(uint8_t *pixels,RasterHeader &h) {
  if(!pixels || h.palette) return;
  for(size_t i=0;i<h.bytes;i++) {
    uint8_t lo=pixels[i]&15,hi=pixels[i]>>4;
    if(lo>=4 && lo<=7) lo&=2;
    if(hi>=4 && hi<=7) hi&=2;
    pixels[i]=lo|(hi<<4);
  }
  h.buildings=false;
}
