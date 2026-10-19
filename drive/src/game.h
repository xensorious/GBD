#ifndef COASTLINE_GAME_H
#define COASTLINE_GAME_H
#include <stdint.h>

#define DRIVE_HZ 30
#define Q 256
#define VEHICLE_COUNT 31
#define UPGRADE_MAX 6
#define PERK_COUNT 3
#define SKIN_COUNT 4
#define LOCATION_COUNT 13
#define SAVE_DISK_SIZE 500
#define CLASS_COUNT 5
#define LIVERY_COUNT 5
#define WHEEL_COUNT 4
#define CONTRACT_OFFERS 3
#define SMOKE_COUNT 5
#define PRESET_COUNT 3
#define TRAFFIC_COUNT 20
#define PICKUP_COUNT 24
#define BONUS_COUNT 7
#define CHALLENGE_COUNT 8
#define ROAD_HALF 96
#define ROAD_VIEW 1200
#define COMBO_MAX_Q8 (5*Q)
#define COMBO_STEP_Q8 (Q/4)
#define SPEED_COMBO_TICKS (48*DRIVE_HZ)
#define SKILL_COMBO_TICKS (24*DRIVE_HZ)
#define POLICE_GRACE_TICKS (3*DRIVE_HZ)
#define POLICE_CATCH_TICKS (3*DRIVE_HZ)
/* One Q unit is one update of boost: a full tank lasts eight seconds. */
#define TURBO_MAX_Q8 (8*DRIVE_HZ*Q)
#define TURBO_START_Q8 (TURBO_MAX_Q8/10)

enum DriveKey { K_UP=1u, K_DOWN=2u, K_LEFT=4u, K_RIGHT=8u,
 K_A=16u,K_B=32u,K_L=64u,K_R=128u,K_START=256u,K_SELECT=512u };
typedef struct { uint32_t held,pressed; } Input;
enum GameMode { M_HOME,M_RUN,M_RECEIPT,M_GARAGE,M_DEPARTURES,M_CHALLENGES,M_CONTROLS,M_PAUSE,M_CAREER,M_SETTINGS,M_CONTRACTS };
enum InterfaceSkin { SKIN_APEX,SKIN_SIGNAL,SKIN_MIDNIGHT,SKIN_FUSION };
enum VehicleStat { STAT_TOP,STAT_ACCEL,STAT_GRIP,STAT_ARMOR };
enum Bonus { B_DISTANCE,B_SURVIVAL,B_NEAR,B_SPEED,B_DRIFT,B_WHEELIE,B_COIN };
enum TurboSource { TS_NONE,TS_NEAR,TS_DRIFT,TS_WHEELIE,TS_SLOW };
enum RunEnd { END_NONE,END_STOPPED,END_CRASH,END_BANKED,END_LIMIT };
enum Biome { CITY,COAST,CAUSEWAY,PINES,CANYON,MARINA,WETLANDS,GROVES,NEON,JUNGLE,SWAMP,AURORA,SALT };
enum VehicleClass { CLASS_D,CLASS_C,CLASS_B,CLASS_A,CLASS_S };
enum VehicleSpecialty { VS_VALUE,VS_BALANCED,VS_TOP,VS_ACCEL,VS_ARMOR };
enum DispatchKind { DC_NEAR,DC_DRIFT,DC_DISTANCE,DC_COINS,DC_SPEED,DC_NONE };
typedef struct {
 const char *name,*description,*requirement;
 uint16_t price,top_speed,acceleration,grip,armor;
 uint8_t bike,unlock,class_id,specialty;
} VehicleDef;
typedef struct {uint32_t goal,reward;uint8_t kind,reserved[3];} DispatchContract;
typedef struct { const char *name,*description,*requirement;uint16_t price;uint8_t unlock; } LocationDef;
extern const VehicleDef vehicles[VEHICLE_COUNT];
extern const LocationDef locations[LOCATION_COUNT];
extern const char *const bonus_names[BONUS_COUNT];

/* Unpacked profile view. The cartridge codec packs it into 500 bytes.
 * No pointers or native enums are persisted. */
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
 uint8_t recovery[VEHICLE_COUNT],livery[VEHICLE_COUNT],wheels[VEHICLE_COUNT];
 uint8_t livery_owned[VEHICLE_COUNT],wheels_owned[VEHICLE_COUNT],dispatch_choice;
 uint32_t contracts_completed;
 uint8_t reserved[4];
 uint8_t paint_seen[VEHICLE_COUNT],smoke[VEHICLE_COUNT],smoke_seen[VEHICLE_COUNT];
 uint32_t presets[PRESET_COUNT];
 uint8_t preset_mask;
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
 /* Pursuit is run-only. The ticket always credits the earned run rewards. */
 uint16_t police_ticks;
 uint8_t police_pressure,end_reason;
 uint8_t garage_filter,livery_choice,wheel_choice,contract_complete;
 DispatchContract active_contract;
 uint32_t contract_progress,contract_cash;
 uint8_t smoke_choice,build_choice,preset_choice;
} Game;

void game_init(Game *g,uint32_t seed);
void game_update(Game *g,const Input *in);
void game_begin_run(Game *g);
void game_finish_run(Game *g);
/* Canonical unpacked helpers for rules/inspection; byte codecs handle disk. */
void game_make_save(const Game *g,Save *out);
int game_load(Game *g,const Save *save);
int game_encode_save(const Game *g,void *out,unsigned size);
int game_load_bytes(Game *g,const void *raw,unsigned size);
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
int game_owned_count(const Game *g);
int game_vehicle_class(int id);
const char *game_class_name(int class_id);
const char *game_specialty_name(int specialty);
int game_class_requirement(int class_id);
int game_class_unlocked(const Game *g,int class_id);
const char *game_vehicle_lock_reason(const Game *g,int id);
const char *game_livery_name(int value);
const char *game_wheel_name(int value);
int game_cosmetic_cost(const Game *g,int kind,int value);
int game_apply_cosmetics(Game *g);
const char *game_smoke_name(int value);
const char *game_build_name(int value);
int game_build_choice(const Game *g,int vehicle);
int game_preset_cost(const Game *g,int slot);
int game_dealer_count(int class_id);
int game_dealer_vehicle(int class_id,int row);
void game_contract_offer(const Game *g,int slot,DispatchContract *out);
const char *game_contract_name(int kind);
uint32_t game_contract_progress(const Game *g);
int game_can_location(const Game *g,int id);
const char *game_challenge_name(int i);
const char *game_challenge_reward(int i);
int game_challenge_progress(const Game *g,int i,int *goal);
#endif
