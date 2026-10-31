#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <chrono>
#undef assert
#define assert(condition) do {if(!(condition)){fprintf(stderr,"CHECK FAILED line %d: %s\n",__LINE__,#condition);abort();}} while(false)
using std::min;using std::max;
#define PI 3.14159265358979323846
#define RASTER_BUF_BYTES 720000
#define RASTER_MAX_PX 1440000
#define RASTER_MAX_BYTES (500 * 1024)
#include "fixture/RasterContinuity.h"
struct tinfl_decompressor{};using tinfl_status=int;
constexpr int TINFL_FLAG_PARSE_ZLIB_HEADER=1,TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF=2,TINFL_STATUS_DONE=0;
void tinfl_init(tinfl_decompressor*){}
int tinfl_decompress(tinfl_decompressor*,const uint8_t*,size_t*,uint8_t*,uint8_t*,size_t*,int){return -1;}
void *ps_malloc(size_t n){return malloc(n);}
unsigned long millis(){return 1;}
int32_t rd32(const uint8_t*p){return rasterI32(p);}uint16_t rdu16(const uint8_t*p){return rasterU16(p);}
bool rasterValid=false,rasterBackValid=false,navDirty=false;int themeIdx=1;
uint32_t mediaConnectionEpoch=1;
uint8_t *rasterBuf=nullptr,*rasterBackBuf=nullptr;
RasterHeader rasterBackHeader={};
int rasBW=0,rasX0=0,rasY0=0,rasW=0,rasH=0,rasterFrame=0;
float rasMpp=2;unsigned long rasterAt=0;
// Includes the production decoder unchanged. Only hardware/miniz services are stubbed.
#include "fixture/src/map/RasterDecoder.h"
static inline uint16_t blendF(uint16_t a,uint16_t,uint32_t){return a;}
void u16(std::vector<uint8_t>&b,int n){b.push_back(n&255);b.push_back(n>>8&255);}
void i32(std::vector<uint8_t>&b,int n){for(int k=0;k<4;k++)b.push_back(uint32_t(n)>>(k*8)&255);}
std::vector<uint8_t> packet(const std::vector<uint8_t>&p,int n,int x=0,int y=0,bool roofs=false,int frame=1,int mm=2000,bool pal=false){
 std::vector<uint8_t>b;u16(b,frame);i32(b,x);i32(b,y);u16(b,n);u16(b,n);u16(b,mm|(roofs?0x4000:0)|(pal?0x8000:0));
 if(pal)for(int i=0;i<16;i++)u16(b,i*4096);
 for(size_t i=0;i<p.size();){size_t count=1;while(i+count<p.size()&&p[i+count]==p[i]&&count<271)count++;
  if(count<=15)b.push_back(((count-1)<<4)|p[i]);else{b.push_back(240|p[i]);b.push_back(count-16);}i+=count;}
 return b;
}
void deliver(const std::vector<uint8_t>&p){assert(parseRaster(p.data(),p.size()));}
uint8_t at(int x,int y){RasterHeader h={};h.width=rasW;h.height=rasH;h.blocksWide=rasBW;uint8_t c=255;assert(rasterRead(rasterBuf,h,x,y,c));return c;}
size_t roofCount(){size_t n=0;for(int y=0;y<rasH;y++)for(int x=0;x<rasW;x++)n+=at(x,y)>=4;return n;}
int main(){
 const int n=32;std::vector<uint8_t> base(n*n,0),detail=base;
 for(int y=3;y<25;y++)for(int x=3;x<25;x++)detail[y*n+x]=(x==3||y==3||x==24||y==24)?5:4;
 deliver(packet(detail,n,0,0,true));assert(roofCount()>0);
 deliver(packet(base,n));assert(roofCount()==0); // Reproduce old full/base blinking.
 setRasterBuildingCarry(true);assert(rasterBuildingCarryEpoch==mediaConnectionEpoch);
 deliver(packet(detail,n,0,0,true));
 for(int i=0;i<6;i++){deliver(packet(base,n));assert(roofCount()==22*22);}
 // The rendered base contains retained roofs, unlike the sender's ground-only reference.
 // Reject XOR against it so the existing NACK path requests a complete frame.
 auto delta=packet(base,n);delta[15]|=0x10;
 delta.insert(delta.begin()+16,8,0); // reference x0/y0 are both zero here
 assert(!rasterDeltaBaseClean);
 assert(!parseRaster(delta.data(),delta.size()) && roofCount()==22*22);
 // New ground/park semantics are kept; new roads and water supersede old roofs.
 std::vector<uint8_t> changed=base;changed[10*n+10]=1;changed[10*n+11]=3;changed[10*n+12]=2;
 deliver(packet(changed,n));assert(at(10,10)==1 && at(11,10)==3 && at(12,10)==6);
 deliver(packet(detail,n,0,0,true));
 deliver(packet(base,n,16,-8));assert(at(2,6)>=4 && at(24,6)==0); // Exact +8,+4 source pixels.
 deliver(packet(base,n,16,-8));assert(at(2,6)>=4); // Survives more than one base replacement.
 const size_t before=roofCount();auto bad=packet(base,n,16,-8);bad.pop_back();
 assert(!parseRaster(bad.data(),bad.size()) && roofCount()==before);
 setRasterBuildingCarry(false);assert(roofCount()==0);
 setRasterBuildingCarry(true);deliver(packet(base,n,16,-8));assert(roofCount()==0); // No back-buffer resurrection.
 deliver(packet(detail,n,0,0,true));deliver(packet(base,n,1,0));assert(roofCount()==0); // Half-pixel shift rejected.
 deliver(packet(detail,n,0,0,true));deliver(packet(base,n,0,0,false,2));assert(roofCount()==0);
 deliver(packet(detail,n,0,0,true));deliver(packet(base,n,0,0,false,1,4000));assert(roofCount()==0);
 deliver(packet(detail,n,0,0,true));themeIdx=5;deliver(packet(base,n));assert(roofCount()==0);themeIdx=1;
 std::vector<uint8_t> pal(n*n,5);deliver(packet(pal,n,0,0,false,1,2000,true));setRasterBuildingCarry(false);assert(at(0,0)==5);
 // Full-size scrolling test at the real 600 m handoff spacing; checking every
 // pixel ensures retained contours cannot move onto another street.
 setRasterBuildingCarry(true);const int w=1200;
 std::vector<uint8_t> whole(w*w,0);
 for(int y=20;y<1150;y+=50)for(int x=20;x<1150;x+=50)
  for(int j=y;j<y+20;j++)for(int i=x;i<x+20;i++)whole[j*w+i]=(i==x||j==y||i==x+19||j==y+19)?5:4;
 deliver(packet(whole,w,0,2400,true));
 double maxMs=0;
 for(int step=1;step<=3;step++){
  std::vector<uint8_t> road(w*w,0);for(int y=0;y<w;y++)road[y*w+500]=1;
  auto b=packet(road,w,step*600,2400);
  auto start=std::chrono::steady_clock::now();deliver(b);
  maxMs=max(maxMs,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
  for(int y=0;y<w;y++)for(int x=0;x<w;x++){
   int sourceX=x+step*300;uint8_t expected=x==500?1:(sourceX<w?whole[y*w+sourceX]:0);
   // Earlier step's new road at world x=1600 stays ground/road according to
   // the latest base; roofs cannot be resurrected after road/water replacement.
   if(sourceX==800 && step>=2 && x!=500)expected=0;
   if(sourceX==1100 && step>=3 && x!=500)expected=0;
   assert(at(x,y)==expected);
  }
 }
 printf("PASS production raster decoder: old blink reproduced; six base/detail handoffs retain roofs; exact 600m scrolling; road/water priority; clear, frame/resolution/style/palette and malformed-packet isolation. Host max decode+merge %.3fms (not ESP32 timing).\n",maxMs);
 free(rasterBuf);free(rasterBackBuf);
}
