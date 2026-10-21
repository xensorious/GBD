/* Pixel oracle for native speaking portraits, reduced actors and cache leases. */
#include <stdlib.h>
#include "../src/dialogue_render.c"

enum { W=256,H=192,GUARD=16 };
typedef struct {uint16_t before[GUARD],pixels[W*H],after[GUARD];} Frame;
static Frame actual,expected;
static uint8_t fixture[STORY_PORTRAIT_SIZE*STORY_PORTRAIT_SIZE];
static uint16_t fixture_palette[256];
static unsigned checks;
#define CHECK(v) do{checks++;if(!(v)){fprintf(stderr,"dialogue pixels:%d: %s\n",__LINE__,#v);exit(1);}}while(0)

static void fill(Frame*f){
 for(int i=0;i<GUARD;i++)f->before[i]=f->after[i]=(uint16_t)(0xa571+i);
 for(int i=0;i<W*H;i++)f->pixels[i]=(uint16_t)(0x8000|((i*71+19)&32767));
}

/* Visit screen pixels independently of the renderer's clipping and row loops.
 * At native size this compares each output pixel directly with its source. */
static void reference(int x,int y,int width,int height,int opacity,int light){
 for(int row=0;row<H;row++)for(int col=0;col<W;col++){
  int local_x=col-x,local_y=row-y;
  if(local_x<0||local_x>=width||local_y<0||local_y>=height)continue;
  int sx=local_x*STORY_PORTRAIT_SIZE/width,sy=local_y*STORY_PORTRAIT_SIZE/height;
  unsigned index=fixture[sy*STORY_PORTRAIT_SIZE+sx];if(!index)continue;
  uint16_t src=fixture_palette[index],dst=expected.pixels[row*W+col],out=0x8000;
  for(int shift=0;shift<=10;shift+=5){
   int lit=((src>>shift)&31)*light/256;
   int channel=(lit*opacity+((dst>>shift)&31)*(256-opacity))/256;
   out|=(uint16_t)(channel<<shift);
  }
  expected.pixels[row*W+col]=out;
 }
}

static void pixel_checks(void){
 CHECK(STORY_PORTRAIT_SIZE==168);
 for(int i=1;i<256;i++)fixture_palette[i]=(uint16_t)(0x8000|((i*127)&32767));
 for(int y=0;y<STORY_PORTRAIT_SIZE;y++)for(int x=0;x<STORY_PORTRAIT_SIZE;x++)
  fixture[y*STORY_PORTRAIT_SIZE+x]=(uint8_t)(x%11==0||y%13==0?0:1+(x+4*y)%255);
 people[0]=fixture;people_palette[0]=fixture_palette;
 const int sizes[][2]={{168,168},{121,121},{55,55},{74,103},{1,1}};
 const int xs[]={0,88,44,-47,210,300,-180},ys[]={24,23,-60,180,250,-170};
 const int opacities[]={256,210,0},lights[]={256,160};
 for(unsigned s=0;s<sizeof sizes/sizeof sizes[0];s++)
 for(unsigned ix=0;ix<sizeof xs/sizeof xs[0];ix++)
 for(unsigned iy=0;iy<sizeof ys/sizeof ys[0];iy++)
 for(unsigned op=0;op<sizeof opacities/sizeof opacities[0];op++)
 for(unsigned li=0;li<sizeof lights/sizeof lights[0];li++){
  fill(&actual);fill(&expected);
  reference(xs[ix],ys[iy],sizes[s][0],sizes[s][1],opacities[op],lights[li]);
  Surface surface={actual.pixels,W,H};
  actor(&surface,0,xs[ix],ys[iy],sizes[s][0],sizes[s][1],opacities[op],lights[li]);
  CHECK(!memcmp(&actual,&expected,sizeof actual));
 }
 /* Oversized requests must not reintroduce an enlargement path. */
 fill(&actual);fill(&expected);Surface surface={actual.pixels,W,H};
 actor(&surface,0,0,0,169,169,256,256);CHECK(!memcmp(&actual,&expected,sizeof actual));
 actor(&surface,0,0,0,0,168,256,256);CHECK(!memcmp(&actual,&expected,sizeof actual));
}

static uint32_t hash_pixels(const uint8_t*p,unsigned count){
 uint32_t value=2166136261u;for(unsigned i=0;i<count;i++)value=(value^p[i])*16777619u;return value;
}

static void lease_checks(void){
 unsigned bytes;uint8_t*base=gfx_story_scratch(&bytes);
 CHECK(bytes==307200);CHECK(lease());
 CHECK(back[0]==base&&people[0]==base+2*512*216);
 CHECK((uint8_t*)(people_palette[1]+256)<=base+bytes);
 CHECK((unsigned)((uint8_t*)(people_palette[1]+256)-base)==279680);
 int protected_slot=person(0,-1);CHECK(protected_slot==0);
 unsigned count=STORY_PORTRAIT_SIZE*STORY_PORTRAIT_SIZE;
 uint32_t pixels=hash_pixels(people[protected_slot],count);
 uint16_t palette[256];memcpy(palette,people_palette[protected_slot],sizeof palette);
 for(int id=1;id<STORY_PORTRAITS;id++){
  CHECK(person(id,0)==1);
  CHECK(hash_pixels(people[protected_slot],count)==pixels);
  CHECK(!memcmp(palette,people_palette[protected_slot],sizeof palette));
 }
 int first=background(0);CHECK(first>=0);background_slot=first;
 int second=background(1);CHECK(second>=0&&second!=first);
 CHECK(hash_pixels(people[protected_slot],count)==pixels);
 CHECK(!story_visual_stats[0]);
 story_render_release();CHECK(!leased&&people_id[0]==-1&&back_id[0]==-1);
}

int main(void){
 pixel_checks();lease_checks();
 printf("dialogue pixels: %u checks; native 1:1 pixels, depth/thumbnail reductions, clipping, opacity, lighting and two-background/two-portrait cache isolation pass\n",checks);
 return 0;
}
