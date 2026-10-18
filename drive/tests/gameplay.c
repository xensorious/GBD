/* Functional rules checks: controls, risk/reward, economy and battery saves. */
#include "game.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static unsigned checks;
#define CHECK(c) do { ++checks; if(!(c)) { fprintf(stderr,"gameplay:%d: %s\n",__LINE__,#c); exit(1); } } while(0)
static void step(Game *g,uint32_t held,uint32_t pressed) { Input i={held,pressed};game_update(g,&i); }
static void quiet(Game *g) {
 memset(g->traffic,0,sizeof g->traffic);memset(g->pickups,0,sizeof g->pickups);
 g->next_spawn_q8=6000000*Q;
}
static void ticks(Game *g,int n,uint32_t keys) {
 for(int i=0;i<n;i++) {quiet(g);step(g,keys,0);CHECK(g->mode==M_RUN);}
}
static void begin(Game *g) {game_init(g,0x12345678u);game_begin_run(g);quiet(g);g->x_q8=0;}
static uint32_t sum(const uint32_t *a) { uint32_t n=0;for(int i=0;i<BONUS_COUNT;i++) n+=a[i];return n; }
static void controls(void) {
 Game gas,turbo,slow,no_gas;
 begin(&gas);begin(&turbo);begin(&slow);begin(&no_gas);
 ticks(&gas,150,K_R);ticks(&turbo,150,K_R|K_UP);ticks(&slow,150,K_R|K_DOWN);ticks(&no_gas,150,K_UP);
 CHECK(no_gas.speed_q8==0 && !no_gas.turbo);
 CHECK(turbo.speed_q8>gas.speed_q8 && gas.speed_q8>slow.speed_q8);
 CHECK(turbo.turbo && !turbo.slow && slow.slow && !slow.turbo);
 CHECK(gas.position_q8>slow.position_q8 && turbo.position_q8>gas.position_q8);
 ticks(&turbo,60,K_R|K_DOWN|K_UP);CHECK(turbo.slow && !turbo.turbo);
 CHECK(turbo.speed_q8<gas.speed_q8);
 int32_t braking=gas.speed_q8;ticks(&gas,15,K_L|K_R|K_UP|K_DOWN);
 CHECK(gas.speed_q8<braking && !gas.turbo && !gas.slow);
 int32_t speed=gas.speed_q8;ticks(&gas,30,0);CHECK(gas.speed_q8<speed);
 Game reverse;begin(&reverse);reverse.position_q8=reverse.furthest_q8=3000*Q;
 ticks(&reverse,60,K_L);CHECK(reverse.speed_q8<0 && reverse.speed_q8>=-34*Q);
 CHECK(reverse.position_q8<3000*Q && !reverse.run.distance_m && !reverse.run.points[B_DISTANCE]);
 int32_t z=reverse.position_q8;ticks(&reverse,30,K_L);CHECK(reverse.position_q8<z);
 Game left,right;begin(&left);begin(&right);ticks(&left,60,K_R);ticks(&right,60,K_R);
 ticks(&left,12,K_R|K_LEFT);ticks(&right,12,K_R|K_RIGHT);CHECK(left.x_q8<0 && right.x_q8>0);
 CHECK(left.steering==-1 && right.steering==1);
 ticks(&left,3,K_R|K_LEFT|K_RIGHT);CHECK(!left.steering);
 step(&right,K_R|K_A,K_A);CHECK(right.horn_timer>0 && !right.wheelie);
 step(&right,K_R|K_B|K_LEFT,0);CHECK(right.drifting);
 step(&right,K_R,0);CHECK(!right.drifting);
 int32_t paused_z=right.position_q8,paused_speed=right.speed_q8;
 step(&right,K_START,K_START);CHECK(right.mode==M_PAUSE);
 for(int i=0;i<90;i++) step(&right,K_R|K_UP|K_LEFT,0);
 CHECK(right.position_q8==paused_z && right.speed_q8==paused_speed);
 step(&right,K_START,K_START);CHECK(right.mode==M_RUN);
 Game creep;begin(&creep);int base=game_top_speed(&creep);
 creep.run.elapsed_ticks=30*DRIVE_HZ-1;CHECK(game_top_speed(&creep)==base);
 creep.run.elapsed_ticks=30*DRIVE_HZ;CHECK(game_top_speed(&creep)>base);
 creep.run.elapsed_ticks=1000000;CHECK(game_top_speed(&creep)<=base+50);
}
static void awards(void) {
 Game g;begin(&g);g.run_ticks=100;g.speed_q8=150*Q;
 g.traffic[0]=(Traffic){25*Q,1*Q,0,1,0,0,1,0};
 step(&g,K_R,0);CHECK(g.run.count[B_NEAR]==1 && g.run.points[B_NEAR]==100);
 for(int i=0;i<12;i++) {step(&g,K_R,0);}
 CHECK(g.run.count[B_NEAR]==1);
 begin(&g);g.run_ticks=100;g.speed_q8=150*Q;
 g.traffic[0]=(Traffic){0,1*Q,0,1,0,0,1,0};
 step(&g,K_R,0);CHECK(g.health<100 && !g.run.count[B_NEAR]);
 uint8_t health=g.health;step(&g,K_R,0);CHECK(g.health==health);
 begin(&g);g.run_ticks=100;g.speed_q8=150*Q;
 g.traffic[0]=(Traffic){55*Q,1*Q,0,1,0,0,1,0};step(&g,K_R,0);CHECK(!g.run.count[B_NEAR]);
 begin(&g);g.speed_q8=150*Q;
 g.pickups[0]=(Pickup){0,2*Q,3,1,0};step(&g,K_R,0);
 CHECK(g.run.count[B_COIN]==1 && g.run.points[B_COIN]==10 && !g.pickups[0].active);
 for(int i=0;i<12;i++) {step(&g,K_R,0);}
 CHECK(g.run.count[B_COIN]==1);
 /* Reversing and re-driving already travelled road cannot farm distance. */
 begin(&g);ticks(&g,180,K_R);uint32_t distance=g.run.distance_m;uint32_t score=g.run.points[B_DISTANCE];
 int32_t far=g.furthest_q8;g.position_q8-=200*Q;g.speed_q8=90*Q;
 ticks(&g,25,K_R);CHECK(g.position_q8<far && g.run.distance_m==distance && g.run.points[B_DISTANCE]==score);
 /* Full-speed streak ramps continuously, reaches its cap and resets on slow. */
 begin(&g);ticks(&g,150,K_R);g.speed_q8=game_top_speed(&g)*Q;
 ticks(&g,30,K_R);unsigned low=g.combo_q8;CHECK(low>Q && low<COMBO_MAX_Q8);
 ticks(&g,300,K_R);CHECK(g.combo_q8==COMBO_MAX_Q8);
 CHECK(g.run.points[B_SPEED]>0 && g.run.count[B_SPEED]>0);
 CHECK(g.run.total_points==sum(g.run.points));
 ticks(&g,1,K_R|K_DOWN);CHECK(g.combo_q8==Q && !g.speed_hold);
 /* A sustained slide can build a streak while the player alternates bends. */
 begin(&g);g.speed_q8=120*Q;g.run_ticks=100;
 for(int i=0;i<360;i++) {
  quiet(&g);g.x_q8=0;g.vx_q8=0;
  step(&g,K_R|K_B|((i/15)&1?K_LEFT:K_RIGHT),0);
  CHECK(g.mode==M_RUN && g.combo_q8<=COMBO_MAX_Q8);
 }
 CHECK(g.run.count[B_DRIFT]>=10 && g.run.drift_points==g.run.points[B_DRIFT]);
 CHECK(g.run.max_combo_q8==COMBO_MAX_Q8);
 /* A motorcycle's A action is a wheelie; B cancels that action. */
 begin(&g);g.save.selected_vehicle=3;g.save.owned|=1u<<3;game_begin_run(&g);quiet(&g);
 g.speed_q8=100*Q;ticks(&g,300,K_R|K_A);
 CHECK(g.wheelie && !g.horn_timer && g.run.count[B_WHEELIE]>0);
 CHECK(g.run.wheelie_points==g.run.points[B_WHEELIE]);
 step(&g,K_R|K_A|K_B,0);CHECK(!g.wheelie);
 /* Every finished run has an exact receipt and can only be banked once. */
 game_finish_run(&g);CHECK(g.mode==M_RECEIPT && g.run.banked);
 CHECK(g.run.total_points==sum(g.run.points));
 CHECK(g.run.total_cash==g.run.count[B_COIN]*5+g.run.total_points/100);
 uint32_t cash=g.save.cash,runs=g.save.total_runs,total=g.save.total_distance;
 game_finish_run(&g);game_finish_run(&g);
 CHECK(g.save.cash==cash && g.save.total_runs==runs && g.save.total_distance==total);
 step(&g,K_A,K_A);CHECK(g.mode==M_HOME);
}
static void economy(void) {
 Game g;game_init(&g,77);g.mode=M_GARAGE;g.selection=1;g.cursor=1;
 step(&g,K_A,K_A);CHECK(!(g.save.owned&2u) && !g.save.cash);
 g.save.cash=vehicles[1].price;step(&g,K_A,K_A);
 CHECK(g.save.owned&2u);CHECK(!g.save.cash && g.save.selected_vehicle==1 && g.save_requested);
 step(&g,K_A,K_A);CHECK(!g.save.cash); /* re-equipping is free */
 g.selection=2;g.cursor=2;g.save.cash=10000;
 uint32_t cash=g.save.cash;step(&g,K_A,K_A);CHECK(g.save.cash==cash && !g.save.engine[2]);
 g.selection=1;
 for(int kind=0;kind<3;kind++) {
  g.cursor=(uint8_t)(2+kind);
  for(int level=0;level<3;level++) {
   int cost=game_upgrade_cost(&g,kind);CHECK(cost>0);cash=g.save.cash;
   step(&g,K_A,K_A);CHECK(g.save.cash==cash-(unsigned)cost);
  }
  CHECK(game_upgrade_cost(&g,kind)==0);cash=g.save.cash;step(&g,K_A,K_A);CHECK(g.save.cash==cash);
 }
 CHECK(g.save.engine[1]==3 && g.save.grip[1]==3 && g.save.armor[1]==3);
 CHECK(!game_upgrade_cost(&g,-1) && !game_upgrade_cost(&g,3));
 g.cursor=5;g.paint_choice=3;step(&g,K_A,K_A);CHECK(!g.save.paint[1]);
 g.paint_choice=2;step(&g,K_A,K_A);CHECK(g.save.paint[1]==2);
 g.mode=M_DEPARTURES;g.cursor=1;g.save.cash=locations[1].price-1;
 step(&g,K_A,K_A);CHECK(!(g.save.unlocked_locations&2u));
 ++g.save.cash;step(&g,K_A,K_A);CHECK(g.save.unlocked_locations&2u);CHECK(!g.save.cash && g.save.selected_location==1);
 step(&g,K_A,K_A);CHECK(!g.save.cash); /* no repeat charge */
 g.cursor=2;g.save.cash=10000;step(&g,K_A,K_A);CHECK(!(g.save.unlocked_locations&4u));
 /* Standard motorcycle is a cash purchase and is never challenge gated. */
 CHECK(vehicles[3].bike && vehicles[3].price==650 && !vehicles[3].unlock);
 CHECK(game_can_vehicle(&g,3));g.mode=M_GARAGE;g.selection=3;g.cursor=1;g.save.cash=650;
 step(&g,K_A,K_A);CHECK((g.save.owned&(1u<<3)) && !g.save.cash);
 /* Challenge rewards are applied to the completed run's best statistics. */
 begin(&g);g.run.count[B_NEAR]=10;g.run.distance_m=6000;g.run.drift_points=1500;
 g.run.best_speed_hold=8*DRIVE_HZ;g.run.wheelie_points=1000;
 game_finish_run(&g);CHECK(g.save.unlocked_locations==29 && (g.save.owned&(1u<<5)) && g.save.paints==63);
 CHECK(!(g.save.owned&(1u<<3))); /* near misses must not gift a standard bike */
 for(int i=0;i<5;i++) {int goal=0;CHECK(game_challenge_progress(&g,i,&goal)==goal && goal>0);CHECK(*game_challenge_name(i));CHECK(*game_challenge_reward(i));}
 CHECK(!game_can_vehicle(&g,-1) && !game_can_vehicle(&g,VEHICLE_COUNT));
 CHECK(!game_can_location(&g,-1) && !game_can_location(&g,LOCATION_COUNT));
}
static void saves(void) {
 CHECK(sizeof(Save)==100);Game g,loaded;Save s;
 game_init(&g,0);game_make_save(&g,&s);CHECK(game_save_valid(&s));
 game_init(&loaded,7);CHECK(game_load(&loaded,&s));CHECK(loaded.has_save && loaded.mode==M_HOME);
 for(size_t i=0;i<sizeof s;i++) {Save bad=s;((unsigned char*)&bad)[i]^=0x40;CHECK(!game_save_valid(&bad));}
 Save bad=s;bad.magic=0x52424d42u;CHECK(!game_load(&loaded,&bad));CHECK(!game_save_valid(NULL));
 /* Re-signing malformed fields still cannot bypass semantic validation. */
 #define INVALID(field,value) do {game_init(&g,9);g.save.field=(value);game_make_save(&g,&bad);CHECK(!game_save_valid(&bad));} while(0)
 INVALID(selected_vehicle,6);INVALID(selected_vehicle,1);INVALID(selected_location,5);
 INVALID(owned,0);INVALID(owned,65);INVALID(unlocked_locations,0);INVALID(unlocked_locations,33);
 INVALID(paints,0);INVALID(paints,127);INVALID(engine[0],4);INVALID(grip[0],4);INVALID(armor[0],4);
 INVALID(paint[0],6);INVALID(paint[0],3);INVALID(cash,UINT32_MAX);INVALID(seed,0);INVALID(best_score,UINT32_MAX);
 #undef INVALID
 game_init(&g,5);game_save_result(&g,0);CHECK(!g.save_ok && !g.has_save);
 game_save_result(&g,1);CHECK(g.save_ok && g.has_save && !g.save_requested);
}
static void grip_response(void) {
 /* Each paid level must change handling for every chassis, even those whose
  * stock grip was already above the old binary response threshold. */
 for(int v=0;v<VEHICLE_COUNT;v++) {
  int32_t last_response=0,last_damping=INT32_MAX,last_slide=0;
  for(int level=0;level<=3;level++) {
   Game normal,slide,damping;
   begin(&normal);normal.vehicle=(uint8_t)v;normal.save.grip[v]=(uint8_t)level;
   normal.speed_q8=120*Q;slide=normal;damping=normal;
   step(&normal,K_R|K_RIGHT,0);step(&slide,K_R|K_RIGHT|K_B,0);
   damping.vx_q8=2*Q;step(&damping,K_R,0);
   CHECK(normal.vx_q8>0 && normal.vx_q8>last_response);
   CHECK(damping.vx_q8>=0 && damping.vx_q8<last_damping);
   CHECK(slide.drifting && slide.vx_q8>last_slide && slide.vx_q8<normal.vx_q8);
   CHECK(normal.x_q8==normal.vx_q8 && slide.x_q8==slide.vx_q8);
   last_response=normal.vx_q8;last_damping=damping.vx_q8;last_slide=slide.vx_q8;
  }
 }
}
static void generated_traffic(void) {
 Game a,b;
 for(uint32_t seed=1;seed<=48;seed++) {
  game_init(&a,seed);game_init(&b,seed);game_begin_run(&a);game_begin_run(&b);
  CHECK(!memcmp(a.traffic,b.traffic,sizeof a.traffic));
  CHECK(!memcmp(a.pickups,b.pickups,sizeof a.pickups));
  for(int i=0;i<TRAFFIC_COUNT;i++) if(a.traffic[i].active) {
   Traffic *t=&a.traffic[i];int row=0;unsigned occupied=0;
   CHECK(t->x_q8>=-81*Q && t->x_q8<=81*Q);
   for(int j=0;j<TRAFFIC_COUNT;j++) if(a.traffic[j].active && a.traffic[j].serial==t->serial) {
    row++;unsigned lane=(unsigned)((a.traffic[j].x_q8/Q+96)/48);
    CHECK(!(occupied&(1u<<lane)));occupied|=1u<<lane;
   }
   CHECK(row>=1 && row<=2); /* Every generated row leaves two open lanes. */
   for(int j=0;j<PICKUP_COUNT;j++) if(a.pickups[j].active && a.pickups[j].serial==t->serial)
    CHECK(a.pickups[j].x_q8!=t->x_q8); /* Coins never ask for a fresh collision. */
  }
  for(int tick=0;tick<3000;tick++) {
   a.x_q8=a.vx_q8=0;a.damage_cooldown=65535;step(&a,K_R|K_UP,0);CHECK(a.mode==M_RUN);
   CHECK(a.run.total_points==sum(a.run.points));
   CHECK(a.x_q8>=-88*Q && a.x_q8<=88*Q && a.combo_q8<=COMBO_MAX_Q8);
   for(int i=0;i<TRAFFIC_COUNT;i++) if(a.traffic[i].active) {
    CHECK(a.traffic[i].x_q8>=-81*Q && a.traffic[i].x_q8<=81*Q);
    CHECK(a.traffic[i].kind<6 && a.traffic[i].paint<5);
   }
  }
 }
 /* Arbitrary button chords cannot break fixed-width state or save bounds. */
 game_init(&a,73);game_begin_run(&a);uint32_t rng=0xa53497u;
 for(int i=0;i<120000;i++) {
  rng=rng*1664525u+1013904223u;
  uint32_t keys=(rng>>8)&255u;
  if(a.mode!=M_RUN) game_begin_run(&a);
  step(&a,keys,keys&(K_A|K_B));
  CHECK(a.x_q8>=-88*Q && a.x_q8<=88*Q && a.health<=100);
  CHECK(a.run.total_points==sum(a.run.points) && a.combo_q8<=COMBO_MAX_Q8);
  if(i%1000==0) {Save save;game_make_save(&a,&save);CHECK(game_save_valid(&save));}
 }
}
int main(void) {
 controls();awards();economy();saves();grip_response();generated_traffic();
 printf("gameplay: %u checks; eight-button physics, streak caps, one-time awards/banking, exact receipts, purchases, challenges and save corruption pass\n",checks);
 return 0;
}
