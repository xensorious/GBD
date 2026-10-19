/* Actual indexed vehicle assets, cosmetic materials and reused-source cache
 * checks. No game state, emulator memory writes or hand-painted fixtures. */
#include "assets.h"
#include "game.h"
#include "gfx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

_Static_assert(ASSET_VEHICLE_COUNT==VEHICLE_COUNT,"Style tests must cover the complete roster");

#define W 96
#define H 144
#define PIXELS (48*72)
#define STYLES 33
static unsigned long long checks;
#define CHECK(c) do {++checks;if(!(c)){fprintf(stderr,"style:%d: %s\n",__LINE__,#c);exit(1);}}while(0)
static const uint16_t background=C(29,48,61);
static uint16_t screen[W*H],reference[PIXELS];
static uint32_t recorded[VEHICLE_COUNT][5][STYLES][3];
static int context_vehicle,context_pose,context_style;

typedef struct {int paint,livery,wheels;} Style;
static const Style styles[STYLES]={
 {0,0,0},{1,0,0},{2,0,0},{3,0,0},{4,0,0},{5,0,0},
 {0,1,0},{0,2,0},{0,3,0},{0,4,0},
 {0,0,1},{0,0,2},{0,0,3},{2,3,1},{4,4,3},
 {6,0,0},{7,0,0},{8,0,0},{9,0,0},{10,0,0},{11,0,0},{12,0,0},{13,0,0},{14,0,0},{15,0,0},
 {6,1,0},{6,2,0},{6,3,0},{6,4,0},{5,1,0},{5,2,0},{5,3,0},{5,4,0}
};

static void clear(void){for(int i=0;i<W*H;i++)screen[i]=background;}
static uint32_t hash_screen(void){
 uint32_t hash=2166136261u;
 for(int i=0;i<W*H;i++){hash^=screen[i];hash*=16777619u;}
 return hash;
}

static void source_case(int vehicle,int pose){
 const uint16_t *source=gfx_vehicle_pixels(vehicle,pose);
 const uint8_t *indices=vehicle_indexed+(vehicle*5+pose)*PIXELS;
 const uint16_t *palette=vehicle_palette+vehicle*256;
 const uint8_t *flags=vehicle_palette_flags+vehicle*256;
 CHECK(palette[0]==0 && flags[0]==0);
 for(int i=0;i<256;i++)if(!(flags[i]&1u))CHECK(!(flags[i]&0xfcu));
 /* Authored neutral rear lamp samples: purple Hellcat, blue Charger,
    blue Bugatti and silver Rolls must keep red emitters through repaint. */
 static const int lamps[][3]={{31,12,47},{35,10,45},{39,12,45},{41,11,47}};
 if(pose==2)for(unsigned n=0;n<sizeof lamps/sizeof lamps[0];n++)if(vehicle==lamps[n][0]){
  int at=lamps[n][2]*48+lamps[n][1];unsigned color=palette[indices[at]];
  CHECK((color&31)>15 && (color&31)>((color>>5)&31)*2);
  CHECK(flags[indices[at]]==0);
 }
 /* Paired semantic probes: the lamp stays factory-colored while adjacent
  * quarter panels, fascia, wings and tailgates must take the chosen paint. */
 static const int panels[][3]={{0,9,46},{0,12,46},{6,9,50},{6,15,52},{14,24,50},{16,24,42},{24,24,43},{31,10,43},{32,15,48},{38,24,43},{38,24,46},{38,24,48},{13,24,25},{20,24,24},{21,24,27},{23,23,28},{40,24,22}};
 if(pose==2)for(unsigned n=0;n<sizeof panels/sizeof panels[0];n++)if(vehicle==panels[n][0])CHECK(flags[indices[panels[n][2]*48+panels[n][1]]]&1u);
 if(vehicle==32&&pose==2)CHECK(flags[indices[47*48+18]]==0); /* white reverse lens */
 unsigned paintable=0,hardware=0,protected_pixels=0;
 for(int at=0;at<PIXELS;at++){
  uint16_t color=palette[indices[at]];
  unsigned role=vehicle_roles[(vehicle*5+pose)*PIXELS+at];
  CHECK((flags[indices[at]]&3u)==(role==3?0u:role));
  CHECK(source[at]==color);
  CHECK(!color || (color&0x8000u));
  if(color){paintable+=(flags[indices[at]]&1u)!=0;hardware+=(flags[indices[at]]&2u)!=0;protected_pixels+=flags[indices[at]]==0;}
 }
 if(!paintable || !hardware)fprintf(stderr,"Missing material vehicle=%d pose=%d paint=%u hardware=%u\n",vehicle,pose,paintable,hardware);
 CHECK(paintable>0 && hardware>0 && protected_pixels>0);
}

static uint32_t full_size_case(int vehicle,int pose,int style){
 const uint16_t *source=gfx_vehicle_pixels(vehicle,pose);
 const uint8_t *indices=vehicle_indexed+(vehicle*5+pose)*PIXELS;
 const uint16_t *palette=vehicle_palette+vehicle*256;
 const uint8_t *flags=vehicle_palette_flags+vehicle*256;
 Style chosen=styles[style];
 Surface surface={screen,W,H};clear();
 gfx_vehicle_style(&surface,8,12,source,48,72,48,72,chosen.paint,chosen.livery,chosen.wheels,0);
 unsigned changed=0;
 for(int y=0;y<72;y++)for(int x=0;x<48;x++){
  int at=y*48+x;uint16_t original=palette[indices[at]],actual=screen[(y+12)*W+x+8];
  unsigned material=flags[indices[at]];
  int permitted=((material&1u)&&(chosen.paint||chosen.livery)) || ((material&2u)&&chosen.wheels);
  if(!original)CHECK(actual==background);
  else{
   CHECK(actual&0x8000u);
   if(!permitted)CHECK(actual==original);
   if(chosen.paint==6&&(material&1u)&&!(chosen.livery&&(material&(1u<<(chosen.livery+1))))){
    /* A dark body panel cannot retain a bright red/orange factory patch. */
    CHECK((actual&31)<=3 && ((actual>>5)&31)<=4 && ((actual>>10)&31)<=5);
   }
   changed+=actual!=original;
  }
  reference[at]=original?actual:0;
 }
 if(style && !changed)fprintf(stderr,"Invisible cosmetic vehicle=%d pose=%d style=%d\n",vehicle,pose,style);
 CHECK(style?changed>0:changed==0);
 return hash_screen();
}

static uint32_t scaled_case(int vehicle,int pose,int style,int dw,int dh,int left,int top){
 Surface surface={screen,W,H};clear();Style chosen=styles[style];
 const uint16_t *source=gfx_vehicle_pixels(vehicle,pose);
 gfx_vehicle_style(&surface,left,top,source,48,72,dw,dh,chosen.paint,chosen.livery,chosen.wheels,0);
 for(int y=0;y<H;y++)for(int x=0;x<W;x++){
  uint16_t expected=background;
  if(x>=left && x<left+dw && y>=top && y<top+dh){
   uint16_t color=reference[((y-top)*72/dh)*48+(x-left)*48/dw];
   if(color)expected=color;
  }
  if(screen[y*W+x]!=expected)fprintf(stderr,"Scaled style mismatch vehicle=%d pose=%d style=%d at%d,%d size%d,%d\n",context_vehicle,context_pose,context_style,x,y,dw,dh);
  CHECK(screen[y*W+x]==expected);
 }
 return hash_screen();
}

static void style_case(int vehicle,int pose,int style,int repeat){
 context_vehicle=vehicle;context_pose=pose;context_style=style;
 uint32_t hashes[3];
 hashes[0]=full_size_case(vehicle,pose,style);
 hashes[1]=scaled_case(vehicle,pose,style,29,42,-3,9);
 hashes[2]=scaled_case(vehicle,pose,style,78,116,7,-5);
 for(int i=0;i<3;i++){
  if(repeat)CHECK(recorded[vehicle][pose][style][i]==hashes[i]);
  else recorded[vehicle][pose][style][i]=hashes[i];
 }
}

static void indexed_palette_identity(void){
 static const uint8_t indices[4]={0,1,1,0};
 static const uint16_t palette_a[256]={[1]=C(224,46,53)},palette_b[256]={[1]=C(41,79,220)};
 Surface surface={screen,W,H};
 for(int n=0;n<80;n++){
  const uint16_t *palette=(n&1)?palette_a:palette_b;
  clear();gfx_indexed_sprite(&surface,3,7,indices,palette,2,2,16,16);
  CHECK(screen[7*W+3]==background);
  CHECK(screen[7*W+11]==palette[1]);
 }
}

int main(int argc,char**argv){
 for(int vehicle=0;vehicle<VEHICLE_COUNT;vehicle++)for(int pose=0;pose<5;pose++){
  source_case(vehicle,pose);
  for(int style=0;style<STYLES;style++)style_case(vehicle,pose,style,0);
 }
 /* Every ID/pose/style switches through the same pair of backing pointers.
  * Reverse traversal catches stale pixels in both large and small caches. */
 for(int vehicle=VEHICLE_COUNT-1;vehicle>=0;vehicle--)for(int pose=4;pose>=0;pose--)
  for(int style=STYLES-1;style>=0;style--)style_case(vehicle,pose,style,1);
 indexed_palette_identity();
 if(argc>1){
  static uint16_t gallery[384*4608];Surface sheet={gallery,384,4608};gfx_rect(&sheet,0,0,384,4608,C(108,128,139));
  static const char*labels[8]={"FACTORY","BLACK","GRAPHITE","TWIN","TWO TONE","VELOCITY","CONTOUR","MATERIAL"};
  const int paints[8]={0,6,5,6,6,4,5,0},patterns[8]={0,0,0,1,2,3,4,0};
  for(int v=0;v<VEHICLE_COUNT;v++)for(int col=0;col<8;col++){
   int xx=col*48,yy=v*96;char title[24];snprintf(title,sizeof title,"%02d %s",v,labels[col]);gfx_text_small(&sheet,xx,yy+2,title,C(239,246,245));
   gfx_vehicle_style(&sheet,xx,yy+18,gfx_vehicle_pixels(v,2),48,72,48,72,paints[col],patterns[col],0,0);
   if(col==7){static const uint16_t colors[4]={C(18,30,39),C(64,203,225),C(255,184,82),C(255,57,57)};
    const uint8_t*roles=vehicle_roles+(v*5+2)*PIXELS;const uint16_t*source=gfx_vehicle_pixels(v,2);
    for(int at=0;at<PIXELS;at++)if(source[at])gallery[(yy+18+at/48)*384+xx+at%48]=colors[roles[at]];
   }
  }
  char path[512];snprintf(path,sizeof path,"%s/paint-review.ppm",argv[1]);FILE*f=fopen(path,"wb");CHECK(f!=0);fprintf(f,"P6\n384 4608\n255\n");
  for(int i=0;i<384*4608;i++){unsigned c=gallery[i];fputc((c&31)*255/31,f);fputc((c>>5&31)*255/31,f);fputc((c>>10&31)*255/31,f);}fclose(f);
 }

 printf("style: %llu checks; %d vehicles x5 poses x%d cosmetic cases, exact alpha/protected materials, visible hardware and source/palette cache invalidation pass\n",checks,VEHICLE_COUNT,STYLES);
 return 0;
}
