#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(x) do { ++checks; if(!(x)) {fprintf(stderr,"progression:%d: %s\n",__LINE__,#x);exit(1);} } while(0)
static void step(Game *g,unsigned held,unsigned pressed) {Input in={held,pressed};game_update(g,&in);}
static void quiet(Game *g) {
 memset(g->traffic,0,sizeof(g->traffic));memset(g->pickups,0,sizeof(g->pickups));g->next_spawn_q8=6000000*Q;
}
static void init(Game *g) {game_init(g,123);g->save.cash=50000;}
static void thresholds(void) {
 static const unsigned mastery[]={0,100,300,650,1200,2000,3200};
 static const unsigned career[]={0,200,600,1300,2400,4000,6500,10000,15000,22000};
 Game g;init(&g);
 for(int r=0;r<7;++r) {
  g.save.mastery_xp[0]=mastery[r];CHECK(game_mastery_level(&g,0)==r);
  CHECK(game_mastery_next(&g,0)==(r<6?mastery[r+1]:0));
  if(r) {--g.save.mastery_xp[0];CHECK(game_mastery_level(&g,0)==r-1);}
 }
 for(int r=0;r<10;++r) {
  g.save.career_xp=career[r];CHECK(game_career_level(&g)==r);CHECK(game_perk_available(&g)==r);
  CHECK(game_career_next(&g)==(r<9?career[r+1]:0));
  if(r) {--g.save.career_xp;CHECK(game_career_level(&g)==r-1);}
 }
 CHECK(!game_mastery_level(&g,-1));CHECK(!game_mastery_level(&g,VEHICLE_COUNT));
}
static void parts_and_tunes(void) {
 Game g;init(&g);g.mode=M_GARAGE;g.garage_tab=1;g.selection=0;g.cursor=1;
 CHECK(game_upgrade_cost(&g,0)==60);
 step(&g,K_A,K_A);CHECK(g.save.engine[0]==1 && g.save.cash==49940);
 step(&g,K_A,K_A);CHECK(g.save.engine[0]==2);
 unsigned cash=g.save.cash;CHECK(game_upgrade_rank(&g,0)==1);
 step(&g,K_A,K_A);CHECK(g.save.engine[0]==2 && g.save.cash==cash);
 for(int next=3;next<=UPGRADE_MAX;++next) {
  static const unsigned ranks[]={0,100,300,650,1200};
  CHECK(game_upgrade_rank(&g,0)==next-2);
  g.save.mastery_xp[0]=ranks[next-2];
  int cost=game_upgrade_cost(&g,0);cash=g.save.cash;
  step(&g,K_A,K_A);CHECK(g.save.engine[0]==next && g.save.cash==cash-(unsigned)cost);
 }
 CHECK(!game_upgrade_cost(&g,0));cash=g.save.cash;step(&g,K_A,K_A);CHECK(g.save.cash==cash);
 for(int v=0;v<VEHICLE_COUNT;++v) {
  CHECK(vehicles[v].name&&*vehicles[v].name);CHECK(game_vehicle_stat(&g,v,STAT_TOP)>=100);
  CHECK(game_vehicle_stat(&g,v,STAT_ACCEL)>=15);CHECK(game_vehicle_stat(&g,v,STAT_GRIP)>=40);
 }
 init(&g);g.mode=M_GARAGE;g.garage_tab=2;g.cursor=1;g.selection=0;g.tune_choice=1;
 int top=game_vehicle_stat(&g,0,STAT_TOP),acc=game_vehicle_stat(&g,0,STAT_ACCEL);
 CHECK(game_preview_stat(&g,0,STAT_TOP)==top-12);
 CHECK(game_preview_stat(&g,0,STAT_ACCEL)==acc+12);
 step(&g,K_A,K_A);CHECK(g.save.power_tune[0]==0); /* Preview is safe while locked. */
 g.save.mastery_xp[0]=100;cash=g.save.cash;step(&g,K_A,K_A);
 CHECK(g.save.power_tune[0]==1 && g.save.cash==cash);
 CHECK(game_vehicle_stat(&g,0,STAT_TOP)==top-12 && game_vehicle_stat(&g,0,STAT_ACCEL)==acc+12);
 step(&g,K_RIGHT,K_RIGHT);CHECK(g.tune_choice==2 && g.save.power_tune[0]==1);
 CHECK(game_preview_stat(&g,0,STAT_TOP)==top+24);
 step(&g,K_A,K_A);CHECK(g.save.power_tune[0]==2);
 CHECK(game_vehicle_stat(&g,0,STAT_ACCEL)==acc-10);
 g.cursor=2;g.tune_choice=1;step(&g,K_A,K_A);CHECK(!g.save.handling_tune[0]);
 g.save.mastery_xp[0]=300;step(&g,K_A,K_A);CHECK(g.save.handling_tune[0]==1);
 g.cursor=3;g.tune_choice=2;step(&g,K_A,K_A);CHECK(!g.save.chassis_tune[0]);
 g.save.mastery_xp[0]=650;step(&g,K_A,K_A);CHECK(g.save.chassis_tune[0]==2);
 CHECK(game_vehicle_stat(&g,0,STAT_ARMOR)==vehicles[0].armor+24);
 for(int kind=0;kind<3;++kind)for(int value=0;value<3;++value) {
  CHECK(*game_tune_name(kind,value));CHECK(*game_tune_description(kind,value));
 }
 /* Both specializations change actual acceleration, not just the stat card. */
 Game stock,sprint,overdrive;game_init(&stock,99);game_init(&sprint,99);game_init(&overdrive,99);
 sprint.save.power_tune[0]=1;overdrive.save.power_tune[0]=2;
 game_begin_run(&stock);game_begin_run(&sprint);game_begin_run(&overdrive);
 quiet(&stock);quiet(&sprint);quiet(&overdrive);
 step(&stock,K_R,0);step(&sprint,K_R,0);step(&overdrive,K_R,0);
 CHECK(sprint.speed_q8>stock.speed_q8 && stock.speed_q8>overdrive.speed_q8);
 CHECK(game_top_speed(&sprint)<game_top_speed(&stock) && game_top_speed(&stock)<game_top_speed(&overdrive));
}
static void perks_and_banking(void) {
 Game g;init(&g);g.mode=M_CAREER;g.cursor=0;
 unsigned cash=g.save.cash;step(&g,K_A,K_A);CHECK(!g.save.perks[0]);
 g.save.career_xp=22000;
 for(int branch=0;branch<3;++branch) {
  g.cursor=(uint8_t)branch;
  for(int rank=1;rank<=3;++rank){step(&g,K_A,K_A);CHECK(g.save.perks[branch]==rank);}
 }
 CHECK(!game_perk_available(&g) && g.save.cash==cash);
 g.cursor=3;step(&g,K_A,K_A);CHECK(game_perk_available(&g)==9 && g.save.cash==cash);
 g.save.perks[2]=3;game_begin_run(&g);quiet(&g);
 g.run.distance_m=5000;g.run.points[B_NEAR]=1000;g.run.points[B_DRIFT]=1000;g.run.total_points=2000;
 g.run.count[B_COIN]=4;cash=g.save.cash;unsigned xp=g.save.career_xp;
 game_finish_run(&g);
 CHECK(g.run.mastery_xp==1200 && g.run.career_xp==1200);
 CHECK(g.save.mastery_xp[0]==1200 && g.save.career_xp==xp+1200);
 CHECK(g.run.cash_bonus==12 && g.run.total_cash==65 && g.save.cash==cash+65);
 CHECK(g.save.unlocked_locations&(1u<<6));
 cash=g.save.cash;xp=g.save.career_xp;game_finish_run(&g);
 CHECK(g.save.cash==cash && g.save.career_xp==xp && g.save.mastery_xp[0]==1200);
 /* Recovery can rescue a lethal impact exactly once per run. */
 for(int rank=1;rank<=3;++rank) {
  init(&g);g.save.perks[0]=(uint8_t)rank;game_begin_run(&g);quiet(&g);
  g.run_ticks=100;g.x_q8=0;g.speed_q8=100*Q;g.health=10;
  g.traffic[0].active=1;g.traffic[0].z_q8=Q;
  step(&g,K_R,0);CHECK(g.repair_used && g.health==2+8*rank && g.mode==M_RUN);
  g.damage_cooldown=0;g.traffic[0].z_q8=g.position_q8+Q;
  step(&g,K_R,0);CHECK(!g.health && g.mode==M_RECEIPT);
  game_begin_run(&g);CHECK(!g.repair_used);
 }
 Game normal,focus;game_init(&normal,5);game_init(&focus,5);focus.save.perks[1]=3;
 game_begin_run(&normal);game_begin_run(&focus);quiet(&normal);quiet(&focus);
 normal.speed_q8=focus.speed_q8=game_top_speed(&normal)*Q;
 normal.speed_hold=focus.speed_hold=SPEED_COMBO_TICKS*55/100-1;
 step(&normal,K_R,0);step(&focus,K_R,0);
 CHECK(normal.combo_q8<COMBO_MAX_Q8 && focus.combo_q8==COMBO_MAX_Q8);
}
static void settings_and_locations(void) {
 Game g;game_init(&g,5);
 CHECK(g.save.music_on && g.save.sfx_on && g.save.music_volume==70 && g.save.sfx_volume==75);
 CHECK(g.save.shake && g.save.motion==1 && !g.save.units && !g.save.skin);
 g.mode=M_SETTINGS;g.menu_return=M_PAUSE;g.cursor=0;step(&g,K_A,K_A);CHECK(!g.save.music_on);
 g.cursor=1;g.save.music_volume=100;step(&g,K_RIGHT,K_RIGHT);CHECK(g.save.music_volume==100);
 g.save.music_volume=0;step(&g,K_LEFT,K_LEFT);CHECK(!g.save.music_volume);
 g.cursor=3;step(&g,K_LEFT,K_LEFT);CHECK(g.save.sfx_volume==70);
 g.cursor=4;step(&g,K_LEFT,K_LEFT);CHECK(g.save.skin==2);step(&g,K_RIGHT,K_RIGHT);CHECK(g.save.skin==0);
 g.cursor=5;step(&g,K_RIGHT,K_RIGHT);CHECK(g.save.motion==2);
 g.cursor=6;step(&g,K_A,K_A);CHECK(!g.save.shake);
 g.cursor=7;step(&g,K_A,K_A);CHECK(g.save.units);
 g.cursor=8;step(&g,K_A,K_A);CHECK(g.mode==M_CONTROLS);
 step(&g,K_B,K_B);CHECK(g.mode==M_SETTINGS && g.cursor==8);
 step(&g,K_B,K_B);CHECK(g.mode==M_PAUSE && g.cursor==3);
 Save save;game_make_save(&g,&save);CHECK(game_save_valid(&save));
 Game loaded;game_init(&loaded,7);CHECK(game_load(&loaded,&save));
 CHECK(loaded.save.skin==0 && loaded.save.motion==2 && !loaded.save.shake && loaded.save.units);
 CHECK(!loaded.save.music_on && loaded.save.sfx_volume==70);
 CHECK(LOCATION_COUNT==8 && vehicles[3].bike && !vehicles[6].bike && !vehicles[7].bike && !vehicles[8].bike);
 CHECK(game_can_location(&g,5) && game_can_location(&g,7) && !game_can_location(&g,6));
 g.save.career_xp=600;CHECK(game_can_location(&g,6));
 g.mode=M_DEPARTURES;g.cursor=7;g.save.cash=450;step(&g,K_A,K_A);
 CHECK(g.save.selected_location==7 && !g.save.cash && (g.save.unlocked_locations&(1u<<7)));
}
int main(void) {
 thresholds();parts_and_tunes();perks_and_banking();settings_and_locations();
 printf("progression: %u checks; mastery gates, effective setup tradeoffs, career perks, bank-once XP/cash and persistent settings pass\n",checks);
 return 0;
}
