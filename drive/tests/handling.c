/* Rules fixtures deliberately remove traffic and recenter between samples.
 * Cartridge tests separately exercise ordinary joypad-only driving. */
#include "game.h"
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(c) do { ++checks; if(!(c)) { fprintf(stderr,"handling:%d: %s\n",__LINE__,#c); exit(1); } } while(0)
static int magnitude(int n) { return n<0?-n:n; }
static void quiet(Game *g) {
 memset(g->traffic,0,sizeof(g->traffic));memset(g->pickups,0,sizeof(g->pickups));
 g->next_spawn_q8=6000000*Q;
}
static void begin(Game *g,int vehicle) {
 game_init(g,0x7ab31u);save_add_vehicle(&g->save,vehicle);
 g->save.selected_vehicle=(uint8_t)vehicle;game_begin_run(g);quiet(g);
 g->x_q8=g->vx_q8=0;
}
static void step(Game *g,uint32_t held,uint32_t pressed) {
 quiet(g);Input in={held,pressed};game_update(g,&in);
 CHECK(g->combo_q8>=Q && g->combo_q8<=COMBO_MAX_Q8);
 CHECK((g->combo_q8-Q)%COMBO_STEP_Q8==0);
 CHECK(g->police_ticks<=POLICE_CATCH_TICKS && g->police_pressure<=100);
}
static void centred_step(Game *g,uint32_t held) {
 g->x_q8=0;step(g,held,0);
}
static void combo_ramps(void) {
 for(int focus=0;focus<=3;++focus)for(int skill=0;skill<=2;++skill) {
  Game g;begin(&g,skill==2?3:0);g.save.perks[1]=(uint8_t)focus;
  int full=(skill?SKILL_COMBO_TICKS:SPEED_COMBO_TICKS)*(100-15*focus)/100;
  uint32_t keys=K_R|(skill==1?(K_B|K_RIGHT):skill==2?K_A:0);
  for(int tick=1;tick<=full+90;++tick) {
   g.speed_q8=game_top_speed(&g)*Q;
   centred_step(&g,keys);
   int expected=Q+(tick<full?tick*16/full:16)*COMBO_STEP_Q8;
   CHECK(g.mode==M_RUN && g.combo_q8==expected);
   CHECK(g.run.max_combo_q8==(uint32_t)expected);
  }
  if(!focus)CHECK(full==(skill?720:1440));
  if(focus==3)CHECK(full==(skill?396:792));
  CHECK(g.run.total_points==g.run.points[B_DISTANCE]+g.run.points[B_SURVIVAL]+
        g.run.points[B_SPEED]+g.run.points[B_DRIFT]+g.run.points[B_WHEELIE]);
  /* An intentional Slow cruise or a released trick breaks that hold. */
  centred_step(&g,K_R|K_DOWN);
  CHECK(g.combo_q8==Q && !g.speed_hold && !g.drift_hold && !g.wheelie_hold);
 }
 /* A mere eight seconds at top speed is now just 1.50x. */
 Game g;begin(&g,0);
 for(int i=0;i<8*DRIVE_HZ;++i) {
  g.speed_q8=game_top_speed(&g)*Q;centred_step(&g,K_R);
 }
 CHECK(g.combo_q8==Q+2*COMBO_STEP_Q8);
}
static void drift_control(void) {
 for(int v=0;v<VEHICLE_COUNT;++v)for(int grip=0;grip<=UPGRADE_MAX;grip+=UPGRADE_MAX) {
  Game normal,slide;begin(&normal,v);normal.save.grip[v]=(uint8_t)grip;
  normal.speed_q8=160*Q;slide=normal;
  centred_step(&normal,K_R|K_RIGHT);centred_step(&slide,K_R|K_RIGHT|K_B);
  CHECK(slide.drifting && !normal.drifting);
  CHECK(slide.vx_q8>normal.vx_q8 && slide.yaw>normal.yaw);
  for(int i=0;i<7;++i)centred_step(&slide,K_R|K_RIGHT|K_B);
  CHECK(slide.vx_q8>Q && slide.vx_q8<3*Q && slide.yaw>=70);
  /* Opposite steering catches the slide in one update, including the
   * lowest-grip stock classic. It does not coast across the next lane. */
  centred_step(&slide,K_R|K_LEFT|K_B);
  CHECK(slide.drifting && slide.vx_q8<0);
  for(int i=0;i<4;++i)centred_step(&slide,K_R);
  CHECK(!slide.drifting && magnitude(slide.vx_q8)<Q/3 && magnitude(slide.yaw)<25);
  for(int i=0;i<8;++i)centred_step(&slide,K_R|K_RIGHT|K_B);
  for(int i=0;i<4;++i)centred_step(&slide,K_R|K_B);
  CHECK(!slide.drifting && magnitude(slide.vx_q8)<Q/3);
 }
 Game g;begin(&g,0);g.speed_q8=30*Q;
 centred_step(&g,K_R|K_B|K_RIGHT);CHECK(!g.drifting);
 g.speed_q8=160*Q;g.crash_timer=5;
 centred_step(&g,K_R|K_B|K_RIGHT);CHECK(!g.drifting);
}
static void police_capture(void) {
 Game g;begin(&g,0);uint32_t cash=g.save.cash;
 for(int i=0;i<POLICE_GRACE_TICKS;++i) {
  step(&g,0,0);CHECK(g.mode==M_RUN && !g.police_ticks && !g.police_pressure);
 }
 for(int i=1;i<POLICE_CATCH_TICKS;++i) {
  step(&g,0,0);CHECK(g.mode==M_RUN && g.police_ticks==i);
  CHECK(g.police_pressure==i*100/POLICE_CATCH_TICKS);
 }
 step(&g,0,0);
 CHECK(g.mode==M_RECEIPT && g.end_reason==END_STOPPED && g.police_pressure==100);
 CHECK(g.health==100 && g.run.banked && g.save.total_runs==1);
 CHECK(g.save.cash==cash+g.run.total_cash);
 Save banked=g.save;game_finish_run(&g);CHECK(!memcmp(&banked,&g.save,sizeof(banked)));
 game_begin_run(&g);
 CHECK(!g.police_ticks && !g.police_pressure && g.end_reason==END_NONE);
 /* Reverse is a brief correction rather than a way to escape pursuit. */
 quiet(&g);g.run_ticks=POLICE_GRACE_TICKS;g.position_q8=10000*Q;
 for(int i=1;i<=POLICE_CATCH_TICKS;++i) {
  g.x_q8=0;g.speed_q8=-20*Q;step(&g,K_L,0);
  CHECK(g.police_ticks==i);
 }
 CHECK(g.mode==M_RECEIPT && g.end_reason==END_STOPPED);
 /* Arrest is presentation, never a currency fine: an earned payout and
  * mastery progress still bank once, just like a collision-ended run. */
 begin(&g,0);g.speed_q8=100*Q;cash=g.save.cash;
 for(int i=0;i<3*DRIVE_HZ;++i)centred_step(&g,K_R|K_DOWN);
 CHECK(g.run.total_points>60 && g.run.total_cash>0 && g.run.distance_m>0);
 uint32_t earned=g.run.total_cash;g.speed_q8=0;
 for(int i=0;i<POLICE_CATCH_TICKS;++i)step(&g,0,0);
 CHECK(g.mode==M_RECEIPT && g.end_reason==END_STOPPED && g.save.cash==cash+earned);
 CHECK(g.run.mastery_xp>0 && g.save.mastery_xp[0]==g.run.mastery_xp);
 CHECK(g.run.career_xp>0 && g.save.career_xp==g.run.career_xp);
 banked=g.save;game_finish_run(&g);CHECK(!memcmp(&banked,&g.save,sizeof(banked)));
}
static void police_recovery_and_pause(void) {
 Game g;begin(&g,0);g.run_ticks=POLICE_GRACE_TICKS;
 for(int i=0;i<30;++i)step(&g,0,0);
 CHECK(g.police_ticks==30 && g.police_pressure==33);
 /* The threshold uses actual speed after coasting drag, with no rounding
  * of the displayed integer speed. Just above 2 km/h recovers pressure. */
 g.speed_q8=2*Q+18*Q/DRIVE_HZ;step(&g,0,0);CHECK(g.police_ticks==31);
 g.speed_q8=2*Q+18*Q/DRIVE_HZ+1;step(&g,0,0);CHECK(g.police_ticks==29);
 uint32_t ticks=g.run_ticks;uint16_t stopped=g.police_ticks;
 step(&g,K_START,K_START);CHECK(g.mode==M_PAUSE && g.run_ticks==ticks);
 for(int i=0;i<300;++i)step(&g,0,0);
 CHECK(g.police_ticks==stopped && g.run_ticks==ticks);
 step(&g,K_START,K_START);CHECK(g.mode==M_RUN);
 g.speed_q8=100*Q;
 for(int i=0;i<30;++i)centred_step(&g,K_R|K_DOWN);
 CHECK(!g.police_ticks && !g.police_pressure);
 for(int i=0;i<20*DRIVE_HZ;++i)centred_step(&g,K_R|K_DOWN);
 CHECK(g.mode==M_RUN && !g.police_ticks && !g.police_pressure);
 /* Stop/start manipulation cannot erase pressure in a single update. */
 g.police_ticks=80;g.speed_q8=100*Q;centred_step(&g,K_R);
 CHECK(g.police_ticks==78 && g.police_pressure==86);
}
static void reasons_and_save(void) {
 Game g,loaded;Save before,after;begin(&g,0);
 CHECK(sizeof(Save)==1032 && sizeof(RunStats)==108);
 CHECK(sizeof(Game)==2132 && offsetof(Game,police_ticks)==2100);
 CHECK(offsetof(Game,police_pressure)==2102 && offsetof(Game,end_reason)==2103);
 game_make_save(&g,&before);g.police_ticks=89;g.police_pressure=98;g.end_reason=END_CRASH;
 game_make_save(&g,&after);CHECK(!memcmp(&before,&after,sizeof(before)));
 game_init(&loaded,41);CHECK(game_load(&loaded,&before));game_begin_run(&loaded);
 CHECK(!loaded.police_ticks && !loaded.police_pressure && loaded.end_reason==END_NONE);
 begin(&g,0);g.health=0;step(&g,0,0);CHECK(g.end_reason==END_CRASH && g.mode==M_RECEIPT);
 begin(&g,0);game_finish_run(&g);CHECK(g.end_reason==END_BANKED && g.mode==M_RECEIPT);
}
int main(void) {
 combo_ramps();drift_control();police_capture();police_recovery_and_pause();reasons_and_save();
 printf("handling: %u checks; exact quarter combos, 48s/24s ramps, controlled slides, pursuit grace/capture/recovery, pause and unchanged saves pass\n",checks);
 return 0;
}
