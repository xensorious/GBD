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
static void overhead_roles(int v,uint8_t*out){
 const uint8_t*src=overhead_lz+overhead_offsets[v];unsigned in=4,pos=0,size=overhead_offsets[v+1]-overhead_offsets[v];
 CHECK(src[0]==0x10 && (src[1]|src[2]<<8|src[3]<<16)==32*48);
 while(pos<32*48){CHECK(in<size);unsigned flags=src[in++];
  for(unsigned bit=128;bit&&pos<32*48;bit>>=1){
   if(flags&bit){CHECK(in+2<=size);unsigned code=src[in]<<8|src[in+1];in+=2;unsigned n=(code>>12)+3,back=(code&4095)+1;
    CHECK(back<=pos&&n<=32*48-pos);while(n--){out[pos]=out[pos-back];pos++;}
   }else{CHECK(in<size);out[pos++]=src[in++];}
  }
 }
 for(int at=0;at<32*48;at++)if((out[at]&15)>7 || !(out[at]&15))CHECK(!(out[at]&0xf0));
}
int main(int argc,char**argv){
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
  uint16_t original[32*48],painted[32*48],decaled[32*48],rims[32*48];uint8_t roles[32*48];overhead_roles(v,roles);Save finish={0};
  city_vehicle_pixels(original,v,&finish);
  for(int pattern=1;pattern<5;pattern++){
   finish.paint[v]=4;finish.livery[v]=0;finish.wheels[v]=0;city_vehicle_pixels(painted,v,&finish);
   finish.livery[v]=(uint8_t)pattern;finish.livery_color[v]=3;city_vehicle_pixels(decaled,v,&finish);
   finish.livery[v]=0;finish.wheels[v]=3;city_vehicle_pixels(rims,v,&finish);
   unsigned changes=0,hardware=0,lights=0,rubber=0;
   for(int at=0;at<32*48;at++){
    uint16_t c=original[at];CHECK((c!=0)==(painted[at]!=0));CHECK((c!=0)==(decaled[at]!=0));
    int index=roles[at]&15;CHECK(c==overhead_palette[v*16+index]);
    if(index>=8){
     CHECK(painted[at]==c&&decaled[at]==c);
     if(index!=11)CHECK(rims[at]==c);
     lights+=index==12||index==13;rubber+=index==10;
    }
    changes+=decaled[at]!=painted[at];hardware+=rims[at]!=painted[at];
   }
   if(!changes||!hardware||!lights||!rubber)fprintf(stderr,"vehicle %d stencil %d: decals=%u rims=%u lights=%u rubber=%u\n",v,pattern,changes,hardware,lights,rubber);
   CHECK(changes>0&&hardware>0&&lights>0&&rubber>0);
  }
 }
 for(int v=0;v<VEHICLE_COUNT;v++){
  uint16_t base[32*48],finished[32*48];uint8_t roles[32*48];overhead_roles(v,roles);Save finish={0};city_vehicle_pixels(base,v,&finish);
  for(int paint=1;paint<PAINT_COUNT;paint++)for(int pattern=1;pattern<5;pattern++)for(int color=0;color<LIVERY_COLOR_COUNT;color++){
   finish.paint[v]=(uint8_t)paint;finish.livery[v]=(uint8_t)pattern;finish.livery_color[v]=(uint8_t)color;
   city_vehicle_pixels(finished,v,&finish);
   for(int at=0;at<32*48;at++){
    CHECK((base[at]!=0)==(finished[at]!=0));
    if((roles[at]&15)>=8)CHECK(finished[at]==base[at]);
   }
  }
 }
 /* Every owned vehicle has an overhead silhouette; all skins render city UI. */
 for(int v=0;v<VEHICLE_COUNT;v++){memset(top,0,sizeof top);Surface s={top,256,192};city_draw_vehicle(&s,128,96,v*13,v,&g.save);unsigned occupied=0;for(int i=0;i<256*192;i++)occupied+=top[i]!=0;CHECK(occupied>100&&occupied<2200);}
 if(dir){
  static uint16_t gallery[512*384];Surface sheet={gallery,512,384};gfx_rect(&sheet,0,0,512,384,C(145,165,170));
  for(int v=0;v<VEHICLE_COUNT;v++){int x=v%8*64,y=v/8*64;city_draw_vehicle(&sheet,x+32,y+38,0,v,&g.save);gfx_text_small(&sheet,x+2,y+2,vehicles[v].name,C(21,37,48));}
  char path[512];snprintf(path,sizeof path,"%s/overhead-vehicles.ppm",dir);FILE*f=fopen(path,"wb");CHECK(f!=0);fprintf(f,"P6\n512 384\n255\n");
  for(int i=0;i<512*384;i++){unsigned c=gallery[i];fputc((c&31)*255/31,f);fputc((c>>5&31)*255/31,f);fputc((c>>10&31)*255/31,f);}
  fclose(f);
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
 printf("city: %u checks; fixed road connectivity, free steering, bridges, solid lots/water, pause/parking, all 48 overhead vehicles, six dashboards, shared rewards, v6 saves and highway isolation pass\n",checks);return 0;
}
