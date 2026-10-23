/* Authored player replies must retain the preceding visible conversation. */
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef SHOT_CONTINUITY_RENDERER
#define SHOT_CONTINUITY_RENDERER "../src/dialogue_render.c"
#endif
#include SHOT_CONTINUITY_RENDERER
#include "render.h"

enum {W=256,H=192,GUARD=16};
typedef struct {
 uint16_t before[GUARD],top[W*H],middle[GUARD],bottom[W*H],after[GUARD];
} Frames;
static Frames screen,baseline,reply;
static unsigned checks,reply_cases,cold_cases,fade_cases;
static const char*context="startup";
#define CHECK(v) do{checks++;if(!(v)){fprintf(stderr,"shot continuity [%s]:%d: %s\n",context,__LINE__,#v);exit(1);}}while(0)

static void press(Game*g,unsigned key){Input in={key,key};game_update(g,&in);}
static void tick(Game*g){Input in={0,0};game_update(g,&in);}
static void draw(Game*g){
 for(int n=0;n<GUARD;n++)screen.before[n]=screen.middle[n]=screen.after[n]=(uint16_t)(0xa6c0+n);
 Save saved=g->save;StoryState untouched=story;
 render_game(g,screen.top,screen.bottom);
 for(int n=0;n<GUARD;n++)CHECK(screen.before[n]==0xa6c0+n&&screen.middle[n]==0xa6c0+n&&screen.after[n]==0xa6c0+n);
 CHECK(!memcmp(&saved,&g->save,sizeof saved));
 CHECK(!memcmp(&untouched,&story,sizeof untouched));
 CHECK(!story_visual_stats[0]);
}
static int same_scene(const Frames*a,const Frames*b){
 return !memcmp(a->top,b->top,sizeof a->top)&&
        !memcmp(a->bottom,b->bottom,W*STORY_DIALOG_Y*sizeof(uint16_t));
}
static void settle(Game*g){
 unsigned steps=0;
 while(story.phase!=ST_READING){CHECK(story.active&&!story.failed&&++steps<40);tick(g);}
 /* Let typewriter text finish without directly altering any story field. */
 while(story.revealed<story.page_end){CHECK(++steps<150);tick(g);}
 CHECK(!story.pause&&g->mode==M_DIALOG);
}
static void seek(Game*g,unsigned line){
 unsigned steps=0;
 for(;;){
  settle(g);CHECK(story.line<=line);
  if(story.line==line)break;
  CHECK(++steps<40);press(g,K_A);
 }
}
static void open(Game*g,unsigned event){
 game_init(g,0x73686f74u);
 /* Native codec test profile: earned frontier is deliberately a review fixture. */
 g->save.reserved[0]=125;
 story_render_release();CHECK(story_open(g,0,(int)event));
 CHECK(story.replay&&story.count>3&&sizeof(StoryState)==488);
 CHECK(offsetof(StoryState,page_start)==485);
}
static void loaded_pair(int first,int second){
 CHECK(first>=76&&second>=76&&first!=second);
 CHECK((people_id[0]==first&&people_id[1]==second)||
       (people_id[0]==second&&people_id[1]==first));
}
static void capture_reply(Game*g,unsigned before_line,int first,int second){
 seek(g,before_line);CHECK(story.current.speaker<STORY_PEOPLE);
 draw(g);loaded_pair(first,second);baseline=screen;
 unsigned loads=story_visual_stats[2],line=story.line;
 press(g,K_A);settle(g);
 CHECK(story.line==line+1&&story.current.speaker==SC_YOU);
 draw(g);
 /* Independent visual oracle: the prior real authored page already contains
  * the correct people, poses, direction, scale, placement and emphasis. */
 CHECK(same_scene(&baseline,&screen));
 loaded_pair(first,second);CHECK(story_visual_stats[2]==loads);
 reply=screen;reply_cases++;
 draw(g);CHECK(same_scene(&reply,&screen));
 /* A direct render of the reply must not depend on a previous drawn frame. */
 story_render_release();draw(g);CHECK(same_scene(&reply,&screen));
 loaded_pair(first,second);cold_cases++;
}
static void pause_replay_abort(Game*g,unsigned event,unsigned before_line,int first,int second){
 Save saved=g->save;
 press(g,K_B);CHECK(story.pause);
 uint16_t age=story.age,ticks=story.ticks;
 for(int n=0;n<12;n++)tick(g);
 CHECK(story.age==age&&story.ticks==ticks);
 press(g,K_B);CHECK(!story.pause);draw(g);CHECK(same_scene(&reply,&screen));
 press(g,K_B);press(g,K_DOWN);press(g,K_A);
 CHECK(story.active&&story.line==0&&story.phase==ST_FADEIN&&story.event==event);
 capture_reply(g,before_line,first,second);
 CHECK(!memcmp(&saved,&g->save,sizeof saved));
 press(g,K_B);press(g,K_DOWN);press(g,K_DOWN);press(g,K_A);
 CHECK(!story.active&&g->mode==M_STORY);
 CHECK(!memcmp(&saved,&g->save,sizeof saved));
}
static void examples(void){
 Game g;
 context="D01 pages 6 to 7";
 open(&g,0);
 int nia_gear=story_quarter_portraits[1][2][SC_NIA];
 int mina_default=story_quarter_portraits[0][0][SC_MINA];
 capture_reply(&g,5,nia_gear,mina_default);
 pause_replay_abort(&g,0,5,nia_gear,mina_default);

 context="D03 pages 3 to 4";
 open(&g,2);
 capture_reply(&g,2,story_quarter_portraits[1][1][SC_NIA],story_quarter_portraits[0][1][SC_VALE]);

 context="D05 pages 1 to 2, three-person scene";
 open(&g,4);
 capture_reply(&g,0,story_quarter_portraits[1][1][SC_MINA],story_quarter_portraits[0][1][SC_ELI]);
 /* The scene names three people; the already visible Mina/Eli pair must be
  * retained instead of substituting Nia merely because she is listed first. */
 CHECK(people_id[0]!=story_quarter_portraits[1][1][SC_NIA]&&people_id[1]!=story_quarter_portraits[1][1][SC_NIA]);

 context="D05 pages 8 to 9, skeptical pose retained";
 capture_reply(&g,7,story_quarter_portraits[1][4][SC_NIA],story_quarter_portraits[0][1][SC_ELI]);

 context="D14 pages 2 to 3, reply before a cut";
 open(&g,13);
 int nia_casual=story_quarter_portraits[1][1][SC_NIA];
 int mina_casual=story_quarter_portraits[0][1][SC_MINA];
 capture_reply(&g,1,nia_casual,mina_casual);
 unsigned loads=story_visual_stats[2];
 press(&g,K_A);
 CHECK(story.line==3&&story.phase==ST_SHOT&&!story.phase_tick);
 CHECK(story.previous.speaker==SC_YOU&&story.previous.stage==ST_FACE&&story.current.stage==ST_PAN);
 /* Tick zero is the previous shot at full brightness. Its cue belongs to
  * line-1, even though progression already points at the new shot. */
 draw(&g);CHECK(same_scene(&reply,&screen));
 loaded_pair(nia_casual,mina_casual);CHECK(story_visual_stats[2]==loads);fade_cases++;
 for(int n=0;n<16;n++){tick(&g);draw(&g);}
 CHECK(story.phase==ST_READING&&story.line==3&&!story_visual_stats[0]);
 story_render_release();
}
int main(void){
 CHECK(STORY_QUARTER_AVAILABLE);
 examples();
 CHECK(reply_cases==6&&cold_cases==6&&fade_cases==1);
 printf("shot continuity: %u checks; %u authored reply comparisons, %u cold-cache replies, three-person cast, skeptical pose, pause/replay/abort and %u previous-shot fade pass; original story/save state preserved\n",checks,reply_cases,cold_cases,fade_cases);
 return 0;
}
