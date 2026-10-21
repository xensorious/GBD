/* Indexed cutscene staging, painted people, soft cuts and an ordinary bitmap
 * dialogue window. All large images lease the existing sprite-cache storage. */
#include "story.h"
#include "assets.h"
#include "career.h"
#include <stdio.h>
#include <string.h>
#include "story_visuals.h"
static uint8_t *back[2],*people[2];
static uint16_t *back_palette[2],*people_palette[2];
static int leased,back_id[2]={-1,-1},people_id[2]={-1,-1},background_slot;
static int camera_q8,last_background=-1,camera_line=-1;
volatile uint32_t story_visual_stats[5]; /* archive failures, bg loads, portraits, camera, stage */
static int limit(int n,int lo,int hi){return n<lo?lo:n>hi?hi:n;}
static uint16_t mix_color(uint16_t a,uint16_t b,int alpha){
 int inv=256-alpha;return (uint16_t)(0x8000|((((a&31)*alpha+(b&31)*inv)>>8)&31)|
 (((((a>>5&31)*alpha+(b>>5&31)*inv)>>8)&31)<<5)|(((((a>>10&31)*alpha+(b>>10&31)*inv)>>8)&31)<<10));
}
void story_render_release(void){
 if(leased)gfx_story_release();
 leased=0;last_background=-1;camera_line=-1;back_id[0]=back_id[1]=people_id[0]=people_id[1]=-1;
}
static int lease(void){
 if(leased)return 1;
 const unsigned portrait_bytes=STORY_PORTRAIT_SIZE*STORY_PORTRAIT_SIZE;
 const unsigned required=2*512*216+2*portrait_bytes+4*256*sizeof(uint16_t);
 unsigned bytes;uint8_t*base=gfx_story_scratch(&bytes);if(bytes<required)return 0;
 back[0]=base;back[1]=base+512*216;people[0]=base+2*512*216;people[1]=people[0]+portrait_bytes;
 uint16_t*pal=(uint16_t*)(people[1]+portrait_bytes);back_palette[0]=pal;back_palette[1]=pal+256;people_palette[0]=pal+512;people_palette[1]=pal+768;
 leased=1;return 1;
}
static int background(int id){
 for(int i=0;i<2;i++)if(back_id[i]==id)return i;
 int slot=1-background_slot,w,h;
 if(!story_asset(id,back[slot],back_palette[slot],512*216,&w,&h)||w!=512||h!=216){story_visual_stats[0]++;return -1;}
 back_id[slot]=id;story_visual_stats[1]++;return slot;
}
static int person(int id,int protect){
 if(id<0||id>=STORY_PORTRAITS)return -1;
 for(int i=0;i<2;i++)if(people_id[i]==id)return i;
 int slot=protect>=0&&people_id[0]==protect?1:0,w,h;
 if(!story_asset(STORY_BACKGROUNDS+id,people[slot],people_palette[slot],STORY_PORTRAIT_SIZE*STORY_PORTRAIT_SIZE,&w,&h)||w!=STORY_PORTRAIT_SIZE||h!=STORY_PORTRAIT_SIZE){story_visual_stats[0]++;return -1;}
 people_id[slot]=id;story_visual_stats[2]++;return slot;
}
static void actor(Surface*s,int slot,int x,int y,int width,int height,int opacity,int light){
 if(slot<0||width<=0||height<=0||width>STORY_PORTRAIT_SIZE||height>STORY_PORTRAIT_SIZE)return;
 uint16_t palette[256];palette[0]=0;
 for(int i=1;i<256;i++)palette[i]=mix_color(people_palette[slot][i],C(0,0,0),light);
 int native=width==STORY_PORTRAIT_SIZE&&height==STORY_PORTRAIT_SIZE;
 int columns[STORY_PORTRAIT_SIZE];
 if(!native)for(int xx=0;xx<width;xx++)columns[xx]=xx*STORY_PORTRAIT_SIZE/width;
 int left=limit(-x,0,width),right=limit(s->w-x,0,width),top=limit(-y,0,height),end=limit(s->h-y,0,height);
 if(left>=right||top>=end)return;
 for(int yy=top;yy<end;yy++){
  const uint8_t*p=people[slot]+(native?yy:yy*STORY_PORTRAIT_SIZE/height)*STORY_PORTRAIT_SIZE;
  uint16_t*d=s->pixels+(y+yy)*s->w+x+left;
  if(native){
   /* One indexed source pixel becomes one screen pixel, without resampling. */
   p+=left;
   for(int xx=left;xx<right;xx++,d++){uint8_t index=*p++;if(index){uint16_t c=palette[index];*d=opacity>=256?c:mix_color(c,*d,opacity);}}
  }else{
   for(int xx=left;xx<right;xx++,d++){uint8_t index=p[columns[xx]];if(index){uint16_t c=palette[index];*d=opacity>=256?c:mix_color(c,*d,opacity);}}
  }
 }
}
static void overlay(Surface*s,int x,int y,int w,int h,uint16_t c,int alpha){
 for(int yy=limit(y,0,s->h);yy<limit(y+h,0,s->h);yy++)for(int xx=limit(x,0,s->w);xx<limit(x+w,0,s->w);xx++)s->pixels[yy*s->w+xx]=mix_color(c,s->pixels[yy*s->w+xx],alpha);
}
static void text_center(Surface*s,int y,const char*text,uint16_t color){gfx_text(s,(256-gfx_text_width(text))/2,y,text,color);}
static void panel(Surface*s,int x,int y,int w,int h){
 gfx_card(s,x,y,w,h,C(10,36,47),C(96,150,154));gfx_rect(s,x+2,y+2,w-4,1,C(157,207,192));
 for(int yy=y+4;yy<y+h-3;yy++)for(int xx=x+3;xx<x+w-3;xx++){
  unsigned grain=(unsigned)(xx*13+yy*7+(xx/9)*11);int wave=((xx/13+yy/5)%5==0?7:0);
  s->pixels[yy*s->w+xx]=C(13+(grain&3),43+(grain%5)+wave,53+(grain%7)+wave);
 }
}
int story_text_lines(const char*text,int width){
 int rows=1,line=0;const char*p=text;
 while(*p){char word[STORY_TEXT_MAX];int n=0;while(*p&&*p!=' '&&*p!='\n'){if(n<STORY_TEXT_MAX-1)word[n++]=*p;p++;}word[n]=0;
  int ww=gfx_text_width(word);if(line&&line+ww>width){rows++;line=0;}line+=ww;
  if(*p=='\n'){rows++;line=0;p++;}else if(*p==' '){line+=gfx_text_width(" ");p++;}
 }return rows;
}
/* Subject captions identify an insert, rather than labelling unrelated scenery.
 * Source coordinates and the shot-by-shot cast are authored in staging.json. */
static const char*subject_name(int bg,int right){
 return bg>=6&&bg<STORY_BACKGROUNDS?story_subjects[bg][right]:NULL;
}
static int portrait_id(int actor_id,int pose){return actor_id>=0&&actor_id<STORY_PEOPLE?story_portraits[pose][actor_id]:-1;}
void story_draw(const Game*g,uint16_t*top,uint16_t*bottom){
 Surface image={top,256,192},dialog={bottom,256,192};
 const StoryLine*line=&story.current;
 int brightness=256;
 if(story.phase==ST_FADEIN)brightness=story.phase_tick*256/16;
 else if(story.phase==ST_SHOT){if(story.phase_tick<8){line=&story.previous;brightness=(8-story.phase_tick)*32;}else brightness=(story.phase_tick-8)*32;}
 else if(story.phase==ST_ENDING)brightness=(16-story.phase_tick)*16;
 else if(story.phase==ST_CLEAR)brightness=story.phase_tick<16?story.phase_tick*16:256;
 int okay=lease();int bg=okay?background(line->background):-1;okay=bg>=0;
 int speaking=line->speaker<STORY_PEOPLE;
 int face=speaking?line->speaker:line->companion,other=speaking?line->companion:255;
 int face_pose=(line->focus>>(speaking?9:11))&3,other_pose=(line->focus>>11)&3;
 int face_id=portrait_id(face,face_pose),other_id=portrait_id(other,other_pose);
 int lead=-1,companion=-1;
 if(okay&&line->stage==ST_FACE){lead=person(face_id,other_id);if(other<STORY_PEOPLE&&other!=face)companion=person(other_id,face_id);}
 else if(okay&&speaking)lead=person(face_id,-1);
 if(okay){
  background_slot=bg;
  int target=(int)(line->focus&STORY_FOCUS_X)*256;
  if(last_background!=line->background){camera_q8=target;last_background=line->background;camera_line=-1;}
  int camera_shot=(int)story.line*2+(line==&story.previous);
  if(camera_line!=camera_shot){if(!(line->focus&STORY_FOCUS_MOVE))camera_q8=target;camera_line=camera_shot;}
  /* A one-way move to the named subject, then a hold. No idle oscillation and
   * no speaker-derived offsets that can move the subject out of frame. */
  if(!story.pause){int difference=target-camera_q8;camera_q8+=difference/10;if(difference<10&&difference>-10)camera_q8=target;}
  int view_width=line->stage==ST_MONTAGE?276:256,view_height=view_width*3/4;
  int cx=limit(camera_q8/256,0,512-view_width),cy=(216-view_height)/2;
  uint16_t palette[256];for(int i=0;i<256;i++)palette[i]=mix_color(back_palette[bg][i],C(0,0,0),brightness);
  int cols[256];for(int x=0;x<256;x++)cols[x]=cx+x*view_width/256;
  for(int y=0;y<192;y++){const uint8_t*p=back[bg]+(cy+y*view_height/192)*512;uint16_t*d=top+y*256;for(int x=0;x<256;x++)d[x]=palette[p[cols[x]]];}
  if(line->stage==ST_FACE){
   int light=brightness,breath=(story.ticks/35)%2;
   if(speaking){
    int left=other>=STORY_PEOPLE||line->speaker<other;
    actor(&image,companion,left?137:-2,72,121,121,210,light*160/256);
    int opacity=story.age<7?160+(int)story.age*96/7:256;
    actor(&image,lead,left?0:256-STORY_PORTRAIT_SIZE,192-STORY_PORTRAIT_SIZE-breath,STORY_PORTRAIT_SIZE,STORY_PORTRAIT_SIZE,opacity,light);
   }else actor(&image,lead,(256-STORY_PORTRAIT_SIZE)/2,192-STORY_PORTRAIT_SIZE-breath,STORY_PORTRAIT_SIZE,STORY_PORTRAIT_SIZE,256,light);
  }
  if(line->stage==ST_MONTAGE||line->stage==ST_RADIO){overlay(&image,0,0,256,14,C(1,6,13),190);overlay(&image,0,178,256,14,C(1,6,13),190);}
  story_visual_stats[3]=(uint32_t)cx;story_visual_stats[4]=line->stage;
 }else{gfx_rect(&image,0,0,256,192,C(8,18,31));text_center(&image,84,"Scene artwork unavailable",C(240,182,117));}
 char title[70];snprintf(title,sizeof title,"%s.%02d  /  %s",game_class_name(story.chapter),story.event+1,career_event_name(story.chapter,story.event));
 overlay(&image,5,5,246,16,C(3,10,20),150);gfx_text_small(&image,11,9,title,C(231,238,225));
 if(line->stage==ST_MONTAGE)gfx_text_small(&image,82,181,"STORY MONTAGE",C(232,210,161));
 if(line->stage==ST_RADIO)gfx_text_small(&image,11,181,"SUNLINE / CREW RADIO",C(154,214,209));
 const char*subject=subject_name(line->background,(line->focus&STORY_FOCUS_SUBJECT)!=0);
 if(subject&&line->stage!=ST_FACE){int y=(line->stage==ST_MONTAGE||line->stage==ST_RADIO)?161:176;
  overlay(&image,5,y-3,limit(gfx_text_width(subject)+10,0,246),13,C(3,10,20),150);gfx_text_small(&image,10,y,subject,C(220,220,195));}
 if(line->stage==ST_FACE&&story_locations[line->background][0]){
  const char*place=story_locations[line->background];overlay(&image,5,173,limit(gfx_text_width(place)+10,0,246),14,C(3,10,20),145);
  gfx_text_small(&image,10,177,place,C(220,220,195));
 }
 gfx_rect(&dialog,0,0,256,192,C(5,14,26));
 for(int y=0;y<192;y+=4)gfx_rect(&dialog,0,y,256,1,C(8,20,31));
 panel(&dialog,5,12,246,148);
 int portrait=line->stage!=ST_FACE&&speaking&&lead>=0&&story_text_lines(line->text,163)<=8;
 int tx=portrait?79:15,tw=portrait?163:226;
 gfx_rect(&dialog,11,18,234,19,C(9,30,39));gfx_text(&dialog,15,21,story_cast_names[line->speaker],C(245,217,151));
 char pages[24];snprintf(pages,sizeof pages,"%d / %d",story.line+1,story.count);gfx_text_small(&dialog,204,25,pages,C(141,187,185));
 if(portrait){
  gfx_card(&dialog,12,43,59,63,C(13,29,40),C(80,119,123));actor(&dialog,lead,14,46,55,55,256,256);
  if(line->stage==ST_RADIO)for(int i=0;i<12;i++){int h=story.revealed<line->length?3+(int)((story.age/2+(unsigned)i*7)%13):3;gfx_rect(&dialog,17+i*4,124-h/2,2,h,C(112,201,187));}
 }
 char visible[STORY_TEXT_MAX];unsigned count=story.phase==ST_READING?story.revealed:0;if(count>line->length)count=line->length;
 if(story.phase==ST_CLEAR||story.phase==ST_ENDING)count=line->length;
 memcpy(visible,line->text,count);visible[count]=0;
 gfx_wrap(&dialog,tx+1,45,visible,tw,13,8,C(4,15,23));gfx_wrap(&dialog,tx,44,visible,tw,13,8,C(239,243,225));
 if(story.phase==ST_READING&&story.revealed>=line->length){gfx_line(&dialog,231,148,235,152,C(236,205,129));gfx_line(&dialog,235,152,239,148,C(236,205,129));}
 gfx_text_small(&dialog,12,172,career_chapters[story.chapter].name,C(112,164,171));
 gfx_text_small(&dialog,180,172,story.replay?"REPLAY":"STORY EPISODE",C(112,164,171));
 if(story.phase==ST_CLEAR){
  panel(&dialog,9,20,238,151);text_center(&dialog,34,story.replay?"SCENE REPLAYED":"STORY CLEARED",C(246,215,143));
  gfx_wrap(&dialog,24,58,career_event_name(story.chapter,story.event),208,14,2,C(237,242,228));
  int done=story_completed(g);char record[60];snprintf(record,sizeof record,"%d / 125 story events",done);text_center(&dialog,91,record,C(155,206,198));
  if(story.replay)text_center(&dialog,111,"Your record is unchanged.",C(165,186,191));
  else if(done>=125)text_center(&dialog,111,"Sunline's door stays open.",C(165,186,191));
  else{snprintf(record,sizeof record,"Next: %s.%02d",game_class_name(done/25),done%25+1);text_center(&dialog,111,record,C(165,186,191));}
  gfx_card(&dialog,49,138,158,22,C(26,87,92),C(140,203,190));text_center(&dialog,143,"Continue",C(245,237,202));
 }
 if(story.pause){
  overlay(&image,0,0,256,192,C(2,8,16),100);panel(&dialog,22,30,212,134);text_center(&dialog,41,story.failed?"SCENE UNAVAILABLE":"SCENE PAUSED",C(246,215,143));
  static const char*const choices[3]={"Resume","Restart scene","Return to map"};
  for(int i=0;i<3;i++){int selected=story.cursor==i;gfx_card(&dialog,36,65+i*28,184,23,selected?C(32,96,99):C(9,29,39),selected?C(159,214,192):C(48,86,98));text_center(&dialog,71+i*28,choices[i],selected?C(248,235,188):C(181,209,211));}
 }
}
