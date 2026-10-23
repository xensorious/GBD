/* Independent bounds/pixel checks for inward-facing standing conversations. */
#include <stddef.h>
#include <stdlib.h>
#include "../src/dialogue_render.c"
#include "render.h"

enum {W=256,H=192,GUARD=16};
typedef struct {uint16_t before[GUARD],top[W*H],middle[GUARD],bottom[W*H],after[GUARD];} Frames;
static Frames frame;
static uint8_t figures[2][STORY_PORTRAIT_WIDTH*STORY_PORTRAIT_HEIGHT];
static uint16_t colors[2][256];
static unsigned checks,native_pairs,reduced_pairs,quarter_views;
#if STORY_QUARTER_AVAILABLE
static unsigned real_pairs,real_native_pairs,real_reduced_pairs,floor_scenes;
#endif
#define CHECK(v) do{checks++;if(!(v)){fprintf(stderr,"quarter staging:%d: %s\n",__LINE__,#v);exit(1);}}while(0)

static void clear_frame(void){
 for(int i=0;i<GUARD;i++)frame.before[i]=frame.middle[i]=frame.after[i]=(uint16_t)(0xa940+i);
 for(int i=0;i<W*H;i++)frame.top[i]=frame.bottom[i]=C(45,57,68);
}
static void guards(void){for(int i=0;i<GUARD;i++)CHECK(frame.before[i]==0xa940+i&&frame.middle[i]==0xa940+i&&frame.after[i]==0xa940+i);}
static uint16_t pixel(int x,int y){return y<H?frame.top[y*W+x]:frame.bottom[(y-H)*W+x];}
static uint16_t lit(uint16_t source,int light){
 uint16_t value=0x8000;
 for(int shift=0;shift<=10;shift+=5)value|=(uint16_t)((((source>>shift)&31)*light/256)<<shift);
 return value;
}
static void fixture(int slot,int width){
 memset(figures[slot],0,sizeof figures[slot]);
 int left=(STORY_PORTRAIT_WIDTH-width)/2;
 for(int y=2;y<STORY_PORTRAIT_HEIGHT;y++)for(int x=left;x<left+width;x++)figures[slot][y*STORY_PORTRAIT_WIDTH+x]=1;
 people[slot]=figures[slot];people_palette[slot]=colors[slot];
 people_left[slot]=(uint16_t)left;people_right[slot]=(uint16_t)(left+width);people_foot[slot]=128;
}
static void check_figure(int slot,const StandingActor*at,int light){
 unsigned count=0;
 for(int y=0;y<STORY_SCENE_HEIGHT;y++)for(int x=0;x<W;x++){
  int xx=x-at->x,yy=y-at->y;
  if(xx<0||xx>=at->width||yy<0||yy>=at->height)continue;
  unsigned index=figures[slot][yy*STORY_PORTRAIT_HEIGHT/at->height*STORY_PORTRAIT_WIDTH+xx*STORY_PORTRAIT_WIDTH/at->width];
  if(index){CHECK(pixel(x,y)==lit(colors[slot][index],light));count++;}
 }
 CHECK(count>1000);
}
static void geometry(void){
 colors[0][1]=C(235,84,53);colors[1][1]=C(66,135,235);
 static const int widths[]={40,63,97,112,128,163,211,250};
 Surface top={frame.top,W,H},bottom={frame.bottom,W,H};
 for(unsigned l=0;l<sizeof widths/sizeof widths[0];l++)for(unsigned r=0;r<sizeof widths/sizeof widths[0];r++){
  fixture(0,widths[l]);fixture(1,widths[r]);
  StandingActor left,right;standing_pair(0,1,284,&left,&right);
  CHECK(left.floor==284&&right.floor==280&&left.y+left.height==284&&right.y+right.height==280);
  CHECK(left.width==right.width&&left.height==right.height);
  CHECK(left.width<=256&&left.height<=264&&left.width>0&&left.height>0);
  if(widths[l]+widths[r]+12<=240){CHECK(left.width==256&&left.height==264);native_pairs++;}
  else{CHECK(left.width<256&&left.height<264);reduced_pairs++;}
  int left_min=W,left_max=-1,right_min=W,right_max=-1;
  for(int x=0;x<left.width;x++){
   int source=x*STORY_PORTRAIT_WIDTH/left.width;
   if(source>=people_left[0]&&source<people_right[0]){if(left_min==W)left_min=left.x+x;left_max=left.x+x;}
   if(source>=people_left[1]&&source<people_right[1]){if(right_min==W)right_min=right.x+x;right_max=right.x+x;}
  }
  CHECK(left_min==8&&right_max==247);
  CHECK(left_max<right_min&&right_min-left_max-1>=12);
  CHECK(left.center>=left_min&&left.center<=left_max&&right.center>=right_min&&right.center<=right_max);
  for(int speaker=0;speaker<2;speaker++){
   clear_frame();standing_draw(&top,&bottom,0,1,284,speaker,256,256);
   check_figure(0,&left,speaker?256:232);check_figure(1,&right,speaker?232:256);
   for(int i=W*STORY_DIALOG_Y;i<W*H;i++)CHECK(frame.bottom[i]==C(45,57,68));
   guards();
  }
 }
 CHECK(native_pairs&&reduced_pairs);
}

static void press(Game*g,unsigned key){Input in={key,key};game_update(g,&in);}
static void gallery_inputs(void){
 Game g;game_init(&g,0x715a);Save saved=g.save;StoryState before=story;
 CHECK(sizeof(CareerUI)==12&&offsetof(CareerUI,revision)==8&&offsetof(CareerUI,gallery_pose)==7);
 career_enter(&g);press(&g,K_L);press(&g,K_UP);press(&g,K_A);
 CHECK(campaign.modal==CAREER_CAST_MODAL&&!campaign.gallery_pose&&!campaign.page);
 press(&g,K_X);CHECK(campaign.gallery_pose==CAREER_GALLERY_LEFT);
 press(&g,K_Y);CHECK(campaign.gallery_pose==(CAREER_GALLERY_LEFT|CAREER_GALLERY_LEGACY));
 press(&g,K_R);CHECK(campaign.gallery_pose==(CAREER_GALLERY_LEFT|CAREER_GALLERY_LEGACY|1));
 press(&g,K_RIGHT);CHECK(campaign.page==1&&campaign.gallery_pose==(CAREER_GALLERY_LEFT|CAREER_GALLERY_LEGACY));
 press(&g,K_L);CHECK(campaign.gallery_pose==(CAREER_GALLERY_LEFT|CAREER_GALLERY_LEGACY|3));
 press(&g,K_A);CHECK(campaign.gallery_pose==(CAREER_GALLERY_LEFT|CAREER_GALLERY_LEGACY));
 press(&g,K_X);CHECK(campaign.gallery_pose==CAREER_GALLERY_LEGACY);
 press(&g,K_Y);CHECK(!campaign.gallery_pose);
 Input held={K_X|K_Y|K_R,0};game_update(&g,&held);CHECK(!campaign.gallery_pose&&campaign.page==1);
 CHECK(!memcmp(&saved,&g.save,sizeof saved)&&!memcmp(&before,&story,sizeof before));
 press(&g,K_B);CHECK(!campaign.modal&&g.cursor==4);
}

#if STORY_QUARTER_AVAILABLE
static uint8_t source[STORY_PORTRAIT_WIDTH*STORY_PORTRAIT_HEIGHT];
static uint16_t palette[256];
static void gallery_pixels(void){
 Game g;game_init(&g,91);career_enter(&g);campaign.modal=CAREER_CAST_MODAL;
 static const uint8_t counts[STORY_GALLERY_COUNT]=STORY_GALLERY_POSE_COUNTS_INITIALIZER;
 Save saved=g.save;StoryState before=story;
 for(int entry=0;entry<STORY_GALLERY_COUNT;entry++)for(int pose=0;pose<counts[entry];pose++)for(int facing=0;facing<2;facing++){
  campaign.page=(uint8_t)entry;campaign.gallery_pose=(uint8_t)(pose|(facing?0:CAREER_GALLERY_LEFT));
  int id=story_gallery_quarters[entry][pose][facing],w,h;
  CHECK(id>=0&&id<STORY_PORTRAITS&&story_asset(STORY_BACKGROUNDS+id,source,palette,sizeof source,&w,&h));
  CHECK(w==256&&h==264);clear_frame();render_game(&g,frame.top,frame.bottom);guards();
  unsigned pixels=0,lower=0;
  for(int y=2;y<h;y++)for(int x=0;x<w;x++){
   unsigned index=source[y*w+x];int yy=284-h+y;
   if(index&&yy>=22){CHECK(pixel(x,yy)==palette[index]);pixels++;lower+=yy>=192;}
  }
  CHECK(pixels>1000&&lower>100);quarter_views++;
  CHECK(!memcmp(&saved,&g.save,sizeof saved)&&!memcmp(&before,&story,sizeof before));
 }
 CHECK(quarter_views==142);
}
static void line_pose(StoryLine*line,unsigned pose,unsigned companion){
 line->focus=(uint16_t)(128|((pose&3)<<9)|((companion&3)<<11));line->pose_flags=(uint8_t)((pose>>2)|((companion>>2)<<4));
}
/* Check every sampled opaque pixel, including hands outside the nominal body
 * center. A source canvas may extend off-screen only where it is transparent. */
static void paired_pixels(int slot,const StandingActor*at,int light){
 unsigned top=0,bottom=0;
 int first=W,last=-1;
 for(int y=0;y<at->height;y++)for(int x=0;x<at->width;x++){
  int sx=x*STORY_PORTRAIT_WIDTH/at->width,sy=y*STORY_PORTRAIT_HEIGHT/at->height;
  unsigned index=people[slot][sy*STORY_PORTRAIT_WIDTH+sx];
  if(!index)continue;
  int xx=at->x+x,yy=at->y+y;
  CHECK(xx>=8&&xx<248&&yy>=15&&yy<STORY_SCENE_HEIGHT);
  CHECK(pixel(xx,yy)==lit(people_palette[slot][index],light));
  if(xx<first)first=xx;
  if(xx>last)last=xx;
  if(yy<H)top++;else bottom++;
 }
 CHECK(top>1000&&bottom>100&&first<=last);
}
static void real_pair(Game*g,int left,int right,int left_pose,int right_pose,int bg){
 story.current.background=(uint8_t)bg;story.current.speaker=(uint8_t)left;story.current.companion=(uint8_t)right;
 line_pose(&story.current,(unsigned)left_pose,(unsigned)right_pose);
 StoryState expected=story;Save saved=g->save;
 clear_frame();render_game(g,frame.top,frame.bottom);guards();
 int left_id=story_quarter_portraits[1][left_pose][left],right_id=story_quarter_portraits[0][right_pose][right];
 int left_slot=people_id[0]==left_id?0:1,right_slot=people_id[0]==right_id?0:1;
 CHECK(left_id>=76&&right_id>=76&&left_id<STORY_PORTRAITS&&right_id<STORY_PORTRAITS);
 CHECK(people_id[left_slot]==left_id&&people_id[right_slot]==right_id&&left_slot!=right_slot);
 StandingActor a,b;standing_pair(left_slot,right_slot,story_ground_y[bg],&a,&b);
 int span=people_right[left_slot]-people_left[left_slot]+people_right[right_slot]-people_left[right_slot];
 CHECK(a.floor==284&&b.floor==280&&a.y+a.height==a.floor&&b.y+b.height==b.floor);
 CHECK(a.width==b.width&&a.height==b.height&&a.width>0&&a.width<=256);
 CHECK(a.width*264-a.height*256>=0&&a.width*264-a.height*256<256);
 if(span+12<=240){CHECK(a.width==256&&a.height==264);real_native_pairs++;}
 else{CHECK(a.width<256&&a.height<264);real_reduced_pairs++;}
 int left_first=a.x+(people_left[left_slot]*a.width+255)/256;
 int left_last=a.x+(people_right[left_slot]*a.width+255)/256-1;
 int right_first=b.x+(people_left[right_slot]*b.width+255)/256;
 int right_last=b.x+(people_right[right_slot]*b.width+255)/256-1;
 CHECK(left_first==8&&right_last==247&&right_first-left_last-1>=12);
 CHECK(a.center>=left_first&&a.center<=left_last&&b.center>=right_first&&b.center<=right_last);
 paired_pixels(left_slot,&a,256);paired_pixels(right_slot,&b,232);
 CHECK(!memcmp(&expected,&story,sizeof story)&&!memcmp(&saved,&g->save,sizeof saved));
 unsigned loads=story_visual_stats[2];
 story.current.speaker=(uint8_t)right;story.current.companion=(uint8_t)left;
 line_pose(&story.current,(unsigned)right_pose,(unsigned)left_pose);
 expected=story;clear_frame();render_game(g,frame.top,frame.bottom);guards();
 CHECK(people_id[left_slot]==left_id&&people_id[right_slot]==right_id&&story_visual_stats[2]==loads);
 StandingActor aa,bb;standing_pair(left_slot,right_slot,story_ground_y[bg],&aa,&bb);
 CHECK(!memcmp(&a,&aa,sizeof a)&&!memcmp(&b,&bb,sizeof b));
 paired_pixels(left_slot,&aa,232);paired_pixels(right_slot,&bb,256);
 CHECK(!memcmp(&expected,&story,sizeof story)&&!memcmp(&saved,&g->save,sizeof saved));
 real_pairs++;
}
static void story_pairing(void){
 Game g;game_init(&g,47);g.mode=M_DIALOG;story_reset();story.active=1;story.phase=ST_READING;story.age=40;story.count=2;
 story.current.stage=ST_FACE;story.current.background=0;strcpy(story.current.text,"A conversation.");
 story.current.length=(uint8_t)strlen(story.current.text);story.page_end=story.revealed=story.current.length;
 static const uint8_t counts[STORY_GALLERY_COUNT]=STORY_GALLERY_POSE_COUNTS_INITIALIZER;
 CHECK(sizeof(StoryState)==488&&offsetof(StoryState,page_start)==485&&SC_YOU==14&&SC_NARRATOR==15);
 /* All 1,508 outfit combinations on the garage floor; all 91 character pairs
  * also visit each other compatible floor. Both speakers retain their pose. */
 for(int bg=0;bg<STORY_BACKGROUNDS;bg++)if(story_fullbody_background[bg]){
  floor_scenes++;
  for(int left=0;left<STORY_PEOPLE;left++)for(int right=left+1;right<STORY_PEOPLE;right++)
   for(int lp=0;lp<(bg==0?counts[left]:1);lp++)for(int rp=0;rp<(bg==0?counts[right]:1);rp++)
    real_pair(&g,left,right,lp,rp,bg);
 }
 CHECK(floor_scenes==8&&real_pairs==2145&&real_native_pairs+real_reduced_pairs==real_pairs);
 /* Fifth expression, off-camera player, close composition and radio remain separate. */
 story.current.speaker=SC_NIA;story.current.companion=SC_MINA;line_pose(&story.current,4,3);
 clear_frame();render_game(&g,frame.top,frame.bottom);CHECK(people_id[0]==story_quarter_portraits[1][4][SC_NIA]||people_id[1]==story_quarter_portraits[1][4][SC_NIA]);
 story.current.speaker=SC_YOU;story.current.companion=SC_JULES;line_pose(&story.current,0,2);
 clear_frame();render_game(&g,frame.top,frame.bottom);CHECK(people_id[0]==story_quarter_portraits[1][2][SC_JULES]||people_id[1]==story_quarter_portraits[1][2][SC_JULES]);
 story.current.speaker=SC_NIA;story.current.companion=SC_JULES;story.current.background=1;line_pose(&story.current,0,0);
 clear_frame();render_game(&g,frame.top,frame.bottom);CHECK(people_id[0]==story_portraits[0][SC_NIA]||people_id[1]==story_portraits[0][SC_NIA]);
 story.current.background=0;story.current.stage=ST_RADIO;story.current.companion=SC_NONE;
 clear_frame();render_game(&g,frame.top,frame.bottom);CHECK(people_id[0]==story_portraits[0][SC_NIA]||people_id[1]==story_portraits[0][SC_NIA]);guards();
 CHECK(!story_visual_stats[0]);
 story_render_release();
}
#endif
int main(int argc,char**argv){
 (void)argv;geometry();gallery_inputs();
 if(argc<2){
  CHECK(STORY_QUARTER_AVAILABLE);
#if STORY_QUARTER_AVAILABLE
  gallery_pixels();story_pairing();
#endif
 }
 printf("quarter staging: %u checks; %u native pairs / %u reduced pairs, stable inward-facing sides, grounded feet, reserved UI, packed orientation inputs and %u authored directional gallery views pass\n",checks,native_pairs,reduced_pairs,quarter_views);
#if STORY_QUARTER_AVAILABLE
 if(argc<2)printf("real art: %u pairs across %u floor scenes; %u native / %u reduced pairs; both speakers retain positions, outfit pixels and cached portraits\n",real_pairs,floor_scenes,real_native_pairs,real_reduced_pairs);
#endif
 return 0;
}
