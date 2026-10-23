/* Cast studio input, native portrait pixels, cache restoration and saved-state guards. */
#include "story.h"
#include "career.h"
#include "render.h"
#include "city.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "story_visuals.h"
#ifndef STORY_QUARTER_AVAILABLE
#define STORY_QUARTER_AVAILABLE 0
#endif

extern volatile uint32_t story_visual_stats[5];
enum { W=256,H=192,GUARD=16 };
typedef struct {uint16_t before[GUARD],top[W*H],middle[GUARD],bottom[W*H],after[GUARD];} Frames;
static Frames screen,snapshot;
static uint8_t portrait[STORY_PORTRAIT_WIDTH*STORY_PORTRAIT_HEIGHT];
static uint16_t palette[256];
static const uint8_t pose_counts[STORY_GALLERY_COUNT]=STORY_GALLERY_POSE_COUNTS_INITIALIZER;
static unsigned checks,portraits_checked,directional_checked;
#define CHECK(v) do{checks++;if(!(v)){fprintf(stderr,"cast gallery:%d: %s\n",__LINE__,#v);exit(1);}}while(0)

static void press(Game*g,unsigned key){Input in={key,key};game_update(g,&in);}
static void guards(void){
 for(int i=0;i<GUARD;i++)screen.before[i]=screen.middle[i]=screen.after[i]=(uint16_t)(0xa051+i);
}
static void draw(Game*g){
 guards();render_game(g,screen.top,screen.bottom);
 for(int i=0;i<GUARD;i++)CHECK(screen.before[i]==0xa051+i&&screen.middle[i]==0xa051+i&&screen.after[i]==0xa051+i);
 CHECK(!story_visual_stats[0]);
}
static void open_gallery(Game*g){
 career_enter(g);press(g,K_L);CHECK(campaign.tab==4&&g->cursor==0);
 press(g,K_UP);CHECK(g->cursor==4&&career_rows(4)==5);
 press(g,K_A);CHECK(g->mode==M_STORY&&campaign.modal==CAREER_CAST_MODAL&&!campaign.page&&!campaign.gallery_pose);
 draw(g);
}
static void native_portrait(void){
 int entry=campaign.page,pose=campaign.gallery_pose&CAREER_GALLERY_POSE_MASK,w,h;
 int id=story_gallery_portraits[entry][pose],full=story_gallery_fullbody[entry][pose];
#if STORY_QUARTER_AVAILABLE
 if(!(campaign.gallery_pose&CAREER_GALLERY_LEGACY)){
  int facing=campaign.gallery_pose&CAREER_GALLERY_LEFT?0:1;
  id=story_gallery_quarters[entry][pose][facing];full=1;directional_checked++;
 }
#endif
 CHECK(story_asset(STORY_BACKGROUNDS+id,portrait,palette,sizeof portrait,&w,&h));
 CHECK(w==STORY_PORTRAIT_WIDTH&&h==STORY_PORTRAIT_HEIGHT);
 int floor=full?story_ground_y[0]:STORY_SCENE_HEIGHT;
 unsigned pixels=0,lower=0;
 /* The studio always draws its lead one-to-one. Check original indexed pixels
  * directly on both LCDs; only the tiny top title can overlap a native pixel. */
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){
  unsigned index=portrait[y*w+x];int yy=y+floor-h;
  if(!index||yy<22||yy>=STORY_SCENE_HEIGHT)continue;
  uint16_t actual=yy<H?screen.top[yy*W+x]:screen.bottom[(yy-H)*W+x];
  CHECK(actual==palette[index]);pixels++;lower+=yy>=H;
 }
 CHECK(pixels>1000&&lower>100);portraits_checked++;
}

static void browsing(void){
 CHECK(STORY_GALLERY_COUNT==28&&STORY_PEOPLE==14&&SC_YOU==14&&SC_NARRATOR==15);
 CHECK(sizeof(CareerUI)==12&&offsetof(CareerUI,revision)==8&&offsetof(CareerUI,gallery_pose)==7);
 Game g;game_init(&g,0x435354u);g.save.reserved[0]=37;g.save.cash=54321;g.save.paint[0]=4;
 Save saved=g.save;StoryState untouched=story;
 open_gallery(&g);
 CHECK(pose_counts[0]==5);
 for(int entry=0;entry<STORY_GALLERY_COUNT;entry++){
  CHECK(campaign.page==entry&&!campaign.gallery_pose);
  CHECK(story_gallery_draft[entry]==(entry>=STORY_PEOPLE));
  CHECK(pose_counts[entry]==(entry==0?5:entry<STORY_PEOPLE?4:1));
  for(int pose=0;pose<pose_counts[entry];pose++){
   CHECK(campaign.gallery_pose==pose);
   draw(&g);native_portrait();snapshot=screen;draw(&g);
   CHECK(!memcmp(snapshot.top,screen.top,sizeof screen.top)&&!memcmp(snapshot.bottom,screen.bottom,sizeof screen.bottom));
   CHECK(!memcmp(&saved,&g.save,sizeof saved)&&!memcmp(&untouched,&story,sizeof untouched));
#if STORY_QUARTER_AVAILABLE
   press(&g,K_X);CHECK(campaign.gallery_pose==(unsigned)(pose|CAREER_GALLERY_LEFT));draw(&g);native_portrait();
   CHECK(!memcmp(&saved,&g.save,sizeof saved)&&!memcmp(&untouched,&story,sizeof untouched));
   press(&g,K_X);CHECK(campaign.gallery_pose==pose);draw(&g);
   CHECK(!memcmp(snapshot.top,screen.top,sizeof screen.top)&&!memcmp(snapshot.bottom,screen.bottom,sizeof screen.bottom));
   press(&g,K_Y);CHECK(campaign.gallery_pose==(unsigned)(pose|CAREER_GALLERY_LEGACY));draw(&g);native_portrait();
   CHECK(!memcmp(&saved,&g.save,sizeof saved)&&!memcmp(&untouched,&story,sizeof untouched));
   press(&g,K_Y);CHECK(campaign.gallery_pose==pose);draw(&g);
   CHECK(!memcmp(snapshot.top,screen.top,sizeof screen.top)&&!memcmp(snapshot.bottom,screen.bottom,sizeof screen.bottom));
#endif
   Input held={K_RIGHT|K_R,0};game_update(&g,&held);
   CHECK(campaign.page==entry&&campaign.gallery_pose==pose);
   press(&g,K_R);
  }
  CHECK(!campaign.gallery_pose);
  press(&g,K_L);CHECK(campaign.gallery_pose==pose_counts[entry]-1);
  press(&g,K_A);CHECK(!campaign.gallery_pose);
  press(&g,K_RIGHT);
 }
 CHECK(!campaign.page&&!campaign.gallery_pose&&portraits_checked==(STORY_QUARTER_AVAILABLE?213u:71u));
 CHECK(directional_checked==(STORY_QUARTER_AVAILABLE?142u:0u));
 press(&g,K_LEFT);CHECK(campaign.page==27&&!campaign.gallery_pose);
 press(&g,K_DOWN);CHECK(!campaign.page);
 press(&g,K_UP);CHECK(campaign.page==27);
 press(&g,K_RIGHT);CHECK(!campaign.page);
 press(&g,K_A);CHECK(campaign.gallery_pose==1);press(&g,K_DOWN);CHECK(campaign.page==1&&!campaign.gallery_pose);
 unsigned loads=story_visual_stats[1];
 press(&g,K_B);CHECK(g.mode==M_STORY&&!campaign.modal&&campaign.tab==4&&g.cursor==4);draw(&g);
 CHECK(!memcmp(&saved,&g.save,sizeof saved)&&!memcmp(&untouched,&story,sizeof untouched));
 press(&g,K_A);draw(&g);CHECK(story_visual_stats[1]==loads+1);
 press(&g,K_B);draw(&g);press(&g,K_B);CHECK(g.mode==M_DRIVE&&g.cursor==2);draw(&g);
 CHECK(!memcmp(&saved,&g.save,sizeof saved)&&!memcmp(&untouched,&story,sizeof untouched));
}

static void story_restore(void){
 Game g;game_init(&g,9);CHECK(story_open(&g,0,0));
 for(int i=0;i<16;i++){Input idle={0,0};game_update(&g,&idle);}
 story.revealed=story.page_end;story.age=40;draw(&g);snapshot=screen;
 StoryState expected=story;Save saved=g.save;
 career_enter(&g);campaign.modal=CAREER_CAST_MODAL;campaign.page=20;campaign.gallery_pose=0;draw(&g);
 CHECK(!memcmp(&expected,&story,sizeof expected));
 g.mode=M_DIALOG;draw(&g);
 CHECK(!memcmp(snapshot.top,screen.top,sizeof screen.top)&&!memcmp(snapshot.bottom,screen.bottom,sizeof screen.bottom));
 CHECK(!memcmp(&saved,&g.save,sizeof saved)&&!memcmp(&expected,&story,sizeof expected));
 press(&g,K_B);press(&g,K_DOWN);press(&g,K_DOWN);press(&g,K_A);draw(&g);
 CHECK(g.mode==M_STORY&&!story.active&&!story_completed(&g));
}

static void driving_restore(int free_roam){
 Game g;game_init(&g,0x535455u);
 if(free_roam)city_begin(&g);else game_begin_run(&g);
 g.frame=200;draw(&g);snapshot=screen;Game driving=g;City world=city;
 career_enter(&g);campaign.modal=CAREER_CAST_MODAL;campaign.page=27;campaign.gallery_pose=0;draw(&g);
 press(&g,K_B);draw(&g);g=driving;draw(&g);
 CHECK(!memcmp(snapshot.top,screen.top,sizeof screen.top)&&!memcmp(snapshot.bottom,screen.bottom,sizeof screen.bottom));
 CHECK(!memcmp(&world,&city,sizeof world));
}

int main(void){
 browsing();story_restore();driving_restore(0);driving_restore(1);
 printf("cast gallery: %u checks; all 28 identities / 71 outfits / %u directional views / %u total gallery views, native pixels across LCDs, button navigation, unchanged saved/story state, bounds and exact story/highway/city cache restoration pass\n",checks,directional_checked,portraits_checked);
 return 0;
}
