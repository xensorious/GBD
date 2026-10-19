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
 CHECK(LOCATION_COUNT==13 && vehicles[3].bike && !vehicles[6].bike && !vehicles[7].bike && !vehicles[8].bike);
 CHECK(game_can_location(&g,5) && game_can_location(&g,7) && !game_can_location(&g,6));
 g.save.career_xp=600;CHECK(game_can_location(&g,6));
 g.mode=M_DEPARTURES;g.cursor=7;g.save.cash=450;step(&g,K_A,K_A);
 CHECK(g.save.selected_location==7 && !g.save.cash && (g.save.unlocked_locations&(1u<<7)));
}
static void classes_and_roster(void) {
 static const int gates[5]={0,2,5,8,10};
 Game g;init(&g);CHECK(VEHICLE_COUNT==31 && LOCATION_COUNT==13);
 int old_value_price=-1,old_fastest=0;
 for(int cls=CLASS_D;cls<=CLASS_S;++cls) {
  int members=0,seen=0,value=-1,top=-1,accel=-1,armor=-1;
  CHECK(game_class_requirement(cls)==gates[cls] && *game_class_name(cls));
  for(int v=0;v<25;++v)if(game_vehicle_class(v)==cls) {
   ++members;CHECK(vehicles[v].specialty<=VS_ARMOR);
   CHECK(!(seen&(1<<vehicles[v].specialty)));seen|=1<<vehicles[v].specialty;
   CHECK(*game_specialty_name(vehicles[v].specialty));
   if(vehicles[v].specialty==VS_VALUE)value=v;
   if(vehicles[v].specialty==VS_TOP)top=v;
   if(vehicles[v].specialty==VS_ACCEL)accel=v;
   if(vehicles[v].specialty==VS_ARMOR)armor=v;
  }
  CHECK(members==5 && seen==31 && value>=0 && top>=0 && accel>=0 && armor>=0);
  for(int v=0;v<25;++v)if(game_vehicle_class(v)==cls) {
   CHECK(vehicles[top].top_speed>=vehicles[v].top_speed);
   CHECK(vehicles[accel].acceleration>=vehicles[v].acceleration);
   CHECK(vehicles[armor].armor>=vehicles[v].armor);
   /* Sundown remains a challenge gift rather than a cash-price competitor. */
   if(vehicles[v].price)CHECK(vehicles[value].price<=vehicles[v].price);
  }
  CHECK(vehicles[value].price>old_value_price && vehicles[top].top_speed>old_fastest);
  old_value_price=vehicles[value].price;old_fastest=vehicles[top].top_speed;
  for(int n=0;n<=VEHICLE_COUNT;++n) {
   g.save.owned=n?((1u<<n)-1u):0;
   CHECK(game_owned_count(&g)==n);
   CHECK(game_class_unlocked(&g,cls)==(n>=gates[cls]));
  }
 }
 CHECK(!strcmp(vehicles[0].name,"Pacer") && !strcmp(vehicles[3].name,"Kestrel") && !strcmp(vehicles[9].name,"Tempest"));
 CHECK(vehicles[10].price==300 && vehicles[3].bike && vehicles[26].bike && vehicles[27].bike);
 for(int v=25;v<31;++v) {
  CHECK(game_vehicle_class(v)==(v==27||v==30?CLASS_C:CLASS_D));
  CHECK(vehicles[v].top_speed>=175 && vehicles[v].grip>=90);
 }
 CHECK(game_vehicle_class(-1)==-1 && game_vehicle_class(VEHICLE_COUNT)==-1);
 /* Existing S ownership is honored even below the new ten-key gate. */
 init(&g);g.save.owned|=1u<<9;CHECK(!game_class_unlocked(&g,CLASS_S) && game_can_vehicle(&g,9));
 g.mode=M_GARAGE;g.selection=9;g.cursor=1;g.save.cash=0;step(&g,K_A,K_A);
 CHECK(g.save.selected_vehicle==9 && !g.save.cash);
 /* A new purchase needs both its class and its independent challenge. */
 init(&g);CHECK(!game_can_vehicle(&g,1));g.save.owned|=1u<<10;CHECK(game_can_vehicle(&g,1));
 CHECK(!game_can_vehicle(&g,30));g.save.best_near=20;CHECK(game_can_vehicle(&g,30));
 CHECK(!game_can_vehicle(&g,28));g.save.contracts_completed=3;CHECK(game_can_vehicle(&g,28));
 CHECK(!game_can_vehicle(&g,29));g.save.best_drift=3000;CHECK(game_can_vehicle(&g,29));
 CHECK(CHALLENGE_COUNT==8);
 for(int i=5;i<CHALLENGE_COUNT;++i) {
  int goal=0;CHECK(game_challenge_progress(&g,i,&goal)==goal && goal>0);
  CHECK(*game_challenge_name(i) && *game_challenge_reward(i));
 }
 g.mode=M_CHALLENGES;g.cursor=0;step(&g,K_UP,K_UP);CHECK(g.cursor==CHALLENGE_COUNT-1);
 /* Select filters browse real classes, and Oddities includes all six rides. */
 init(&g);g.mode=M_GARAGE;g.cursor=0;
 for(int filter=1;filter<=6;++filter) {
  step(&g,K_SELECT,K_SELECT);CHECK(g.garage_filter==filter);
  for(int i=0;i<VEHICLE_COUNT;++i) {
   CHECK(filter==6?g.selection>=25:game_vehicle_class(g.selection)==filter-1);
   step(&g,K_RIGHT,K_RIGHT);
  }
 }
 step(&g,K_SELECT,K_SELECT);CHECK(!g.garage_filter);
 g.garage_tab=1;g.cursor=0;step(&g,K_UP,K_UP);CHECK(g.cursor==4);
}
static void cosmetics_and_recovery(void) {
 Game g,loaded;init(&g);g.selection=0;g.livery_choice=1;g.wheel_choice=1;
 int cost=game_cosmetic_cost(&g,0,1)+game_cosmetic_cost(&g,1,1);
 g.save.cash=(uint32_t)cost-1;CHECK(!game_apply_cosmetics(&g));
 CHECK(g.save.livery_owned[0]==1 && g.save.wheels_owned[0]==1 && !g.save.livery[0] && !g.save.wheels[0]);
 g.save.cash=(uint32_t)cost;CHECK(game_apply_cosmetics(&g));
 CHECK(!g.save.cash && g.save.livery[0]==1 && g.save.wheels[0]==1);
 CHECK(g.save.livery_owned[0]==3 && g.save.wheels_owned[0]==3);
 CHECK(!game_cosmetic_cost(&g,0,1) && !game_cosmetic_cost(&g,1,1));
 CHECK(game_apply_cosmetics(&g) && !g.save.cash);
 g.livery_choice=g.wheel_choice=0;CHECK(game_apply_cosmetics(&g));
 CHECK(g.save.livery_owned[0]==3 && g.save.wheels_owned[0]==3);
 g.selection=10;g.save.cash=50000;g.livery_choice=2;g.wheel_choice=3;
 CHECK(!game_apply_cosmetics(&g));g.save.owned|=1u<<10;CHECK(game_apply_cosmetics(&g));
 CHECK(g.save.livery[0]==0 && g.save.livery[10]==2 && g.save.wheels[10]==3);
 for(int i=0;i<LIVERY_COUNT;++i)CHECK(*game_livery_name(i));
 for(int i=0;i<WHEEL_COUNT;++i)CHECK(*game_wheel_name(i));
 CHECK(game_cosmetic_cost(&g,-1,0)==-1 && game_cosmetic_cost(&g,0,LIVERY_COUNT)==-1);
 uint8_t disk[512];memset(disk,0xa5,sizeof(disk));CHECK(game_encode_save(&g,disk,sizeof(disk)));
 for(int i=SAVE_DISK_SIZE;i<512;++i)CHECK(disk[i]==0xa5);
 game_init(&loaded,7);CHECK(game_load_bytes(&loaded,disk,sizeof(disk)));
 CHECK(loaded.save.livery_owned[0]==3 && loaded.save.livery[10]==2 && loaded.save.wheels[10]==3);
 CHECK(loaded.save.livery_owned[10]==5 && loaded.save.wheels_owned[10]==9);
 /* Recovery follows the same mastery gate as the other lasting parts. */
 init(&g);g.mode=M_GARAGE;g.garage_tab=1;g.cursor=4;g.selection=0;
 int previous_cost=0;
 for(int rank=1;rank<=UPGRADE_MAX;++rank) {
  int cost_now=game_upgrade_cost(&g,3);CHECK(cost_now>previous_cost);previous_cost=cost_now;
  if(rank>=3) {
   uint32_t cash=g.save.cash;step(&g,K_A,K_A);CHECK(g.save.recovery[0]==rank-1 && g.save.cash==cash);
   static const uint32_t xp[7]={0,0,0,100,300,650,1200};g.save.mastery_xp[0]=xp[rank];
  }
  step(&g,K_A,K_A);CHECK(g.save.recovery[0]==rank);
 }
 CHECK(!game_upgrade_cost(&g,3));
 int last=0;
 for(int v=0;v<25;++v)if(vehicles[v].specialty==VS_VALUE) {
  g.selection=(uint8_t)v;int cost_now=game_upgrade_cost(&g,0);
  CHECK(cost_now>=last);last=cost_now;
 }
}
static void dispatch_contracts(void) {
 Game g,other;unsigned kinds=0;
 for(unsigned seed=1;seed<=64;++seed) {
  game_init(&g,seed);other=g;other.frame=123456;unsigned board=0;
  for(int slot=0;slot<CONTRACT_OFFERS;++slot) {
   DispatchContract a,b;game_contract_offer(&g,slot,&a);game_contract_offer(&other,slot,&b);
   CHECK(!memcmp(&a,&b,sizeof(a)) && a.kind<DC_NONE && a.goal && a.reward);
   CHECK(!(board&(1u<<a.kind)));board|=1u<<a.kind;kinds|=1u<<a.kind;
   CHECK(*game_contract_name(a.kind));
  }
 }
 CHECK(kinds==31);
 for(int kind=DC_NEAR;kind<DC_NONE;++kind)for(int complete=0;complete<2;++complete) {
  init(&g);g.save.career_xp=22000;g.save.perks[2]=3;game_begin_run(&g);quiet(&g);
  g.active_contract=(DispatchContract){.goal=10,.reward=150,.kind=(uint8_t)kind};
  uint32_t progress=complete?10:9;
  if(kind==DC_NEAR)g.run.count[B_NEAR]=progress;
  if(kind==DC_DRIFT)g.run.drift_points=progress;
  if(kind==DC_DISTANCE)g.run.distance_m=progress;
  if(kind==DC_COINS)g.run.count[B_COIN]=progress;
  if(kind==DC_SPEED)g.run.best_speed_hold=progress;
  g.run.points[B_DISTANCE]=g.run.total_points=600;
  uint32_t cash=g.save.cash;CHECK(game_contract_progress(&g)==progress);
  game_finish_run(&g);
  uint32_t base=10+g.run.count[B_COIN]*5,perk=base*24/100;
  CHECK(g.contract_complete==complete && g.contract_cash==(complete?150u:0u));
  CHECK(g.run.cash_bonus==perk && g.run.total_cash==base+perk+(complete?150u:0u));
  CHECK(g.save.cash==cash+g.run.total_cash && g.save.contracts_completed==(unsigned)complete);
  Save banked=g.save;game_finish_run(&g);CHECK(!memcmp(&banked,&g.save,sizeof(banked)));
 }
 init(&g);g.mode=M_CONTRACTS;g.cursor=1;DispatchContract chosen;game_contract_offer(&g,1,&chosen);
 step(&g,K_A,K_A);CHECK(g.mode==M_HOME && !g.cursor && g.save.dispatch_choice==1);
 game_begin_run(&g);CHECK(!memcmp(&chosen,&g.active_contract,sizeof(chosen)));
 g.mode=M_CONTRACTS;g.cursor=CONTRACT_OFFERS;step(&g,K_A,K_A);game_begin_run(&g);
 CHECK(g.active_contract.kind==DC_NONE && !g.active_contract.goal && !g.contract_progress);
 /* A completed objective can award no more than the total-cash cap allows. */
 g.active_contract=(DispatchContract){.goal=1,.reward=400,.kind=DC_COINS};g.run.count[B_COIN]=2000000;
 game_finish_run(&g);CHECK(g.contract_complete && !g.contract_cash && g.run.total_cash==9999999);
}
int main(void) {
 thresholds();parts_and_tunes();perks_and_banking();settings_and_locations();
 classes_and_roster();cosmetics_and_recovery();dispatch_contracts();
 printf("progression: %u checks; 31-ride classes/specialties/gates, lasting parts, cosmetics, contracts, mastery, perks and bank-once rewards pass\n",checks);
 return 0;
}
