/* Native before/after oracle: identical draw sequence and simulation inputs.
 * Run write against the pinned old sources, then read against optimized sources.
 * The pixel corpus is a diagnostic artifact, never cartridge data. */
#include "game.h"
#include "city.h"
#include "render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint16_t top[256*192],bottom[256*192],expected[2*256*192];
static unsigned checks,frames;
static void frame(FILE*f,int write,Game*g){
 render_game(g,top,bottom);
 if(write){fwrite(top,1,sizeof top,f);fwrite(bottom,1,sizeof bottom,f);fwrite(g,1,sizeof *g,f);fwrite(&city,1,sizeof city,f);}
 else{
  if(fread(expected,1,sizeof expected,f)!=sizeof expected||memcmp(top,expected,sizeof top)||memcmp(bottom,expected+256*192,sizeof bottom)){fprintf(stderr,"Pixel mismatch at frame %u\n",frames);exit(1);}
  Game saved;City c;if(fread(&saved,1,sizeof saved,f)!=sizeof saved||fread(&c,1,sizeof c,f)!=sizeof c||memcmp(g,&saved,sizeof saved)||memcmp(&city,&c,sizeof c)){fprintf(stderr,"Simulation mismatch %u\n",frames);exit(1);}
 }
 checks+=2*256*192;++frames;
}
int main(int argc,char**argv){
 if(argc!=3)return 2;int write=!strcmp(argv[1],"write");FILE*f=fopen(argv[2],write?"wb":"rb");if(!f)return 2;
 const int places[8][2]={{654,1664},{1280,1408},{2496,1900},{1280,2880},{2912,2880},{2080,2048},{640,512},{3680,350}};
 Game g;
 for(int p=0;p<8;p++)for(int a=0;a<256;a+=16){
  game_init(&g,123);city_begin(&g);g.mode=M_RUN;g.vehicle=(p*7)%VEHICLE_COUNT;g.frame=120;
  g.save.paint[g.vehicle]=p%16;g.save.livery[g.vehicle]=p%5;g.save.livery_color[g.vehicle]=p%12;g.save.wheels[g.vehicle]=p%4;
  city.x=places[p][0]*Q;city.y=places[p][1]*Q;city.camera_x=city.x+113;city.camera_y=city.y+219;city.angle=a;frame(f,write,&g);
 }
 game_init(&g,456);city_begin(&g);g.mode=M_RUN;
 for(int i=0;i<480;i++){Input in={K_R|(i%120<40?K_LEFT:i%120<80?K_RIGHT:0)|(i%90<25?K_B:0),0};game_update(&g,&in);if(i%4==0)frame(f,write,&g);}
 fclose(f);printf("Optimization equivalence: %u scenes; %u exact pixels; unchanged simulation/save bytes.\n",frames,checks);return 0;
}
