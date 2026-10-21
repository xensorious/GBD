/* Pixel oracle for rectangular native actors, continuous scenes and cache leases. */
#include <stdlib.h>
#include "../src/dialogue_render.c"

enum { W=256,H=192,GUARD=16 };
typedef struct {uint16_t before[GUARD],pixels[W*H],after[GUARD];} Frame;
static Frame actual,expected,actual_bottom,expected_bottom;
static uint16_t whole_scene[W*STORY_SCENE_HEIGHT];
static uint8_t fixture[STORY_PORTRAIT_WIDTH*STORY_PORTRAIT_HEIGHT];
static uint16_t fixture_palette[256];
static unsigned checks;
#define CHECK(v) do{checks++;if(!(v)){fprintf(stderr,"dialogue pixels:%d: %s\n",__LINE__,#v);exit(1);}}while(0)

static void fill(Frame*f){
 for(int i=0;i<GUARD;i++)f->before[i]=f->after[i]=(uint16_t)(0xa571+i);
 for(int i=0;i<W*H;i++)f->pixels[i]=(uint16_t)(0x8000|((i*71+19)&32767));
}

/* Visit screen pixels independently of the renderer's clipping and row loops.
 * At native size this compares each output pixel directly with its source. */
static void reference_frame(Frame*f,int screen_y,int clip_height,int x,int y,int width,int height,int opacity,int light){
 for(int row=0;row<clip_height;row++)for(int col=0;col<W;col++){
  int local_x=col-x,local_y=row+screen_y-y;
  if(local_x<0||local_x>=width||local_y<0||local_y>=height)continue;
  int sx=local_x*STORY_PORTRAIT_WIDTH/width,sy=local_y*STORY_PORTRAIT_HEIGHT/height;
  unsigned index=fixture[sy*STORY_PORTRAIT_WIDTH+sx];if(!index)continue;
  uint16_t src=fixture_palette[index],dst=f->pixels[row*W+col],out=0x8000;
  for(int shift=0;shift<=10;shift+=5){
   int lit=((src>>shift)&31)*light/256;
   int channel=(lit*opacity+((dst>>shift)&31)*(256-opacity))/256;
   out|=(uint16_t)(channel<<shift);
  }
  f->pixels[row*W+col]=out;
 }
}

static void pixel_checks(void){
 CHECK(STORY_PORTRAIT_WIDTH==256&&STORY_PORTRAIT_HEIGHT==264);
 for(int i=1;i<256;i++)fixture_palette[i]=(uint16_t)(0x8000|((i*127)&32767));
 for(int y=0;y<STORY_PORTRAIT_HEIGHT;y++)for(int x=0;x<STORY_PORTRAIT_WIDTH;x++)
  fixture[y*STORY_PORTRAIT_WIDTH+x]=(uint8_t)(x%11==0||y%13==0?0:1+(x+4*y)%255);
 people[0]=fixture;people_palette[0]=fixture_palette;
 const int sizes[][2]={{256,264},{180,186},{55,57},{74,103},{1,1},{256,1},{1,264}};
 const int xs[]={0,88,44,-47,210,300,-260},ys[]={24,23,-60,180,250,-270};
 const int opacities[]={256,210,0},lights[]={256,160};
 for(unsigned s=0;s<sizeof sizes/sizeof sizes[0];s++)
 for(unsigned ix=0;ix<sizeof xs/sizeof xs[0];ix++)
 for(unsigned iy=0;iy<sizeof ys/sizeof ys[0];iy++)
 for(unsigned op=0;op<sizeof opacities/sizeof opacities[0];op++)
 for(unsigned li=0;li<sizeof lights/sizeof lights[0];li++){
  fill(&actual);fill(&expected);
  reference_frame(&expected,0,H,xs[ix],ys[iy],sizes[s][0],sizes[s][1],opacities[op],lights[li]);
  Surface surface={actual.pixels,W,H};
  actor(&surface,0,xs[ix],ys[iy],sizes[s][0],sizes[s][1],opacities[op],lights[li]);
  CHECK(!memcmp(&actual,&expected,sizeof actual));
 }
 /* Oversized requests must not reintroduce an enlargement path. */
 fill(&actual);fill(&expected);Surface surface={actual.pixels,W,H};
 actor(&surface,0,0,0,257,264,256,256);CHECK(!memcmp(&actual,&expected,sizeof actual));
 actor(&surface,0,0,0,256,265,256,256);CHECK(!memcmp(&actual,&expected,sizeof actual));
 actor(&surface,0,0,0,0,264,256,256);CHECK(!memcmp(&actual,&expected,sizeof actual));
}

static void seam_checks(void){
 CHECK(STORY_SCENE_HEIGHT==288&&STORY_DIALOG_Y==96);
 const int sizes[][2]={{256,264},{180,186},{74,103},{1,1}};
 const int xs[]={0,-47,200},ys[]={-270,20,23,24,25,180,191,192,250,288,400};
 const int opacities[]={256,210,0},lights[]={256,160};
 for(unsigned s=0;s<sizeof sizes/sizeof sizes[0];s++)
 for(unsigned ix=0;ix<sizeof xs/sizeof xs[0];ix++)
 for(unsigned iy=0;iy<sizeof ys/sizeof ys[0];iy++)
 for(unsigned op=0;op<sizeof opacities/sizeof opacities[0];op++)
 for(unsigned li=0;li<sizeof lights/sizeof lights[0];li++){
  fill(&actual);fill(&expected);fill(&actual_bottom);fill(&expected_bottom);
  int x=xs[ix],y=ys[iy],width=sizes[s][0],height=sizes[s][1];
  reference_frame(&expected,0,H,x,y,width,height,opacities[op],lights[li]);
  reference_frame(&expected_bottom,H,96,x,y,width,height,opacities[op],lights[li]);
  Surface top={actual.pixels,W,H},bottom={actual_bottom.pixels,W,H};
  scene_actor(&top,&bottom,0,x,y,width,height,opacities[op],lights[li]);
  CHECK(top.w==W&&top.h==H&&bottom.w==W&&bottom.h==H);
  CHECK(!memcmp(&actual,&expected,sizeof actual));
  CHECK(!memcmp(&actual_bottom,&expected_bottom,sizeof actual_bottom));
 }
}

/* Labels and fades use the same world coordinates. Compare their split draw
 * against the ordinary graphics routines on one independent 288px surface. */
static void decoration_checks(void){
 const int xs[]={-7,5,250},ys[]={-8,187,192,283,288};
 for(unsigned ix=0;ix<sizeof xs/sizeof xs[0];ix++)
 for(unsigned iy=0;iy<sizeof ys/sizeof ys[0];iy++)
 for(int kind=0;kind<2;kind++){
  fill(&actual);fill(&expected);fill(&actual_bottom);fill(&expected_bottom);
  memcpy(whole_scene,expected.pixels,sizeof expected.pixels);
  memcpy(whole_scene+W*H,expected_bottom.pixels,W*96*sizeof(uint16_t));
  Surface whole={whole_scene,W,STORY_SCENE_HEIGHT};
  Surface top={actual.pixels,W,H},bottom={actual_bottom.pixels,W,H};
  if(kind){
   gfx_text_small(&whole,xs[ix],ys[iy],"Wide scene seam",C(217,201,133));
   scene_text(&top,&bottom,xs[ix],ys[iy],"Wide scene seam",C(217,201,133));
  }else{
   overlay(&whole,xs[ix],ys[iy],262,20,C(51,137,209),173);
   scene_overlay(&top,&bottom,xs[ix],ys[iy],262,20,C(51,137,209),173);
  }
  memcpy(expected.pixels,whole_scene,sizeof expected.pixels);
  memcpy(expected_bottom.pixels,whole_scene+W*H,W*96*sizeof(uint16_t));
  CHECK(!memcmp(&actual,&expected,sizeof actual));
  CHECK(!memcmp(&actual_bottom,&expected_bottom,sizeof actual_bottom));
 }
}

static uint32_t hash_pixels(const uint8_t*p,unsigned count){
 uint32_t value=2166136261u;for(unsigned i=0;i<count;i++)value=(value^p[i])*16777619u;return value;
}

static void lease_checks(void){
 unsigned bytes;uint8_t*base=gfx_story_scratch(&bytes);
 CHECK(bytes==307200);CHECK(lease());
 CHECK(sizeof back/sizeof back[0]==1&&sizeof back_palette/sizeof back_palette[0]==1);
 CHECK(sizeof people/sizeof people[0]==2&&sizeof people_palette/sizeof people_palette[0]==2);
 CHECK(back[0]==base&&people[0]==base+512*324&&people[1]==people[0]+256*264);
 CHECK((uint8_t*)(people_palette[1]+256)<=base+bytes);
 CHECK((unsigned)((uint8_t*)(people_palette[1]+256)-base)==302592);
 int protected_slot=person(0,-1);CHECK(protected_slot==0);
 unsigned count=STORY_PORTRAIT_WIDTH*STORY_PORTRAIT_HEIGHT;
 uint32_t pixels=hash_pixels(people[protected_slot],count);
 uint16_t palette[256];memcpy(palette,people_palette[protected_slot],sizeof palette);
 for(int id=1;id<STORY_PORTRAITS;id++){
  CHECK(person(id,0)==1);
  CHECK(hash_pixels(people[protected_slot],count)==pixels);
  CHECK(!memcmp(palette,people_palette[protected_slot],sizeof palette));
 }
 uint32_t loads=story_visual_stats[1];
 int first=background(0);CHECK(first==0&&story_visual_stats[1]==loads+1);
 CHECK(background(0)==0&&story_visual_stats[1]==loads+1);
 int second=background(1);CHECK(second==first&&story_visual_stats[1]==loads+2);
 CHECK(background(0)==0&&story_visual_stats[1]==loads+3);
 CHECK(hash_pixels(people[protected_slot],count)==pixels);
 CHECK(!memcmp(palette,people_palette[protected_slot],sizeof palette));
 CHECK(!story_visual_stats[0]);
 story_render_release();CHECK(!leased&&people_id[0]==-1&&people_id[1]==-1);
 CHECK(lease()&&background(0)==0&&story_visual_stats[1]==loads+4);
 story_render_release();
}

int main(void){
 pixel_checks();seam_checks();decoration_checks();lease_checks();
 printf("dialogue pixels: %u checks; rectangular native 1:1 pixels, depth reductions, clipping, opacity, lighting, continuous LCD seam, reserved lower UI and one-background/two-actor cache isolation pass\n",checks);
 return 0;
}
