#include "render.h"
#include "gfx.h"
#include "ui.h"
#include "assets.h"
#include "ambient.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define INK C(21,44,66)
#define PAPER C(251,240,216)
#define GOLD C(237,189,104)
#define MINT C(145,217,181)
#define TEAL C(38,126,115)
static int cam_x=-1,cam_y=-1,last_x,last_y;
static int path_x[20],path_y[20],path_head,path_count;
static uint8_t viewport_depth[256*192] __attribute__((aligned(4)));
static int min(int a,int b){return a<b?a:b;}
static int max(int a,int b){return a>b?a:b;}
static int clamp(int a,int lo,int hi){return max(lo,min(hi,a));}
static void label(Surface*s,int x,int y,const char*t){gfx_card(s,x,y,gfx_text_width(t)+16,20,INK,TEAL);gfx_text(s,x+8,y+5,t,PAPER);}
static void ground_sprite(Surface*s,int footx,int footy,const uint16_t*p,int w,int h){
 int x=footx-w/2,y=footy-h,cut=footy/4;
 if(x+w<=cam_x||x>=cam_x+256||y+h<=cam_y||y>=cam_y+192)return;
 /* A compact silhouette depth plane lets a person pass behind a painted roof
    or tree canopy, then emerge in front of its foot without repainting it. */
 for(int j=0;j<h;j++){int wy=y+j,sy=wy-cam_y;if(wy<0||wy>=768||sy<0||sy>=192)continue;
  for(int i=0;i<w;i++){int wx=x+i,sx=wx-cam_x;if(wx<0||wx>=1024||sx<0||sx>=256)continue;
   uint16_t c=p[j*w+i];if(c&&viewport_depth[sy*256+sx]<=cut)s->pixels[sy*256+sx]=c;
  }
 }
}
static void shadow(Surface*s,int wx,int wy,int radius){
 if(wx+radius<cam_x||wx-radius>=cam_x+256||wy+2<cam_y||wy-2>=cam_y+192)return;
 for(int j=-2;j<=2;j++)for(int i=-radius;i<=radius;i++)if(i*i*4+j*j*radius*radius<=radius*radius*4){
  int x=wx+i,y=wy+j,px=x-cam_x,py=y-cam_y;if(x<0||x>=1024||y<0||y>=768||px<0||px>=256||py<0||py>=192)continue;
  if(viewport_depth[py*256+px]>wy/4)continue;
  uint16_t c=s->pixels[py*256+px];s->pixels[py*256+px]=0x8000|((c&31)*3/4)|(((c>>5&31)*3/4)<<5)|(((c>>10&31)*3/4)<<10);
 }
}
/* A readable lantern blossom marks each collectible, then vanishes when picked. */
static const uint16_t blossom_art[12*18]={
 0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,C(186,140,65),C(186,140,65),0,0,0,0,0,
 0,0,0,0,C(186,140,65),C(245,199,94),C(245,199,94),C(186,140,65),0,0,0,0,
 0,0,0,C(186,140,65),C(245,199,94),C(255,247,188),C(255,247,188),C(245,199,94),C(186,140,65),0,0,0,
 0,0,C(186,140,65),C(245,199,94),C(255,247,188),C(255,247,188),C(255,247,188),C(255,247,188),C(245,199,94),C(186,140,65),0,0,
 0,0,C(186,140,65),C(245,199,94),C(255,247,188),C(255,247,188),C(255,247,188),C(255,247,188),C(245,199,94),C(186,140,65),0,0,
 0,0,0,C(186,140,65),C(245,199,94),C(255,247,188),C(255,247,188),C(245,199,94),C(186,140,65),0,0,0,
 0,0,0,0,C(245,199,94),C(245,199,94),C(245,199,94),C(245,199,94),0,0,0,0,
 0,0,0,0,0,C(100,163,94),C(100,163,94),0,0,0,0,0,
 0,0,0,0,0,C(100,163,94),C(100,163,94),0,0,0,0,0,
 0,0,0,C(42,100,77),C(42,100,77),C(100,163,94),C(100,163,94),C(42,100,77),C(42,100,77),0,0,0,
 0,0,C(42,100,77),C(42,100,77),C(42,100,77),C(100,163,94),C(100,163,94),C(42,100,77),C(42,100,77),C(42,100,77),0,0,
 0,0,0,0,0,C(100,163,94),C(100,163,94),0,0,0,0,0,
 0,0,0,0,0,C(100,163,94),C(100,163,94),0,0,0,0,0,
 0,0,0,0,0,C(42,100,77),C(42,100,77),0,0,0,0,0,
 0,0,0,0,0,C(42,100,77),C(42,100,77),0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,
 0,0,0,0,0,0,0,0,0,0,0,0,
};
typedef struct {int x,y,kind,id,anim;} Actor;
static void follower_position(const Game*g,int*fx,int*fy){
 *fx=g->save.player_x+20;*fy=g->save.player_y+8;
 if(!path_count)return;
 int x=g->save.player_x,y=g->save.player_y,remaining=20;
 for(int n=0;n<path_count;n++){
  int index=(path_head-n+20)%20;
  int dx=path_x[index]-x,dy=path_y[index]-y;
  int length=abs(dx)+abs(dy);
  if(length>=remaining){
   *fx=x+dx*remaining/length;*fy=y+dy*remaining/length;return;
  }
  remaining-=length;x=path_x[index];y=path_y[index];
 }
 *fx=x;*fy=y;
}
static void draw_world(Surface*s,const Game*g){
 if(cam_x<0||abs(g->save.player_x-last_x)+abs(g->save.player_y-last_y)>100){path_count=0;last_x=g->save.player_x;last_y=g->save.player_y;}
 cam_x=clamp(g->save.player_x-128,0,768);
 cam_y=clamp(g->save.player_y-104,0,576);
 if(g->save.player_x!=last_x||g->save.player_y!=last_y){
  path_head=(path_head+1)%20;path_x[path_head]=last_x;path_y[path_head]=last_y;if(path_count<20)path_count++;
 }
 last_x=g->save.player_x;last_y=g->save.player_y;
 for(int y=0;y<192;y++){
  gfx_copy(s->pixels+y*256,world_art+(cam_y+y)*1024+cam_x,256);
  memcpy(viewport_depth+y*256,world_depth+(cam_y+y)*1024+cam_x,256);
 }
 ambient_world(s,viewport_depth,cam_x,cam_y,g->frame);
 Actor actors[16];int count=0;
 if(g->world){
  for(int i=0;i<4;i++)actors[count++]=(Actor){g->world->npcs[i].x,g->world->npcs[i].y,0,i,(g->frame/36+i)&1};
  for(int i=0;i<6;i++)if(game_wild_visible(g,i)){
   static const int wild_species[6]={3,4,5,3,4,5};
   actors[count++]=(Actor){g->world->wilds[i].x,g->world->wilds[i].y,1,wild_species[i],(g->frame/36+i)&1};
  }
 }
 if(g->world&&(g->save.flags&PF_CHOSEN))for(int i=0;i<3;i++)if(!(g->save.flowers&(1u<<i)))
  actors[count++]=(Actor){g->world->flowers[i].x,g->world->flowers[i].y,3,i,0};
 if(g->save.party_count){int fx,fy;follower_position(g,&fx,&fy);
  int species=g->save.party[g->save.lead].species;
  actors[count++]=(Actor){fx,fy,1,species,(g->frame/36+species)&1};}
 actors[count++]=(Actor){g->save.player_x,g->save.player_y,2,g->facing,g->walking?(g->anim_tick/8)%4:(g->frame/36)&1};
 for(int i=1;i<count;i++){Actor a=actors[i];int j=i;while(j&&actors[j-1].y>a.y){actors[j]=actors[j-1];j--;}actors[j]=a;}
 for(int i=0;i<count;i++){
  Actor*a=&actors[i];shadow(s,a->x,a->y,7);
  if(a->kind==0)ground_sprite(s,a->x,a->y,people_art+(a->id*2+a->anim)*24*34,24,34);
  else if(a->kind==1)ground_sprite(s,a->x,a->y,creature_field+(a->id*2+a->anim)*28*28,28,28);
  else if(a->kind==3)ground_sprite(s,a->x,a->y,blossom_art,12,18);
  else ground_sprite(s,a->x,a->y,g->walking?hero_art+(a->id*4+a->anim)*24*32:hero_idle_art+(a->id*2+a->anim)*24*32,24,32);
 }
 ambient_foreground(s,cam_x,cam_y,g->frame);
 if(g->world){
  int goal=!(g->save.flags&PF_CHOSEN)?0:!(g->save.flags&PF_MET_JUN)?1:!(g->save.flags&PF_TRIAL_WON)?3:-1;
  if(goal>=0){int x=g->world->npcs[goal].x-cam_x,y=g->world->npcs[goal].y-cam_y-40-(g->frame/20%2);
   gfx_circle(s,x,y,5,INK);gfx_circle(s,x,y,4,GOLD);gfx_text(s,x-1,y-4,"!",INK);}
 }
 label(s,7,6,game_location(g));
 if(g->ui_focus){gfx_card(s,210,6,39,20,TEAL,GOLD);gfx_text(s,218,11,"Menu",PAPER);}
 int target=game_nearest_interaction(g);
 if(target>=0&&g->mode==GM_FIELD&&!g->ui_focus){const char*t=game_interaction_label(g,target);char b[84];snprintf(b,sizeof(b),"%s",t);int width=gfx_text_width(b)+16;
  gfx_card(s,(256-width)/2,168,width,18,INK,GOLD);gfx_text(s,(256-width)/2+8,172,b,PAPER);}
 if(g->toast_timer){gfx_card(s,7,144,242,21,INK,TEAL);gfx_text(s,14,149,g->toast,PAPER);}
}
static void hp(Surface*s,int x,int y,int w,int val,int total){
 gfx_rect(s,x,y,w,5,INK);gfx_rect(s,x+1,y+1,w-2,3,C(79,93,95));
 gfx_rect(s,x+1,y+1,(w-2)*max(0,val)/max(1,total),3,val*3<total?C(218,120,99):MINT);
}
static void draw_battle(Surface*s,const Game*g){
 gfx_copy(s->pixels,battle_art,256*192);
 const Battle*b=&g->battle;const Companion*f=&g->save.party[b->active];
 int enemyx=151+(b->flash_enemy?((g->frame&2)?3:-3):0),playx=6+(b->flash_player?((g->frame&2)?3:-3):0);
 int bob=(g->frame/20)%2;
 if(b->is_trial){gfx_circle(s,202,96,41,C(214,183,100));gfx_circle(s,202,96,38,C(114,157,120));}
 gfx_creature(s,enemyx,45+bob,b->species,100,0,g->frame);
 gfx_creature(s,playx,91-bob,f->species,100,1,g->frame);
 gfx_card(s,7,7,130,43,INK,TEAL);gfx_text(s,15,13,game_species[b->species].name,PAPER);
 char text[32];snprintf(text,sizeof(text),"Lv %u %s",b->level,b->is_trial?"  Trial":"  Wild");gfx_text_small(s,15,27,text,MINT);
 hp(s,15,40,112,b->hp,b->max_hp);
 gfx_card(s,139,148,110,38,INK,TEAL);gfx_text(s,147,154,game_species[f->species].name,PAPER);
 snprintf(text,sizeof(text),"Lv %u  %u / %d",f->level,f->hp,game_max_hp(f));gfx_text_small(s,147,166,text,MINT);hp(s,147,179,94,f->hp,game_max_hp(f));
 if(b->effect_timer){
  static const uint16_t colors[5]={C(145,217,181),C(250,182,81),C(137,216,236),C(222,161,105),GOLD};
  uint16_t c=colors[game_species[f->species].element];
  for(int i=0;i<16;i++){int t=(i*17+b->effect_timer*7)%67;int x=178+(i*29%75)-20,y=66+t;gfx_rect(s,x,y,2,2,c);}
 }
}
int render_game(const Game*g,uint16_t*top,uint16_t*bottom){
 static Game previous_ui;
 static int ui_ready,old_map_x,old_map_y;
 static const char*last_location;
 Game key=*g;
 key.frame=0;key.facing=0;key.walking=0;key.anim_tick=0;
 key.move_dx=key.move_dy=0;key.move_remaining=key.move_speed=key.move_preference=key.move_queued=key.action_queued=0;
 key.save.player_x=key.save.player_y=0;key.save.steps=0;key.save.facing=0;key.save.checksum=0;
 memset(key.wild_cooldown,0,sizeof(key.wild_cooldown));key.interaction_cooldown=0;
 key.toast_timer=!!g->toast_timer;key.battle.flash_enemy=key.battle.flash_player=0;key.battle.effect_timer=0;
 int changed=!ui_ready||memcmp(&key,&previous_ui,sizeof(key))||last_location!=game_location(g);
 int updated=0;
 previous_ui=key;last_location=game_location(g);ui_ready=1;
 Surface t={top,256,192},b={bottom,256,192};
 if(g->mode==GM_TITLE){if(changed){gfx_copy(top,title_art,256*192);updated|=1;}}
 else if(g->mode==GM_BATTLE||(g->mode==GM_PARTY&&g->menu_from_battle))draw_battle(&t,g);
 else if(g->mode==GM_STARTER){
  gfx_copy(top,battle_art,256*192);label(&t,59,10,"A new companion");
  for(int i=0;i<3;i++){int chosen=g->cursor==i;gfx_creature(&t,6+i*84-(chosen?6:0),chosen?77:103,i,chosen?92:76,0,g->frame);}
 }else if(g->mode==GM_PARTY&&!g->save.party_count){
  draw_world(&t,g);
 }else if(g->mode==GM_GROWTH||g->mode==GM_GUIDE||g->mode==GM_PARTY){
  gfx_copy(top,battle_art,256*192);int id=g->mode==GM_GUIDE?g->cursor:g->save.party[g->mode==GM_GROWTH?g->growth_slot:g->cursor].species;
  if(g->mode!=GM_GUIDE||(g->save.seen&(1u<<id)))gfx_creature(&t,64,45,id,128,0,g->frame);
  label(&t,9,9,g->mode==GM_GROWTH?"A bond takes shape":g->mode==GM_GUIDE?"Island field guide":"Traveling companions");
 }else if(g->mode==GM_ENDING){
  gfx_copy(top,title_art,256*192);
  gfx_card(&t,36,33,184,117,INK,GOLD);gfx_circle(&t,128,79,27,GOLD);gfx_circle(&t,128,79,22,TEAL);
  gfx_line(&t,118,87,128,66,PAPER);gfx_line(&t,128,66,138,87,PAPER);gfx_line(&t,122,80,134,80,PAPER);
  gfx_text(&t,(256-gfx_text_width("The Lantern Crest"))/2,116,"The Lantern Crest",PAPER);
  for(int i=0;i<18;i++)gfx_rect(&t,(i*59+g->frame*2)%256,(i*37+g->frame)%192,2,2,i&1?GOLD:MINT);
 }else{
  draw_world(&t,g);
  if(g->mode==GM_DIALOG){
   int id=-1;const char*names[4]={"Mira","Jun","Edda","Vale"};for(int i=0;i<4;i++)if(strstr(g->dialog.speaker,names[i]))id=i;
   if(id>=0){gfx_card(&t,7,87,80,99,INK,TEAL);gfx_sprite(&t,11,92,portrait_art+id*72*90,72,90,0);}
  }
 }
 if(g->mode!=GM_TITLE)updated|=1;
 if(changed){ui_draw_bottom(&b,g);updated|=2;}
 else if(g->mode==GM_FIELD&&(old_map_x!=g->save.player_x||old_map_y!=g->save.player_y)){
  gfx_map_marker(&b,g,old_map_x,old_map_y);updated|=2;
 }
 old_map_x=g->save.player_x;old_map_y=g->save.player_y;
 return updated;
}
