#include "gfx.h"
#include "assets.h"
#include <string.h>
#include <stdio.h>

#define FAST __attribute__((section(".itcm"),long_call))
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
static void FAST draw_text(Surface*s,int x,int y,const char*t,uint16_t c,const uint8_t*alpha,const uint8_t*advance){
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
void FAST gfx_creature(Surface*s,int x,int y,int species,int size,int back,int frame){
 (void)frame;if(species<0||species>=6||size<=0)return;
 const uint16_t*p=(back?creature_back:creature_front)+species*96*96;
 if(size==96){gfx_sprite(s,x,y,p,96,96,0);return;}
 int columns[256],n=min(size,256);
 for(int i=0;i<n;i++)columns[i]=i*96/size;
 for(int j=max(0,-y);j<min(size,s->h-y);j++){
  const uint16_t*row=p+(j*96/size)*96;
  for(int i=max(0,-x);i<min(n,s->w-x);i++){uint16_t c=row[columns[i]];if(c)s->pixels[(y+j)*s->w+x+i]=c;}
 }
}
void gfx_line(Surface*s,int x,int y,int x2,int y2,uint16_t c){
 int dx=x2>x?x2-x:x-x2,sx=x<x2?1:-1,dy=-(y2>y?y2-y:y-y2),sy=y<y2?1:-1,err=dx+dy;
 for(;;){gfx_rect(s,x,y,1,1,c);if(x==x2&&y==y2)break;int e=2*err;if(e>=dy){err+=dy;x+=sx;}if(e<=dx){err+=dx;y+=sy;}}
}
void gfx_circle(Surface*s,int x,int y,int r,uint16_t c){for(int j=-r;j<=r;j++)for(int i=-r;i<=r;i++)if(i*i+j*j<=r*r)gfx_rect(s,x+i,y+j,1,1,c);}
void FAST gfx_map(Surface*s,int x,int y,int w,int h,const Game*g){
 int mw=w,mh=w*3/4;if(mh>h){mh=h;mw=h*4/3;}x+=(w-mw)/2;y+=(h-mh)/2;
 gfx_card(s,x-2,y-2,mw+4,mh+4,C(251,240,216),C(219,190,132));
 int columns[256];for(int i=0;i<mw&&i<256;i++)columns[i]=i*192/mw;
 for(int j=0;j<mh;j++){
  const uint16_t*row=world_thumbnail+(j*144/mh)*192;
  for(int i=0;i<mw&&i<256;i++){int xx=x+i,yy=y+j;if(xx>=0&&xx<s->w&&yy>=0&&yy<s->h)s->pixels[yy*s->w+xx]=row[columns[i]];}
 }
 if(!g->world)return;
 for(int i=0;i<4;i++){
  int px=x+g->world->npcs[i].x*mw/1024,py=y+g->world->npcs[i].y*mh/768;
  gfx_circle(s,px,py,3,C(21,44,66));gfx_circle(s,px,py,2,C(237,189,104));
 }
 int px=x+g->save.player_x*mw/1024,py=y+g->save.player_y*mh/768;
 gfx_circle(s,px,py,4,C(21,44,66));gfx_circle(s,px,py,3,C(255,249,222));gfx_circle(s,px,py,1,C(38,126,115));
}

static int map_visible(const Game*g,int x,int y){
 return x>=38&&x<218&&y>=29&&y<148&&!(g->ui_focus&&x>=51&&x<205&&y>=33&&y<49);
}
static void map_dot(Surface*s,const Game*g,int x,int y,int radius,uint16_t c){
 for(int j=-radius;j<=radius;j++)for(int i=-radius;i<=radius;i++)if(i*i+j*j<=radius*radius&&map_visible(g,x+i,y+j))s->pixels[(y+j)*s->w+x+i]=c;
}
void gfx_map_marker(Surface*s,const Game*g,int old_x,int old_y){
 int x=38+old_x*180/1024,y=29+old_y*135/768;
 for(int j=-4;j<=4;j++)for(int i=-4;i<=4;i++){
  int px=x+i,py=y+j;if(map_visible(g,px,py))s->pixels[py*s->w+px]=world_thumbnail[((py-29)*144/135)*192+(px-38)*192/180];
 }
 for(int i=0;i<4;i++){
  int px=38+g->world->npcs[i].x*180/1024,py=29+g->world->npcs[i].y*135/768;
  map_dot(s,g,px,py,3,C(21,44,66));map_dot(s,g,px,py,2,C(237,189,104));
 }
 x=38+g->save.player_x*180/1024;y=29+g->save.player_y*135/768;
 map_dot(s,g,x,y,4,C(21,44,66));map_dot(s,g,x,y,3,C(255,249,222));map_dot(s,g,x,y,1,C(38,126,115));
}
