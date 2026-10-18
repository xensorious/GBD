#include "game.h"
#include "road.h"
#include "traffic.h"
#include <stddef.h>
#include <string.h>

#define SAVE_MAGIC 0x434c4452u
#define SAVE_VERSION 2u
#define MONEY_LIMIT 9999999u
#define STAT_LIMIT 999999999u
#define WORLD_LIMIT (7000000*Q)
#define PAINT_COUNT 6

const VehicleDef vehicles[VEHICLE_COUNT] = {
 {"Pacer","A nimble everyday hatchback.","Your first set of keys",0,175,46,78,20,0,0},
 {"Vela GT","A fast coupe with a gentle slide.","Buy for $750",750,215,54,71,15,0,0},
 {"Nomad","A sturdy pickup that takes a hit.","Buy for $1000",1000,185,42,85,45,0,0},
 {"Kestrel","A light motorcycle. A holds a wheelie.","Buy for $650",650,205,60,92,4,1,0},
 {"Vector","A quick, planted supercar.","Buy for $2500",2500,245,65,88,18,0,0},
 {"Sundown","A classic built for long drifts.","1500 drift points in one run",0,225,56,65,25,0,3},
 {"Mariner","An eager, light coastal roadster.","Buy for $1450",1450,205,62,80,12,0,0},
 {"Regent","A composed luxury touring sedan.","Buy for $1850",1850,220,47,90,38,0,0},
 {"Rallye","A grippy sport wagon with quick recovery.","Buy for $2200",2200,212,57,96,30,0,0},
 {"Tempest","An exotic built for the longest straights.","Buy for $4800",4800,275,72,83,10,0,0}
};
const LocationDef locations[LOCATION_COUNT] = {
 {"Metro Dawn","Glass towers, warm dawn and four open lanes.","Open from the start",0,0},
 {"Palm Coast","A tropical highway beside the sea.","Buy for $600",600,0},
 {"Skywater Causeway","An endless ribbon above blue water.","10 near misses in one run",0,1},
 {"Pine Ridge","Cool mountain air and forested shoulders.","Travel 6000 m in one run",0,2},
 {"Redrock Pass","Warm sandstone and sweeping canyon roads.","Earn 1500 drift points in one run",0,3},
 {"Marina Nights","Harbor lights and a glittering waterfront.","Buy for $1200",1200,0},
 {"Mangrove Run","Tropical wetlands and quiet green channels.","Reach career level 2",0,4},
 {"Citrus Parkway","A sunny route between rolling orchards.","Buy for $450",450,0}
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
typedef struct {
 uint32_t magic,version,checksum,cash,owned,unlocked_locations,paints;
 uint32_t best_score,best_distance,best_near,best_drift,best_speed_hold,best_wheelie,best_coins;
 uint32_t total_runs,total_distance,total_cash,seed;
 uint8_t selected_vehicle,selected_location;
 uint8_t paint[6],engine[6],grip[6],armor[6],reserved[2];
} LegacySave;
_Static_assert(sizeof(LegacySave)==100,"Version 1 saves must remain exactly 100 bytes");
_Static_assert(sizeof(Save)==204,"Version 2 saves must remain exactly 204 bytes");

static uint32_t checksum_bytes(const void *save,size_t size) {
 const uint8_t *p=(const uint8_t *)save; uint32_t h=2166136261u;
 for(size_t i=0;i<size;++i) {
  uint8_t b=(i>=offsetof(Save,checksum)&&i<offsetof(Save,checksum)+4)?0:p[i];
  h=(h^b)*16777619u;
 }
 return h;
}
static void default_settings(Save *s) {
 s->music_on=1;s->sfx_on=1;s->music_volume=70;s->sfx_volume=75;
 s->skin=0;s->shake=1;s->units=0;s->motion=1;
}
void game_make_save(const Game *g,Save *out) {
 *out=g->save; out->magic=SAVE_MAGIC; out->version=SAVE_VERSION;
 memset((uint8_t *)out+offsetof(Save,armor)+sizeof(out->armor),0,
        offsetof(Save,mastery_xp)-offsetof(Save,armor)-sizeof(out->armor));
 memset(out->reserved,0,sizeof(out->reserved));
 out->checksum=0; out->checksum=checksum_bytes(out,sizeof(*out));
}
static int valid_common(const Save *s,int count,int upgrade_max,int location_count) {
 if(s->cash>MONEY_LIMIT || s->total_cash>STAT_LIMIT || !s->seed) return 0;
 uint32_t mask=(1u<<count)-1;
 if(!(s->owned&1u) || (s->owned&~mask) || !(s->unlocked_locations&1u) || (s->unlocked_locations&~((1u<<location_count)-1))) return 0;
 if((s->paints&7u)!=7u || (s->paints&~63u)) return 0;
 if(s->selected_vehicle>=count || !(s->owned&(1u<<s->selected_vehicle))) return 0;
 if(s->selected_location>=location_count || !(s->unlocked_locations&(1u<<s->selected_location))) return 0;
 for(int i=0;i<count;++i) {
  if(s->engine[i]>upgrade_max || s->grip[i]>upgrade_max || s->armor[i]>upgrade_max || s->paint[i]>=PAINT_COUNT || !(s->paints&(1u<<s->paint[i]))) return 0;
 }
 if(s->best_score>STAT_LIMIT || s->best_distance>STAT_LIMIT || s->best_near>STAT_LIMIT ||
    s->best_drift>STAT_LIMIT || s->best_speed_hold>STAT_LIMIT || s->best_wheelie>STAT_LIMIT ||
    s->best_coins>STAT_LIMIT || s->total_runs>STAT_LIMIT || s->total_distance>STAT_LIMIT) return 0;
 return 1;
}
static void legacy_convert(const LegacySave *old,Save *out) {
 memset(out,0,sizeof(*out));
 /* The first eighteen 32-bit fields and both selection bytes are unchanged. */
 memcpy(out,old,offsetof(LegacySave,paint));
 for(int i=0;i<6;++i) {
  out->paint[i]=old->paint[i];out->engine[i]=old->engine[i];
  out->grip[i]=old->grip[i];out->armor[i]=old->armor[i];
 }
 out->version=SAVE_VERSION;out->checksum=0;default_settings(out);
}
static int read_legacy(const Save *raw,LegacySave *old,Save *converted) {
 memcpy(old,raw,sizeof(*old));
 if(old->magic!=SAVE_MAGIC || old->version!=1u || old->checksum!=checksum_bytes(old,sizeof(*old))) return 0;
 legacy_convert(old,converted);
 return valid_common(converted,6,3,5);
}
int game_save_valid(const Save *s) {
 if(!s) return 0;
 uint32_t header[2];memcpy(header,s,sizeof(header));
 if(header[0]!=SAVE_MAGIC) return 0;
 if(header[1]==1u) {LegacySave old;Save converted;return read_legacy(s,&old,&converted);}
 if(header[1]!=SAVE_VERSION || s->checksum!=checksum_bytes(s,sizeof(*s)) || !valid_common(s,VEHICLE_COUNT,UPGRADE_MAX,LOCATION_COUNT)) return 0;
 if(s->career_xp>STAT_LIMIT || s->music_on>1 || s->sfx_on>1 || s->music_volume>100 || s->sfx_volume>100 ||
    s->skin>=SKIN_COUNT || s->motion>2 || s->shake>1 || s->units>1) return 0;
 int spent=0;
 for(int i=0;i<PERK_COUNT;++i) {if(s->perks[i]>3)return 0;spent+=s->perks[i];}
 static const uint32_t career_thresholds[9]={200,600,1300,2400,4000,6500,10000,15000,22000};
 int level=0;while(level<9&&s->career_xp>=career_thresholds[level])++level;
 if(spent>level) return 0;
 for(int i=0;i<VEHICLE_COUNT;++i) {
  if(s->mastery_xp[i]>STAT_LIMIT || s->power_tune[i]>2 || s->handling_tune[i]>2 || s->chassis_tune[i]>2) return 0;
  if((s->power_tune[i]&&s->mastery_xp[i]<100) || (s->handling_tune[i]&&s->mastery_xp[i]<300) ||
     (s->chassis_tune[i]&&s->mastery_xp[i]<650)) return 0;
 }
 return 1;
}
int game_load(Game *g,const Save *save) {
 if(!game_save_valid(save)) return 0;
 uint32_t version;memcpy(&version,(const uint8_t *)save+4,sizeof(version));
 if(version==1u) {
  LegacySave old;Save converted;
  if(!read_legacy(save,&old,&converted)) return 0;
  converted.career_xp=converted.total_distance/5;
  converted.mastery_xp[converted.selected_vehicle]=converted.career_xp;
  for(int i=0;i<6;++i) if((converted.engine[i]>=3 || converted.grip[i]>=3 || converted.armor[i]>=3) && converted.mastery_xp[i]<100) converted.mastery_xp[i]=100;
  g->save=converted;g->save_requested=1;
 } else {g->save=*save;g->save_requested=0;}
 g->has_save=1; g->save_ok=1;
 g->mode=M_HOME; g->cursor=0; g->selection=g->save.selected_vehicle;
 g->vehicle=g->save.selected_vehicle; g->start_biome=g->save.selected_location;
 g->biome=g->start_biome; g->paint_choice=g->save.paint[g->selection];
 g->garage_tab=0;g->tune_choice=0;
 g->muted=(uint8_t)!g->save.music_on;
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
 default_settings(&g->save);
 g->save.paints=7; g->mode=M_HOME; g->health=100; g->combo_q8=Q;
 g->turbo_charge_q8=TURBO_MAX_Q8;
}

static int challenge_done(const Game *g,int id) {
 switch(id) {
  case 1:return g->save.best_near>=10;
  case 2:return g->save.best_distance>=6000;
  case 3:return g->save.best_drift>=1500;
  case 4:return game_career_level(g)>=2;
  default:return 1;
 }
}
int game_can_vehicle(const Game *g,int id) {
 return id>=0&&id<VEHICLE_COUNT && challenge_done(g,vehicles[id].unlock);
}
int game_can_location(const Game *g,int id) {
 return id>=0&&id<LOCATION_COUNT && challenge_done(g,locations[id].unlock);
}
static const uint32_t mastery_thresholds[7]={0,100,300,650,1200,2000,3200};
static const uint32_t career_thresholds[10]={0,200,600,1300,2400,4000,6500,10000,15000,22000};
int game_mastery_level(const Game *g,int vehicle) {
 if(vehicle<0 || vehicle>=VEHICLE_COUNT) return 0;
 int rank=0;while(rank<6&&g->save.mastery_xp[vehicle]>=mastery_thresholds[rank+1]) ++rank;
 return rank;
}
uint32_t game_mastery_next(const Game *g,int vehicle) {
 if(vehicle<0 || vehicle>=VEHICLE_COUNT)return 0;
 int rank=game_mastery_level(g,vehicle);
 return rank<6?mastery_thresholds[rank+1]:0;
}
int game_career_level(const Game *g) {
 int level=0;while(level<9&&g->save.career_xp>=career_thresholds[level+1]) ++level;
 return level;
}
uint32_t game_career_next(const Game *g) {
 int level=game_career_level(g);return level<9?career_thresholds[level+1]:0;
}
int game_perk_available(const Game *g) {
 int spent=0;for(int i=0;i<PERK_COUNT;++i)spent+=g->save.perks[i];
 int available=game_career_level(g)-spent;return available>0?available:0;
}
const char *game_tune_name(int kind,int value) {
 static const char *const names[3][3]={{"Stock","Sprint","Overdrive"},{"Balanced","Grip","Slide"},{"Standard","Lightweight","Reinforced"}};
 return kind>=0&&kind<3&&value>=0&&value<3?names[kind][value]:"";
}
const char *game_tune_description(int kind,int value) {
 static const char *const descriptions[3][3]={
  {"Factory power delivery","+12 acceleration / -12 top speed","+24 top speed / -10 acceleration"},
  {"Neutral steering and slide recovery","+12 grip / -5 top speed","-12 grip / +6 acceleration"},
  {"Factory weight and impact protection","+8 acceleration / -16 armor","+24 armor / -7 acceleration"}};
 return kind>=0&&kind<3&&value>=0&&value<3?descriptions[kind][value]:"";
}
static int vehicle_stat(const Game *g,int v,int kind,int preview) {
 if(v<0 || v>=VEHICLE_COUNT || kind<STAT_TOP || kind>STAT_ARMOR) return 0;
 int power=g->save.power_tune[v],handling=g->save.handling_tune[v],chassis=g->save.chassis_tune[v];
 if(preview && g->mode==M_GARAGE && g->garage_tab==2 && g->cursor>=1 && g->cursor<=3 && g->selection==v) {
  if(g->cursor==1)power=g->tune_choice;
  else if(g->cursor==2)handling=g->tune_choice;
  else chassis=g->tune_choice;
 }
 int top=vehicles[v].top_speed+g->save.engine[v]*12;
 int acceleration=vehicles[v].acceleration+g->save.engine[v]*5;
 int grip=vehicles[v].grip+g->save.grip[v]*7;
 int armor=vehicles[v].armor+g->save.armor[v]*20;
 if(power==1) {acceleration+=12;top-=12;} else if(power==2) {top+=24;acceleration-=10;}
 if(handling==1) {grip+=12;top-=5;} else if(handling==2) {grip-=12;acceleration+=6;}
 if(chassis==1) {acceleration+=8;armor-=16;} else if(chassis==2) {armor+=24;acceleration-=7;}
 return kind==STAT_TOP?clampi(top,100,400):kind==STAT_ACCEL?clampi(acceleration,15,160):
        kind==STAT_GRIP?clampi(grip,40,160):clampi(armor,0,220);
}
int game_vehicle_stat(const Game *g,int vehicle,int kind) {return vehicle_stat(g,vehicle,kind,0);}
int game_preview_stat(const Game *g,int vehicle,int kind) {return vehicle_stat(g,vehicle,kind,1);}
int game_top_speed(const Game *g) {
 int v=g->vehicle<VEHICLE_COUNT?g->vehicle:0;
 int creep=(int)(g->run.elapsed_ticks/(30*DRIVE_HZ))*2;
 return game_vehicle_stat(g,v,STAT_TOP) + clampi(creep,0,50);
}
static int upgrade_level(const Game *g,int kind) {
 if(kind<0 || kind>2 || g->selection>=VEHICLE_COUNT)return 0;
 int v=g->selection;return kind==0?g->save.engine[v]:kind==1?g->save.grip[v]:g->save.armor[v];
}
int game_upgrade_cost(const Game *g,int kind) {
 if(kind<0 || kind>2 || g->selection>=VEHICLE_COUNT) return 0;
 static const uint16_t costs[UPGRADE_MAX]={60,95,145,215,310,440};
 int level=upgrade_level(g,kind);
 if(level>=UPGRADE_MAX) return 0;
 return costs[level]*(kind==0?100:kind==1?90:95)/100;
}
int game_upgrade_rank(const Game *g,int kind) {
 int next=upgrade_level(g,kind)+1;return next<=2?0:clampi(next-2,1,4);
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
 if(game_career_level(g)>=2)g->save.unlocked_locations|=1u<<6;
}
static void run_cash(Game *g) {
 uint32_t coin_cash=g->run.count[B_COIN]>MONEY_LIMIT/5?MONEY_LIMIT:g->run.count[B_COIN]*5;
 uint32_t base=add_sat(coin_cash,g->run.total_points/60,MONEY_LIMIT);
 uint32_t bonus=base*(uint32_t)(8*g->save.perks[2])/100;
 if(bonus>MONEY_LIMIT-base)bonus=MONEY_LIMIT-base;
 g->run.cash_bonus=bonus;g->run.total_cash=base+bonus;
}
static int turbo_clean(const Game *g) {
 return g->mode==M_RUN && !g->turbo && !g->crash_timer && !g->damage_cooldown &&
        g->speed_q8>0 && absi(g->x_q8)<80*Q;
}
static int turbo_charge_factor(const Game *g) {
 /* Five times the score combo doubles fuel income rather than multiplying
  * it by five. The tank remains a resource even during long skill streaks. */
 return Q+(clampi(g->combo_q8,Q,COMBO_MAX_Q8)-Q)/4;
}
static void turbo_add(Game *g,int amount,int source) {
 if(amount<=0 || g->turbo_charge_q8>=TURBO_MAX_Q8) return;
 int fuel=g->turbo_charge_q8+amount;
 g->turbo_charge_q8=(uint16_t)(fuel>TURBO_MAX_Q8?TURBO_MAX_Q8:fuel);
 g->turbo_recharging=1;
 /* Keep a just-earned near miss visible when a continuous source also pays. */
 if(g->turbo_source!=TS_NEAR)g->turbo_source=(uint8_t)source;
 if(g->turbo_charge_q8==TURBO_MAX_Q8)g->turbo_remainder=0;
}
static void turbo_near(Game *g,uint32_t count) {
 if(!turbo_clean(g))return;
 int amount=TURBO_MAX_Q8/10*turbo_charge_factor(g)/Q;
 if(count>10)count=10; /* A callback cannot overflow even with malformed input. */
 turbo_add(g,amount*(int)count,TS_NEAR);
}
static void turbo_recharge(Game *g) {
 if(!turbo_clean(g))return;
 if(g->turbo_charge_q8>=TURBO_MAX_Q8) {g->turbo_remainder=0;return;}
 int percent=0,source=TS_NONE,factor=turbo_charge_factor(g);
 /* A wall-bound handbrake or an unmoving steering sprite is not a slide. */
 if(g->drifting && g->drift_hold>=(3*DRIVE_HZ+3)/4 && absi(g->vx_q8)>Q/2) {
  percent=4;source=TS_DRIFT;
 } else if(g->wheelie && g->wheelie_hold>=DRIVE_HZ) {
  percent=3;source=TS_WHEELIE;
 } else if(g->slow && g->speed_q8>40*Q) {
  percent=2;source=TS_SLOW;factor=Q;
 }
 if(!percent)return;
 /* MAX is a whole number of Q units. Keep the remainder over a denominator
  * of 3000 so all three rates are exact over time without floating point. */
 int numerator=(TURBO_MAX_Q8/Q)*percent*factor+g->turbo_remainder;
 int amount=numerator/(100*DRIVE_HZ);
 g->turbo_remainder=(uint16_t)(numerator%(100*DRIVE_HZ));
 turbo_add(g,amount,source);
}
static void points(Game *g,int category,uint32_t count,uint32_t amount) {
 /* Keep the receipt sum exact even at the deliberately remote score cap. */
 uint32_t room=STAT_LIMIT-g->run.total_points;
 if(amount>room) amount=room;
 g->run.count[category]=add_sat(g->run.count[category],count,STAT_LIMIT);
 g->run.points[category]+=amount; g->run.total_points+=amount;
 run_cash(g);
 if(category==B_NEAR)turbo_near(g,count);
}
static int combo_for(const Game *g,uint32_t hold) {
 uint32_t window=(uint32_t)(8*DRIVE_HZ*(100-15*g->save.perks[1])/100);
 uint32_t capped=hold>window?window:hold;
 return Q+(int)(capped*4*Q/window);
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
 g->turbo_charge_q8=TURBO_MAX_Q8;
 g->turbo_latched=g->turbo_recharging=g->turbo_source=0;g->turbo_remainder=0;
 g->repair_used=0;
 g->mode=M_RUN; g->cursor=0; g->next_spawn_q8=200*Q; traffic_spawn_ahead(g);
}
void game_finish_run(Game *g) {
 if(g->run.banked) { g->mode=M_RECEIPT; return; }
 g->run.banked=1;
 run_cash(g);
 uint32_t skill=g->run.points[B_NEAR]+g->run.points[B_SPEED]+g->run.points[B_DRIFT]+g->run.points[B_WHEELIE];
 uint32_t xp=g->run.distance_m/5+skill/10;
 if(xp>20000)xp=20000;
 g->run.mastery_xp=g->run.career_xp=xp;
 g->save.mastery_xp[g->vehicle]=add_sat(g->save.mastery_xp[g->vehicle],xp,STAT_LIMIT);
 g->save.career_xp=add_sat(g->save.career_xp,xp,STAT_LIMIT);
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
 g->turbo_recharging=g->turbo_source=0;
}

static void damage(Game *g,int raw,int rail) {
 if(g->damage_cooldown || g->run_ticks<2*DRIVE_HZ) return;
 int armor=game_vehicle_stat(g,g->vehicle,STAT_ARMOR)/4;
 int hurt=raw-armor; if(hurt<5) hurt=5;
 g->health=(uint8_t)(g->health>hurt?g->health-hurt:0);
 if(g->health<=25 && !g->repair_used && g->save.perks[0]) {
  g->health=(uint8_t)(g->health+2+8*g->save.perks[0]);
  g->repair_used=1;notice(g,"Recovery: emergency repair deployed.");
 }
 g->damage_cooldown=rail?30:38; g->crash_timer=12; g->shake=10;
 g->speed_q8=g->speed_q8*(rail?88:62)/100;
 g->speed_hold=g->drift_hold=g->wheelie_hold=0; g->combo_q8=Q;
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
 int best=combo_for(g,g->speed_hold),c=combo_for(g,g->drift_hold);
 if(c>best) best=c;
 c=combo_for(g,g->wheelie_hold);
 if(c>best) best=c;
 g->combo_q8=(uint16_t)best;
 if(g->combo_q8>g->run.max_combo_q8) g->run.max_combo_q8=g->combo_q8;
 if(g->speed_hold && g->speed_hold%DRIVE_HZ==0) points(g,B_SPEED,1,(uint32_t)(20*combo_for(g,g->speed_hold)/Q));
 if(g->drift_hold && g->drift_hold%DRIVE_HZ==0) {
  uint32_t p=(uint32_t)(35*combo_for(g,g->drift_hold)/Q); points(g,B_DRIFT,1,p);
  g->run.drift_points=add_sat(g->run.drift_points,p,STAT_LIMIT);
 }
 if(g->wheelie_hold && g->wheelie_hold%DRIVE_HZ==0) {
  uint32_t p=(uint32_t)(35*combo_for(g,g->wheelie_hold)/Q); points(g,B_WHEELIE,1,p);
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
 g->turbo_recharging=g->turbo_source=0;
 g->slow=(uint8_t)(gas && (in->held&K_DOWN));
 if(!(in->held&K_UP))g->turbo_latched=0;
 int turbo_requested=gas && !g->slow && (in->held&K_UP) && !g->turbo_latched;
 g->turbo=(uint8_t)(turbo_requested && g->turbo_charge_q8 &&
                    (g->turbo || g->turbo_charge_q8>=TURBO_START_Q8));
 if(g->turbo) {
  if(g->turbo_charge_q8<=Q) {g->turbo_charge_q8=0;g->turbo_latched=1;}
  else g->turbo_charge_q8-=Q;
  g->turbo_remainder=0;
 }
 uint16_t fuel_before_recharge=g->turbo_charge_q8;
 int base=game_top_speed(g),target=g->slow?base*62/100:g->turbo?base*125/100:base;
 int32_t oldspeed=g->speed_q8;
 int acceleration=game_vehicle_stat(g,v,STAT_ACCEL);
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
 int grip=game_vehicle_stat(g,v,STAT_GRIP);
 /* Grip changes both turn-in and recovery from a slide at every upgrade.
    The handbrake keeps a much lower gain so sideways momentum survives. */
 int gain=hand?18+grip/4:clampi(2*grip-55,48,248);
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
 traffic_tick(g,old_position,damage,points); pickup_tick(g,old_position); traffic_spawn_ahead(g);
 g->accel_q8=(g->speed_q8-oldspeed)*DRIVE_HZ;
 if(g->speed_q8>0 && (uint32_t)(g->speed_q8/Q)>g->run.max_speed) g->run.max_speed=(uint32_t)(g->speed_q8/Q);
 bonus_tick(g,target,motion);
 /* A near miss earlier in this update cannot pay fuel if another NPC hits
  * the player later in the same update. Scores retain their existing rules. */
 if(g->crash_timer || g->damage_cooldown) {
  g->turbo_charge_q8=fuel_before_recharge;g->turbo_recharging=g->turbo_source=0;
 } else turbo_recharge(g);
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
 if(!cost) { notice(g,"This upgrade is already at level 6."); return; }
 if(game_mastery_level(g,v)<game_upgrade_rank(g,kind)) {notice(g,"Drive this vehicle to raise its mastery rank.");return;}
 if(g->save.cash<(uint32_t)cost) { notice(g,"Not enough cash for this upgrade."); return; }
 g->save.cash-=(uint32_t)cost;
 uint8_t *level=kind==0?g->save.engine:kind==1?g->save.grip:g->save.armor;
 ++level[v]; g->save_requested=1; notice(g,"Upgrade installed.");
}
static uint8_t *tune_array(Game *g,int kind) {
 return kind==0?g->save.power_tune:kind==1?g->save.handling_tune:g->save.chassis_tune;
}
static void refresh_tune(Game *g) {
 g->tune_choice=g->cursor>=1&&g->cursor<=3?tune_array(g,g->cursor-1)[g->selection]:0;
}
static void apply_paint(Game *g) {
 if(!(g->save.owned&(1u<<g->selection))) notice(g,"Own this vehicle before painting it.");
 else if(!(g->save.paints&(1u<<g->paint_choice))) notice(g,g->paint_choice==3?"Hold full speed for 8 seconds to unlock.":g->paint_choice==4?"Earn 1000 wheelie points in one run to unlock.":"Earn 1500 drift points in one run to unlock.");
 else {g->save.paint[g->selection]=g->paint_choice;g->save_requested=1;notice(g,"Fresh paint applied.");}
}
static void garage_tick(Game *g,const Input *in) {
 if(in->pressed&K_B) {g->mode=M_HOME;g->cursor=1;return;}
 int tab=((in->pressed&K_R)?1:0)-((in->pressed&K_L)?1:0);
 if(tab) {g->garage_tab=(uint8_t)((g->garage_tab+3+tab)%3);g->cursor=0;refresh_tune(g);}
 int dy=vertical(in),dx=horizontal(in),rows=g->garage_tab==0?3:4;
 int previous=g->cursor;
 g->cursor=(uint8_t)((g->cursor+rows+dy)%rows);
 if(g->garage_tab==2&&previous!=g->cursor)refresh_tune(g);
 if(g->cursor==0&&dx) {
  g->selection=(uint8_t)((g->selection+VEHICLE_COUNT+dx)%VEHICLE_COUNT);
  g->paint_choice=g->save.paint[g->selection];refresh_tune(g);
 }
 if(g->garage_tab==0&&g->cursor==2&&dx) g->paint_choice=(uint8_t)((g->paint_choice+PAINT_COUNT+dx)%PAINT_COUNT);
 if(g->garage_tab==2&&g->cursor>=1&&dx)g->tune_choice=(uint8_t)((g->tune_choice+3+dx)%3);
 if(!(in->pressed&K_A))return;
 if(g->cursor==0) {g->cursor=1;refresh_tune(g);return;}
 if(g->garage_tab==0) {if(g->cursor==1)purchase_vehicle(g);else apply_paint(g);}
 else if(g->garage_tab==1)upgrade(g,g->cursor-1);
 else {
  int kind=g->cursor-1;
  if(!(g->save.owned&(1u<<g->selection)))notice(g,"Own this vehicle before changing its setup.");
  else if(g->tune_choice&&game_mastery_level(g,g->selection)<kind+1)notice(g,kind==0?"Power setups unlock at mastery rank 1.":kind==1?"Handling setups unlock at mastery rank 2.":"Chassis setups unlock at mastery rank 3.");
  else {tune_array(g,kind)[g->selection]=g->tune_choice;g->save_requested=1;notice(g,"Setup applied. Changes are always free.");}
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
static void career_tick(Game *g,const Input *in) {
 if(in->pressed&K_B) {g->mode=M_HOME;g->cursor=4;return;}
 g->cursor=(uint8_t)((g->cursor+4+vertical(in))%4);
 if(!(in->pressed&K_A))return;
 if(g->cursor==3) {memset(g->save.perks,0,sizeof(g->save.perks));g->save_requested=1;notice(g,"Perks reset. All points returned.");}
 else if(g->save.perks[g->cursor]>=3)notice(g,"This perk is already at rank 3.");
 else if(!game_perk_available(g))notice(g,"Reach the next career level for a perk point.");
 else {++g->save.perks[g->cursor];g->save_requested=1;notice(g,"Career perk assigned.");}
}
static void settings_tick(Game *g,const Input *in) {
 if(in->pressed&K_B) {g->mode=g->menu_return==M_PAUSE?M_PAUSE:M_HOME;g->cursor=g->mode==M_PAUSE?3:5;return;}
 g->cursor=(uint8_t)((g->cursor+9+vertical(in))%9);
 int dx=horizontal(in),confirm=(in->pressed&K_A)!=0;
 if(g->cursor==8) {if(confirm){g->mode=M_CONTROLS;g->paused_from=M_SETTINGS;g->controls_page=0;}return;}
 if(!dx&&!confirm)return;
 uint8_t *value;
 int choices=2;
 switch(g->cursor) {
  case 0:value=&g->save.music_on;break;
  case 1:value=&g->save.music_volume;break;
  case 2:value=&g->save.sfx_on;break;
  case 3:value=&g->save.sfx_volume;break;
  case 4:value=&g->save.skin;choices=SKIN_COUNT;break;
  case 5:value=&g->save.motion;choices=3;break;
  case 6:value=&g->save.shake;break;
  default:value=&g->save.units;break;
 }
 if(g->cursor==1||g->cursor==3) {
  if(confirm)*value=(uint8_t)(*value>=100?0:clampi(*value+5,0,100));
  else *value=(uint8_t)clampi(*value+dx*5,0,100);
 } else *value=(uint8_t)((*value+choices+(dx?dx:1))%choices);
 g->muted=(uint8_t)!g->save.music_on;g->save_requested=1;
}
void game_update(Game *g,const Input *in) {
 ++g->frame;if(g->notice_timer) --g->notice_timer;
 switch(g->mode) {
  case M_RUN:run_tick(g,in);break;
  case M_RECEIPT:
   if(in->pressed&K_A) {g->mode=M_HOME;g->cursor=0;}
   break;
  case M_HOME:
   g->cursor=(uint8_t)((g->cursor+6+vertical(in))%6);
   if(in->pressed&K_A) {
    switch(g->cursor) {
     case 0:game_begin_run(g);break;
     case 1:g->mode=M_GARAGE;g->cursor=0;g->garage_tab=0;g->selection=g->save.selected_vehicle;g->paint_choice=g->save.paint[g->selection];break;
     case 2:g->mode=M_DEPARTURES;g->cursor=g->save.selected_location;break;
     case 3:g->mode=M_CHALLENGES;g->cursor=0;break;
     case 4:g->mode=M_CAREER;g->cursor=0;break;
     case 5:g->mode=M_SETTINGS;g->cursor=0;g->menu_return=M_HOME;break;
    }
   }
   break;
  case M_GARAGE:garage_tick(g,in);break;
  case M_DEPARTURES:departure_tick(g,in);break;
  case M_CAREER:career_tick(g,in);break;
  case M_SETTINGS:settings_tick(g,in);break;
  case M_CHALLENGES:
   g->cursor=(uint8_t)((g->cursor+5+vertical(in))%5);
   if(in->pressed&(K_A|K_B)) {g->mode=M_HOME;g->cursor=3;}
   break;
  case M_CONTROLS:
   if(in->pressed&(K_L|K_R))g->controls_page^=1;
   if(in->pressed&(K_A|K_B)) {
    g->mode=g->paused_from==M_PAUSE?M_PAUSE:g->paused_from==M_SETTINGS?M_SETTINGS:M_HOME;
    g->cursor=g->mode==M_PAUSE?2:g->mode==M_SETTINGS?8:5;
   }
   break;
  case M_PAUSE:
   if(in->pressed&(K_B|K_START)) {g->mode=M_RUN;break;}
   g->cursor=(uint8_t)((g->cursor+4+vertical(in))%4);
   if(in->pressed&K_A) {
    if(g->cursor==0) g->mode=M_RUN;
    else if(g->cursor==1) game_finish_run(g);
    else if(g->cursor==2) {g->mode=M_CONTROLS;g->paused_from=M_PAUSE;g->controls_page=0;}
    else {g->mode=M_SETTINGS;g->cursor=0;g->menu_return=M_PAUSE;}
   }
   break;
  default:g->mode=M_HOME;g->cursor=0;break;
 }
}
