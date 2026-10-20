#include "gfx.h"
#include "finish.h"
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
 }else{
  /* Sprite runs often have opposite halfword alignment. Keep source loads
     halfword-safe, but align VRAM writes and combine each pair into a word. */
  if(((uintptr_t)d&3)&&count){*d++=*s++;count--;}
  uint32_t*dest=(uint32_t*)d;
  while(count>=8){
   uint32_t a=s[0]|((uint32_t)s[1]<<16),b=s[2]|((uint32_t)s[3]<<16);
   uint32_t c=s[4]|((uint32_t)s[5]<<16),e=s[6]|((uint32_t)s[7]<<16);
   dest[0]=a;dest[1]=b;dest[2]=c;dest[3]=e;
   s+=8;dest+=4;count-=8;
  }
  while(count>=2){*dest++=s[0]|((uint32_t)s[1]<<16);s+=2;count-=2;}
  if(count)*(uint16_t*)dest=*s;
 }
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
 int left=max(0,-x),right=min(w,s->w-x),top=max(0,-y),bottom=min(h,s->h-y);
 for(int j=top;j<bottom;j++){
  const uint16_t*source=p+j*w;uint16_t*dest=s->pixels+(y+j)*s->w+x;
  for(int i=left;i<right;i++){uint16_t c=source[flip?w-1-i:i];if(c)dest[i]=c;}
 }
}
void FAST gfx_shadow_sprite(Surface*s,int x,int y,const uint16_t*p,int w,int h){
 int left=max(0,-x),right=min(w,s->w-x),top=max(0,-y),bottom=min(h,s->h-y);
 for(int j=top;j<bottom;j++){
  const uint16_t*source=p+j*w;uint16_t*dest=s->pixels+(y+j)*s->w+x;
  for(int i=left;i<right;i++)if(source[i])dest[i]=C(25,34,40);
 }
}
void gfx_line(Surface*s,int x,int y,int x2,int y2,uint16_t c){
 int dx=x2>x?x2-x:x-x2,sx=x<x2?1:-1,dy=-(y2>y?y2-y:y-y2),sy=y<y2?1:-1,err=dx+dy;
 for(;;){gfx_rect(s,x,y,1,1,c);if(x==x2&&y==y2)break;int e=2*err;if(e>=dy){err+=dy;x+=sx;}if(e<=dx){err+=dx;y+=sy;}}
}
void gfx_circle(Surface*s,int x,int y,int r,uint16_t c){for(int j=-r;j<=r;j++)for(int i=-r;i<=r;i++)if(i*i+j*j<=r*r)gfx_rect(s,x+i,y+j,1,1,c);}
/* Exact old channel*light/22 colors for every possible light0..35. Keeping
 * this tiny table avoids three repeated channel quotients on cold NPC fills. */
static const uint16_t paint_tint[6][36]={
 {0x8000,0x8421,0x8842,0x9084,0x94a5,0x9ce7,0xa108,0xa529,0xad6b,0xb18c,0xb9ce,0xbdef,0xc210,0xca52,0xce73,0xd6b5,0xdad6,0xdef7,0xe739,0xeb5a,0xf39c,0xf7bd,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff,0xffff},
 {0x8000,0x8001,0x8022,0x8424,0x8445,0x8847,0x8868,0x8c69,0x8c8b,0x908c,0x90ae,0x94cf,0x94d0,0x94f2,0x98f3,0x9915,0x9d16,0x9d37,0xa139,0xa15a,0xa55c,0xa57d,0xa99f,0xa99f,0xa9bf,0xadbf,0xaddf,0xb1df,0xb1ff,0xb5ff,0xb61f,0xba1f,0xba3f,0xbe5f,0xbe5f,0xbe7f},
 {0x8000,0x8420,0x8840,0x8c60,0x90a1,0x98c1,0x9ce1,0xa122,0xa542,0xad62,0xb1a3,0xb5c3,0xb9e3,0xbe24,0xc644,0xca64,0xcea5,0xd2c5,0xdae5,0xdf26,0xe346,0xe766,0xefa7,0xf3c7,0xf7e7,0xfbe7,0xffe8,0xffe8,0xffe8,0xffe9,0xffe9,0xffe9,0xffea,0xffea,0xffea,0xffeb},
 {0x8000,0x8021,0x8042,0x8064,0x8485,0x84a7,0x84c8,0x88e9,0x892b,0x894c,0x8d6e,0x8d8f,0x8db0,0x91d2,0x91f3,0x9235,0x9656,0x9677,0x9699,0x9aba,0x9adc,0x9afd,0x9f3f,0x9f5f,0x9f7f,0x9f9f,0xa3bf,0xa3df,0xa3ff,0xa7ff,0xa7ff,0xa7ff,0xabff,0xabff,0xabff,0xafff},
 {0x8000,0x8401,0x8822,0x9023,0x9444,0x9c45,0xa066,0xa487,0xac89,0xb0aa,0xb8ab,0xbccc,0xc0ed,0xc8ee,0xcd0f,0xd511,0xd932,0xdd53,0xe554,0xe975,0xf176,0xf597,0xfdb9,0xfdba,0xfddb,0xfddc,0xfdfd,0xfdfe,0xfe1f,0xfe3f,0xfe3f,0xfe5f,0xfe5f,0xfe7f,0xfe9f,0xfe9f},
 {0x8000,0x8400,0x8821,0x8c42,0x9063,0x9483,0x98a4,0x9cc5,0xa0e6,0xa506,0xa927,0xad48,0xb149,0xb56a,0xb98a,0xbdab,0xc1cc,0xc5ed,0xca0d,0xce2e,0xd24f,0xd670,0xde91,0xe291,0xe6b2,0xead3,0xeef4,0xf314,0xf735,0xfb56,0xff77,0xff97,0xffb8,0xffd9,0xffda,0xfffb}
};
/* Classification depends only on the original RGB555 color, so all five
 * paints share one byte per color. Zero protects glass/trim; eligible colors
 * store their exact light4..35. This replaces the old320KB color-paint table. */
static uint8_t paint_light[32768];
static int paint_ready;
void gfx_init(void){
 if(paint_ready)return;
 for(unsigned color=0;color<32768;color++){
  int r=color&31,g=(color>>5)&31,b=(color>>10)&31;
  int hi=max(r,max(g,b)),lo=min(r,min(g,b));
  paint_light[color]=(uint8_t)((hi<14||hi-lo<7||lo>22)?0:(r*2+g*3+b)/6+4);
 }
 paint_ready=1;
}
static inline uint16_t lookup_paint(uint16_t c,int paint){
 unsigned light=paint_light[c&32767u];
 return light?paint_tint[paint%6][light]:c;
}
/* Exact scaled sprites are retained as opaque row runs. Most perspective
   sprites are small, so a separate small pool keeps far scenery cached
   without reserving a full 64x96 buffer for every distant tree. Both pools
   have fixed memory use, independent of road length. */
typedef struct {uint16_t x,y,length,offset;} SpriteRun;
typedef struct {
 const void *source;
 const uint16_t *palette;
 unsigned used;
 uint16_t sw,sh,w,h,paint,count;
 SpriteRun *runs;
 uint16_t *pixels;
 uint16_t run_capacity,pixel_capacity;
} SpriteCache;
#define SMALL_CACHE_COUNT 64
#define LARGE_CACHE_COUNT 20
#define CACHE_COUNT (SMALL_CACHE_COUNT+LARGE_CACHE_COUNT)
static SpriteCache sprite_cache[CACHE_COUNT];
static SpriteRun small_runs[SMALL_CACHE_COUNT][256],large_runs[LARGE_CACHE_COUNT][512];
static uint16_t small_pixels[SMALL_CACHE_COUNT][2048] __attribute__((aligned(4)));
static uint16_t large_pixels[LARGE_CACHE_COUNT][7680] __attribute__((aligned(4)));
static unsigned sprite_clock;
static int sprite_cache_ready;
static void forget_sprite(const void *source){
 for(int n=0;n<CACHE_COUNT;n++)if(sprite_cache[n].source==source)sprite_cache[n].source=0;
}
static SpriteCache * __attribute__((noinline)) scaled_cache(const void *source,const uint16_t *palette,int sw,int sh,int w,int h,int paint){
 if(w>96||h>96)return 0;
 if(!sprite_cache_ready){
  for(int n=0;n<CACHE_COUNT;n++){
   SpriteCache*c=&sprite_cache[n];int small=n<SMALL_CACHE_COUNT,index=small?n:n-SMALL_CACHE_COUNT;
   c->runs=small?small_runs[index]:large_runs[index];
   c->pixels=small?small_pixels[index]:large_pixels[index];
   c->run_capacity=small?256:512;c->pixel_capacity=small?2048:7680;
  }
  sprite_cache_ready=1;
 }
 int first=w<=32&&h<=64?0:SMALL_CACHE_COUNT;
 int end=first?CACHE_COUNT:SMALL_CACHE_COUNT;
 SpriteCache *entry=&sprite_cache[first];
 ++sprite_clock;
 for(int n=first;n<end;n++){
  SpriteCache*c=&sprite_cache[n];
  if(c->source==source&&c->palette==palette&&c->sw==sw&&c->sh==sh&&c->w==w&&c->h==h&&c->paint==paint){c->used=sprite_clock;return c;}
  if(c->used<entry->used)entry=c;
 }
 entry->source=0;entry->used=sprite_clock;entry->count=0;
 int columns[96];
 /* Ceiling 16.16 steps retain exact nearest-neighbour samples for every
    cached size (at most96x96), without a software divide per row/column. */
 unsigned step_x=((unsigned)sw*65536u+(unsigned)w-1)/(unsigned)w;
 unsigned step_y=((unsigned)sh*65536u+(unsigned)h-1)/(unsigned)h;
 for(int i=0;i<w;i++)columns[i]=(int)((i*step_x)>>16);
 unsigned stored=0;
 for(int j=0;j<h;j++){
  unsigned row=((j*step_y)>>16)*(unsigned)sw;
  SpriteRun *run=0;
  for(int i=0;i<w;i++){
   unsigned at=row+(unsigned)columns[i];
   uint16_t c=palette?palette[((const uint8_t*)source)[at]]:((const uint16_t*)source)[at];
   if(!c){run=0;continue;}
   if(!run){
    if(entry->count>=entry->run_capacity)return 0;
    run=&entry->runs[entry->count++];
    *run=(SpriteRun){(uint16_t)i,(uint16_t)j,0,(uint16_t)stored};
   }
   if(stored>=entry->pixel_capacity)return 0;
   entry->pixels[stored++]=paint?lookup_paint(c,paint):c;
   run->length++;
  }
 }
 entry->sw=sw;entry->sh=sh;entry->w=w;entry->h=h;entry->paint=paint;entry->palette=palette;entry->source=source;
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
 paint%=6;
 if(paint&&!paint_ready)gfx_init();
 SpriteCache*cached=scaled_cache(p,0,sw,sh,dw,dh,paint);
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
  for(int i=left;i<right;i++){uint16_t c=row[columns[i]];if(c)dest[i]=paint?lookup_paint(c,paint):c;}
 }
}
void FAST gfx_indexed_sprite(Surface*s,int x,int y,const uint8_t*p,const uint16_t*palette,
 int sw,int sh,int dw,int dh){
 if(!p||!palette||sw<=0||sh<=0||dw<=0||dh<=0||x>=s->w||y>=s->h||x+dw<=0||y+dh<=0)return;
 SpriteCache*cached=scaled_cache(p,palette,sw,sh,dw,dh,0);
 if(cached){cached_sprite(s,x,y,cached);return;}
 int columns[256],n=min(dw,256);
 unsigned step_x=((unsigned)sw*65536u+(unsigned)dw-1)/(unsigned)dw;
 unsigned step_y=((unsigned)sh*65536u+(unsigned)dh-1)/(unsigned)dh;
 for(int i=0;i<n;i++)columns[i]=(int)((i*step_x)>>16);
 int left=max(0,-x),right=min(n,s->w-x);
 for(int j=max(0,-y);j<min(dh,s->h-y);j++){
  const uint8_t*row=p+((j*step_y)>>16)*sw;uint16_t*dest=s->pixels+(y+j)*s->w+x;
  for(int i=left;i<right;i++){uint16_t c=palette[row[columns[i]]];if(c)dest[i]=c;}
 }
}

#define VEHICLE_POSE_PIXELS (48*72)
static uint16_t decoded_vehicle[VEHICLE_POSE_PIXELS] __attribute__((aligned(4)));
static uint16_t styled_vehicle[VEHICLE_POSE_PIXELS] __attribute__((aligned(4)));
static uint8_t decoded_material[VEHICLE_POSE_PIXELS];
static int decoded_id=-1,decoded_pose=-1,styled_id=-1,styled_pose=-1;
static int styled_paint=-1,styled_livery=-1,styled_wheels=-1,styled_color=-1;

/* Decompress only when changing models; steering poses reuse the same bank.
 * The compressed library stays resident without paying 17 KiB per owned car. */
static uint8_t vehicle_bank[5*VEHICLE_POSE_PIXELS];
static int bank_id=-1;
static int unpack_bank(int vehicle){
 const uint8_t*src=vehicle_lz+vehicle_lz_offsets[vehicle];
 unsigned size=vehicle_lz_offsets[vehicle+1]-vehicle_lz_offsets[vehicle],in=4,out=0;
 if(size<4 || src[0]!=0x10 || ((unsigned)src[1]|(unsigned)src[2]<<8|(unsigned)src[3]<<16)!=sizeof vehicle_bank)return 0;
 while(out<sizeof vehicle_bank){if(in>=size)return 0;unsigned flags=src[in++];
  for(unsigned bit=128;bit && out<sizeof vehicle_bank;bit>>=1){
   if(flags&bit){if(in+2>size)return 0;unsigned code=(unsigned)src[in]<<8|src[in+1];in+=2;
    unsigned n=(code>>12)+3,back=(code&4095)+1;if(back>out || n>sizeof vehicle_bank-out)return 0;
    while(n--){vehicle_bank[out]=vehicle_bank[out-back];out++;}
   }else{if(in>=size)return 0;vehicle_bank[out++]=src[in++];}
  }
 }return 1;
}

const uint16_t *gfx_vehicle_pixels(int vehicle,int pose){
 if(vehicle<0||vehicle>=ASSET_VEHICLE_COUNT)vehicle=0;
 if(pose<0||pose>=5)pose=2;
 if(vehicle==decoded_id && pose==decoded_pose)return decoded_vehicle;
 forget_sprite(decoded_vehicle);
 if(bank_id!=vehicle){if(!unpack_bank(vehicle)){memset(decoded_vehicle,0,sizeof decoded_vehicle);return decoded_vehicle;}bank_id=vehicle;}
 const uint8_t*indices=vehicle_bank+pose*VEHICLE_POSE_PIXELS;
 const uint16_t*palette=vehicle_palette+vehicle*256;
 const uint8_t*flags=vehicle_palette_flags+vehicle*256;
 for(int i=0;i<VEHICLE_POSE_PIXELS;i++){
  decoded_vehicle[i]=palette[indices[i]];
  decoded_material[i]=flags[indices[i]];
 }
 decoded_id=vehicle;decoded_pose=pose;
 return decoded_vehicle;
}

static uint16_t material_tint(uint16_t c,int r,int g,int b){
 int light=min(31,((c&31)*2+((c>>5)&31)*3+((c>>10)&31))/6+9);
 return (uint16_t)(0x8000|(r*light/31)|((g*light/31)<<5)|((b*light/31)<<10));
}

static uint16_t material_paint(uint16_t c,int paint){
 const FinishColor*color=body_finish(paint);
 return paint?material_tint(c,color->r,color->g,color->b):c;
}

void gfx_vehicle_style(Surface*s,int x,int y,const uint16_t*source,int sw,int sh,
 int dw,int dh,int paint,int livery,int wheels,int livery_color){
 paint=(paint>=0&&paint<PAINT_COUNT)?paint:0;
 livery=(livery>=0&&livery<5)?livery:0;
 wheels=(wheels>=0&&wheels<4)?wheels:0;
 livery_color=(livery_color>=0&&livery_color<LIVERY_COLOR_COUNT)?livery_color:0;
 if(source!=decoded_vehicle || sw!=48 || sh!=72){gfx_vehicle(s,x,y,source,sw,sh,dw,dh,paint);return;}
 if(!paint&&!livery&&!wheels){gfx_vehicle(s,x,y,source,sw,sh,dw,dh,0);return;}
 if(styled_id!=decoded_id || styled_pose!=decoded_pose || styled_paint!=paint ||
    styled_livery!=livery || styled_wheels!=wheels || styled_color!=livery_color){
  static const uint8_t rims[4][3]={{0,0,0},{9,12,14},{28,30,31},{31,24,8}};
  forget_sprite(styled_vehicle);
  for(int at=0;at<VEHICLE_POSE_PIXELS;at++){
   uint16_t c=decoded_vehicle[at];unsigned material=decoded_material[at];
   if(c && (material&1u)){
    c=material_paint(c,paint);
    if(livery && (material & (1u<<(livery+1)))){
     const FinishColor*color=livery_finish(livery_color);
     /* Shade the decal from factory material, independently of body paint. */
     c=material_tint(decoded_vehicle[at],color->r,color->g,color->b);
    }
   }
   if(c && (material&2u) && wheels)c=material_tint(c,rims[wheels][0],rims[wheels][1],rims[wheels][2]);
   styled_vehicle[at]=c;
  }
  styled_id=decoded_id;styled_pose=decoded_pose;styled_paint=paint;styled_livery=livery;styled_wheels=wheels;styled_color=livery_color;
 }
 gfx_vehicle(s,x,y,styled_vehicle,48,72,dw,dh,0);
}
void FAST gfx_scaled_sprite(Surface*s,int x,int y,const uint16_t*p,int sw,int sh,int dw,int dh){
 /* Indexed scenery selects its prefiltered LOD before this layer. The RGB555
    entry point remains available for decoded vehicles and small UI art. */
 gfx_vehicle(s,x,y,p,sw,sh,dw,dh,0);
}
