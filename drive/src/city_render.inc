volatile uint32_t city_profile[8],city_last_profile[8];
static int city_abs(int x){return x<0?-x:x;}
typedef struct {int model,angle,pose,paint,pattern,color,wheels;unsigned used;uint16_t pixels[54*54];} CitySprite;
enum {CITY_SPRITE_CACHE=8};
static CitySprite city_sprites[CITY_SPRITE_CACHE];
static unsigned city_sprite_clock;
/* Each source now has a 256-color material palette and three steered-axle
 * drawings. Only one small indexed model bank is decoded at a time. */
static void city_pixels_pose(uint16_t*out,int model,int pose,const Save*save){
 static uint8_t art[3*48*72];static int last=-1;int bank=model;
 if(last!=bank){
  const uint8_t*src=overhead_lz+overhead_offsets[bank];unsigned in=4,pos=0;
  while(pos<sizeof art){unsigned flags=src[in++];
   for(unsigned bit=128;bit&&pos<sizeof art;bit>>=1){
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
static void city_shade(Surface*s,int x,int y,int w,int h){
 int x1=x<0?0:x,y1=y<0?0:y,x2=x+w>s->w?s->w:x+w,y2=y+h>s->h?s->h:y+h;
 for(int yy=y1;yy<y2;yy++)for(int xx=x1;xx<x2;xx++){
  uint16_t*p=s->pixels+yy*s->w+xx,c=*p;*p=(uint16_t)(0x8000|(((c&0x7bde)>>1)+((c&0x739c)>>2)));
 }
}
static void city_garden(Surface*s,int x,int y,int prop,int w,int h,unsigned frame);
static void city_building(Surface*s,int col,int row,int kind,int camx,int camy){
 CityLot lot=city_lot_bounds(col,row);int x=lot.x-camx,y=lot.y-camy,w=lot.w,h=lot.h;
 int height=kind==1||kind==4?14:10,style=city_building_style(col,row);
 int dw=w,dh=w*80/64;if(dh>h+height){dh=h+height;dw=dh*64/80;}
 int bx=x+(w-dw)/2,by=y+h-dh;
 city_shade(s,bx+dw-1,by+7,6,dh-5);city_shade(s,bx+5,y+h-1,dw-3,5);
 /* An enclosed garden fills long or wide lots instead of stretching a shop
  * into a tower. The planted boundary also makes its solid footprint clear. */
 if(h+height-dh>14||w-dw>16){
  gfx_rect(s,x,y,w,1,C(125,141,102));gfx_rect(s,x,y,1,h,C(125,141,102));gfx_rect(s,x+w-1,y,1,h,C(125,141,102));
  if(h+height-dh>14){
   city_garden(s,x+w/2,y+10,6,w-6,14,0);
   city_garden(s,x+w/2,y+25,11,w-8,24,0);
  }else city_garden(s,x+8,y+h/2,4,14,h-12,0);
 }
 gfx_rect(s,bx-1,y+h-2,dw+2,4,C(179,176,147));
 gfx_indexed_sprite(s,bx,by,city_building_idx+style*64*80,city_building_palette,64,80,dw,dh);
 /* These are signs painted onto real storefronts, with no UI panel. */
 static const char*signs[16]={"","","","","CAFE","BLOOM","SUNMART","BAKERY","POSTCARD","","","","MARINA","QUAYSIDE","GARDENS","AUTO CARE"};
 if(*signs[style]&&dw>=54){int textw=0;for(const char*p=signs[style];*p;p++)textw+=font_small_advance[(unsigned char)*p-32];
  gfx_text_small(s,bx+(dw-textw)/2,y+h-14,signs[style],style==5||style==13?C(255,241,193):C(243,238,216));}

}
static void city_prop(Surface*s,int x,int y,int prop,int w,int h){
 gfx_indexed_sprite(s,x-w/2,y-h/2,city_prop_idx+prop*48*48,city_prop_palette,48,48,w,h);
}
static void city_tree(Surface*s,int x,int y,int kind){
 /* Detailed canopy artwork replaces the eight rigid spokes. The tiny offset
  * is reserved for individual fronds rather than moving the whole tree. */
 int prop=kind%5;
 static const uint8_t widths[13]={7,10,13,15,16,17,17,17,16,15,13,10,7};
 for(int row=0;row<13;row++)city_shade(s,x+4-widths[row],y-1+row,widths[row]*2,1);
 city_prop(s,x,y-5,prop,48,48);
}
typedef struct {int y,type,a,b,c;} CityDraw;
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
static void city_garden(Surface*s,int x,int y,int prop,int w,int h,unsigned frame){
 unsigned phase=(frame/24)&1u;
 if(prop<8&&phase)prop+=16;
 gfx_indexed_sprite(s,x-w/2,y-h/2,city_garden_idx+prop*32*32,city_garden_palette,32,32,w,h);
}
static void city_add_garden(CityDraw*items,int*count,int wx,int wy,int prop,int w,int h,int camx,int camy){
 if(*count>=118||wx+w/2<camx||wx-w/2>camx+256||wy+h/2<camy||wy-h/2>camy+192)return;
 if(city_garden_clear(wx-w/2,wy-h/2,w,h))items[(*count)++]=(CityDraw){wy,4,wx,wy,prop|(w<<8)|(h<<16)};
}
static void city_scene(Surface*s,const Game*g){
 for(int k=0;k<8;k++)city_profile[k]=0;
 unsigned stage=profile_clock();
 int camx=city.camera_x/Q-128,camy=city.camera_y/Q-112;
 city_draw_ground(s,camx,camy);
 /* Only water highlights animate; every piece of road paint is map-fixed. */
 for(int wy=(camy/32)*32;wy<camy+s->h;wy+=32)for(int wx=(camx/64)*64;wx<camx+s->w;wx+=64)
  if(city_water(wx,wy)&&!city_on_road(wx,wy))gfx_rect(s,wx-camx+(g->frame/12)%5,wy-camy,10,1,C(76,167,176));
 city_profile[0]=(profile_clock()-stage)&65535;stage=profile_clock();
 /* Marina boats float in the canal; fingers are physically bank-attached. */
 for(int berth=0;berth<14;berth++){
  int wy=1248+berth*160;if(wy<camy-80||wy>camy+260)continue;
  int side=berth&1,wx=side?2240:1920,bx=side?wx-42:wx+42;
  gfx_rect(s,(side?wx-63:wx)-camx,wy-camy-3,63,7,C(113,88,60));
  gfx_rect(s,(side?wx-63:wx)-camx,wy-camy-3,63,1,C(199,171,113));
  for(int plank=0;plank<60;plank+=5)gfx_rect(s,(side?wx-63+plank:wx+plank)-camx,wy-camy-2,1,5,C(158,128,83));
  city_prop(s,bx-camx,wy-camy+27,8+berth%3,32,58);
 }
 for(int i=0;i<64;i++)if(!city.tokens[i]){const CityRoad*r=&city_roads[i%city_road_count];int span=r->x2-r->x1+r->y2-r->y1,p=(int)(road_hash(0x434f494eu,(unsigned)i)%(unsigned)span);
  int x=r->x1+(r->x2!=r->x1?p:12)-camx,y=r->y1+(r->y2!=r->y1?p:12)-camy;
  gfx_circle(s,x,y,3,C(246,194,73));gfx_rect(s,x,y-2,1,4,C(255,242,159));
 }
 city_profile[1]=(profile_clock()-stage)&65535;stage=profile_clock();
 /* Property palettes differ by district; all garden locations are stable
  * world coordinates. Planting clusters replace the old lone flower icons. */
 CityDraw items[128];int count=0;
 for(int row=clampi(camy/128-1,0,31);row<=clampi((camy+260)/128,0,31);row++)
  for(int col=clampi(camx/128-1,0,31);col<=clampi((camx+300)/128,0,31);col++){
   CityLot lot=city_lot_bounds(col,row);int wx=col*128+64,wy=row*128+64;
   unsigned decor=road_hash(0x47415244u,(unsigned)(row*32+col));
   if(lot.kind&&count<116){
    items[count++]=(CityDraw){lot.y+lot.h,0,col,row,0};
    int center=lot.x+lot.w/2,front=lot.y+lot.h+12;
    /* Planted entrances, short hedges, side-yard flowers and stone paths. */
    if(city_garden_clear(center-6,front-3,12,10))city_tile_rect(s,center-6,front-3,12,10,camx,camy,2);
    city_add_garden(items,&count,lot.x+6,front,(int)(decor%4),20,16,camx,camy);
    city_add_garden(items,&count,lot.x+lot.w-8,front,6,24,14,camx,camy);
    city_add_garden(items,&count,lot.x-12,lot.y+lot.h/2,4+(decor&1),14,30,camx,camy);
    city_add_garden(items,&count,lot.x+lot.w+12,lot.y+12,(int)((decor>>4)%4),18,18,camx,camy);
   }else if(city_garden_clear(wx-28,wy-26,56,52)){
    int park=(int)(decor%4);city_tile_rect(s,wx-28,wy-26,56,52,camx,camy,2);
    if(park==0){
     city_garden(s,wx-camx,wy-camy,13,42,40,g->frame);
     city_add_garden(items,&count,wx+23,wy+28,10,22,16,camx,camy);
    }else if(park==1){
     city_garden(s,wx-13-camx,wy-camy,8,26,26,g->frame);
     city_garden(s,wx+14-camx,wy+8-camy,9,24,24,g->frame);
     city_garden(s,wx-camx,wy-24-camy,6,44,14,g->frame);
    }else{
     city_prop(s,wx-camx,wy-camy,park==2?12:13,44,38);
     /* A few small moving glints keep courtyard water alive. */
     int phase=(int)((g->frame/8+decor)%17)-8;
     gfx_rect(s,wx-camx+phase,wy-camy+3,2,1,C(215,247,233));
    }
    city_add_garden(items,&count,wx-24,wy+23,(int)((decor>>5)%4),20,16,camx,camy);
    city_add_garden(items,&count,wx+25,wy-22,7,22,20,camx,camy);
   }
   /* Offset canopy groups and planted verges keep each block recognizable. */
   int tx=col*128+13+(int)((decor>>10)%9),ty=row*128+12+(int)((decor>>14)%9);
   if(!city_water(tx,ty)&&!city_on_road(tx,ty)&&!city_solid(tx,ty)&&count<116)
    items[count++]=(CityDraw){ty,2,tx,ty,(int)((decor>>8)%5)};
   if(row>18||(decor&3)==0){
    city_add_garden(items,&count,col*128+100,row*128+9,(int)((decor>>3)%4),24,20,camx,camy);
    city_add_garden(items,&count,col*128+12,row*128+103,7,28,22,camx,camy);
   }
  }
 /* Small sidewalk lamps are located by street station, not camera phase. */
 for(int i=0;i<city_road_count;i++){
  const CityRoad*r=&city_roads[i];int hor=r->y1==r->y2,view=hor?camx:camy,begin=hor?r->x1:r->y1,end=hor?r->x2:r->y2;
  for(int at=city_floor(view,256);at<view+(hor?256:192);at+=256)if(at>begin+65&&at<end-65)
   for(int side=-1;side<=1;side+=2){int x=hor?at:r->x1+side*(r->half+13),y=hor?r->y1+side*(r->half+13):at;
    city_add_garden(items,&count,x,y,15,18,18,camx,camy);}
 }
 for(int i=0;i<CITY_TRAFFIC;i++)if(city_abs(city.cars[i].x-camx-128)<180&&city_abs(city.cars[i].y-camy-96)<150&&count<127)items[count++]=(CityDraw){city.cars[i].y+17,1,i,0,0};
 items[count++]=(CityDraw){city.y/Q+17,3,0,0,0};
 for(int i=1;i<count;i++){CityDraw a=items[i];int j=i;while(j&&items[j-1].y>a.y){items[j]=items[j-1];--j;}items[j]=a;}
 city_profile[2]=(profile_clock()-stage)&65535;stage=profile_clock();
 for(int i=0;i<count;i++){CityDraw*d=&items[i];
  if(d->type==0){unsigned start=profile_clock();city_building(s,d->a,d->b,city_lot(d->a,d->b),camx,camy);city_profile[6]+=(profile_clock()-start)&65535;}
  else if(d->type==2)city_tree(s,d->a-camx,d->b-camy,d->c);
  else if(d->type==4){
   city_garden(s,d->a-camx,d->b-camy,d->c&255,d->c>>8&255,d->c>>16&255,g->frame);
   if((d->c&255)<4&&((d->a+d->b)&7)==0){
    int phase=(int)g->frame*3+d->a,px=d->a-camx+city_sin(phase)*5/256,py=d->b-camy-9+city_cos(phase)*3/256;
    gfx_rect(s,px,py,1,1,C(76,63,66));gfx_rect(s,px-1,py-((g->frame/6)&1),1,1,C(255,215,142));gfx_rect(s,px+1,py,1,1,C(255,175,145));
   }
  }
  else if(d->type==1){CityCar*c=&city.cars[d->a];city_draw_vehicle(s,c->x-camx,c->y-camy,c->angle,c->model,0);}
  else{
   if(g->drifting&&g->save.smoke[g->vehicle]){static const uint16_t smoke[5]={C(199,211,212),C(241,146,138),C(83,205,188),C(172,133,225),C(231,194,105)};int px=city.x/Q-camx-city_sin(city.angle)*21/256,py=city.y/Q-camy+city_cos(city.angle)*21/256;
    gfx_circle(s,px+3,py+2,4,smoke[g->save.smoke[g->vehicle]]);gfx_circle(s,px-3,py,3,smoke[g->save.smoke[g->vehicle]]);}
   city_draw_vehicle_steering(s,city.x/Q-camx,city.y/Q-camy,city.angle,g->steering,g->vehicle,&g->save);
  }
 }
 city_profile[3]=(profile_clock()-stage)&65535;
 for(int gull=0;gull<2;gull++){
  int wx=1960+(int)((g->frame/2+(unsigned)gull*97)%250),wy=1300+gull*740+(int)((g->frame/3)%900);
  int x=wx-camx,y=wy-camy;
  if(x>=3&&x<253&&y>=22&&y<190){int flap=(int)((g->frame/7+gull)&1u);
   gfx_line(s,x-3,y-flap,x,y+1,C(234,242,223));gfx_line(s,x,y+1,x+3,y-flap,C(234,242,223));}
 }
 const StudioTheme*theme=theme_for(g);overlay_panel(s,6,4,244,15,theme->pane);gfx_text_small(s,12,8,city_district(city.x/Q,city.y/Q),theme->ink);
 if(g->crash_timer){gfx_text_small(s,198,8,"IMPACT",C(255,145,104));}
 for(int k=0;k<8;k++)city_last_profile[k]=city_profile[k];
}
