#include "game.h"
#include "road.h"
#include <stddef.h>
#include <string.h>

#define SAVE_MAGIC 0x434c4452u
#define SAVE_VERSION 1u
#define MONEY_LIMIT 9999999u
#define STAT_LIMIT 999999999u
#define WORLD_LIMIT (7000000*Q)
#define PAINT_COUNT 6

const VehicleDef vehicles[VEHICLE_COUNT] = {
 {"Pacer","A nimble everyday hatchback.","Your first set of keys",0,175,46,78,20,0,0},
 {"Vela GT","A fast coupe with a gentle slide.","Buy for $750",750,215,54,71,15,0,0},
 {"Nomad","A sturdy wagon that takes a hit.","Buy for $1000",1000,185,42,85,45,0,0},
 {"Kestrel","A light motorcycle. A holds a wheelie.","Buy for $650",650,205,60,92,4,1,0},
 {"Vector","A quick, planted supercar.","Buy for $2500",2500,245,65,88,18,0,0},
 {"Sundown","A classic built for long drifts.","1500 drift points in one run",0,225,56,65,25,0,3}
};
const LocationDef locations[LOCATION_COUNT] = {
 {"Metro Dawn","Glass towers, warm dawn and four open lanes.","Open from the start",0,0},
 {"Palm Coast","A tropical highway beside the sea.","Buy for $600",600,0},
 {"Skywater Causeway","An endless ribbon above blue water.","10 near misses in one run",0,1},
 {"Pine Ridge","Cool mountain air and forested shoulders.","Travel 6000 m in one run",0,2},
 {"Redrock Pass","Warm sandstone and sweeping canyon roads.","Earn 1500 drift points in one run",0,3}
};
const char *const bonus_names[BONUS_COUNT] = {
 "Distance", "Time on the road", "Near miss", "Full speed", "Drift", "Wheelie", "Road coin"
};

static int clampi(int n,int lo,int hi) { return n<lo?lo:n>hi?hi:n; }
static int absi(int n) { return n<0?-n:n; }
static uint32_t add_sat(uint32_t a,uint32_t b,uint32_t limit) {
 return a>=limit || b>limit-a ? limit : a+b;
}
static void notice(Game *g,const char *s) {
 size_t n=strlen(s); if(n>=sizeof(g->notice)) n=sizeof(g->notice)-1;
 memcpy(g->notice,s,n); g->notice[n]=0; g->notice_timer=120;
}
static uint32_t checksum(const Save *save) {
 const uint8_t *p=(const uint8_t *)save; uint32_t h=2166136261u;
 for(size_t i=0;i<sizeof(*save);++i) {
  uint8_t b=(i>=offsetof(Save,checksum)&&i<offsetof(Save,checksum)+4)?0:p[i];
  h=(h^b)*16777619u;
 }
 return h;
}
void game_make_save(const Game *g,Save *out) {
 *out=g->save; out->magic=SAVE_MAGIC; out->version=SAVE_VERSION;
 out->checksum=0; out->checksum=checksum(out);
}
int game_save_valid(const Save *s) {
 if(!s || s->magic!=SAVE_MAGIC || s->version!=SAVE_VERSION || s->checksum!=checksum(s)) return 0;
 if(s->cash>MONEY_LIMIT || s->total_cash>STAT_LIMIT || !s->seed) return 0;
 if(!(s->owned&1u) || (s->owned&~63u) || !(s->unlocked_locations&1u) || (s->unlocked_locations&~31u)) return 0;
 if((s->paints&7u)!=7u || (s->paints&~63u)) return 0;
 if(s->selected_vehicle>=VEHICLE_COUNT || !(s->owned&(1u<<s->selected_vehicle))) return 0;
 if(s->selected_location>=LOCATION_COUNT || !(s->unlocked_locations&(1u<<s->selected_location))) return 0;
 for(int i=0;i<VEHICLE_COUNT;++i) {
  if(s->engine[i]>3 || s->grip[i]>3 || s->armor[i]>3 || s->paint[i]>=PAINT_COUNT || !(s->paints&(1u<<s->paint[i]))) return 0;
 }
 if(s->best_score>STAT_LIMIT || s->best_distance>STAT_LIMIT || s->best_near>STAT_LIMIT ||
    s->best_drift>STAT_LIMIT || s->best_speed_hold>STAT_LIMIT || s->best_wheelie>STAT_LIMIT ||
    s->best_coins>STAT_LIMIT || s->total_runs>STAT_LIMIT || s->total_distance>STAT_LIMIT) return 0;
 return 1;
}
int game_load(Game *g,const Save *save) {
 if(!game_save_valid(save)) return 0;
 g->save=*save; g->has_save=1; g->save_ok=1; g->save_requested=0;
 g->mode=M_HOME; g->cursor=0; g->selection=save->selected_vehicle;
 g->vehicle=save->selected_vehicle; g->start_biome=save->selected_location;
 g->biome=g->start_biome; g->paint_choice=save->paint[g->selection];
 return 1;
}
void game_save_result(Game *g,int okay) {
 g->save_requested=0; g->save_ok=(uint8_t)(okay!=0);
 if(okay) g->has_save=1;
 else notice(g,"Save unavailable. Keep playing; retry after a run.");
}
void game_init(Game *g,uint32_t seed) {
 memset(g,0,sizeof(*g)); g->save.magic=SAVE_MAGIC; g->save.version=SAVE_VERSION;
 g->save.seed=seed?seed:0x73bda521u; g->save.owned=1; g->save.unlocked_locations=1;
 g->save.paints=7; g->mode=M_HOME; g->health=100; g->combo_q8=Q;
}

static int challenge_done(const Game *g,int id) {
 switch(id) {
  case 1:return g->save.best_near>=10;
  case 2:return g->save.best_distance>=6000;
  case 3:return g->save.best_drift>=1500;
  default:return 1;
 }
}
int game_can_vehicle(const Game *g,int id) {
 return id>=0&&id<VEHICLE_COUNT && challenge_done(g,vehicles[id].unlock);
}
int game_can_location(const Game *g,int id) {
 return id>=0&&id<LOCATION_COUNT && challenge_done(g,locations[id].unlock);
}
int game_top_speed(const Game *g) {
 int v=g->vehicle<VEHICLE_COUNT?g->vehicle:0;
 int creep=(int)(g->run.elapsed_ticks/(30*DRIVE_HZ))*2;
 return vehicles[v].top_speed + g->save.engine[v]*12 + clampi(creep,0,50);
}
int game_upgrade_cost(const Game *g,int kind) {
 if(kind<0 || kind>2 || g->selection>=VEHICLE_COUNT) return 0;
 int v=g->selection;
 int level=kind==0?g->save.engine[v]:kind==1?g->save.grip[v]:g->save.armor[v];
 if(level>=3) return 0;
 return (kind==0?250:kind==1?175:200)*(level+1);
}
const char *game_challenge_name(int i) {
 static const char *const names[5]={"Thread the Needle","Long Way Home","Sideways Summer","Redline Rhythm","One Wheel Wonder"};
 return i>=0&&i<5?names[i]:"";
}
const char *game_challenge_reward(int i) {
 static const char *const rewards[5]={"Skywater Causeway departure","Pine Ridge departure","Sundown + Redrock + Graphite","Sunburst paint","Orchid paint"};
 return i>=0&&i<5?rewards[i]:"";
}
int game_challenge_progress(const Game *g,int i,int *goal) {
 int n=0,d=1;
 switch(i) {
  case 0:n=(int)g->save.best_near;d=10;break;
  case 1:n=(int)g->save.best_distance;d=6000;break;
  case 2:n=(int)g->save.best_drift;d=1500;break;
  case 3:n=(int)g->save.best_speed_hold;d=8*DRIVE_HZ;break;
  case 4:n=(int)g->save.best_wheelie;d=1000;break;
 }
 if(goal) *goal=d;
 return clampi(n,0,d);
}
static void reward_unlocks(Game *g) {
 if(g->save.best_near>=10) g->save.unlocked_locations|=1u<<2;
 if(g->save.best_distance>=6000) g->save.unlocked_locations|=1u<<3;
 if(g->save.best_drift>=1500) { g->save.owned|=1u<<5; g->save.unlocked_locations|=1u<<4; g->save.paints|=1u<<5; }
 if(g->save.best_speed_hold>=8*DRIVE_HZ) g->save.paints|=1u<<3;
 if(g->save.best_wheelie>=1000) g->save.paints|=1u<<4;
}
static void points(Game *g,int category,uint32_t count,uint32_t amount) {
 /* Keep the receipt sum exact even at the deliberately remote score cap. */
 uint32_t room=STAT_LIMIT-g->run.total_points;
 if(amount>room) amount=room;
 g->run.count[category]=add_sat(g->run.count[category],count,STAT_LIMIT);
 g->run.points[category]+=amount; g->run.total_points+=amount;
 uint32_t coin_cash=g->run.count[B_COIN]>MONEY_LIMIT/5?MONEY_LIMIT:g->run.count[B_COIN]*5;
 g->run.total_cash=add_sat(coin_cash,g->run.total_points/100,MONEY_LIMIT);
}
static int combo_for(uint32_t hold) {
 uint32_t capped=hold>8*DRIVE_HZ?8*DRIVE_HZ:hold;
 return Q+(int)(capped*4*Q/(8*DRIVE_HZ));
}
static int free_traffic(const Game *g) { for(int i=0;i<TRAFFIC_COUNT;++i) if(!g->traffic[i].active) return i; return -1; }
static int free_pickup(const Game *g) { for(int i=0;i<PICKUP_COUNT;++i) if(!g->pickups[i].active) return i; return -1; }
static void spawn_row(Game *g,int32_t z) {
 uint32_t h=road_hash(g->run_seed,(uint32_t)g->spawn_serial+0x4157u);
 int lane1=(int)(h&3),lane2=(lane1+1+(int)((h>>6)%3))&3;
 int n=((h>>9)&3)?2:1;
 for(int j=0;j<n;++j) {
  int slot=free_traffic(g); if(slot<0) break;
  Traffic *t=&g->traffic[slot]; memset(t,0,sizeof(*t));
  int lane=j?lane2:lane1;
  int jitter=(int)(road_hash(h,(uint32_t)j+97u)%19)-9;
  t->x_q8=(-72+lane*48+jitter)*Q; t->z_q8=z;
  t->speed_q8=(80+(int)((h>>(j*5+11))%46))*Q;
  t->kind=(uint8_t)((h>>(j*3+20))%6); t->paint=(uint8_t)((h>>(j*4+23))%5);
  t->serial=g->spawn_serial; t->active=1;
 }
 int coin_lane=(int)((h>>17)&3);
 for(int tries=0;tries<4 && (coin_lane==lane1 || (n==2&&coin_lane==lane2));++tries) coin_lane=(coin_lane+1)&3;
 for(int j=0;j<3;++j) {
  int slot=free_pickup(g); if(slot<0) break;
  Pickup *p=&g->pickups[slot]; memset(p,0,sizeof(*p));
  p->x_q8=(-72+coin_lane*48)*Q; p->z_q8=z-(60+j*24)*Q;
  p->serial=g->spawn_serial; p->active=1;
 }
 ++g->spawn_serial;
}
static void spawn_ahead(Game *g) {
 while(g->next_spawn_q8<=g->furthest_q8+ROAD_VIEW*Q && g->next_spawn_q8<WORLD_LIMIT) {
  spawn_row(g,g->next_spawn_q8);
  uint32_t h=road_hash(g->run_seed,(uint32_t)g->spawn_serial+0xaceu);
  g->next_spawn_q8+=(170+(int)(h%61))*Q;
 }
}
void game_begin_run(Game *g) {
 memset(&g->run,0,sizeof(g->run)); memset(g->traffic,0,sizeof(g->traffic)); memset(g->pickups,0,sizeof(g->pickups));
 g->run_seed=road_hash(g->save.seed,g->save.total_runs+1+g->frame); if(!g->run_seed) g->run_seed=1;
 g->save.seed=g->run_seed; g->run_ticks=0;
 g->position_q8=g->furthest_q8=g->odometer_q8=g->x_q8=g->vx_q8=g->speed_q8=g->accel_q8=0;
 g->x_q8=-24*Q;
 g->speed_hold=g->drift_hold=g->wheelie_hold=0;
 g->combo_q8=Q; g->damage_cooldown=g->crash_timer=g->horn_timer=g->shake=g->spawn_serial=0;
 g->steering=g->yaw=0; g->health=100; g->vehicle=g->save.selected_vehicle;
 g->start_biome=g->save.selected_location; g->biome=g->start_biome;
 g->turbo=g->slow=g->drifting=g->wheelie=0; g->notice_timer=0;
 g->mode=M_RUN; g->cursor=0; g->next_spawn_q8=200*Q; spawn_ahead(g);
}
void game_finish_run(Game *g) {
 if(g->run.banked) { g->mode=M_RECEIPT; return; }
 g->run.banked=1;
 uint32_t coin_cash=g->run.count[B_COIN]>MONEY_LIMIT/5?MONEY_LIMIT:g->run.count[B_COIN]*5;
 g->run.total_cash=add_sat(coin_cash,g->run.total_points/100,MONEY_LIMIT);
 g->save.cash=add_sat(g->save.cash,g->run.total_cash,MONEY_LIMIT);
 g->save.total_cash=add_sat(g->save.total_cash,g->run.total_cash,STAT_LIMIT);
 g->save.total_runs=add_sat(g->save.total_runs,1,STAT_LIMIT);
 g->save.total_distance=add_sat(g->save.total_distance,g->run.distance_m,STAT_LIMIT);
 if(g->run.total_points>g->save.best_score) g->save.best_score=g->run.total_points;
 if(g->run.distance_m>g->save.best_distance) g->save.best_distance=g->run.distance_m;
 if(g->run.count[B_NEAR]>g->save.best_near) g->save.best_near=g->run.count[B_NEAR];
 if(g->run.drift_points>g->save.best_drift) g->save.best_drift=g->run.drift_points;
 if(g->run.best_speed_hold>g->save.best_speed_hold) g->save.best_speed_hold=g->run.best_speed_hold;
 if(g->run.wheelie_points>g->save.best_wheelie) g->save.best_wheelie=g->run.wheelie_points;
 if(g->run.count[B_COIN]>g->save.best_coins) g->save.best_coins=g->run.count[B_COIN];
 reward_unlocks(g); g->save_requested=1; g->mode=M_RECEIPT; g->cursor=0;
 g->turbo=g->slow=g->drifting=g->wheelie=0;
}

static void damage(Game *g,int raw,int rail) {
 if(g->damage_cooldown || g->run_ticks<2*DRIVE_HZ) return;
 int armor=vehicles[g->vehicle].armor/4+g->save.armor[g->vehicle]*5;
 int hurt=raw-armor; if(hurt<5) hurt=5;
 g->health=(uint8_t)(g->health>hurt?g->health-hurt:0);
 g->damage_cooldown=rail?30:38; g->crash_timer=12; g->shake=10;
 g->speed_q8=g->speed_q8*(rail?88:62)/100;
 g->speed_hold=g->drift_hold=g->wheelie_hold=0; g->combo_q8=Q;
}
static void traffic_tick(Game *g,int32_t old_position) {
 for(int i=0;i<TRAFFIC_COUNT;++i) {
  Traffic *t=&g->traffic[i]; if(!t->active) continue;
  int32_t before=t->z_q8-old_position;
  t->z_q8+=t->speed_q8/32;
  int32_t dz=t->z_q8-g->position_q8;
  if(dz < -120*Q || dz > 3*ROAD_VIEW*Q) { t->active=0; continue; }
  int dx=absi(t->x_q8-g->x_q8);
  if(g->horn_timer && dz>45*Q && dz<180*Q && dx<23*Q) {
   int dir=t->x_q8<0?-1:1;
   int32_t target=t->x_q8+dir*Q;
   int safe=absi(target)<=80*Q;
   for(int j=0;j<TRAFFIC_COUNT&&safe;++j) if(j!=i && g->traffic[j].active &&
      absi(g->traffic[j].z_q8-t->z_q8)<50*Q && absi(g->traffic[j].x_q8-target)<35*Q) safe=0;
   if(safe) t->x_q8=target;
   dx=absi(t->x_q8-g->x_q8);
  }
  int half_width=t->kind>=4?19:17,half_length=t->kind>=4?31:25;
  if(absi(dz)<half_length*Q && dx<half_width*Q) {
   t->near_awarded=1; damage(g,43+absi(g->speed_q8-t->speed_q8)/(10*Q),0);
  }
  if(!t->near_awarded && before>0 && dz<=0) {
   t->near_awarded=1;
   if(dx>=half_width*Q && dx<=36*Q && g->speed_q8>t->speed_q8 && g->speed_q8>40*Q)
    points(g,B_NEAR,1,100);
  }
 }
}
static void pickup_tick(Game *g,int32_t old_position) {
 int32_t lo=old_position<g->position_q8?old_position:g->position_q8;
 int32_t hi=old_position>g->position_q8?old_position:g->position_q8;
 for(int i=0;i<PICKUP_COUNT;++i) {
  Pickup *p=&g->pickups[i]; if(!p->active) continue;
  int32_t dz=p->z_q8-g->position_q8;
  if(dz < -120*Q || dz>3*ROAD_VIEW*Q) { p->active=0; continue; }
  if(p->z_q8>=lo-12*Q && p->z_q8<=hi+12*Q && absi(p->x_q8-g->x_q8)<14*Q) {
   p->active=0; points(g,B_COIN,1,10);
  }
 }
}
static void bonus_tick(Game *g,int target,int motion) {
 int speed_ok=motion && !g->slow && !g->crash_timer && g->speed_q8>=target*Q*95/100;
 g->speed_hold=speed_ok?g->speed_hold+1:0;
 g->drift_hold=g->drifting?g->drift_hold+1:0;
 g->wheelie_hold=g->wheelie?g->wheelie_hold+1:0;
 if(g->speed_hold>g->run.best_speed_hold) g->run.best_speed_hold=g->speed_hold;
 int best=combo_for(g->speed_hold),c=combo_for(g->drift_hold);
 if(c>best) best=c;
 c=combo_for(g->wheelie_hold);
 if(c>best) best=c;
 g->combo_q8=(uint16_t)best;
 if(g->combo_q8>g->run.max_combo_q8) g->run.max_combo_q8=g->combo_q8;
 if(g->speed_hold && g->speed_hold%DRIVE_HZ==0) points(g,B_SPEED,1,(uint32_t)(20*combo_for(g->speed_hold)/Q));
 if(g->drift_hold && g->drift_hold%DRIVE_HZ==0) {
  uint32_t p=(uint32_t)(35*combo_for(g->drift_hold)/Q); points(g,B_DRIFT,1,p);
  g->run.drift_points=add_sat(g->run.drift_points,p,STAT_LIMIT);
 }
 if(g->wheelie_hold && g->wheelie_hold%DRIVE_HZ==0) {
  uint32_t p=(uint32_t)(35*combo_for(g->wheelie_hold)/Q); points(g,B_WHEELIE,1,p);
  g->run.wheelie_points=add_sat(g->run.wheelie_points,p,STAT_LIMIT);
 }
}
static void run_tick(Game *g,const Input *in) {
 if(in->pressed&K_START) { g->mode=M_PAUSE; g->cursor=0; return; }
 ++g->run_ticks;
 if(g->damage_cooldown) --g->damage_cooldown;
 if(g->crash_timer) --g->crash_timer;
 if(g->horn_timer) --g->horn_timer;
 if(g->shake) --g->shake;
 int v=g->vehicle,brake=(in->held&K_L)!=0;
 int gas=(in->held&K_R)!=0 && !brake;
 g->slow=(uint8_t)(gas && (in->held&K_DOWN));
 g->turbo=(uint8_t)(gas && !g->slow && (in->held&K_UP));
 int base=game_top_speed(g),target=g->slow?base*62/100:g->turbo?base*125/100:base;
 int32_t oldspeed=g->speed_q8;
 int acceleration=vehicles[v].acceleration+g->save.engine[v]*5;
 if(gas) {
  int rate=g->slow?acceleration*65/100:g->turbo?acceleration*14/10:acceleration;
  if(g->speed_q8<0) rate+=70;
  int32_t step=rate*Q/DRIVE_HZ;
  if(g->speed_q8<target*Q) { g->speed_q8+=step; if(g->speed_q8>target*Q) g->speed_q8=target*Q; }
  else { g->speed_q8-=90*Q/DRIVE_HZ; if(g->speed_q8<target*Q) g->speed_q8=target*Q; }
 } else if(brake) {
  g->speed_q8-=(g->speed_q8>0?115:25)*Q/DRIVE_HZ;
  if(g->speed_q8 < -34*Q) g->speed_q8=-34*Q;
 } else {
  int drag=18*Q/DRIVE_HZ;
  if(g->speed_q8>drag) g->speed_q8-=drag;
  else if(g->speed_q8 < -drag) g->speed_q8+=drag;
  else g->speed_q8=0;
 }
 g->steering=(int16_t)(((in->held&K_RIGHT)?1:0)-((in->held&K_LEFT)?1:0));
 int hand=(in->held&K_B)!=0;
 g->drifting=(uint8_t)(hand && g->steering && g->speed_q8>45*Q && !g->crash_timer);
 g->wheelie=(uint8_t)(vehicles[v].bike && (in->held&K_A) && g->speed_q8>50*Q && !hand && !g->crash_timer);
 if((in->pressed&K_A) && !vehicles[v].bike) g->horn_timer=30;
 if(hand && g->speed_q8>0) {
  g->speed_q8-=12*Q/DRIVE_HZ;
  if(g->speed_q8<0) g->speed_q8=0;
 }
 int speed=absi(g->speed_q8)/Q;
 int steering_power=clampi(speed,0,120)*4;
 if(hand) steering_power=steering_power*12/10;
 if(g->wheelie) steering_power=steering_power*7/10;
 int desired=g->steering*steering_power;
 if(g->speed_q8<0) desired=-desired;
 int grip=vehicles[v].grip+g->save.grip[v]*7;
 /* Grip changes both turn-in and recovery from a slide at every upgrade.
    The handbrake keeps a much lower gain so sideways momentum survives. */
 int gain=hand?18+grip/4:clampi(2*grip-55,64,192);
 g->vx_q8+=(desired-g->vx_q8)*gain/Q;
 RoadSample road; road_sample(g->run_seed,g->position_q8/Q,g->start_biome,&road);
 g->biome=(road.blend>=128)?road.next_biome:road.biome;
 if(speed>15) g->vx_q8-=road.curve*g->speed_q8/(Q*400);
 g->x_q8+=g->vx_q8;
 int yaw_target=g->steering*(hand?90:42)+g->vx_q8/22;
 g->yaw=(int16_t)(g->yaw+(yaw_target-g->yaw)/3);
 g->yaw=(int16_t)clampi(g->yaw,-100,100);
 if(g->x_q8 > 88*Q) { g->x_q8=88*Q; g->vx_q8=-absi(g->vx_q8)/2; if(speed>12) damage(g,17,1); }
 if(g->x_q8 < -88*Q) { g->x_q8=-88*Q; g->vx_q8=absi(g->vx_q8)/2; if(speed>12) damage(g,17,1); }
 int32_t old_position=g->position_q8;
 g->position_q8+=g->speed_q8/32;
 if(g->position_q8<0) { g->position_q8=0; if(g->speed_q8<0) g->speed_q8=0; }
 if(g->position_q8>WORLD_LIMIT) g->position_q8=WORLD_LIMIT;
 if(g->position_q8>g->furthest_q8) {
  int32_t fresh=g->position_q8-g->furthest_q8;
  g->furthest_q8=g->position_q8;
  g->odometer_q8+=fresh;
  uint32_t metres=(uint32_t)(g->odometer_q8/(3*Q));
  if(metres>g->run.distance_m) points(g,B_DISTANCE,metres-g->run.distance_m,metres-g->run.distance_m);
  g->run.distance_m=metres;
 }
 int motion=gas && g->speed_q8>20*Q;
 if(motion) {
  ++g->run.elapsed_ticks;
  if(g->run.elapsed_ticks%DRIVE_HZ==0) points(g,B_SURVIVAL,1,5);
 }
 traffic_tick(g,old_position); pickup_tick(g,old_position); spawn_ahead(g);
 g->accel_q8=(g->speed_q8-oldspeed)*DRIVE_HZ;
 if(g->speed_q8>0 && (uint32_t)(g->speed_q8/Q)>g->run.max_speed) g->run.max_speed=(uint32_t)(g->speed_q8/Q);
 bonus_tick(g,target,motion);
 if(!g->health || g->position_q8>=WORLD_LIMIT) game_finish_run(g);
}

static int vertical(const Input *in) {
 return ((in->pressed&K_DOWN)?1:0)-((in->pressed&K_UP)?1:0);
}
static int horizontal(const Input *in) {
 return ((in->pressed&K_RIGHT)?1:0)-((in->pressed&K_LEFT)?1:0);
}
static void purchase_vehicle(Game *g) {
 int v=g->selection;
 if(g->save.owned&(1u<<v)) {
  g->save.selected_vehicle=(uint8_t)v; g->vehicle=(uint8_t)v; g->save_requested=1;
  notice(g,"Keys ready. Vehicle equipped."); return;
 }
 if(!game_can_vehicle(g,v)) { notice(g,vehicles[v].requirement); return; }
 if(g->save.cash<vehicles[v].price) { notice(g,"Not enough cash. Take another drive."); return; }
 g->save.cash-=vehicles[v].price; g->save.owned|=1u<<v;
 g->save.selected_vehicle=(uint8_t)v; g->vehicle=(uint8_t)v; g->save_requested=1;
 notice(g,"New keys! Vehicle bought and equipped.");
}
static void upgrade(Game *g,int kind) {
 int v=g->selection;
 if(!(g->save.owned&(1u<<v))) { notice(g,"Own this vehicle before upgrading it."); return; }
 int cost=game_upgrade_cost(g,kind);
 if(!cost) { notice(g,"This upgrade is already at level 3."); return; }
 if(g->save.cash<(uint32_t)cost) { notice(g,"Not enough cash for this upgrade."); return; }
 g->save.cash-=(uint32_t)cost;
 uint8_t *level=kind==0?g->save.engine:kind==1?g->save.grip:g->save.armor;
 ++level[v]; g->save_requested=1; notice(g,"Upgrade installed.");
}
static void garage_tick(Game *g,const Input *in) {
 if(in->pressed&K_B) { g->mode=M_HOME;g->cursor=1;return; }
 int dy=vertical(in),dx=horizontal(in);
 g->cursor=(uint8_t)((g->cursor+6+dy)%6);
 if(g->cursor==0 && dx) {
  g->selection=(uint8_t)((g->selection+VEHICLE_COUNT+dx)%VEHICLE_COUNT);
  g->paint_choice=g->save.paint[g->selection];
 }
 if(g->cursor==5 && dx) {
  g->paint_choice=(uint8_t)((g->paint_choice+PAINT_COUNT+dx)%PAINT_COUNT);
 }
 if(!(in->pressed&K_A)) return;
 switch(g->cursor) {
  case 0:g->cursor=1;break;
  case 1:purchase_vehicle(g);break;
  case 2:case 3:case 4:upgrade(g,g->cursor-2);break;
  case 5:
   if(!(g->save.owned&(1u<<g->selection))) notice(g,"Own this vehicle before painting it.");
   else if(!(g->save.paints&(1u<<g->paint_choice))) notice(g,g->paint_choice==3?"Hold full speed for 8 seconds to unlock.":g->paint_choice==4?"Earn 1000 wheelie points in one run to unlock.":"Earn 1500 drift points in one run to unlock.");
   else { g->save.paint[g->selection]=g->paint_choice;g->save_requested=1;notice(g,"Fresh paint applied."); }
   break;
 }
}
static void departure_tick(Game *g,const Input *in) {
 if(in->pressed&K_B) {g->mode=M_HOME;g->cursor=2;return;}
 g->cursor=(uint8_t)((g->cursor+LOCATION_COUNT+vertical(in))%LOCATION_COUNT);
 if(!(in->pressed&K_A)) return;
 int id=g->cursor;
 if(!(g->save.unlocked_locations&(1u<<id))) {
  if(!game_can_location(g,id)) {notice(g,locations[id].requirement);return;}
  if(g->save.cash<locations[id].price) {notice(g,"Not enough cash for this departure.");return;}
  g->save.cash-=locations[id].price;g->save.unlocked_locations|=1u<<id;
 }
 g->save.selected_location=(uint8_t)id;g->start_biome=(uint8_t)id;g->biome=(uint8_t)id;
 g->save_requested=1;notice(g,"Departure set. Your next run starts here.");
}
void game_update(Game *g,const Input *in) {
 ++g->frame;if(g->notice_timer) --g->notice_timer;
 switch(g->mode) {
  case M_RUN:run_tick(g,in);break;
  case M_RECEIPT:
   if(in->pressed&K_A) {g->mode=M_HOME;g->cursor=0;}
   break;
  case M_HOME:
   g->cursor=(uint8_t)((g->cursor+5+vertical(in))%5);
   if(in->pressed&K_A) {
    switch(g->cursor) {
     case 0:game_begin_run(g);break;
     case 1:g->mode=M_GARAGE;g->cursor=0;g->selection=g->save.selected_vehicle;g->paint_choice=g->save.paint[g->selection];break;
     case 2:g->mode=M_DEPARTURES;g->cursor=g->save.selected_location;break;
     case 3:g->mode=M_CHALLENGES;g->cursor=0;break;
     case 4:g->mode=M_CONTROLS;g->paused_from=M_HOME;break;
    }
   }
   break;
  case M_GARAGE:garage_tick(g,in);break;
  case M_DEPARTURES:departure_tick(g,in);break;
  case M_CHALLENGES:
   g->cursor=(uint8_t)((g->cursor+5+vertical(in))%5);
   if(in->pressed&(K_A|K_B)) {g->mode=M_HOME;g->cursor=3;}
   break;
  case M_CONTROLS:
   if(in->pressed&(K_A|K_B)) {
    g->mode=g->paused_from==M_PAUSE?M_PAUSE:M_HOME;
    g->cursor=g->mode==M_PAUSE?2:4;
   }
   break;
  case M_PAUSE:
   if(in->pressed&(K_B|K_START)) {g->mode=M_RUN;break;}
   g->cursor=(uint8_t)((g->cursor+3+vertical(in))%3);
   if(in->pressed&K_A) {
    if(g->cursor==0) g->mode=M_RUN;
    else if(g->cursor==1) game_finish_run(g);
    else {g->mode=M_CONTROLS;g->paused_from=M_PAUSE;}
   }
   break;
  default:g->mode=M_HOME;g->cursor=0;break;
 }
}
