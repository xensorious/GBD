#include "render.h"
#include "gfx.h"
#include "assets.h"
#include "road.h"
#include "vehicle_projection.h"
#include "vehicle_geometry.h"
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
volatile uint32_t scenery_profile[8];
static uint32_t scenery_work[8];

#ifdef __arm__
#define FAST __attribute__((section(".itcm"),long_call))
#else
#define FAST
#endif
#define VW 256
#define VH 192
#define HORIZON DRIVE_HORIZON
#define CAMERA DRIVE_CAMERA
#define PLAYER_Y DRIVE_PLAYER_Y
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
enum { ITEM_PROP,ITEM_CAR,ITEM_COIN,ITEM_PLAYER,ITEM_BACK_PROP };
typedef struct { int z,x,kind,id,variant,side; } DrawItem;
typedef struct { int x,z;unsigned tick; } TireMark;
typedef struct {
 unsigned seed,frame;
 int position,visual_position,visual_scale,center,cam_x,start,attract,speed,motion;
} View;

#define DEPTH_CACHE_MIN (-44)
#define DEPTH_CACHE_COUNT (ROAD_VIEW+128-DEPTH_CACHE_MIN+1)
static int16_t depth_y[DEPTH_CACHE_COUNT],depth_scale[DEPTH_CACHE_COUNT];
static uint32_t vertical_step[256];
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

/* The established low chase projection is shared by the road, vehicles and
 * scenery ground contacts.  Upright scenery keeps its vertical height. */
static int project_y(int z)
{
 unsigned index=(unsigned)(z-DEPTH_CACHE_MIN);
 return index<DEPTH_CACHE_COUNT?depth_y[index]:HORIZON+((PLAYER_Y-HORIZON)*CAMERA)/(z+CAMERA);
}

static int project_scale(int z)
{
 unsigned index=(unsigned)(z-DEPTH_CACHE_MIN);
 return index<DEPTH_CACHE_COUNT?depth_scale[index]:(276*CAMERA)/(z+CAMERA);
}

static int item_x(const View *v,int world_x,int z)
{return 128+((world_x-v->center-v->cam_x)*project_scale(z))/256;}

static void init_projection(void)
{
 static int initialized;
 if(initialized)return;
 initialized=1;
 for(int i=0;i<DEPTH_CACHE_COUNT;i++) {
  int z=i+DEPTH_CACHE_MIN;
  depth_y[i]=(int16_t)(HORIZON+((PLAYER_Y-HORIZON)*CAMERA)/(z+CAMERA));
  depth_scale[i]=(int16_t)((276*CAMERA)/(z+CAMERA));
 }
 for(int i=1;i<256;i++)vertical_step[i]=65536u/(unsigned)i;
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
 const uint8_t (*spans)[2],int source_w,int source_h)
{
 if(w<=0 || h<=0)return;
 /* A one-pixel contact rim follows the actual opaque chassis row by row.
  * A filled bounding rectangle leaves black blocks around tapered bonnets,
  * especially in the larger garage view. Source art supplies the steering
  * silhouette, so the ground contact needs no additional shearing. */
 unsigned y_step=((unsigned)source_h*65536u+(unsigned)h-1)/(unsigned)h;
 unsigned x_step=((unsigned)w*65536u)/(unsigned)source_w;
 for(int row=0;row<h;row++) {
  unsigned sy=((unsigned)row*y_step)>>16;
  int left=spans[sy][0],right=spans[sy][1];
  if(right<=left)continue;
  left=(int)(((unsigned)left*x_step)>>16);
  right=(int)(((unsigned)right*x_step+65535u)>>16);
  gfx_rect(s,x+left-1,y+row+1,right-left+2,1,C(36,51,56));
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
static FAST __attribute__((noinline,noclone)) void road_span(Surface *s,int x,int y,int width,uint16_t color)
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

static void add_item(DrawItem *items,int *count,int z,int x,int kind,int id,int variant,int side)
{
 if(*count>=DRAW_MAX || z < ((kind==ITEM_PROP || kind==ITEM_BACK_PROP)?-140:-44) || z>ROAD_VIEW)return;
 int at=(*count)++;
 /* Far to near.  Props are queued before the vehicle on equal coordinates. */
 while(at>0 && items[at-1].z<z){items[at]=items[at-1];at--;}
 items[at]=(DrawItem){z,x,kind,id,variant,side};
}

/* Each category has its own immutable frontage rhythm.  Buildings occupy
 * lots, lamps follow the shoulder, plants form irregular clusters, and large
 * waterside structures have a much wider exclusion interval. */
static int building_id(int id)
{return id==SC_BUILDING || id==SC_SHOP || id==SC_WAREHOUSE || id==SC_HOTEL || id==SC_VILLA;}

static int scenery_material(const View *v,int absolute,int side,uint32_t hash,int *water)
{
 RoadSample identity;
 road_sample(v->seed,physical_anchor(absolute,v->visual_scale),v->start,&identity);
 int mix=material_blend(identity.blend);
 *water=(biome_water(identity.biome,side)*(256-mix)+biome_water(identity.next_biome,side)*mix)/256;
 return (int)((hash>>16)&255)<mix?identity.next_biome:identity.biome;
}

static void place_scenery(DrawItem *items,int *count,const View *v,int absolute,
 int side,int offset,int kind,uint32_t hash,int back)
{
 int z=absolute-v->visual_position;
 if(z < (building_id(kind)?-140:-44) || z>ROAD_VIEW)return;
 int water;int biome=scenery_material(v,absolute,side,hash,&water);
 int floating=kind==SC_SAIL || kind==SC_PIER;
 int wet=kind==SC_MANGROVE || kind==SC_REEDS;
 int bridge=kind==SC_LAMP && bridge_region(biome);
 if((floating && water!=256) || (!floating && !wet && !bridge && water))return;
 RoadSample road;road_sample(v->seed,v->position+z,v->start,&road);
 add_item(items,count,z,road.center+side*offset,back?ITEM_BACK_PROP:ITEM_PROP,
          kind,(int)hash,side);
}

static int dock_allowed(int biome,uint32_t hash,int side)
{return side<0 && (biome==MARINA?(hash&3)!=0:biome==COAST && (hash&3)==0);}

static __attribute__((noinline)) void queue_scenery(DrawItem *items,int *count,const View *v)
{
 /* A full lot remains stable while approaching it or reversing past it.
  * Adjacent homes share a block setback; frontage spacing exceeds depth. */
 int first=floor_div(v->visual_position-160,160);
 int last=floor_div(v->visual_position+ROAD_VIEW,160);
 for(int n=first;n<=last;n++)for(int side=-1;side<=1;side+=2) {
  uint32_t h=road_landmark(v->seed^0x4181u,(uint32_t)n,side);
  uint32_t block=road_landmark(v->seed^0x20b1u,(uint32_t)floor_div(n,4),side);
  int absolute=n*160+12+(int)((block>>9)%13);
  int water,biome=scenery_material(v,absolute,side,h,&water),kind=-1;
  if(biome==CITY)kind=(int)((block>>4)%3);
  if(biome==MARINA && side>0 && (h&3)!=0)kind=SC_HOTEL;
  if(biome==COAST && side>0 && (h&7)==0)kind=SC_VILLA;
  if(biome==GROVES && (h&15)==0)kind=SC_WAREHOUSE;
  if(kind>=0)place_scenery(items,count,v,absolute,side,ROAD_HALF+33+(int)((block>>12)&7),kind,h,0);
 }
 /* Street lamps are deliberately regular, with a small safe shoulder
  * offset, rather than competing with houses for the same random slot. */
 first=floor_div(v->visual_position-44,224);last=floor_div(v->visual_position+ROAD_VIEW,224);
 for(int n=first;n<=last;n++)for(int side=-1;side<=1;side+=2) {
  uint32_t h=road_landmark(v->seed^0x1a4bu,(uint32_t)n,side);
  int absolute=n*224,water,biome=scenery_material(v,absolute,side,h,&water);
  if(biome==CITY || biome==CAUSEWAY || (biome==MARINA && side>0))
   place_scenery(items,count,v,absolute,side,ROAD_HALF+11,SC_LAMP,h,0);
 }
 /* Plant clusters use unequal spacing and lateral depths.  They are never
  * laid out like a row of houses, and dense forest remains intentional. */
 first=floor_div(v->visual_position-180,240);last=floor_div(v->visual_position+ROAD_VIEW,240);
 for(int n=first;n<=last;n++)for(int side=-1;side<=1;side+=2) {
  uint32_t h=road_landmark(v->seed^0x7ee5u,(uint32_t)n,side);
  int base=n*240+(int)((h>>8)%43),water,biome=scenery_material(v,base,side,h,&water);
  int kind=-1,number=1;
  if(biome==PINES){kind=(h&3)?SC_PINE:SC_TREE;number=3+(int)(h&1);}
  if(biome==GROVES){kind=SC_CITRUS;number=2+(int)(h&1);}
  if(biome==COAST && side>0){kind=SC_PALM;number=2+(int)(h&1);}
  if(biome==WETLANDS){kind=(h&3)?SC_REEDS:SC_MANGROVE;number=2+(int)(h&1);}
  if(biome==CANYON){kind=(h&3)?SC_ROCK:SC_BUSH;number=1+(int)(h%3);}
  if(biome==CITY && (h&3)==0){kind=SC_TREE;number=1;}
  if(kind<0)continue;
  for(int j=0;j<number;j++) {
   uint32_t plant=road_hash(h,(uint32_t)j);
   int absolute=base+j*34+(int)((plant>>10)%17);
   int offset=118+(int)((plant>>3)%47)+(j&1)*17;
   if(biome==CITY)offset=184+(int)(plant%21);
   if(biome==WETLANDS)offset=kind==SC_REEDS?110+(int)(plant%16):137+(int)(plant%24);
   if(kind==SC_ROCK)offset=122+(int)(plant%61);
   place_scenery(items,count,v,absolute,side,offset,kind,plant,j&1);
  }
 }
 /* A dock is one whole shore-connected structure, never a run of unrelated
  * cards.  At least 520 world units separate dock anchors; a structure has
  * exactly one or two 60-unit sections extending across the water. */
 first=floor_div(v->visual_position-44,520);last=floor_div(v->visual_position+ROAD_VIEW,520);
 for(int n=first;n<=last;n++)for(int side=-1;side<=1;side+=2) {
  uint32_t h=road_landmark(v->seed^0xd0c4u,(uint32_t)n,side);
  int absolute=n*520+52,water,biome=scenery_material(v,absolute,side,h,&water);
  if(dock_allowed(biome,h,side))
   place_scenery(items,count,v,absolute,side,ROAD_HALF+7,SC_PIER,h,0);
 }
 /* Boats keep their own wide mooring intervals offshore, so a marina reads
  * as water with occasional vessels instead of a tightly packed wall. */
 first=floor_div(v->visual_position-44,408);last=floor_div(v->visual_position+ROAD_VIEW,408);
 for(int n=first;n<=last;n++)for(int side=-1;side<=1;side+=2) {
  uint32_t h=road_landmark(v->seed^0xb0a7u,(uint32_t)n,side);
  int absolute=n*408+184+(int)((h>>10)%53),water,biome=scenery_material(v,absolute,side,h,&water);
  if((biome==MARINA || biome==COAST) && side<0 && (h&3)!=0) {
   int dock_n=floor_div(absolute-52+260,520),dock_absolute=dock_n*520+52;
   uint32_t dock_hash=road_landmark(v->seed^0xd0c4u,(uint32_t)dock_n,side);
   int dock_water,dock_biome=scenery_material(v,dock_absolute,side,dock_hash,&dock_water);
   int separation=absolute-dock_absolute;if(separation<0)separation=-separation;
   if(separation<58 && dock_water==256 && dock_allowed(dock_biome,dock_hash,side))continue;
   place_scenery(items,count,v,absolute,side,190+(int)((h>>5)%42),SC_SAIL,h,0);
  }
 }
}

/* Organic scenery uses upright, root-anchored art. Buildings are projected
 * solids; broadside boats rise above water and docks follow its plane. */
static int side_variant(int id)
{
 switch(id) {
 case SC_BUILDING:return 0;case SC_SHOP:return 1;case SC_WAREHOUSE:return 2;
 case SC_ROCK:return 3;case SC_LAMP:return 4;case SC_HOTEL:return 5;
 case SC_VILLA:return 6;default:return -1;
 }
}

static const uint16_t *prop_source(int id,int side,int projected_width,
 int *source_width,int *source_height,const uint8_t **anchor,const uint8_t **bounds)
{
 int variant=side>0?side_variant(id):-1;
 const uint16_t *pixels;
 int width=projected_width>=40?64:projected_width>=27?48:projected_width>=13?32:16;
 int height=width*PROP_H/PROP_W;
 if(variant>=0) {
  pixels=width==64?prop_side_art:width==48?prop_side_lod48:
         width==32?prop_side_lod32:prop_side_lod16;
  pixels+=variant*width*height;
  *anchor=prop_side_anchor+variant*2;
  *bounds=(width==64?prop_side_bounds:width==48?prop_side_lod48_bounds:
           width==32?prop_side_lod32_bounds:prop_side_lod16_bounds)+variant*4;
 } else {
  pixels=width==64?prop_art:width==48?prop_lod48:width==32?prop_lod32:prop_lod16;
  pixels+=id*width*height;
  *anchor=prop_anchor+id*2;
  *bounds=(width==64?prop_bounds:width==48?prop_lod48_bounds:
           width==32?prop_lod32_bounds:prop_lod16_bounds)+id*4;
 }
 *source_width=width;*source_height=height;
 return pixels;
}

/* Inverse projected rows need one division per scanline, never per texel.
 * Transparent top-view wood/hull texels preserve the actual water beneath. */
static FAST __attribute__((noinline)) void ground_plane(Surface *s,const View *v,
 int world_x,int world_z,int width,int depth,const uint16_t *pixels,
 int source_width,int source_height,uint16_t color)
{
 int near=world_z-depth/2,far=world_z+(depth+1)/2;
 if(far < -44 || near>ROAD_VIEW+depth)return;
 int top=clampi(project_y(far),HORIZON+1,VH-1);
 int bottom=clampi(project_y(near < -44?-44:near),HORIZON+1,VH-1);
 for(int y=top;y<=bottom;y++) {
  int z=row_z[y];if(z<near || z>far)continue;
  int scale=row_scale[y],w=width*scale/256;
  if(w<1)continue;
  int left=128+((world_x-v->center-v->cam_x)*scale)/256-w/2;
  int begin=left<0?-left:0,end=left+w>VW?VW-left:w;
  if(end<=begin)continue;
  uint16_t *dest=s->pixels+y*s->w+left+begin;
  if(!pixels) {
   for(int x=begin;x<end;x++)*dest++=color;
  } else {
   int sy=clampi(((far-z)*source_height)/depth,0,source_height-1);
   const uint16_t *row=pixels+sy*source_width;
   unsigned step=vertical_step[clampi(w,1,255)]*(unsigned)source_width;
   unsigned sample=(unsigned)begin*step;
   for(int x=begin;x<end;x++,sample+=step) {
    uint16_t pixel=row[sample>>16];
    if(pixel)*dest=pixel;
    dest++;
   }
  }
 }
}

/* Buildings are small world-space solids, not skewed isometric cards.  Every
 * roof/wall vertex uses the road camera.  In particular, a constant-x edge
 * along z converges on (128,HORIZON), while its vertical edges stay vertical.
 * The wall elevations contain only flat, orthographic doors and windows. */
typedef struct {int x,y;} SceneryPoint;

static SceneryPoint scenery_point(const View *v,int x,int z,int height)
{
 int scale=project_scale(z);
 SceneryPoint point={128+((x-v->center-v->cam_x)*scale)/256,
                     project_y(z)-height*scale/256};
 return point;
}

static FAST __attribute__((noinline)) void scenery_polygon(Surface *s,const SceneryPoint *points,int count,uint16_t color)
{
 int left[VH],right[VH];
 int first=VH,last=0;
 for(int y=0;y<VH;y++){left[y]=VW;right[y]=-1;}
 for(int edge=0;edge<count;edge++) {
  SceneryPoint a=points[edge],b=points[(edge+1)%count];
  if(a.y>b.y){SceneryPoint swap=a;a=b;b=swap;}
  if(a.y==b.y)continue;
  int y0=clampi(a.y,0,VH),y1=clampi(b.y,0,VH);
  if(y1<=y0)continue;
  int step=((b.x-a.x)*65536)/(b.y-a.y);
  int x=a.x*65536+step*(y0-a.y);
  for(int y=y0;y<y1;y++,x+=step) {
   int at=x/65536;
   if(at<left[y])left[y]=at;
   if(at>right[y])right[y]=at;
  }
  if(y0<first)first=y0;
  if(y1>last)last=y1;
 }
 for(int y=first;y<last;y++)if(right[y]>=left[y])
  road_span(s,left[y],y,right[y]-left[y]+1,color);
}

static int scenery_outcode(SceneryPoint p)
{return (p.x<0?1:p.x>=VW?2:0)|(p.y<0?4:p.y>=VH?8:0);}

static FAST __attribute__((noinline)) void scenery_edge(Surface *s,SceneryPoint a,SceneryPoint b,uint16_t color)
{
 /* A nearby roof can extend far outside the LCD.  Clip before Bresenham
  * walks it; fully hidden tiles must cost no framebuffer pixel calls. */
 int ca=scenery_outcode(a),cb=scenery_outcode(b);
 for(int iteration=0;iteration<8;iteration++) {
  if(!(ca|cb)) {
   int dx=b.x-a.x,dy=b.y-a.y,sx=dx<0?-1:1,sy=dy<0?-1:1;
   if(dx<0)dx=-dx;
   if(dy<0)dy=-dy;
   int error=dx-dy;
   for(;;) {
    s->pixels[a.y*VW+a.x]=color;
    if(a.x==b.x && a.y==b.y)return;
    int twice=error*2;
    if(twice>-dy){error-=dy;a.x+=sx;}
    if(twice<dx){error+=dx;a.y+=sy;}
   }
  }
  if(ca&cb)return;
  int code=ca?ca:cb;
  SceneryPoint p;
  if(code&8){p.y=VH-1;p.x=a.x+(b.x-a.x)*(p.y-a.y)/(b.y-a.y);}
  else if(code&4){p.y=0;p.x=a.x+(b.x-a.x)*(p.y-a.y)/(b.y-a.y);}
  else if(code&2){p.x=VW-1;p.y=a.y+(b.y-a.y)*(p.x-a.x)/(b.x-a.x);}
  else {p.x=0;p.y=a.y+(b.y-a.y)*(p.x-a.x)/(b.x-a.x);}
  if(code==ca){a=p;ca=scenery_outcode(a);}else {b=p;cb=scenery_outcode(b);}
 }
}

/* One perspective division per visible screen column, followed by a compact
 * row-major raster.  There is no matrix stack, z buffer, or per-pixel divide.
 * Perspective-correct horizontal texture coordinates keep window rows and
 * vertical window edges attached to the actual wall plane. */
static FAST __attribute__((noinline)) void building_wall(Surface *s,const View *v,
 int ax,int az,int bx,int bz,int height,const uint16_t *texture,int tw,int th)
{
 if(az < -44 && bz < -44)return;
 int ua=0,ub=tw-1;
 if(az < -44) {ua=((-44-az)*(tw-1))/(bz-az);az=-44;}
 if(bz < -44) {ub=((az+44)*(tw-1))/(az-bz);bz=-44;}
 SceneryPoint a=scenery_point(v,ax,az,0),b=scenery_point(v,bx,bz,0);
 SceneryPoint at=scenery_point(v,ax,az,height),bt=scenery_point(v,bx,bz,height);
 int sa=project_scale(az),sb=project_scale(bz);
 if(a.x>b.x) {
  SceneryPoint swap=a;a=b;b=swap;swap=at;at=bt;bt=swap;
  int temp=sa;sa=sb;sb=temp;temp=ua;ua=ub;ub=temp;
 }
 int span=b.x-a.x;
 int x0=clampi(a.x,0,VW),x1=clampi(b.x+1,0,VW);
 if(span<1 || x1<=x0)return;
 typedef struct {int16_t top,bottom;uint16_t u;uint32_t step;} WallColumn;
 WallColumn columns[VW];
 int top_step=((bt.y-at.y)*65536)/span,bottom_step=((b.y-a.y)*65536)/span;
 int scale_step=((sb-sa)*65536)/span;
 int u_step=((ub*sb-ua*sa)*65536)/span;
 int offset=x0-a.x;
 int top=at.y*65536+top_step*offset,bottom=a.y*65536+bottom_step*offset;
 int scale=sa*65536+scale_step*offset,u=ua*sa*65536+u_step*offset;
 int first=VH,last=0;
 for(int x=x0;x<x1;x++,top+=top_step,bottom+=bottom_step,scale+=scale_step,u+=u_step) {
  int t=top/65536,base=bottom/65536;
  if(base<=t || scale<=0){columns[x]=(WallColumn){0,0,0,0};continue;}
  int tex=clampi(u/scale,0,tw-1);
  columns[x]=(WallColumn){(int16_t)t,(int16_t)base,(uint16_t)tex,
                           (uint32_t)((th*65536)/(base-t))};
  int y0=clampi(t,0,VH),y1=clampi(base,0,VH);
  if(y0<first)first=y0;
  if(y1>last)last=y1;
 }
 /* Top and bottom boundaries are monotone across this vertical plane.
  * Four advancing edge cursors give the exact occupied span of each row,
  * including the original integer rounding. Empty trapezoid corners cost no
  * texture fetches and the inner loop needs no coverage/alpha branches. */
 int top_left=x0,top_right=top_step>=0?x0:x1;
 int bottom_left=x0,bottom_right=x1;
 if(top_step<0)top_left=x1;
 for(int y=first;y<last;y++) {
  if(top_step>=0) {
   while(top_right<x1 && columns[top_right].top<=y)top_right++;
  } else {
   while(top_left>x0 && columns[top_left-1].top<=y)top_left--;
  }
  if(bottom_step>=0) {
   while(bottom_left<x1 && columns[bottom_left].bottom<=y)bottom_left++;
  } else {
   while(bottom_right>x0 && columns[bottom_right-1].bottom<=y)bottom_right--;
  }
  int left=top_left>bottom_left?top_left:bottom_left;
  int right=top_right<bottom_right?top_right:bottom_right;
  uint16_t *dest=s->pixels+y*VW+left;
  for(int x=left;x<right;x++,dest++) {
   const WallColumn *column=&columns[x];
   unsigned sy=((unsigned)(y-column->top)*column->step)>>16;
   *dest=texture[sy*(unsigned)tw+column->u];
  }
 }
}

static int building_style(int id)
{return id==SC_SHOP?1:id==SC_WAREHOUSE?2:id==SC_HOTEL?3:id==SC_VILLA?4:0;}

static const uint16_t *building_end_texture(int style,int mirrored,int width)
{
 static uint16_t reflected[5][32*64+16*32+8*16];
 static uint8_t ready[5];
 int height=width*2;
 int offset=width==32?0:width==16?32*64:32*64+16*32;
 unsigned flag=width==32?1u:width==16?2u:4u;
 const uint16_t *source=(width==32?building_end_art:
                        width==16?building_end_lod16:building_end_lod8)+style*width*height;
 if(!mirrored)return source;
 if(!(ready[style]&flag)) {
  for(int y=0;y<height;y++)for(int x=0;x<width;x++)
   reflected[style][offset+y*width+x]=source[y*width+width-1-x];
  ready[style]|=(uint8_t)flag;
 }
 return reflected[style]+offset;
}

static void building_end_wall(Surface *s,const View *v,int inner,int outer,int z,
 int height,int style)
{
 if(z < -44)return;
 SceneryPoint a=scenery_point(v,inner,z,0),b=scenery_point(v,outer,z,height);
 int left=a.x<b.x?a.x:b.x,width=b.x-a.x;
 if(width<0)width=-width;
 int h=a.y-b.y;
 if(width<1 || h<1 || left>=VW || left+width<0 || b.y>=VH || a.y<0)return;
 /* Constant-z faces are opaque rectangles. They deliberately bypass the
  * shared car/foliage cache: a newly resized wall must not evict traffic. */
 width++;
 int x0=clampi(left,0,VW),x1=clampi(left+width,0,VW);
 int y0=clampi(b.y,0,VH),y1=clampi(a.y,0,VH);
 if(x1<=x0 || y1<=y0)return;
 int source_width=width<=8 && h<=16?8:width<=16 && h<=32?16:32;
 int source_height=source_width*2;
 const uint16_t *texture=building_end_texture(style,a.x>b.x,source_width);
 uint8_t source_x[VW];
 unsigned x_step=((unsigned)source_width*65536u+(unsigned)width-1)/(unsigned)width;
 unsigned y_step=((unsigned)source_height*65536u+(unsigned)h-1)/(unsigned)h;
 for(int x=x0;x<x1;x++)source_x[x]=(uint8_t)(((unsigned)(x-left)*x_step)>>16);
 unsigned sample=(unsigned)(y0-b.y)*y_step;
 for(int y=y0;y<y1;y++,sample+=y_step) {
  const uint16_t *row=texture+(sample>>16)*(unsigned)source_width;
  uint16_t *dest=s->pixels+y*VW+x0;
  int x=x0;
  for(;x+3<x1;x+=4) {
   dest[0]=row[source_x[x]];dest[1]=row[source_x[x+1]];
   dest[2]=row[source_x[x+2]];dest[3]=row[source_x[x+3]];dest+=4;
  }
  for(;x<x1;x++)*dest++=row[source_x[x]];
 }
}

static FAST __attribute__((noinline)) void draw_building(Surface *s,const View *v,const DrawItem *item)
{
 static const uint8_t widths[5]={54,62,68,58,60};
 static const uint8_t depths[5]={88,92,104,92,94};
 static const uint8_t heights[5]={48,43,47,76,52};
 static const uint8_t pitches[5]={18,12,8,10,20};
 int style=building_style(item->id),width=widths[style],depth=depths[style];
 int height=heights[style],pitch=pitches[style],side=item->side;
 int inner=item->x,outer=inner+side*width,ridge=inner+side*(width/2);
 int near=item->z,far=near+depth;
 if(far < -44)return;
 SceneryPoint ib=scenery_point(v,inner,far,0),ob=scenery_point(v,outer,far,0);
 SceneryPoint inf=scenery_point(v,inner,near,0),onf=scenery_point(v,outer,near,0);
 int minx=inf.x,maxx=onf.x;
 if(minx>maxx){int temp=minx;minx=maxx;maxx=temp;}
 if(ib.x<minx)minx=ib.x;
 if(ib.x>maxx)maxx=ib.x;
 if(ob.x<minx)minx=ob.x;
 if(ob.x>maxx)maxx=ob.x;
 if(maxx<0 || minx>=VW)return;
 unsigned wall_start=profile_clock();
 uint16_t paving=C(184,190,174);
 ground_plane(s,v,inner+side*width/2,near+depth/2,width+6,depth+7,0,0,0,paving);
 int projected_width=inf.x-ib.x;if(projected_width<0)projected_width=-projected_width;
 int projected_height=height*project_scale(near < -44?-44:near)/256;
 int resolution=projected_width<=16 && projected_height<=16?16:
                projected_width<=32 && projected_height<=32?32:64;
 const uint16_t *wall=(resolution==64?building_wall_art:
                       resolution==32?building_wall_lod32:building_wall_lod16)+style*resolution*resolution;
 building_wall(s,v,inner,near,inner,far,height,wall,resolution,resolution);
 building_end_wall(s,v,inner,outer,near,height,style);
 scenery_work[6]+=(profile_clock()-wall_start)&65535u;
 unsigned roof_start=profile_clock();
 SceneryPoint in=scenery_point(v,inner,near,height),iff=scenery_point(v,inner,far,height);
 SceneryPoint out=scenery_point(v,outer,near,height),of=scenery_point(v,outer,far,height);
 SceneryPoint rn=scenery_point(v,ridge,near,height+pitch),rf=scenery_point(v,ridge,far,height+pitch);
 const uint16_t *roof=building_roof_art+style*32*32;
 uint16_t roof_color=roof[16*32+16],roof_dark=blend_color(roof_color,C(26,49,53),36);
 uint16_t seam=blend_color(roof[8*32+8],C(39,53,57),64);
 SceneryPoint outer_plane[4]={out,of,rf,rn};
 SceneryPoint inner_plane[4]={in,iff,rf,rn};
 int camera_delta=ridge-v->center-v->cam_x;if(camera_delta<0)camera_delta=-camera_delta;
 int outer_visible=(130-height-pitch)*(width/2)>pitch*camera_delta;
 if(outer_visible)scenery_polygon(s,outer_plane,4,roof_dark);
 scenery_polygon(s,inner_plane,4,roof_color);
 /* Tile seams are world-space lines on the pitched roof.  Longitudinal
  * seams share the road vanishing point; cross seams follow the roof slope. */
 for(int u=1;u<4;u++) {
  int x=inner+side*(width*u/8),y=height+pitch*u/4;
  scenery_edge(s,scenery_point(v,x,near,y),scenery_point(v,x,far,y),seam);
 }
 for(int z=near+18;z<far;z+=18) {
  scenery_edge(s,scenery_point(v,inner,z,height),scenery_point(v,ridge,z,height+pitch),seam);
  if(outer_visible)scenery_edge(s,scenery_point(v,ridge,z,height+pitch),scenery_point(v,outer,z,height),seam);
 }
 if(near>=-44) {
  SceneryPoint gable[3]={in,out,rn};
  uint16_t wall=building_end_art[style*32*64+4*32+16];
  scenery_polygon(s,gable,3,wall);
  scenery_edge(s,in,rn,seam);scenery_edge(s,rn,out,seam);
  scenery_edge(s,in,inf,C(86,104,102));scenery_edge(s,out,onf,C(86,104,102));
 }
 scenery_edge(s,in,iff,seam);scenery_edge(s,rn,rf,seam);
 scenery_work[7]+=(profile_clock()-roof_start)&65535u;
}

static void draw_water_prop(Surface *s,const View *v,const DrawItem *item)
{
 if(item->z < -44)return;
 int scale=project_scale(item->z),x=item_x(v,item->x,item->z),y=project_y(item->z);
 if(item->id==SC_SAIL) {
  int w=78*scale/256,h=w*48/64;
  if(w<3 || h<3)return;
  uint16_t sea=sea_color(row_biome[clampi(y,HORIZON+1,VH-1)]);
  ellipse(s,x,y+1,w+3,clampi(w/9,1,4),blend_color(sea,C(193,233,225),85));
  gfx_scaled_sprite(s,x-boat_profile_anchor[0]*w/64,y-boat_profile_anchor[1]*h/48,
                    boat_profile,64,48,w,h);
 } else {
  int sections=1+(((unsigned)item->variant>>8)&1);
  int width=60,depth=16;
  /* The innermost edge is exactly the terrain shoreline (ROAD_HALF+7).
   * Both sections share z and a common endpoint; they cannot float apart. */
  for(int section=sections-1;section>=0;section--) {
   int center=item->x+item->side*(section*width+width/2);
   ground_plane(s,v,center,item->z,width,depth,dock_segment,64,32,0);
   int far_x=item->x+item->side*((section+1)*width);
   for(int edge=-1;edge<=1;edge+=2) {
    int z=item->z+edge*(depth/2),sx=item_x(v,far_x,z),sy=project_y(z);
    int high=clampi(5*project_scale(z)/256,1,5);
    gfx_line(s,sx,sy,sx,sy-high,C(172,145,103));
    gfx_rect(s,sx,sy-high,2,1,C(235,219,173));
   }
  }
 }
}

static void draw_prop(Surface *s,const View *v,const DrawItem *item)
{
 if(building_id(item->id)){draw_building(s,v,item);return;}
 if(item->id==SC_SAIL || item->id==SC_PIER){draw_water_prop(s,v,item);return;}
 if(item->z < -44)return;
 int y=project_y(item->z),x=item_x(v,item->x,item->z);
 int scale=project_scale(item->z),world_width=48;
 if(item->id==SC_LAMP)world_width=36;
 if(item->kind==ITEM_BACK_PROP)world_width=world_width*218/256;
 int w=world_width*scale/256,h=w*PROP_H/PROP_W;
 if(w<2 || h<3)return;
 int source_width,source_height;
 const uint8_t *anchor,*bounds;
 const uint16_t *pixels=prop_source(item->id,item->side,w,&source_width,&source_height,&anchor,&bounds);
 (void)bounds;
 int contact_width=item->id==SC_LAMP?clampi(w/8,1,4):clampi(w/5,2,8);
 ellipse(s,x,y,contact_width,clampi(contact_width/3,1,3),C(53,81,73));
 int sway=(item->id==SC_REEDS || item->id==SC_BUSH)?
             (int)((v->frame/14+(unsigned)item->variant)&1):0;
 gfx_scaled_sprite(s,x-anchor[0]*w/PROP_W+sway,y-anchor[1]*h/PROP_H,
                   pixels,source_width,source_height,w,h);
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
 int x=item_x(v,item->x,item->z);
 VehicleScreenRect r=vehicle_traffic_rect(x,item->z);
 if(r.width<2 || r.height<3)return;
 contact_shadow(s,r.left,r.top,r.width,r.height,traffic_body_spans[item->id],TRAFFIC_W,TRAFFIC_H);
 gfx_vehicle(s,r.left,r.top,traffic_art+item->id*TRAFFIC_W*TRAFFIC_H,
             TRAFFIC_W,TRAFFIC_H,r.width,r.height,item->variant%6);
}

static void draw_player(Surface *s,const Game *g,const View *v,const DrawItem *item)
{
 int x=item_x(v,item->x,0),y=PLAYER_Y;
 int vehicle=v->attract?g->save.selected_vehicle:g->vehicle;
 int yaw=v->attract?((int)(v->frame/32)%3-1)*12:g->yaw;
 int pose=vehicle_yaw_pose(yaw);
 int lift=!v->attract && g->wheelie?3+(g->frame/6&1):0;
 VehicleScreenRect r=vehicle_player_rect(x,vehicles[vehicle].bike,lift);
 int w=r.width,h=r.height;
 contact_shadow(s,x-w/2,y-h/2,w,h,vehicle_body_spans[vehicle*5+pose],CAR_W,CAR_H);
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
 gfx_vehicle(s,r.left,r.top,art,CAR_W,CAR_H,w,h,paint);
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


static void draw_police(Surface *s,const Game *g,const View *v)
{
 if(v->attract)return;
 int pressure=clampi(g->police_pressure,0,100);
 int x=item_x(v,v->center+g->x_q8/Q,0);
 int y=215-pressure*23/100,w=28,h=42;
 x+=26*pressure/100;
 /* Pursuit is a run-ending pressure system, separate from NPC traffic.
  * The patrol approaches visibly from behind only when the player stops. */
 if(pressure>5) {
  gfx_vehicle(s,x-w/2,y-h/2,traffic_art,TRAFFIC_W,TRAFFIC_H,w,h,5);
  gfx_rect(s,x-7,y-4,14,3,C(29,44,57));
  int flash=(g->frame/4)&1;
  gfx_rect(s,x-6,y-4,6,2,flash?C(255,80,66):C(96,45,56));
  gfx_rect(s,x+1,y-4,6,2,flash?C(32,64,109):C(70,169,255));
  gfx_rect(s,x-5,y+3,10,1,C(242,245,220));
 }
 if(pressure>25) {
  int pulse=clampi(pressure/15,2,6);
  uint16_t left=(g->frame/4)&1?C(226,88,84):C(72,129,192);
  gfx_rect(s,0,VH-2,pulse,2,left);
  gfx_rect(s,VW-pulse,VH-2,pulse,2,left);
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
 /* Reserve the player before any optional scenery or pooled traffic. */
 add_item(items,&count,0,v.center+(attract?0:g->x_q8/Q),ITEM_PLAYER,0,0,0);
 unsigned queue_start=profile_clock();
 queue_scenery(items,&count,&v);
 for(int q=0;q<8;q++)scenery_work[q]=0;
 scenery_work[0]=(profile_clock()-queue_start)&65535u;
 if(attract) {
  for(int i=0;i<6;i++) {
   int z=70+(int)((g->frame*(2+i%2)+i*113)%550);
   RoadSample r;road_sample(v.seed,v.position+z,v.start,&r);
   int lane=(i*3+1)%4;
   add_item(items,&count,z,r.center-72+lane*48,ITEM_CAR,i%6,i%6,0);
  }
 } else {
  for(int i=0;i<TRAFFIC_COUNT;i++)if(g->traffic[i].active) {
   int z=(g->traffic[i].z_q8-g->position_q8)/Q;
   RoadSample r;road_sample(v.seed,g->traffic[i].z_q8/Q,v.start,&r);
   add_item(items,&count,z,r.center+g->traffic[i].x_q8/Q,ITEM_CAR,
            g->traffic[i].kind%6,g->traffic[i].paint,0);
  }
  for(int i=0;i<PICKUP_COUNT;i++)if(g->pickups[i].active) {
   int z=(g->pickups[i].z_q8-g->position_q8)/Q;
   RoadSample r;road_sample(v.seed,g->pickups[i].z_q8/Q,v.start,&r);
   add_item(items,&count,z,r.center+g->pickups[i].x_q8/Q,ITEM_COIN,
            g->pickups[i].serial,0,0);
  }
 }
 for(int i=0;i<count;i++) {
  unsigned stage_start=profile_clock();
  switch(items[i].kind) {
  case ITEM_PROP:case ITEM_BACK_PROP:
   draw_prop(s,&v,&items[i]);
   scenery_work[building_id(items[i].id)?1:(items[i].id==SC_PIER || items[i].id==SC_SAIL)?3:2]+=
      (profile_clock()-stage_start)&65535u;break;
  case ITEM_CAR:draw_traffic(s,&v,&items[i]);scenery_work[4]+=(profile_clock()-stage_start)&65535u;break;
  case ITEM_COIN:draw_coin(s,&v,&items[i]);break;
  case ITEM_PLAYER:draw_player(s,g,&v,&items[i]);scenery_work[5]+=(profile_clock()-stage_start)&65535u;break;
  default:break;
  }
 }
 draw_police(s,g,&v);
 draw_ambient(s,&v);
 if(!attract && v.motion && v.speed>145)for(int i=0;i<(g->turbo?3:2);i++) {
  int y=HORIZON+10+(int)(((unsigned)v.visual_position*2+(unsigned)i*67)%(VH-HORIZON-20));
  int side=(i&1)?1:-1;
  int x=road_x[y]+side*(road_half[y]-6);
  gfx_line(s,x,y,x+side*2,y+clampi(v.speed/35,4,10),C(137,176,180));
 }
 for(int q=0;q<8;q++)scenery_profile[q]=scenery_work[q];
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
 static uint16_t combo_art[48*18],location_art[LOCATION_COUNT][157*19],notice_art[240*17];
 static uint8_t location_ready[LOCATION_COUNT];
 static char old_notice[88];
 static int old_combo=-1,notice_width;
 static int old_skin=-1;
 const StudioTheme *theme=theme_for(g);
 if(old_skin!=g->save.skin){memset(location_ready,0,sizeof(location_ready));old_combo=-1;notice_width=0;old_skin=g->save.skin;}
 if(g->combo_q8>Q+Q/8) {
  int value=g->combo_q8*100/Q;
  if(value!=old_combo) {
   Surface cache={combo_art,48,18};
   gfx_rect(&cache,0,0,48,18,0);
   char combo[16];snprintf(combo,sizeof(combo),"%u.%02ux",g->combo_q8/Q,(g->combo_q8%Q)*100/Q);
   gfx_card(&cache,0,0,48,18,theme->pane,theme->rim);
   gfx_text(&cache,6,2,combo,theme->accent);
   old_combo=value;
  }
  copy_cached_card(s,201,1,combo_art,48,48,18);
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
 contact_shadow(scene,x,y,w,h,vehicle_body_spans[id*5+pose],CAR_W,CAR_H);
 gfx_vehicle(scene,x,y,art,CAR_W,CAR_H,w,h,paint);
 gfx_card(scene,10,7,236,27,theme->pane,theme->rim);
 const char *receipt_title=g->end_reason==END_STOPPED?"PURSUIT ENDED":
                           g->end_reason==END_CRASH?"VEHICLE RECOVERED":"RUN BANKED";
 const char *receipt_subtitle=g->end_reason==END_STOPPED?"TRAFFIC DIVISION / CITATION ISSUED":"REWARDS SECURED";
 gfx_text(scene,18,11,receipt?receipt_title:"COASTLINE  /  MOTOR STUDIO",theme->ink);
 gfx_text_small(scene,18,24,receipt?receipt_subtitle:g->garage_tab==1?"PARTS  /  BUILT FOR THE LONG RUN":
      g->garage_tab==2?"SETUP  /  FIND YOUR OWN BALANCE":"SHOWROOM  /  YOUR NEXT SET OF KEYS",theme->muted);
 gfx_text(scene,13,40,vehicles[id].name,theme->ink);
 char mastery[24];snprintf(mastery,sizeof(mastery),"MASTERY %d",game_mastery_level(g,id));
 gfx_text_small(scene,174,42,mastery,theme->ink);
 gfx_card(scene,165,61,80,116,theme->pane,theme->rim);
 gfx_rect(scene,172,67,65,2,theme->accent);
 if(receipt) {
  gfx_text_small(scene,173,74,g->end_reason==END_STOPPED?"CAUGHT":
                 g->end_reason==END_CRASH?"RUN CLOSED":"RUN BANKED",theme->accent);
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
