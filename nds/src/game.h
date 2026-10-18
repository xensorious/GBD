#ifndef ASTERWILD_GAME_H
#define ASTERWILD_GAME_H

#include <stdint.h>

#define GAME_WORLD_W 1024
#define GAME_WORLD_H 768
#define GAME_PARTY_MAX 6
#define GAME_SPECIES_COUNT 6
#define GAME_WILD_COUNT 6
#define GAME_NPC_COUNT 4
#define GAME_DIALOG_MAX 8
#define GAME_LOG_MAX 8
#define GAME_TILE_SIZE 16
#define GAME_WALK_SPEED 4
#define GAME_RUN_SPEED 8
#define GAME_MAP_REVISION 2

enum GameKey {
    GK_UP=1u<<0, GK_DOWN=1u<<1, GK_LEFT=1u<<2, GK_RIGHT=1u<<3,
    GK_A=1u<<4, GK_B=1u<<5, GK_START=1u<<6, GK_SELECT=1u<<7,
    GK_X=1u<<8, GK_Y=1u<<9, GK_L=1u<<10, GK_R=1u<<11
};
typedef struct {
    uint32_t held, pressed;
    int touch_pressed, touch_x, touch_y;
} GameInput;

typedef enum {
    GM_TITLE, GM_FIELD, GM_DIALOG, GM_STARTER, GM_BATTLE,
    GM_MENU, GM_PARTY, GM_JOURNAL, GM_GUIDE, GM_GROWTH, GM_ENDING
} GameMode;
typedef enum { TYPE_GROVE, TYPE_EMBER, TYPE_TIDE, TYPE_STONE, TYPE_SPARK } Element;
typedef enum { NPC_MIRA, NPC_JUN, NPC_EDDA, NPC_VALE } NpcId;
typedef enum { BP_COMMAND, BP_TEXT, BP_DONE } BattlePhase;
typedef enum { BI_QUICK, BI_HEAVY, BI_REST } BattleIntent;
typedef enum { BR_NONE, BR_WIN, BR_RECRUIT, BR_LOSS, BR_RETREAT } BattleResult;
typedef enum {
    GA_ACCEPT, GA_BACK, GA_INTERACT, GA_MENU, GA_PARTY, GA_JOURNAL,
    GA_GUIDE, GA_SAVE, GA_REST, GA_BATTLE_MOVE, GA_CHOOSE_STARTER,
    GA_SELECT_PARTY, GA_CHOOSE_GROWTH, GA_SET_CURSOR
} GameAction;

enum ProgressFlag {
    PF_STARTED=1u<<0, PF_CHOSEN=1u<<1, PF_MET_JUN=1u<<2,
    PF_GARDEN_QUEST=1u<<3, PF_GARDEN_DONE=1u<<4,
    PF_MET_VALE=1u<<5, PF_TRIAL_WON=1u<<6,
    PF_FIRST_BATTLE=1u<<7, PF_FIRST_FRIEND=1u<<8
};

typedef struct { int16_t x,y; } WorldPoint;
typedef struct { int16_t x,y,w,h; } WorldSolid;
typedef struct {
    const WorldSolid *solids;
    uint16_t solid_count;
    WorldPoint spawn, camp;
    WorldPoint npcs[GAME_NPC_COUNT];
    WorldPoint wilds[GAME_WILD_COUNT];
    WorldPoint flowers[3];
} WorldDef;

typedef struct {
    const char *name, *epithet, *description, *skill, *skill_description;
    uint8_t element, base_hp, power, defense;
} Species;
extern const Species game_species[GAME_SPECIES_COUNT];
extern const char *const game_element_names[5];

typedef struct {
    uint8_t species, level, xp, path;
    uint16_t hp;
    uint8_t energy, reserved;
} Companion;

/* Fixed-format progress data: no pointers or platform-dependent integers. */
typedef struct {
    uint32_t magic, version, checksum, flags;
    uint32_t seed;
    int16_t player_x, player_y;
    uint8_t facing, party_count, lead, starter;
    uint8_t tonics, flowers, seen, recruited;
    uint8_t wild_taken, reserved[3];
    uint16_t battles_won, steps;
    Companion party[GAME_PARTY_MAX];
} GameSave;

typedef struct {
    char speaker[24];
    char lines[GAME_DIALOG_MAX][112];
    uint8_t count, index, after;
} Dialogue;

typedef struct {
    uint8_t species, level, is_trial, encounter;
    uint16_t hp, max_hp;
    uint8_t turn, intent, phase, result;
    uint8_t calm, guarded, active, log_count, log_index;
    uint8_t flash_enemy, flash_player, effect, effect_timer;
    char logs[GAME_LOG_MAX][112];
} Battle;

typedef struct {
    GameSave save;
    const WorldDef *world;
    GameMode mode, return_mode;
    Dialogue dialog;
    Battle battle;
    uint32_t frame;
    uint16_t wild_cooldown[GAME_WILD_COUNT];
    uint8_t cursor, facing, walking, anim_tick;
    uint8_t menu_from_battle, growth_slot, pending_growth;
    uint8_t save_requested, save_feedback, has_save, title_cursor;
    uint8_t ui_focus, ui_tab; /* Focused bottom screen: Map, Party, Journal, Guide, System. */
    uint8_t new_confirm; /* Title: a second A is required before replacing an existing journey. */
    uint16_t toast_timer, interaction_cooldown;
    char toast[80];
    /* Runtime movement is deliberately outside the fixed 88-byte save format.
     * Coordinates are the interpolated feet; settled tiles are 8 mod 16.
     * Directions: down, left, right, up. A queued value of 4 means none. */
    int8_t move_dx, move_dy;
    uint8_t move_remaining, move_speed, move_preference, move_queued;
    uint8_t action_queued;
} Game;

void game_init(Game *g, uint32_t seed);
void game_set_world(Game *g, const WorldDef *world);
void game_update(Game *g, const GameInput *input);
void game_action(Game *g, GameAction action, int argument);
int game_load(Game *g, const GameSave *save);
void game_make_save(const Game *g, GameSave *save);
void game_save_result(Game *g, int success);
int game_save_valid(const GameSave *save);

int game_max_hp(const Companion *friend_);
int game_power(const Companion *friend_);
int game_defense(const Companion *friend_);
int game_xp_needed(const Companion *friend_);
int game_type_advantage(int attacking_element, int defending_element);
const char *game_objective(const Game *g);
const char *game_location(const Game *g);
const char *game_intent_name(const Battle *battle);
const char *game_growth_name(int path);
const char *game_growth_description(int path);
int game_nearest_interaction(const Game *g); /* NPC 0..3, wild 10..15, flower 20..22, camp 30, -1 none. */
int game_wild_visible(const Game *g, int index);
const char *game_interaction_label(const Game *g, int interaction);

#endif
