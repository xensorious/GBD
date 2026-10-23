/* Full story progression, replay/abort rules, indexed art and bank compatibility. */
#include "story.h"
#include "career.h"
#include "render.h"
#include "opening.h"
#include "assets.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern volatile uint32_t story_visual_stats[5];
static unsigned checks,lines_seen,pages_seen,expected_pages,stages[ST_PLATE+1],extended_roles[2],companion_fixture_passed;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"story:%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void press(Game*g,unsigned key){Input in={key,key};game_update(g,&in);}
static void tick(Game*g){Input in={0,0};game_update(g,&in);}
static struct{unsigned before;uint16_t top[256*192];unsigned middle;uint16_t bottom[256*192];unsigned after;}screen;
static void draw(Game*g){
 render_game(g,screen.top,screen.bottom);CHECK(screen.before==0xabc12345u&&screen.middle==0xabc12345u&&screen.after==0xabc12345u);
}
static void extended_pose(Game*g,int companion);
static void sign_disk(uint8_t*p){memset(p+8,0,4);uint32_t hash=2166136261u;for(int i=0;i<1000;i++)hash=(hash^p[i])*16777619u;memcpy(p+8,&hash,4);}
/* Use the renderer's actual linked font as an independent wrapping oracle. */
static unsigned font_page_end(const char*text,unsigned start){
 unsigned at=start,rows=1;int pixels=0,used=0;
 while(text[at]){
  if(text[at]=='\n'){at++;if(rows==STORY_DIALOG_ROWS)return at;rows++;pixels=used=0;continue;}
  if(text[at]==' '){if(used){pixels+=gfx_text_width(" ");used++;}at++;continue;}
  char word[STORY_TEXT_MAX];unsigned count=0;
  while(text[at+count]&&text[at+count]!=' '&&text[at+count]!='\n'){word[count]=text[at+count];count++;}
  word[count]=0;int width=gfx_text_width(word);CHECK(width<=STORY_DIALOG_TEXT_WIDTH);
  if(used&&pixels+width>STORY_DIALOG_TEXT_WIDTH){if(rows==STORY_DIALOG_ROWS)return at;rows++;pixels=used=0;}
  pixels+=width;used+=(int)count;at+=count;
 }
 return at;
}
static unsigned check_pages(const StoryLine*line){
 char covered[STORY_TEXT_MAX]={0};unsigned start=0,pages=0;
 do{
  unsigned end=story_text_page_end(line->text,start);char page[STORY_TEXT_MAX];
  CHECK(end>start&&end<=line->length&&end==font_page_end(line->text,start));
  memcpy(page,line->text+start,end-start);page[end-start]=0;
  CHECK(story_text_lines(page,STORY_DIALOG_TEXT_WIDTH)<=STORY_DIALOG_ROWS);
  memcpy(covered+start,page,end-start);start=end;pages++;
 }while(start<line->length);
 CHECK(!memcmp(covered,line->text,(unsigned)line->length+1));return pages;
}
static void pagination_rules(void){
 for(unsigned ch=32;ch<=127;ch++){
  char glyph[2]={(char)ch,0};CHECK(story_text_advance(ch)==font_advance[ch-32]);CHECK(story_text_advance(ch)==(unsigned)gfx_text_width(glyph));
 }
 CHECK(story_text_advance(0)==story_text_advance('?')&&story_text_advance(255)==story_text_advance('?'));
 Game g;game_init(&g,123);CHECK(story_open(&g,0,0));
 for(int t=0;t<16;t++)tick(&g);
 CHECK(story.phase==ST_READING);
 /* A final authored line with enough wide words to need two dialogue pages. */
 const char*text="WWWW WWWW WWWW WWWW WWWW WWWW WWWW WWWW WWWW WWWW WWWW WWWW WWWW WWWW WWWW WWWW WWWW WWWW WWWW WWWW WWWW WWWW WWWW WWWW WWWW WWWW";
 strcpy(story.current.text,text);story.current.length=(uint8_t)strlen(text);story.line=(uint8_t)(story.count-1);
 story.page_start=story.page_index=0;story.page_end=(uint8_t)story_text_page_end(text,0);story.revealed=0;story.age=63;story.ticks=301;
 CHECK(check_pages(&story.current)==2&&story.page_end<story.current.length);
 StoryLine current=story.current,previous=story.previous;Save untouched=g.save;
 for(int t=0;t<80;t++){Input held={K_A,0};game_update(&g,&held);CHECK(story.revealed<=story.page_end);}
 CHECK(story.phase==ST_READING&&story.page_index==0&&story.revealed==story.page_end&&!story_completed(&g));
 press(&g,K_B);unsigned age=story.age,ticks=story.ticks;for(int t=0;t<20;t++)tick(&g);
 CHECK(story.pause&&story.age==age&&story.ticks==ticks);press(&g,K_B);CHECK(!story.pause);
 story.revealed=story.page_start;press(&g,K_A);CHECK(story.revealed==story.page_end&&story.page_index==0&&story.phase==ST_READING);
 StoryState before=story;press(&g,K_A);
 CHECK(story.page_index==1&&story.page_start==before.page_end&&story.page_end==story.current.length&&story.revealed==story.page_start);
 CHECK(story.line==before.line&&story.phase==before.phase&&story.phase_tick==before.phase_tick&&story.age==before.age+1&&story.ticks==(uint16_t)(before.ticks+1));
 CHECK(!memcmp(&current,&story.current,sizeof current)&&!memcmp(&previous,&story.previous,sizeof previous)&&!memcmp(&untouched,&g.save,sizeof untouched));
 StoryState second_page=story;press(&g,K_B);press(&g,K_DOWN);press(&g,K_A);
 CHECK(story.phase==ST_FADEIN&&!story.line&&!story.page_start&&!story.page_index&&!story.revealed&&story.page_end==story_text_page_end(story.current.text,0));
 CHECK(!memcmp(&untouched,&g.save,sizeof untouched));story=second_page;
 for(int t=0;t<80;t++)tick(&g);
 CHECK(story.revealed==story.page_end&&story.phase==ST_READING&&!story_completed(&g));
 press(&g,K_A);CHECK(story.phase==ST_ENDING&&!story_completed(&g));for(int t=0;t<16;t++)tick(&g);
 CHECK(story.phase==ST_CLEAR&&story_completed(&g)==1&&g.save_requested);
}
static void finish(Game*g,int render){
 unsigned observed=~0u,observed_page=~0u,steps=0;uint8_t previous_flags=0;
 while(story.phase!=ST_CLEAR){
  CHECK(g->mode==M_DIALOG&&++steps<3000);
  if(story.phase==ST_READING){
   if(story.line!=observed){
    if(observed!=~0u)CHECK(story.previous.pose_flags==previous_flags);
    observed=story.line;lines_seen++;CHECK(story.current.stage<=ST_PLATE);stages[story.current.stage]++;
    previous_flags=story.current.pose_flags;
    CHECK(story.current.speaker<STORY_SPEAKERS&&story.current.background<STORY_BACKGROUNDS&&story.current.length>0&&story.current.length<STORY_TEXT_MAX);
    CHECK(story_pose(&story.current,0)<STORY_POSES&&story_pose(&story.current,1)<STORY_POSES);
    CHECK(story.current.text[story.current.length]==0);
    expected_pages+=check_pages(&story.current);observed_page=~0u;
    CHECK(!strstr(story.current.text,"Proposed objective")&&!strstr(story.current.text,"IDEA:"));
    if(story.current.stage==ST_FACE&&story.current.speaker==SC_NIA&&story_pose(&story.current,0)==4){if(!extended_roles[0])extended_pose(g,0);extended_roles[0]++;}
    if(story.current.stage==ST_FACE&&story.current.speaker<STORY_PEOPLE&&story.current.companion==SC_NIA&&story_pose(&story.current,1)==4){if(!extended_roles[1])extended_pose(g,1);extended_roles[1]++;}
   }
   CHECK(story.page_start<story.page_end&&story.page_end<=story.current.length&&story.revealed>=story.page_start&&story.revealed<=story.page_end);
   if(observed_page!=story.page_index){CHECK(observed_page==~0u?story.page_index==0:story.page_index==observed_page+1);observed_page=story.page_index;pages_seen++;}
   StoryState before=story;Save saved=g->save;press(g,K_A);
   if(before.revealed<before.page_end){
    CHECK(story.line==before.line&&story.page_index==before.page_index&&story.revealed==before.page_end&&story.phase==ST_READING);
   }else if(before.page_end<before.current.length){
    CHECK(story.line==before.line&&story.phase==ST_READING&&story.phase_tick==before.phase_tick&&story.page_start==before.page_end&&story.page_index==before.page_index+1&&story.revealed==story.page_start);
    CHECK(story.age==(before.age<65535?before.age+1:65535)&&story.ticks==(uint16_t)(before.ticks+1));
    CHECK(!memcmp(&before.current,&story.current,sizeof story.current)&&!memcmp(&before.previous,&story.previous,sizeof story.previous)&&!memcmp(&saved,&g->save,sizeof saved));
   }
   if(render)draw(g);
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
static unsigned isolated_hash(Game*g){
 const StoryLine*line=story.phase==ST_SHOT&&story.phase_tick<8?&story.previous:&story.current;
 unsigned portraits=line->speaker<STORY_PEOPLE;
 if(line->companion<STORY_PEOPLE&&line->companion!=line->speaker)portraits++;
 uint32_t loaded=story_visual_stats[2];story_render_release();draw(g);
 CHECK(story_visual_stats[2]==loaded+portraits);
 return image_hash();
}
static void extended_pose(Game*g,int companion){
 StoryState saved=story;
 story.phase=ST_READING;story.phase_tick=0;story.age=8;story.ticks=0;story.pause=0;story.revealed=story.page_end;
 story.current.focus&=(uint16_t)~STORY_FOCUS_MOVE;
 /* A close scene keeps both actors visible while comparing each pose role. */
 story.current.background=1;
 StoryLine extended=story.current,ordinary=extended;
 ordinary.pose_flags&=(uint8_t)~(companion?0xf0u:0x0fu);
 CHECK(story_pose(&extended,companion)==4&&story_pose(&ordinary,companion)==0);
 unsigned extended_hash=isolated_hash(g);
 story.current=ordinary;unsigned ordinary_hash=isolated_hash(g);CHECK(extended_hash!=ordinary_hash);
 /* The first half of a cut must use the previous line's own extended flags. */
 story.previous=extended;story.phase=ST_SHOT;story.phase_tick=0;
 CHECK(isolated_hash(g)==extended_hash);
 story.previous=ordinary;CHECK(isolated_hash(g)==ordinary_hash);
 /* After the midpoint only the current line's pose may affect the image. */
 story.phase_tick=12;unsigned current_hash=isolated_hash(g);
 story.previous=extended;CHECK(isolated_hash(g)==current_hash);
 story.current=extended;CHECK(isolated_hash(g)!=current_hash);
 if(companion){
  /* YOU has no portrait, so the companion becomes the full-size actor. */
  story.phase=ST_READING;story.current.speaker=SC_YOU;
  unsigned listener_hash=isolated_hash(g);
  story.current.pose_flags=ordinary.pose_flags;CHECK(isolated_hash(g)!=listener_hash);
 }
 story=saved;story_render_release();
}
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
static void companion_pose_fixture(void){
 /* The new illustrated D05 no longer authors a fifth-expression companion.
  * Keep codec/drawing coverage as an explicit fixture, without forcing a new
  * expression into unrelated dialogue merely to satisfy the regression. */
 Game g;game_init(&g,28);CHECK(story_open(&g,0,0));
 memset(&story.current,0,sizeof story.current);
 story.current.speaker=SC_JULES;story.current.companion=SC_NIA;story.current.stage=ST_FACE;
 story.current.background=1;story.current.focus=128;story.current.pose_flags=0x10;
 strcpy(story.current.text,"Companion pose fixture.");story.current.length=(uint8_t)strlen(story.current.text);
 story.previous=story.current;story.page_start=story.page_index=0;story.page_end=story.current.length;
 CHECK(story_pose(&story.current,1)==4);extended_pose(&g,1);companion_fixture_passed=1;
}
int main(void){
 screen.before=screen.middle=screen.after=0xabc12345u;
 CHECK(STORY_PORTRAIT_WIDTH==256&&STORY_PORTRAIT_HEIGHT==264);
 CHECK(STORY_POSES>=5&&STORY_POSES<=64&&sizeof(StoryLine)==200&&sizeof(StoryState)==488);
 CHECK(offsetof(StoryLine,length)==6&&offsetof(StoryLine,pose_flags)==7&&offsetof(StoryLine,text)==8);
 CHECK(offsetof(StoryState,page_start)==485&&offsetof(StoryState,page_end)==486&&offsetof(StoryState,page_index)==487);
 CHECK(STORY_SCENE_HEIGHT==288&&STORY_DIALOG_Y==96&&STORY_DIALOG_TEXT_X==15&&STORY_DIALOG_TEXT_Y==124&&STORY_DIALOG_ROWS==3&&STORY_DIALOG_LINE_HEIGHT==14);
 uint8_t pixels[STORY_BACKGROUND_WIDTH*STORY_BACKGROUND_HEIGHT];uint16_t palette[256];int w,h;
 for(int asset=0;asset<STORY_ASSETS;asset++){
  CHECK(story_asset(asset,pixels,palette,sizeof pixels,&w,&h));CHECK(w==(asset<STORY_BACKGROUNDS?STORY_BACKGROUND_WIDTH:STORY_PORTRAIT_WIDTH)&&h==(asset<STORY_BACKGROUNDS?STORY_BACKGROUND_HEIGHT:STORY_PORTRAIT_HEIGHT));
  unsigned filled=0,empty=0;for(int i=0;i<w*h;i++){filled+=pixels[i]!=0;empty+=pixels[i]==0;}
  if(asset>=STORY_BACKGROUNDS)CHECK(filled>1000&&empty>100&&palette[0]==0);
 }
 CHECK(!story_asset(STORY_ASSETS,pixels,palette,sizeof pixels,&w,&h));CHECK(!story_asset(0,pixels,palette,10,&w,&h));
 compatibility();
 pagination_rules();
 directed_camera();
 companion_pose_fixture();
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
 CHECK(lines_seen==STORY_LINES&&pages_seen==expected_pages&&pages_seen>=lines_seen&&extended_roles[0]&&companion_fixture_passed);unsigned campaign_pages=pages_seen;for(int method=0;method<=ST_PLATE;method++)CHECK(stages[method]>0);
 for(int ch=0;ch<5;ch++)for(int ev=0;ev<25;ev++)CHECK(story_status(&g,ch,ev)==2);
 Save completed=g.save;CHECK(story_open(&g,0,5)&&story.replay);finish(&g,1);CHECK(!memcmp(&completed,&g.save,sizeof completed));press(&g,K_B);CHECK(campaign.chapter==0&&campaign.event==5);
 for(int skin=SKIN_FUSION;skin<SKIN_STORAGE_LIMIT;skin++){
  g.save.skin=(uint8_t)skin;
  for(int tab=0;tab<5;tab++){campaign.tab=(uint8_t)tab;campaign.revision++;g.mode=M_STORY;g.cursor=0;draw(&g);}
  CHECK(story_open(&g,4,24));for(int t=0;t<18;t++)tick(&g);press(&g,K_A);draw(&g);press(&g,K_B);draw(&g);press(&g,K_DOWN);press(&g,K_DOWN);press(&g,K_A);draw(&g);
 }
 CHECK(!story_visual_stats[0]);g.mode=M_GARAGE;g.cursor=0;draw(&g);g.mode=M_DRIVE;g.cursor=0;press(&g,K_A);draw(&g);CHECK(g.mode==M_RUN);
 printf("story: %u checks; all 125 scenes / %u lines / %u dialogue pages, exact font wrapping and text coverage, page reveal/advance rules, five staging methods, fifth poses for lead/companion/previous-line cuts, replay/abort/held-input rules, saved frontier0..125, v6-to-v7 rich-profile migration, three independent profiles and scratch restoration pass\n",checks,(unsigned)STORY_LINES,campaign_pages);
 return 0;
}
