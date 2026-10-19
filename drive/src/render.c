#include "render.h"
#include "gfx.h"
#include "assets.h"
#include "road.h"
#include "vehicle_projection.h"
#include "vehicle_geometry.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#ifdef ARM9
#include <nds/arm9/math.h>
#endif

/* The renderer never consumes the simulation's random stream.  Every roadside
 * detail has a world coordinate, including while reversing past it. */
extern void ui_draw_bottom(Surface *,const Game *);

#ifdef ARM9
extern unsigned ds_profile_clock(void);
static unsigned profile_clock(void){return ds_profile_clock();}
#else
static unsigned profile_clock(void){return 0;}
#endif
/* ARM9 timer ticks at BUS_CLOCK/64: top, bottom, total, road, objects. */
volatile uint32_t render_profile[5];
volatile uint32_t scenery_profile[8];
static uint32_t scenery_work[8];

#ifdef __arm__
#define FAST __attribute__((section(".itcm"),long_call))
#else
#define FAST
#endif
/* ARM9 has a dedicated signed divider. Variable perspective quotients are
 * substantially cheaper here than libgcc's software long division. */
static FAST __attribute__((noinline,noclone,const)) int scene_divide(int numerator,int denominator)
{
#ifdef ARM9
 return div32(numerator,denominator);
#else
 return numerator/denominator;
#endif
}
#define VW 256
#define VH 192
#define HORIZON world_horizon
#define CAMERA DRIVE_CAMERA
#define PLAYER_Y world_player_y
#define FAR_DISTANCE 2200
#define DRAW_MAX 112
#define WORLD_MAX_HEIGHT 384
static int world_height=VH,world_horizon=DRIVE_HORIZON,world_player_y=DRIVE_PLAYER_Y,sky_origin;
#define CAR_W 48
#define CAR_H 72
#define TRAFFIC_W 40
#define TRAFFIC_H 64
#define PROP_W 64
#define PROP_H 96

enum { SC_BUILDING=0,SC_SHOP=1,SC_WAREHOUSE=2,SC_PALM=3,SC_PINE=4,SC_TREE=5,
       SC_BUSH=6,SC_ROCK=7,SC_LAMP=8,SC_GANTRY=9,SC_BILLBOARD=10,SC_SAIL=11,
       SC_MANGROVE=12,SC_CITRUS=13,SC_HOTEL=14,SC_REEDS=15,SC_VILLA=16,SC_PIER=17,
       SC_VENDING=18,SC_SCULPTURE=19,SC_UTILITY=20,SC_NEON_PALM=21,
       SC_BANANA=22,SC_BAMBOO=23,SC_FERN=24,SC_CANOPY=25,SC_FLOWER=26,SC_VINE=27,
       SC_CYPRESS=28,SC_DEADWOOD=29,SC_RUSH=30,SC_LOG=31,
       SC_SPRUCE=32,SC_BIRCH=33,SC_SNOW_ROCK=34,SC_SNOW_BUSH=35,SC_SNOW_BANK=36,
       SC_CRYSTAL=37,SC_SOLAR=38,SC_SALT_MOUND=39,SC_SCRUB=40,SC_FLAMINGO=41,
       SC_BARRIER=255 };
enum { BL_HOUSE,BL_SHOP,BL_WAREHOUSE,BL_HOTEL,BL_VILLA,
       BL_ARCADE,BL_NOODLES,BL_OFFICE,BL_CLUB,BL_JUNGLE_LODGE,BL_JUNGLE_HUT,
       BL_BAYOU_CABIN,BL_BOATHOUSE,BL_CHALET,BL_OBSERVATORY,BL_SNOW_LODGE,
       BL_PAVILION,BL_SOLAR_STATION,BL_DUPLEX,BL_POOL_VILLA,BL_MOTEL,BL_CAFE,
       BL_MARINA_SHED,BL_APARTMENT,BL_MARINA_TOWER,BUILDING_STYLE_COUNT };
enum { ITEM_PROP,ITEM_CAR,ITEM_COIN,ITEM_PLAYER,ITEM_BACK_PROP,ITEM_BUILDING };
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
static int row_z[WORLD_MAX_HEIGHT],row_scale[WORLD_MAX_HEIGHT],road_x[WORLD_MAX_HEIGHT],road_half[WORLD_MAX_HEIGHT];
static uint8_t row_biome[WORLD_MAX_HEIGHT];
static uint16_t row_feature[WORLD_MAX_HEIGHT];
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
 return index<DEPTH_CACHE_COUNT?depth_y[index]:HORIZON+scene_divide((PLAYER_Y-HORIZON)*CAMERA,z+CAMERA);
}

static int project_scale(int z)
{
 unsigned index=(unsigned)(z-DEPTH_CACHE_MIN);
 return index<DEPTH_CACHE_COUNT?depth_scale[index]:scene_divide(276*CAMERA,z+CAMERA);
}

static int item_x(const View *v,int world_x,int z)
{return 128+((world_x-v->center-v->cam_x)*project_scale(z))/256;}

static void set_projection(int height,int horizon,int player_y)
{
 static int initialized;
 if(initialized && world_height==height && HORIZON==horizon && PLAYER_Y==player_y)return;
 initialized=1;world_height=height;world_horizon=horizon;world_player_y=player_y;
 for(int i=0;i<DEPTH_CACHE_COUNT;i++) {
  int z=i+DEPTH_CACHE_MIN;
  depth_y[i]=(int16_t)(HORIZON+scene_divide((PLAYER_Y-HORIZON)*CAMERA,z+CAMERA));
  depth_scale[i]=(int16_t)(scene_divide(276*CAMERA,z+CAMERA));
 }
 for(int i=1;i<256;i++)vertical_step[i]=65536u/(unsigned)i;
 for(int y=0;y<world_height;y++) {
  if(y<=HORIZON){row_z[y]=FAR_DISTANCE;row_scale[y]=0;continue;}
  row_z[y]=clampi(((PLAYER_Y-y)*CAMERA)/(y-HORIZON),-44,FAR_DISTANCE);
  row_scale[y]=(276*(y-HORIZON))/(PLAYER_Y-HORIZON);
 }
}

static void init_projection(void){sky_origin=0;set_projection(VH,DRIVE_HORIZON,DRIVE_PLAYER_Y);}

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
 int whole=scene_divide(visual_position,scale);
 return whole*128+scene_divide((visual_position-whole*scale)*128,scale);
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
{return biome==CAUSEWAY || biome==WETLANDS ||
 ((biome==COAST || biome==MARINA || biome==SWAMP || biome==AURORA || biome==SALT) && side<0)?256:0;}

static uint16_t terrain_color(int biome)
{
 static const uint16_t colors[LOCATION_COUNT]={
  C(157,173,164),C(119,176,116),C(41,144,169),C(67,113,85),C(192,136,91),
  C(59,77,98),C(84,129,107),C(152,177,103),
  C(41,52,77),C(48,112,80),C(62,105,85),C(203,223,229),C(224,223,217)};
 return colors[biome%LOCATION_COUNT];
}

static uint16_t verge_color(int biome)
{
 static const uint16_t colors[LOCATION_COUNT]={
  C(221,212,187),C(230,215,166),C(202,216,208),C(149,156,119),C(222,170,118),
  C(106,132,145),C(163,177,137),C(193,180,127),
  C(78,105,131),C(119,145,94),C(126,147,112),C(229,237,230),C(236,229,226)};
 return colors[biome%LOCATION_COUNT];
}

static uint16_t sea_color(int biome)
{return biome==MARINA?C(27,86,123):biome==WETLANDS?C(57,118,118):
 biome==SWAMP?C(35,91,79):biome==AURORA?C(65,136,166):
 biome==SALT?C(185,177,211):C(35,151,180);}

static int bridge_region(int biome)
{return biome==CAUSEWAY || biome==WETLANDS;}

typedef struct {uint16_t sky,sky_low,floor,floor_low,pane,ink,muted,accent,rim;} StudioTheme;
static const StudioTheme studio_themes[SKIN_COUNT]={
 {C(17,35,61),C(41,62,82),C(49,78,99),C(27,54,77),C(35,66,89),C(236,250,247),C(145,190,201),C(105,233,220),C(77,124,144)},
 {C(233,229,218),C(208,217,216),C(255,252,241),C(234,236,225),C(239,239,223),C(28,57,61),C(74,99,100),C(17,110,108),C(151,165,161)},
 {C(17,8,39),C(41,17,62),C(36,17,67),C(18,12,43),C(29,15,55),C(245,235,255),C(182,158,218),C(86,238,245),C(124,64,175)}
};

static const StudioTheme *theme_for(const Game *g)
{return &studio_themes[clampi(g->save.skin-SKIN_FUSION,0,SKIN_COUNT-1)];}

typedef struct {uint16_t high,low,haze;uint8_t night;} SkyTheme;
static const SkyTheme sky_themes[LOCATION_COUNT]={
 {C(82,168,211),C(189,225,225),C(122,172,177),0},
 {C(70,171,219),C(201,236,226),C(120,184,182),0},
 {C(83,173,216),C(202,235,233),C(135,195,201),0},
 {C(100,171,194),C(204,222,202),C(103,145,127),0},
 {C(122,171,211),C(242,216,173),C(177,148,126),0},
 {C(16,28,57),C(47,80,113),C(42,74,104),255},
 {C(91,154,168),C(187,211,184),C(99,151,139),0},
 {C(102,181,214),C(220,235,194),C(141,177,132),0},
 {C(16,16,47),C(45,61,104),C(35,43,76),255},
 {C(63,137,156),C(166,204,163),C(66,129,102),0},
 {C(30,62,76),C(95,145,133),C(48,95,88),205},
 {C(17,35,73),C(88,148,180),C(111,166,190),220},
 {C(119,170,217),C(243,215,226),C(191,180,210),0}
};

static __attribute__((noinline)) void draw_sky(Surface *s,const View *v,const RoadSample *region)
{
 int mix=material_blend(region->blend),sky_height=HORIZON-sky_origin;
 const SkyTheme *a=&sky_themes[region->biome],*b=&sky_themes[region->next_biome];
 int night=(a->night*(256-mix)+b->night*mix)/256;
 for(int y=0;y<=HORIZON;y++) {
  int gradient=scene_divide(clampi(y-sky_origin,0,sky_height)*256,sky_height);
  uint16_t color=blend_color(a->high,a->low,gradient);
  if(mix)color=blend_color(color,blend_color(b->high,b->low,gradient),mix);
  gfx_rect(s,0,y,VW,1,color);
 }
 if(night>25)for(int n=0;n<22;n++) {
  uint32_t h=road_hash(v->seed^0x57a4u,(unsigned)n);
  int x=(int)(h%VW),y=sky_origin+1+(int)((h>>9)%(sky_height-3));
  int pulse=170+(int)((v->frame/32+h)%86);
  gfx_rect(s,x,y,1,1,blend_color(blend_color(a->high,b->high,mix),C(235,247,238),night*pulse/256));
 }
 uint16_t sky=blend_color(a->high,b->high,mix);
 uint16_t sun=blend_color(C(255,231,158),C(213,236,242),night);
 static const uint8_t body_x[LOCATION_COUNT]={42,46,190,62,198,192,46,64,203,43,54,195,51};
 static const uint8_t body_y[LOCATION_COUNT]={17,7,8,9,13,7,12,7,10,9,8,7,16};
 static const uint8_t body_radius[LOCATION_COUNT]={6,5,5,4,6,4,5,5,4,4,4,4,7};
 int celestial_x=(body_x[region->biome]*(256-mix)+body_x[region->next_biome]*mix)/256;
 int celestial_y=(body_y[region->biome]*(256-mix)+body_y[region->next_biome]*mix)/256;
 celestial_y=sky_origin+celestial_y*sky_height/DRIVE_HORIZON;
 int radius=(body_radius[region->biome]*(256-mix)+body_radius[region->next_biome]*mix)/256;
 gfx_circle(s,celestial_x,celestial_y,radius,sun);
 if(night>130)gfx_circle(s,celestial_x+2,celestial_y-2,radius,sky);
 int aura=(region->biome==AURORA)*(256-mix)+(region->next_biome==AURORA)*mix;
 if(aura)for(int x=0;x<VW;x+=8) {
  unsigned phase=((unsigned)x/8+v->frame/28)&31u;
  int wave=(int)(phase<16?phase:31-phase);
  gfx_rect(s,x,sky_origin+3+wave/4,8,1,blend_color(sky,C(99,231,202),aura*3/4));
  gfx_rect(s,x,sky_origin+5+(15-wave)/5,8,1,blend_color(sky,C(164,139,221),aura/2));
 }
 int biome=region->blend>=128?region->next_biome:region->biome;
 uint16_t distant=blend_color(a->haze,b->haze,mix);
 if(biome==CITY || biome==MARINA || biome==NEON) {
  for(int n=0;n<17;n++) {
   int x=n*17-4;if(x>108 && x<143)continue;
   unsigned h=road_hash(v->seed^0x6163u,(unsigned)n);
   int height=3+(int)(h&7);
   gfx_rect(s,x,HORIZON-height,10+(int)(h>>7&3),height+1,distant);
   if(night>100) {
    uint16_t light=biome==NEON?(h&1?C(103,222,225):C(211,124,218)):C(159,204,215);
    gfx_rect(s,x+3,HORIZON-height+2,2,1,light);
    if(height>5)gfx_rect(s,x+7,HORIZON-height+4,2,1,light);
   }
  }
 } else if(biome==PINES || biome==JUNGLE || biome==SWAMP || biome==GROVES || biome==WETLANDS) {
  for(int n=0;n<22;n++) {
   int x=n*13-8;if(x>112 && x<140)continue;
   unsigned h=road_hash(v->seed^0x7ee1u,(unsigned)n);
   int height=3+(int)(h&7),half=biome==JUNGLE?10:7;
   for(int y=0;y<height;y++)gfx_rect(s,x-half+y/3,HORIZON-height+y,half*2-y*2/3,1,distant);
  }
 } else if(biome==CANYON || biome==AURORA || biome==SALT) {
  for(int n=0;n<7;n++) {
   int x=n<3?12+n*28:164+(n-3)*29;
   int height=4+(int)(road_hash(v->seed,(unsigned)n)&7);
   for(int row=0;row<height;row++) {
    int half=(biome==CANYON?12:3)+row*(biome==SALT?1:2);
    gfx_rect(s,x-half,HORIZON-height+row,half*2,1,distant);
   }
  }
 } else {
  gfx_rect(s,0,HORIZON-2,102,3,distant);
  gfx_rect(s,148,HORIZON-2,108,3,distant);
 }
 if(night<150)for(int i=0;i<2;i++) {
  int x=(int)((v->frame/18+(unsigned)i*157)%320)-28,y=sky_origin+(3+i*5)*sky_height/DRIVE_HORIZON;
  uint16_t cloud=blend_color(sky,C(235,244,228),150);
  gfx_rect(s,x,y,22,2,cloud);gfx_rect(s,x+5,y-1,12,1,cloud);
 }
 if(world_height>VH && sky_height>35){
  uint16_t mist=blend_color(sky,C(227,239,235),night>130?45:145);
  for(int i=0;i<4;i++){
   int x=(int)((v->frame/(15u+i*3u)+(unsigned)i*93u)%340u)-35;
   int y=sky_origin+(20+i*19)*sky_height/128;
   ellipse(s,x+10,y+2,54+i*5,6,blend_color(mist,sky,85));
   ellipse(s,x,y,24,8,mist);ellipse(s,x+17,y-3,29,12,mist);ellipse(s,x+34,y,25,8,mist);
  }
  uint16_t bird=blend_color(sky,C(43,74,89),180);
  for(int i=0;i<5;i++){
   int x=(int)((v->frame/5u+(unsigned)i*13u)%330u)-30;
   int y=sky_origin+(80+(i%3)*6)*sky_height/128,wing=((v->frame/8u+i)&1u)?-2:1;
   gfx_line(s,x-3,y+wing,x,y,bird);gfx_line(s,x,y,x+3,y+wing,bird);
  }
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

static inline void road_ground_spans(Surface *s,int y,int outside_left,int outside_right,
 int left_water_span,int right_water_span,uint16_t color)
{
 /* Water overwrites the screen edges below. Only fill terrain which remains
  * visible between those spans; preserve the complete later overlay order. */
 int left=left_water_span,right=VW-right_water_span;
 if(left>=right)return;
 int end=outside_left<right?outside_left:right;
 int start=outside_right>left?outside_right:left;
 road_span(s,left,y,end-left,color);
 road_span(s,start,y,right-start,color);
}

/* Each scanline is a handful of halfword-safe spans, rather than a textured
 * software triangle.  The art around those spans supplies the height. */
/* Keep the ordinary driving loop specialized for 192 rows. The tall opening
 * uses the same source in a separate ARM9-RAM function, leaving ITCM for play. */
#define ROAD_FUNCTION draw_road_standard
#define ROAD_SPEED FAST
#define ROAD_HEIGHT VH
#define ROAD_HORIZON DRIVE_HORIZON
#define ROAD_KNOTS 25
#include "road_render.inc"
#undef ROAD_FUNCTION
#undef ROAD_SPEED
#undef ROAD_HEIGHT
#undef ROAD_HORIZON
#undef ROAD_KNOTS
#define ROAD_FUNCTION draw_road_cinematic
#define ROAD_SPEED
#define ROAD_HEIGHT world_height
#define ROAD_HORIZON world_horizon
#define ROAD_KNOTS 49
#include "road_render.inc"
#undef ROAD_FUNCTION
#undef ROAD_SPEED
#undef ROAD_HEIGHT
#undef ROAD_HORIZON
#undef ROAD_KNOTS
static void draw_road(Surface*s,const Game*g,const View*v){
 if(world_height==VH)draw_road_standard(s,g,v);else draw_road_cinematic(s,g,v);
}

static void add_item(DrawItem *items,int *count,int z,int x,int kind,int id,int variant,int side)
{
 if(*count>=DRAW_MAX || z < ((kind==ITEM_PROP || kind==ITEM_BACK_PROP || kind==ITEM_BUILDING)?-140:-44) || z>ROAD_VIEW)return;
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

static int prop_world_width(int id)
{
 static const uint8_t widths[42]={62,62,62,54,60,58,42,48,36,64,64,78,62,54,62,44,62,120,
  30,44,36,58,62,54,42,88,40,58,65,48,44,48,68,57,50,40,58,52,64,52,36,34};
 return (unsigned)id<42?widths[id]:48;
}

static void place_scenery(DrawItem *items,int *count,const View *v,int absolute,
 int side,int offset,int kind,uint32_t hash,int back)
{
 int z=absolute-v->visual_position;
 if(z < -44 || z>ROAD_VIEW)return;
 int water;int biome=scenery_material(v,absolute,side,hash,&water);
 int floating=kind==SC_SAIL || kind==SC_PIER;
 int wet=kind==SC_MANGROVE || kind==SC_REEDS || kind==SC_CYPRESS || kind==SC_RUSH ||
         kind==SC_DEADWOOD || kind==SC_FLAMINGO || (kind==SC_CRYSTAL && biome==SALT);
 int bridge=kind==SC_LAMP && bridge_region(biome);
 if((floating && water!=256) || (!floating && !wet && !bridge && water))return;
 RoadSample road;road_sample(v->seed,v->position+z,v->start,&road);
 int world_x=road.center+side*offset;
 int x=item_x(v,world_x,z),half=prop_world_width(kind)*project_scale(z)/512+2;
 if(x+half<0 || x-half>=VW)return;
 add_item(items,count,z,world_x,back?ITEM_BACK_PROP:ITEM_PROP,kind,(int)hash,side);
}

static void place_building(DrawItem *items,int *count,const View *v,int absolute,
 int side,int offset,int style,uint32_t hash)
{
 int z=absolute-v->visual_position;
 if(z < -140 || z>ROAD_VIEW || style<0)return;
 int water;scenery_material(v,absolute,side,hash,&water);if(water)return;
 RoadSample road;road_sample(v->seed,v->position+z,v->start,&road);
 int inner=road.center+side*offset,outer=inner+side*100;
 int near=z < -44?-44:z,far=z+140;
 int a=item_x(v,inner,near),b=item_x(v,outer,near),c=item_x(v,inner,far),d=item_x(v,outer,far);
 int low=a,high=a;
 if(b<low)low=b;
 if(b>high)high=b;
 if(c<low)low=c;
 if(c>high)high=c;
 if(d<low)low=d;
 if(d>high)high=d;
 if(high<0 || low>=VW)return;
 add_item(items,count,z,inner,ITEM_BUILDING,style,(int)hash,side);
}

static int district_building(int biome,uint32_t h,int distant)
{
 switch(biome) {
 case CITY: {
  static const uint8_t near[6]={BL_HOUSE,BL_SHOP,BL_DUPLEX,BL_WAREHOUSE,BL_APARTMENT,BL_CAFE};
  static const uint8_t far[4]={BL_DUPLEX,BL_APARTMENT,BL_HOUSE,BL_MOTEL};
  return distant?far[h%4]:near[h%6];
 }
 case COAST: {
  static const uint8_t types[4]={BL_VILLA,BL_POOL_VILLA,BL_MOTEL,BL_CAFE};return types[h%4];
 }
 case PINES:return h&1?BL_JUNGLE_LODGE:BL_CHALET;
 case CANYON:return h&1?BL_PAVILION:BL_WAREHOUSE;
 case MARINA: {
  static const uint8_t types[4]={BL_HOTEL,BL_MARINA_SHED,BL_MARINA_TOWER,BL_CAFE};return types[h%4];
 }
 case GROVES:return distant?BL_DUPLEX:(h&1?BL_WAREHOUSE:BL_CAFE);
 case NEON: {
  static const uint8_t near[5]={BL_ARCADE,BL_NOODLES,BL_CLUB,BL_ARCADE,BL_NOODLES};
  static const uint8_t far[4]={BL_OFFICE,BL_CLUB,BL_APARTMENT,BL_ARCADE};
  return distant?far[h%4]:near[h%5];
 }
 case JUNGLE:return h&1?BL_JUNGLE_LODGE:BL_JUNGLE_HUT;
 case SWAMP:return h&1?BL_BAYOU_CABIN:BL_BOATHOUSE;
 case AURORA:return distant?(h&1?BL_OBSERVATORY:BL_SNOW_LODGE):BL_CHALET;
 case SALT:return h&1?BL_PAVILION:BL_SOLAR_STATION;
 default:return -1;
 }
}

static int district_lot(int biome,uint32_t h,int side,int distant)
{
 switch(biome) {
 case CITY:return distant?(h&1)!=0:1;
 case NEON:return distant?(h&1)!=0:(h&3)!=0;
 case MARINA:return side>0 && (distant?(h&3)!=0:(h&3)!=0);
 case COAST:return side>0 && (distant?(h&3)==0:(h&7)==0);
 case GROVES:return (h&15)==0;
 case PINES:case CANYON:return (h&31)==0;
 case JUNGLE:return distant?(h&7)==0:(h&15)==0;
 case SWAMP:return side>0 && (distant?(h&3)==0:(h&7)==0);
 case AURORA:return side>0 && (distant?(h&3)==0:(h&7)==0);
 case SALT:return side>0 && (distant?(h&7)==0:(h&15)==0);
 default:return 0;
 }
}

static int district_plant(int biome,uint32_t h,int side)
{
 switch(biome) {
 case CITY:return h&1?SC_TREE:SC_PALM;
 case COAST:return side<0?-1:(h&3?SC_PALM:SC_BANANA);
 case CAUSEWAY:return -1;
 case PINES: {static const uint8_t kinds[5]={SC_PINE,SC_SPRUCE,SC_TREE,SC_BIRCH,SC_ROCK};return kinds[h%5];}
 case CANYON:return h&3?SC_ROCK:SC_SCRUB;
 case MARINA:return side<0?-1:(h&1?SC_PALM:SC_BUSH);
 case WETLANDS: {static const uint8_t kinds[4]={SC_REEDS,SC_MANGROVE,SC_CYPRESS,SC_RUSH};return kinds[h%4];}
 case GROVES:return h&7?SC_CITRUS:SC_BUSH;
 case NEON:return side>0?(h&1?SC_NEON_PALM:SC_SCULPTURE):SC_NEON_PALM;
 case JUNGLE: {static const uint8_t kinds[7]={SC_CANOPY,SC_BANANA,SC_BAMBOO,SC_FERN,SC_FLOWER,SC_VINE,SC_PALM};return kinds[h%7];}
 case SWAMP: {static const uint8_t kinds[6]={SC_CYPRESS,SC_DEADWOOD,SC_RUSH,SC_MANGROVE,SC_LOG,SC_REEDS};return kinds[h%6];}
 case AURORA: {static const uint8_t kinds[5]={SC_SPRUCE,SC_BIRCH,SC_SNOW_ROCK,SC_SNOW_BUSH,SC_SNOW_BANK};return side<0?-1:kinds[h%5];}
 case SALT: {static const uint8_t kinds[5]={SC_CRYSTAL,SC_SALT_MOUND,SC_SCRUB,SC_SOLAR,SC_FLAMINGO};return side<0?(h&1?SC_CRYSTAL:SC_FLAMINGO):kinds[h%5];}
 default:return -1;
 }
}

static int dock_allowed(int biome,uint32_t hash,int side)
{
 if(side>=0)return 0;
 if(biome==MARINA)return (hash&3)!=0;
 if(biome==COAST)return (hash&3)==0;
 if(biome==SWAMP)return (hash&7)==0;
 if(biome==AURORA)return (hash&15)==0;
 return 0;
}

static __attribute__((noinline)) void queue_scenery(DrawItem *items,int *count,const View *v)
{
 int first=floor_div(v->visual_position-160,256),last=floor_div(v->visual_position+ROAD_VIEW,256);
 for(int n=first;n<=last;n++)for(int side=-1;side<=1;side+=2) {
  uint32_t h=road_landmark(v->seed^0x4181u,(uint32_t)n,side);
  uint32_t block=road_landmark(v->seed^0x20b1u,(uint32_t)floor_div(n,4),side);
  int absolute=n*256+12+(int)((block>>9)%13),water;
  int biome=scenery_material(v,absolute,side,h,&water);
  if(district_lot(biome,h,side,0))place_building(items,count,v,absolute,side,
     ROAD_HALF+33+(int)((block>>12)&7),district_building(biome,h,0),h);
 }
 /* A second two-dimensional lot grid occupies neighborhoods and clearings,
  * rather than placing every object in a single band beside the shoulder. */
 first=floor_div(v->visual_position-160,320);last=floor_div(v->visual_position+ROAD_VIEW,320);
 for(int n=first;n<=last;n++)for(int side=-1;side<=1;side+=2)for(int column=0;column<2;column++) {
  uint32_t h=road_landmark(v->seed^0x621du^(unsigned)column*0x815u,(uint32_t)n,side);
  int absolute=n*320+34+column*77+(int)((h>>9)%37),water;
  int biome=scenery_material(v,absolute,side,h,&water);
  if(district_lot(biome,h,side,1))place_building(items,count,v,absolute,side,
     242+column*166+(int)((h>>15)%37),district_building(biome,h>>4,1),h);
 }
 first=floor_div(v->visual_position-44,224);last=floor_div(v->visual_position+ROAD_VIEW,224);
 for(int n=first;n<=last;n++)for(int side=-1;side<=1;side+=2) {
  uint32_t h=road_landmark(v->seed^0x1a4bu,(uint32_t)n,side);
  int absolute=n*224,water,biome=scenery_material(v,absolute,side,h,&water);
  if(biome==CITY || biome==CAUSEWAY || biome==NEON || (biome==MARINA && side>0))
   place_scenery(items,count,v,absolute,side,ROAD_HALF+11,biome==NEON?SC_UTILITY:SC_LAMP,h,0);
  if((biome==NEON || biome==CITY || biome==MARINA) && side>0 && (h&3)==0)
   place_scenery(items,count,v,absolute+44,side,178+(int)((h>>15)%40),biome==NEON?SC_VENDING:SC_SCULPTURE,h^0x83a5u,0);
 }
 /* Clusters vary species, setback and gaps. The outer two members provide
  * real midground foliage at the corners, with intentional open clearings. */
 first=floor_div(v->visual_position-180,240);last=floor_div(v->visual_position+ROAD_VIEW,240);
 for(int n=first;n<=last;n++)for(int side=-1;side<=1;side+=2) {
  uint32_t h=road_landmark(v->seed^0x7ee5u,(uint32_t)n,side);
  int base=n*240+(int)((h>>8)%43),water,biome=scenery_material(v,base,side,h,&water);
  int number=biome==JUNGLE?4:biome==PINES || biome==GROVES || biome==SWAMP || biome==WETLANDS?3:
             biome==COAST || biome==AURORA || biome==SALT?2:1;
  if((biome==CITY || biome==MARINA || biome==NEON) && (h&3)!=0)continue;
  if((biome==JUNGLE || biome==PINES) && (h&31)==0)continue;
  for(int j=0;j<number;j++) {
   uint32_t plant=road_hash(h,(uint32_t)j);
   int kind=district_plant(biome,plant,side);if(kind<0)continue;
   int absolute=base+j*34+(int)((plant>>10)%17);
   int offset=118+(int)((plant>>3)%49);
   if(j>=2)offset=214+(j-2)*92+(int)((plant>>14)%48);
   if(biome==CITY || biome==MARINA || biome==NEON)offset=181+(int)(plant%58);
   if(biome==WETLANDS || biome==SWAMP)offset=(kind==SC_REEDS || kind==SC_RUSH)?110+(int)(plant%26):137+(int)(plant%96);
   if(biome==SALT && side<0)offset=170+(int)(plant%121);
   place_scenery(items,count,v,absolute,side,offset,kind,plant,j>=2);
  }
 }
 /* Rare shore-connected docks keep their independent exclusion rhythm. */
 first=floor_div(v->visual_position-44,520);last=floor_div(v->visual_position+ROAD_VIEW,520);
 for(int n=first;n<=last;n++)for(int side=-1;side<=1;side+=2) {
  uint32_t h=road_landmark(v->seed^0xd0c4u,(uint32_t)n,side);
  int absolute=n*520+52,water,biome=scenery_material(v,absolute,side,h,&water);
  if(dock_allowed(biome,h,side))place_scenery(items,count,v,absolute,side,ROAD_HALF+7,SC_PIER,h,0);
 }
 first=floor_div(v->visual_position-44,408);last=floor_div(v->visual_position+ROAD_VIEW,408);
 for(int n=first;n<=last;n++)for(int side=-1;side<=1;side+=2) {
  uint32_t h=road_landmark(v->seed^0xb0a7u,(uint32_t)n,side);
  int absolute=n*408+184+(int)((h>>10)%53),water,biome=scenery_material(v,absolute,side,h,&water);
  int boats=biome==MARINA || biome==COAST || biome==SWAMP || (biome==AURORA && (h&7)==0);
  if(boats && side<0 && (h&3)!=0) {
   int dock_n=floor_div(absolute-52+260,520),dock_absolute=dock_n*520+52;
   uint32_t dock_hash=road_landmark(v->seed^0xd0c4u,(uint32_t)dock_n,side);
   int dock_water,dock_biome=scenery_material(v,dock_absolute,side,dock_hash,&dock_water);
   int separation=absolute-dock_absolute;if(separation<0)separation=-separation;
   if(separation<58 && dock_water==256 && dock_allowed(dock_biome,dock_hash,side))continue;
   place_scenery(items,count,v,absolute,side,190+(int)((h>>5)%611),SC_SAIL,h,0);
  }
 }
}

/* Organic scenery uses upright, root-anchored art. Buildings are projected
 * solids; broadside boats rise above water and docks follow its plane. */
static const uint8_t *prop_source(int id,int side,int projected_width,
 int *source_width,int *source_height,const uint8_t **anchor,const uint8_t **bounds,const uint16_t **palette)
{
 int variant=side>0 && prop_side_slot[id]!=255?prop_side_slot[id]:-1;
 int slot=prop_slot[id];
 const uint8_t *pixels;
 int width=projected_width>=40?64:projected_width>=27?48:projected_width>=13?32:16;
 int height=width*PROP_H/PROP_W;
 if(variant>=0) {
  pixels=width==64?prop_side_idx:width==48?prop_side_lod48_idx:
         width==32?prop_side_lod32_idx:prop_side_lod16_idx;
  pixels+=variant*width*height;
  *palette=prop_palette+slot*256;
  *anchor=prop_side_anchor+variant*2;
  *bounds=(width==64?prop_side_bounds:width==48?prop_side_lod48_bounds:
           width==32?prop_side_lod32_bounds:prop_side_lod16_bounds)+variant*4;
 } else {
  pixels=width==64?prop_idx:width==48?prop_lod48_idx:width==32?prop_lod32_idx:prop_lod16_idx;
  pixels+=slot*width*height;
  *palette=prop_palette+slot*256;
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
 int top=clampi(project_y(far),HORIZON+1,world_height-1);
 int bottom=clampi(project_y(near < -44?-44:near),HORIZON+1,world_height-1);
 for(int y=top;y<=bottom;y++) {
  int z=row_z[y];if(z<near || z>far)continue;
  int scale=row_scale[y],w=width*scale/256;
  if(w<1)continue;
  int left=128+((world_x-v->center-v->cam_x)*scale)/256-w/2;
  int begin=left<0?-left:0,end=left+w>VW?VW-left:w;
  if(end<=begin)continue;
  uint16_t *dest=s->pixels+y*s->w+left+begin;
  if(!pixels) {
   road_span(s,left+begin,y,end-begin,color);
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

static FAST __attribute__((noinline,noclone)) void scenery_polygon(Surface *s,const SceneryPoint *points,int count,uint16_t color)
{
 int left[WORLD_MAX_HEIGHT],right[WORLD_MAX_HEIGHT];
 int first=world_height,last=0;
 for(int i=0;i<count;i++) {
  if(points[i].y<first)first=points[i].y;
  if(points[i].y>last)last=points[i].y;
 }
 first=clampi(first,0,world_height);last=clampi(last,0,world_height);
 if(last<=first)return;
 for(int y=first;y<last;y++){left[y]=VW;right[y]=-1;}
 for(int edge=0;edge<count;edge++) {
  SceneryPoint a=points[edge],b=points[(edge+1)%count];
  if(a.y>b.y){SceneryPoint swap=a;a=b;b=swap;}
  if(a.y==b.y)continue;
  int y0=clampi(a.y,0,world_height),y1=clampi(b.y,0,world_height);
  if(y1<=y0)continue;
  int step=scene_divide((b.x-a.x)*65536,b.y-a.y);
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
{return (p.x<0?1:p.x>=VW?2:0)|(p.y<0?4:p.y>=world_height?8:0);}

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
  if(code&8){p.y=world_height-1;p.x=a.x+scene_divide((b.x-a.x)*(p.y-a.y),b.y-a.y);}
  else if(code&4){p.y=0;p.x=a.x+scene_divide((b.x-a.x)*(p.y-a.y),b.y-a.y);}
  else if(code&2){p.x=VW-1;p.y=a.y+scene_divide((b.y-a.y)*(p.x-a.x),b.x-a.x);}
  else {p.x=0;p.y=a.y+scene_divide((b.y-a.y)*(p.x-a.x),b.x-a.x);}
  if(code==ca){a=p;ca=scenery_outcode(a);}else {b=p;cb=scenery_outcode(b);}
 }
}

/* One perspective division per visible screen column, followed by a compact
 * row-major raster.  There is no matrix stack, z buffer, or per-pixel divide.
 * Perspective-correct horizontal texture coordinates keep window rows and
 * vertical window edges attached to the actual wall plane. */
/* The cartridge stores indexed faces, while this bounded working set keeps
 * the hot wall loop to one texture read. Small distant faces are reused by
 * every district; eight full-resolution nearby faces share an LRU. */
typedef struct {
 const uint8_t *source;const uint16_t *palette;
 unsigned stamp;int width,height;
 uint16_t *pixels;
} WallTexture;
/* Variable capacity pools spend memory on the visible working set rather
 * than permanently reserving both distant LODs for all 25 districts. */
static uint16_t wall_near_pixels[8][64*64];
static uint16_t wall_mid_pixels[12][32*32];
static uint16_t wall_far_pixels[12][16*16];
static WallTexture wall_textures[32];
static unsigned wall_texture_clock;
static int wall_textures_ready;
volatile unsigned wall_cache_stats[3];

static __attribute__((noinline)) const uint16_t *wall_texture_rgb(
 const uint8_t *source,const uint16_t *palette,int width,int height)
{
 if(!wall_textures_ready) {
  for(int i=0;i<8;i++)wall_textures[i].pixels=wall_near_pixels[i];
  for(int i=0;i<12;i++)wall_textures[i+8].pixels=wall_mid_pixels[i];
  for(int i=0;i<12;i++)wall_textures[i+20].pixels=wall_far_pixels[i];
  wall_textures_ready=1;
 }
 int count=width*height,first=count<=256?20:count<=1024?8:0;
 int last=first==0?8:first==8?20:32;
 WallTexture *entry=&wall_textures[first];++wall_texture_clock;
 for(int i=first;i<last;i++) {
  WallTexture *candidate=&wall_textures[i];
  if(candidate->source==source && candidate->palette==palette &&
     candidate->width==width && candidate->height==height) {
   candidate->stamp=wall_texture_clock;wall_cache_stats[0]++;return candidate->pixels;
  }
  if(candidate->stamp<entry->stamp)entry=candidate;
 }
 wall_cache_stats[first==0?2:1]++;
 for(int i=0;i<count;i++)entry->pixels[i]=palette[source[i]];
 entry->source=source;entry->palette=palette;entry->width=width;entry->height=height;
 entry->stamp=wall_texture_clock;return entry->pixels;
}

static FAST __attribute__((noinline)) void building_wall(Surface *s,const View *v,
 int ax,int az,int bx,int bz,int height,const uint8_t *texture,const uint16_t *palette,int tw,int th)
{
 if(az < -44 && bz < -44)return;
 int ua=0,ub=tw-1;
 if(az < -44) {ua=scene_divide((-44-az)*(tw-1),bz-az);az=-44;}
 if(bz < -44) {ub=scene_divide((az+44)*(tw-1),az-bz);bz=-44;}
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
 const uint16_t *rgb=wall_texture_rgb(texture,palette,tw,th);
 typedef struct {int16_t top,bottom;uint16_t u;uint32_t step,sample;} WallColumn;
 WallColumn columns[VW];
 int top_step=scene_divide((bt.y-at.y)*65536,span),bottom_step=scene_divide((b.y-a.y)*65536,span);
 int scale_step=scene_divide((sb-sa)*65536,span);
 int u_step=scene_divide((ub*sb-ua*sa)*65536,span);
 int offset=x0-a.x;
 int top=at.y*65536+top_step*offset,bottom=a.y*65536+bottom_step*offset;
 int scale=sa*65536+scale_step*offset,u=ua*sa*65536+u_step*offset;
 int first=world_height,last=0;
 for(int x=x0;x<x1;x++,top+=top_step,bottom+=bottom_step,scale+=scale_step,u+=u_step) {
  int t=top/65536,base=bottom/65536;
  if(base<=t || scale<=0){columns[x]=(WallColumn){0,0,0,0,0};continue;}
  int tex=clampi(scene_divide(u,scale),0,tw-1);
  int y0=clampi(t,0,world_height),y1=clampi(base,0,world_height);
  unsigned step=(unsigned)scene_divide(th*65536,base-t);
  columns[x]=(WallColumn){(int16_t)t,(int16_t)base,(uint16_t)tex,step,(unsigned)(y0-t)*step};
  if(y0<first)first=y0;
  if(y1>last)last=y1;
 }
 int texture_shift=tw==64?6:tw==32?5:4;
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
  int x=left;
  if((x&1) && x<right) {
   WallColumn *c=&columns[x++];
   *dest++=rgb[((c->sample>>16)<<texture_shift)+c->u];c->sample+=c->step;
  }
  for(;x+1<right;x+=2,dest+=2) {
   WallColumn *a=&columns[x],*b=&columns[x+1];
   uint32_t p=rgb[((a->sample>>16)<<texture_shift)+a->u];
   p|=(uint32_t)rgb[((b->sample>>16)<<texture_shift)+b->u]<<16;
   a->sample+=a->step;b->sample+=b->step;
   *(uint32_t *)dest=p;
  }
  if(x<right) {
   WallColumn *c=&columns[x];
   *dest=rgb[((c->sample>>16)<<texture_shift)+c->u];c->sample+=c->step;
  }
 }
}

static int building_style(int id)
{return id==SC_SHOP?1:id==SC_WAREHOUSE?2:id==SC_HOTEL?3:id==SC_VILLA?4:0;}

static const uint8_t *building_end_texture(int style,int width)
{
 int height=width*2;
 return (width==32?building_end_idx:width==16?building_end_lod16_idx:building_end_lod8_idx)+style*width*height;
}

static void building_end_wall(Surface *s,const View *v,int inner,int outer,int z,
 int height,int style)
{
 if(z < -44)return;
 SceneryPoint a=scenery_point(v,inner,z,0),b=scenery_point(v,outer,z,height);
 int left=a.x<b.x?a.x:b.x,width=b.x-a.x;
 if(width<0)width=-width;
 int h=a.y-b.y;
 if(width<1 || h<1 || left>=VW || left+width<0 || b.y>=world_height || a.y<0)return;
 /* Constant-z faces are opaque rectangles. They deliberately bypass the
  * shared car/foliage cache: a newly resized wall must not evict traffic. */
 width++;
 int x0=clampi(left,0,VW),x1=clampi(left+width,0,VW);
 int y0=clampi(b.y,0,world_height),y1=clampi(a.y,0,world_height);
 if(x1<=x0 || y1<=y0)return;
 int source_width=width<=8 && h<=16?8:width<=16 && h<=32?16:32;
 int source_height=source_width*2;
 const uint8_t *texture=building_end_texture(style,source_width);
 const uint16_t *palette=building_palette+style*256;
 uint8_t source_x[VW];
 unsigned x_step=(unsigned)scene_divide(source_width*65536+width-1,width);
 unsigned y_step=(unsigned)scene_divide(source_height*65536+h-1,h);
 for(int x=x0;x<x1;x++) {
  unsigned sample=((unsigned)(x-left)*x_step)>>16;
  source_x[x]=(uint8_t)(a.x>b.x?(unsigned)source_width-1u-sample:sample);
 }
 unsigned sample=(unsigned)(y0-b.y)*y_step;
 for(int y=y0;y<y1;y++,sample+=y_step) {
  const uint8_t *row=texture+(sample>>16)*(unsigned)source_width;
  uint16_t *dest=s->pixels+y*VW+x0;
  int x=x0;
  if((uintptr_t)dest&2) {
   *dest++=palette[row[source_x[x++]]];
  }
  for(;x+1<x1;x+=2,dest+=2) {
   uint32_t pair=palette[row[source_x[x]]];
   pair|=(uint32_t)palette[row[source_x[x+1]]]<<16;
   *(uint32_t *)dest=pair;
  }
  if(x<x1)*dest=palette[row[source_x[x]]];
 }
}

static FAST __attribute__((noinline)) void draw_building(Surface *s,const View *v,const DrawItem *item)
{
 static const uint8_t widths[BUILDING_STYLE_COUNT]={54,62,68,58,60,65,50,75,70,60,42,52,60,58,80,75,60,82,60,68,96,55,70,64,68};
 static const uint8_t depths[BUILDING_STYLE_COUNT]={88,92,104,92,94,98,80,112,110,92,72,86,96,90,110,124,88,112,94,104,132,78,110,112,124};
 static const uint8_t heights[BUILDING_STYLE_COUNT]={48,43,47,76,52,50,42,100,64,46,35,43,44,55,64,68,33,40,54,51,44,35,48,104,120};
 static const uint8_t pitches[BUILDING_STYLE_COUNT]={18,12,8,10,20,8,14,0,4,20,24,16,12,24,20,22,8,7,18,16,10,12,8,4,3};
 int style=item->kind==ITEM_BUILDING?item->id:building_style(item->id);
 int width=widths[style],depth=depths[style];
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
 int projected_width=inf.x-ib.x;if(projected_width<0)projected_width=-projected_width;
 int projected_height=height*project_scale(near < -44?-44:near)/256;
 const uint16_t *palette=building_palette+style*256;
 int distant=near>360;
 if(!distant) {
  uint16_t paving=C(184,190,174);
  ground_plane(s,v,inner+side*width/2,near+depth/2,width+6,depth+7,0,0,0,paving);
 }
 int resolution=projected_width<=16 && projected_height<=16?16:
                projected_width<=32 && projected_height<=32?32:64;
 const uint8_t *wall=(resolution==64?building_wall_idx:
                       resolution==32?building_wall_lod32_idx:building_wall_lod16_idx)+style*resolution*resolution;
 building_wall(s,v,inner,near,inner,far,height,wall,palette,resolution,resolution);
 building_end_wall(s,v,inner,outer,near,height,style);
 scenery_work[6]+=(profile_clock()-wall_start)&65535u;
 unsigned roof_start=profile_clock();
 SceneryPoint in=scenery_point(v,inner,near,height),iff=scenery_point(v,inner,far,height);
 SceneryPoint out=scenery_point(v,outer,near,height),of=scenery_point(v,outer,far,height);
 SceneryPoint rn=scenery_point(v,ridge,near,height+pitch),rf=scenery_point(v,ridge,far,height+pitch);
 const uint8_t *roof=building_roof_idx+style*32*32;
 uint16_t roof_color=palette[roof[16*32+16]],roof_dark=blend_color(roof_color,C(26,49,53),36);
 uint16_t seam=blend_color(palette[roof[8*32+8]],C(39,53,57),64);
 SceneryPoint outer_plane[4]={out,of,rf,rn};
 SceneryPoint inner_plane[4]={in,iff,rf,rn};
 int camera_delta=ridge-v->center-v->cam_x;if(camera_delta<0)camera_delta=-camera_delta;
 int outer_visible=(130-height-pitch)*(width/2)>pitch*camera_delta;
 if(outer_visible)scenery_polygon(s,outer_plane,4,roof_dark);
 scenery_polygon(s,inner_plane,4,roof_color);
 /* Tile seams are world-space lines on the pitched roof.  Longitudinal
  * seams share the road vanishing point; cross seams follow the roof slope. */
 if(!distant && projected_height>16)for(int u=1;u<4;u++) {
  int x=inner+side*(width*u/8),y=height+pitch*u/4;
  scenery_edge(s,scenery_point(v,x,near,y),scenery_point(v,x,far,y),seam);
 }
 if(!distant && projected_height>16)for(int z=near+18;z<far;z+=18) {
  scenery_edge(s,scenery_point(v,inner,z,height),scenery_point(v,ridge,z,height+pitch),seam);
  if(outer_visible)scenery_edge(s,scenery_point(v,ridge,z,height+pitch),scenery_point(v,outer,z,height),seam);
 }
 if(near>=-44) {
  SceneryPoint gable[3]={in,out,rn};
  uint16_t wall=palette[building_end_idx[style*32*64+4*32+16]];
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
  int boat=(int)((unsigned)item->variant>>14)%4;
  int biome=row_biome[clampi(y,HORIZON+1,world_height-1)];
  if(biome==SWAMP)boat=2+boat%2;
  if(biome==AURORA)boat=3;
  static const uint8_t widths[4]={78,74,48,65};
  int w=widths[boat]*scale/256,h=w*48/64;
  if(w<3 || h<3)return;
  uint16_t sea=sea_color(row_biome[clampi(y,HORIZON+1,world_height-1)]);
  ellipse(s,x,y+1,w+3,clampi(w/9,1,4),blend_color(sea,C(193,233,225),85));
  gfx_indexed_sprite(s,x-boat_profile_anchor[boat*2]*w/64,y-boat_profile_anchor[boat*2+1]*h/48,
                    boat_profile_idx+boat*64*48,boat_palette+boat*256,64,48,w,h);
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
 if(item->kind==ITEM_BUILDING || building_id(item->id)){draw_building(s,v,item);return;}
 if(item->id==SC_SAIL || item->id==SC_PIER){draw_water_prop(s,v,item);return;}
 if(item->z < -44)return;
 int y=project_y(item->z),x=item_x(v,item->x,item->z);
 int scale=project_scale(item->z),world_width=prop_world_width(item->id);
 if(item->kind==ITEM_BACK_PROP)world_width=world_width*218/256;
 int w=world_width*scale/256,h=w*PROP_H/PROP_W;
 if(w<2 || h<3)return;
 int source_width,source_height;
 const uint8_t *anchor,*bounds;
 const uint16_t *palette;
 const uint8_t *pixels=prop_source(item->id,item->side,w,&source_width,&source_height,&anchor,&bounds,&palette);
 (void)bounds;
 int contact_width=item->id==SC_LAMP?clampi(w/8,1,4):clampi(w/5,2,8);
 ellipse(s,x,y,contact_width,clampi(contact_width/3,1,3),C(53,81,73));
 int sway=(item->id==SC_REEDS || item->id==SC_BUSH)?
             (int)((v->frame/14+(unsigned)item->variant)&1):0;
 gfx_indexed_sprite(s,x-anchor[0]*w/PROP_W+sway,y-anchor[1]*h/PROP_H,
                   pixels,palette,source_width,source_height,w,h);
}

static __attribute__((noinline)) void draw_far_landscape(Surface *s,const View *v)
{
 int first=floor_div(v->visual_position+ROAD_VIEW,320);
 int last=floor_div(v->visual_position+FAR_DISTANCE,320);
 for(int n=last;n>=first;n--)for(int side=-1;side<=1;side+=2)for(int column=2;column>=0;column--) {
  uint32_t h=road_landmark(v->seed^0xfa418u^(unsigned)column*0x751u,(uint32_t)n,side);
  int absolute=n*320+41+(int)((h>>9)%67),z=absolute-v->visual_position;
  if(z<ROAD_VIEW || z>FAR_DISTANCE || (h&15)==0)continue;
  int water,biome=scenery_material(v,absolute,side,h,&water);
  int urban=biome==CITY || biome==MARINA || biome==NEON;
  if(!urban && !(biome==AURORA && side<0))continue;
  RoadSample road;road_sample(v->seed,v->position+z,v->start,&road);
  int world_x=road.center+side*(410+column*282+(int)((h>>15)%72));
  int scale=project_scale(z),x=item_x(v,world_x,z),ground=project_y(z);
  int width=(55+(int)(h%63))*scale/256;
  if(width<2 || x+width<0 || x-width>=VW)continue;
  uint16_t haze=sky_themes[biome].haze;
  if(urban && !water) {
   int style=district_building(biome,h,1);
   int height=(58+(int)((h>>7)%115))*scale/256;
   const uint16_t *palette=building_palette+style*256;
   uint16_t base=palette[building_wall_idx[style*64*64+4*64+4]];
   uint16_t face=blend_color(base,haze,150),end=blend_color(face,C(40,67,83),35);
   int outer=world_x+side*(70+(int)(h%34)),back=z+90;
   SceneryPoint a=scenery_point(v,world_x,z,0),b=scenery_point(v,world_x,back,0);
   SceneryPoint at=scenery_point(v,world_x,z,58+(int)((h>>7)%115));
   SceneryPoint bt=scenery_point(v,world_x,back,58+(int)((h>>7)%115));
   SceneryPoint plane[4]={a,b,bt,at};scenery_polygon(s,plane,4,face);
   int edge=item_x(v,outer,z),left=x<edge?x:edge,w=x<edge?edge-x:x-edge;
   gfx_rect(s,left,ground-height,w+1,height,end);
   if(height>=6) {
    uint16_t light=biome==NEON?(h&1?C(100,203,212):C(204,115,209)):C(175,194,181);
    gfx_rect(s,left+1,ground-height+2,clampi(w-1,1,10),1,light);
    if(height>=10)gfx_rect(s,left+1,ground-height+5,clampi(w-1,1,10),1,light);
   }
  } else if(biome==AURORA && side<0) {
   SceneryPoint berg[4]={scenery_point(v,world_x-65,z,0),scenery_point(v,world_x-30,z+45,27),
     scenery_point(v,world_x+24,z+60,45),scenery_point(v,world_x+70,z,0)};
   scenery_polygon(s,berg,4,C(174,212,225));
   gfx_rect(s,x-width/2,ground+1,width,1,C(118,175,200));
  }
 }
 /* Authored small sprites form the outer woodland, marsh and salt layers.
  * Their ground grid spans hundreds of lateral units, so distant cards enter
  * at the corners and clip sideways before growing into foreground trees.
  * Midground cells extend closer, but their large setbacks carry them off
  * the side naturally. The outer, sparser cells occupy the distant corners. */
 first=floor_div(v->visual_position-44,360);
 last=floor_div(v->visual_position+FAR_DISTANCE,360);
 for(int n=last;n>=first;n--)for(int side=-1;side<=1;side+=2)for(int column=1;column>=0;column--) {
  uint32_t h=road_landmark(v->seed^0x7fba9u^(unsigned)column*0x27fu,(uint32_t)n,side);
  if(column && (h&3)!=0)continue;
  int absolute=n*360+31+column*67+(int)((h>>8)%59),z=absolute-v->visual_position;
  if(z < -44 || z>FAR_DISTANCE)continue;
  int water,biome=scenery_material(v,absolute,side,h,&water),kind=-1;
  if(biome==CITY || biome==NEON || biome==MARINA || biome==CAUSEWAY)continue;
  if(biome==JUNGLE) {
   static const uint8_t types[5]={SC_CANOPY,SC_BANANA,SC_BAMBOO,SC_PALM,SC_VINE};kind=types[h%5];
  } else if(biome==PINES) {
   static const uint8_t types[4]={SC_PINE,SC_SPRUCE,SC_BIRCH,SC_TREE};kind=types[h%4];
  } else if(biome==SWAMP || biome==WETLANDS) {
   static const uint8_t types[4]={SC_CYPRESS,SC_MANGROVE,SC_DEADWOOD,SC_RUSH};kind=types[h%4];
  } else if(biome==AURORA && side>0)kind=h&1?SC_SPRUCE:SC_BIRCH;
  else if(biome==SALT)kind=h&1?SC_CRYSTAL:SC_SALT_MOUND;
  else if(biome==COAST && side>0)kind=h&1?SC_PALM:SC_BANANA;
  else if(biome==GROVES)kind=h&3?SC_CITRUS:SC_TREE;
  else if(biome==CANYON)kind=h&3?SC_ROCK:SC_SCRUB;
  if(kind<0)continue;
  if(water && !(biome==SWAMP || biome==WETLANDS || biome==SALT))continue;
  RoadSample road;road_sample(v->seed,v->position+z,v->start,&road);
  int offset=column?700+(int)((h>>16)%301):320+(int)((h>>16)%201);
  int world_x=road.center+side*offset;
  int x=item_x(v,world_x,z),y=project_y(z);
  int width=prop_world_width(kind)*project_scale(z)/256,height=width*PROP_H/PROP_W;
  if(width<3 || height<3 || x+width<0 || x-width>=VW)continue;
  int sw,sh;const uint8_t *anchor,*bounds;const uint16_t *palette;
  const uint8_t *pixels=prop_source(kind,side,width,&sw,&sh,&anchor,&bounds,&palette);(void)bounds;
  gfx_indexed_sprite(s,x-anchor[0]*width/PROP_W,y-anchor[1]*height/PROP_H,pixels,palette,sw,sh,width,height);
 }
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
 r.top=project_y(item->z)-r.height/2;
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
 r.top+=PLAYER_Y-DRIVE_PLAYER_Y;
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
   static const uint16_t smoke_colors[SMOKE_COUNT]={C(190,204,208),C(248,133,124),C(94,222,205),C(186,145,246),C(245,203,111)};
   uint16_t smoke=smoke_colors[g->save.smoke[vehicle]%SMOKE_COUNT];
   gfx_circle(s,sx,sy,2+age/6,age>9?blend_color(smoke,C(92,109,116),120):smoke);
  }
 }
 int paint=g->save.paint[vehicle];
 if(!v->attract && g->damage_cooldown && (g->frame&2))paint=5;
 const uint16_t *art=gfx_vehicle_pixels(vehicle,pose);
 gfx_vehicle_style(s,r.left,r.top,art,CAR_W,CAR_H,w,h,paint,g->save.livery[vehicle],g->save.wheels[vehicle]);
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
  gfx_rect(s,0,world_height-2,pulse,2,left);
  gfx_rect(s,VW-pulse,world_height-2,pulse,2,left);
 }
}

static __attribute__((noinline)) void draw_ambient(Surface *s,const View *v)
{
 RoadSample region;road_sample(v->seed,v->position,v->start,&region);
 int biome=region.blend>=128?region.next_biome:region.biome;
 if(biome==COAST || biome==CAUSEWAY || biome==SALT)for(int i=0;i<3;i++) {
  unsigned t=v->frame+(unsigned)i*61;
  int x=(int)((t/2+(unsigned)i*23)%310)-25,y=HORIZON+i*15+(int)((t/22)%3);
  if(x>road_x[y]-road_half[y]-9 && x<road_x[y]+road_half[y]+9)continue;
  int wing=(t/9)&1?2:-1;uint16_t color=biome==SALT?C(222,166,194):C(240,241,215);
  gfx_line(s,x-3,y+wing,x,y,color);gfx_line(s,x,y,x+3,y+wing,color);
 }
 if(biome==NEON || biome==JUNGLE)for(int i=0;i<10;i++) {
  uint32_t h=road_hash(v->seed^0x4a17u,(unsigned)i);
  int x=(int)((h+v->frame*3u)%278u)-11;
  int y=HORIZON+7+(int)(((h>>9)+v->frame*5u)%(world_height-HORIZON-10));
  gfx_line(s,x,y,x-1,y+3,biome==NEON?C(108,143,177):C(133,183,163));
 }
 if(biome==SWAMP || biome==WETLANDS)for(int i=0;i<13;i++) {
  uint32_t h=road_hash(v->seed^0xf14eu,(unsigned)i);
  int x=(int)((h+v->frame/8)%VW),y=HORIZON+12+(int)(((h>>9)+v->frame/17)%128);
  if(x>road_x[y]-road_half[y] && x<road_x[y]+road_half[y])continue;
  if(((v->frame/15+h)&7u)<5u)gfx_rect(s,x,y,1+(int)(h&1),1,C(206,226,132));
 }
 if(biome==SWAMP || biome==JUNGLE)for(int i=0;i<3;i++) {
  int side=i&1?1:-1,y=HORIZON+28+i*18;
  int x=side<0?14+(int)((v->frame/12+(unsigned)i*17)%26u):223-(int)((v->frame/15)%26u);
  ellipse(s,x,y,37+i*6,3,biome==SWAMP?C(110,148,135):C(115,165,140));
 }
 if(biome==AURORA)for(int i=0;i<17;i++) {
  uint32_t h=road_hash(v->seed^0x5a0fu,(unsigned)i);
  int x=(int)((h+v->frame/2u)%VW),y=HORIZON+5+(int)(((h>>9)+v->frame/2u)%165u);
  gfx_rect(s,x,y,1+(int)(h&1),1,h&1?C(214,236,240):C(160,205,224));
 }
 if(biome==PINES || biome==GROVES || biome==COAST)for(int i=0;i<4;i++) {
  unsigned t=v->frame+(unsigned)i*59u;
  if((t/90u)&1u)continue;
  int x=(int)((t*2u+(unsigned)i*71u)%290u)-17,y=HORIZON+19+(int)((t/8u+(unsigned)i*29u)%118u);
  gfx_rect(s,x,y,2,1,biome==COAST?C(178,198,104):C(147,180,101));
 }
}

static View make_view(const Game *g,int attract,RoadSample *player)
{
 View v;
 v.attract=attract;v.frame=g->frame;
 v.seed=attract?g->save.seed:g->run_seed;
 /* A departure preview loops inside its selected section. Its moving road
  * never advances to the next biome while the player studies the menu. */
 unsigned preview_span=ROAD_BIOME_LENGTH-ROAD_BLEND_LENGTH-FAR_DISTANCE-1000;
 if(attract && g->mode==M_DEPARTURES)
  v.position=(int)((g->frame%(preview_span/4u))*4u);
 else v.position=attract?(int)((g->frame*4u)%(ROAD_BIOME_LENGTH*LOCATION_COUNT)):g->position_q8/Q;
 v.motion=clampi(g->save.motion,0,2);
 v.visual_scale=v.motion==0?205:v.motion==2?384:307;
 v.visual_position=visual_travel(v.position,v.motion);
 v.speed=attract?145:(g->speed_q8<0?-g->speed_q8:g->speed_q8)/Q;
 v.start=attract?(g->mode==M_DEPARTURES?g->cursor:COAST):g->start_biome;
 road_sample(v.seed,v.position,v.start,player);
 v.center=player->center;
 v.cam_x=attract?0:g->x_q8/(Q*8);
 if(!attract && g->save.shake && g->shake)v.cam_x+=(g->frame&1?1:-1)*clampi(g->shake/4,0,2);
 return v;
}

static FAST __attribute__((noinline)) void draw_world(Surface *s,const Game *g,int attract)
{
 RoadSample player;View v=make_view(g,attract,&player);
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
 draw_far_landscape(s,&v);
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
  case ITEM_PROP:case ITEM_BACK_PROP:case ITEM_BUILDING:
   draw_prop(s,&v,&items[i]);
   scenery_work[(items[i].kind==ITEM_BUILDING || building_id(items[i].id))?1:(items[i].id==SC_PIER || items[i].id==SC_SAIL)?3:2]+=
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
  int y=HORIZON+10+(int)(((unsigned)v.visual_position*2+(unsigned)i*67)%(world_height-HORIZON-20));
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

/* A room is staged around the authored rear-three-quarter vehicle, with
 * floor depth converging above its bonnet and three upright enclosing walls. */
static void overlay_panel(Surface *s,int x,int y,int w,int h,uint16_t color){
 for(int row=y;row<y+h;row++)for(int col=x;col<x+w;col++)if((unsigned)col<VW&&(unsigned)row<VH)
  s->pixels[row*VW+col]=blend_color(s->pixels[row*VW+col],color,160);
}
static void draw_garage_room(Surface *s,const Game *g,const StudioTheme *ui){
 static const uint16_t wall[LOCATION_COUNT]={C(93,107,124),C(106,176,176),C(77,135,163),C(108,119,103),C(177,119,94),C(35,65,92),C(88,116,103),C(174,164,115),C(31,29,67),C(62,107,83),C(65,92,81),C(127,156,180),C(169,143,167)};
 static const uint16_t floor[LOCATION_COUNT]={C(80,90,104),C(130,165,160),C(86,122,143),C(133,113,85),C(153,119,92),C(50,74,97),C(88,104,87),C(155,147,115),C(28,37,59),C(93,111,85),C(75,94,83),C(119,141,157),C(172,163,178)};
 static const uint8_t faces[LOCATION_COUNT]={6,4,2,16,13,3,9,10,23,14,17,15,24};
 int region=g->save.selected_location,id=clampi(g->selection,0,VEHICLE_COUNT-1);
 uint16_t base=wall[region],ground=floor[region],rim=blend_color(base,C(21,32,46),130);
 gfx_rect(s,0,0,VW,VH,base);
 gfx_indexed_sprite(s,32,0,building_wall_idx+faces[region]*4096,building_palette+faces[region]*256,64,64,192,57);
 overlay_panel(s,32,0,192,57,base);
 SceneryPoint left[4]={{0,0},{34,16},{34,57},{0,98}},right[4]={{222,16},{256,0},{256,98},{222,57}};
 for(int y=57;y<VH;y++){int amount=(y-57)*110/(VH-57);gfx_rect(s,0,y,VW,1,blend_color(ground,C(21,31,43),amount));}
 uint16_t seam=blend_color(ground,C(14,28,39),90);
 for(int n=-3;n<=4;n++)gfx_line(s,128+n*22,57,128+n*86,191,seam);
 for(int y=67;y<192;y+=(y-40)/2)gfx_rect(s,0,y,256,1,seam);
 scenery_polygon(s,left,4,blend_color(base,C(16,29,43),70));scenery_polygon(s,right,4,blend_color(base,C(16,29,43),130));
 gfx_line(s,0,97,34,57,rim);gfx_line(s,34,57,222,57,rim);gfx_line(s,222,57,255,97,rim);
 /* Window and local wall fittings distinguish the departure's workshop. */
 uint16_t sky=sky_themes[region].high;
 gfx_rect(s,181,18,32,24,rim);gfx_rect(s,184,21,26,18,sky);
 if(region==NEON||region==MARINA){gfx_rect(s,182,29,4,10,C(224,90,191));gfx_rect(s,190,24,5,15,C(73,211,218));gfx_rect(s,199,26,7,13,C(129,111,223));}
 else if(region==AURORA){gfx_rect(s,184,32,26,7,C(207,231,239));gfx_line(s,185,26,194,20,C(129,236,210));}
 else if(region==JUNGLE||region==SWAMP||region==WETLANDS||region==PINES||region==GROVES){for(int n=0;n<4;n++){gfx_rect(s,186+n*6,27-n%2*4,2,12,C(48,91,66));gfx_circle(s,187+n*6,27-n%2*4,4,C(61,118,78));}}
 else {gfx_rect(s,184,33,26,6,sea_color(region));gfx_rect(s,198,24,4,4,C(248,226,161));}
 gfx_rect(s,196,20,1,20,rim);gfx_rect(s,183,29,28,1,rim);
 gfx_rect(s,7,77,23,15,blend_color(base,C(12,26,40),155));gfx_rect(s,6,74,25,3,C(174,185,184));
 for(int i=0;i<3;i++){gfx_rect(s,10+i*7,80,4,2,C(190,197,190));gfx_rect(s,10+i*7,85,4,2,C(190,197,190));}
 uint16_t light=region==NEON?C(229,100,213):region==SWAMP?C(181,209,141):region==AURORA?C(153,228,239):C(224,235,221);
 gfx_line(s,6,14,30,25,light);gfx_line(s,226,25,250,14,light);
 if(region==NEON||region==MARINA){gfx_line(s,34,58,0,100,C(84,197,214));gfx_line(s,222,58,255,100,C(209,86,194));}
 /* Service pad shares the car's depth axis; it is not a flat portrait card. */
 SceneryPoint pad[4]={{91,67},{145,67},{164,183},{67,183}};
 scenery_polygon(s,pad,4,blend_color(ground,C(25,37,48),65));
 gfx_line(s,91,67,67,183,blend_color(light,ground,110));gfx_line(s,145,67,164,183,blend_color(light,ground,110));
 int w=vehicles[id].bike?43:73,h=108,x=116-w/2,y=65;
 int preview=g->garage_tab==2;
 int paint=preview?g->paint_choice:g->save.paint[id],livery=preview?g->livery_choice:g->save.livery[id],wheels=preview?g->wheel_choice:g->save.wheels[id];
 int smoke=preview?g->smoke_choice:g->save.smoke[id];
 if(preview && smoke){static const uint16_t colors[5]={C(190,204,208),C(248,133,124),C(94,222,205),C(186,145,246),C(245,203,111)};ellipse(s,x+8,175,15,4,blend_color(colors[smoke],ground,90));ellipse(s,x+w-8,177,17,4,blend_color(colors[smoke],ground,90));}
 contact_shadow(s,x,y,w,h,vehicle_body_spans[id*5+2],CAR_W,CAR_H);
 gfx_vehicle_style(s,x,y,gfx_vehicle_pixels(id,2),CAR_W,CAR_H,w,h,paint,livery,wheels);
 overlay_panel(s,4,3,248,27,ui->pane);gfx_text(s,10,7,vehicles[id].name,ui->ink);
 char subtitle[56];snprintf(subtitle,sizeof subtitle,"%s / %s",locations[region].name,g->garage_tab==3?"DEALER":g->garage_tab==1?"TUNING":g->garage_tab==2?"STYLE":"YOUR COLLECTION");gfx_text_small(s,10,20,subtitle,ui->muted);
 overlay_panel(s,179,52,73,119,ui->pane);
 char class_text[16];snprintf(class_text,sizeof class_text,"CLASS %s",game_class_name(vehicles[id].class_id));gfx_text_small(s,184,57,class_text,ui->accent);
 static const char*const labels[4]={"TOP SPEED","0-100 km/h","ROAD GRIP","ARMOR"};
 for(int stat=0;stat<4;stat++){int value=game_preview_stat(g,id,stat),row=70+stat*24;gfx_text_small(s,184,row,labels[stat],ui->muted);char text[24];
  if(stat==0){if(g->save.units)value=(value*621+500)/1000;snprintf(text,sizeof text,"%d %s",value,g->save.units?"mph":"km/h");}
  else if(stat==1){int tenth=(1000+value/2)/(value?value:1);snprintf(text,sizeof text,"%d.%d s",tenth/10,tenth%10);}
  else snprintf(text,sizeof text,"%d",value);
  gfx_text(s,184,row+10,text,ui->ink);
 }
 overlay_panel(s,4,179,248,13,ui->pane);
 if(g->notice_timer && g->notice[0])gfx_text_small(s,9,182,g->notice,ui->ink);
 else{char line[64];snprintf(line,sizeof line,"MASTERY %d  /  %s",game_mastery_level(g,id),game_specialty_name(vehicles[id].specialty));gfx_text_small(s,9,182,line,ui->ink);}
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
 if(!receipt){draw_garage_room(scene,g,theme);old_key=key;ready=1;gfx_copy(s->pixels,studio_art,VW*VH);return;}
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
 int paint=!receipt && g->garage_tab==0 && g->cursor==2?g->paint_choice:g->save.paint[id];
 contact_shadow(scene,x,y,w,h,vehicle_body_spans[id*5+pose],CAR_W,CAR_H);
 const uint16_t *art=gfx_vehicle_pixels(id,pose);
 int livery=g->garage_tab==3?g->livery_choice:g->save.livery[id];
 int wheels=g->garage_tab==3?g->wheel_choice:g->save.wheels[id];
 gfx_vehicle_style(scene,x,y,art,CAR_W,CAR_H,w,h,paint,livery,wheels);
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

static const char *const menu_captions[6]={
 "CITY LIGHTS TO OPEN WATER","A NEW HORIZON STARTS HERE",
 "MAKE EVERY MILE COUNT","BUILT FOR BUTTONS",
 "MOMENTUM BUILDS YOUR STORY","MAKE THIS DRIVE YOURS"};

/* Cached glyph coverage keeps antialiasing transparent over the moving scene. */
static uint8_t menu_chrome_alpha[VW*45];
static void chrome_glyphs(int x,int y,const char*text,int small){
 const uint8_t *alpha=small?font_small_alpha:font_alpha,*advance=small?font_small_advance:font_advance;
 while(*text){unsigned ch=(unsigned char)*text++;if(ch<32||ch>127)ch='?';const uint8_t*p=alpha+(ch-32)*12*14;
  for(int j=0;j<14 && y+j<45;j++)for(int i=0;i<12 && x+i<VW;i++)if(x+i>=0 && y+j>=0){
   uint8_t *a=&menu_chrome_alpha[(y+j)*VW+x+i];if(p[j*12+i]>*a)*a=p[j*12+i];
  }x+=advance[ch-32];
 }
}
static void draw_menu_chrome(Surface*s,const Game*g,int caption,int fade){
 static int old_caption=-1;
 const StudioTheme *ui=theme_for(g);
 if(old_caption!=caption){
  memset(menu_chrome_alpha,0,sizeof menu_chrome_alpha);
  chrome_glyphs(12,5,"C O A S T L I N E",0);
  chrome_glyphs(12,18,"ENDLESS ROADS. YOUR OWN LINE.",1);
  chrome_glyphs(12,36,menu_captions[caption],1);old_caption=caption;
 }
 /* Compact 38%-opaque floating material, never full-width letterboxing. */
 for(int y=2;y<28;y++)for(int x=7;x<180;x++){
  int cut=y==2||y==27?3:y==3||y==26?1:0;
  if(x>=7+cut&&x<180-cut)s->pixels[y*VW+x]=blend_color(s->pixels[y*VW+x],ui->pane,fade*96/256);
 }
 int width=0;for(const char*p=menu_captions[caption];*p;p++)width+=font_small_advance[(unsigned char)*p-32];
 for(int y=177;y<190;y++)for(int x=7;x<18+width;x++)s->pixels[y*VW+x]=blend_color(s->pixels[y*VW+x],ui->pane,fade*96/256);
 for(int y=2;y<28;y++)for(int x=7;x<180;x++){int alpha=menu_chrome_alpha[y*VW+x];if(alpha)s->pixels[y*VW+x]=blend_color(s->pixels[y*VW+x],ui->ink,fade*alpha/255);}
 for(int y=177;y<190;y++)for(int x=7;x<18+width;x++){int alpha=menu_chrome_alpha[(y-145)*VW+x];if(alpha)s->pixels[y*VW+x]=blend_color(s->pixels[y*VW+x],ui->ink,fade*alpha/255);}
}

static void draw_menu_decoration(Surface *s,const Game *g)
{
 if(g->mode==M_GARAGE || g->mode==M_RECEIPT) {
  draw_showroom(s,g,g->mode==M_RECEIPT);return;
 }
 draw_world(s,g,1);

 int caption=g->mode==M_DEPARTURES?1:g->mode==M_CHALLENGES?2:g->mode==M_CONTROLS?3:g->mode==M_CAREER?4:g->mode==M_SETTINGS?5:0;
 draw_menu_chrome(s,g,caption,256);
}

static Game bottom_key;
static int bottom_valid;
int render_game(const Game *g,uint16_t *top,uint16_t *bottom)
{
 unsigned total_start=profile_clock();
 init_projection();
 Surface t={top,VW,VH},b={bottom,VW,VH};
 if(g->mode==M_RUN || g->mode==M_PAUSE) {
  draw_world(&t,g,0);draw_run_overlay(&t,g);
 } else draw_menu_decoration(&t,g);
 render_profile[0]=(profile_clock()-total_start)&65535u;

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

/* A tall camera frame lives in spare LCDC VRAM, not the 4 MiB main RAM.
 * During the slide, its lower half becomes the ordinary menu camera exactly. */
static void opening_wordmark(Surface*s,const char*text,uint16_t color){
 int x=(VW-gfx_text_width(text)*2)/2;
 while(*text){unsigned c=(unsigned char)*text++;if(c<32||c>127)c='?';const uint8_t*a=font_alpha+(c-32)*12*14;
  for(int y=0;y<14;y++)for(int i=0;i<12;i++)if(a[y*12+i]){
   int xx=x+i*2,yy=25+y*2;if(xx>=0&&xx<VW)gfx_rect(s,xx,yy,2,2,blend_color(s->pixels[yy*VW+xx],color,a[y*12+i]));
  }x+=font_advance[c-32]*2;
 }
}
int render_opening(const Game*g,const Opening*o,uint16_t*scratch,uint16_t*top,uint16_t*bottom)
{
 bottom_valid=0;
 int p=0;
 if(o->phase==OPEN_SLIDE){
  int t=clampi(o->slide,0,OPENING_SLIDE_TICKS),q=t*256/OPENING_SLIDE_TICKS;
  q=q*q*(768-2*q)/65536;p=VH*q/256;
 }
 sky_origin=p;
 set_projection(WORLD_MAX_HEIGHT,128+(VH+DRIVE_HORIZON-128)*p/VH,VH+DRIVE_PLAYER_Y);
 Surface movie={scratch,VW,WORLD_MAX_HEIGHT};draw_world(&movie,g,1);
 gfx_copy(top,scratch+p*VW,VW*VH);
 gfx_copy(bottom,scratch+(VH+p)*VW,VW*(VH-p));
 Surface upper={top,VW,VH};
 if(o->phase==OPEN_SLIDE){
  Surface menu={scratch,VW,VH};ui_draw_bottom(&menu,g);
  gfx_copy(bottom+(VH-p)*VW,scratch,VW*p);
  if(p>128){
   draw_menu_chrome(&upper,g,0,(p-128)*256/64);
  }
 }else{
  const StudioTheme*theme=theme_for(g);
  overlay_panel(&upper,19,22,218,43,theme->pane);
  opening_wordmark(&upper,"Coastline",theme->ink);
  gfx_text_small(&upper,100,55,"ENDLESS DRIVE",theme->accent);
  if(o->phase==OPEN_TITLE){
   overlay_panel(&upper,37,83,182,19,theme->pane);
   gfx_text_small(&upper,56,90,"PRESS ANY BUTTON TO START",(g->frame/24u)&1u?theme->ink:theme->accent);
  }else{
   for(int i=0;i<SAVE_SLOT_COUNT;i++){
    int y=69+i*35,selected=o->cursor==i;
    overlay_panel(&upper,19,y,218,32,theme->pane);
    overlay_panel(&upper,19,y,218,32,theme->pane);
    if(selected){gfx_rect(&upper,19,y,2,32,theme->accent);gfx_rect(&upper,21,y,216,1,theme->accent);}
    char label[48];snprintf(label,sizeof label,"%s SAVE %d",selected?">":" ",i+1);gfx_text(&upper,27,y+4,label,theme->ink);
    const SlotInfo*slot=&o->slots[i];
    if(slot->valid){snprintf(label,sizeof label,"$%lu  /  %lu RUNS",(unsigned long)slot->cash,(unsigned long)slot->runs);gfx_text_small(&upper,27,y+20,label,theme->muted);gfx_text_small(&upper,129,y+6,vehicles[slot->vehicle].name,theme->accent);}
    else gfx_text_small(&upper,27,y+20,"NEW DRIVE / EMPTY SLOT",theme->muted);
   }
   overlay_panel(&upper,19,177,218,14,theme->pane);overlay_panel(&upper,19,177,218,14,theme->pane);
   gfx_text_small(&upper,31,181,"UP/DOWN: CHOOSE   A: OPEN   B: BACK",theme->ink);
  }
 }
 return 3;
}
