#include "render.h"
#include "gfx.h"
#include "assets.h"
#include "road.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>

/* The renderer never consumes the simulation's random stream.  Every roadside
 * detail has a world coordinate, including while reversing past it. */
extern void ui_draw_bottom(Surface *,const Game *);

#ifdef ARM9
extern unsigned ds_profile_clock(void);
static unsigned profile_clock(void){return ds_profile_clock();}
#else
static unsigned profile_clock(void){return 0;}
#endif
/* Microsecond-scale ARM9 timer ticks: top, bottom, total, road, objects. */
volatile uint32_t render_profile[5];

#ifdef __arm__
#define FAST __attribute__((section(".itcm"),long_call))
#else
#define FAST
#endif
#define VW 256
#define VH 192
#define HORIZON 20
#define CAMERA 190
#define PLAYER_Y 160
#define FAR_DISTANCE 2200
#define DRAW_MAX 112
#define CAR_W 48
#define CAR_H 72
#define TRAFFIC_W 40
#define TRAFFIC_H 64
#define PROP_W 64
#define PROP_H 96

enum { SC_BUILDING=0,SC_SHOP=1,SC_WAREHOUSE=2,SC_PALM=3,SC_PINE=4,SC_TREE=5,
       SC_BUSH=6,SC_ROCK=7,SC_LAMP=8,SC_GANTRY=9,SC_BILLBOARD=10,SC_SAIL=11,
       SC_MANGROVE=12,SC_CITRUS=13,SC_HOTEL=14,SC_REEDS=15,SC_VILLA=16,SC_PIER=17,
       SC_BARRIER=18 };
enum { ITEM_PROP,ITEM_CAR,ITEM_COIN,ITEM_PLAYER,ITEM_OVERPASS,ITEM_BACK_PROP };
typedef struct { int z,x,kind,id,variant; } DrawItem;
typedef struct { int x,z;unsigned tick; } TireMark;
typedef struct {
 unsigned seed,frame;
 int position,visual_position,visual_scale,center,cam_x,start,attract,speed,motion;
} View;

static int row_z[VH],row_scale[VH],road_x[VH],road_half[VH];
static uint8_t row_biome[VH];
static uint16_t row_feature[VH];
static TireMark tiremarks[24];
static unsigned tire_cursor,last_trail_tick;
static uint32_t trail_seed;

static int clampi(int value,int low,int high)
{return value<low?low:value>high?high:value;}

static int floor_div(int value,int divisor)
{ int result=value/divisor;return value<0 && value%divisor?result-1:result; }

static uint16_t blend_color(uint16_t a,uint16_t b,int amount)
{
 int opposite=256-amount;
 int r=(((a&31)*opposite+(b&31)*amount)>>8);
 int g=((((a>>5)&31)*opposite+((b>>5)&31)*amount)>>8);
 int bl=((((a>>10)&31)*opposite+((b>>10)&31)*amount)>>8);
 return (uint16_t)(0x8000|r|(g<<5)|(bl<<10));
}

static int material_blend(unsigned amount)
{return (int)amount+(amount>=128);}

/* A high, close chase camera: the bottom of the frame has useful lateral
 * space, while the upper frame can show distant bends without a horizon. */
static int project_y(int z)
{return HORIZON+((PLAYER_Y-HORIZON)*CAMERA)/(z+CAMERA);}

static int project_scale(int z)
{return (276*CAMERA)/(z+CAMERA);}

static int item_x(const View *v,int world_x,int z)
{return 128+((world_x-v->center-v->cam_x)*project_scale(z))/256;}

static void init_projection(void)
{
 static int initialized;
 if(initialized)return;
 initialized=1;
 for(int y=0;y<VH;y++) {
  if(y<=HORIZON){row_z[y]=FAR_DISTANCE;row_scale[y]=0;continue;}
  row_z[y]=clampi(((PLAYER_Y-y)*CAMERA)/(y-HORIZON),-44,FAR_DISTANCE);
  row_scale[y]=(276*(y-HORIZON))/(PLAYER_Y-HORIZON);
 }
}

static __attribute__((noinline)) void ellipse(Surface *s,int cx,int cy,int width,int height,uint16_t color)
{
 int half=height/2;
 for(int y=0;y<height;y++) {
  int d=y-half;
  int w=width-(width*d*d)/(half*half+1);
  if(w>0)gfx_rect(s,cx-w/2,cy+y-half,w,1,color);
 }
}

static void shadow(Surface *s,int x,int y,int w,int h)
{ellipse(s,x+2,y+2,w,h,C(26,43,48));}

static __attribute__((noinline)) int visual_travel(int position,int motion)
{
 /* Optical travel has no effect on traffic, road curves, distance or scoring.
  * Splitting before the multiply also keeps a full-length run inside int32. */
 int scale=motion==0?205:motion==2?384:307;
 return (position/128)*scale+((position%128)*scale)/128;
}

static __attribute__((noinline)) int physical_anchor(int visual_position,int scale)
{
 return (visual_position/scale)*128+((visual_position%scale)*128)/scale;
}

static __attribute__((noinline)) void contact_shadow(Surface *s,int x,int y,int w,int h,
 const uint8_t *bounds,int source_w,int source_h,int yaw)
{
 int left=x+bounds[0]*w/source_w,top=y+bounds[1]*h/source_h;
 int width=(bounds[2]-bounds[0])*w/source_w;
 int height=(bounds[3]-bounds[1])*h/source_h;
 int bend=clampi(yaw/40,-2,2);
 /* The contact patch follows the complete chassis footprint.  Its front
  * shears gently with the steering; the rear axle remains planted. */
 for(int row=0;row<height;row++) {
  int edge=row<height-1-row?row:height-1-row;
  int inset=edge==0?width/3:edge==1?width/5:edge<4?width/10:0;
  int shift=bend*(height-1-row)/(height?height:1);
  gfx_rect(s,left+shift+inset-1,top+row+1,width-inset*2+2,1,C(30,44,49));
 }
}

static int biome_water(int biome,int side)
{return biome==CAUSEWAY || biome==WETLANDS || ((biome==COAST || biome==MARINA) && side<0)?256:0;}

static uint16_t terrain_color(int biome)
{
 static const uint16_t colors[LOCATION_COUNT]={
  C(157,173,164),C(119,176,116),C(41,144,169),C(67,113,85),C(192,136,91),
  C(59,77,98),C(84,129,107),C(152,177,103)};
 return colors[biome%LOCATION_COUNT];
}

static uint16_t verge_color(int biome)
{
 static const uint16_t colors[LOCATION_COUNT]={
  C(221,212,187),C(230,215,166),C(202,216,208),C(149,156,119),C(222,170,118),
  C(106,132,145),C(163,177,137),C(193,180,127)};
 return colors[biome%LOCATION_COUNT];
}

static uint16_t sea_color(int biome)
{return biome==MARINA?C(27,86,123):biome==WETLANDS?C(57,118,118):C(35,151,180);}

static int bridge_region(int biome)
{return biome==CAUSEWAY || biome==WETLANDS;}

typedef struct {uint16_t sky,sky_low,floor,floor_low,pane,ink,muted,accent,rim;} StudioTheme;
static const StudioTheme studio_themes[SKIN_COUNT]={
 {C(180,230,233),C(248,236,224),C(247,255,250),C(206,235,234),C(230,249,244),C(28,66,83),C(82,124,132),C(4,137,151),C(150,198,197)},
 {C(248,195,181),C(218,185,223),C(255,235,225),C(239,208,212),C(251,225,226),C(76,50,78),C(139,98,126),C(186,80,111),C(213,158,179)},
 {C(17,35,61),C(41,62,82),C(49,78,99),C(27,54,77),C(35,66,89),C(236,250,247),C(145,190,201),C(105,233,220),C(77,124,144)}
};

static const StudioTheme *theme_for(const Game *g)
{return &studio_themes[clampi(g->save.skin,0,SKIN_COUNT-1)];}

static __attribute__((noinline)) void draw_sky(Surface *s,const View *v,const RoadSample *region)
{
 int transition=material_blend(region->blend);
 int night=(region->biome==MARINA)*(256-transition)+(region->next_biome==MARINA)*transition;
 for(int y=0;y<=HORIZON;y++) {
  uint16_t day=blend_color(C(82,168,211),C(189,225,225),y*256/HORIZON);
  uint16_t dark=blend_color(C(16,28,57),C(47,80,113),y*256/HORIZON);
  gfx_rect(s,0,y,VW,1,blend_color(day,dark,night));
 }
 int biome=region->blend>=128?region->next_biome:region->biome;
 uint16_t distant=blend_color(C(122,172,177),C(42,74,104),night);
 if(biome==CITY || biome==MARINA) {
  for(int n=0;n<16;n++) {
   int x=n*17-3;
   if(x>108 && x<142)continue;
   unsigned h=road_hash(v->seed^0x6163u,(unsigned)n);
   int height=3+(h&7);
   gfx_rect(s,x,HORIZON-height,10+(h>>7&3),height+1,distant);
   if(night && (h&1))gfx_rect(s,x+3,HORIZON-height+2,2,1,C(159,204,215));
  }
 } else if(biome==PINES || biome==CANYON) {
  for(int n=0;n<4;n++) {
   int x=n<2?24+n*46:162+(n-2)*44;
   int height=5+(int)(road_hash(v->seed,(unsigned)n)&5);
   for(int row=0;row<height;row++) {
    int half=biome==CANYON?11+row:row*2+3;
    gfx_rect(s,x-half,HORIZON-height+row,half*2,1,distant);
   }
  }
 } else if(biome==GROVES || biome==WETLANDS) {
  for(int n=0;n<16;n++) {
   int x=n*18-4;if(x>112 && x<139)continue;
   int h=2+(int)(road_hash(v->seed,(unsigned)n)&3);
   gfx_rect(s,x,HORIZON-h,12,h+1,distant);
  }
 } else {
  gfx_rect(s,0,HORIZON-2,102,3,C(133,193,201));
  gfx_rect(s,148,HORIZON-2,108,3,C(133,193,201));
  ellipse(s,41,HORIZON-1,44,3,distant);
  ellipse(s,207,HORIZON,56,3,distant);
 }
 if(night<200)for(int i=0;i<2;i++) {
  int x=(int)((v->frame/18+(unsigned)i*157)%320)-28;
  int y=4+i*4;
  uint16_t cloud=blend_color(C(216,237,233),C(72,104,131),night);
  gfx_rect(s,x,y,22,2,cloud);gfx_rect(s,x+5,y-1,12,1,cloud);
 }
}

/* Road rows always have a valid y coordinate.  Keep the clipped fill in the
 * ITCM caller and avoid generic rectangle setup for each one-pixel span. */
static FAST __attribute__((noinline)) void road_span(Surface *s,int x,int y,int width,uint16_t color)
{
 if(x<0){width+=x;x=0;}
 if(x+width>VW)width=VW-x;
 if(width<=0)return;
 uint16_t *p=s->pixels+y*VW+x;
 if((uintptr_t)p&2){*p++=color;--width;}
 uint32_t pattern=(uint32_t)color|((uint32_t)color<<16);
 uint32_t *words=(uint32_t *)p;
 while(width>=16) {
  words[0]=pattern;words[1]=pattern;words[2]=pattern;words[3]=pattern;
  words[4]=pattern;words[5]=pattern;words[6]=pattern;words[7]=pattern;
  words+=8;width-=16;
 }
 while(width>=2){*words++=pattern;width-=2;}
 if(width)*(uint16_t *)words=color;
}

/* Each scanline is a handful of halfword-safe spans, rather than a textured
 * software triangle.  The art around those spans supplies the height. */
static FAST __attribute__((noinline)) void draw_road(Surface *s,const Game *g,const View *v)
{
 RoadSample knots[25];
 RoadSample materials[25];
 int knot_x[25];
 uint16_t grounds[25],verges[25],seas[25],asphalts[25];
 int left_water[25],right_water[25],bridges[25],rails[25];
 for(int k=0;k<25;k++) {
  int y=k*8;if(y>=VH)y=VH-1;
  road_sample(v->seed,v->position+row_z[y],v->start,&knots[k]);
  road_sample(v->seed,physical_anchor(v->visual_position+row_z[y],v->visual_scale),v->start,&materials[k]);
  knot_x[k]=knots[k].center;
  const RoadSample *r=&materials[k];
  int transition=material_blend(r->blend);
  grounds[k]=blend_color(terrain_color(r->biome),terrain_color(r->next_biome),transition);
  verges[k]=blend_color(verge_color(r->biome),verge_color(r->next_biome),transition);
  seas[k]=blend_color(sea_color(r->biome),sea_color(r->next_biome),transition);
  left_water[k]=(biome_water(r->biome,-1)*(256-transition)+biome_water(r->next_biome,-1)*transition)/256;
  right_water[k]=(biome_water(r->biome,1)*(256-transition)+biome_water(r->next_biome,1)*transition)/256;
  bridges[k]=bridge_region(r->biome)*(256-transition)+bridge_region(r->next_biome)*transition;
  rails[k]=(biome_water(r->biome,-1)!=0)*(256-transition)+(biome_water(r->next_biome,-1)!=0)*transition;
  asphalts[k]=blend_color(r->biome==MARINA?C(49,61,78):C(64,77,85),
                        r->next_biome==MARINA?C(49,61,78):C(64,77,85),transition);
 }
 for(int y=HORIZON+1;y<VH;y++) {
  int k=y/8,t=y&7;
  int center=knot_x[k]+((knot_x[k+1]-knot_x[k])*t)/8;
  int scale=row_scale[y];
  int cx=128+((center-v->center-v->cam_x)*scale)/256;
  int half=(ROAD_HALF*scale)/256;
  road_x[y]=cx;road_half[y]=half;
  const RoadSample *r=&materials[k];
  int biome=r->blend>=128?r->next_biome:r->biome;
  row_biome[y]=(uint8_t)biome;row_feature[y]=r->feature;
  uint16_t ground=blend_color(grounds[k],grounds[k+1],t*32);
  uint16_t sea=blend_color(seas[k],seas[k+1],t*32);
  uint16_t verge=blend_color(verges[k],verges[k+1],t*32);
  uint16_t asphalt=blend_color(asphalts[k],asphalts[k+1],t*32);
  int haze=clampi(72-y,0,50);
  int transition=material_blend(r->blend);
  uint16_t haze_color=blend_color(C(185,218,219),C(41,80,110),
     (r->biome==MARINA)*(256-transition)+(r->next_biome==MARINA)*transition);
  ground=blend_color(ground,haze_color,haze);
  sea=blend_color(sea,haze_color,haze);
  verge=blend_color(verge,haze_color,haze);
  asphalt=blend_color(asphalt,haze_color,haze);
  int world=v->visual_position+row_z[y];
  int shoulder=clampi(scale*3/256,1,5),bank=clampi(scale*3/256,1,4);
  int outside_left=cx-half-shoulder-bank;
  int outside_right=cx+half+shoulder+bank;
  road_span(s,0,y,outside_left,ground);
  road_span(s,outside_right,y,VW-outside_right,ground);

  /* Water only occupies the appropriate side of a coastal or bridge road.
   * The shoreline itself approaches gradually during an environment blend. */
  for(int side=-1;side<=1;side+=2) {
   int water=side<0?left_water[k]+(left_water[k+1]-left_water[k])*t/8:
                     right_water[k]+(right_water[k+1]-right_water[k])*t/8;
   if(water) {
    int edge=cx+side*(half+7*scale/256);
    int span=side<0?edge:VW-edge;
    span=clampi((span*water)/256,0,VW);
    int x=side<0?0:VW-span;
    road_span(s,x,y,span,sea);
    if(span>0)road_span(s,side<0?span-1:x,y,1,blend_color(sea,C(180,223,197),100));
    if(span>12 && (world%43+43)%43<2) {
     unsigned wave=road_hash(v->seed,(unsigned)floor_div(world,43));
     int wx=x+3+(int)(wave%(unsigned)(span-9));
     road_span(s,wx,y,clampi(scale*7/256,2,10),blend_color(sea,C(161,220,222),90));
    }
   }
  }

  int bridge=bridges[k]+(bridges[k+1]-bridges[k])*t/8;
  if(bridge) {
   uint16_t cast=blend_color(sea,C(22,65,83),bridge*3/4);
   road_span(s,outside_left-bank,y,bank,cast);
   road_span(s,outside_right,y,bank,cast);
  }
  road_span(s,outside_left,y,bank,verge);
  road_span(s,cx+half+shoulder,y,bank,verge);
  road_span(s,cx-half-shoulder,y,shoulder,C(76,88,91));
  road_span(s,cx+half,y,shoulder,C(76,88,91));
  road_span(s,cx-half,y,half*2,asphalt);

  /* A little broken aggregate avoids a featureless slab, without turning the
   * road into television noise.  All positions are attached to the road. */
  if(half>8 && (world&15)==0) {
   unsigned h=road_hash(v->seed^0x40a1u,(unsigned)floor_div(world,8));
   int gx=cx-half+6+(int)(h%(unsigned)(half*2-12));
   road_span(s,gx,y,1,C(81,92,96));
  }
  int white=scale>246?2:1;
  road_span(s,cx-half+3,y,white,C(225,225,201));
  road_span(s,cx+half-4,y,white,C(225,225,201));
  int dash=(world%96+96)%96;
  int paint_length=34+clampi((v->speed-55)/6,0,v->motion==0?10:24);
  if(dash<paint_length)for(int lane=1;lane<4;lane++) {
   int x=cx-half+(half*lane)/2;
   road_span(s,x,y,white,dash<34?C(225,229,213):C(183,198,193));
  }
  if((world%66+66)%66<24) {
   road_span(s,cx-half-shoulder-2,y,2,C(237,208,148));
   road_span(s,cx+half+shoulder,y,2,C(237,208,148));
  }
  int rail=rails[k]+(rails[k+1]-rails[k])*t/8;
  if(rail) {
   for(int side=-1;side<=1;side+=2) {
    int rx=cx+side*(half+shoulder+bank);
    road_span(s,rx,y,clampi(scale*2/256,1,3),blend_color(verge,C(215,228,211),rail));
    road_span(s,rx+side*2,y,1,blend_color(verge,C(76,118,122),rail));
    if((world%84+84)%84<7)road_span(s,rx-1,y,4,C(244,235,202));
   }
  }
 }

 /* Old tyre marks are world-anchored, and disappear after a short while. */
 if(!v->attract)for(unsigned i=0;i<24;i++) {
  const TireMark *m=&tiremarks[i];
  if(!m->tick || g->run_ticks-m->tick>42)continue;
  int z=m->z-v->visual_position;
  if(z < -44 || z>ROAD_VIEW)continue;
  int y=project_y(z),x=item_x(v,m->x,z);
  int separation=clampi(project_scale(z)*8/256,4,11);
  gfx_rect(s,x-separation,y,2,4,C(43,55,62));
  gfx_rect(s,x+separation,y,2,4,C(43,55,62));
 }
}

static void add_item(DrawItem *items,int *count,int z,int x,int kind,int id,int variant)
{
 if(*count>=DRAW_MAX || z < -44 || z>ROAD_VIEW)return;
 int at=(*count)++;
 /* Far to near.  Props are queued before the vehicle on equal coordinates. */
 while(at>0 && items[at-1].z<z){items[at]=items[at-1];at--;}
 items[at]=(DrawItem){z,x,kind,id,variant};
}

static int prop_for_biome(int biome,uint32_t hash,int side)
{
 switch(biome) {
 case CITY:
  if((hash&15)==0)return SC_LAMP;
  if((hash&15)==1)return SC_BILLBOARD;
  return (int)((hash>>4)%3);
 case COAST:
  if(side<0)return SC_SAIL;
  return (hash&7)?SC_PALM:SC_VILLA;
 case CAUSEWAY:return (hash&1)?SC_LAMP:SC_BARRIER;
 case PINES:return (hash&3)?SC_PINE:SC_TREE;
 case CANYON:return (hash&3)?SC_ROCK:SC_BUSH;
 case MARINA:return side<0?((hash&3)?SC_PIER:SC_SAIL):((hash&7)?SC_HOTEL:SC_LAMP);
 case WETLANDS:return (hash&3)?SC_REEDS:SC_MANGROVE;
 default:return (hash&7)?SC_CITRUS:SC_WAREHOUSE;
 }
}

static void queue_scenery(DrawItem *items,int *count,const View *v)
{
 int first=floor_div(v->visual_position-45,88);
 int last=floor_div(v->visual_position+ROAD_VIEW,88);
 for(int n=first;n<=last;n++)for(int side=-1;side<=1;side+=2) {
  uint32_t h=road_landmark(v->seed,(uint32_t)n,side);
  int absolute=n*88+(int)((h>>10)%25);
  int z=absolute-v->visual_position;
  RoadSample sample,identity;
  road_sample(v->seed,v->position+z,v->start,&sample);
  road_sample(v->seed,physical_anchor(absolute,v->visual_scale),v->start,&identity);
  int biome=(int)((h>>16)&255)<material_blend(identity.blend)?identity.next_biome:identity.biome;
  int kind=prop_for_biome(biome,h,side);
  int mix=material_blend(identity.blend);
  int water=(biome_water(identity.biome,side)*(256-mix)+biome_water(identity.next_biome,side)*mix)/256;
  int bridge_prop=kind==SC_BARRIER || (kind==SC_LAMP && bridge_region(biome));
  int wet_plant=kind==SC_MANGROVE || kind==SC_REEDS;
  int floating=kind==SC_SAIL || kind==SC_PIER;
  /* Material belongs to this immutable anchor too.  Leave transition banks
   * open instead of putting a building in a growing sea or a boat on grass. */
  if(water && !bridge_prop && !wet_plant && !floating)continue;
  if(floating && water<256)continue;
  if(biome==COAST && side<0 && (h&3))continue;
  if(biome==CAUSEWAY && (n&1))continue;
  int x=sample.center+side*(125+(int)((h>>6)%26));
  if(biome==CAUSEWAY)x=sample.center+side*(ROAD_HALF+7);
  if(biome==WETLANDS)x=sample.center+side*(kind==SC_REEDS?108:140);
  if(biome==MARINA && kind==SC_PIER)x=sample.center+side*130;
  /* Boats sit offshore, beyond the shoreline rather than on the shoulder. */
  if(kind==SC_SAIL)x+=side*27;
  if(!(n&1) && (biome==CITY || biome==PINES || biome==GROVES || (biome==MARINA && side>0))) {
   int back_kind=biome==PINES?SC_PINE:biome==GROVES?SC_CITRUS:biome==MARINA?SC_HOTEL:SC_BUILDING;
   add_item(items,count,z,sample.center+side*(188+(int)((h>>18)%25)),ITEM_BACK_PROP,back_kind,(int)h);
  }
  add_item(items,count,z,x,ITEM_PROP,kind,(int)h);
 }
 /* A gantry is deliberately open over the lanes: posts live outside the
  * collision boundary and a shallow header does not hide approaching cars. */
 int segment=floor_div(v->visual_position,6400);
 for(int n=segment;n<=segment+1;n++) {
  int absolute=n*6400+4840;
  int z=absolute-v->visual_position;
  RoadSample r;road_sample(v->seed,v->position+z,v->start,&r);
  if(r.biome==CITY)add_item(items,count,z,r.center,ITEM_OVERPASS,0,0);
 }
}

static void draw_prop(Surface *s,const View *v,const DrawItem *item)
{
 int y=project_y(item->z),x=item_x(v,item->x,item->z);
 int scale=project_scale(item->z);
 int size=(scale*190)/256;
 if(item->kind==ITEM_BACK_PROP)size=size*218/256;
 int w=PROP_W*size/256,h=PROP_H*size/256;
 if(w<2 || h<3)return;
 if(item->id==SC_BARRIER) {
  int span=clampi(scale*20/256,11,25),height=clampi(scale*13/256,7,16);
  shadow(s,x,y,span,3);
  gfx_rect(s,x-span/2,y-height,3,height,C(203,217,195));
  gfx_rect(s,x+span/2-3,y-height,3,height,C(170,193,183));
  gfx_rect(s,x-span/2,y-height,span,3,C(229,231,200));
  gfx_rect(s,x-span/2,y-height+3,span,2,C(94,139,145));
  return;
 }
 if(item->id==SC_LAMP){w=w*3/4;h=h*3/4;}
 shadow(s,x,y-1,w*2/3,clampi(w/5,2,8));
 if(item->id==SC_REEDS || item->id==SC_BUSH)x+=(v->frame/14+(unsigned)item->variant)&1;
 gfx_scaled_sprite(s,x-w/2,y-h,prop_art+item->id*PROP_W*PROP_H,PROP_W,PROP_H,w,h);
}

static void draw_coin(Surface *s,const View *v,const DrawItem *item)
{
 int x=item_x(v,item->x,item->z),y=project_y(item->z);
 int scale=project_scale(item->z);
 int h=clampi(scale*10/256,2,14);
 int spin=(v->frame/4+(unsigned)item->id)&7;
 int width=spin<4?spin+2:9-spin;
 width=clampi(width*h/7,1,h-1);
 shadow(s,x,y+2,h,3);
 ellipse(s,x,y-h/2,width+2,h+2,C(163,98,36));
 ellipse(s,x,y-h/2,width,h,C(251,207,75));
 gfx_rect(s,x-width/2+1,y-h+2,1,clampi(h-4,1,6),C(255,245,175));
}

static void draw_traffic(Surface *s,const View *v,const DrawItem *item)
{
 int x=item_x(v,item->x,item->z),y=project_y(item->z);
 int size=(project_scale(item->z)*180)/256;
 int w=TRAFFIC_W*size/256,h=TRAFFIC_H*size/256;
 if(w<2 || h<3)return;
 contact_shadow(s,x-w/2,y-h/2,w,h,traffic_bounds+item->id*4,TRAFFIC_W,TRAFFIC_H,0);
 gfx_vehicle(s,x-w/2,y-h/2,traffic_art+item->id*TRAFFIC_W*TRAFFIC_H,
             TRAFFIC_W,TRAFFIC_H,w,h,item->variant%6);
}

static int car_pose(int yaw)
{return yaw<-28?0:yaw<-8?1:yaw>28?4:yaw>8?3:2;}

static void draw_player(Surface *s,const Game *g,const View *v,const DrawItem *item)
{
 int x=item_x(v,item->x,0),y=PLAYER_Y;
 int vehicle=v->attract?g->save.selected_vehicle:g->vehicle;
 int yaw=v->attract?((int)(v->frame/32)%3-1)*12:g->yaw;
 int pose=car_pose(yaw);
 int w=29,h=42;
 if(vehicles[vehicle].bike){w=24;h=41;}
 int lift=!v->attract && g->wheelie?3+(g->frame/6&1):0;
 contact_shadow(s,x-w/2,y-h/2,w,h,vehicle_bounds+(vehicle*5+pose)*4,CAR_W,CAR_H,yaw);
 if(!v->attract && g->turbo && g->speed_q8>40*Q) {
  int flick=(g->frame&1)*3;
  gfx_rect(s,x-6,y+17,3,9+flick,C(36,182,204));
  gfx_rect(s,x+4,y+17,3,9+flick,C(36,182,204));
  gfx_rect(s,x-5,y+17,1,6+flick,C(232,252,210));
  gfx_rect(s,x+5,y+17,1,6+flick,C(232,252,210));
 }
 if(!v->attract && g->drifting && g->speed_q8>30*Q) {
  for(int i=0;i<3;i++) {
   int age=(g->frame+i*5)%15;
   int sx=x+(yaw>0?-8:8)+(yaw>0?-age:age)/2;
   int sy=y+14+age/2;
   gfx_circle(s,sx,sy,2+age/6,age>9?C(114,131,132):C(179,190,183));
  }
 }
 const uint16_t *art=vehicle_art+(vehicle*5+pose)*CAR_W*CAR_H;
 int paint=g->save.paint[vehicle];
 if(!v->attract && g->damage_cooldown && (g->frame&2))paint=5;
 gfx_vehicle(s,x-w/2,y-h/2-lift,art,CAR_W,CAR_H,w,h,paint);
 if(!v->attract && (g->slow || g->drifting)) {
  gfx_rect(s,x-w/2+5,y+h/2-5-lift,3,2,C(255,83,64));
  gfx_rect(s,x+w/2-7,y+h/2-5-lift,3,2,C(255,83,64));
 }
 if(!v->attract && g->horn_timer) {
  int reach=20+(g->horn_timer%8);
  gfx_line(s,x-reach,y-8,x-reach-3,y-14,C(248,216,129));
  gfx_line(s,x+reach,y-8,x+reach+3,y-14,C(248,216,129));
 }
}

static void draw_gantry(Surface *s,const View *v,const DrawItem *item)
{
 int y=project_y(item->z),x=item_x(v,item->x,item->z),scale=project_scale(item->z);
 int half=(ROAD_HALF+12)*scale/256;
 int height=20*scale/256;
 gfx_rect(s,x-half,y-height,3,height,C(111,134,136));
 gfx_rect(s,x+half-2,y-height,3,height,C(111,134,136));
 gfx_rect(s,x-half,y-height,half*2,3,C(210,216,191));
 gfx_rect(s,x-half,y-height+3,half*2,2,C(79,111,119));
 if(scale>185) {
  int width=clampi(80*scale/256,52,90);
  gfx_card(s,x-width/2,y-height-12,width,14,C(29,108,101),C(178,213,187));
  gfx_text_small(s,x-width/2+5,y-height-11,"NORTH  /  COAST",C(240,239,202));
 }
}

static void draw_ambient(Surface *s,const View *v)
{
 /* Distant seabirds stay beside the coast; they are decorative, never traffic. */
 int biome=row_biome[30];
 if(biome!=COAST && biome!=CAUSEWAY)return;
 for(int i=0;i<3;i++) {
  unsigned t=v->frame+i*61;
  int x=(int)((t/2+i*23)%310)-25;
  int y=20+i*15+(int)((t/22)%3);
  if(x>road_x[y]-road_half[y]-9 && x<road_x[y]+road_half[y]+9)continue;
  int wing=(t/9)&1?2:-1;
  gfx_line(s,x-3,y+wing,x,y,C(240,241,215));
  gfx_line(s,x,y,x+3,y+wing,C(240,241,215));
 }
}

static FAST __attribute__((noinline)) void draw_world(Surface *s,const Game *g,int attract)
{
 View v;
 v.attract=attract;v.frame=g->frame;
 v.seed=attract?g->save.seed:g->run_seed;
 v.position=attract?(int)((g->frame*4)%(ROAD_BIOME_LENGTH*LOCATION_COUNT)):g->position_q8/Q;
 v.motion=clampi(g->save.motion,0,2);
 v.visual_scale=v.motion==0?205:v.motion==2?384:307;
 v.visual_position=visual_travel(v.position,v.motion);
 v.speed=attract?145:(g->speed_q8<0?-g->speed_q8:g->speed_q8)/Q;
 v.start=attract?(g->mode==M_DEPARTURES?g->cursor:COAST):g->start_biome;
 RoadSample player;road_sample(v.seed,v.position,v.start,&player);
 v.center=player.center;
 v.cam_x=attract?0:g->x_q8/(Q*8);
 if(!attract && g->save.shake && g->shake)v.cam_x+=(g->frame&1?1:-1)*clampi(g->shake/4,0,2);
 draw_sky(s,&v,&player);

 if(!attract) {
  if(trail_seed!=g->run_seed || g->run_ticks<last_trail_tick) {
   for(int i=0;i<24;i++)tiremarks[i].tick=0;
   trail_seed=g->run_seed;tire_cursor=0;last_trail_tick=0;
  }
  if(g->drifting && g->speed_q8>30*Q && g->run_ticks-last_trail_tick>=2) {
   tiremarks[tire_cursor]=(TireMark){v.center+g->x_q8/Q,v.visual_position,g->run_ticks};
   tire_cursor=(tire_cursor+1)%24;last_trail_tick=g->run_ticks;
  }
 }
 unsigned road_start=profile_clock();
 draw_road(s,g,&v);
 render_profile[3]=(profile_clock()-road_start)&65535u;
 unsigned object_start=profile_clock();
 DrawItem items[DRAW_MAX];int count=0;
 queue_scenery(items,&count,&v);
 if(attract) {
  for(int i=0;i<6;i++) {
   int z=70+(int)((g->frame*(2+i%2)+i*113)%550);
   RoadSample r;road_sample(v.seed,v.position+z,v.start,&r);
   int lane=(i*3+1)%4;
   add_item(items,&count,z,r.center-72+lane*48,ITEM_CAR,i%6,i%6);
  }
 } else {
  for(int i=0;i<TRAFFIC_COUNT;i++)if(g->traffic[i].active) {
   int z=(g->traffic[i].z_q8-g->position_q8)/Q;
   RoadSample r;road_sample(v.seed,g->traffic[i].z_q8/Q,v.start,&r);
   add_item(items,&count,z,r.center+g->traffic[i].x_q8/Q,ITEM_CAR,
            g->traffic[i].kind%6,g->traffic[i].paint);
  }
  for(int i=0;i<PICKUP_COUNT;i++)if(g->pickups[i].active) {
   int z=(g->pickups[i].z_q8-g->position_q8)/Q;
   RoadSample r;road_sample(v.seed,g->pickups[i].z_q8/Q,v.start,&r);
   add_item(items,&count,z,r.center+g->pickups[i].x_q8/Q,ITEM_COIN,
            g->pickups[i].serial,0);
  }
 }
 add_item(items,&count,0,v.center+(attract?0:g->x_q8/Q),ITEM_PLAYER,0,0);
 for(int i=0;i<count;i++) {
  switch(items[i].kind) {
  case ITEM_PROP:case ITEM_BACK_PROP:draw_prop(s,&v,&items[i]);break;
  case ITEM_CAR:draw_traffic(s,&v,&items[i]);break;
  case ITEM_COIN:draw_coin(s,&v,&items[i]);break;
  case ITEM_PLAYER:draw_player(s,g,&v,&items[i]);break;
  default:draw_gantry(s,&v,&items[i]);break;
  }
 }
 draw_ambient(s,&v);
 if(!attract && v.motion && v.speed>145)for(int i=0;i<(g->turbo?3:2);i++) {
  int y=HORIZON+10+(int)(((unsigned)v.visual_position*2+(unsigned)i*67)%(VH-HORIZON-20));
  int side=(i&1)?1:-1;
  int x=road_x[y]+side*(road_half[y]-6);
  gfx_line(s,x,y,x+side*2,y+clampi(v.speed/35,4,10),C(137,176,180));
 }
 render_profile[4]=(profile_clock()-object_start)&65535u;
}

static void copy_cached_card(Surface *s,int x,int y,const uint16_t *pixels,int stride,int w,int h)
{
 /* These HUD cards are wholly on screen.  Rounded corners preserve the
  * changing road underneath; each remaining row is one opaque copy. */
 for(int j=0;j<h;j++) {
  int edge=j<h-1-j?j:h-1-j;
  int inset=edge==0?4:edge==1?2:edge<4?1:0;
  gfx_copy(s->pixels+(y+j)*VW+x+inset,pixels+j*stride+inset,w-inset*2);
 }
}

static void draw_run_overlay(Surface *s,const Game *g)
{
 static uint16_t combo_art[39*18],location_art[LOCATION_COUNT][157*19],notice_art[240*17];
 static uint8_t location_ready[LOCATION_COUNT];
 static char old_notice[88];
 static int old_combo=-1,notice_width;
 static int old_skin=-1;
 const StudioTheme *theme=theme_for(g);
 if(old_skin!=g->save.skin){memset(location_ready,0,sizeof(location_ready));old_combo=-1;notice_width=0;old_skin=g->save.skin;}
 if(g->combo_q8>Q+Q/8) {
  int value=g->combo_q8*10/Q;
  if(value!=old_combo) {
   Surface cache={combo_art,39,18};
   gfx_rect(&cache,0,0,39,18,0);
   char combo[16];snprintf(combo,sizeof(combo),"%u.%ux",g->combo_q8/Q,(g->combo_q8%Q)*10/Q);
   gfx_card(&cache,0,0,39,18,theme->pane,theme->rim);
   gfx_text(&cache,6,2,combo,theme->accent);
   old_combo=value;
  }
  copy_cached_card(s,210,1,combo_art,39,39,18);
 }
 if(g->run_ticks<150) {
  int id=g->start_biome;
  if(!location_ready[id]) {
   Surface cache={location_art[id],157,19};
   gfx_rect(&cache,0,0,157,19,0);
   gfx_card(&cache,0,0,157,19,theme->pane,theme->rim);
   gfx_text_small(&cache,7,4,locations[id].name,theme->ink);
   location_ready[id]=1;
  }
  copy_cached_card(s,8,0,location_art[id],157,157,19);
 }
 if(g->notice_timer && g->notice[0]) {
  if(!notice_width || strcmp(old_notice,g->notice)!=0) {
   notice_width=clampi(gfx_text_width(g->notice)+14,100,240);
   Surface cache={notice_art,240,17};
   gfx_rect(&cache,0,0,240,17,0);
   gfx_card(&cache,0,0,notice_width,17,theme->pane,theme->rim);
   gfx_text_small(&cache,7,3,g->notice,theme->ink);
   memcpy(old_notice,g->notice,sizeof(old_notice));
  }
  copy_cached_card(s,(VW-notice_width)/2,27,notice_art,240,notice_width,17);
 }
 if(g->crash_timer) {
  gfx_rect(s,0,0,VW,3,C(216,99,78));gfx_rect(s,0,VH-3,VW,3,C(216,99,78));
 }
}

static void draw_showroom(Surface *s,const Game *g,int receipt)
{
 static uint16_t studio_art[VW*VH] __attribute__((aligned(4)));
 static Game old_key;
 static int ready;
 Game key=*g;key.frame=0;key.notice_timer=0;memset(key.notice,0,sizeof(key.notice));
 if(ready && memcmp(&old_key,&key,sizeof(key))==0){gfx_copy(s->pixels,studio_art,VW*VH);return;}
 const StudioTheme *theme=theme_for(g);
 Surface cache={studio_art,VW,VH};
 Surface *scene=&cache;
 for(int y=0;y<58;y++)gfx_rect(scene,0,y,VW,1,blend_color(theme->sky,theme->sky_low,y*256/58));
 gfx_rect(scene,0,40,VW,16,blend_color(theme->sky,C(75,166,180),100));
 for(int i=0;i<5;i++)gfx_rect(scene,15+i*53,45+i%3,20,1,blend_color(theme->sky,C(239,253,239),130));
 ellipse(scene,196,43,46,4,blend_color(theme->sky,C(71,137,134),130));
 for(int x=23;x<256;x+=103)gfx_rect(scene,x,0,2,57,blend_color(theme->pane,theme->ink,35));
 gfx_rect(scene,0,55,VW,3,theme->rim);
 for(int y=58;y<VH;y++)gfx_rect(scene,0,y,VW,1,blend_color(theme->floor,theme->floor_low,(y-58)*256/(VH-58)));
 uint16_t grout=blend_color(theme->floor_low,theme->rim,65);
 for(int i=0;i<6;i++)gfx_line(scene,119,58,i*61-24,VH-1,grout);
 for(int y=85;y<VH;y+=33)gfx_rect(scene,0,y,VW,1,grout);
 gfx_card(scene,60,59,91,125,theme->pane,theme->rim);
 gfx_rect(scene,64,65,2,17,theme->accent);gfx_rect(scene,64,65,14,2,theme->accent);
 gfx_rect(scene,143,160,2,17,theme->accent);gfx_rect(scene,131,175,14,2,theme->accent);
 int id=receipt?g->vehicle:g->selection;
 if(id<0 || id>=VEHICLE_COUNT)id=g->save.selected_vehicle;
 int pose=2,w=vehicles[id].bike?48:78,h=116,x=104-w/2,y=63;
 const uint16_t *art=vehicle_art+(id*5+pose)*CAR_W*CAR_H;
 int paint=!receipt && g->garage_tab==0 && g->cursor==2?g->paint_choice:g->save.paint[id];
 contact_shadow(scene,x,y,w,h,vehicle_bounds+(id*5+pose)*4,CAR_W,CAR_H,0);
 gfx_vehicle(scene,x,y,art,CAR_W,CAR_H,w,h,paint);
 gfx_card(scene,10,7,236,27,theme->pane,theme->rim);
 gfx_text(scene,18,11,receipt?"BACK AT THE STUDIO":"COASTLINE  /  MOTOR STUDIO",theme->ink);
 gfx_text_small(scene,18,24,receipt?"ANOTHER ROAD. ANOTHER STORY.":g->garage_tab==1?"PARTS  /  BUILT FOR THE LONG RUN":
      g->garage_tab==2?"SETUP  /  FIND YOUR OWN BALANCE":"SHOWROOM  /  YOUR NEXT SET OF KEYS",theme->muted);
 gfx_text(scene,13,40,vehicles[id].name,theme->ink);
 char mastery[24];snprintf(mastery,sizeof(mastery),"MASTERY %d",game_mastery_level(g,id));
 gfx_text_small(scene,174,42,mastery,theme->ink);
 gfx_card(scene,165,61,80,116,theme->pane,theme->rim);
 gfx_rect(scene,172,67,65,2,theme->accent);
 if(receipt) {
  gfx_text_small(scene,173,74,"RUN BANKED",theme->accent);
  gfx_text_small(scene,173,94,"POINTS",theme->muted);gfx_number(scene,173,106,(int)g->run.total_points,theme->ink);
  gfx_text_small(scene,173,128,"CASH EARNED",theme->muted);
  char cash[24];snprintf(cash,sizeof(cash),"$%lu",(unsigned long)g->run.total_cash);gfx_text(scene,173,141,cash,theme->ink);
 } else {
  static const char *const labels[4]={"TOP SPEED","0-100 km/h","ROAD GRIP","ARMOR"};
  for(int stat=0;stat<4;stat++) {
   int value=game_preview_stat(g,id,stat),row=74+stat*25;
   gfx_text_small(scene,173,row,stat==1 && g->save.units?"0-60 mph":labels[stat],theme->muted);
   char text[24];
   if(stat==0){if(g->save.units)value=(value*621+500)/1000;snprintf(text,sizeof(text),"%d %s",value,g->save.units?"mph":"km/h");}
   else if(stat==1){int time=((g->save.units?97:100)*10+value/2)/(value?value:1);snprintf(text,sizeof(text),"%d.%d s",time/10,time%10);}
   else snprintf(text,sizeof(text),"%d",value);
   gfx_text(scene,173,row+11,text,theme->ink);
  }
 }
 char progress[48];uint32_t next=game_mastery_next(g,id);
 if(next)snprintf(progress,sizeof(progress),"%lu / %lu CAR XP",(unsigned long)g->save.mastery_xp[id],(unsigned long)next);
 else snprintf(progress,sizeof(progress),"VEHICLE MASTERY COMPLETE");
 gfx_text_small(scene,13,183,progress,theme->muted);
 old_key=key;ready=1;
 gfx_copy(s->pixels,studio_art,VW*VH);
}

static uint16_t menu_header_art[VW*33],menu_footer_art[6][VW*17];
static const char *const menu_captions[6]={
 "CITY LIGHTS TO OPEN WATER","A NEW HORIZON STARTS HERE",
 "MAKE EVERY MILE COUNT","BUILT FOR BUTTONS",
 "MOMENTUM BUILDS YOUR STORY","MAKE THIS DRIVE YOURS"};

static void init_menu_chrome(const Game *g)
{
 static int skin=-1;
 if(skin==g->save.skin)return;
 const StudioTheme *theme=theme_for(g);
 Surface header={menu_header_art,VW,33};
 for(int y=0;y<33;y++)gfx_rect(&header,0,y,VW,1,blend_color(theme->floor,theme->pane,y*5));
 gfx_rect(&header,0,31,VW,2,theme->accent);
 gfx_text(&header,14,5,"C O A S T L I N E",theme->ink);
 gfx_text_small(&header,15,21,"ENDLESS ROADS.  YOUR OWN LINE.",theme->muted);
 for(int i=0;i<6;i++) {
  Surface footer={menu_footer_art[i],VW,17};
  gfx_rect(&footer,0,0,VW,17,theme->pane);
  gfx_text_small(&footer,14,5,menu_captions[i],theme->muted);
 }
 skin=g->save.skin;
}

static void draw_menu_decoration(Surface *s,const Game *g)
{
 if(g->mode==M_GARAGE || g->mode==M_RECEIPT) {
  draw_showroom(s,g,g->mode==M_RECEIPT);return;
 }
 draw_world(s,g,1);
 /* Letterboxing gives the menu's animated drive a calmer, cinematic frame. */
 init_menu_chrome(g);
 int caption=g->mode==M_DEPARTURES?1:g->mode==M_CHALLENGES?2:g->mode==M_CONTROLS?3:g->mode==M_CAREER?4:g->mode==M_SETTINGS?5:0;
 gfx_copy(s->pixels,menu_header_art,VW*33);
 gfx_copy(s->pixels+VW*175,menu_footer_art[caption],VW*17);
}

int render_game(const Game *g,uint16_t *top,uint16_t *bottom)
{
 unsigned total_start=profile_clock();
 init_projection();
 Surface t={top,VW,VH},b={bottom,VW,VH};
 if(g->mode==M_RUN || g->mode==M_PAUSE) {
  draw_world(&t,g,0);draw_run_overlay(&t,g);
 } else draw_menu_decoration(&t,g);
 render_profile[0]=(profile_clock()-total_start)&65535u;

 static Game bottom_key;
 static int bottom_valid;
 Game key=*g;
 key.frame=0;
 key.notice_timer=(uint16_t)(g->notice_timer!=0);
 int draw_bottom;
 if(g->mode==M_RUN) {
  draw_bottom=!(g->frame&1) || !bottom_valid || bottom_key.mode!=g->mode ||
   bottom_key.health!=g->health || bottom_key.notice_timer!=key.notice_timer;
 } else {
  /* Menus contain no animated widgets.  Ignore the attract movie's frame and
   * notice countdown, but retain every menu value and notice visibility. */
  draw_bottom=!bottom_valid || memcmp(&bottom_key,&key,sizeof(key))!=0;
 }
 unsigned bottom_start=profile_clock();
 if(draw_bottom)ui_draw_bottom(&b,g);
 render_profile[1]=(profile_clock()-bottom_start)&65535u;
 if(draw_bottom){bottom_key=key;bottom_valid=1;}
 render_profile[2]=(profile_clock()-total_start)&65535u;
 return 1|(draw_bottom?2:0);
}
