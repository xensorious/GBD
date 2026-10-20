volatile uint32_t city_profile[8],city_last_profile[8];
static int city_abs(int x){return x<0?-x:x;}
/* Each source now has a 256-color material palette and three steered-axle
 * drawings. Only one small indexed model bank is decoded at a time. */
static uint8_t city_pose_art[3*48*72];
static void city_pixels_pose(uint16_t*out,int model,int pose,const Save*save){
 uint8_t*art=city_pose_art;static int last=-1;int bank=model;
 if(last!=bank){
  const uint8_t*src=overhead_lz+overhead_offsets[bank];unsigned in=4,pos=0;
  while(pos<sizeof city_pose_art){unsigned flags=src[in++];
   for(unsigned bit=128;bit&&pos<sizeof city_pose_art;bit>>=1){
    if(flags&bit){unsigned code=(unsigned)src[in]<<8|src[in+1];in+=2;unsigned n=(code>>12)+3,back=(code&4095)+1;
     while(n--){art[pos]=art[pos-back];pos++;}
    }else art[pos++]=src[in++];
   }
  }last=bank;
 }
 int paint=save?save->paint[model]:0,livery=save?save->livery[model]:0,wheels=save?save->wheels[model]:0;
 const FinishColor*body=body_finish(paint),*decal=livery_finish(save?save->livery_color[model]:0);
 const uint16_t*source=overhead_palette+model*256;const uint8_t*flags=overhead_flags+model*256;
 static const uint8_t rims[4][3]={{16,18,20},{9,12,14},{28,30,31},{31,24,8}};
 uint16_t palette[256];
 for(int index=0;index<256;index++){
  uint16_t c=source[index];unsigned tag=flags[index];
  if(tag&1){
   const FinishColor*color=livery>0&&livery<5&&(tag&(1u<<(livery+1)))?decal:paint?body:0;
   if(color){int r=c&31,g=c>>5&31,b=c>>10&31,light=r>g?r:g;if(b>light)light=b;
    int spec=r<g?r:g;if(b<spec)spec=b;spec=spec>19?(spec-19)/4:0;
    c=(uint16_t)(0x8000|clampi(color->r*light/27+spec,0,31)|(clampi(color->g*light/27+spec,0,31)<<5)|(clampi(color->b*light/27+spec,0,31)<<10));}
  }else if((tag&2)&&wheels>0&&wheels<4){int shade=(c&31)+(c>>5&31)+(c>>10&31);shade=clampi(shade/3,8,31);
   c=(uint16_t)(0x8000|clampi(rims[wheels][0]*shade/24,0,31)|(clampi(rims[wheels][1]*shade/24,0,31)<<5)|(clampi(rims[wheels][2]*shade/24,0,31)<<10));}
  palette[index]=c;
 }
 for(unsigned at=0;at<48*72;at++)out[at]=palette[art[pose*48*72+at]];
}
void city_vehicle_pixels(uint16_t*out,int model,const Save*save){city_pixels_pose(out,model,1,save);}
#ifndef ARM9
typedef struct {int model,angle,pose,paint,pattern,color,wheels;unsigned used;uint16_t pixels[54*54];} CitySprite;
enum {CITY_SPRITE_CACHE=8};
static CitySprite city_sprites[CITY_SPRITE_CACHE];
static unsigned city_sprite_clock;
static uint16_t city_sample(const uint16_t*p,int u,int v){
 unsigned red=0,green=0,blue=0,n=0;
 for(int dy=-64;dy<=64;dy+=128)for(int dx=-64;dx<=64;dx+=128){
  int x=(u+dx)>>8,y=(v+dy)>>8;if((unsigned)x>=48||(unsigned)y>=72)continue;
  uint16_t c=p[y*48+x];if(c){red+=c&31;green+=c>>5&31;blue+=c>>10&31;++n;}
 }
 if(n<2)return 0;
 if(n==4){red>>=2;green>>=2;blue>>=2;}else if(n==2){red>>=1;green>>=1;blue>>=1;}else{red=red*10923>>15;green=green*10923>>15;blue=blue*10923>>15;}
 return (uint16_t)(0x8000|red|(green<<5)|(blue<<10));
}
void city_draw_vehicle_steering(Surface*s,int x,int y,int angle,int steer,int model,const Save*save){
 int pose=steer<0?0:steer>0?2:1;angle&=255;
 int paint=save?save->paint[model]:0,pattern=save?save->livery[model]:0,color=save?save->livery_color[model]:0,wheels=save?save->wheels[model]:0;
 unsigned call_start=profile_clock();
 CitySprite*cached=&city_sprites[0];++city_sprite_clock;
 for(int n=0;n<CITY_SPRITE_CACHE;n++){CitySprite*c=&city_sprites[n];
  if(c->used&&c->model==model&&c->pose==pose&&c->angle==angle&&c->paint==paint&&c->pattern==pattern&&c->color==color&&c->wheels==wheels){c->used=city_sprite_clock;gfx_shadow_sprite(s,x-25,y-24,c->pixels,54,54);gfx_sprite(s,x-27,y-27,c->pixels,54,54,0);city_profile[7]+=(profile_clock()-call_start)&65535;return;}
  if(c->used<cached->used)cached=c;
 }
 cached->model=model;cached->pose=pose;cached->angle=angle;cached->paint=paint;cached->pattern=pattern;cached->color=color;cached->wheels=wheels;cached->used=city_sprite_clock;
 unsigned render_start=profile_clock();
 static uint16_t pixels[48*72];city_pixels_pose(pixels,model,pose,save);
 city_profile[4]+=(profile_clock()-render_start)&65535;render_start=profile_clock();
 int sn=city_sin(angle)*3/2,cs=city_cos(angle)*3/2;uint16_t*rotated=cached->pixels;
 for(int yy=-27;yy<27;yy++){
  int u=(24<<8)-27*cs+yy*sn,v=(36<<8)+27*sn+yy*cs;
  uint16_t*dest=rotated+(yy+27)*54;
  for(int xx=-27;xx<27;xx++,u+=cs,v-=sn)*dest++=city_sample(pixels,u,v);
 }
 gfx_shadow_sprite(s,x-25,y-24,rotated,54,54);gfx_sprite(s,x-27,y-27,rotated,54,54,0);
 city_profile[5]+=(profile_clock()-render_start)&65535;city_profile[7]+=(profile_clock()-call_start)&65535;
}
void city_draw_vehicle(Surface*s,int x,int y,int angle,int model,const Save*save){city_draw_vehicle_steering(s,x,y,angle,0,model,save);}
#endif
void city_draw_map(Surface*s,int x,int y,int w,int h,const Game*g){
 gfx_rect(s,x,y,w,h,C(31,67,64));gfx_rect(s,x+w*1920/CITY_SIZE,y+h*1152/CITY_SIZE,w*320/CITY_SIZE,h*(CITY_SIZE-1152)/CITY_SIZE,C(37,100,131));
 gfx_rect(s,x+w*3712/CITY_SIZE,y,w*384/CITY_SIZE,h,C(37,100,131));
 for(int i=0;i<city_road_count;i++){const CityRoad*r=&city_roads[i];gfx_rect(s,x+r->x1*w/CITY_SIZE-1,y+r->y1*h/CITY_SIZE-1,(r->x2-r->x1)*w/CITY_SIZE+3,(r->y2-r->y1)*h/CITY_SIZE+3,r->highway?C(199,200,164):C(101,142,140));}
 if(city.active&&g){int px=x+(city.x/Q)*w/CITY_SIZE,py=y+(city.y/Q)*h/CITY_SIZE;gfx_circle(s,px,py,3,C(255,203,98));gfx_line(s,px,py,px+city_sin(city.angle)*7/256,py-city_cos(city.angle)*7/256,C(255,244,196));}
 (void)g;
}
/* Ground coordinates are never screen coordinates. In particular, 48 is
 * not a power of two: masking camera coordinates with 47 made dashes slide.
 * Clip paint to its own finite road before intersections erase the overlaps. */
static int city_floor(int value,int period){int r=value%period;return value-r-(r<0?period:0);}
static void city_tile_rect(Surface*s,int wx,int wy,int w,int h,int camx,int camy,int tile){
 int left=wx-camx,top=wy-camy,right=left+w,bottom=top+h;
 if(left<0)left=0;
 if(top<0)top=0;
 if(right>s->w)right=s->w;
 if(bottom>s->h)bottom=s->h;
 static const uint16_t base[6]={C(91,133,87),C(55,67,74),C(181,186,165),C(119,142,135),C(39,135,155),C(155,159,111)};
 static const uint16_t grain[6]={C(98,139,91),C(62,73,78),C(164,172,155),C(134,152,141),C(43,141,158),C(164,164,118)};
 if(right<=left||bottom<=top)return;
 if(tile==0){
  /* Three copies of each 128px lawn row allow a whole DS scanline to be
   * copied in one span, with no palette lookup or modulo per screen pixel. */
  for(int y=top;y<bottom;y++)for(int x=left;x<right;){
   int u=(x+camx)&127,n=384-u;if(n>right-x)n=right-x;
   gfx_copy(s->pixels+y*s->w+x,city_lawn_rgb+((y+camy)&31)*384+u,n);x+=n;
  }return;
 }
 gfx_rect(s,left,top,right-left,bottom-top,base[tile]);
 if(tile==3)return;
 /* Sparse world-fixed grain avoids repainting every background texel through
  * a wrapping sampler. Large flat spans use the existing fast span writer. */
 for(int y=city_floor(top+camy,16);y<bottom+camy;y+=16)
  for(int x=city_floor(left+camx,16);x<right+camx;x+=16){
   int xx=x-camx+((y&16)?11:3),yy=y-camy+7;
   if(yy>=top&&yy<bottom){int l=xx<left?left:xx,r=xx+2>right?right:xx+2;if(r>l)gfx_rect(s,l,yy,r-l,1,grain[tile]);}
  }

}
static void city_mark(Surface*s,int wx,int wy,int w,int h,int camx,int camy,uint16_t c){
 /* The set map has a hard extent even when the camera looks past an edge. */
 int right=wx+w,bottom=wy+h;if(wx<0)wx=0;if(wy<0)wy=0;
 if(right>CITY_SIZE)right=CITY_SIZE;
 if(bottom>CITY_SIZE)bottom=CITY_SIZE;
 if(right>wx&&bottom>wy)gfx_rect(s,wx-camx,wy-camy,right-wx,bottom-wy,c);
}
static void city_lawn_background(Surface*s,int camx,int camy){
 int best=-1,orientation=0,from=0,to=0;
 /* Do not copy lawn that will immediately be covered by an entire street.
  * The skipped rectangle is inside the exact sidewalk fill below. */
 for(int i=0;i<city_road_count;i++){
  const CityRoad*r=&city_roads[i];int half=r->half+10,a,b,area;
  if(r->x1==r->x2&&r->y1-half<=camy&&r->y2+half>=camy+s->h){
   a=clampi(r->x1-half-camx,0,s->w);b=clampi(r->x1+half-camx,0,s->w);area=(b-a)*s->h;
   if(area>best){best=area;orientation=1;from=a;to=b;}
  }else if(r->y1==r->y2&&r->x1-half<=camx&&r->x2+half>=camx+s->w){
   a=clampi(r->y1-half-camy,0,s->h);b=clampi(r->y1+half-camy,0,s->h);area=(b-a)*s->w;
   if(area>best){best=area;orientation=2;from=a;to=b;}
  }
 }
 if(best<=0)city_tile_rect(s,camx,camy,s->w,s->h,camx,camy,0);
 else if(orientation==1){city_tile_rect(s,camx,camy,from,s->h,camx,camy,0);city_tile_rect(s,camx+to,camy,s->w-to,s->h,camx,camy,0);}
 else{city_tile_rect(s,camx,camy,s->w,from,camx,camy,0);city_tile_rect(s,camx,camy+to,s->w,s->h-to,camx,camy,0);}
}
void city_draw_ground(Surface*s,int camx,int camy){
 if(camx<0||camy<256||camx+s->w>3712||camy+s->h>CITY_SIZE){
  city_tile_rect(s,camx,camy,s->w,s->h,camx,camy,4);
  city_tile_rect(s,0,256,3712,CITY_SIZE-256,camx,camy,0);
 }else city_lawn_background(s,camx,camy);
 city_tile_rect(s,1898,1152,364,CITY_SIZE-1152,camx,camy,5);
 city_tile_rect(s,1908,1152,344,CITY_SIZE-1152,camx,camy,3);
 city_tile_rect(s,1920,1152,320,CITY_SIZE-1152,camx,camy,4);
 /* Quayside paving and fixed joints give the water a physical bank. */
 for(int side=0;side<2;side++)for(int wy=city_floor(camy,32);wy<camy+s->h;wy+=32)
  if(wy>=1152)city_mark(s,side?2241:1909,wy,10,1,camx,camy,C(153,157,142));
 for(int i=0;i<city_road_count;i++){
  const CityRoad*r=&city_roads[i];int wx=r->x1-r->half,wy=r->y1-r->half,w=r->x2-r->x1+2*r->half,h=r->y2-r->y1+2*r->half;
  if(wx-camx>s->w||wy-camy>s->h||wx+w<camx||wy+h<camy)continue;
  city_tile_rect(s,wx-10,wy-10,w+20,h+20,camx,camy,2);
  city_tile_rect(s,wx-2,wy-2,w+4,h+4,camx,camy,3);
  city_tile_rect(s,wx,wy,w,h,camx,camy,1);
 }
 for(int i=0;i<city_road_count;i++){
  const CityRoad*r=&city_roads[i];int hor=r->y1==r->y2,begin=hor?r->x1:r->y1,end=hor?r->x2:r->y2;
  int fixed=hor?r->y1:r->x1,view=hor?camx:camy,extent=hor?s->w:s->h;
  if(fixed-r->half>(hor?camy+s->h:camx+s->w)||fixed+r->half<(hor?camy:camx)||end<view||begin>view+extent)continue;
  uint16_t yellow=C(222,183,102),white=C(215,218,200);
  for(int side=-1;side<=1;side+=2){
   if(hor){city_mark(s,begin,fixed+side*2,end-begin,1,camx,camy,yellow);
    city_mark(s,begin,fixed+side*(r->half-3),end-begin,1,camx,camy,C(164,175,174));}
   else{city_mark(s,fixed+side*2,begin,1,end-begin,camx,camy,yellow);
    city_mark(s,fixed+side*(r->half-3),begin,1,end-begin,camx,camy,C(164,175,174));}
  }
  if(r->highway)for(int at=city_floor(view,48);at<view+extent;at+=48){
   int a=at<begin?begin:at,b=at+24>end?end:at+24;
   if(b<=a)continue;
   for(int side=-1;side<=1;side+=2)
    if(hor)city_mark(s,a,fixed+side*28,b-a,2,camx,camy,white);
    else city_mark(s,fixed+side*28,a,2,b-a,camx,camy,white);
  }
  /* Expansion joints, raised pale parapets and regularly spaced piers. */
  if(hor&&r->y1>1152&&r->x1<=1920&&r->x2>=2240){
   for(int side=-1;side<=1;side+=2){int by=fixed+side*(r->half+3);
    city_mark(s,1920,by,320,3,camx,camy,C(208,214,197));city_mark(s,1920,by+3,320,1,camx,camy,C(99,127,130));
    for(int bx=1920;bx<2240;bx+=40)city_mark(s,bx,by-1,4,6,camx,camy,C(237,235,207));}
   for(int bx=1920;bx<=2240;bx+=80)city_mark(s,bx,fixed-r->half+5,1,2*r->half-10,camx,camy,C(50,66,72));
  }
 }
 for(int i=0;i<city_road_count;i++)if(city_roads[i].y1==city_roads[i].y2)
  for(int j=0;j<city_road_count;j++)if(city_roads[j].x1==city_roads[j].x2){
   const CityRoad*a=&city_roads[i],*b=&city_roads[j];int wx=b->x1,wy=a->y1;
   if(wx<a->x1||wx>a->x2||wy<b->y1||wy>b->y2)continue;
   if(wx-b->half>camx+s->w||wx+b->half<camx||wy-a->half>camy+s->h||wy+a->half<camy)continue;
   city_tile_rect(s,wx-b->half,wy-a->half,2*b->half,2*a->half,camx,camy,1);
   if(!a->highway||!b->highway){
    for(int side=-1;side<=1;side+=2){
     for(int n=-18;n<20;n+=7){
      city_mark(s,wx+n,wy+side*(a->half-9),4,7,camx,camy,C(219,216,191));
      if(!a->highway)city_mark(s,wx+side*(b->half-9),wy+n,7,4,camx,camy,C(219,216,191));}
     city_mark(s,wx+side*(b->half+2),wy+a->half+2,4,4,camx,camy,C(230,199,131));
     city_mark(s,wx+side*(b->half+7),wy+a->half+2,3,7,camx,camy,C(49,68,69));
     city_mark(s,wx+side*(b->half+7),wy+a->half+2,2,2,camx,camy,C(221,108,81));
    }
   }
  }
 /* Drain grates and sidewalk joints stay in the same blocks as the asphalt. */
 for(int i=0;i<city_road_count;i++){
  const CityRoad*r=&city_roads[i];int hor=r->y1==r->y2,fixed=hor?r->y1:r->x1;
  int view=hor?camx:camy,begin=hor?r->x1:r->y1,end=hor?r->x2:r->y2;
  for(int at=city_floor(view,64);at<view+(hor?s->w:s->h);at+=64)if(at>begin+60&&at<end-60)
   for(int side=-1;side<=1;side+=2){
    int xx=hor?at:fixed+side*(r->half+6),yy=hor?fixed+side*(r->half+6):at;
    city_mark(s,xx,yy,hor?1:4,hor?4:1,camx,camy,C(146,160,150));
    if(!(at&127))for(int n=0;n<3;n++)city_mark(s,xx+(hor?n*2:0),yy+(hor?0:n*2),1,1,camx,camy,C(72,91,91));
   }
 }
}
int city_building_style(int col,int row){
 unsigned h=road_hash(0x43495459u,(unsigned)(col+row*32));
 static const uint8_t coast[8]={0,1,2,3,4,11,12,13};
 static const uint8_t downtown[8]={4,5,6,7,8,9,10,15};
 static const uint8_t quay[8]={3,4,8,9,10,11,12,13};
 static const uint8_t gardens[8]={0,1,2,4,5,7,14,1};
 static const uint8_t heights[8]={0,1,2,3,3,4,7,14};
 return row<7?coast[h%8]:row>18?(col>17?heights[h%8]:gardens[h%8]):col>17?quay[h%8]:downtown[h%8];
}
/* Decorative beds and street furniture use an actual footprint test; they
 * never appear in a traffic lane, canal, solid building, or outside the city. */
int city_garden_clear(int x,int y,int w,int h){
 if(x<160||y<320||x+w>3648||y+h>3800)return 0;
 if(city_water(x,y)||city_water(x+w,y)||city_water(x,y+h)||city_water(x+w,y+h))return 0;
 for(int i=0;i<city_road_count;i++){
  const CityRoad*r=&city_roads[i];
  if(x+w>=r->x1-r->half&&x<=r->x2+r->half&&y+h>=r->y1-r->half&&y<=r->y2+r->half)return 0;
 }
 for(int row=y/128;row<=(y+h)/128;row++)for(int col=x/128;col<=(x+w)/128;col++){
  CityLot lot=city_lot_bounds(col,row);
  if(lot.kind&&x+w>=lot.x&&x<=lot.x+lot.w&&y+h>=lot.y&&y<=lot.y+lot.h)return 0;
 }return 1;
}
#include "city_mesh.inc"
