#include "opening.h"
#include "render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"opening:%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void key(Opening*o,Game*g,unsigned k){Input i={k,k};opening_update(o,g,&i);}
static void menu(Opening*o,Game*g){while(o->phase==OPEN_SLIDE)key(o,g,0);CHECK(o->phase==OPEN_DONE);}
static struct{unsigned a;uint16_t movie[256*384],top[256*192],bottom[256*192];unsigned b;} pixels;
int main(void){
 Opening o,reloaded;Game g,loaded;game_init(&g,123);game_init(&loaded,7);opening_init(&o,&g,0,0);
 CHECK(o.phase==OPEN_TITLE && o.active_slot==255);CHECK(!o.slots[0].valid && !o.slots[1].valid && !o.slots[2].valid);
 key(&o,&g,K_X);CHECK(o.phase==OPEN_SLOTS);key(&o,&g,K_B);CHECK(o.phase==OPEN_TITLE);
 key(&o,&g,K_Y);key(&o,&g,K_A);CHECK(o.phase==OPEN_SLIDE && o.active_slot==0);
 CHECK(opening_store(&o,&g));game_save_result(&g,1);menu(&o,&g);
 g.save.cash=12345;g.save.skin=SKIN_FUSION;g.save.presets[0]=0;g.save.preset_mask=1;CHECK(opening_store(&o,&g));
 uint8_t first[512];memcpy(first,o.records,512);
 key(&o,&g,K_B);key(&o,&g,K_DOWN);key(&o,&g,K_A);CHECK(o.active_slot==1 && !g.save.cash && !g.save.preset_mask);
 g.save.cash=22222;CHECK(opening_store(&o,&g));CHECK(!memcmp(first,o.records,512));menu(&o,&g);
 key(&o,&g,K_B);key(&o,&g,K_DOWN);key(&o,&g,K_A);CHECK(o.active_slot==2 && !g.save.cash);g.save.cash=33333;CHECK(opening_store(&o,&g));menu(&o,&g);
 opening_init(&reloaded,&loaded,o.records,sizeof o.records);CHECK(reloaded.slots[0].cash==12345 && reloaded.slots[1].cash==22222 && reloaded.slots[2].cash==33333);
 CHECK(loaded.save.skin==SKIN_FUSION && loaded.save.preset_mask==1);
 o.records[512+90]^=1;opening_init(&reloaded,&loaded,o.records,sizeof o.records);CHECK(reloaded.slots[0].valid && !reloaded.slots[1].valid && reloaded.slots[2].valid);
 opening_init(&reloaded,&loaded,first,500);CHECK(reloaded.slots[0].valid && !reloaded.slots[1].valid && !reloaded.slots[2].valid);
 pixels.a=pixels.b=0xa55aa55a;game_init(&g,1);opening_init(&o,&g,0,0);
 for(int region=0;region<LOCATION_COUNT;region++){
  g.frame=(unsigned)region*50;
  for(int phase=0;phase<3;phase++){
   o.phase=(uint8_t)phase;o.slide=(uint16_t)(region*3%OPENING_SLIDE_TICKS);
   render_opening(&g,&o,pixels.movie,pixels.top,pixels.bottom);CHECK(pixels.a==0xa55aa55a && pixels.b==0xa55aa55a);
  }
 }
 /* At the slide endpoint, no resampling: both LCDs match the actual menu. */
 o.phase=OPEN_SLIDE;o.slide=OPENING_SLIDE_TICKS;g.mode=M_HOME;
 render_opening(&g,&o,pixels.movie,pixels.top,pixels.bottom);
 static uint16_t top[256*192],bottom[256*192];render_game(&g,top,bottom);
 if(memcmp(top,pixels.top,sizeof top)){unsigned n=0;for(int i=0;i<256*192;i++)if(top[i]!=pixels.top[i]){if(n<12)fprintf(stderr,"endpoint (%d,%d): %04x != %04x\n",i%256,i/256,top[i],pixels.top[i]);++n;}fprintf(stderr,"endpoint differences: %u\n",n);}
 CHECK(!memcmp(top,pixels.top,sizeof top));CHECK(!memcmp(bottom,pixels.bottom,sizeof bottom));
 printf("opening: %u checks; three isolated profiles, corruption containment, old Slot 1 import, any-button gate, tall camera and exact slide endpoint pass\n",checks);return 0;
}
