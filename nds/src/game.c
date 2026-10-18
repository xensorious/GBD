#include "game.h"
#include <stdio.h>
#include <string.h>

#define SAVE_MAGIC 0x41535452u
#define SAVE_VERSION 1u
#define CLAMP(v,a,b) ((v)<(a)?(a):((v)>(b)?(b):(v)))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

enum { DA_FIELD, DA_STARTER, DA_TRIAL, DA_ENDING, DA_WILD=16 };

const char *const game_element_names[5] = {"Grove", "Ember", "Tide", "Stone", "Spark"};
const Species game_species[GAME_SPECIES_COUNT] = {
    {"Ferro", "The Fern-Tail", "A curious forest cub. Its fern tail unfurls when it trusts someone.",
     "Briar Arc", "A sweeping Grove strike. Strong against Tide.", TYPE_GROVE, 24, 7, 7},
    {"Solkin", "The Little Sun", "A bold little dragon. Its golden horn warms in the company of friends.",
     "Solar Flare", "A bright Ember strike. Strong against Grove.", TYPE_EMBER, 22, 9, 5},
    {"Nimu", "The Pearl Keeper", "A gentle moon otter. It keeps a shining pearl safe beneath its chin.",
     "Pearl Tide", "A flowing Tide strike. Strong against Ember.", TYPE_TIDE, 27, 6, 7},
    {"Bellwing", "The Bloom Moth", "Its soft wings ring like tiny bells when lantern flowers open.",
     "Petal Spiral", "A drifting Grove strike. Strong against Tide.", TYPE_GROVE, 20, 7, 5},
    {"Craglet", "The Crystal Crab", "It polishes its orange claws on shoreline stones. Surprisingly shy.",
     "Crystal Claw", "A sturdy Stone strike. Strong against Spark.", TYPE_STONE, 27, 7, 9},
    {"Voltik", "The Storm Finch", "A daring coastal bird that gathers sparks inside its dark blue wings.",
     "Sky Spark", "A quick Spark strike. Strong against Tide.", TYPE_SPARK, 21, 9, 5}
};

static const WorldDef default_world = {
    0,0,{448,624},{384,512},
    {{448,560},{360,360},{640,520},{680,180}},
    {{240,400},{720,340},{320,180},{840,470},{560,280},{160,520}},
    {{760,440},{240,280},{580,200}}
};
static const uint8_t wild_species[GAME_WILD_COUNT]={3,4,5,3,4,5};
static const uint8_t wild_levels[GAME_WILD_COUNT]={1,1,2,2,2,3};

static void copy_text(char *dst, const char *src, unsigned capacity) {
    if (!capacity) return;
    snprintf(dst,capacity,"%s",src?src:"");
}
static void toast(Game *g,const char *message) {
    copy_text(g->toast,message,sizeof(g->toast)); g->toast_timer=100;
}
static void begin_dialog(Game *g,const char *speaker,int after) {
    memset(&g->dialog,0,sizeof(g->dialog));
    copy_text(g->dialog.speaker,speaker,sizeof(g->dialog.speaker));
    g->dialog.after=(uint8_t)after; g->mode=GM_DIALOG;
}
static void say(Game *g,const char *line) {
    if (g->dialog.count<GAME_DIALOG_MAX)
        copy_text(g->dialog.lines[g->dialog.count++],line,sizeof(g->dialog.lines[0]));
}
static uint32_t save_checksum(const GameSave *save) {
    const uint8_t *bytes=(const uint8_t *)save;
    uint32_t hash=2166136261u;
    unsigned i;
    for(i=0;i<sizeof(*save);i++) {
        unsigned checksum_offset=(unsigned)((const uint8_t *)&save->checksum-bytes);
        uint8_t value=(i>=checksum_offset && i<checksum_offset+4)?0:bytes[i];
        hash=(hash^value)*16777619u;
    }
    return hash;
}

int game_max_hp(const Companion *c) {
    if (c->species>=GAME_SPECIES_COUNT) return 1;
    return game_species[c->species].base_hp+c->level*5+(c->path==2?8:0);
}
int game_power(const Companion *c) {
    return game_species[c->species].power+c->level*2+(c->path==1?3:0);
}
int game_defense(const Companion *c) {
    return game_species[c->species].defense+c->level+(c->path==2?3:0);
}
int game_xp_needed(const Companion *c) { return 4+c->level*2; }
int game_type_advantage(int attack,int defend) {
    return (attack==TYPE_GROVE && defend==TYPE_TIDE)
        || (attack==TYPE_EMBER && defend==TYPE_GROVE)
        || (attack==TYPE_TIDE && defend==TYPE_EMBER)
        || (attack==TYPE_STONE && defend==TYPE_SPARK)
        || (attack==TYPE_SPARK && defend==TYPE_TIDE);
}
const char *game_growth_name(int path) { return path==1?"Radiant":"Resolute"; }
const char *game_growth_description(int path) {
    return path==1?"Brighter attacks: +3 Power. A bold path for a decisive companion.":
                   "A stronger bond: +8 HP and +3 Defense. A steady path for a guardian.";
}

static Companion make_companion(int species,int level) {
    Companion c;
    memset(&c,0,sizeof(c)); c.species=(uint8_t)species; c.level=(uint8_t)level;
    c.hp=(uint16_t)game_max_hp(&c); c.energy=3; return c;
}
static void heal_party(Game *g) {
    int i; for(i=0;i<g->save.party_count;i++) {
        g->save.party[i].hp=(uint16_t)game_max_hp(&g->save.party[i]);
        g->save.party[i].energy=3;
    }
}
static void reset_progress(Game *g,uint32_t seed) {
    memset(&g->save,0,sizeof(g->save));
    g->save.magic=SAVE_MAGIC; g->save.version=SAVE_VERSION;
    g->save.seed=seed?seed:0xA57E2026u;
    g->save.player_x=g->world->spawn.x; g->save.player_y=g->world->spawn.y;
    g->save.tonics=3; g->save.facing=0; g->facing=0;
    g->pending_growth=0; g->cursor=0; g->has_save=0; g->ui_focus=0; g->ui_tab=0;
    memset(g->wild_cooldown,0,sizeof(g->wild_cooldown));
}
void game_init(Game *g,uint32_t seed) {
    memset(g,0,sizeof(*g)); g->world=&default_world;
    reset_progress(g,seed); g->mode=GM_TITLE;
}
void game_set_world(Game *g,const WorldDef *world) {
    g->world=world?world:&default_world;
    if(!(g->save.flags&PF_STARTED)) {
        g->save.player_x=g->world->spawn.x; g->save.player_y=g->world->spawn.y;
    }
}
void game_make_save(const Game *g,GameSave *save) {
    *save=g->save; save->magic=SAVE_MAGIC; save->version=SAVE_VERSION;
    save->facing=g->facing; save->checksum=0; save->checksum=save_checksum(save);
}
int game_save_valid(const GameSave *save) {
    unsigned i;
    if(!save || save->magic!=SAVE_MAGIC || save->version!=SAVE_VERSION
       || save->checksum!=save_checksum(save)) return 0;
    if(save->player_x<8 || save->player_x>GAME_WORLD_W-8 || save->player_y<8 || save->player_y>GAME_WORLD_H-8
       || save->party_count>GAME_PARTY_MAX || save->facing>3 || save->flowers>7
       || save->seen>63 || save->recruited>63 || save->wild_taken>63 || save->tonics>99
       || save->starter>2 || (save->flags&~511u)) return 0;
    if(save->party_count && save->lead>=save->party_count) return 0;
    if(!save->party_count && save->lead) return 0;
    if(!!(save->flags&PF_CHOSEN)!=!!save->party_count) return 0;
    if((save->flags&PF_CHOSEN) && (!(save->flags&PF_STARTED) || save->party[0].species!=save->starter)) return 0;
    if((save->flags&PF_TRIAL_WON) && save->party_count<2) return 0;
    if((save->flags&PF_GARDEN_DONE) && (save->flowers!=7 || !(save->flags&PF_GARDEN_QUEST))) return 0;
    for(i=0;i<save->party_count;i++) {
        const Companion *c=&save->party[i]; unsigned j;
        if(c->species>=GAME_SPECIES_COUNT || c->level<1 || c->level>8 || c->path>2
           || c->hp>game_max_hp(c) || c->energy>5 || c->xp>=game_xp_needed(c)) return 0;
        if(!(save->seen&(1u<<c->species)) || !(save->recruited&(1u<<c->species))) return 0;
        for(j=0;j<i;j++) if(save->party[j].species==c->species) return 0;
    }
    return 1;
}
int game_load(Game *g,const GameSave *save) {
    int i;
    if(!game_save_valid(save)) return 0;
    g->save=*save; g->facing=save->facing; g->has_save=1; g->title_cursor=0;
    g->mode=GM_TITLE; g->pending_growth=0;
    for(i=0;i<save->party_count;i++)
        if(save->party[i].level>=3 && !save->party[i].path) g->pending_growth|=(uint8_t)(1u<<i);
    return 1;
}
void game_save_result(Game *g,int success) {
    g->save_requested=0;
    if(success) { g->has_save=1; if(g->save_feedback) toast(g,"Journey saved."); }
    else toast(g,"Save could not be written. Your journey is still open.");
    g->save_feedback=0;
}

const char *game_objective(const Game *g) {
    if(!(g->save.flags&PF_CHOSEN)) return "Meet Mira at the Research Lodge. Choose a companion.";
    if(g->save.flags&PF_TRIAL_WON) return "Lantern Crest earned! Explore, befriend, and grow.";
    if(!(g->save.flags&PF_MET_JUN)) return "Find Jun beside the fountain in Lantern Commons.";
    if(g->save.party_count<2) return "Befriend a wild creature, then visit Vale on the hill.";
    return "Meet Vale at the hilltop lantern for your first trial.";
}
const char *game_location(const Game *g) {
    int x=g->save.player_x,y=g->save.player_y;
    if(y>540 && x<455) return "Sunwake Harbor";
    if(x>750 && y<235) return "Aster Terrace";
    if(x>740 && y>310) return "Moonwater Banks";
    if(x>495 && y<210) return "Northgate";
    if(x<330 && y<215) return "Research Lodge";
    if(x<385 && y<395) return "Willow Garden";
    if(y>450 && x<455) return "Harbor Walk";
    return "Lantern Commons";
}
const char *game_intent_name(const Battle *b) {
    if(b->intent==BI_HEAVY) return "Heavy strike: guard or strike first";
    if(b->intent==BI_REST) return "Gathering light: a safe opening";
    return "Quick strike: a light attack";
}
int game_wild_visible(const Game *g,int index) {
    return index>=0 && index<GAME_WILD_COUNT && (g->save.flags&PF_CHOSEN)
        && !(g->save.wild_taken&(1u<<index)) && !g->wild_cooldown[index];
}
static int distance_squared(int ax,int ay,int bx,int by) {
    int dx=ax-bx,dy=ay-by; return dx*dx+dy*dy;
}
int game_nearest_interaction(const Game *g) {
    int i,best=-1,best_distance=46*46;
    int px=g->save.player_x,py=g->save.player_y;
    for(i=0;i<GAME_NPC_COUNT;i++) {
        int d=distance_squared(px,py,g->world->npcs[i].x,g->world->npcs[i].y);
        if(d<best_distance) { best=i; best_distance=d; }
    }
    if(g->save.flags&PF_CHOSEN) {
        for(i=0;i<GAME_WILD_COUNT;i++) if(game_wild_visible(g,i)) {
            int d=distance_squared(px,py,g->world->wilds[i].x,g->world->wilds[i].y);
            if(d<best_distance && d<40*40) { best=10+i; best_distance=d; }
        }
        for(i=0;i<3;i++) if(!(g->save.flowers&(1u<<i))) {
            int d=distance_squared(px,py,g->world->flowers[i].x,g->world->flowers[i].y);
            if(d<best_distance && d<32*32) { best=20+i; best_distance=d; }
        }
        {
            int d=distance_squared(px,py,g->world->camp.x,g->world->camp.y);
            if(d<best_distance) best=30;
        }
    }
    return best;
}
const char *game_interaction_label(const Game *g,int interaction) {
    static const char *const npc_names[4]={"A: Talk to Mira","A: Talk to Jun","A: Talk to Edda","A: Talk to Vale"};
    if(interaction>=0 && interaction<4) return npc_names[interaction];
    if(interaction>=10 && interaction<16) return "A: Meet this wild creature";
    if(interaction>=20 && interaction<23) return "A: Gather a lantern blossom";
    if(interaction==30) return "A: Rest at the lantern camp";
    (void)g; return "Explore with the D-pad. Hold B to run.";
}

static void maybe_growth(Game *g) {
    int i;
    if(g->mode!=GM_FIELD || !g->pending_growth) return;
    for(i=0;i<g->save.party_count;i++) if(g->pending_growth&(1u<<i)) {
        if(g->save.party[i].path || g->save.party[i].level<3) {
            g->pending_growth&=(uint8_t)~(1u<<i); continue;
        }
        g->growth_slot=(uint8_t)i; g->cursor=0; g->mode=GM_GROWTH; break;
    }
}
static void start_battle(Game *g,int encounter,int trial) {
    int i;
    Battle *b=&g->battle;
    memset(b,0,sizeof(*b)); b->is_trial=(uint8_t)trial; b->encounter=(uint8_t)encounter;
    b->species=trial?5:wild_species[encounter]; b->level=trial?3:wild_levels[encounter];
    b->max_hp=(uint16_t)(game_species[b->species].base_hp+b->level*5+(trial?12:-2));
    b->hp=b->max_hp; b->intent=BI_QUICK; b->phase=BP_COMMAND;
    b->active=g->save.lead;
    if(!g->save.party[b->active].hp) {
        for(i=0;i<g->save.party_count;i++) if(g->save.party[i].hp) { b->active=(uint8_t)i; break; }
    }
    if(!g->save.party[b->active].hp) heal_party(g);
    g->save.seen|=(uint8_t)(1u<<b->species); g->save.flags|=PF_FIRST_BATTLE;
    g->cursor=0; g->mode=GM_BATTLE; g->menu_from_battle=0;
}
static void dialog_finished(Game *g) {
    int after=g->dialog.after;
    g->mode=GM_FIELD; g->interaction_cooldown=8;
    if(after==DA_STARTER) { g->mode=GM_STARTER; g->cursor=0; g->save.seen|=7; }
    else if(after==DA_TRIAL) start_battle(g,0,1);
    else if(after==DA_ENDING) { g->mode=GM_ENDING; g->cursor=0; }
    else if(after>=DA_WILD && after<DA_WILD+GAME_WILD_COUNT) start_battle(g,after-DA_WILD,0);
    else maybe_growth(g);
}
static void interact_npc(Game *g,int npc) {
    if(!(g->save.flags&PF_CHOSEN) && npc!=NPC_MIRA) {
        begin_dialog(g,npc==NPC_JUN?"Jun":npc==NPC_EDDA?"Edda":"Vale",DA_FIELD);
        say(g,"Mira is waiting at the Research Lodge in the northwest. Your first companion is excited to meet you!"); return;
    }
    if(npc==NPC_MIRA) {
        if(!(g->save.flags&PF_CHOSEN)) {
            begin_dialog(g,"Mira",DA_STARTER);
            say(g,"Welcome to Sunwake Harbor! Every lantern keeper starts with a companion, not a collection.");
            say(g,"Ferro is steady, Solkin is daring, and Nimu is gentle. Choose whoever feels right to you.");
            say(g,"All three can grow into wonderful partners. There is no wrong beginning.");
        } else {
            heal_party(g); begin_dialog(g,"Mira",DA_FIELD);
            say(g,"There. Everyone is rested, and their light is bright again. You can always come home to rest.");
            say(g,(g->save.flags&PF_TRIAL_WON)?"That Lantern Crest suits you. Keep meeting the island's creatures; every bond has room to grow.":
                "Jun is beside the Commons fountain. A friend beside you makes the first trial much less daunting.");
            g->save_requested=1;
        }
    } else if(npc==NPC_JUN) {
        begin_dialog(g,"Jun",DA_FIELD);
        if(!(g->save.flags&PF_MET_JUN)) {
            g->save.flags|=PF_MET_JUN;
            say(g,"Wild creatures wander in plain sight. Walk close and press A when you want to meet one.");
            say(g,"In a battle, Strike builds energy. A special skill spends two. Guard restores energy and softens a hit.");
            say(g,"Befriend twice to earn trust, or weaken a creature to half its health and Befriend once. It always works!");
            say(g,"Watch the next move above your commands. Heavy strike coming? Guard! Gathering light? You have an opening.");
            say(g,"Meet one wild friend, then find Vale at the hilltop lantern. The camp here restores your whole team for free.");
            g->save_requested=1;
        } else {
            say(g,"Befriend works at half health or after two gentle approaches. You never need a rare item or a lucky roll.");
            say(g,"Your entire team shares experience. At level three, each companion can choose its own growth path.");
        }
    } else if(npc==NPC_EDDA) {
        begin_dialog(g,"Edda",DA_FIELD);
        if(g->save.flags&PF_GARDEN_DONE) {
            say(g,"The lantern blossoms are thriving. Thank you for making this little corner of the island brighter.");
            say(g,"Your companions grow through shared journeys. You can review their paths and field notes in your Journal.");
        } else if(g->save.flowers==7) {
            g->save.flags|=PF_GARDEN_QUEST|PF_GARDEN_DONE;
            g->save.tonics=(uint8_t)MIN(99,g->save.tonics+3); heal_party(g); g->save_requested=1;
            say(g,"All three lantern blossoms! These will keep the harbor lantern burning through the next storm.");
            say(g,"Please take three Dew Tonics. Open your Team, select a companion, and press X to heal it anywhere.");
            say(g,"You did a kind thing without being asked to win a battle. The island needs that sort of keeper, too.");
        } else {
            g->save.flags|=PF_GARDEN_QUEST;
            say(g,"I'm gathering three lantern blossoms for the harbor light. Look for glowing flowers beside the paths.");
            say(g,"Walk close and press A to pick one. Bring all three back for a small thank-you. There is no hurry.");
        }
    } else {
        begin_dialog(g,"Vale",DA_FIELD);
        if(g->save.flags&PF_TRIAL_WON) {
            say(g,"Your Lantern Crest marks a beginning, not an ending. Our island still has friends for you to meet.");
            say(g,"Want to practice again? Rest up and meet the wild creatures. Every companion shares the experience.");
        } else if(g->save.party_count<2) {
            g->save.flags|=PF_MET_VALE;
            say(g,"I'm Vale, the lookout keeper. The first trial is about trust. Bring at least two companions who trust you.");
            say(g,"Wild Bellwing, Craglet, and Voltik live around the island. Jun can teach you how to befriend them.");
        } else {
            g->save.flags|=PF_MET_VALE; g->dialog.after=DA_TRIAL; heal_party(g);
            say(g,"Two friends already! Let me restore your team before we begin. A trial should test your choices fairly.");
            say(g,"My Voltik gathers stormlight, then strikes hard. Read its intent, guard when needed, and switch friends with X.");
            say(g,"Win together and the Lantern Crest is yours. Press A when you're ready, or B to come back later.");
        }
    }
}
static void interact(Game *g) {
    int target=game_nearest_interaction(g);
    if(target>=0 && target<4) interact_npc(g,target);
    else if(target>=10 && target<16) {
        int encounter=target-10;
        if(!(g->save.flags&PF_FIRST_BATTLE)) {
            begin_dialog(g,"A wild encounter",DA_WILD+encounter);
            say(g,"A curious creature notices your lantern. It wants to see what kind of keeper you are.");
            say(g,"Strike builds energy. Your element skill costs two. Guard restores two energy and halves the next hit.");
            say(g,"Befriend at half health, or Befriend twice to build trust. Press X to switch your team; B lets you leave.");
        } else start_battle(g,encounter,0);
    } else if(target>=20 && target<23) {
        char line[112]; int count;
        g->save.flowers|=(uint8_t)(1u<<(target-20));
        count=!!(g->save.flowers&1)+!!(g->save.flowers&2)+!!(g->save.flowers&4);
        begin_dialog(g,"Lantern blossom",DA_FIELD);
        snprintf(line,sizeof(line),"You gathered a lantern blossom. Its petals glow softly. Blossoms found: %d of 3.",count); say(g,line);
        if(count==3) say(g,"Edda in the gardens will know just what to do with these.");
        g->save_requested=1;
    } else if(target==30) {
        heal_party(g); begin_dialog(g,"Lantern camp",DA_FIELD);
        say(g,"You share a quiet moment beneath the lantern. Every companion's health and energy are restored.");
        g->save_requested=1;
    } else toast(g,game_objective(g));
}

static void battle_log(Game *g,const char *line) {
    Battle *b=&g->battle;
    if(b->log_count<GAME_LOG_MAX)
        copy_text(b->logs[b->log_count++],line,sizeof(b->logs[0]));
}
static void log_number(Game *g,const char *name,const char *verb,int number) {
    char line[112]; snprintf(line,sizeof(line),"%s %s %d damage.",name,verb,number); battle_log(g,line);
}
static int enemy_damage(const Game *g,int heavy) {
    const Companion *c=&g->save.party[g->battle.active];
    int base=(heavy?11:7)+g->battle.level*2+(g->battle.is_trial?1:0);
    return MAX(2,base-game_defense(c)/2);
}
static void enemy_turn(Game *g) {
    Battle *b=&g->battle; Companion *c=&g->save.party[b->active]; int damage;
    if(b->result || !b->hp) return;
    if(b->intent==BI_REST) battle_log(g,"The wild light settles. Your opponent gathers strength instead of attacking.");
    else {
        damage=enemy_damage(g,b->intent==BI_HEAVY);
        if(b->guarded) damage=MAX(1,damage/2);
        c->hp=(uint16_t)MAX(0,(int)c->hp-damage);
        b->flash_player=18; b->effect=2; b->effect_timer=16;
        log_number(g,game_species[b->species].name,b->intent==BI_HEAVY?"lands a heavy strike for":"strikes for",damage);
        if(!c->hp) {
            int i;
            battle_log(g,"Your companion needs a rest. Another friend can carry the lantern.");
            for(i=0;i<g->save.party_count;i++) if(g->save.party[i].hp) break;
            if(i==g->save.party_count) { b->result=BR_LOSS; battle_log(g,"Your team returns safely to the lantern camp. No progress is lost."); }
            else { b->active=(uint8_t)i; g->save.lead=(uint8_t)i; }
        }
    }
    b->guarded=0; b->turn++;
    b->intent=(uint8_t)(b->turn%3==1?BI_HEAVY:b->turn%3==2?BI_REST:BI_QUICK);
}
static void give_experience(Game *g,int amount) {
    int i;
    for(i=0;i<g->save.party_count;i++) {
        Companion *c=&g->save.party[i]; int xp=c->xp+amount;
        while(c->level<8 && xp>=game_xp_needed(c)) {
            xp-=game_xp_needed(c); c->level++; c->hp=(uint16_t)MIN(game_max_hp(c),c->hp+5);
            if(c->level>=3 && !c->path) g->pending_growth|=(uint8_t)(1u<<i);
        }
        c->xp=(uint8_t)(c->level==8?MIN(xp,game_xp_needed(c)-1):xp);
        if(c->level>=3 && !c->path) g->pending_growth|=(uint8_t)(1u<<i);
    }
}
static void finish_battle(Game *g) {
    Battle *b=&g->battle;
    int result=b->result;
    g->mode=GM_FIELD; g->interaction_cooldown=20; g->ui_focus=0; g->ui_tab=0; g->menu_from_battle=0;
    if(result==BR_LOSS) {
        heal_party(g); g->save.player_x=g->world->camp.x; g->save.player_y=g->world->camp.y+28;
        begin_dialog(g,"A safe return",DA_FIELD);
        say(g,"Your companions are rested at the lantern camp. Take a breath, try a different plan, and set out again.");
    } else if(result==BR_RETREAT) {
        if(!b->is_trial) g->wild_cooldown[b->encounter]=90;
        toast(g,"You step away safely. There is always another chance.");
    } else {
        if(!b->is_trial) g->wild_cooldown[b->encounter]=600;
        g->save.battles_won++;
        if(result==BR_RECRUIT) {
            int species=b->species; int first=!(g->save.flags&PF_FIRST_FRIEND);
            g->save.party[g->save.party_count++]=make_companion(species,b->level);
            g->save.recruited|=(uint8_t)(1u<<species); g->save.wild_taken|=(uint8_t)(1u<<b->encounter);
            g->save.flags|=PF_FIRST_FRIEND;
            give_experience(g,4);
            begin_dialog(g,"A new companion",DA_FIELD);
            {
                char line[112]; snprintf(line,sizeof(line),"%s joins your team! All of your companions share the experience of this new bond.",game_species[species].name); say(g,line);
            }
            if(first) say(g,"Open Team to choose your lead companion. Vale is waiting at the hilltop lantern when you're ready.");
        } else {
            give_experience(g,b->is_trial?10:6);
            if(b->is_trial) {
                g->save.flags|=PF_TRIAL_WON; heal_party(g);
                begin_dialog(g,"Vale",DA_ENDING);
                say(g,"You watched, adapted, and trusted your companions. That is what a lantern keeper does.");
                say(g,"The Lantern Crest is yours. Let it remind you: even a small light can guide a friend home.");
                say(g,"This first chapter is complete. The island stays open, so keep exploring and growing your team.");
            } else toast(g,"Victory! Every companion gained experience.");
        }
    }
    g->save_requested=1;
    maybe_growth(g);
}
static void battle_move(Game *g,int move) {
    Battle *b=&g->battle;
    Companion *c=&g->save.party[b->active];
    int damage,enemy_defense,already=0,i;
    char line[112];
    if(b->phase!=BP_COMMAND || move<0 || move>3) return;
    if(move==1 && c->energy<2) { toast(g,"Your skill needs 2 energy. Strike or Guard to recharge."); return; }
    b->log_count=0; b->log_index=0; b->result=BR_NONE;
    enemy_defense=game_species[b->species].defense+b->level;
    if(move==0 || move==1) {
        damage=MAX(2,game_power(c)+(move==1?7:2)-enemy_defense/2);
        if(move==1) {
            c->energy-=2;
            if(game_type_advantage(game_species[c->species].element,game_species[b->species].element)) damage+=5;
            snprintf(line,sizeof(line),"%s uses %s!",game_species[c->species].name,game_species[c->species].skill); battle_log(g,line);
        } else c->energy=(uint8_t)MIN(5,c->energy+1);
        b->hp=(uint16_t)MAX(0,(int)b->hp-damage);
        b->flash_enemy=18; b->effect=(uint8_t)(move==1?3+game_species[c->species].element:1); b->effect_timer=20;
        log_number(g,game_species[c->species].name,move==1?"shines for":"strikes for",damage);
        if(!b->hp) { b->result=BR_WIN; battle_log(g,b->is_trial?"The trial is won! Your team stands together.":"The wild creature yields. Your whole team shares the experience."); }
    } else if(move==2) {
        c->energy=(uint8_t)MIN(5,c->energy+2); b->guarded=1;
        battle_log(g,"Your companion guards the lantern, restores 2 energy, and braces for the next hit.");
    } else {
        for(i=0;i<g->save.party_count;i++) if(g->save.party[i].species==b->species) already=1;
        if(b->is_trial) { battle_log(g,"This companion is already bonded with Vale. It appreciates the kind gesture."); b->guarded=1; c->energy=(uint8_t)MIN(5,c->energy+1); }
        else if(already) { battle_log(g,"You already have this species as a friend. Its wild kin stays on the island."); b->guarded=1; c->energy=(uint8_t)MIN(5,c->energy+1); }
        else if(g->save.party_count>=GAME_PARTY_MAX) { battle_log(g,"Your lantern is full. This creature will remain here for now."); b->guarded=1; }
        else {
            b->calm++;
            if(b->hp*2<=b->max_hp || b->calm>=2) {
                b->result=BR_RECRUIT;
                battle_log(g,"You lower your lantern and offer your hand. The creature chooses to trust you!");
            } else battle_log(g,"The creature comes closer. One more kind approach, or lower its health to half, will earn its trust.");
        }
    }
    if(!b->result) enemy_turn(g);
    b->phase=BP_TEXT; g->cursor=0;
}

static void choose_starter(Game *g,int species) {
    char line[112];
    if(species<0 || species>2 || (g->save.flags&PF_CHOSEN)) return;
    g->save.party[0]=make_companion(species,1); g->save.party_count=1;
    g->save.starter=(uint8_t)species; g->save.lead=0; g->save.recruited=(uint8_t)(1u<<species);
    g->save.flags|=PF_CHOSEN; g->save_requested=1;
    begin_dialog(g,"Mira",DA_FIELD);
    snprintf(line,sizeof(line),"%s touches your lantern. A new bond begins!",game_species[species].name); say(g,line);
    say(g,"Take these three Dew Tonics for the road. Jun waits by the Commons fountain, and the camp always welcomes you.");
    say(g,"SELECT switches to your notebook. L and R turn its tabs. Press SELECT again to return to your journey.");
}
static void use_tonic(Game *g,int slot) {
    Companion *c;
    if(slot<0 || slot>=g->save.party_count) return;
    c=&g->save.party[slot];
    if(!g->save.tonics) { toast(g,"No Dew Tonics left. Rest at camp for free."); return; }
    if(c->hp==game_max_hp(c)) { toast(g,"This companion is already at full health."); return; }
    c->hp=(uint16_t)MIN(game_max_hp(c),c->hp+24); g->save.tonics--;
    toast(g,"A Dew Tonic restores 24 health."); g->save_requested=1;
    if(g->menu_from_battle) {
        Battle *b=&g->battle; g->mode=GM_BATTLE; b->log_count=0; b->log_index=0;
        battle_log(g,"You share a Dew Tonic with your companion."); enemy_turn(g); b->phase=BP_TEXT;
    }
}
static void focus_tab(Game *g,int tab) {
    static const GameMode modes[5]={GM_FIELD,GM_PARTY,GM_JOURNAL,GM_GUIDE,GM_MENU};
    g->ui_focus=1; g->ui_tab=(uint8_t)((tab+5)%5); g->mode=modes[g->ui_tab];
    g->cursor=g->ui_tab==1?g->save.lead:0; g->menu_from_battle=0;
}
void game_action(Game *g,GameAction action,int arg) {
    if(action==GA_SET_CURSOR) { g->cursor=(uint8_t)MAX(0,arg); return; }
    if(action==GA_SAVE) {
        if(g->save.flags&PF_STARTED) { g->save_requested=1; g->save_feedback=1; }
        return;
    }
    if(action==GA_BACK && g->ui_focus && !g->menu_from_battle
       && (g->mode==GM_FIELD || g->mode==GM_PARTY || g->mode==GM_JOURNAL || g->mode==GM_GUIDE || g->mode==GM_MENU)) {
        int was_map=g->mode==GM_FIELD;
        g->mode=GM_FIELD; g->ui_tab=0; g->cursor=0; g->ui_focus=(uint8_t)!was_map;
        return;
    }
    if(g->mode==GM_TITLE) {
        if(action==GA_BACK) { g->new_confirm=0; g->title_cursor=0; return; }
        if(action==GA_ACCEPT) {
            if(g->has_save && g->title_cursor==0) { g->mode=GM_FIELD; maybe_growth(g); return; }
            if(g->has_save && !g->new_confirm) { g->new_confirm=1; return; }
            g->new_confirm=0;
            reset_progress(g,g->save.seed); g->save.flags|=PF_STARTED;
            begin_dialog(g,"Lantern Isles",DA_FIELD);
            say(g,"On these islands, lanterns guide people home. Companions lend them their light.");
            say(g,"Your first morning begins in Sunwake Harbor. Mira is waiting at the Research Lodge, northwest of here.");
            say(g,"Move with the D-pad. A talks or meets a creature. Hold B to run. SELECT opens your map and notebook.");
        }
        return;
    }
    if(g->mode==GM_DIALOG) {
        if(action==GA_BACK && g->dialog.after==DA_TRIAL) { g->mode=GM_FIELD; g->interaction_cooldown=10; return; }
        if(action==GA_ACCEPT || action==GA_BACK) {
            if(g->dialog.index+1<g->dialog.count) g->dialog.index++;
            else dialog_finished(g);
        }
        return;
    }
    if(g->mode==GM_STARTER) {
        if(action==GA_CHOOSE_STARTER) choose_starter(g,arg);
        else if(action==GA_ACCEPT) choose_starter(g,g->cursor);
        else if(action==GA_BACK) g->mode=GM_FIELD;
        return;
    }
    if(g->mode==GM_GROWTH) {
        if(action==GA_CHOOSE_GROWTH || action==GA_ACCEPT) {
            Companion *c=&g->save.party[g->growth_slot];
            int path=action==GA_ACCEPT?g->cursor+1:arg;
            if(path!=1 && path!=2) return;
            c->path=(uint8_t)path; c->hp=(uint16_t)game_max_hp(c);
            g->pending_growth&=(uint8_t)~(1u<<g->growth_slot); g->save_requested=1;
            g->mode=GM_FIELD; toast(g,"A new path opens. Your companion shines brighter!"); maybe_growth(g);
        }
        return;
    }
    if(g->mode==GM_BATTLE) {
        Battle *b=&g->battle;
        if(action==GA_ACCEPT && b->phase==BP_TEXT) {
            if(b->log_index+1<b->log_count) b->log_index++;
            else if(b->result) finish_battle(g);
            else b->phase=BP_COMMAND;
        } else if(action==GA_ACCEPT && b->phase==BP_COMMAND) battle_move(g,g->cursor);
        else if(action==GA_BATTLE_MOVE) battle_move(g,arg);
        else if(action==GA_BACK && b->phase==BP_COMMAND) {
            b->result=BR_RETREAT; b->log_count=0; b->log_index=0;
            battle_log(g,b->is_trial?"You pause the trial. Vale will be ready when you return.":"You lower your lantern and step away safely."); b->phase=BP_TEXT;
        } else if(action==GA_PARTY && b->phase==BP_COMMAND) {
            g->mode=GM_PARTY; g->menu_from_battle=1; g->cursor=b->active;
        }
        return;
    }
    if(g->mode==GM_PARTY) {
        if(action==GA_BACK) { g->mode=g->menu_from_battle?GM_BATTLE:GM_FIELD; g->cursor=0; g->menu_from_battle=0; }
        else if(action==GA_SELECT_PARTY || action==GA_ACCEPT) {
            int slot=action==GA_ACCEPT?g->cursor:arg;
            if(slot<0 || slot>=g->save.party_count) return;
            if(!g->save.party[slot].hp) { toast(g,"This companion needs a tonic or a rest at camp."); return; }
            if(g->menu_from_battle) {
                Battle *b=&g->battle;
                if(slot==b->active) { g->mode=GM_BATTLE; g->cursor=0; return; }
                b->active=(uint8_t)slot; g->save.lead=(uint8_t)slot; g->mode=GM_BATTLE;
                b->log_count=0; b->log_index=0; b->guarded=1;
                battle_log(g,"A fresh companion takes the lantern. It braces while stepping into the battle.");
                enemy_turn(g); b->phase=BP_TEXT; g->cursor=0;
            } else { g->save.lead=(uint8_t)slot; toast(g,"Your companion takes the lead."); g->save_requested=1; }
        }
        return;
    }
    if(g->mode==GM_MENU) {
        if(action==GA_BACK) g->mode=GM_FIELD;
        else if(action==GA_ACCEPT) {
            int choice=g->cursor; g->cursor=0;
            if(choice==0) { g->mode=GM_PARTY; g->menu_from_battle=0; }
            else if(choice==1) g->mode=GM_JOURNAL;
            else if(choice==2) g->mode=GM_GUIDE;
            else if(choice==3) { g->save_requested=1; g->save_feedback=1; g->cursor=3; }
            else { g->mode=GM_FIELD; g->ui_focus=0; g->ui_tab=0; }
            if(choice<3) { g->ui_focus=1; g->ui_tab=(uint8_t)(choice+1); }
        }
        return;
    }
    if(g->mode==GM_JOURNAL || g->mode==GM_GUIDE) {
        if(action==GA_BACK || action==GA_MENU) { g->mode=GM_FIELD; g->cursor=0; }
        return;
    }
    if(g->mode==GM_ENDING) {
        if(action==GA_ACCEPT || action==GA_BACK) { g->mode=GM_FIELD; maybe_growth(g); }
        return;
    }
    if(g->mode!=GM_FIELD) return;
    if(g->ui_focus && (action==GA_INTERACT || action==GA_ACCEPT)) return;
    if(action==GA_INTERACT || action==GA_ACCEPT) { if(!g->interaction_cooldown) interact(g); }
    else if(action==GA_MENU) { g->mode=GM_MENU; g->cursor=0; g->ui_focus=1; g->ui_tab=4; }
    else if(action==GA_PARTY) {
        if(!g->save.party_count) toast(g,"Mira has your first companion waiting at the Research Lodge.");
        else { g->mode=GM_PARTY; g->cursor=g->save.lead; g->menu_from_battle=0; g->ui_focus=1; g->ui_tab=1; }
    } else if(action==GA_JOURNAL) { g->mode=GM_JOURNAL; g->cursor=0; g->ui_focus=1; g->ui_tab=2; }
    else if(action==GA_GUIDE) { g->mode=GM_GUIDE; g->cursor=0; g->ui_focus=1; g->ui_tab=3; }
    else if(action==GA_REST && game_nearest_interaction(g)==30) interact(g);
}

static int blocked(const Game *g,int x,int y) {
    unsigned i;
    if(x<8 || y<8 || x>GAME_WORLD_W-8 || y>GAME_WORLD_H-8) return 1;
    for(i=0;i<g->world->solid_count;i++) {
        const WorldSolid *r=&g->world->solids[i];
        if(x+6>r->x && x-6<r->x+r->w && y+2>r->y && y-7<r->y+r->h) return 1;
    }
    for(i=0;i<GAME_NPC_COUNT;i++)
        if(distance_squared(x,y,g->world->npcs[i].x,g->world->npcs[i].y)<12*12) return 1;
    return 0;
}
static void cycle_cursor(Game *g,int change,int count) {
    if(count>0) g->cursor=(uint8_t)((g->cursor+count+change)%count);
}
void game_update(Game *g,const GameInput *input) {
    uint32_t p=input->pressed;
    int i;
    g->frame++;
    if(g->toast_timer) g->toast_timer--;
    if(g->interaction_cooldown) g->interaction_cooldown--;
    if(g->battle.flash_enemy) g->battle.flash_enemy--;
    if(g->battle.flash_player) g->battle.flash_player--;
    if(g->battle.effect_timer) g->battle.effect_timer--;
    for(i=0;i<GAME_WILD_COUNT;i++) if(g->wild_cooldown[i]) g->wild_cooldown[i]--;
    g->walking=0;
    /* This game is deliberately button-only, including in an emulator on a phone. */
    if((g->mode==GM_FIELD || g->mode==GM_PARTY || g->mode==GM_JOURNAL || g->mode==GM_GUIDE || g->mode==GM_MENU)
       && !g->menu_from_battle) {
        if(p&GK_SELECT) {
            if(g->ui_focus) { g->ui_focus=0; g->ui_tab=0; g->mode=GM_FIELD; g->cursor=0; }
            else focus_tab(g,0);
            return;
        }
        if(g->ui_focus && (p&(GK_L|GK_R))) {
            focus_tab(g,g->ui_tab+((p&GK_L)?-1:1)); return;
        }
        if(g->ui_focus && (p&GK_B)) { game_action(g,GA_BACK,0); return; }
        if(g->ui_focus && g->mode==GM_FIELD) return;
    }
    if(g->mode==GM_TITLE) {
        if((p&(GK_UP|GK_DOWN)) && g->has_save) { g->title_cursor^=1; g->new_confirm=0; }
        if(p&(GK_A|GK_START)) game_action(g,GA_ACCEPT,0);
        else if(p&GK_B) game_action(g,GA_BACK,0);
        return;
    }
    if(g->mode==GM_DIALOG || g->mode==GM_ENDING) {
        if(p&GK_A) game_action(g,GA_ACCEPT,0);
        else if(p&GK_B) game_action(g,GA_BACK,0);
        return;
    }
    if(g->mode==GM_FIELD) {
        int dx=0,dy=0,speed=(input->held&GK_B)?3:2;
        if(p&GK_START) { game_action(g,GA_MENU,0); return; }
        if(p&GK_X) { game_action(g,GA_PARTY,0); return; }
        if(p&GK_Y) { game_action(g,GA_JOURNAL,0); return; }
        if(p&GK_A) { game_action(g,GA_INTERACT,0); return; }
        if(input->held&GK_LEFT) { dx=-speed; g->facing=1; }
        else if(input->held&GK_RIGHT) { dx=speed; g->facing=2; }
        if(input->held&GK_UP) { dy=-speed; g->facing=3; }
        else if(input->held&GK_DOWN) { dy=speed; g->facing=0; }
        if(dx && dy) { dx=dx<0?-(speed-1):speed-1; dy=dy<0?-(speed-1):speed-1; }
        if(dx && !blocked(g,g->save.player_x+dx,g->save.player_y)) { g->save.player_x+=(int16_t)dx; g->walking=1; }
        if(dy && !blocked(g,g->save.player_x,g->save.player_y+dy)) { g->save.player_y+=(int16_t)dy; g->walking=1; }
        if(g->walking) { g->anim_tick++; g->save.steps++; }
        g->save.facing=g->facing; maybe_growth(g); return;
    }
    if(g->mode==GM_BATTLE && g->battle.phase==BP_TEXT) {
        if(p&(GK_A|GK_B)) game_action(g,GA_ACCEPT,0);
        return;
    }
    if(g->mode==GM_STARTER) {
        if(p&GK_LEFT) cycle_cursor(g,-1,3);
        if(p&GK_RIGHT) cycle_cursor(g,1,3);
    } else if(g->mode==GM_BATTLE) {
        if(p&(GK_LEFT|GK_RIGHT)) g->cursor^=1;
        if(p&(GK_UP|GK_DOWN)) g->cursor^=2;
        if(p&GK_X) { game_action(g,GA_PARTY,0); return; }
    } else if(g->mode==GM_MENU) {
        if(p&GK_UP) cycle_cursor(g,-1,5);
        if(p&GK_DOWN) cycle_cursor(g,1,5);
    } else if(g->mode==GM_PARTY) {
        if(p&GK_UP) cycle_cursor(g,-1,g->save.party_count);
        if(p&GK_DOWN) cycle_cursor(g,1,g->save.party_count);
        if(p&GK_X) { use_tonic(g,g->cursor); return; }
    } else if(g->mode==GM_JOURNAL) {
        g->cursor=0;
    } else if(g->mode==GM_GUIDE) {
        if(p&(GK_LEFT|GK_UP)) cycle_cursor(g,-1,GAME_SPECIES_COUNT);
        if(p&(GK_RIGHT|GK_DOWN)) cycle_cursor(g,1,GAME_SPECIES_COUNT);
    } else if(g->mode==GM_GROWTH) {
        if(p&(GK_LEFT|GK_RIGHT|GK_UP|GK_DOWN)) g->cursor^=1;
    }
    if(p&GK_A) game_action(g,GA_ACCEPT,0);
    else if(p&(GK_B|GK_START)) game_action(g,GA_BACK,0);
}
