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

enum { WIDTH=83,HEIGHT=109,GUARD=8,MAX_ASSETS=1024 };
typedef struct {
 uint16_t before[GUARD],pixels[WIDTH*HEIGHT],after[GUARD];
} GuardedFrame;
typedef struct {
 const uint16_t *pixels;
 int width,height,scaled_api,prop_id;
 const char *name;
 const uint8_t *indices;
 const uint16_t *palette;
} Asset;
static GuardedFrame actual,expected;
static Asset assets[MAX_ASSETS];
static unsigned asset_count,checks;
static uint16_t uncached_source[65*97];
static uint16_t *owned[MAX_ASSETS];
static unsigned owned_count;
static const uint16_t *vehicle_images[ASSET_VEHICLE_COUNT*5];
static const uint16_t *prop_images[4][ASSET_PROP_SPRITE_COUNT];
static const uint16_t *side_images[4][ASSET_PROP_SIDE_COUNT];
static const uint16_t *building_images[7][ASSET_BUILDING_COUNT];
static const uint16_t *boat_images[ASSET_BOAT_PROFILE_COUNT];

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
static void nearest_reference(Asset a,int w,int h,int x,int y)
{
 if(w<=0||h<=0)return;
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
 if(a.indices&&!paint)gfx_indexed_sprite(&surface,x,y,a.indices,a.palette,a.width,a.height,w,h);
 else if(a.scaled_api&&!paint)gfx_scaled_sprite(&surface,x,y,a.pixels,a.width,a.height,w,h);
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
 assets[asset_count++]=(Asset){pixels,width,height,scaled_api,prop_id,name,0,0};
}
static uint16_t *allocate_pixels(int count){
 uint16_t *pixels=malloc((size_t)count*sizeof(*pixels));
 if(!pixels||owned_count>=MAX_ASSETS)fail("native reference allocation",asset_count,0,0,0,0,0,0);
 owned[owned_count++]=pixels;return pixels;
}
static const uint16_t *add_indexed(const uint8_t *indices,const uint16_t *palette,int w,int h,const char *name){
 uint16_t *pixels=allocate_pixels(w*h);
 for(int at=0;at<w*h;at++)pixels[at]=palette[indices[at]];
 add_asset(pixels,w,h,1,-1,name);
 assets[asset_count-1].indices=indices;assets[asset_count-1].palette=palette;
 return pixels;
}
static void catalog(void)
{
 for(int i=0;i<ASSET_VEHICLE_COUNT*5;i++){
  uint16_t *pixels=allocate_pixels(48*72);
  memcpy(pixels,gfx_vehicle_pixels(i/5,i%5),48*72*sizeof(*pixels));
  vehicle_images[i]=pixels;add_asset(pixels,48,72,0,-1,"decoded vehicle pose");
 }
 for(int i=0;i<6;i++)add_asset(traffic_art+i*40*64,40,64,0,-1,"traffic");
 const uint8_t *prop_levels[]={prop_idx,prop_lod48_idx,prop_lod32_idx,prop_lod16_idx};
 const uint8_t *side_levels[]={prop_side_idx,prop_side_lod48_idx,prop_side_lod32_idx,prop_side_lod16_idx};
 const int widths[]={64,48,32,16};
 for(int level=0;level<4;level++){
  int w=widths[level],h=w*3/2;
  for(int i=0;i<ASSET_PROP_SPRITE_COUNT;i++)
   prop_images[level][i]=add_indexed(prop_levels[level]+i*w*h,prop_palette+i*256,w,h,"compact prop LOD");
  for(int id=0;id<ASSET_PROP_COUNT;id++)if(prop_side_slot[id]!=255){
   int side=prop_side_slot[id],slot=prop_slot[id];
   side_images[level][side]=add_indexed(side_levels[level]+side*w*h,prop_palette+slot*256,w,h,"mirrored prop LOD");
  }
 }
 const uint8_t *faces[]={building_wall_idx,building_end_idx,building_roof_idx,
                       building_wall_lod32_idx,building_wall_lod16_idx,building_end_lod16_idx,building_end_lod8_idx};
 const int fw[]={64,32,32,32,16,16,8},fh[]={64,64,32,32,16,32,16};
 for(int i=0;i<ASSET_BUILDING_COUNT;i++) {
  for(int face=0;face<7;face++)building_images[face][i]=add_indexed(faces[face]+i*fw[face]*fh[face],
                                            building_palette+i*256,fw[face],fh[face],"building face LOD");
 }
 for(int i=0;i<ASSET_BOAT_PROFILE_COUNT;i++)boat_images[i]=add_indexed(boat_profile_idx+i*64*48,boat_palette+i*256,64,48,"raised profile boat");
 add_asset(dock_segment,ASSET_DOCK_SEGMENT_W,ASSET_DOCK_SEGMENT_H,1,-1,"modular dock");
 if(asset_count!=ASSET_VEHICLE_COUNT*5+6+ASSET_PROP_SPRITE_COUNT*4+ASSET_PROP_SIDE_COUNT*4+ASSET_BUILDING_COUNT*7+ASSET_BOAT_PROFILE_COUNT+1)
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
  for(int id=0;id<count;id++) {
   const uint16_t *source=group?traffic_art+id*w*h:vehicle_images[id];
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
    /* Single-pixel diagonal mirror stems are connected in pixel artwork. */
    const int dx[]={-1,1,0,0,-1,-1,1,1},dy[]={0,0,-1,1,-1,1,-1,1};
    for(int n=0;n<8;n++) {
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
 const int lengths[]={64*64,32*64,32*32,32*32,16*16,16*32,8*16};
 for(int face=0;face<7;face++)for(int id=0;id<ASSET_BUILDING_COUNT;id++){
  for(int p=0;p<lengths[face];p++)if(!building_images[face][id][p])
   fail("wall/roof face contains a transparent seam",(unsigned)id,face,0,0,0,0,p);
  checks++;
 }
 for(int p=0;p<64*32;p++)if(!dock_segment[p])
  fail("modular dock contains a transparent seam",0,0,64,32,0,0,p);
 checks++;
 for(int boat=0;boat<ASSET_BOAT_PROFILE_COUNT;boat++){
  int first=48,last=-1,left=64,right=-1;
  for(int y=0;y<48;y++)for(int x=0;x<64;x++)if(boat_images[boat][y*64+x]){
   if(y<first)first=y;
   if(y>last)last=y;
   if(x<left)left=x;
   if(x>right)right=x;
  }
  int ax=boat_profile_anchor[boat*2],ay=boat_profile_anchor[boat*2+1];
  /* A sailboat may have a tall mast; every hull is still anchored to its
   * lowest waterline instead of flattened into a top-down water decal. */
  if(last-first<10||right-left<16||ay!=last||ax<left||ax>right)
   fail("boat profile/waterline anchor",(unsigned)boat,0,64,48,left,first,last);
  checks++;
 }
}
static void ground_metadata(void)
{
 const uint8_t *all_bounds[]={prop_bounds,prop_lod48_bounds,prop_lod32_bounds,prop_lod16_bounds};
 const uint8_t *side_bounds[]={prop_side_bounds,prop_side_lod48_bounds,prop_side_lod32_bounds,prop_side_lod16_bounds};
 const int widths[]={64,48,32,16};
 unsigned slots[ASSET_PROP_SPRITE_COUNT]={0},sides[ASSET_PROP_SIDE_COUNT]={0};
 for(int id=0;id<ASSET_PROP_COUNT;id++){
  int slot=prop_slot[id],side=prop_side_slot[id];
  if(slot!=255){
   if(slot>=ASSET_PROP_SPRITE_COUNT||slots[slot]++)fail("invalid compact prop slot",(unsigned)id,0,0,0,slot,0,0);
   int anchor_x=prop_anchor[id*2],anchor_y=prop_anchor[id*2+1],lowest=-1;
   if(anchor_x>=64||anchor_y>=96)fail("ground anchor outside sprite",(unsigned)id,0,64,96,anchor_x,anchor_y,0);
   for(int row=0;row<96;row++)for(int col=0;col<64;col++)if(prop_images[0][slot][row*64+col])lowest=row;
   if(lowest<0||anchor_y<lowest-4||anchor_y>lowest+1)
    fail("ground anchor above foot",(unsigned)id,0,64,96,anchor_x,anchor_y,lowest);
   checks++;
   for(int level=0;level<4;level++){
    int w=widths[level],h=w*3/2,left=w,right=-1,top=h,bottom=-1;
    const uint16_t *source=prop_images[level][slot];
    for(int row=0;row<h;row++)for(int col=0;col<w;col++)if(source[row*w+col]){
     if(col<left)left=col;
     if(col>right)right=col;
     if(row<top)top=row;
     if(row>bottom)bottom=row;
    }
    const uint8_t *bounds=all_bounds[level]+id*4;
    if(bounds[0]!=left||bounds[1]!=top||bounds[2]!=right+1||bounds[3]!=bottom+1)
     fail("compact silhouette bounds",(unsigned)id,level,w,h,left,top,0);
    checks++;
   }
  }
  if(side!=255){
   if(side>=ASSET_PROP_SIDE_COUNT||sides[side]++||slot==255)
    fail("invalid compact side slot",(unsigned)id,0,0,0,side,0,0);
   if(prop_side_anchor[side*2]!=63-prop_anchor[id*2]||prop_side_anchor[side*2+1]!=prop_anchor[id*2+1])
    fail("opposite shoulder anchor mirror",(unsigned)id,0,64,96,side,0,0);
   for(int level=0;level<4;level++){
    int w=widths[level],h=w*3/2;
    const uint8_t *original=all_bounds[level]+id*4,*mirror=side_bounds[level]+side*4;
    if(mirror[0]!=w-original[2]||mirror[2]!=w-original[0]||mirror[1]!=original[1]||mirror[3]!=original[3])
     fail("opposite shoulder bounds mirror",(unsigned)id,level,w,h,side,0,0);
    for(int row=0;row<h;row++)for(int col=0;col<w;col++)
     if(side_images[level][side][row*w+col]!=prop_images[level][slot][row*w+w-1-col])
      fail("opposite shoulder indexed art mirror",(unsigned)id,level,w,h,col,row,side);
    checks++;
   }
  }
 }
 for(int slot=0;slot<ASSET_PROP_SPRITE_COUNT;slot++)if(slots[slot]!=1)
  fail("unmapped compact prop sprite",(unsigned)slot,0,0,0,0,0,0);
 for(int side=0;side<ASSET_PROP_SIDE_COUNT;side++)if(sides[side]!=1)
  fail("unmapped compact side sprite",(unsigned)side,0,0,0,0,0,0);
 static const int omitted[]={0,1,2,9,10,11,14,16,17};
 for(unsigned id=0;id<sizeof omitted/sizeof *omitted;id++)if(prop_slot[omitted[id]]!=255)
  fail("unused billboard or old facade occupies texture memory",id,0,0,0,omitted[id],0,0);
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
/* Independent pre-optimization formula. A mutable width65 source bypasses
 * opaque sprite caching, so every RGB555 color tests the live paint path. */
static uint16_t old_paint_formula(uint16_t c,int paint)
{
 if(!paint)return c;
 int r=c&31,g=(c>>5)&31,b=(c>>10)&31;
 int hi=r>g?r:g;if(b>hi)hi=b;
 int lo=r<g?r:g;if(b<lo)lo=b;
 if(hi<14||hi-lo<7||lo>22)return c;
 static const int colors[6][3]={{31,31,31},{31,12,10},{7,29,27},{31,25,7},{25,13,31},{17,20,23}};
 int light=(r*2+g*3+b)/6+4;
 r=colors[paint][0]*light/22;g=colors[paint][1]*light/22;b=colors[paint][2]*light/22;
 if(r>31)r=31;
 if(g>31)g=31;
 if(b>31)b=31;
 return (uint16_t)(0x8000|r|(g<<5)|(b<<10));
}
static void all_rgb555_paints(void)
{
 uint16_t source[65]={0},target[65]={0};Surface surface={target,65,1};
 for(int paint=0;paint<6;paint++){
  for(unsigned rgb=0;rgb<32768;rgb++){
   source[0]=(uint16_t)(0x8000u|rgb);target[0]=0;
   gfx_vehicle(&surface,0,0,source,65,1,65,1,paint);
   if(target[0]!=old_paint_formula(source[0],paint))
    fail("all RGB555 paint formula",rgb,paint,65,1,0,0,target[0]);
   checks++;
  }
  source[0]=0;target[0]=0xa571u;
  gfx_vehicle(&surface,0,0,source,65,1,65,1,paint);
  if(target[0]!=0xa571u)fail("transparent paint source",0,paint,65,1,0,0,target[0]);
  checks++;
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
  {opaque,64,96,0,-1,"full opaque capacity",0,0},
  {checker,64,96,0,-1,"run overflow fallback",0,0},
  {empty,64,96,0,-1,"empty alpha",0,0}
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
 all_rgb555_paints();
 eviction_replay();
 capacity_and_empty_cases();
 for(unsigned i=0;i<owned_count;i++)free(owned[i]);
 printf("graphics: %u pixel-exact checks passed across %u asset frames; indexed/RGB555 cache/paint/LOD/clipping/capacity\n",checks,asset_count);
 return 0;
}
