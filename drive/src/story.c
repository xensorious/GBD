#include "story.h"
#include "career.h"
#include <string.h>
StoryState story;
const char*const story_cast_names[STORY_SPEAKERS]={"Nia","Jules","Calder","Imani","Marisol","Kade","Rook","Ren","Aster","Sable","Voss","Mina","Eli","Mrs. Vale","You","Later..."};
/* Generated from the same font used by gfx_text, without linking the artwork
 * into progression-only fixtures. Wrapping follows gfx_wrap's word boundaries. */
static const uint8_t text_advance[96]=STORY_FONT_ADVANCE_INITIALIZER;
unsigned story_text_advance(unsigned ch){return text_advance[(ch>=32&&ch<=127?ch:'?')-32];}
static unsigned word_width(const char*text,unsigned*length){
 unsigned n=0,width=0;while(text[n]&&text[n]!=' '&&text[n]!='\n'){width+=story_text_advance((unsigned char)text[n]);n++;}
 *length=n;return width;
}
int story_text_lines(const char*text,int width){
 int rows=1;unsigned used=0,pixels=0;
 while(*text){
  if(*text=='\n'){if(text[1])rows++;used=pixels=0;text++;continue;}
  if(*text==' '){if(used){used++;pixels+=story_text_advance(' ');}text++;continue;}
  unsigned length,word=word_width(text,&length);
  if(used&&pixels+word>(unsigned)(width>0?width:0)){rows++;used=pixels=0;}
  used+=length;pixels+=word;text+=length;
 }
 return rows;
}
unsigned story_text_page_end(const char*text,unsigned start){
 unsigned at=start,rows=1,used=0,pixels=0;
 while(text[at]){
  if(text[at]=='\n'){at++;if(rows==STORY_DIALOG_ROWS)return at;rows++;used=pixels=0;continue;}
  if(text[at]==' '){if(used){used++;pixels+=story_text_advance(' ');}at++;continue;}
  unsigned length,word=word_width(text+at,&length);
  if(used&&pixels+word>STORY_DIALOG_TEXT_WIDTH){if(rows==STORY_DIALOG_ROWS)return at;rows++;used=pixels=0;}
  used+=length;pixels+=word;at+=length;
 }
 return at;
}
static void start_pages(void){story.page_start=story.page_index=0;story.page_end=(uint8_t)story_text_page_end(story.current.text,0);story.revealed=0;}
static int index_of(int ch,int ev){return ch*25+ev;}
int story_completed(const Game*g){return g->save.reserved[0]<=125?g->save.reserved[0]:0;}
int story_status(const Game*g,int ch,int ev){int i=index_of(ch,ev),done=story_completed(g);return i<done?2:i==done?1:0;}
void story_reset(void){memset(&story,0,sizeof story);story.revision=1;}
static int read_line(int n,StoryLine*out){
 uint8_t h[8];if(n<0||n>=story.count||!story_read(story.offsets[n],h,sizeof h))return 0;
 memset(out,0,sizeof *out);out->speaker=h[0];out->companion=h[1];out->stage=h[2];out->background=h[3];
 out->focus=(uint16_t)(h[4]|h[5]<<8);out->length=h[6];out->pose_flags=h[7];
 if(out->speaker>=STORY_SPEAKERS||(out->companion>=STORY_PEOPLE&&out->companion!=255)||out->stage>ST_PLATE||out->background>=STORY_BACKGROUNDS||
    (out->focus&STORY_FOCUS_X)>256||(out->focus&~(STORY_FOCUS_MOVE|STORY_FOCUS_SUBJECT|STORY_FOCUS_X|STORY_FOCUS_POSE|STORY_FOCUS_COMPANION_POSE))||
    !out->length||out->length>=STORY_TEXT_MAX||story_pose(out,0)>=STORY_POSES||story_pose(out,1)>=STORY_POSES)return 0;
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
 story.previous=story.current;start_pages();story.active=1;story.phase=ST_FADEIN;g->mode=M_DIALOG;g->notice_timer=0;return 1;
}
static void return_map(Game*g,int next){
 int done=story_completed(g);campaign.chapter=story.chapter;campaign.event=story.event;
 if(next&&!story.replay){campaign.chapter=(uint8_t)(done<125?done/25:4);campaign.event=(uint8_t)(done<125?done%25:24);}
 story.active=0;career_enter(g);if(!next||story.replay){campaign.chapter=story.chapter;campaign.event=story.event;}g->cursor=1;
}
static void advance_line(void){
 StoryLine next;if(!read_line(story.line+1,&next)){story.failed=1;story.pause=1;story.cursor=2;return;}
 story.previous=story.current;story.current=next;story.line++;start_pages();story.age=0;
 int jump=(int)(story.current.focus&STORY_FOCUS_X)-(int)(story.previous.focus&STORY_FOCUS_X);
 if(story.current.background!=story.previous.background||story.current.stage!=story.previous.stage||
    (!(story.current.focus&STORY_FOCUS_MOVE)&&(jump>64||jump< -64))){story.phase=ST_SHOT;story.phase_tick=0;}
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
  if(story.revealed<story.page_end)story.revealed=story.page_end;
  else if(story.page_end<story.current.length){
   story.page_start=story.page_end;story.page_end=(uint8_t)story_text_page_end(story.current.text,story.page_start);
   story.page_index++;story.revealed=story.page_start;
  }
  else if(story.line+1<story.count)advance_line();
  else{story.phase=ST_ENDING;story.phase_tick=0;}
  story.revision++;return;
 }
 if(story.revealed<story.page_end){story.revealed=(uint16_t)(story.revealed+2);if(story.revealed>story.page_end)story.revealed=story.page_end;story.revision++;}
}
