#include "gfx.h"
#include "assets.h"
#include <string.h>
#include <stdio.h>

#ifdef __arm__
#define FAST __attribute__((section(".itcm"),long_call))
#else
#define FAST
#endif
static int min(int a,int b){return a<b?a:b;}
static int max(int a,int b){return a>b?a:b;}
void FAST gfx_copy(uint16_t*d,const uint16_t*s,int count){
 if(!(((uintptr_t)d|(uintptr_t)s)&3)){
  uint32_t*dest=(uint32_t*)d;const uint32_t*src=(const uint32_t*)s;
  while(count>=8){uint32_t a=src[0],b=src[1],c=src[2],e=src[3];dest[0]=a;dest[1]=b;dest[2]=c;dest[3]=e;src+=4;dest+=4;count-=8;}
  while(count>=2){*dest++=*src++;count-=2;}
  if(count)*(uint16_t*)dest=*(const uint16_t*)src;
 }else{while(count>=4){uint16_t a=s[0],b=s[1],c=s[2],e=s[3];d[0]=a;d[1]=b;d[2]=c;d[3]=e;s+=4;d+=4;count-=4;}while(count--)*d++=*s++;}
}
void FAST gfx_rect(Surface*s,int x,int y,int w,int h,uint16_t c){
 int x1=max(0,x),y1=max(0,y),x2=min(s->w,x+w),y2=min(s->h,y+h);
 if(x2<=x1||y2<=y1)return;
 uint32_t pattern=c|((uint32_t)c<<16);
 for(int j=y1;j<y2;j++){
  uint16_t*p=s->pixels+j*s->w+x1;int n=x2-x1;
  if(((uintptr_t)p&3)&&n){*p++=c;n--;}
  uint32_t*q=(uint32_t*)p;
  while(n>=8){q[0]=pattern;q[1]=pattern;q[2]=pattern;q[3]=pattern;q+=4;n-=8;}
  while(n>=2){*q++=pattern;n-=2;}if(n)*(uint16_t*)q=c;
 }
}
void FAST gfx_card(Surface*s,int x,int y,int w,int h,uint16_t fill,uint16_t border){
 for(int j=0;j<h;j++){
  int edge=min(j,h-1-j),outer=edge==0?4:edge==1?2:edge<4?1:0;
  if(edge==0){gfx_rect(s,x+outer,y+j,w-2*outer,1,border);continue;}
  int inner=edge==1?4:edge==2?2:1;
  gfx_rect(s,x+inner,y+j,w-2*inner,1,fill);
  gfx_rect(s,x+outer,y+j,inner-outer,1,border);
  gfx_rect(s,x+w-inner,y+j,inner-outer,1,border);
 }
}
static uint16_t FAST blend(uint16_t a,uint16_t b,unsigned v){
 if(v>=248)return a;
 unsigned alpha=(v+8)>>4;if(alpha>16)alpha=16;unsigned iv=16-alpha;
 unsigned r=((a&31)*alpha+(b&31)*iv)>>4;
 unsigned g=((a>>5&31)*alpha+(b>>5&31)*iv)>>4;
 unsigned blue=((a>>10&31)*alpha+(b>>10&31)*iv)>>4;
 return 0x8000|r|(g<<5)|(blue<<10);
}
static void FAST __attribute__((noinline)) draw_text(Surface*s,int x,int y,const char*t,uint16_t c,const uint8_t*alpha,const uint8_t*advance){
 int origin=x;
 while(*t){unsigned ch=(unsigned char)*t++;if(ch=='\n'){x=origin;y+=12;continue;}if(ch<32||ch>127)ch='?';
  const uint8_t*p=alpha+(ch-32)*12*14;
  for(int j=0;j<14;j++)if(y+j>=0&&y+j<s->h)for(int i=0;i<12;i++)if(x+i>=0&&x+i<s->w&&p[j*12+i]){
   uint16_t*d=s->pixels+(y+j)*s->w+x+i;*d=blend(c,*d,p[j*12+i]);}
  x+=advance[ch-32];
 }
}
void gfx_text(Surface*s,int x,int y,const char*t,uint16_t c){draw_text(s,x,y,t,c,font_alpha,font_advance);}
void gfx_text_small(Surface*s,int x,int y,const char*t,uint16_t c){draw_text(s,x,y,t,c,font_small_alpha,font_small_advance);}
int gfx_text_width(const char*t){int n=0;while(*t){unsigned c=(unsigned char)*t++;n+=font_advance[(c>=32&&c<=127?c:'?')-32];}return n;}
void gfx_number(Surface*s,int x,int y,int n,uint16_t c){char b[20];snprintf(b,sizeof(b),"%d",n);gfx_text(s,x,y,b,c);}
void gfx_wrap(Surface*s,int x,int y,const char*t,int width,int lineheight,int maxlines,uint16_t c){
 char line[256];int len=0,row=0,w=0;
 while(*t&&row<maxlines){
  if(*t=='\n'){line[len]=0;gfx_text(s,x,y+row*lineheight,line,c);len=0;w=0;row++;t++;continue;}
  int n=0,wordw=0;while(t[n]&&t[n]!=' '&&t[n]!='\n'){unsigned ch=(unsigned char)t[n];wordw+=font_advance[(ch>=32&&ch<=127?ch:'?')-32];n++;}
  if(!n){if(len&&len<250){line[len++]=' ';w+=font_advance[0];}t++;continue;}
  if(len&&w+wordw>width){while(len&&line[len-1]==' ')len--;line[len]=0;gfx_text(s,x,y+row*lineheight,line,c);len=0;w=0;row++;if(row>=maxlines)return;}
  for(int i=0;i<n;i++){char ch=*t++;if(len<250)line[len++]=ch;}
  w+=wordw;
 }
 if(len&&row<maxlines){line[len]=0;gfx_text(s,x,y+row*lineheight,line,c);}
}
void FAST gfx_sprite(Surface*s,int x,int y,const uint16_t*p,int w,int h,int flip){
 for(int j=max(0,-y);j<min(h,s->h-y);j++)for(int i=max(0,-x);i<min(w,s->w-x);i++){
  uint16_t c=p[j*w+(flip?w-1-i:i)];if(c)s->pixels[(y+j)*s->w+x+i]=c;
 }
}
void gfx_line(Surface*s,int x,int y,int x2,int y2,uint16_t c){
 int dx=x2>x?x2-x:x-x2,sx=x<x2?1:-1,dy=-(y2>y?y2-y:y-y2),sy=y<y2?1:-1,err=dx+dy;
 for(;;){gfx_rect(s,x,y,1,1,c);if(x==x2&&y==y2)break;int e=2*err;if(e>=dy){err+=dy;x+=sx;}if(e<=dx){err+=dx;y+=sy;}}
}
void gfx_circle(Surface*s,int x,int y,int r,uint16_t c){for(int j=-r;j<=r;j++)for(int i=-r;i<=r;i++)if(i*i+j*j<=r*r)gfx_rect(s,x+i,y+j,1,1,c);}
static uint16_t FAST paint_color(uint16_t c,int paint){
 if(!paint)return c;
 int r=c&31,g=(c>>5)&31,b=(c>>10)&31;
 int hi=max(r,max(g,b)),lo=min(r,min(g,b));
 /* Preserve dark glass, tires, chrome and pale reflections. */
 if(hi<14||hi-lo<7||lo>22)return c;
 static const uint8_t colors[6][3]={{31,31,31},{31,12,10},{7,29,27},{31,25,7},{25,13,31},{17,20,23}};
 int light=(r*2+g*3+b)/6+4;
 r=min(31,colors[paint%6][0]*light/22);
 g=min(31,colors[paint%6][1]*light/22);
 b=min(31,colors[paint%6][2]*light/22);
 return (uint16_t)(0x8000|r|(g<<5)|(b<<10));
}
static uint16_t paint_lookup[5][32768] __attribute__((aligned(4)));
static int paint_ready;
void __attribute__((noinline)) gfx_init(void){
 if(paint_ready)return;
 for(int p=1;p<6;p++)for(int c=0;c<32768;c++)paint_lookup[p-1][c]=paint_color((uint16_t)(c|0x8000),p);
 paint_ready=1;
}
/* Exact scaled sprites are retained as opaque row runs. Repeated scenery
   therefore copies contiguous halfwords instead of resampling every pixel.
   The fixed pool stays below 800 KiB and is independent of road length. */
typedef struct {uint16_t x,y,length,offset;} SpriteRun;
typedef struct {
 const uint16_t *source;
 unsigned used;
 uint16_t sw,sh,w,h,paint,count;
 SpriteRun runs[512];
 uint16_t pixels[6144];
} SpriteCache;
static SpriteCache sprite_cache[48];
static unsigned sprite_clock;
static SpriteCache * __attribute__((noinline)) scaled_cache(const uint16_t *source,int sw,int sh,int w,int h,int paint){
 if(w>64||h>96)return 0;
 SpriteCache *entry=&sprite_cache[0];
 ++sprite_clock;
 for(int n=0;n<48;n++){
  SpriteCache*c=&sprite_cache[n];
  if(c->source==source&&c->sw==sw&&c->sh==sh&&c->w==w&&c->h==h&&c->paint==paint){c->used=sprite_clock;return c;}
  if(c->used<entry->used)entry=c;
 }
 entry->source=0;entry->used=sprite_clock;entry->count=0;
 int columns[64];
 for(int i=0;i<w;i++)columns[i]=i*sw/w;
 const uint16_t*map=paint?paint_lookup[paint-1]:0;
 unsigned stored=0;
 for(int j=0;j<h;j++){
  const uint16_t*row=source+(j*sh/h)*sw;
  SpriteRun *run=0;
  for(int i=0;i<w;i++){
   uint16_t c=row[columns[i]];
   if(!c){run=0;continue;}
   if(!run){
    if(entry->count>=512)return 0;
    run=&entry->runs[entry->count++];
    *run=(SpriteRun){(uint16_t)i,(uint16_t)j,0,(uint16_t)stored};
   }
   if(stored>=6144)return 0;
   entry->pixels[stored++]=map?map[c&32767]:c;
   run->length++;
  }
 }
 entry->sw=sw;entry->sh=sh;entry->w=w;entry->h=h;entry->paint=paint;entry->source=source;
 return entry;
}
static void FAST __attribute__((noinline)) cached_sprite(Surface*s,int x,int y,const SpriteCache*c){
 for(unsigned n=0;n<c->count;n++){
  const SpriteRun*r=&c->runs[n];int yy=y+r->y,xx=x+r->x,length=r->length,offset=r->offset;
  if((unsigned)yy>=(unsigned)s->h||xx>=s->w||xx+length<=0)continue;
  if(xx<0){offset-=xx;length+=xx;xx=0;}
  if(xx+length>s->w)length=s->w-xx;
  gfx_copy(s->pixels+yy*s->w+xx,c->pixels+offset,length);
 }
}
void FAST gfx_vehicle(Surface*s,int x,int y,const uint16_t*p,int sw,int sh,int dw,int dh,int paint){
 if(sw<=0||sh<=0||dw<=0||dh<=0||x>=s->w||y>=s->h||x+dw<=0||y+dh<=0)return;
 const uint16_t*map=0;
 paint%=6;
 if(paint){if(!paint_ready)gfx_init();map=paint_lookup[paint-1];}
 SpriteCache*cached=scaled_cache(p,sw,sh,dw,dh,paint);
 if(cached){cached_sprite(s,x,y,cached);return;}
 int columns[256],n=min(dw,256);
 /* Ceiling fixed-point steps preserve exact nearest-neighbor samples for
    DS-sized targets without software division in every source row/column. */
 unsigned step_x=((unsigned)sw*65536u+(unsigned)dw-1)/(unsigned)dw;
 unsigned step_y=((unsigned)sh*65536u+(unsigned)dh-1)/(unsigned)dh;
 for(int i=0;i<n;i++)columns[i]=(int)((i*step_x)>>16);
 int left=max(0,-x),right=min(n,s->w-x);
 for(int j=max(0,-y);j<min(dh,s->h-y);j++){
  const uint16_t*row=p+((j*step_y)>>16)*sw;uint16_t*dest=s->pixels+(y+j)*s->w+x;
  for(int i=left;i<right;i++){uint16_t c=row[columns[i]];if(c)dest[i]=map?map[c&32767]:c;}
 }
}
void FAST gfx_scaled_sprite(Surface*s,int x,int y,const uint16_t*p,int sw,int sh,int dw,int dh){gfx_vehicle(s,x,y,p,sw,sh,dw,dh,0);}
