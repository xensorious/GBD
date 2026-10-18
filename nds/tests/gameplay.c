/* Native integration tests for the portable rules, with explicit scenario
 * fixtures. Actual ROM/button validation is separate in playthrough.py. */
#include "game.h"
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(test) do { checks++; if (!(test)) { \
    fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#test); return 1; \
} } while (0)

static void tick(Game *g,unsigned frames) {
    GameInput input={0};
    while(frames--) game_update(g,&input);
}
static void settle(Game *g,int growth_path) {
    unsigned guard=0;
    while(guard++<100) {
        if(g->mode==GM_DIALOG || (g->mode==GM_BATTLE && g->battle.phase==BP_TEXT))
            game_action(g,GA_ACCEPT,0);
        else if(g->mode==GM_GROWTH) game_action(g,GA_CHOOSE_GROWTH,growth_path);
        else break;
    }
    assert(guard<100);
    tick(g,24);
}
static void at(Game *g,WorldPoint p) {
    /* Scenario positioning is allowed only in native rule tests. The emulator
     * integration follows a collision-aware route using held buttons. */
    g->save.player_x=p.x; g->save.player_y=p.y+24;
    tick(g,24);
}
static void meet(Game *g,WorldPoint p,int path) {
    at(g,p); game_action(g,GA_INTERACT,0); settle(g,path);
}
static void start(Game *g,int starter) {
    game_init(g,0xA57E2026);
    game_action(g,GA_ACCEPT,0); settle(g,1);
    meet(g,g->world->npcs[NPC_MIRA],1);
    assert(g->mode==GM_STARTER);
    game_action(g,GA_CHOOSE_STARTER,starter); settle(g,1);
}
static void command(Game *g,int move,int path) {
    assert(g->mode==GM_BATTLE && g->battle.phase==BP_COMMAND);
    game_action(g,GA_BATTLE_MOVE,move); settle(g,path);
}
static void recruit(Game *g,int encounter,int path) {
    meet(g,g->world->npcs[NPC_MIRA],path);
    meet(g,g->world->wilds[encounter],path);
    assert(g->mode==GM_BATTLE);
    command(g,3,path); command(g,3,path);
}

int main(void) {
    Game g,loaded;
    GameSave save,corrupt;
    unsigned starter,i,turns;
    CHECK(sizeof(Companion)==8);
    CHECK(sizeof(GameSave)==88);
    for(starter=0;starter<3;starter++) {
        start(&g,(int)starter);
        CHECK(g.mode==GM_FIELD);
        CHECK(g.save.flags&PF_CHOSEN);
        CHECK(g.save.party_count==1 && g.save.party[0].species==starter);
        CHECK(g.save.party[0].hp==game_max_hp(&g.save.party[0]));
        CHECK(strstr(game_objective(&g),"Jun")!=0);
        meet(&g,g.world->npcs[NPC_JUN],1);
        CHECK(g.save.flags&PF_MET_JUN);
        recruit(&g,0,starter==1?2:1);
        CHECK(g.save.party_count==2 && g.save.party[1].species==3);
        CHECK(g.save.flags&PF_FIRST_FRIEND);
        CHECK(!game_wild_visible(&g,0));
        CHECK(g.save.recruited&(1u<<3));
        /* Retreat is reversible and consumes no tonic or progress. */
        meet(&g,g.world->wilds[1],1);
        CHECK(g.mode==GM_BATTLE);
        game_action(&g,GA_BACK,0); settle(&g,1);
        CHECK(g.mode==GM_FIELD && g.save.party_count==2);
        CHECK(g.wild_cooldown[1]>0);
        tick(&g,100);
        /* Half-health recruitment works after attacks, without randomness. */
        meet(&g,g.world->npcs[NPC_MIRA],1);
        meet(&g,g.world->wilds[1],1);
        for(turns=0;g.battle.hp*2>g.battle.max_hp && turns<10;turns++)
            command(&g,0,1);
        CHECK(g.mode==GM_BATTLE && g.battle.hp>0);
        command(&g,3,1);
        CHECK(g.save.party_count==3 && g.save.party[2].species==4);
        /* Garden quest is optional, completable, and rewards once. */
        meet(&g,g.world->npcs[NPC_EDDA],1);
        CHECK(g.save.flags&PF_GARDEN_QUEST);
        for(i=0;i<3;i++) meet(&g,g.world->flowers[i],1);
        CHECK(g.save.flowers==7);
        i=g.save.tonics;
        meet(&g,g.world->npcs[NPC_EDDA],1);
        CHECK(g.save.flags&PF_GARDEN_DONE);
        CHECK(g.save.tonics==i+3);
        meet(&g,g.world->npcs[NPC_EDDA],1);
        CHECK(g.save.tonics==i+3);
        /* Read intent, guard the heavy strike, and win the first chapter. */
        meet(&g,g.world->npcs[NPC_VALE],1);
        CHECK(g.mode==GM_BATTLE && g.battle.is_trial);
        for(turns=0;g.mode==GM_BATTLE && turns<40;turns++) {
            Companion *active=&g.save.party[g.battle.active];
            int move=g.battle.intent==BI_HEAVY?2:active->energy>=2?1:0;
            command(&g,move,starter==1?2:1);
        }
        CHECK(turns<40);
        CHECK(g.mode==GM_ENDING && (g.save.flags&PF_TRIAL_WON));
        game_action(&g,GA_ACCEPT,0); settle(&g,starter==1?2:1);
        CHECK(g.mode==GM_FIELD);
        CHECK(g.save.party[0].level>=3 && g.save.party[0].path!=0);
        CHECK(g.save.party[1].level>=3); /* Shared experience. */
        /* Serialized progress survives a fresh Game instance. */
        game_make_save(&g,&save);
        CHECK(game_save_valid(&save));
        game_init(&loaded,1);
        CHECK(game_load(&loaded,&save));
        CHECK(loaded.has_save && loaded.mode==GM_TITLE);
        CHECK(!memcmp(&loaded.save,&save,sizeof(save)));
        game_action(&loaded,GA_ACCEPT,0);
        CHECK(loaded.mode==GM_FIELD && (loaded.save.flags&PF_TRIAL_WON));
        /* Every byte of the fixed save participates in corruption detection. */
        for(i=0;i<sizeof(save);i++) {
            corrupt=save; ((unsigned char *)&corrupt)[i]^=1;
            CHECK(!game_save_valid(&corrupt));
        }
    }
    /* A defeat restores the party at camp and keeps friends/progress. */
    start(&g,0);
    meet(&g,g.world->wilds[0],1);
    g.save.party[0].hp=1;
    command(&g,2,1);
    CHECK(g.mode==GM_FIELD);
    CHECK(g.save.player_x==g.world->camp.x);
    CHECK(g.save.player_y==g.world->camp.y+28);
    CHECK(g.save.party_count==1 && g.save.party[0].hp==game_max_hp(&g.save.party[0]));
    /* In-battle partner switching costs a turn and cannot revive a fainted ally. */
    recruit(&g,0,1);
    meet(&g,g.world->wilds[1],1);
    game_action(&g,GA_PARTY,0);
    CHECK(g.mode==GM_PARTY);
    game_action(&g,GA_SELECT_PARTY,1);
    CHECK(g.mode==GM_BATTLE && g.battle.active==1 && g.battle.turn==1);
    settle(&g,1);
    CHECK(g.battle.phase==BP_COMMAND);
    /* All notebook navigation works with buttons and freezes field movement. */
    game_action(&g,GA_BACK,0); settle(&g,1);
    {
        GameInput input={0};
        int x=g.save.player_x,y=g.save.player_y;
        input.pressed=GK_SELECT; game_update(&g,&input);
        CHECK(g.ui_focus && g.ui_tab==0 && g.mode==GM_FIELD);
        input.pressed=0; input.held=GK_RIGHT;
        for(i=0;i<60;i++) game_update(&g,&input);
        CHECK(g.save.player_x==x && g.save.player_y==y);
        input.held=0; input.pressed=GK_R;
        for(i=1;i<5;i++) {
            game_update(&g,&input); CHECK(g.ui_tab==i && g.ui_focus);
        }
        input.pressed=GK_L; game_update(&g,&input);
        CHECK(g.ui_tab==3);
        input.pressed=GK_SELECT; game_update(&g,&input);
        CHECK(g.mode==GM_FIELD && !g.ui_focus);
        input.pressed=0; input.touch_pressed=1; input.touch_x=30; input.touch_y=50;
        game_update(&g,&input);
        CHECK(g.mode==GM_FIELD && !g.ui_focus);
    }
    /* Starting over cannot replace a saved journey with a single A press. */
    game_make_save(&g,&save); game_init(&loaded,1);
    CHECK(game_load(&loaded,&save));
    loaded.title_cursor=1; game_action(&loaded,GA_ACCEPT,0);
    CHECK(loaded.mode==GM_TITLE && loaded.new_confirm && loaded.save.party_count==2);
    game_action(&loaded,GA_BACK,0);
    CHECK(!loaded.new_confirm && loaded.title_cursor==0 && loaded.save.party_count==2);
    loaded.title_cursor=1; game_action(&loaded,GA_ACCEPT,0); game_action(&loaded,GA_ACCEPT,0);
    CHECK(loaded.mode==GM_DIALOG && loaded.save.party_count==0 && (loaded.save.flags&PF_STARTED));
    printf("Portable gameplay: %u checks passed across all three starters.\n",checks);
    return 0;
}
