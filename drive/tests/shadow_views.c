/* Full-scene visibility and deterministic lighting checks. Native reference
 * renders are diagnostics, not DS performance or emulator screenshots. */
#include "game.h"
#include "city.h"
#include "render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern int city_unculled_diagnostic;
extern volatile uint32_t city_mesh_last_stats[6],city_sun_stats[9];
static uint16_t top[256*192],bottom[256*192],reference[256*192];
static void dump(const char*path,const uint16_t*p){
 FILE*f=fopen(path,"wb");if(!f)return;fprintf(f,"P6\n256 192\n255\n");
 for(int i=0;i<256*192;i++){fputc((p[i]&31)*255/31,f);fputc((p[i]>>5&31)*255/31,f);fputc((p[i]>>10&31)*255/31,f);}fclose(f);
}
int main(void){
 const int scenes[6][2]={{1280,640},{1280,1408},{2496,1900},{1280,2880},{2912,2880},{2080,2048}};
 Game g;game_init(&g,0x53484144);unsigned views=0,max_quads=0;unsigned long checks=0;
 for(int scene=0;scene<6;scene++)for(int angle=0;angle<256;angle+=8){
  city_begin(&g);g.mode=M_RUN;g.vehicle=scene==0?3:scene==1?46:scene==2?0:scene==3?30:scene==4?47:42;
  city.x=scenes[scene][0]*Q;city.y=scenes[scene][1]*Q;city.camera_x=city.x;city.camera_y=city.y;city.angle=(uint16_t)angle;g.frame=120;
  render_game(&g,top,bottom);memcpy(reference,top,sizeof reference);
  if(city_mesh_last_stats[0]>max_quads)max_quads=city_mesh_last_stats[0];
  city_unculled_diagnostic=1;render_game(&g,top,bottom);city_unculled_diagnostic=0;
  for(int i=0;i<256*192;i++){
   checks++;if(top[i]!=reference[i]){
    fprintf(stderr,"Visibility mismatch: scene %d angle %d pixel %d,%d\n",scene,angle,i%256,i/256);
    dump("/tmp/shadow-culled.ppm",reference);dump("/tmp/shadow-unculled.ppm",top);return 1;
   }
  }
  g.frame=121;render_game(&g,top,bottom);
  for(int i=0;i<256*192;i++){
   checks++;if(top[i]!=reference[i]){
    fprintf(stderr,"Static-light flicker: scene %d angle %d pixel %d,%d\n",scene,angle,i%256,i/256);return 1;
   }
  }
  views++;
 }
 printf("Shadow views: %u district/heading/vehicle cases; %lu exact pixel checks; no visibility loss or static-light flicker; max %u native quads.\n",views,checks,max_quads);return 0;
}
