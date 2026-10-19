#include "game.h"
#include "city.h"
#include "city_render.h"
#include "render.h"
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
 /* Every owned vehicle has an overhead silhouette; all skins render city UI. */
 for(int v=0;v<VEHICLE_COUNT;v++){memset(top,0,sizeof top);Surface s={top,256,192};city_draw_vehicle(&s,128,96,v*13,v,&g.save);unsigned occupied=0;for(int i=0;i<256*192;i++)occupied+=top[i]!=0;CHECK(occupied>100&&occupied<2200);}
 if(dir){
  static uint16_t gallery[512*384];Surface sheet={gallery,512,384};gfx_rect(&sheet,0,0,512,384,C(145,165,170));
  for(int v=0;v<VEHICLE_COUNT;v++){int x=v%8*64,y=v/8*64;city_draw_vehicle(&sheet,x+32,y+38,0,v,&g.save);gfx_text_small(&sheet,x+2,y+2,vehicles[v].name,C(21,37,48));}
  char path[512];snprintf(path,sizeof path,"%s/overhead-vehicles.ppm",dir);FILE*f=fopen(path,"wb");CHECK(f!=0);fprintf(f,"P6\n512 384\n255\n");
  for(int i=0;i<512*384;i++){unsigned c=gallery[i];fputc((c&31)*255/31,f);fputc((c>>5&31)*255/31,f);fputc((c>>10&31)*255/31,f);}
  fclose(f);
 }
 city_begin(&g);for(int skin=3;skin<9;skin++){g.save.skin=(uint8_t)skin;shot(0,"",&g);}
 printf("city: %u checks; fixed road connectivity, free steering, bridges, solid lots/water, pause/parking, all 48 overhead vehicles, six dashboards, shared rewards, v6 saves and highway isolation pass\n",checks);return 0;
}
