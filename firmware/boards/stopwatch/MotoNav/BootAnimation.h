#pragma once
// Shared by both boards. Draws only into the existing 466 x 466 canvas.
// Stable brand IDs are also used by BootBrand.kt. No heap allocation or external files at runtime.
#include <stdint.h>
#include <math.h>
#include <string.h>
#include "BrandLogos.h"

namespace BootAnimation {
static const int Size = 466;
static const uint32_t Duration = 1900;
struct Brand { const char *name; uint32_t color; };
static const Brand Brands[] = {
  {"MOTONAV",0x38E6C1}, {"HONDA",0xFF354D}, {"YAMAHA",0x5688FF},
  {"SUZUKI",0x4D9EFF}, {"KAWASAKI",0x79E52B}, {"BMW",0x63B8FF},
  {"DUCATI",0xF43C45}, {"KTM",0xFF841F}, {"TRIUMPH",0xE1E8EF},
  {"HARLEY-DAVIDSON",0xFF972F}, {"APRILIA",0xFF4148},
  {"CFMOTO",0x36D4D9}, {"QJMOTOR",0xFF424E}, {"VOGE",0x44B5FF},
  {"ZONTES",0x58D4F5}, {"BENELLI",0x55CD88}, {"ROYAL ENFIELD",0xEFBC67}
};
static const int Count = sizeof(Brands) / sizeof(Brands[0]);
static const int Off = Count;
inline constexpr int valid(int id) { return id >= 0 && id <= Off ? id : 0; }
inline constexpr int parse(const char *s) {
  if (!s || !*s) return -1;
  int id = 0;
  for (int i = 0; s[i]; ++i) {
    if (i >= 2 || s[i] < '0' || s[i] > '9') return -1;
    id = id * 10 + s[i] - '0';
  }
  return id <= Off ? id : -1;
}
static_assert(valid(-1)==0 && valid(999)==0 && valid(Off)==Off,"Invalid stored brand must fall back");
static_assert(parse("0")==0 && parse("16")==16 && parse("17")==Off,"Stable protocol IDs");
static_assert(parse("")==-1 && parse("-1")==-1 && parse("18")==-1 && parse("1x")==-1 &&
              parse("999")==-1 && parse("1,2")==-1,"Reject malformed settings without a write");
inline uint16_t rgb(uint32_t c, float gain=1.f) {
  int r=((c>>16)&255)*gain, g=((c>>8)&255)*gain, b=(c&255)*gain;
  return ((r>>3)<<11)|((g>>2)<<5)|(b>>3);
}
inline float clamp(float n) { return n < 0 ? 0 : n > 1 ? 1 : n; }
inline float ease(float n) { n=clamp(n); return n*n*(3-2*n); }

class Painter {
  uint16_t *fb;
public:
  explicit Painter(uint16_t *p):fb(p){}
  void pixel(int x,int y,uint16_t c) {
    if ((unsigned)x < Size && (unsigned)y < Size) fb[y*Size+x]=c;
  }
  void light(int x,int y,uint16_t c) {
    if ((unsigned)x >= Size || (unsigned)y >= Size) return;
    // Preserve brighter strokes at joins; AA segment endpoints must not cut dark seams.
    uint16_t old=fb[y*Size+x];
    uint16_t r=(old&0xF800)>(c&0xF800)?old&0xF800:c&0xF800;
    uint16_t g=(old&0x07E0)>(c&0x07E0)?old&0x07E0:c&0x07E0;
    uint16_t b=(old&0x001F)>(c&0x001F)?old&0x001F:c&0x001F;
    fb[y*Size+x]=r|g|b;
  }
  void rect(int x,int y,int w,int h,uint16_t c) {
    for(int yy=y;yy<y+h;++yy) for(int xx=x;xx<x+w;++xx) pixel(xx,yy,c);
  }
  void line(float ax,float ay,float bx,float by,float width,uint32_t color,float gain=1.f) {
    float vx=bx-ax,vy=by-ay,den=vx*vx+vy*vy;
    float r=width*.5f;
    int x0=(int)floorf(fminf(ax,bx)-r-1),x1=(int)ceilf(fmaxf(ax,bx)+r+1);
    int y0=(int)floorf(fminf(ay,by)-r-1),y1=(int)ceilf(fmaxf(ay,by)+r+1);
    for(int y=y0;y<=y1;++y) for(int x=x0;x<=x1;++x) {
      float t=den>0 ? clamp(((x-ax)*vx+(y-ay)*vy)/den):0;
      float dx=x-ax-t*vx,dy=y-ay-t*vy;
      float a=clamp(r+.5f-sqrtf(dx*dx+dy*dy));
      if(a>0) light(x,y,rgb(color,gain*a));
    }
  }
  void arc(float cx,float cy,float radius,float start,float end,float width,uint32_t color,float gain=1.f) {
    const float rad=.01745329252f;
    for(float a=start;a<end;a+=3) {
      float b=fminf(a+3,end);
      line(cx+cosf(a*rad)*radius,cy+sinf(a*rad)*radius,
           cx+cosf(b*rad)*radius,cy+sinf(b*rad)*radius,width,color,gain);
    }
  }
  // 5 x 7 display lettering. Uppercase A-Z, space and hyphen only.
  void text(const char *s,int y,int scale,uint32_t color,float gain=1.f,int spacing=1) {
    static const uint8_t font[26][5]={
      {0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},
      {0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x22,0x1C},
      {0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},
      {0x3E,0x41,0x49,0x49,0x7A},{0x7F,0x08,0x08,0x08,0x7F},
      {0x41,0x41,0x7F,0x41,0x41},{0x20,0x40,0x41,0x3F,0x01},
      {0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
      {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},
      {0x3E,0x41,0x41,0x41,0x3E},{0x7F,0x09,0x09,0x09,0x06},
      {0x3E,0x41,0x51,0x21,0x5E},{0x7F,0x09,0x19,0x29,0x46},
      {0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},
      {0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},
      {0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},
      {0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43}
    };
    int n=strlen(s),advance=5*scale+spacing;
    int x=(Size-(n*advance-spacing))/2;
    for(int i=0;i<n;++i,x+=advance) {
      for(int col=0;col<5;++col) {
        uint8_t bits=s[i]>='A'&&s[i]<='Z'?font[s[i]-'A'][col]:s[i]=='-'?8:0;
        for(int row=0;row<7;++row) if(bits&(1<<row))
          rect(x+col*scale,y+row*scale,scale,scale,rgb(color,gain));
      }
    }
  }
};


inline void drawBadge(Painter &p,int id,float gain,uint32_t ms) {
  if(id > 0 && id <= BrandLogos::Count) {
    const BrandLogos::Logo &logo=BrandLogos::Logos[id-1];
    const int x0=(Size-logo.w)/2, y0=184-logo.h/2;
    uint32_t position=0;
    for(unsigned i=0;i<logo.pairs;++i) {
      uint16_t count=logo.data[i*2], color=logo.data[i*2+1];
      uint16_t faded=((int)(((color>>11)&31)*gain)<<11) |
                     ((int)(((color>>5)&63)*gain)<<5) | (int)((color&31)*gain);
      if(faded) {
        for(unsigned j=0;j<count;++j) {
          uint32_t offset=position+j;
          p.pixel(x0+offset%logo.w,y0+offset/logo.w,faded);
        }
      }
      position+=count;
    }
    return;
  }
  uint32_t color=Brands[0].color;
  // Keep the original default MotoNav motorcycle animation.
  p.arc(168,202,28,0,360,3,0xD7E4ED,gain);
  p.arc(300,202,28,0,360,3,0xD7E4ED,gain);
  p.arc(168,202,19,ms*.15f,ms*.15f+220,1,color,gain*.8f);
  p.arc(300,202,19,ms*.15f,ms*.15f+220,1,color,gain*.8f);
  p.line(168,202,205,157,3,color,gain);
  p.line(205,157,238,202,3,color,gain);
  p.line(168,202,238,202,3,color,gain);
  p.line(238,202,274,158,3,color,gain);
  p.line(274,158,300,202,4,0xEDF6FF,gain);
  p.line(274,158,264,139,3,0xEDF6FF,gain);
  p.line(264,139,248,139,3,0xEDF6FF,gain);
  p.line(202,155,227,147,5,0xEDF6FF,gain);
  p.line(227,147,254,155,5,0xEDF6FF,gain);
  p.line(179,151,202,155,5,0xEDF6FF,gain);
  p.line(177,151,164,141,3,color,gain);
  p.line(210,172,237,172,4,0xEDF6FF,gain);
  p.line(214,180,235,180,3,0xEDF6FF,gain);
  p.line(218,188,232,188,3,0xEDF6FF,gain);
  p.line(285,153,295,153,4,color,gain);
}

inline void render(uint16_t *fb,uint32_t ms,int id) {
  if(!fb) return;
  memset(fb,0,Size*Size*sizeof(uint16_t));
  id=valid(id);
  if(id==Off) return;
  Painter p(fb);
  uint32_t color=Brands[id].color;
  float intro=ease(ms/420.f), fade=1-ease((ms-1670.f)/230.f);
  float gain=intro*fade;
  float progress=ease(ms/1300.f);
  // Instrument bezel, moving sweep and sparse index marks.
  p.arc(233,233,211,-90,270,1,0x32414E,gain*.7f);
  p.arc(233,233,211,-90,-90+360*progress,3,color,gain*.85f);
  float head=-90+360*progress;
  p.arc(233,233,203,head-22,head,2,0xF3FAFF,gain*.8f);
  for(int a=0;a<360;a+=15) {
    float r=a*.01745329252f;
    float inner=(a%45==0)?191:196;
    p.line(233+cosf(r)*inner,233+sinf(r)*inner,
           233+cosf(r)*199,233+sinf(r)*199,1,0x667583,gain*.65f);
  }
  p.text("MOTONAV",89,1,0x92A4B4,gain,4);
  float bike=ease((ms-120.f)/500.f)*fade;
  drawBadge(p,id,bike,ms);
  float title=ease((ms-320.f)/500.f)*fade;
  int scale=strlen(Brands[id].name)>12?4:strlen(Brands[id].name)>8?5:6;
  p.text(Brands[id].name,263+(int)((1-title)*9),scale,0xEFF7FC,title,scale);
  p.line(205,324,261,324,2,color,title);
  p.text("RIDE YOUR WAY",348,1,0x8D9BA7,title,3);
}
} // namespace BootAnimation
