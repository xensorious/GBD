/* Baked illustrated pages must not acquire separate actors or camera overlays. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/dialogue_render.c"
#include "render.h"

enum {W=256,H=192,GUARD=16};
typedef struct {uint16_t before[GUARD],top[W*H],middle[GUARD],bottom[W*H],after[GUARD];} Frames;
static Frames frame,reference;
static uint8_t expected[STORY_BACKGROUND_WIDTH*STORY_BACKGROUND_HEIGHT];
static uint16_t colors[256];
static unsigned checks,plates,lines,pages;
#define CHECK(v) do{checks++;if(!(v)){fprintf(stderr,"baked plate:%d: %s\n",__LINE__,#v);exit(1);}}while(0)
static void press(Game*g,unsigned key){Input in={key,key};game_update(g,&in);}
static void tick(Game*g){Input in={0,0};game_update(g,&in);}
static uint16_t pixel(int x,int y){return y<H?frame.top[y*W+x]:frame.bottom[(y-H)*W+x];}
static void draw(Game*g){
 Save saved=g->save;StoryState untouched=story;
 for(int n=0;n<GUARD;n++)frame.before[n]=frame.middle[n]=frame.after[n]=(uint16_t)(0xa7b0+n);
 render_game(g,frame.top,frame.bottom);
 for(int n=0;n<GUARD;n++)CHECK(frame.before[n]==0xa7b0+n&&frame.middle[n]==0xa7b0+n&&frame.after[n]==0xa7b0+n);
 CHECK(!memcmp(&saved,&g->save,sizeof saved)&&!memcmp(&untouched,&story,sizeof untouched));
 CHECK(!story_visual_stats[0]);
}
static void reading(Game*g){
 unsigned count=0;
 while(story.phase!=ST_READING){CHECK(story.active&&!story.failed&&++count<40);tick(g);}
}
static void check_pixels(void){
 const int x0=(STORY_BACKGROUND_WIDTH-W)/2,y0=(STORY_BACKGROUND_HEIGHT-STORY_SCENE_HEIGHT)/2;
 for(int y=0;y<STORY_SCENE_HEIGHT;y++)for(int x=0;x<W;x++)
  CHECK(pixel(x,y)==colors[expected[(y0+y)*STORY_BACKGROUND_WIDTH+x0+x]]);
}
static void load_expected(int background_id){
 int w,h;CHECK(story_asset(background_id,expected,colors,sizeof expected,&w,&h));
 CHECK(w==STORY_BACKGROUND_WIDTH&&h==STORY_BACKGROUND_HEIGHT);
}
static void illustrated_levels(void){
 Game g;game_init(&g,0x706c6174u);g.save.reserved[0]=125;
 int backgrounds[3]={-1,-1,-1};
 for(int event=3;event<6;event++){
  CHECK(story_open(&g,0,event));reading(&g);CHECK(story.current.stage==ST_PLATE&&story.replay);
  backgrounds[event-3]=story.current.background;
  load_expected(story.current.background);
  unsigned actors=story_visual_stats[2],loaded=story_visual_stats[1];
  unsigned observed=~0u,observed_page=~0u,steps=0;
  while(story.phase!=ST_CLEAR){
   CHECK(++steps<500&&!story.failed);
   if(story.phase!=ST_READING){tick(&g);continue;}
   CHECK(story.current.stage==ST_PLATE&&story.current.background==backgrounds[event-3]);
   if(story.line!=observed){observed=story.line;lines++;observed_page=~0u;}
   if(story.page_index!=observed_page){pages++;observed_page=story.page_index;}
   draw(&g);check_pixels();
   CHECK(story_visual_stats[2]==actors&&story_visual_stats[3]==(STORY_BACKGROUND_WIDTH-W)/2);
   if(story.line==0&&story.page_index==0&&story.revealed==0){
    /* Idle text reveal, a directed-focus bit and an existing actor speaker
     * must not move, breathe or draw over the baked illustration. */
    for(int n=0;n<40;n++){tick(&g);draw(&g);check_pixels();}
    press(&g,K_B);CHECK(story.pause);draw(&g);CHECK(story_visual_stats[2]==actors);
    press(&g,K_B);CHECK(!story.pause);draw(&g);check_pixels();
    /* Cold-cache rendering yields the same page without actor loads. */
    story_render_release();draw(&g);check_pixels();CHECK(story_visual_stats[2]==actors);
   }
   press(&g,K_A);
  }
  CHECK(story_visual_stats[2]==actors&&story_visual_stats[1]==loaded+2);
  CHECK(g.save.reserved[0]==125);plates++;
  press(&g,K_A);CHECK(!story.active&&g.mode==M_STORY);
 }
 CHECK(backgrounds[0]!=backgrounds[1]&&backgrounds[0]!=backgrounds[2]&&backgrounds[1]!=backgrounds[2]);
 CHECK(plates==3&&lines>=12&&pages>=lines);
}
static void open_normal(Game*g){
 CHECK(story_open(g,0,0));reading(g);
 for(int line=0;line<2;line++){
  while(story.revealed<story.page_end)tick(g);
  press(g,K_A);reading(g);
 }
 CHECK(story.current.stage==ST_FACE);
 draw(g);
}
static void cache_return(void){
 Game g;game_init(&g,74);g.save.reserved[0]=125;
 open_normal(&g);reference=frame;
 CHECK(story_open(&g,0,3));reading(&g);draw(&g);
 unsigned actors=story_visual_stats[2];CHECK(story.current.stage==ST_PLATE);
 story_render_release();open_normal(&g);
 CHECK(!memcmp(reference.top,frame.top,sizeof frame.top)&&!memcmp(reference.bottom,frame.bottom,sizeof frame.bottom));
 CHECK(story_visual_stats[2]>actors);
 story_render_release();
}
static void progression(void){
 Game g;game_init(&g,38);g.save.reserved[0]=3;
 for(int event=3;event<6;event++){
  CHECK(story_open(&g,0,event)&&!story.replay);
  unsigned steps=0;
  while(story.phase!=ST_CLEAR){CHECK(++steps<500&&!story.failed);if(story.phase==ST_READING)press(&g,K_A);else tick(&g);}
  CHECK(g.save.reserved[0]==event+1&&g.save_requested);
  press(&g,K_A);CHECK(!story.active&&g.mode==M_STORY);
 }
 CHECK(g.save.reserved[0]==6);
 story_render_release();
}
int main(void){
 CHECK(ST_PLATE==4&&sizeof(StoryLine)==200&&sizeof(StoryState)==488&&offsetof(StoryState,page_start)==485);
 illustrated_levels();cache_return();progression();
 printf("baked plates: %u checks; %u illustrated levels, %u lines / %u pages, exact native pixels across both LCDs, zero actor loads, static camera, pause, cache return and progression pass\n",checks,plates,lines,pages);
 return 0;
}
