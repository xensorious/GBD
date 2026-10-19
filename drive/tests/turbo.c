/* Fuel is a run resource: reward deliberate manoeuvres, never passive gas. */
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned checks;
#define CHECK(c) do { ++checks; if(!(c)) { fprintf(stderr,"turbo:%d: %s\n",__LINE__,#c); exit(1); } } while(0)
static void step(Game *g,uint32_t held,uint32_t pressed) {
 Input in={held,pressed};game_update(g,&in);
 CHECK(g->turbo_charge_q8<=TURBO_MAX_Q8);
 CHECK(g->turbo_source<=TS_SLOW);
 CHECK(g->turbo_remainder<100*DRIVE_HZ);
}
static void quiet(Game *g) {
 memset(g->traffic,0,sizeof(g->traffic));memset(g->pickups,0,sizeof(g->pickups));
 g->next_spawn_q8=6000000*Q;
}
static void begin(Game *g,int vehicle) {
 game_init(g,0x12345678u);
 g->save.owned|=1u<<vehicle;g->save.selected_vehicle=(uint8_t)vehicle;
 game_begin_run(g);quiet(g);g->x_q8=g->vx_q8=0;
}
/* Hold the road centre in these unit fixtures to isolate fuel from generated
 * bends. Actual cartridge tests separately exercise joypad-only driving. */
static void ticks(Game *g,int count,uint32_t keys) {
 for(int i=0;i<count;++i) {
  quiet(g);g->x_q8=0;step(g,keys,0);CHECK(g->mode==M_RUN);
 }
}
static void near_car(Game *g,int slot,int lateral) {
 g->traffic[slot]=(Traffic){.x_q8=lateral*Q,.z_q8=g->position_q8+Q,
  .serial=(uint16_t)(slot+1),.active=1,.lane=2,.target_lane=2};
}
static void finite_tank(void) {
 Game g;begin(&g,0);
 CHECK(TURBO_MAX_Q8==61440 && TURBO_START_Q8==6144);
 CHECK(g.turbo_charge_q8==TURBO_MAX_Q8 && !g.turbo_latched);
 for(int i=1;i<=8*DRIVE_HZ;++i) {
  quiet(&g);g.x_q8=0;step(&g,K_R|K_UP,0);
  CHECK(g.turbo && g.turbo_charge_q8==TURBO_MAX_Q8-i*Q);
  CHECK(!g.turbo_recharging && g.turbo_source==TS_NONE);
 }
 CHECK(!g.turbo_charge_q8 && g.turbo_latched);
 ticks(&g,300,K_R|K_UP);
 CHECK(!g.turbo && !g.turbo_charge_q8 && g.turbo_latched);
 /* Even an established speed streak cannot refill fuel. Its deliberate
  * quarter-step ramp now needs 48 seconds rather than eight. */
 CHECK(g.combo_q8>Q && g.combo_q8<COMBO_MAX_Q8 && g.run.points[B_SPEED]>0);
 ticks(&g,SPEED_COMBO_TICKS,K_R);
 CHECK(g.combo_q8==COMBO_MAX_Q8 && !g.turbo_charge_q8 && !g.turbo_latched);
 CHECK(!g.turbo_recharging);
 /* There is no boost pulse before a 10% starting reserve is available. */
 g.turbo_charge_q8=TURBO_START_Q8-1;
 ticks(&g,30,K_R|K_UP);CHECK(!g.turbo && g.turbo_charge_q8==TURBO_START_Q8-1);
 g.turbo_charge_q8=TURBO_START_Q8;
 step(&g,K_R|K_UP,0);CHECK(g.turbo && g.turbo_charge_q8==TURBO_START_Q8-Q);
 ticks(&g,23,K_R|K_UP);CHECK(!g.turbo_charge_q8 && g.turbo_latched);
 step(&g,K_R|K_UP,0);CHECK(!g.turbo);
 /* Slow can recover a depleted tank while Up stays held, but that hold
  * cannot re-arm boost. Let go of Up deliberately before spending it. */
 g.speed_q8=100*Q;
 ticks(&g,5*DRIVE_HZ,K_R|K_DOWN|K_UP);
 CHECK(g.turbo_charge_q8==TURBO_START_Q8 && g.turbo_latched && !g.turbo);
 int fuel=g.turbo_charge_q8;
 ticks(&g,30,K_R|K_UP);CHECK(!g.turbo && g.turbo_charge_q8==fuel);
 step(&g,K_R,0);CHECK(!g.turbo_latched);
 step(&g,K_R|K_UP,0);CHECK(g.turbo && g.turbo_charge_q8==fuel-Q);
 game_finish_run(&g);CHECK(!g.turbo && !g.turbo_recharging);
 game_begin_run(&g);CHECK(g.turbo_charge_q8==TURBO_MAX_Q8 && !g.turbo_latched && !g.turbo_remainder);
}
static void near_miss_fuel(void) {
 Game low,high;begin(&low,0);low.run_ticks=100;low.speed_q8=150*Q;
 low.turbo_charge_q8=0;near_car(&low,0,25);
 step(&low,K_R,0);
 CHECK(low.run.count[B_NEAR]==1 && low.run.points[B_NEAR]==100);
 CHECK(low.turbo_charge_q8==TURBO_MAX_Q8/10 && low.turbo_recharging && low.turbo_source==TS_NEAR);
 int low_fuel=low.turbo_charge_q8;
 for(int i=0;i<20;++i)step(&low,K_R,0);
 CHECK(low.run.count[B_NEAR]==1 && low.turbo_charge_q8==low_fuel);
 begin(&high,0);high.run_ticks=100;high.speed_q8=game_top_speed(&high)*Q;
 high.speed_hold=SPEED_COMBO_TICKS;high.combo_q8=COMBO_MAX_Q8;high.turbo_charge_q8=0;
 near_car(&high,0,25);step(&high,K_R,0);
 CHECK(high.combo_q8==COMBO_MAX_Q8 && high.turbo_charge_q8==2*low_fuel);
 /* Two distinct close passes may pay together; one NPC never pays twice. */
 begin(&high,0);high.run_ticks=100;high.speed_q8=150*Q;high.turbo_charge_q8=0;
 near_car(&high,0,25);near_car(&high,1,-25);step(&high,K_R,0);
 CHECK(high.run.count[B_NEAR]==2 && high.turbo_charge_q8==2*low_fuel);
 /* Fuel cannot be regenerated while it is being spent. */
 begin(&high,0);high.run_ticks=100;high.speed_q8=150*Q;
 high.turbo_charge_q8=TURBO_MAX_Q8/2;near_car(&high,0,25);
 step(&high,K_R|K_UP,0);
 CHECK(high.run.count[B_NEAR]==1 && high.turbo_charge_q8==TURBO_MAX_Q8/2-Q);
 CHECK(high.turbo && !high.turbo_recharging && high.turbo_source==TS_NONE);
 /* Check both callback orders: a same-update impact cancels fuel from a
  * near pass while keeping the pre-existing receipt-scoring rules. */
 for(int impact_first=0;impact_first<2;++impact_first) {
  begin(&high,0);high.run_ticks=100;high.speed_q8=150*Q;high.turbo_charge_q8=1000;
  near_car(&high,impact_first?1:0,25);near_car(&high,impact_first?0:1,0);
  step(&high,K_R,0);
  CHECK(high.health<100 && high.run.count[B_NEAR]==1);
  CHECK(high.turbo_charge_q8==1000 && !high.turbo_recharging && high.turbo_source==TS_NONE);
 }
 begin(&high,0);high.run_ticks=100;high.speed_q8=150*Q;
 high.turbo_charge_q8=TURBO_MAX_Q8-10;near_car(&high,0,25);step(&high,K_R,0);
 CHECK(high.turbo_charge_q8==TURBO_MAX_Q8);
}
static void skill_recharge(void) {
 Game g;begin(&g,0);g.speed_q8=120*Q;g.turbo_charge_q8=0;
 ticks(&g,22,K_R|K_B|K_RIGHT);
 CHECK(g.drifting && g.drift_hold==22 && !g.turbo_charge_q8);
 ticks(&g,1,K_R|K_B|K_RIGHT);
 CHECK(g.turbo_charge_q8>0 && g.turbo_source==TS_DRIFT);
 /* A max-combo controlled slide pays exactly 8%/second. It beats Slow's
  * fixed 2% source instead of adding the two rates together. */
 begin(&g,0);g.speed_q8=120*Q;g.turbo_charge_q8=0;
 g.drift_hold=SKILL_COMBO_TICKS;g.combo_q8=COMBO_MAX_Q8;g.vx_q8=Q;
 ticks(&g,DRIVE_HZ,K_R|K_DOWN|K_B|K_RIGHT);
 CHECK(g.turbo_source==TS_DRIFT && g.combo_q8==COMBO_MAX_Q8);
 CHECK(g.turbo_charge_q8==TURBO_MAX_Q8*8/100);
 CHECK(g.turbo_remainder==600);
 /* A handbrake pose without lateral slip cannot farm drift fuel. */
 begin(&g,0);g.speed_q8=120*Q;g.turbo_charge_q8=0;
 g.drift_hold=SKILL_COMBO_TICKS;g.combo_q8=COMBO_MAX_Q8;g.vx_q8=-3759; /* Cancel one frame of firm countersteer. */
 step(&g,K_R|K_B|K_RIGHT,0);
 CHECK(g.drifting && g.vx_q8<=Q/2 && g.vx_q8>=-Q/2);
 CHECK(!g.turbo_charge_q8 && !g.turbo_recharging);
 /* A wheelie has its own one-second commitment, and cars cannot use it. */
 begin(&g,3);g.speed_q8=100*Q;g.turbo_charge_q8=0;
 ticks(&g,29,K_R|K_A);CHECK(g.wheelie && !g.turbo_charge_q8);
 ticks(&g,1,K_R|K_A);CHECK(g.turbo_charge_q8>0 && g.turbo_source==TS_WHEELIE);
 begin(&g,3);g.speed_q8=100*Q;g.turbo_charge_q8=0;
 g.wheelie_hold=SKILL_COMBO_TICKS;g.combo_q8=COMBO_MAX_Q8;
 ticks(&g,DRIVE_HZ,K_R|K_A|K_DOWN);
 CHECK(g.turbo_source==TS_WHEELIE && g.turbo_charge_q8==TURBO_MAX_Q8*6/100);
 CHECK(g.turbo_remainder==1200);
 begin(&g,0);g.speed_q8=100*Q;g.turbo_charge_q8=0;
 ticks(&g,90,K_R|K_A);CHECK(!g.wheelie && !g.turbo_charge_q8 && !g.turbo_recharging);
 /* Slow is the deliberate, lower-risk recovery option: 50s for a full
  * empty tank. Its rate stays exact despite the per-update fractions. */
 begin(&g,0);g.speed_q8=100*Q;g.turbo_charge_q8=0;
 ticks(&g,DRIVE_HZ,K_R|K_DOWN);
 CHECK(g.turbo_source==TS_SLOW && g.turbo_charge_q8==TURBO_MAX_Q8*2/100);
 CHECK(g.turbo_remainder==2400);
 ticks(&g,49*DRIVE_HZ,K_R|K_DOWN);
 CHECK(g.turbo_charge_q8==TURBO_MAX_Q8 && !g.turbo_remainder);
 ticks(&g,30,K_R|K_DOWN);
 CHECK(g.turbo_charge_q8==TURBO_MAX_Q8 && !g.turbo_recharging && !g.turbo_remainder);
}
static void exclusions_and_pause(void) {
 Game g;
 for(int i=0;i<8;++i) {
  begin(&g,0);g.turbo_charge_q8=0;g.speed_q8=100*Q;
  uint32_t keys=K_R;
  if(i==0)keys=0; /* Coasting. */
  if(i==1)keys=K_R|K_A; /* Horn. */
  if(i==2){g.speed_q8=0;keys=K_DOWN;} /* Idle. */
  if(i==3){g.position_q8=3000*Q;g.speed_q8=-20*Q;keys=K_L|K_DOWN;} /* Reverse. */
  if(i==4){g.crash_timer=12;keys=K_R|K_DOWN;}
  if(i==5){g.damage_cooldown=20;keys=K_R|K_DOWN;}
  if(i==6){g.x_q8=81*Q;keys=K_R|K_DOWN;}
  if(i==7){g.x_q8=-81*Q;keys=K_R|K_DOWN;}
  step(&g,keys,i==1?K_A:0);
  CHECK(!g.turbo_charge_q8 && !g.turbo_recharging && g.turbo_source==TS_NONE);
 }
 /* Fast, plain driving and coins still award score, never fuel. */
 begin(&g,0);g.speed_q8=game_top_speed(&g)*Q;g.turbo_charge_q8=0;
 g.speed_hold=SPEED_COMBO_TICKS;g.combo_q8=COMBO_MAX_Q8;
 g.pickups[0]=(Pickup){0,2*Q,3,1,0};step(&g,K_R,0);
 CHECK(g.run.count[B_COIN]==1 && g.run.points[B_COIN]==10 && !g.turbo_charge_q8);
 ticks(&g,90,K_R);CHECK(g.combo_q8==COMBO_MAX_Q8 && !g.turbo_charge_q8);
 /* Pausing freezes the run resource, including fractional progress. */
 g.turbo_charge_q8=12345;g.turbo_remainder=123;g.turbo_latched=1;
 step(&g,K_START,K_START);CHECK(g.mode==M_PAUSE);
 for(int i=0;i<120;++i)step(&g,K_R|K_DOWN|K_UP,0);
 CHECK(g.turbo_charge_q8==12345 && g.turbo_remainder==123 && g.turbo_latched);
 step(&g,K_START,K_START);CHECK(g.mode==M_RUN);
 /* Gas / brake / Slow priority stays unchanged with a full tank. */
 begin(&g,0);g.speed_q8=100*Q;
 step(&g,K_R|K_UP|K_DOWN,0);CHECK(g.slow && !g.turbo && g.turbo_charge_q8==TURBO_MAX_Q8);
 step(&g,K_R|K_L|K_UP,0);CHECK(!g.slow && !g.turbo && g.turbo_charge_q8==TURBO_MAX_Q8);
 step(&g,K_UP,0);CHECK(!g.turbo && g.turbo_charge_q8==TURBO_MAX_Q8);
}
static void controls_and_save(void) {
 Game g,loaded;Save before,after;begin(&g,0);
 CHECK(sizeof(Save)==708);
 game_make_save(&g,&before);
 g.turbo_charge_q8=7;g.turbo_latched=1;g.turbo_remainder=2999;
 g.turbo_source=TS_DRIFT;g.turbo_recharging=1;
 game_make_save(&g,&after);CHECK(!memcmp(&before,&after,sizeof(before)));
 game_init(&loaded,23);CHECK(game_load(&loaded,&before));
 game_begin_run(&loaded);CHECK(loaded.turbo_charge_q8==TURBO_MAX_Q8 && !loaded.turbo_latched);
 g.mode=M_SETTINGS;g.cursor=8;g.controls_page=1;
 step(&g,K_A,K_A);CHECK(g.mode==M_CONTROLS && !g.controls_page);
 step(&g,K_R,K_R);CHECK(g.controls_page==1);
 step(&g,K_L,K_L);CHECK(!g.controls_page);
 step(&g,K_A,K_A);CHECK(g.mode==M_SETTINGS && g.cursor==8);
 g.mode=M_PAUSE;g.cursor=2;g.controls_page=1;
 step(&g,K_A,K_A);CHECK(g.mode==M_CONTROLS && !g.controls_page);
 step(&g,K_L,K_L);CHECK(g.controls_page==1);
 step(&g,K_B,K_B);CHECK(g.mode==M_PAUSE && g.cursor==2);
}
static void recovery_upgrades(void) {
 int previous=0;
 for(int rank=0;rank<=UPGRADE_MAX;++rank) {
  Game g;begin(&g,0);g.save.recovery[0]=(uint8_t)rank;g.speed_q8=100*Q;g.turbo_charge_q8=0;
  ticks(&g,DRIVE_HZ,K_R|K_DOWN);
  CHECK(g.turbo_source==TS_SLOW);
  CHECK(g.turbo_charge_q8==TURBO_MAX_Q8*2*(100+5*rank)/10000);
  CHECK(g.turbo_charge_q8>previous);previous=g.turbo_charge_q8;
  begin(&g,0);g.save.recovery[0]=(uint8_t)rank;g.speed_q8=120*Q;g.turbo_charge_q8=0;
  g.drift_hold=SKILL_COMBO_TICKS;g.combo_q8=COMBO_MAX_Q8;g.vx_q8=Q;
  ticks(&g,DRIVE_HZ,K_R|K_DOWN|K_B|K_RIGHT);
  CHECK(g.turbo_source==TS_DRIFT && g.combo_q8==COMBO_MAX_Q8);
  CHECK(g.turbo_charge_q8==TURBO_MAX_Q8*8*(100+5*rank)/10000);
  begin(&g,0);g.save.recovery[0]=(uint8_t)rank;g.run_ticks=100;
  g.speed_q8=game_top_speed(&g)*Q;g.speed_hold=SPEED_COMBO_TICKS;g.combo_q8=COMBO_MAX_Q8;
  g.turbo_charge_q8=0;near_car(&g,0,25);step(&g,K_R,0);
  CHECK(g.turbo_source==TS_NEAR && g.turbo_charge_q8==(TURBO_MAX_Q8/10)*2*(100+5*rank)/100);
  /* Recovery parts increase replenishment, never the boost duration. */
  begin(&g,0);g.save.recovery[0]=(uint8_t)rank;
  ticks(&g,8*DRIVE_HZ,K_R|K_UP);
  CHECK(!g.turbo_charge_q8 && g.turbo_latched);
 }
 /* The new two-wheel models use the same deliberate wheelie recharge. */
 for(int v=26;v<=27;++v) {
  Game g;begin(&g,v);g.speed_q8=100*Q;g.turbo_charge_q8=0;
  ticks(&g,DRIVE_HZ,K_R|K_A);CHECK(g.wheelie && g.turbo_source==TS_WHEELIE);
 }
}
int main(void) {
 finite_tank();near_miss_fuel();skill_recharge();exclusions_and_pause();controls_and_save();recovery_upgrades();
 printf("turbo: %u checks; exact 8s fuel, seven recovery ranks, skill-only recharge, 5x-to-2x balance, exclusions, pause and saves pass\n",checks);
 return 0;
}
