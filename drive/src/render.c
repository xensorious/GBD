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
#define CAR_W 48
#define CAR_H 72
#define TRAFFIC_W 40
#define TRAFFIC_H 64
#define PROP_W 64
#define PROP_H 96

enum { SC_BUILDING=0,SC_SHOP=1,SC_WAREHOUSE=2,SC_PALM=3,SC_PINE=4,SC_TREE=5,
       SC_BUSH=6,SC_ROCK=7,SC_LAMP=8,SC_GANTRY=9,SC_BILLBOARD=10,SC_SAIL=11,
       SC_BARRIER=12 };
enum { ITEM_PROP,ITEM_CAR,ITEM_COIN,ITEM_PLAYER,ITEM_OVERPASS };
typedef struct { int z,x,kind,id,variant; } DrawItem;
typedef struct { int x,z;unsigned tick; } TireMark;
typedef struct {
 unsigned seed,frame;
 int position,center,cam_x,start,attract;
} View;

static int row_z[VH],row_scale[VH],road_x[VH],road_half[VH];
static uint8_t row_biome[VH],row_feature[VH];
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

/* A high, close chase camera: the bottom of the frame has useful lateral
 * space, while the upper frame can show distant bends without a horizon. */
static int project_y(int z)
{return 150-(z*185)/(z+160);}

static int project_scale(int z)
{return 138+(138*160)/(z+160);}

static int item_x(const View *v,int world_x,int z)
{return 128+((world_x-v->center-v->cam_x)*project_scale(z))/256;}

static void init_projection(void)
{
 static int initialized;
 if(initialized)return;
 initialized=1;
 for(int y=0;y<VH;y++) {
  int vertical=150-y;
  row_z[y]=(vertical*160)/(185-vertical);
  row_scale[y]=138+(138*(185-vertical))/185;
 }
}

static void ellipse(Surface *s,int cx,int cy,int width,int height,uint16_t color)
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

static int biome_water(int biome,int side)
{return biome==CAUSEWAY || (biome==COAST && side<0)?256:0;}

static uint16_t terrain_color(int biome)
{
 static const uint16_t colors[LOCATION_COUNT]={
  C(147,160,150),C(109,162,104),C(41,144,169),C(64,111,84),C(183,128,89)};
 return colors[biome%LOCATION_COUNT];
}

/* Road rows always have a valid y coordinate.  Keep the clipped fill in the
 * ITCM caller and avoid generic rectangle setup for each one-pixel span. */
static inline __attribute__((always_inline)) void road_span(Surface *s,int x,int y,int width,uint16_t color)
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
 int knot_x[25];
 for(int k=0;k<25;k++) {
  int y=k*8;if(y>=VH)y=VH-1;
  road_sample(v->seed,v->position+row_z[y],v->start,&knots[k]);
  knot_x[k]=knots[k].center;
 }
 for(int y=0;y<VH;y++) {
  int k=y/8,t=y&7;
  int center=knot_x[k]+((knot_x[k+1]-knot_x[k])*t)/8;
  int scale=row_scale[y];
  int cx=128+((center-v->center-v->cam_x)*scale)/256;
  int half=(ROAD_HALF*scale)/256;
  road_x[y]=cx;road_half[y]=half;
  const RoadSample *r=&knots[k];
  int biome=r->blend>=128?r->next_biome:r->biome;
  row_biome[y]=(uint8_t)biome;row_feature[y]=r->feature;
  uint16_t ground=blend_color(terrain_color(r->biome),terrain_color(r->next_biome),r->blend);
  int world=v->position+row_z[y];
  if((world/7)&1)ground=blend_color(ground,C(142,161,129),12);
  int shoulder=3+(scale>240);
  int outside_left=cx-half-shoulder-3;
  int outside_right=cx+half+shoulder+3;
  road_span(s,0,y,outside_left,ground);
  road_span(s,outside_right,y,VW-outside_right,ground);

  /* Water only occupies the appropriate side of a coastal or bridge road.
   * The shoreline itself approaches gradually during an environment blend. */
  for(int side=-1;side<=1;side+=2) {
   int water=(biome_water(r->biome,side)*(256-r->blend)+
              biome_water(r->next_biome,side)*r->blend)/256;
   if(water) {
    int edge=cx+side*(half+7*scale/256);
    int span=side<0?edge:VW-edge;
    span=clampi((span*water)/256,0,VW);
    int x=side<0?0:VW-span;
    uint16_t sea=C(34,144,172);
    if(((world+36)/31)%4==0)sea=C(42,155,177);
    road_span(s,x,y,span,sea);
    if(span>0)road_span(s,side<0?span-2:x,y,2,C(149,211,190));
    if(span>12 && (world%43+43)%43<2) {
     unsigned wave=road_hash(v->seed,(unsigned)floor_div(world,43));
     int wx=x+3+(int)(wave%(unsigned)(span-9));
     road_span(s,wx,y,5,C(113,197,204));
    }
   }
  }

  uint16_t verge=biome==CITY?C(210,199,167):biome==CANYON?C(216,166,117):C(184,187,143);
  road_span(s,outside_left,y,3,verge);
  road_span(s,cx+half+shoulder,y,3,verge);
  road_span(s,cx-half-shoulder,y,shoulder,C(76,88,91));
  road_span(s,cx+half,y,shoulder,C(76,88,91));
  uint16_t asphalt=((world/12)&1)?C(65,77,84):C(67,79,85);
  road_span(s,cx-half,y,half*2,asphalt);

  /* A little broken aggregate avoids a featureless slab, without turning the
   * road into television noise.  All positions are attached to the road. */
  if((world&7)==0) {
   unsigned h=road_hash(v->seed^0x40a1u,(unsigned)floor_div(world,8));
   int gx=cx-half+6+(int)(h%(unsigned)(half*2-12));
   road_span(s,gx,y,1,C(81,92,96));
  }
  int white=scale>246?2:1;
  road_span(s,cx-half+3,y,white,C(225,225,201));
  road_span(s,cx+half-4,y,white,C(225,225,201));
  int dash=(world%96+96)%96;
  if(dash<34)for(int lane=1;lane<4;lane++) {
   int x=cx-half+(half*lane)/2;
   road_span(s,x,y,white,C(209,215,202));
  }
  if((world%66+66)%66<24) {
   road_span(s,cx-half-shoulder-2,y,2,C(237,208,148));
   road_span(s,cx+half+shoulder,y,2,C(237,208,148));
  }
  if(biome==CAUSEWAY || biome==COAST) {
   for(int side=-1;side<=1;side+=2) {
    int rx=cx+side*(half+shoulder+3);
    road_span(s,rx,y,2,C(215,228,211));
    road_span(s,rx+side*2,y,1,C(76,118,122));
    if((world%84+84)%84<7)road_span(s,rx-1,y,4,C(244,235,202));
   }
  }
 }

 /* Old tyre marks are world-anchored, and disappear after a short while. */
 if(!v->attract)for(unsigned i=0;i<24;i++) {
  const TireMark *m=&tiremarks[i];
  if(!m->tick || g->run_ticks-m->tick>42)continue;
  int z=m->z-v->position;
  if(z < -26 || z>620)continue;
  int y=project_y(z),x=item_x(v,m->x,z);
  int separation=clampi(project_scale(z)*8/256,4,11);
  gfx_rect(s,x-separation,y,2,4,C(43,55,62));
  gfx_rect(s,x+separation,y,2,4,C(43,55,62));
 }
}

static void add_item(DrawItem *items,int *count,int z,int x,int kind,int id,int variant)
{
 if(*count>=90 || z < -29 || z>660)return;
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
  return (hash&3)?SC_PALM:SC_SHOP;
 case CAUSEWAY:return (hash&1)?SC_LAMP:SC_BARRIER;
 case PINES:return (hash&3)?SC_PINE:SC_TREE;
 default:return (hash&3)?SC_ROCK:SC_BUSH;
 }
}

static void queue_scenery(DrawItem *items,int *count,const View *v)
{
 int first=floor_div(v->position-35,88);
 int last=floor_div(v->position+660,88);
 for(int n=first;n<=last;n++)for(int side=-1;side<=1;side+=2) {
  uint32_t h=road_landmark(v->seed,(uint32_t)n,side);
  int absolute=n*88+(int)((h>>10)%25);
  int z=absolute-v->position;
  RoadSample sample;road_sample(v->seed,absolute,v->start,&sample);
  int biome=((h>>16)&255)<sample.blend?sample.next_biome:sample.biome;
  int kind=prop_for_biome(biome,h,side);
  if(biome==COAST && side<0 && (h&3))continue;
  if(biome==CAUSEWAY && (n&1))continue;
  int x=sample.center+side*(125+(int)((h>>6)%26));
  if(biome==CAUSEWAY)x=sample.center+side*(ROAD_HALF+7);
  /* Boats sit offshore, beyond the shoreline rather than on the shoulder. */
  if(kind==SC_SAIL)x+=side*27;
  add_item(items,count,z,x,ITEM_PROP,kind,(int)h);
 }
 /* A gantry is deliberately open over the lanes: posts live outside the
  * collision boundary and a shallow header does not hide approaching cars. */
 int segment=floor_div(v->position,6400);
 for(int n=segment;n<=segment+1;n++) {
  int absolute=n*6400+4840;
  RoadSample r;road_sample(v->seed,absolute,v->start,&r);
  if(r.biome==CITY)add_item(items,count,absolute-v->position,r.center,ITEM_OVERPASS,0,0);
 }
}

static void draw_prop(Surface *s,const View *v,const DrawItem *item)
{
 int y=project_y(item->z),x=item_x(v,item->x,item->z);
 int scale=project_scale(item->z);
 int size=104+(scale*90)/256;
 int w=PROP_W*size/256,h=PROP_H*size/256;
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
 gfx_scaled_sprite(s,x-w/2,y-h,prop_art+item->id*PROP_W*PROP_H,PROP_W,PROP_H,w,h);
}

static void draw_coin(Surface *s,const View *v,const DrawItem *item)
{
 int x=item_x(v,item->x,item->z),y=project_y(item->z);
 int scale=project_scale(item->z);
 int h=clampi(scale*10/256,5,12);
 int spin=(v->frame/4+(unsigned)item->id)&7;
 int width=spin<4?spin+2:9-spin;
 width=clampi(width*h/7,2,h-1);
 shadow(s,x,y+2,h,3);
 ellipse(s,x,y-h/2,width+2,h+2,C(163,98,36));
 ellipse(s,x,y-h/2,width,h,C(251,207,75));
 gfx_rect(s,x-width/2+1,y-h+2,1,clampi(h-4,1,6),C(255,245,175));
}

static void draw_traffic(Surface *s,const View *v,const DrawItem *item)
{
 int x=item_x(v,item->x,item->z),y=project_y(item->z);
 int size=clampi(82+(112*160)/(item->z+160),100,225);
 int w=TRAFFIC_W*size/256,h=TRAFFIC_H*size/256;
 shadow(s,x,y+h/3,w-2,clampi(w/5,2,6));
 gfx_vehicle(s,x-w/2,y-h/2,traffic_art+item->id*TRAFFIC_W*TRAFFIC_H,
             TRAFFIC_W,TRAFFIC_H,w,h,item->variant%6);
}

static int car_pose(int yaw)
{return yaw<-28?0:yaw<-8?1:yaw>28?4:yaw>8?3:2;}

static void draw_player(Surface *s,const Game *g,const View *v,const DrawItem *item)
{
 int x=item_x(v,item->x,0),y=150;
 int vehicle=v->attract?g->save.selected_vehicle:g->vehicle;
 int yaw=v->attract?((int)(v->frame/32)%3-1)*12:g->yaw;
 int pose=car_pose(yaw);
 int w=29,h=44;
 if(vehicles[vehicle].bike){w=24;h=41;}
 int lift=!v->attract && g->wheelie?3+(g->frame/6&1):0;
 shadow(s,x,y+h/3,w-3,7);
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
 if(!v->attract && g->damage_cooldown && (g->frame&2)) {
  /* Flash the chassis only; the stable shadow retains the driving position. */
  if(g->frame&4)gfx_vehicle(s,x-w/2,y-h/2-lift,art,CAR_W,CAR_H,w,h,5);
 } else gfx_vehicle(s,x-w/2,y-h/2-lift,art,CAR_W,CAR_H,w,h,g->save.paint[vehicle]);
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
 v.position=attract?(int)((g->frame*4)%105000):g->position_q8/Q;
 v.start=attract?(g->mode==M_DEPARTURES?g->cursor:g->save.selected_location):g->start_biome;
 RoadSample player;road_sample(v.seed,v.position,v.start,&player);
 v.center=player.center;
 v.cam_x=attract?0:g->x_q8/(Q*8);

 if(!attract) {
  if(trail_seed!=g->run_seed || g->run_ticks<last_trail_tick) {
   for(int i=0;i<24;i++)tiremarks[i].tick=0;
   trail_seed=g->run_seed;tire_cursor=0;last_trail_tick=0;
  }
  if(g->drifting && g->speed_q8>30*Q && g->run_ticks-last_trail_tick>=2) {
   tiremarks[tire_cursor]=(TireMark){v.center+g->x_q8/Q,v.position,g->run_ticks};
   tire_cursor=(tire_cursor+1)%24;last_trail_tick=g->run_ticks;
  }
 }
 unsigned road_start=profile_clock();
 draw_road(s,g,&v);
 render_profile[3]=(profile_clock()-road_start)&65535u;
 unsigned object_start=profile_clock();
 DrawItem items[90];int count=0;
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
  case ITEM_PROP:draw_prop(s,&v,&items[i]);break;
  case ITEM_CAR:draw_traffic(s,&v,&items[i]);break;
  case ITEM_COIN:draw_coin(s,&v,&items[i]);break;
  case ITEM_PLAYER:draw_player(s,g,&v,&items[i]);break;
  default:draw_gantry(s,&v,&items[i]);break;
  }
 }
 draw_ambient(s,&v);
 if(!attract && g->turbo)for(int i=0;i<3;i++) {
  int y=(int)((g->frame*9+i*67)%180);
  int side=(i&1)?1:-1;
  int x=road_x[y]+side*(road_half[y]-6);
  gfx_line(s,x,y,x+side*2,y+7,C(137,176,180));
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
 if(g->combo_q8>Q+Q/8) {
  int value=g->combo_q8*10/Q;
  if(value!=old_combo) {
   Surface cache={combo_art,39,18};
   gfx_rect(&cache,0,0,39,18,0);
   char combo[16];snprintf(combo,sizeof(combo),"%u.%ux",g->combo_q8/Q,(g->combo_q8%Q)*10/Q);
   gfx_card(&cache,0,0,39,18,C(23,48,58),C(97,141,143));
   gfx_text(&cache,6,2,combo,C(250,219,129));
   old_combo=value;
  }
  copy_cached_card(s,210,7,combo_art,39,39,18);
 }
 if(g->run_ticks<150) {
  int id=g->start_biome;
  if(!location_ready[id]) {
   Surface cache={location_art[id],157,19};
   gfx_rect(&cache,0,0,157,19,0);
   gfx_card(&cache,0,0,157,19,C(24,48,58),C(114,148,147));
   gfx_text_small(&cache,7,4,locations[id].name,C(241,230,187));
   location_ready[id]=1;
  }
  copy_cached_card(s,8,7,location_art[id],157,157,19);
 }
 if(g->notice_timer && g->notice[0]) {
  if(!notice_width || strcmp(old_notice,g->notice)!=0) {
   notice_width=clampi(gfx_text_width(g->notice)+14,100,240);
   Surface cache={notice_art,240,17};
   gfx_rect(&cache,0,0,240,17,0);
   gfx_card(&cache,0,0,notice_width,17,C(28,49,58),C(116,146,143));
   gfx_text_small(&cache,7,3,g->notice,C(248,231,174));
   memcpy(old_notice,g->notice,sizeof(old_notice));
  }
  copy_cached_card(s,(VW-notice_width)/2,171,notice_art,240,notice_width,17);
 }
 if(g->crash_timer) {
  gfx_rect(s,0,0,VW,3,C(216,99,78));gfx_rect(s,0,VH-3,VW,3,C(216,99,78));
 }
}

static void draw_showroom(Surface *s,const Game *g,int receipt)
{
 for(int y=0;y<VH;y++) {
  uint16_t c=y<110?blend_color(C(24,47,60),C(46,72,80),y*2):
      blend_color(C(83,109,110),C(33,57,69),(y-110)*3);
  gfx_rect(s,0,y,VW,1,c);
 }
 gfx_rect(s,0,104,VW,3,C(192,172,114));
 gfx_rect(s,0,110,VW,2,C(22,45,55));
 for(int i=0;i<7;i++)gfx_line(s,128,110,i*48-16,VH-1,C(77,99,103));
 for(int y=122;y<VH;y+=17)gfx_rect(s,0,y,VW,1,C(71,93,99));
 gfx_rect(s,13,45,55,3,C(211,224,199));gfx_rect(s,188,45,55,3,C(211,224,199));
 gfx_rect(s,14,48,53,4,C(69,119,123));gfx_rect(s,189,48,53,4,C(69,119,123));
 ellipse(s,130,163,134,33,C(30,53,64));
 ellipse(s,128,158,134,30,C(126,151,145));
 ellipse(s,128,157,126,26,C(76,103,110));
 ellipse(s,128,159,89,15,C(35,60,70));
 int id=receipt?g->vehicle:g->selection;
 if(id<0 || id>=VEHICLE_COUNT)id=g->save.selected_vehicle;
 int pose=2+(int)((g->frame/100)%3)-1;
 int w=vehicles[id].bike?62:79,h=118;
 shadow(s,129,157,w+5,15);
 const uint16_t *art=vehicle_art+(id*5+pose)*CAR_W*CAR_H;
 int paint=!receipt && g->cursor==5?g->paint_choice:g->save.paint[id];
 gfx_vehicle(s,128-w/2,164-h,art,CAR_W,CAR_H,w,h,paint);
 gfx_text(s,14,12,receipt?"END OF THE ROAD":"COASTLINE MOTOR WORKS",C(239,227,185));
 gfx_text_small(s,14,29,vehicles[id].name,C(154,196,189));
 if(receipt) {
  gfx_card(s,173,66,69,25,C(36,63,72),C(124,151,145));
  gfx_text_small(s,180,72,"RUN BANKED",C(241,213,147));
 }
}

static uint16_t menu_header_art[VW*33],menu_footer_art[4][VW*17];
static const char *const menu_captions[4]={
 "CITY LIGHTS TO OPEN WATER","A NEW HORIZON STARTS HERE",
 "MAKE EVERY MILE COUNT","BUILT FOR BUTTONS"};

static void init_menu_chrome(void)
{
 static int ready;
 if(ready)return;
 Surface header={menu_header_art,VW,33};
 gfx_rect(&header,0,0,VW,33,C(24,48,59));
 gfx_rect(&header,0,31,VW,2,C(204,184,125));
 gfx_text(&header,14,5,"C O A S T L I N E",C(242,229,186));
 gfx_text_small(&header,15,21,"ENDLESS ROADS.  YOUR OWN LINE.",C(149,193,185));
 for(int i=0;i<4;i++) {
  Surface footer={menu_footer_art[i],VW,17};
  gfx_rect(&footer,0,0,VW,17,C(24,48,59));
  gfx_text_small(&footer,14,5,menu_captions[i],C(224,212,167));
 }
 ready=1;
}

static void draw_menu_decoration(Surface *s,const Game *g)
{
 if(g->mode==M_GARAGE || g->mode==M_RECEIPT) {
  draw_showroom(s,g,g->mode==M_RECEIPT);return;
 }
 draw_world(s,g,1);
 /* Letterboxing gives the menu's animated drive a calmer, cinematic frame. */
 init_menu_chrome();
 int caption=g->mode==M_DEPARTURES?1:g->mode==M_CHALLENGES?2:g->mode==M_CONTROLS?3:0;
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
