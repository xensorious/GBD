#ifndef COASTLINE_GAME_H
#define COASTLINE_GAME_H
#include <stdint.h>

#define DRIVE_HZ 30
#define Q 256
#define VEHICLE_COUNT 10
#define UPGRADE_MAX 6
#define PERK_COUNT 3
#define SKIN_COUNT 3
#define LOCATION_COUNT 8
#define TRAFFIC_COUNT 20
#define PICKUP_COUNT 24
#define BONUS_COUNT 7
#define ROAD_HALF 96
#define ROAD_VIEW 1200
#define COMBO_MAX_Q8 (5*Q)
/* One Q unit is one update of boost: a full tank lasts eight seconds. */
#define TURBO_MAX_Q8 (8*DRIVE_HZ*Q)
#define TURBO_START_Q8 (TURBO_MAX_Q8/10)

enum DriveKey { K_UP=1u, K_DOWN=2u, K_LEFT=4u, K_RIGHT=8u,
 K_A=16u,K_B=32u,K_L=64u,K_R=128u,K_START=256u,K_SELECT=512u };
typedef struct { uint32_t held,pressed; } Input;
enum GameMode { M_HOME,M_RUN,M_RECEIPT,M_GARAGE,M_DEPARTURES,M_CHALLENGES,M_CONTROLS,M_PAUSE,M_CAREER,M_SETTINGS };
enum VehicleStat { STAT_TOP,STAT_ACCEL,STAT_GRIP,STAT_ARMOR };
enum Bonus { B_DISTANCE,B_SURVIVAL,B_NEAR,B_SPEED,B_DRIFT,B_WHEELIE,B_COIN };
enum TurboSource { TS_NONE,TS_NEAR,TS_DRIFT,TS_WHEELIE,TS_SLOW };
enum Biome { CITY,COAST,CAUSEWAY,PINES,CANYON,MARINA,WETLANDS,GROVES };
typedef struct {
 const char *name,*description,*requirement;
 uint16_t price,top_speed,acceleration,grip,armor;
 uint8_t bike,unlock;
} VehicleDef;
typedef struct { const char *name,*description,*requirement;uint16_t price;uint8_t unlock; } LocationDef;
extern const VehicleDef vehicles[VEHICLE_COUNT];
extern const LocationDef locations[LOCATION_COUNT];
extern const char *const bonus_names[BONUS_COUNT];

/* Fixed-width cartridge save. No pointers or native enums are persisted. */
typedef struct {
 uint32_t magic,version,checksum,cash,owned,unlocked_locations,paints;
 uint32_t best_score,best_distance,best_near,best_drift,best_speed_hold,best_wheelie,best_coins;
 uint32_t total_runs,total_distance,total_cash,seed;
 uint8_t selected_vehicle,selected_location;
 uint8_t paint[VEHICLE_COUNT],engine[VEHICLE_COUNT],grip[VEHICLE_COUNT],armor[VEHICLE_COUNT];
 uint32_t mastery_xp[VEHICLE_COUNT],career_xp;
 uint8_t power_tune[VEHICLE_COUNT],handling_tune[VEHICLE_COUNT],chassis_tune[VEHICLE_COUNT];
 uint8_t perks[PERK_COUNT];
 uint8_t music_on,sfx_on,music_volume,sfx_volume,skin,shake,units,motion;
 uint8_t reserved[3];
} Save;
typedef struct {
 uint32_t count[BONUS_COUNT],points[BONUS_COUNT];
 uint32_t total_points,total_cash,distance_m,elapsed_ticks,max_speed,max_combo_q8;
 uint32_t best_speed_hold,drift_points,wheelie_points;
 uint32_t mastery_xp,career_xp,cash_bonus;
 uint8_t banked;
} RunStats;
typedef struct {
 int32_t x_q8,z_q8,speed_q8,cruise_q8;
 uint16_t serial;
 uint8_t kind,paint,active,near_awarded,lane,target_lane;
} Traffic;
typedef struct {int32_t x_q8,z_q8;uint16_t serial;uint8_t active,reserved;} Pickup;
typedef struct {
 Save save;
 RunStats run;
 Traffic traffic[TRAFFIC_COUNT];
 Pickup pickups[PICKUP_COUNT];
 uint32_t frame,run_seed,run_ticks;
 int32_t position_q8,furthest_q8,odometer_q8,x_q8,vx_q8,speed_q8,accel_q8;
 int32_t next_spawn_q8;
 uint32_t speed_hold,drift_hold,wheelie_hold;
 uint16_t combo_q8,damage_cooldown,crash_timer,horn_timer,shake,spawn_serial;
 int16_t steering,yaw;
 uint8_t mode,cursor,selection,health,vehicle,start_biome,biome,turbo,slow,drifting,wheelie;
 uint8_t save_requested,save_ok,has_save,muted,paused_from;
 uint16_t notice_timer;
 char notice[88];
 uint8_t paint_choice,garage_tab,menu_return,repair_used,tune_choice;
 /* Run-only fuel never changes the versioned battery-save layout. */
 uint16_t turbo_charge_q8;
 uint8_t turbo_latched,turbo_recharging,turbo_source,controls_page;
 uint16_t turbo_remainder;
} Game;

void game_init(Game *g,uint32_t seed);
void game_update(Game *g,const Input *in);
void game_begin_run(Game *g);
void game_finish_run(Game *g);
void game_make_save(const Game *g,Save *out);
int game_load(Game *g,const Save *save);
int game_save_valid(const Save *save);
void game_save_result(Game *g,int okay);
int game_top_speed(const Game *g);
int game_upgrade_cost(const Game *g,int kind);
int game_upgrade_rank(const Game *g,int kind);
int game_vehicle_stat(const Game *g,int vehicle,int kind);
int game_preview_stat(const Game *g,int vehicle,int kind);
int game_mastery_level(const Game *g,int vehicle);
uint32_t game_mastery_next(const Game *g,int vehicle);
int game_career_level(const Game *g);
uint32_t game_career_next(const Game *g);
int game_perk_available(const Game *g);
const char *game_tune_name(int kind,int value);
const char *game_tune_description(int kind,int value);
int game_can_vehicle(const Game *g,int id);
int game_can_location(const Game *g,int id);
const char *game_challenge_name(int i);
const char *game_challenge_reward(int i);
int game_challenge_progress(const Game *g,int i,int *goal);
#endif
