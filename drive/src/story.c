#include "story.h"
#include "career.h"
#include <string.h>
StoryState story;
const char*const story_cast_names[14]={"Nia","Jules","Calder","Imani","Marisol","Kade","Rook","Ren","Aster","Sable","Voss","Mina","You","Later..."};
static int index_of(int ch,int ev){return ch*25+ev;}
int story_completed(const Game*g){return g->save.reserved[0]<=125?g->save.reserved[0]:0;}
int story_status(const Game*g,int ch,int ev){int i=index_of(ch,ev),done=story_completed(g);return i<done?2:i==done?1:0;}
void story_reset(void){memset(&story,0,sizeof story);story.revision=1;}
static int read_line(int n,StoryLine*out){
 uint8_t h[8];if(n<0||n>=story.count||!story_read(story.offsets[n],h,sizeof h))return 0;
 memset(out,0,sizeof *out);out->speaker=h[0];out->companion=h[1];out->stage=h[2];out->background=h[3];
 out->focus=(uint16_t)(h[4]|h[5]<<8);out->length=(uint16_t)(h[6]|h[7]<<8);
 if(out->speaker>=14||(out->companion>=12&&out->companion!=255)||out->stage>3||out->background>=6||out->focus>256||out->length>=STORY_TEXT_MAX)return 0;
 return story_read(story.offsets[n]+8,out->text,out->length);
}
int story_open(Game*g,int ch,int ev){
 int index=index_of(ch,ev);if(ch<0||ch>=5||ev<0||ev>=25||index>story_completed(g))return 0;
 uint32_t header[8],record[3];uint16_t info[2];
 if(!story_read(0,header,sizeof header)||header[0]!=0x4f545343u||header[1]!=0x32305952u||header[3]!=125||
    !story_read(header[2]+(unsigned)index*12,record,sizeof record)||!story_read(record[0],info,sizeof info)||info[0]<4||info[0]>STORY_MAX_LINES)return 0;
 uint32_t revision=story.revision+1;memset(&story,0,sizeof story);story.revision=revision;
 story.chapter=(uint8_t)ch;story.event=(uint8_t)ev;story.count=(uint8_t)info[0];story.replay=(uint8_t)(index<story_completed(g));
 if(!story_read(record[0]+4,story.offsets,(unsigned)story.count*4)||!read_line(0,&story.current)){story.failed=1;return 0;}
 story.previous=story.current;story.active=1;story.phase=ST_FADEIN;g->mode=M_DIALOG;g->notice_timer=0;return 1;
}
static void return_map(Game*g,int next){
 int done=story_completed(g);campaign.chapter=story.chapter;campaign.event=story.event;
 if(next&&!story.replay){campaign.chapter=(uint8_t)(done<125?done/25:4);campaign.event=(uint8_t)(done<125?done%25:24);}
 story.active=0;career_enter(g);if(!next||story.replay){campaign.chapter=story.chapter;campaign.event=story.event;}g->cursor=1;
}
static void advance_line(void){
 StoryLine next;if(!read_line(story.line+1,&next)){story.failed=1;story.pause=1;story.cursor=2;return;}
 story.previous=story.current;story.current=next;story.line++;story.revealed=0;story.age=0;
 if(story.current.background!=story.previous.background||story.current.stage!=story.previous.stage){story.phase=ST_SHOT;story.phase_tick=0;}
 story.revision++;
}
void story_tick(Game*g,const Input*in){
 if(!story.active){g->mode=M_STORY;return;}
 if(story.phase==ST_CLEAR){if(story.phase_tick<16){story.phase_tick++;story.revision++;}if(in->pressed&(K_A|K_B)){return_map(g,1);}return;}
 if(story.pause){
  if(in->pressed&K_B){story.pause=0;story.revision++;return;}
  int dy=((in->pressed&K_DOWN)?1:0)-((in->pressed&K_UP)?1:0);story.cursor=(uint8_t)((story.cursor+3+dy)%3);
  if(in->pressed&K_A){
   if(story.cursor==0&&!story.failed)story.pause=0;
   else if(story.cursor==1){int ch=story.chapter,ev=story.event;if(!story_open(g,ch,ev)){story.failed=1;story.pause=1;story.cursor=2;}}
   else if(story.cursor==2)return_map(g,0);
  }
  story.revision++;return;
 }
 if(in->pressed&(K_B|K_START)){story.pause=1;story.cursor=0;story.revision++;return;}
 story.ticks++;if(story.age<65535)story.age++;
 if(story.phase==ST_FADEIN||story.phase==ST_SHOT){
  if(++story.phase_tick>=16){story.phase=ST_READING;story.phase_tick=0;story.age=0;}story.revision++;return;
 }
 if(story.phase==ST_ENDING){
  if(++story.phase_tick>=16){
   int index=index_of(story.chapter,story.event);
   if(!story.replay&&index==story_completed(g)&&index<125){g->save.reserved[0]=(uint8_t)(index+1);g->save_requested=1;}
   story.phase=ST_CLEAR;story.phase_tick=0;
  }story.revision++;return;
 }
 if(in->pressed&K_A){
  if(story.revealed<story.current.length)story.revealed=story.current.length;
  else if(story.line+1<story.count)advance_line();
  else{story.phase=ST_ENDING;story.phase_tick=0;}
  story.revision++;return;
 }
 if(story.revealed<story.current.length){story.revealed=(uint16_t)(story.revealed+2);if(story.revealed>story.current.length)story.revealed=story.current.length;story.revision++;}
}
