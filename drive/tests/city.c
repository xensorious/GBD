#include "game.h"
#include "city.h"
#include "city_render.h"
#include "render.h"
#include "assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"city:%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void press(Game*g,unsigned key){Input in={key,key};game_update(g,&in);}
static void ticks(Game*g,int n,unsigned held){while(n--){Input in={held,0};game_update(g,&in);}}
static uint16_t top[256*192],bottom[256*192];
static void shot(const char*dir,const char*name,Game*g){
 render_game(g,top,bottom);if(!dir)return;char path[512];snprintf(path,sizeof path,"%s/%s.ppm",dir,name);FILE*f=fopen(path,"wb");CHECK(f!=0);fprintf(f,"P6\n256 384\n255\n");
 for(int screen=0;screen<2;screen++)for(int i=0;i<256*192;i++){unsigned c=screen?bottom[i]:top[i];fputc((c&31)*255/31,f);fputc((c>>5&31)*255/31,f);fputc((c>>10&31)*255/31,f);}
 fclose(f);
}
/* Decode the packed material oracle. Color equality is insufficient: a
 * painted gray panel may legitimately share RGB555 with unpainted chrome. */
static void decode_overhead(int v,uint8_t*out){
 uint8_t decoded[3*48*72];const uint8_t*src=overhead_lz+overhead_offsets[v];unsigned in=4,pos=0,size=overhead_offsets[v+1]-overhead_offsets[v];
 CHECK(src[0]==0x10 && (src[1]|src[2]<<8|src[3]<<16)==3*48*72);
 while(pos<sizeof decoded){CHECK(in<size);unsigned flags=src[in++];
  for(unsigned bit=128;bit&&pos<sizeof decoded;bit>>=1){
   if(flags&bit){CHECK(in+2<=size);unsigned code=src[in]<<8|src[in+1];in+=2;unsigned n=(code>>12)+3,back=(code&4095)+1;
    CHECK(back<=pos&&n<=sizeof decoded-pos);while(n--){decoded[pos]=decoded[pos-back];pos++;}
   }else{CHECK(in<size);decoded[pos++]=src[in++];}
  }
 }
 CHECK(!memcmp(decoded,overhead_indexed+v*3*48*72,sizeof decoded));memcpy(out,decoded+48*72,48*72);
}
/* Independent contact oracle: clip one chassis polygon to the other and
 * require positive intersection area. Production uses separating axes. */
typedef struct {double x,y;} ContactPoint;
static void chassis(ContactPoint*p,int x,int y,int angle,int model,int margin){
 CityShape shape=city_vehicle_shape(model);double cs=city_cos(angle)/256.0,sn=city_sin(angle)/256.0;
 int w=shape.half_width+margin,l=shape.half_length+margin;
 const int side[4]={-1,1,1,-1},fore[4]={-1,-1,1,1};
 for(int i=0;i<4;i++)p[i]=(ContactPoint){x+side[i]*w*cs-fore[i]*l*sn,y+side[i]*w*sn+fore[i]*l*cs};
}
static double contact_side(ContactPoint a,ContactPoint b,ContactPoint p){
 return (b.x-a.x)*(p.y-a.y)-(b.y-a.y)*(p.x-a.x);
}
static int contact_oracle(int ax,int ay,int angle,int model,int bx,int by,int bangle,int bmodel,int margin){
 ContactPoint input[16],output[16],clip[4];chassis(input,ax,ay,angle,model,margin);chassis(clip,bx,by,bangle,bmodel,margin);
 int count=4;
 for(int edge=0;edge<4&&count;edge++){
  int n=0;ContactPoint previous=input[count-1];double before=contact_side(clip[edge],clip[(edge+1)%4],previous);
  for(int i=0;i<count;i++){
   ContactPoint current=input[i];double after=contact_side(clip[edge],clip[(edge+1)%4],current);
   if((before<0)!=(after<0)){
    double t=before/(before-after);
    output[n++]=(ContactPoint){previous.x+(current.x-previous.x)*t,previous.y+(current.y-previous.y)*t};
   }
   if(after>=0)output[n++]=current;
   previous=current;before=after;
  }
  count=n;memcpy(input,output,(unsigned)count*sizeof input[0]);
 }
 double area=0;for(int i=0;i<count;i++)area+=input[i].x*input[(i+1)%count].y-input[i].y*input[(i+1)%count].x;
 return area>0.00001||area< -0.00001;
}
static void contact_checks(void){
 for(int model=0;model<VEHICLE_COUNT;model++){
  CityShape shape=city_vehicle_shape(model);
  CHECK(shape.draw_half_width<16&&shape.draw_half_length<24);
  CHECK(shape.half_width<shape.draw_half_width&&shape.half_length<shape.draw_half_length);
  CHECK(!city_vehicle_overlap(640+13,1500,0,model,640-13,1500,128,model,0));
 }
 /* A standard car can just pass between adjacent highway lanes; large
  * armored trucks leave a smaller gap that requires a narrower vehicle. */
 CHECK(!city_vehicle_overlap(640+28,1500,0,1,640+14,1500,0,1,0));
 CHECK(!city_vehicle_overlap(640+28,1500,0,1,640+42,1500,0,1,0));
 CHECK(city_vehicle_overlap(640+28,1500,0,46,640+14,1500,0,46,0));
 CHECK(!city_vehicle_overlap(640+28,1500,0,3,640+14,1500,0,46,0));
 CHECK(city_vehicle_overlap(1000,1000,64,0,1000,1000,0,0,0));
 for(int a=0;a<VEHICLE_COUNT;a++)for(int b=0;b<VEHICLE_COUNT;b++)
  for(int angle=0;angle<256;angle+=32)for(int dy=-40;dy<=40;dy+=10)for(int dx=-30;dx<=30;dx+=10){
   int other=(angle*7+b*13)&255,margin=(dx+dy)%3==0?5:0;
   int actual=city_vehicle_overlap(2000,2000,angle,a,2000+dx,2000+dy,other,b,margin);
   int expected=contact_oracle(2000,2000,angle,a,2000+dx,2000+dy,other,b,margin);
   if(actual!=expected)fprintf(stderr,"contact %d/%d @%d/%d offset %d,%d margin %d\n",a,b,angle,other,dx,dy,margin);
   CHECK(actual==expected);
  }
}
#include "city_sun.inc"
int main(int argc,char**argv){
 sunlight_checks();
 contact_checks();
 /* Vehicle heading is the view heading through the full wraparound. World
  * translations cancel exactly, height rises toward screen-up, and forward
  * motion always projects upward with no lateral component. */
 for(int a=0;a<256;a++){
  int center[3],front[3],raised[3],shift[3];
  city_project_point(2048,2048,0,2048,2048,a,center);
  city_project_point(2048+city_sin(a),2048-city_cos(a),0,2048,2048,a,front);
  city_project_point(2048,2048,24,2048,2048,a,raised);
  city_project_point(2048+901,2048-617,0,2048+901,2048-617,a,shift);
  CHECK(center[0]==128*16&&center[1]==112*16);
  CHECK(front[0]==center[0]&&front[1]<center[1]-180*16);
  CHECK(raised[0]==center[0]&&raised[1]==center[1]-12*16*CITY_ZOOM_NUM/CITY_ZOOM_DEN&&raised[2]>center[2]);
  CHECK(!memcmp(center,shift,sizeof center));
 }
 /* Real ground polygons remain fixed after scrolling with a rotated view.
  * Compare common world pixels at all four cardinal headings, including
  * highway dash periods, crosswalks, ends, canal banks and map borders. */
 const int ground_sites[][2]={{654,1664},{1280,1408},{2080,2048},{2496,1900},{3600,600},{654,530}};
 for(unsigned site=0;site<sizeof ground_sites/sizeof ground_sites[0];site++)for(int a=0;a<256;a+=64){
  Surface first={top,256,192},second={bottom,256,192};int cx=ground_sites[site][0],cy=ground_sites[site][1];
  city_draw_rotated_ground(&first,cx,cy,a);
  city_draw_rotated_ground(&second,cx+city_cos(a)/8,cy+city_sin(a)/8,a);
  int shift=32*CITY_ZOOM_NUM/CITY_ZOOM_DEN;
  for(int y=0;y<192;y++)for(int x=0;x<256-shift;x++)CHECK(top[y*256+x+shift]==bottom[y*256+x]);
 }
 const char*dir=argc>1?argv[1]:0;Game g;game_init(&g,123);press(&g,K_A);CHECK(g.mode==M_DRIVE&&!city.active);shot(dir,"mode-select",&g);
 press(&g,K_B);CHECK(g.mode==M_HOME);press(&g,K_A);press(&g,K_RIGHT);press(&g,K_A);CHECK(city.active&&g.mode==M_RUN&&g.vehicle==0);
 CHECK(!city_solid(city.x/Q,city.y/Q));int initial_y=city.y;ticks(&g,60,K_R);CHECK(g.mode==M_RUN&&city.y<initial_y&&g.speed_q8>0&&g.run.distance_m>0);shot(dir,"city-highway",&g);
 int x=city.x,y=city.y;press(&g,K_START);ticks(&g,30,K_R);CHECK(city.x==x&&city.y==y&&g.mode==M_PAUSE);press(&g,K_B);
 city.x=1280*Q;city.y=1200*Q;city.vx=city.vy=0;city.angle=0;city.camera_x=city.x;city.camera_y=city.y;g.speed_q8=0;g.health=100;ticks(&g,30,K_R);shot(dir,"city-neighborhood",&g);
 int angle=city.angle;ticks(&g,12,K_R|K_RIGHT);CHECK(city.angle!=angle);ticks(&g,20,K_L);CHECK(g.speed_q8<100*Q);
 city.x=2080*Q;city.y=2061*Q;city.angle=64;city.camera_x=city.x;city.camera_y=city.y;city.vx=city.vy=0;g.speed_q8=0;CHECK(!city_solid(2080,2061)&&city_water(2080,2200)&&city_solid(2080,2200));shot(dir,"city-bridge",&g);
 /* Geometry has one fixed drivable connected component at 16px sampling. */
 static unsigned char seen[256*256];static unsigned short queue[256*256];unsigned head=0,tail=0;queue[tail++]=(unsigned short)(104*256+41);seen[queue[0]]=1;
 while(head<tail){int at=queue[head++],xx=at%256,yy=at/256;int next[4]={at-1,at+1,at-256,at+256};for(int k=0;k<4;k++){int n=next[k];if(n<0||n>=65536||((k==0&&xx==0)||(k==1&&xx==255)||(k==2&&yy==0)||(k==3&&yy==255)))continue;if(!seen[n]&&city_on_road((n%256)*16,(n/256)*16)){seen[n]=1;queue[tail++]=(unsigned short)n;}}}
 for(int i=0;i<city_road_count;i++){const CityRoad*r=&city_roads[i];CHECK(seen[(r->y1/16)*256+r->x1/16]);CHECK(seen[(r->y2/16)*256+r->x2/16]);}
 for(int row=0;row<32;row++)for(int col=0;col<32;col++){CityLot l=city_lot_bounds(col,row);if(l.kind){CHECK(!city_on_road(l.x,l.y)&&!city_on_road(l.x+l.w,l.y+l.h));CHECK(city_solid(l.x+l.w/2,l.y+l.h/2));}}
 /* Parking is allowed; it neither pays survival rewards nor ends a visit. */
 city.vx=city.vy=0;g.speed_q8=0;memset(city.cars,0,sizeof city.cars);ticks(&g,1,0);unsigned score=g.run.total_points;ticks(&g,240,0);CHECK(g.mode==M_RUN&&g.run.total_points==score);
 g.save.cash=1000;unsigned earned=g.run.total_cash;game_finish_run(&g);CHECK(g.mode==M_RECEIPT&&g.save.cash==1000+earned);game_finish_run(&g);CHECK(g.save.cash==1000+earned);
 unsigned char disk[SAVE_DISK_SIZE];CHECK(game_encode_save(&g,disk,sizeof disk));Game loaded;game_init(&loaded,2);CHECK(game_load_bytes(&loaded,disk,sizeof disk)&&loaded.save.cash==g.save.cash);
 game_begin_run(&g);CHECK(!city.active&&g.mode==M_RUN);
 /* Raw material invariants: no cosmetic may change glass, rubber, lights,
    rider or trim. Wheels change only rim metal, and every fitted stencil
    has visible paint coverage without changing the silhouette. */
 for(int v=0;v<VEHICLE_COUNT;v++){
  uint16_t original[48*72],painted[48*72],decaled[48*72],rims[48*72];uint8_t roles[48*72];decode_overhead(v,roles);Save finish={0};
  city_vehicle_pixels(original,v,&finish);
  for(int pattern=1;pattern<5;pattern++){
   finish.paint[v]=4;finish.livery[v]=0;finish.wheels[v]=0;city_vehicle_pixels(painted,v,&finish);
   finish.livery[v]=(uint8_t)pattern;finish.livery_color[v]=3;city_vehicle_pixels(decaled,v,&finish);
   finish.livery[v]=0;finish.wheels[v]=3;city_vehicle_pixels(rims,v,&finish);
   unsigned changes=0,hardware=0,lights=0,rubber=0;
   for(int at=0;at<48*72;at++){
    uint16_t c=original[at];CHECK((c!=0)==(painted[at]!=0));CHECK((c!=0)==(decaled[at]!=0));
    int index=roles[at];unsigned tag=overhead_flags[v*256+index],role=overhead_roles[(v*3+1)*48*72+at];CHECK(c==overhead_palette[v*256+index]);
    CHECK(((tag&1)!=0)==(role==1));CHECK(((tag&2)!=0)==(role==2));
    if(!(tag&1)){
     CHECK(painted[at]==c&&decaled[at]==c);
     if(!(tag&2))CHECK(rims[at]==c);
     lights+=role==3;rubber+=role==4;
    }
    changes+=decaled[at]!=painted[at];hardware+=rims[at]!=painted[at];
   }
   if(!changes||!hardware||!rubber)fprintf(stderr,"vehicle %d stencil %d: decals=%u rims=%u lights=%u rubber=%u\n",v,pattern,changes,hardware,lights,rubber);
   CHECK(changes>0&&hardware>0&&rubber>0);
  }
 }
 for(int v=0;v<VEHICLE_COUNT;v++){
  uint16_t base[48*72],finished[48*72];uint8_t roles[48*72];decode_overhead(v,roles);Save finish={0};city_vehicle_pixels(base,v,&finish);
  for(int paint=1;paint<PAINT_COUNT;paint++)for(int pattern=1;pattern<5;pattern++)for(int color=0;color<LIVERY_COLOR_COUNT;color++){
   finish.paint[v]=(uint8_t)paint;finish.livery[v]=(uint8_t)pattern;finish.livery_color[v]=(uint8_t)color;
   city_vehicle_pixels(finished,v,&finish);
   for(int at=0;at<48*72;at++){
    CHECK((base[at]!=0)==(finished[at]!=0));
    if(!(overhead_flags[v*256+roles[at]]&1))CHECK(finished[at]==base[at]);
   }
  }
 }
 /* Pixel-level world invariance: compare two independently clipped views
  * of the same junction/endpoint while crossing the old 47/48 phase seam. */
 static uint16_t ground_a[256*192],ground_b[256*192];
 Surface ga={ground_a,256,192},gb={ground_b,256,192};
 const int shifts[]={1,16,31,32,47,48,63,64,97};
 for(int road=0;road<city_road_count;road++)for(int endpoint=0;endpoint<2;endpoint++){
  const CityRoad*r=&city_roads[road];int cx=(endpoint?r->x2:r->x1)-128,cy=(endpoint?r->y2:r->y1)-96;
  city_draw_ground(&ga,cx,cy);
  for(int y=0;y<192;y++)for(int x=0;x<256;x++)if(ground_a[y*256+x]==C(215,218,200))CHECK(city_on_road(cx+x,cy+y));
  for(unsigned shift=0;shift<sizeof shifts/sizeof shifts[0];shift++){
   int dx=shifts[shift],dy=shifts[sizeof shifts/sizeof shifts[0]-1-shift];city_draw_ground(&gb,cx+dx,cy+dy);
   for(int y=dy;y<192;y++)for(int x=dx;x<256;x++)CHECK(ground_a[y*256+x]==ground_b[(y-dy)*256+x-dx]);
  }
 }
 for(int v=0;v<VEHICLE_COUNT;v++)for(int pose=0;pose<3;pose++){
  const uint8_t*idx=overhead_indexed+(v*3+pose)*48*72,*role=overhead_roles+(v*3+pose)*48*72;
  for(int at=0;at<48*72;at++){
   unsigned flag=overhead_flags[v*256+idx[at]];
   CHECK(((flag&1)!=0)==(role[at]==1));CHECK(((flag&2)!=0)==(role[at]==2));
   if(role[at]!=1)CHECK(!(flag&0x3c));
  }
  memset(top,0,sizeof top);memset(bottom,0,sizeof bottom);Surface ta={top,256,192},tb={bottom,256,192};
  city_draw_vehicle_steering(&ta,128,96,37,pose-1,v,&g.save);city_draw_vehicle_steering(&tb,128,96,37,pose-1,v,&g.save);CHECK(!memcmp(top,bottom,sizeof top));
 }
 /* New architecture has a complete, district-weighted style set. Garden
  * placements are independently checked against individual terrain pixels. */
 unsigned styles[16]={0},accepted=0;
 for(int row=3;row<30;row++)for(int col=2;col<29;col++){
  if(city_lot(col,row)){int style=city_building_style(col,row);CHECK(style>=0&&style<16);styles[style]++;}
  int wx=col*128+10,wy=row*128+10;
  if(city_garden_clear(wx,wy,20,16)){
   accepted++;
   for(int y=wy;y<=wy+16;y++)for(int x=wx;x<=wx+20;x++)CHECK(!city_on_road(x,y)&&!city_water(x,y)&&!city_solid(x,y));
  }
 }
 CHECK(accepted>50);for(int style=0;style<16;style++)CHECK(styles[style]>0);
 CHECK(!city_garden_clear(628,1600,24,24)&&!city_garden_clear(2010,2200,20,20));
 for(int row=0;row<32;row++)for(int x=0;x<128;x++){
  CHECK(city_lawn_rgb[row*384+x]==city_lawn_rgb[row*384+128+x]);CHECK(city_lawn_rgb[row*384+x]==city_lawn_rgb[row*384+256+x]);
 }
 for(int prop=0;prop<8;prop++)for(int y=0;y<32;y++)if(y<=4||y>=23)for(int x=0;x<32;x++){
  unsigned a=city_garden_idx[prop*1024+y*32+x],b=city_garden_idx[(prop+16)*1024+y*32+x];CHECK(city_garden_palette[a]==city_garden_palette[b]);
 }
 /* Every owned vehicle has an overhead silhouette; all skins render city UI. */
 for(int v=0;v<VEHICLE_COUNT;v++){memset(top,0,sizeof top);Surface s={top,256,192};city_draw_vehicle(&s,128,96,v*13,v,&g.save);unsigned occupied=0;for(int i=0;i<256*192;i++)occupied+=top[i]!=0;CHECK(occupied>100&&occupied<2200);}
 if(dir){
  static uint16_t gallery[512*384];Surface sheet={gallery,512,384};gfx_rect(&sheet,0,0,512,384,C(145,165,170));
  for(int v=0;v<VEHICLE_COUNT;v++){int x=v%8*64,y=v/8*64;city_draw_vehicle(&sheet,x+32,y+38,0,v,&g.save);gfx_text_small(&sheet,x+2,y+2,vehicles[v].name,C(21,37,48));}
  char path[512];snprintf(path,sizeof path,"%s/overhead-vehicles.ppm",dir);FILE*f=fopen(path,"wb");CHECK(f!=0);fprintf(f,"P6\n512 384\n255\n");
  for(int i=0;i<512*384;i++){unsigned c=gallery[i];fputc((c&31)*255/31,f);fputc((c>>5&31)*255/31,f);fputc((c>>10&31)*255/31,f);}
  fclose(f);
  /* Full source paintings make glass/body boundaries reviewable at 1:1. */
  static uint16_t materials[384*4608];Surface panels={materials,384,4608};gfx_rect(&panels,0,0,384,4608,C(104,129,139));
  for(int v=0;v<VEHICLE_COUNT;v++)for(int column=0;column<8;column++){
   Save finish={0};finish.paint[v]=(uint8_t)(column==1||column>=4?6:column==2?5:column==3?7:0);
   finish.livery[v]=(uint8_t)(column>=4?column-3:0);finish.livery_color[v]=0;finish.wheels[v]=2;
   uint16_t paint[48*72];city_vehicle_pixels(paint,v,&finish);gfx_sprite(&panels,column*48,v*96+18,paint,48,72,0);
   gfx_text_small(&panels,column*48+1,v*96+2,vehicles[v].name,C(238,244,241));
  }
  snprintf(path,sizeof path,"%s/overhead-review.ppm",dir);f=fopen(path,"wb");CHECK(f!=0);fprintf(f,"P6\n384 4608\n255\n");
  for(int i=0;i<384*4608;i++){unsigned c=materials[i];fputc((c&31)*255/31,f);fputc((c>>5&31)*255/31,f);fputc((c>>10&31)*255/31,f);}fclose(f);
  /* Factory and all four fitted liveries, both actual renderers, all models. */
  static uint16_t finishes[480*4608];Surface review={finishes,480,4608};
  gfx_rect(&review,0,0,480,4608,C(111,132,141));
  Save finish={0};
  for(int v=0;v<VEHICLE_COUNT;v++)for(int pattern=0;pattern<5;pattern++){
   int xx=pattern*96,yy=v*96;finish.paint[v]=pattern?4:0;finish.livery[v]=(uint8_t)pattern;finish.wheels[v]=2;
   gfx_text_small(&review,xx+1,yy+1,vehicles[v].name,C(230,242,245));
   gfx_vehicle_style(&review,xx+2,yy+17,gfx_vehicle_pixels(v,2),48,72,48,72,finish.paint[v],pattern,2,0);
   city_draw_vehicle(&review,xx+72,yy+60,0,v,&finish);
  }
  snprintf(path,sizeof path,"%s/vehicle-finishes.ppm",dir);f=fopen(path,"wb");CHECK(f!=0);fprintf(f,"P6\n480 4608\n255\n");
  for(int i=0;i<480*4608;i++){unsigned c=finishes[i];fputc((c&31)*255/31,f);fputc((c>>5&31)*255/31,f);fputc((c>>10&31)*255/31,f);}fclose(f);
 }
 city_begin(&g);for(int skin=3;skin<9;skin++){g.save.skin=(uint8_t)skin;shot(0,"",&g);}
 if(dir){
  const int scenes[6][3]={{1280,640,0},{1280,1408,0},{2496,1900,0},{1280,2880,0},{2912,2880,0},{2080,2048,64}};
  for(int scene=0;scene<6;scene++){
   city_begin(&g);city.x=scenes[scene][0]*Q;city.y=scenes[scene][1]*Q;city.camera_x=city.x;city.camera_y=city.y;city.angle=(uint16_t)scenes[scene][2];g.frame=120;
   char name[64];snprintf(name,sizeof name,"district-%d-native",scene);shot(dir,name,&g);
  }
  /* Deliberately positioned native diagnostics, clearly labelled as such.
   * The gameplay cartridge tour below is separately controlled by buttons. */
  for(int angle=0;angle<256;angle+=16){
   city_begin(&g);city.x=1280*Q;city.y=1408*Q;city.camera_x=city.x;city.camera_y=city.y;city.angle=(uint16_t)angle;g.frame=120;
   char name[64];snprintf(name,sizeof name,"rotation-%03d-native",angle);shot(dir,name,&g);
   /* Visibility optimization must preserve every pixel, including roofs
    * projecting into the screen from a cell beyond its ground edge. */
   static uint16_t visible[256*192];memcpy(visible,top,sizeof visible);
   extern int city_unculled_diagnostic;city_unculled_diagnostic=1;shot(0,"",&g);city_unculled_diagnostic=0;
   for(int i=0;i<256*192;i++)CHECK(top[i]==visible[i]);
  }
 }
 printf("city: %u checks; 256 heading-locked projections, rotated ground invariance at 24 cardinal views, world-fixed/clipped lane paint across 288 camera shifts, 1161216 independent model-sized contact cases, 16 district-weighted buildings, rooted wind poses, pavement-safe gardens, fixed road connectivity, free steering, bridges, solid lots/water, pause/parking, all 48 painted overhead vehicles with steered axles, six dashboards, shared rewards, v6 saves and highway isolation pass\n",checks);return 0;
}
