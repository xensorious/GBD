/* Indexed cutscene staging, painted people, soft cuts and an ordinary bitmap
 * dialogue window. All large images lease the existing sprite-cache storage. */
#include "story.h"
#include "assets.h"
#include "career.h"
#include <stdio.h>
#include <string.h>
#include "story_visuals.h"
#ifndef STORY_QUARTER_AVAILABLE
#define STORY_QUARTER_AVAILABLE 0
#endif
static uint8_t *back[1],*people[2];
static uint16_t *back_palette[1],*people_palette[2];
static int leased,back_id[1]={-1},people_id[2]={-1,-1};
static uint16_t people_left[2],people_right[2],people_foot[2];
static uint16_t people_stage_left[2],people_stage_right[2];
static uint16_t people_scale[2]={256,256};
static int people_character[2]={-1,-1};
static int camera_q8,last_background=-1,camera_line=-1;
volatile uint32_t story_visual_stats[5]; /* archive failures, bg loads, portraits, camera, stage */
static int limit(int n,int lo,int hi){return n<lo?lo:n>hi?hi:n;}
static uint16_t mix_color(uint16_t a,uint16_t b,int alpha){
 int inv=256-alpha;return (uint16_t)(0x8000|((((a&31)*alpha+(b&31)*inv)>>8)&31)|
 (((((a>>5&31)*alpha+(b>>5&31)*inv)>>8)&31)<<5)|(((((a>>10&31)*alpha+(b>>10&31)*inv)>>8)&31)<<10));
}
void story_render_release(void){
 if(leased)gfx_story_release();
 leased=0;last_background=-1;camera_line=-1;back_id[0]=people_id[0]=people_id[1]=-1;
}
static int lease(void){
 if(leased)return 1;
 const unsigned portrait_bytes=STORY_PORTRAIT_WIDTH*STORY_PORTRAIT_HEIGHT;
 const unsigned background_bytes=STORY_BACKGROUND_WIDTH*STORY_BACKGROUND_HEIGHT;
 const unsigned required=background_bytes+2*portrait_bytes+3*256*sizeof(uint16_t);
 unsigned bytes;uint8_t*base=gfx_story_scratch(&bytes);if(bytes<required)return 0;
 back[0]=base;people[0]=base+background_bytes;people[1]=people[0]+portrait_bytes;
 uint16_t*pal=(uint16_t*)(people[1]+portrait_bytes);back_palette[0]=pal;people_palette[0]=pal+256;people_palette[1]=pal+512;
 leased=1;return 1;
}
static int background(int id){
 if(back_id[0]==id)return 0;
 int w,h;
 if(!story_asset(id,back[0],back_palette[0],STORY_BACKGROUND_WIDTH*STORY_BACKGROUND_HEIGHT,&w,&h)||w!=STORY_BACKGROUND_WIDTH||h!=STORY_BACKGROUND_HEIGHT){story_visual_stats[0]++;return -1;}
 back_id[0]=id;story_visual_stats[1]++;return 0;
}
static int person(int id,int protect){
 if(id<0||id>=STORY_PORTRAITS)return -1;
 for(int i=0;i<2;i++)if(people_id[i]==id)return i;
 int slot=protect>=0&&people_id[0]==protect?1:0,w,h;
 if(!story_asset(STORY_BACKGROUNDS+id,people[slot],people_palette[slot],STORY_PORTRAIT_WIDTH*STORY_PORTRAIT_HEIGHT,&w,&h)||w!=STORY_PORTRAIT_WIDTH||h!=STORY_PORTRAIT_HEIGHT){story_visual_stats[0]++;return -1;}
 int left=w,right=0,foot_left=w,foot_right=0;
 for(int y=0;y<h;y++)for(int x=0;x<w;x++)if(people[slot][y*w+x]){
  if(x<left)left=x;
  if(x+1>right)right=x+1;
  if(y>=h-16){if(x<foot_left)foot_left=x;if(x+1>foot_right)foot_right=x+1;}
 }
 people_left[slot]=(uint16_t)(left<right?left:0);people_right[slot]=(uint16_t)(left<right?right:w);
 people_foot[slot]=(uint16_t)(foot_left<foot_right?(foot_left+foot_right)/2:(left+right)/2);
 people_stage_left[slot]=people_left[slot];people_stage_right[slot]=people_right[slot];
#if defined(STORY_POSE_ENVELOPES_AVAILABLE) && STORY_POSE_ENVELOPES_AVAILABLE
 /* The authored pose envelope includes hands and tools in every expression.
  * Fitting that stable envelope keeps a gesture from shrinking the person. */
 int stage_left=story_pose_envelopes[id][0],stage_right=story_pose_envelopes[id][1];
 if(stage_left<=people_left[slot]&&stage_right>=people_right[slot]&&stage_right<=w){
  people_stage_left[slot]=(uint16_t)stage_left;people_stage_right[slot]=(uint16_t)stage_right;
 }
#endif
 people_scale[slot]=256;people_character[slot]=-1;
#if defined(STORY_CHARACTER_SCALES_AVAILABLE) && STORY_CHARACTER_SCALES_AVAILABLE
 people_scale[slot]=story_portrait_scales[id];people_character[slot]=story_portrait_characters[id];
#endif
 people_id[slot]=id;story_visual_stats[2]++;return slot;
}
static void actor(Surface*s,int slot,int x,int y,int width,int height,int opacity,int light){
 if(slot<0||width<=0||height<=0||width>STORY_PORTRAIT_WIDTH||height>STORY_PORTRAIT_HEIGHT)return;
 uint16_t palette[256];palette[0]=0;
 for(int i=1;i<256;i++)palette[i]=mix_color(people_palette[slot][i],C(0,0,0),light);
 int native=width==STORY_PORTRAIT_WIDTH&&height==STORY_PORTRAIT_HEIGHT;
 int columns[STORY_PORTRAIT_WIDTH];
 if(!native)for(int xx=0;xx<width;xx++)columns[xx]=xx*STORY_PORTRAIT_WIDTH/width;
 int left=limit(-x,0,width),right=limit(s->w-x,0,width),top=limit(-y,0,height),end=limit(s->h-y,0,height);
 if(left>=right||top>=end)return;
 for(int yy=top;yy<end;yy++){
  const uint8_t*p=people[slot]+(native?yy:yy*STORY_PORTRAIT_HEIGHT/height)*STORY_PORTRAIT_WIDTH;
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
/* One logical scene crosses the physical display boundary without allocating a
 * third framebuffer. The lower surface clips before the dialogue half. */
static void scene_actor(Surface*top,Surface*bottom,int slot,int x,int y,int w,int h,int opacity,int light){
 Surface lower={bottom->pixels,256,STORY_DIALOG_Y};
 actor(top,slot,x,y,w,h,opacity,light);
 actor(&lower,slot,x,y-192,w,h,opacity,light);
}
static void scene_overlay(Surface*top,Surface*bottom,int x,int y,int w,int h,uint16_t color,int alpha){
 Surface lower={bottom->pixels,256,STORY_DIALOG_Y};
 overlay(top,x,y,w,h,color,alpha);overlay(&lower,x,y-192,w,h,color,alpha);
}
static void scene_text(Surface*top,Surface*bottom,int x,int y,const char*text,uint16_t color){
 Surface lower={bottom->pixels,256,STORY_DIALOG_Y};
 gfx_text_small(top,x,y,text,color);gfx_text_small(&lower,x,y-192,text,color);
}
static void contact_shadow(Surface*top,Surface*bottom,int x,int y,int radius,int light){
 for(int row=-3;row<=3;row++)for(int col=-radius;col<=radius;col++){
  if(col*col*9+row*row*radius*radius>radius*radius*9)continue;
  int yy=y+row,xx=x+col;if(xx<0||xx>=256||yy<0||yy>=STORY_SCENE_HEIGHT)continue;
  uint16_t*p=yy<192?top->pixels+yy*256+xx:bottom->pixels+(yy-192)*256+xx;
  *p=mix_color(C(6,12,17),*p,light*65/256);
 }
}
/* Subject captions identify an insert, rather than labelling unrelated scenery.
 * Source coordinates and the shot-by-shot cast are authored in staging.json. */
static const char*subject_name(int bg,int right){
 return bg>=6&&bg<STORY_BACKGROUNDS?story_subjects[bg][right]:NULL;
}
static int portrait_id(int actor_id,int pose){return actor_id>=0&&actor_id<STORY_PEOPLE&&pose>=0&&pose<STORY_POSES?story_portraits[pose][actor_id]:-1;}
static int quarter_portrait(int actor_id,int pose,int facing){
#if STORY_QUARTER_AVAILABLE
 if(actor_id>=0&&actor_id<STORY_PEOPLE&&pose>=0&&pose<STORY_POSES&&facing>=0&&facing<2)return story_quarter_portraits[facing][pose][actor_id];
#else
 (void)actor_id;(void)pose;(void)facing;
#endif
 return -1;
}
/* Player replies belong to the same physical shot as the people being spoken
 * to. The line record names the conversational lead; this compact authored cue
 * retains their other on-screen partner instead of reframing a solo portrait. */
static int scene_companion(const StoryLine*line,int*pose){
 *pose=(int)story_pose(line,1);
 if(line->speaker<STORY_PEOPLE)return line->companion;
#if defined(STORY_REPLY_PAIRS_AVAILABLE) && STORY_REPLY_PAIRS_AVAILABLE
 if(line->speaker==SC_YOU&&line->stage==ST_FACE&&line->companion<STORY_PEOPLE&&
    story.chapter<CAREER_CHAPTERS&&story.event<CAREER_EVENTS){
  unsigned event=(unsigned)story.chapter*CAREER_EVENTS+story.event;
  unsigned begin=story_stage_offsets[event],end=story_stage_offsets[event+1];
  unsigned index=story.line;
  if(line==&story.previous&&index)index--;
  if(end>=begin&&end-begin==story.count&&index<story.count&&begin+index<STORY_LINES){
   unsigned cue=story_reply_pairs[begin+index];
   int other=(int)((cue>>4)&15)-1,other_pose=(int)((cue>>8)&7);
   if((cue&15)==(unsigned)line->companion+1&&((cue>>11)&7)==(unsigned)*pose&&
      other>=0&&other<STORY_PEOPLE&&other!=line->companion&&other_pose<STORY_POSES){
    *pose=other_pose;return other;
   }
  }
 }
#endif
 return 255;
}
/* These are scene positions, not speaker positions. A change of dialogue turns
 * adjusts emphasis while each person stays on the same side of the room. */
typedef struct {int x,y,width,height,center,floor;} StandingActor;
static StandingActor standing_single(int slot,int floor){
 int width=slot>=0?people_scale[slot]:256,height=STORY_PORTRAIT_HEIGHT*width/STORY_PORTRAIT_WIDTH;
 int x=(STORY_PORTRAIT_WIDTH-width)/2;
 return (StandingActor){x,floor-height,width,height,x+(slot>=0?people_foot[slot]:128)*width/STORY_PORTRAIT_WIDTH,floor};
}
static void standing_pair(int left_slot,int right_slot,int floor,StandingActor*out_left,StandingActor*out_right){
#if defined(STORY_CHARACTER_SCALES_AVAILABLE) && STORY_CHARACTER_SCALES_AVAILABLE
 int left_actor=people_character[left_slot],right_actor=people_character[right_slot];
 if(left_actor>=0&&left_actor<STORY_PEOPLE&&right_actor>=0&&right_actor<STORY_PEOPLE&&left_actor!=right_actor){
  int camera=story_pair_camera_scales[left_actor][right_actor];
  int lw=people_scale[left_slot]*camera/256,rw=people_scale[right_slot]*camera/256;
  int lh=STORY_PORTRAIT_HEIGHT*lw/STORY_PORTRAIT_WIDTH,rh=STORY_PORTRAIT_HEIGHT*rw/STORY_PORTRAIT_WIDTH;
  int lx=STORY_STANDING_MARGIN-((int)people_stage_left[left_slot]*lw+255)/256;
  int rx=256-STORY_STANDING_MARGIN-((int)people_stage_right[right_slot]*rw+255)/256;
  *out_left=(StandingActor){lx,floor-lh,lw,lh,lx+people_foot[left_slot]*lw/256,floor};
  *out_right=(StandingActor){rx,floor-4-rh,rw,rh,rx+people_foot[right_slot]*rw/256,floor-4};
  return;
 }
#endif
 int span=(int)people_stage_right[left_slot]-people_stage_left[left_slot]+(int)people_stage_right[right_slot]-people_stage_left[right_slot];
 /* Reserve the conversation gap in screen pixels before scaling bodies.
  * Scaling the gap with a wide pair otherwise makes the two people crowd. */
 int scale=span+12>240?228*256/span:256;
 /* Sampling rounds each silhouette edge independently. Keep twelve empty
  * columns even when that rounding widens the combined silhouettes by one. */
 for(;;){
  int occupied=0;
  for(int slot=0;slot<2;slot++){
   int person_slot=slot?right_slot:left_slot;
   occupied+=((int)people_stage_right[person_slot]*scale+255)/256-((int)people_stage_left[person_slot]*scale+255)/256;
  }
  if(occupied<=228)break;
  scale--;
 }
 int width=STORY_PORTRAIT_WIDTH*scale/256,height=STORY_PORTRAIT_HEIGHT*scale/256;
 int left_edge=((int)people_stage_left[left_slot]*width+STORY_PORTRAIT_WIDTH-1)/STORY_PORTRAIT_WIDTH;
 int right_end=((int)people_stage_right[right_slot]*width+STORY_PORTRAIT_WIDTH-1)/STORY_PORTRAIT_WIDTH;
 int left_x=8-left_edge,right_x=248-right_end;
 *out_left=(StandingActor){left_x,floor-height,width,height,left_x+people_foot[left_slot]*width/STORY_PORTRAIT_WIDTH,floor};
 *out_right=(StandingActor){right_x,floor-4-height,width,height,right_x+people_foot[right_slot]*width/STORY_PORTRAIT_WIDTH,floor-4};
}
static void standing_draw(Surface*top,Surface*bottom,int left_slot,int right_slot,int floor,int speaker_left,int opacity,int light){
 StandingActor left,right;standing_pair(left_slot,right_slot,floor,&left,&right);
 contact_shadow(top,bottom,right.center,right.floor,27*right.width/STORY_PORTRAIT_WIDTH,light);
 contact_shadow(top,bottom,left.center,left.floor,27*left.width/STORY_PORTRAIT_WIDTH,light);
 /* The right person stands four pixels farther away, independent of who talks. */
 scene_actor(top,bottom,right_slot,right.x,right.y,right.width,right.height,opacity,speaker_left?light*232/256:light);
 scene_actor(top,bottom,left_slot,left.x,left.y,left.width,left.height,opacity,speaker_left?light:light*232/256);
}
static int staged_portrait(int actor_id,int pose,int bg,int stage,int*full){
 *full=0;
 if(actor_id>=0&&actor_id<STORY_PEOPLE&&pose>=0&&pose<STORY_POSES&&stage==ST_FACE&&
    bg>=0&&bg<STORY_BACKGROUNDS&&story_fullbody_background[bg]&&story_fullbodies[pose][actor_id]>=0){
  *full=1;return story_fullbodies[pose][actor_id];
 }
 return portrait_id(actor_id,pose);
}
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
 int face=speaking?line->speaker:line->companion,other_pose;
 int face_pose=(int)story_pose(line,!speaking),other=scene_companion(line,&other_pose);
 int lead_full=0,other_full=0;
 int face_id=staged_portrait(face,face_pose,line->background,line->stage,&lead_full);
 int other_id=staged_portrait(other,other_pose,line->background,line->stage,&other_full);
 int standing=line->stage==ST_FACE&&story_fullbody_background[line->background];
 int face_left=other<STORY_PEOPLE&&other!=face?face<other:1;
 int quarter=standing?quarter_portrait(face,face_pose,face_left?1:0):-1;
 int quarter_other=standing&&other<STORY_PEOPLE&&other!=face?quarter_portrait(other,other_pose,face_left?0:1):-1;
 int quarter_pair=quarter>=0&&quarter_other>=0;
 if(quarter>=0){face_id=quarter;lead_full=1;}
 if(quarter_pair){other_id=quarter_other;other_full=1;}
 /* Close compositions keep their original upper-body staging. The standing
  * scenes can now keep both conversational partners physically in the room. */
 int paired=line->stage==ST_FACE&&!story_fullbody_background[line->background];
 int lead=-1,companion=-1;
 if(okay&&line->stage==ST_FACE){
  lead=person(face_id,paired||quarter_pair?other_id:-1);
  if((paired||quarter_pair)&&other<STORY_PEOPLE&&other!=face)companion=person(other_id,face_id);
 }else if(okay&&speaking)lead=person(face_id,-1);
 if(okay){
  int target=(int)(line->focus&STORY_FOCUS_X)*256;
  if(last_background!=line->background){camera_q8=target;last_background=line->background;camera_line=-1;}
  int camera_shot=(int)story.line*2+(line==&story.previous);
  if(camera_line!=camera_shot){if(!(line->focus&STORY_FOCUS_MOVE))camera_q8=target;camera_line=camera_shot;}
  if(!story.pause){int difference=target-camera_q8;camera_q8+=difference/10;if(difference<10&&difference>-10)camera_q8=target;}
  int view_width=line->stage==ST_MONTAGE?276:256;
  int view_height=view_width*STORY_SCENE_HEIGHT/256;
  int cx=limit(camera_q8/256,0,STORY_BACKGROUND_WIDTH-view_width),cy=(STORY_BACKGROUND_HEIGHT-view_height)/2;
  uint16_t palette[256];for(int i=0;i<256;i++)palette[i]=mix_color(back_palette[bg][i],C(0,0,0),brightness);
  int cols[256];for(int x=0;x<256;x++)cols[x]=cx+x*view_width/256;
  for(int y=0;y<STORY_SCENE_HEIGHT;y++){
   const uint8_t*p=back[bg]+(cy+y*view_height/STORY_SCENE_HEIGHT)*STORY_BACKGROUND_WIDTH;
   uint16_t*d=y<192?top+y*256:bottom+(y-192)*256;
   for(int x=0;x<256;x++)d[x]=palette[p[cols[x]]];
  }
  if(line->stage==ST_FACE){
   int left=other>=STORY_PEOPLE||face<other;
   int opacity=quarter>=0?256:story.age<7?160+(int)story.age*96/7:256;
   if(quarter_pair&&lead>=0&&companion>=0){
    standing_draw(&image,&dialog,face_left?lead:companion,face_left?companion:lead,story_ground_y[line->background],face_left,opacity,brightness);
   }else if(lead_full){
    StandingActor at=standing_single(lead,story_ground_y[line->background]);
    contact_shadow(&image,&dialog,at.center,at.floor,27*at.width/STORY_PORTRAIT_WIDTH,brightness);
    scene_actor(&image,&dialog,lead,at.x,at.y,at.width,at.height,opacity,brightness);
   }else{
    if(companion>=0)scene_actor(&image,&dialog,companion,left?132:-32,127,156,161,210,brightness*160/256);
    int x=paired&&face<STORY_PEOPLE?(left?-48:48):0;
    int breath=(story.ticks/35)%2;
    scene_actor(&image,&dialog,lead,x,STORY_SCENE_HEIGHT-STORY_PORTRAIT_HEIGHT-breath,STORY_PORTRAIT_WIDTH,STORY_PORTRAIT_HEIGHT,opacity,brightness);
   }
  }
  if(line->stage==ST_MONTAGE||line->stage==ST_RADIO){
   scene_overlay(&image,&dialog,0,0,256,14,C(1,6,13),190);
   scene_overlay(&image,&dialog,0,STORY_SCENE_HEIGHT-14,256,14,C(1,6,13),190);
  }
  /* Insert/radio speakers remain framed portraits, not people standing at a
   * distant location. Their badge uses the illustration area, not text width. */
  if(line->stage!=ST_FACE&&speaking&&lead>=0){
   Surface lower={bottom,256,STORY_DIALOG_Y};
   gfx_card(&lower,202,31,49,54,C(10,26,37),C(71,112,119));
   scene_actor(&image,&dialog,lead,204,225,45,47,256,brightness);
  }
  story_visual_stats[3]=(uint32_t)cx;story_visual_stats[4]=line->stage;
 }else{
  gfx_rect(&image,0,0,256,192,C(8,18,31));gfx_rect(&dialog,0,0,256,STORY_DIALOG_Y,C(8,18,31));
  text_center(&image,84,"Scene artwork unavailable",C(240,182,117));
 }
 char title[70];snprintf(title,sizeof title,"%s.%02d  /  %s",game_class_name(story.chapter),story.event+1,career_event_name(story.chapter,story.event));
 overlay(&image,5,quarter_pair?3:5,246,quarter_pair?12:16,C(3,10,20),150);gfx_text_small(&image,11,quarter_pair?5:9,title,C(231,238,225));
 if(line->stage==ST_MONTAGE)scene_text(&image,&dialog,82,277,"STORY MONTAGE",C(232,210,161));
 if(line->stage==ST_RADIO)scene_text(&image,&dialog,11,277,"SUNLINE / CREW RADIO",C(154,214,209));
 const char*subject=subject_name(line->background,(line->focus&STORY_FOCUS_SUBJECT)!=0);
 if(subject&&line->stage!=ST_FACE){
  int y=(line->stage==ST_MONTAGE||line->stage==ST_RADIO)?258:275;
  scene_overlay(&image,&dialog,5,y-3,limit(gfx_text_width(subject)+10,0,246),13,C(3,10,20),150);
  scene_text(&image,&dialog,10,y,subject,C(220,220,195));
 }
 /* Only the lower 96 rows belong to dialogue. All art above is a single view. */
 gfx_rect(&dialog,0,STORY_DIALOG_Y,256,192-STORY_DIALOG_Y,C(5,14,26));
 for(int y=STORY_DIALOG_Y;y<192;y+=4)gfx_rect(&dialog,0,y,256,1,C(8,20,31));
 panel(&dialog,5,100,246,86);
 gfx_rect(&dialog,11,105,234,16,C(9,30,39));
 gfx_text(&dialog,15,106,story_cast_names[line->speaker],C(245,217,151));
 unsigned pages=0,at=0;
 do{unsigned end=story_text_page_end(line->text,at);pages++;if(end<=at||end>=line->length)break;at=end;}while(pages<STORY_TEXT_MAX);
 char count_text[32];
 if(pages>1)snprintf(count_text,sizeof count_text,"%u/%u . %u/%u",story.page_index+1,pages,story.line+1,story.count);
 else snprintf(count_text,sizeof count_text,"%u / %u",story.line+1,story.count);
 gfx_text_small(&dialog,177,109,count_text,C(141,187,185));
 char visible[STORY_TEXT_MAX];
 unsigned start=line==&story.current?story.page_start:0;
 unsigned end=line==&story.current?story.page_end:line->length;
 unsigned count=story.phase==ST_READING?story.revealed:start;
 if(story.phase==ST_CLEAR||story.phase==ST_ENDING)count=end;
 if(count<start)count=start;
 if(count>end)count=end;
 unsigned length=count-start;memcpy(visible,line->text+start,length);visible[length]=0;
 gfx_wrap(&dialog,STORY_DIALOG_TEXT_X+1,STORY_DIALOG_TEXT_Y+1,visible,STORY_DIALOG_TEXT_WIDTH,STORY_DIALOG_LINE_HEIGHT,STORY_DIALOG_ROWS,C(4,15,23));
 gfx_wrap(&dialog,STORY_DIALOG_TEXT_X,STORY_DIALOG_TEXT_Y,visible,STORY_DIALOG_TEXT_WIDTH,STORY_DIALOG_LINE_HEIGHT,STORY_DIALOG_ROWS,C(239,243,225));
 if(story.phase==ST_READING&&story.revealed>=story.page_end){gfx_line(&dialog,231,177,235,181,C(236,205,129));gfx_line(&dialog,235,181,239,177,C(236,205,129));}
 if(story.phase==ST_CLEAR){
  panel(&dialog,5,100,246,86);text_center(&dialog,107,story.replay?"SCENE REPLAYED":"STORY CLEARED",C(246,215,143));
  gfx_wrap(&dialog,15,128,career_event_name(story.chapter,story.event),226,14,2,C(237,242,228));
  int done=story_completed(g);char record[60];snprintf(record,sizeof record,"%d / 125 events",done);gfx_text_small(&dialog,15,168,record,C(155,206,198));
  gfx_card(&dialog,162,163,78,19,C(26,87,92),C(140,203,190));gfx_text_small(&dialog,180,168,"Continue",C(245,237,202));
 }
 if(story.pause){
  scene_overlay(&image,&dialog,0,0,256,STORY_SCENE_HEIGHT,C(2,8,16),100);
  panel(&dialog,5,100,246,86);text_center(&dialog,105,story.failed?"SCENE UNAVAILABLE":"SCENE PAUSED",C(246,215,143));
  static const char*const choices[3]={"Resume","Restart scene","Return to map"};
  for(int i=0;i<3;i++){
   int selected=story.cursor==i,y=125+i*19;
   gfx_card(&dialog,26,y,204,18,selected?C(32,96,99):C(9,29,39),selected?C(159,214,192):C(48,86,98));
   text_center(&dialog,y+1,choices[i],selected?C(248,235,188):C(181,209,211));
  }
 }
}

/* The cast studio is a view of the same streamed illustrations, not a story
 * scene. Browsing never writes dialogue state, progression or a saved profile. */
void story_gallery_draw(const Game*g,uint16_t*top,uint16_t*bottom){
 (void)g;
 Surface image={top,256,192},menu={bottom,256,192};
 static const uint8_t pose_counts[STORY_GALLERY_COUNT]=STORY_GALLERY_POSE_COUNTS_INITIALIZER;
 static const char*const pose_names[STORY_POSES]={"Signature","Off duty","On shift","Reflective","Skeptical"};
 int entry=limit(campaign.page,0,STORY_GALLERY_COUNT-1);
 int count=pose_counts[entry];
 int pose=limit(campaign.gallery_pose&CAREER_GALLERY_POSE_MASK,0,count-1);
 int facing=campaign.gallery_pose&CAREER_GALLERY_LEFT?0:1;
 int portrait=story_gallery_portraits[entry][pose],fullbody=story_gallery_fullbody[entry][pose],quarter=0;
#if STORY_QUARTER_AVAILABLE
 if(!(campaign.gallery_pose&CAREER_GALLERY_LEGACY)&&story_gallery_quarters[entry][pose][facing]>=0){
  portrait=story_gallery_quarters[entry][pose][facing];fullbody=quarter=1;
 }
#else
 (void)facing;
#endif
 int okay=lease(),bg=okay?background(0):-1;
 int slot=bg>=0?person(portrait,-1):-1;
 if(bg>=0){
  const int cx=(STORY_BACKGROUND_WIDTH-256)/2,cy=(STORY_BACKGROUND_HEIGHT-STORY_SCENE_HEIGHT)/2;
  for(int y=0;y<STORY_SCENE_HEIGHT;y++){
   const uint8_t*source=back[bg]+(cy+y)*STORY_BACKGROUND_WIDTH+cx;
   uint16_t*dest=y<192?top+y*256:bottom+(y-192)*256;
   for(int x=0;x<256;x++)dest[x]=back_palette[bg][source[x]];
  }
  scene_overlay(&image,&menu,0,0,256,STORY_SCENE_HEIGHT,C(3,10,20),44);
 }else{
  gfx_rect(&image,0,0,256,192,C(8,18,31));
  gfx_rect(&menu,0,0,256,STORY_DIALOG_Y,C(8,18,31));
 }
 if(slot>=0){
  int floor=fullbody?story_ground_y[0]:STORY_SCENE_HEIGHT;
  StandingActor at=standing_single(slot,floor);
#if defined(STORY_CHARACTER_SCALES_AVAILABLE) && STORY_CHARACTER_SCALES_AVAILABLE
  if(fullbody){at.width=story_character_scales[entry];at.height=STORY_PORTRAIT_HEIGHT*at.width/256;at.x=(256-at.width)/2;at.y=floor-at.height;at.center=at.x+people_foot[slot]*at.width/256;}
#endif
  if(fullbody)contact_shadow(&image,&menu,at.center,floor,27*at.width/256,256);
  scene_actor(&image,&menu,slot,at.x,at.y,at.width,at.height,256,256);
 }else text_center(&image,84,"Portrait unavailable",C(240,182,117));
 overlay(&image,5,5,246,16,C(3,10,20),170);
 gfx_text_small(&image,11,9,"SUNLINE / CAST STUDIO",C(231,238,225));
 gfx_text_small(&image,story_gallery_draft[entry]?206:190,9,story_gallery_draft[entry]?"CONCEPT":"RISING TIDE",C(159,214,204));

 gfx_rect(&menu,0,STORY_DIALOG_Y,256,192-STORY_DIALOG_Y,C(5,14,26));
 panel(&menu,5,100,246,86);
 gfx_rect(&menu,11,105,234,16,C(9,30,39));
 gfx_text(&menu,15,106,story_gallery_names[entry],C(245,217,151));
 char position[20];snprintf(position,sizeof position,"%d / %d",entry+1,STORY_GALLERY_COUNT);
 gfx_text_small(&menu,207,109,position,C(141,187,185));
 gfx_text_small(&menu,15,124,story_gallery_styles[entry],C(157,219,207));
 gfx_wrap(&menu,15,138,story_gallery_descriptions[entry],226,14,2,C(235,239,226));
 gfx_text_small(&menu,15,174,pose_names[pose],C(245,217,151));
 gfx_text_small(&menu,98,174,quarter?(facing?"3/4 right":"3/4 left"):"Original",C(157,219,207));
 for(int n=0;n<count;n++)gfx_rect(&menu,239-(count-1-n)*9,175,5,3,n==pose?C(245,217,151):C(68,108,119));
 story_visual_stats[3]=(uint32_t)((STORY_BACKGROUND_WIDTH-256)/2);
 story_visual_stats[4]=ST_FACE;
}
