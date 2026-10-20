/* Full story progression, replay/abort rules, indexed art and bank compatibility. */
#include "story.h"
#include "career.h"
#include "render.h"
#include "opening.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern volatile uint32_t story_visual_stats[5];
static unsigned checks,lines_seen,stages[4];
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"story:%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void press(Game*g,unsigned key){Input in={key,key};game_update(g,&in);}
static void tick(Game*g){Input in={0,0};game_update(g,&in);}
static struct{unsigned before;uint16_t top[256*192];unsigned middle;uint16_t bottom[256*192];unsigned after;}screen;
static void draw(Game*g){
 render_game(g,screen.top,screen.bottom);CHECK(screen.before==0xabc12345u&&screen.middle==0xabc12345u&&screen.after==0xabc12345u);
}
static void sign_disk(uint8_t*p){memset(p+8,0,4);uint32_t hash=2166136261u;for(int i=0;i<1000;i++)hash=(hash^p[i])*16777619u;memcpy(p+8,&hash,4);}
static void finish(Game*g,int render){
 unsigned observed=~0u,steps=0;
 while(story.phase!=ST_CLEAR){
  CHECK(g->mode==M_DIALOG&&++steps<3000);
  if(story.phase==ST_READING){
   if(story.line!=observed){
    observed=story.line;lines_seen++;stages[story.current.stage]++;
    CHECK(story.current.speaker<STORY_SPEAKERS&&story.current.background<STORY_BACKGROUNDS&&story.current.length>0&&story.current.length<STORY_TEXT_MAX);
    CHECK(story.current.text[story.current.length]==0);
    CHECK(story_text_lines(story.current.text,226)<=8);
    CHECK(!strstr(story.current.text,"Proposed objective")&&!strstr(story.current.text,"IDEA:"));
   }
   press(g,K_A);if(render)draw(g);
  }else{tick(g);if(render&&story.phase_tick==8)draw(g);}
 }
}
static void compatibility(void){
 Game rich,loaded;game_init(&rich,0x415254u);rich.save.cash=765432;rich.save.owned=UINT32_MAX;rich.save.owned_hi=65535;rich.save.selected_vehicle=47;
 rich.save.paint[47]=15;rich.save.paint_seen[47]|=1u<<15;rich.save.livery[47]=3;rich.save.livery_owned[47]|=1u<<3;rich.save.livery_color[47]=11;rich.save.livery_color_seen[47]|=1u<<11;
 uint8_t disk[1000];CHECK(game_encode_save(&rich,disk,1000));
 /* A genuine v6 layout has identical finish bits and a zero tail. */
 disk[4]=6;sign_disk(disk);game_init(&loaded,4);CHECK(game_load_bytes(&loaded,disk,1000));CHECK(loaded.save.version==7&&loaded.save_requested);
 Save a,b;game_make_save(&rich,&a);game_make_save(&loaded,&b);CHECK(!memcmp(&a,&b,sizeof a));
 CHECK(loaded.save.paint[47]==15&&loaded.save.livery_color[47]==11&&!story_completed(&loaded));
 for(int done=0;done<=125;done++){
  rich.save.reserved[0]=(uint8_t)done;CHECK(game_encode_save(&rich,disk,1000));game_init(&loaded,3);CHECK(game_load_bytes(&loaded,disk,1000));CHECK(story_completed(&loaded)==done);
 }
 rich.save.reserved[0]=126;CHECK(!game_encode_save(&rich,disk,1000));rich.save.reserved[0]=0;CHECK(game_encode_save(&rich,disk,1000));
 disk[759]=126;sign_disk(disk);CHECK(!game_load_bytes(&loaded,disk,1000));
 rich.save.reserved[0]=124;CHECK(game_encode_save(&rich,disk,1000));disk[4]=6;sign_disk(disk);CHECK(!game_load_bytes(&loaded,disk,1000));
 /* Three independent profiles stay within the original 8 KiB bank. */
 Opening slots,reloaded;opening_init(&slots,&rich,NULL,0);
 for(int i=0;i<3;i++){game_init(&rich,100u+(unsigned)i);rich.save.reserved[0]=(uint8_t)(i==2?125:i*25+1);slots.active_slot=(uint8_t)i;CHECK(opening_store(&slots,&rich));}
 uint8_t bank[SAVE_BANK_SIZE];memcpy(bank,slots.records,sizeof bank);opening_init(&reloaded,&loaded,bank,sizeof bank);
 CHECK(loaded.save.reserved[0]==1);reloaded.phase=OPEN_SLOTS;reloaded.cursor=0;
 Input down={K_DOWN,K_DOWN};opening_update(&reloaded,&loaded,&down);CHECK(story_completed(&loaded)==26);opening_update(&reloaded,&loaded,&down);CHECK(story_completed(&loaded)==125);
 CHECK(!memcmp(bank,reloaded.records,sizeof bank));
}
static unsigned image_hash(void){unsigned h=2166136261u;for(unsigned i=0;i<256*192;i++)h=(h^screen.top[i])*16777619u;return h;}
static void directed_camera(void){
 Game g;game_init(&g,123);CHECK(story_open(&g,0,0));
 for(int t=0;t<20;t++)tick(&g);
 draw(&g);
 /* The actual jack is at source X235, not the empty doorway at X384. */
 CHECK(story.line==0&&story.current.background==6&&story_visual_stats[3]==107);
 press(&g,K_A);press(&g,K_A);CHECK(story.line==1&&story.current.speaker==SC_YOU);
 unsigned x=107;
 for(int t=0;t<200;t++){tick(&g);draw(&g);unsigned now=story_visual_stats[3];CHECK(now<=x);x=now;}
 CHECK(x==0);unsigned held=image_hash();
 for(int t=0;t<400;t++){tick(&g);draw(&g);CHECK(story_visual_stats[3]==0&&image_hash()==held);}
 /* Pausing partway through a directed move freezes the camera. */
 press(&g,K_B);press(&g,K_DOWN);press(&g,K_A);for(int t=0;t<18;t++)tick(&g);draw(&g);
 press(&g,K_A);press(&g,K_A);tick(&g);draw(&g);press(&g,K_B);draw(&g);x=story_visual_stats[3];
 for(int t=0;t<20;t++){tick(&g);draw(&g);CHECK(story_visual_stats[3]==x);}
 press(&g,K_DOWN);press(&g,K_DOWN);press(&g,K_A);CHECK(g.mode==M_STORY&&!story_completed(&g));draw(&g);
}
int main(void){
 screen.before=screen.middle=screen.after=0xabc12345u;
 uint8_t pixels[512*216];uint16_t palette[256];int w,h;
 for(int asset=0;asset<STORY_ASSETS;asset++){
  CHECK(story_asset(asset,pixels,palette,sizeof pixels,&w,&h));CHECK(w==(asset<STORY_BACKGROUNDS?512:128)&&h==(asset<STORY_BACKGROUNDS?216:128));
  unsigned filled=0,empty=0;for(int i=0;i<w*h;i++){filled+=pixels[i]!=0;empty+=pixels[i]==0;}
  if(asset>=STORY_BACKGROUNDS)CHECK(filled>1000&&empty>100&&palette[0]==0);
 }
 CHECK(!story_asset(STORY_ASSETS,pixels,palette,sizeof pixels,&w,&h));CHECK(!story_asset(0,pixels,palette,10,&w,&h));
 compatibility();
 directed_camera();
 Game g;game_init(&g,123);CHECK(!story_open(&g,0,1));CHECK(story_completed(&g)==0&&g.mode==M_HOME);
 CHECK(story_open(&g,0,0));draw(&g);
 for(int t=0;t<70;t++){Input held={K_A,0};game_update(&g,&held);}CHECK(!story.line&&!story_completed(&g));
 press(&g,K_B);CHECK(story.pause);press(&g,K_DOWN);press(&g,K_DOWN);press(&g,K_A);CHECK(g.mode==M_STORY&&!story_completed(&g));
 draw(&g); /* releasing the lease must make ordinary UI and driving caches safe */
 Save untouched=g.save;
 for(int index=0;index<125;index++){
  CHECK(story_completed(&g)==index);CHECK(!story_open(&g,(index+1)/25,(index+1)%25));
  CHECK(story_open(&g,index/25,index%25));CHECK(!story.replay);CHECK(story.count>=4&&story.count<=16);
  finish(&g,1);CHECK(story_completed(&g)==index+1&&g.save_requested);
  Save expected=untouched;expected.reserved[0]=(uint8_t)(index+1);CHECK(!memcmp(&expected,&g.save,sizeof expected));
  for(int t=0;t<30;t++)tick(&g);
  CHECK(story_completed(&g)==index+1);
  press(&g,K_A);CHECK(g.mode==M_STORY&&g.cursor==1);draw(&g);
  CHECK(campaign.chapter==(index+1<125?(index+1)/25:4));CHECK(campaign.event==(index+1<125?(index+1)%25:24));
  CHECK(career_journal_open(&g,index/25,0));
 }
 CHECK(lines_seen==STORY_LINES);for(int method=0;method<4;method++)CHECK(stages[method]>0);
 for(int ch=0;ch<5;ch++)for(int ev=0;ev<25;ev++)CHECK(story_status(&g,ch,ev)==2);
 Save completed=g.save;CHECK(story_open(&g,0,5)&&story.replay);finish(&g,1);CHECK(!memcmp(&completed,&g.save,sizeof completed));press(&g,K_B);CHECK(campaign.chapter==0&&campaign.event==5);
 for(int skin=SKIN_FUSION;skin<SKIN_STORAGE_LIMIT;skin++){
  g.save.skin=(uint8_t)skin;
  for(int tab=0;tab<5;tab++){campaign.tab=(uint8_t)tab;campaign.revision++;g.mode=M_STORY;g.cursor=0;draw(&g);}
  CHECK(story_open(&g,4,24));for(int t=0;t<18;t++)tick(&g);press(&g,K_A);draw(&g);press(&g,K_B);draw(&g);press(&g,K_DOWN);press(&g,K_DOWN);press(&g,K_A);draw(&g);
 }
 CHECK(!story_visual_stats[0]);g.mode=M_GARAGE;g.cursor=0;draw(&g);g.mode=M_DRIVE;g.cursor=0;press(&g,K_A);draw(&g);CHECK(g.mode==M_RUN);
 printf("story: %u checks; all 125 scenes / %u lines, four staging methods, replay/abort/held-input rules, saved frontier0..125, v6-to-v7 rich-profile migration, three independent profiles and scratch restoration pass\n",checks,(unsigned)STORY_LINES);
 return 0;
}
