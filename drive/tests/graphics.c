/* Pixel-exact native checks of scaled sprites, opaque-run caches and clipping.
 * Run from drive/ so assets.c's .incbin paths resolve:
 * cc -std=c11 -O2 -Wall -Wextra -Werror -pedantic -Isrc -Ibuild \
 *   tests/graphics.c src/gfx.c build/assets.c -o build/test-graphics
 * Add -g -fsanitize=address,undefined -fno-omit-frame-pointer for memory checks.
 * These are portable rendering checks, not Nintendo DS emulator proof.
 */
#include "gfx.h"
#include "assets.h"
#include "vehicle_geometry.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { WIDTH=83,HEIGHT=109,GUARD=8,MAX_ASSETS=224 };
typedef struct {
 uint16_t before[GUARD],pixels[WIDTH*HEIGHT],after[GUARD];
} GuardedFrame;
typedef struct {
 const uint16_t *pixels;
 int width,height,scaled_api,prop_id;
 const char *name;
} Asset;
static GuardedFrame actual,expected;
static Asset assets[MAX_ASSETS];
static unsigned asset_count,checks;
static uint16_t uncached_source[65*97];

static void fail(const char *label,unsigned id,int paint,int w,int h,int x,int y,int offset)
{
 fprintf(stderr,"graphics: %s asset=%u paint=%d size=%dx%d at=%d,%d pixel=%d\n",
         label,id,paint,w,h,x,y,offset);
 exit(1);
}
static void fill_frame(GuardedFrame *frame,unsigned seed)
{
 for(int i=0;i<GUARD;i++)frame->before[i]=frame->after[i]=(uint16_t)(0xa571u+i);
 for(unsigned i=0;i<WIDTH*HEIGHT;i++)
  frame->pixels[i]=(uint16_t)(0x8000u|((i*109u+seed*37u)&32767u));
}
static void compare(const char *label,unsigned id,int paint,int w,int h,int x,int y)
{
 for(int i=0;i<GUARD;i++)if(actual.before[i]!=(uint16_t)(0xa571u+i)||actual.after[i]!=(uint16_t)(0xa571u+i))
  fail("destination guard overwritten",id,paint,w,h,x,y,i);
 for(int i=0;i<WIDTH*HEIGHT;i++)if(actual.pixels[i]!=expected.pixels[i])
  fail(label,id,paint,w,h,x,y,i);
 checks++;
}

/* Independent reference: integer nearest-neighbor sampling and explicit
 * destination clipping. Scenery expectations use the documented offline LOD
 * bands, without invoking the implementation's cache or sampling helpers. */
static Asset expected_source(Asset a,int w,int h)
{
 if(a.prop_id>=0) {
  if(w<=16&&h<=24){a.pixels=prop_lod16+a.prop_id*16*24;a.width=16;a.height=24;}
  else if(w<=32&&h<=48){a.pixels=prop_lod32+a.prop_id*32*48;a.width=32;a.height=48;}
  else if(w<=48&&h<=72){a.pixels=prop_lod48+a.prop_id*48*72;a.width=48;a.height=72;}
 }
 return a;
}
static void nearest_reference(Asset a,int w,int h,int x,int y)
{
 if(w<=0||h<=0)return;
 a=expected_source(a,w,h);
 for(int row=0;row<h;row++)for(int col=0;col<w;col++) {
  int xx=x+col,yy=y+row;
  if(xx<0||xx>=WIDTH||yy<0||yy>=HEIGHT)continue;
  uint16_t c=a.pixels[(row*a.height/h)*a.width+col*a.width/w];
  if(c)expected.pixels[yy*WIDTH+xx]=c;
 }
}
static void draw(Asset a,int w,int h,int x,int y,int paint)
{
 Surface surface={actual.pixels,WIDTH,HEIGHT};
 if(a.scaled_api&&!paint)gfx_scaled_sprite(&surface,x,y,a.pixels,a.width,a.height,w,h);
 else gfx_vehicle(&surface,x,y,a.pixels,a.width,a.height,w,h,paint);
}
static void factory_case(Asset a,unsigned id,int w,int h,int x,int y)
{
 fill_frame(&expected,id+17u);nearest_reference(a,w,h,x,y);
 /* The second draw must agree after the new entry has become warm. */
 for(int pass=0;pass<2;pass++) {
  fill_frame(&actual,id+17u);draw(a,w,h,x,y,0);
  compare(pass?"warm factory mismatch":"cold factory mismatch",id,0,w,h,x,y);
 }
}

/* A width-65 identity sprite deliberately bypasses the <=64 cache. Its first
 * w columns contain the exact source samples required by the cached sprite;
 * the rest are transparent. This compares cache paints against the uncached
 * path without duplicating the palette algorithm or altering visible scale. */
static void paint_case(Asset a,unsigned id,int paint,int w,int h,int x,int y)
{
 if(w>64||h>97)fail("paint fixture size",id,paint,w,h,x,y,0);
 fill_frame(&expected,id+31u);
 memset(uncached_source,0,sizeof uncached_source);
 for(int row=0;row<h;row++)for(int col=0;col<w;col++)
  uncached_source[row*65+col]=a.pixels[(row*a.height/h)*a.width+col*a.width/w];
 Surface reference={expected.pixels,WIDTH,HEIGHT};
 gfx_vehicle(&reference,x,y,uncached_source,65,h,65,h,paint);
 for(int pass=0;pass<2;pass++) {
  fill_frame(&actual,id+31u);
  Surface target={actual.pixels,WIDTH,HEIGHT};
  gfx_vehicle(&target,x,y,a.pixels,a.width,a.height,w,h,paint);
  compare(pass?"warm paint/fallback mismatch":"cold paint/fallback mismatch",id,paint,w,h,x,y);
 }
}

static void add_asset(const uint16_t *pixels,int width,int height,int scaled_api,int prop_id,const char *name)
{
 if(asset_count==MAX_ASSETS)fail("asset catalog overflow",asset_count,0,0,0,0,0,0);
 assets[asset_count++]=(Asset){pixels,width,height,scaled_api,prop_id,name};
}
static void catalog(void)
{
 for(int i=0;i<ASSET_VEHICLE_COUNT*5;i++)
  add_asset(vehicle_art+i*48*72,48,72,0,-1,"vehicle pose");
 for(int i=0;i<6;i++)add_asset(traffic_art+i*40*64,40,64,0,-1,"traffic");
 for(int i=0;i<ASSET_PROP_COUNT;i++)add_asset(prop_art+i*64*96,64,96,1,i,"prop with LOD");
 for(int i=0;i<ASSET_PROP_COUNT;i++)add_asset(prop_lod48+i*48*72,48,72,1,-1,"LOD48");
 for(int i=0;i<ASSET_PROP_COUNT;i++)add_asset(prop_lod32+i*32*48,32,48,1,-1,"LOD32");
 for(int i=0;i<ASSET_PROP_COUNT;i++)add_asset(prop_lod16+i*16*24,16,24,1,-1,"LOD16");
 /* Side sprites have already-selected LOD sources in the scene renderer.
  * They intentionally use the generic scaling contract here, rather than
  * the automatic prop_art-pointer LOD selection used by the original set. */
 for(int i=0;i<ASSET_PROP_SIDE_COUNT;i++)add_asset(prop_side_art+i*64*96,64,96,1,-1,"opposite shoulder");
 for(int i=0;i<ASSET_PROP_SIDE_COUNT;i++)add_asset(prop_side_lod48+i*48*72,48,72,1,-1,"side LOD48");
 for(int i=0;i<ASSET_PROP_SIDE_COUNT;i++)add_asset(prop_side_lod32+i*32*48,32,48,1,-1,"side LOD32");
 for(int i=0;i<ASSET_PROP_SIDE_COUNT;i++)add_asset(prop_side_lod16+i*16*24,16,24,1,-1,"side LOD16");
 add_asset(boat_ground,ASSET_BOAT_GROUND_W,ASSET_BOAT_GROUND_H,1,-1,"planar yacht");
 add_asset(pier_ground,ASSET_PIER_GROUND_W,ASSET_PIER_GROUND_H,1,-1,"planar pier");
 for(int i=0;i<ASSET_BUILDING_COUNT;i++) {
  add_asset(building_wall_art+i*64*64,64,64,1,-1,"flat wall elevation");
  add_asset(building_end_art+i*32*64,32,64,1,-1,"flat end elevation");
  add_asset(building_roof_art+i*32*32,32,32,1,-1,"overhead roof");
  add_asset(building_wall_lod32+i*32*32,32,32,1,-1,"wall LOD32");
  add_asset(building_wall_lod16+i*16*16,16,16,1,-1,"wall LOD16");
  add_asset(building_end_lod16+i*16*32,16,32,1,-1,"end LOD16");
  add_asset(building_end_lod8+i*8*16,8,16,1,-1,"end LOD8");
 }
 add_asset(boat_profile,ASSET_BOAT_PROFILE_W,ASSET_BOAT_PROFILE_H,1,-1,"raised profile yacht");
 add_asset(dock_segment,ASSET_DOCK_SEGMENT_W,ASSET_DOCK_SEGMENT_H,1,-1,"modular dock");
 if(asset_count!=ASSET_VEHICLE_COUNT*5+6+ASSET_PROP_COUNT*4+ASSET_PROP_SIDE_COUNT*4+39)
  fail("incomplete asset catalog",asset_count,0,0,0,0,0,0);
 for(unsigned i=0;i<asset_count;i++) {
  unsigned visible=0;
  for(int n=0;n<assets[i].width*assets[i].height;n++) {
   uint16_t c=assets[i].pixels[n];
   if(c&&!(c&0x8000u))fail("invalid RGB555 opacity",i,0,0,0,0,0,n);
   visible+=(c!=0);
  }
  if(!visible)fail("empty asset",i,0,0,0,0,0,0);
  checks++;
 }
}
static void clean_vehicle_metadata(void)
{
 for(int group=0;group<2;group++) {
  int count=group?6:ASSET_VEHICLE_COUNT*5,w=group?40:48,h=group?64:72;
  const uint16_t *pixels=group?traffic_art:vehicle_art;
  for(int id=0;id<count;id++) {
   const uint16_t *source=pixels+id*w*h;
   unsigned total=0,head=0,tail=0,dark=0;
   uint16_t queue[48*72];uint8_t visited[48*72]={0};
   for(int y=0;y<h;y++) {
    int left=w,right=0;
    for(int x=0;x<w;x++) {
     uint16_t c=source[y*w+x];
     uint32_t word=group?traffic_body_masks[id][y][x/32]:vehicle_body_masks[id][y][x/32];
     if(((word>>(x%32))&1u)!=(c!=0))
      fail("collision mask differs from final visible pixels",(unsigned)id,group,w,h,x,y,0);
     if(c) {
      if(x==0||x==w-1||y==0||y==h-1)
       fail("vehicle yaw clipped or opaque matte reaches canvas edge",(unsigned)id,group,w,h,x,y,0);
      if(x<left)left=x;
      if(x+1>right)right=x+1;
      if(!total){queue[tail++]=(uint16_t)(y*w+x);visited[y*w+x]=1;}
      total++;
      dark+=((c&31u)<5u&&((c>>5)&31u)<5u&&((c>>10)&31u)<5u);
     }
    }
    const uint8_t *span=group?traffic_body_spans[id][y]:vehicle_body_spans[id][y];
    if(span[0]!=(right?left:0)||span[1]!=right)
     fail("collision row span differs from final visible pixels",(unsigned)id,group,w,h,y,0,0);
    checks++;
   }
   /* The old white-sedan crop accidentally included a disconnected strip of
    * the coach below it. Every cleaned vehicle is one connected silhouette;
    * this catches that atlas contamination without deleting genuine black
    * tires/glass or treating all dark pixels as a background color. */
   while(head<tail) {
    int pixel=queue[head++],x=pixel%w,y=pixel/w;
    const int dx[]={-1,1,0,0},dy[]={0,0,-1,1};
    for(int n=0;n<4;n++) {
     int xx=x+dx[n],yy=y+dy[n],p=yy*w+xx;
     if(xx<0||xx>=w||yy<0||yy>=h||visited[p]||!source[p])continue;
     visited[p]=1;queue[tail++]=(uint16_t)p;
    }
   }
   if(tail!=total)fail("detached vehicle background artifact",(unsigned)id,group,w,h,0,0,(int)(total-tail));
   if(dark<5)fail("genuine dark tires/glass were removed",(unsigned)id,group,w,h,0,0,(int)dark);
   checks+=2;
  }
 }
}
static void dimensional_texture_metadata(void)
{
 const uint16_t *faces[]={building_wall_art,building_end_art,building_roof_art,dock_segment,
                         building_wall_lod32,building_wall_lod16,building_end_lod16,building_end_lod8};
 const int lengths[]={5*64*64,5*32*64,5*32*32,64*32,5*32*32,5*16*16,5*16*32,5*8*16};
 for(unsigned face=0;face<sizeof faces/sizeof faces[0];face++) {
  for(int p=0;p<lengths[face];p++)if(!faces[face][p])
   fail("wall/roof/dock face contains a transparent seam",face,0,0,0,0,0,p);
  checks++;
 }
 int first=ASSET_BOAT_PROFILE_H,last=-1,left=ASSET_BOAT_PROFILE_W,right=-1;
 for(int y=0;y<ASSET_BOAT_PROFILE_H;y++)for(int x=0;x<ASSET_BOAT_PROFILE_W;x++)
  if(boat_profile[y*ASSET_BOAT_PROFILE_W+x]) {
   if(y<first)first=y;
   if(y>last)last=y;
   if(x<left)left=x;
   if(x>right)right=x;
  }
 if(last-first<12||right-left<=2*(last-first)||boat_profile_anchor[1]!=last
   ||boat_profile_anchor[0]<left||boat_profile_anchor[0]>right)
  fail("boat profile must rise above anchored broadside waterline",0,0,64,48,left,first,last);
 checks++;
}
static void ground_metadata(void)
{
 static const struct {
  const uint16_t *pixels;
  const uint8_t *bounds;
  int width,height,count;
 } silhouettes[]={
  {prop_art,prop_bounds,64,96,ASSET_PROP_COUNT},
  {prop_lod48,prop_lod48_bounds,48,72,ASSET_PROP_COUNT},
  {prop_lod32,prop_lod32_bounds,32,48,ASSET_PROP_COUNT},
  {prop_lod16,prop_lod16_bounds,16,24,ASSET_PROP_COUNT},
  {prop_side_art,prop_side_bounds,64,96,ASSET_PROP_SIDE_COUNT},
  {prop_side_lod48,prop_side_lod48_bounds,48,72,ASSET_PROP_SIDE_COUNT},
  {prop_side_lod32,prop_side_lod32_bounds,32,48,ASSET_PROP_SIDE_COUNT},
  {prop_side_lod16,prop_side_lod16_bounds,16,24,ASSET_PROP_SIDE_COUNT}
 };
 for(unsigned group=0;group<sizeof silhouettes/sizeof silhouettes[0];group++) {
  int w=silhouettes[group].width,h=silhouettes[group].height;
  for(int id=0;id<silhouettes[group].count;id++) {
   int left=w,right=-1,top=h,bottom=-1;
   const uint16_t *source=silhouettes[group].pixels+id*w*h;
   for(int row=0;row<h;row++)for(int col=0;col<w;col++)if(source[row*w+col]) {
    if(col<left)left=col;
    if(col>right)right=col;
    if(row<top)top=row;
    if(row>bottom)bottom=row;
   }
   const uint8_t *bounds=silhouettes[group].bounds+id*4;
   if(bounds[0]!=left||bounds[1]!=top||bounds[2]!=right+1||bounds[3]!=bottom+1)
    fail("silhouette bounds omit or expand pixels",group,0,w,h,id,0,0);
   checks++;
  }
 }
 static const int side_ids[]={ASSET_PROP_GLASS,ASSET_PROP_BRICK,ASSET_PROP_WAREHOUSE,
  ASSET_PROP_ROCK,ASSET_PROP_LAMP,ASSET_PROP_HOTEL,ASSET_PROP_VILLA};
 static const int side_indices[]={ASSET_PROP_SIDE_GLASS,ASSET_PROP_SIDE_BRICK,
  ASSET_PROP_SIDE_WAREHOUSE,ASSET_PROP_SIDE_ROCK,ASSET_PROP_SIDE_LAMP,
  ASSET_PROP_SIDE_HOTEL,ASSET_PROP_SIDE_VILLA};
 if(sizeof side_ids/sizeof side_ids[0]!=ASSET_PROP_SIDE_COUNT)
  fail("side metadata coverage",asset_count,0,0,0,0,0,0);
 for(int id=0;id<ASSET_PROP_COUNT;id++) {
  int x=prop_anchor[id*2],y=prop_anchor[id*2+1],lowest=-1;
  if(x>=ASSET_PROP_W||y>=ASSET_PROP_H)
   fail("ground anchor outside sprite",(unsigned)id,0,ASSET_PROP_W,ASSET_PROP_H,x,y,0);
  const uint16_t *source=prop_art+id*ASSET_PROP_W*ASSET_PROP_H;
  for(int row=0;row<ASSET_PROP_H;row++)for(int col=0;col<ASSET_PROP_W;col++)
   if(source[row*ASSET_PROP_W+col])lowest=row;
  /* Anchors may sit just beside a transparent ground corner. They must still
   * coincide with the foot's vertical band, rather than a roof or canopy. */
  if(lowest<0||y<lowest-4||y>lowest+1)
   fail("ground anchor above foot",(unsigned)id,0,ASSET_PROP_W,ASSET_PROP_H,x,y,lowest);
  checks++;
 }
 for(int side=0;side<ASSET_PROP_SIDE_COUNT;side++) {
  int id=side_ids[side];
  if(side_indices[side]!=side||prop_side_anchor[side*2]!=ASSET_PROP_W-1-prop_anchor[id*2]
    ||prop_side_anchor[side*2+1]!=prop_anchor[id*2+1])
   fail("opposite shoulder anchor mirror",(unsigned)side,0,ASSET_PROP_W,ASSET_PROP_H,0,0,id);
  for(int row=0;row<ASSET_PROP_H;row++)for(int col=0;col<ASSET_PROP_W;col++)
   if(prop_side_art[(side*ASSET_PROP_H+row)*ASSET_PROP_W+col]
     !=prop_art[(id*ASSET_PROP_H+row)*ASSET_PROP_W+ASSET_PROP_W-1-col])
    fail("opposite shoulder art mirror",(unsigned)side,0,ASSET_PROP_W,ASSET_PROP_H,col,row,id);
  checks++;
 }
 if(ASSET_BOAT_GROUND_W!=128||ASSET_BOAT_GROUND_H!=64
   ||ASSET_PIER_GROUND_W!=64||ASSET_PIER_GROUND_H!=128)
  fail("water-plane texture format",asset_count,0,0,0,0,0,0);
 /* Boats run across their source width; piers run along source height. This
  * catches an accidental upright yacht or a transposed pier at pack time. */
 const uint16_t *planes[]={boat_ground,pier_ground};
 const int widths[]={ASSET_BOAT_GROUND_W,ASSET_PIER_GROUND_W};
 const int heights[]={ASSET_BOAT_GROUND_H,ASSET_PIER_GROUND_H};
 for(int plane=0;plane<2;plane++) {
  int left=widths[plane],right=-1,top=heights[plane],bottom=-1;
  for(int row=0;row<heights[plane];row++)for(int col=0;col<widths[plane];col++)
   if(planes[plane][row*widths[plane]+col]) {
    if(col<left)left=col;
    if(col>right)right=col;
    if(row<top)top=row;
    if(row>bottom)bottom=row;
   }
  if(right<left||bottom<top||(plane==0?right-left<=bottom-top:bottom-top<=right-left))
   fail("water-plane texture orientation",(unsigned)plane,0,widths[plane],heights[plane],left,top,0);
  checks++;
 }
}
static void all_scales_and_edges(void)
{
 static const int sizes[][2]={{1,1},{1,96},{64,1},{7,11},{16,24},{23,34},
  {31,47},{32,48},{33,49},{48,72},{64,96},{65,97}};
 for(unsigned id=0;id<asset_count;id++)for(unsigned n=0;n<sizeof sizes/sizeof sizes[0];n++) {
  int w=sizes[n][0],h=sizes[n][1];
  const int positions[][2]={{9,5},{-w/2,5},{WIDTH-w/2,5},{9,-h/2},
   {9,HEIGHT-h/2},{-w/2,-h/2},{WIDTH-w/2,HEIGHT-h/2}};
  for(unsigned p=0;p<sizeof positions/sizeof positions[0];p++)
   factory_case(assets[id],id,w,h,positions[p][0],positions[p][1]);
 }
}
static void all_paints(void)
{
 for(unsigned id=0;id<asset_count;id++)for(int paint=0;paint<6;paint++) {
  paint_case(assets[id],id,paint,29,44,9,5);
  paint_case(assets[id],id,paint,64,96,-17,-11);
  paint_case(assets[id],id,paint,31,47,WIDTH-13,HEIGHT-19);
 }
}
static void palette_contract(void)
{
 /* Fixed examples document glass/chrome preservation and the five paint
  * colors independently of the runtime lookup-table construction. */
 static const uint16_t colors[]={0,0x8c41,0xf7bd,0x8cde,0xe324,0xd20c,0xf7df};
 static const uint16_t values[6][6]={
  {0x8c41,0x8c41,0x8c41,0x8c41,0x8c41,0x8c41},
  {0xf7bd,0xf7bd,0xf7bd,0xf7bd,0xf7bd,0xf7bd},
  {0x8cde,0x9d37,0xd2c5,0x9677,0xdd53,0xc5ed},
  {0xe324,0xa57d,0xe766,0x9afd,0xf597,0xd670},
  {0xd20c,0xa15a,0xdf26,0x9aba,0xe975,0xce2e},
  {0xf7df,0xf7df,0xf7df,0xf7df,0xf7df,0xf7df}
 };
 for(int paint=0;paint<6;paint++) {
  fill_frame(&actual,991);fill_frame(&expected,991);
  Surface surface={actual.pixels,WIDTH,HEIGHT};
  gfx_vehicle(&surface,7,8,colors,7,1,7,1,paint);
  for(int n=0;n<6;n++)expected.pixels[8*WIDTH+8+n]=values[n][paint];
  compare("palette contract",0,paint,7,1,7,8);
 }
}
static void eviction_replay(void)
{
 /* More distinct keys than either cache pool can hold; revisit keys after
  * both small and large pools evict, with transparent holes and edge clips. */
 for(int pass=0;pass<3;pass++)for(unsigned id=0;id<asset_count;id++) {
  unsigned index=pass&1?asset_count-1-id:id;
  factory_case(assets[index],index,16,24,-3,7);
  factory_case(assets[index],index,64,96,24,-7);
 }
}
static void capacity_and_empty_cases(void)
{
 static uint16_t opaque[64*96],checker[64*96],empty[64*96];
 for(int y=0;y<96;y++)for(int x=0;x<64;x++) {
  opaque[y*64+x]=(uint16_t)(0x8000u|((x*43+y*79)&32767));
  checker[y*64+x]=((x+y)&1)?opaque[y*64+x]:0;
 }
 Asset samples[]={
  {opaque,64,96,0,-1,"full opaque capacity"},
  {checker,64,96,0,-1,"run overflow fallback"},
  {empty,64,96,0,-1,"empty alpha"}
 };
 for(unsigned n=0;n<sizeof samples/sizeof samples[0];n++) {
  factory_case(samples[n],200+n,32,64,4,2);
  factory_case(samples[n],200+n,64,96,-7,9);
  factory_case(samples[n],200+n,65,97,20,-9);
  for(int paint=0;paint<6;paint++)paint_case(samples[n],200+n,paint,64,96,-11,-13);
 }
 const int invisible[][4]={{-65,5,64,96},{WIDTH,0,32,48},{3,-96,64,96},
  {3,HEIGHT,32,48},{3,5,0,48},{3,5,32,0},{3,5,-1,48},{3,5,32,-1}};
 for(unsigned n=0;n<sizeof invisible/sizeof invisible[0];n++)
  factory_case(samples[0],200,invisible[n][2],invisible[n][3],invisible[n][0],invisible[n][1]);
}
int main(void)
{
 gfx_init();
 catalog();
 clean_vehicle_metadata();
 dimensional_texture_metadata();
 ground_metadata();
 all_scales_and_edges();
 all_paints();
 palette_contract();
 eviction_replay();
 capacity_and_empty_cases();
 printf("graphics: %u pixel-exact checks passed across %u asset frames; cache/paint/LOD/clipping/capacity\n",checks,asset_count);
 return 0;
}
