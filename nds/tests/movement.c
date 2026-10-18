/* Focused movement contracts. All positions originate in a small authored
 * fixture, then move exclusively through the public button-update API. */
#include "game.h"
#include <stdio.h>
#include <string.h>

static unsigned checks;
#define CHECK(test) do { checks++; if(!(test)) { \
    fprintf(stderr,"FAIL %s:%d: %s\n",__FILE__,__LINE__,#test); return 1; \
} } while(0)

static const WorldSolid wall[]={{184,160,16,16}};
static const WorldSolid thin_wall[]={{179,160,1,16}};
static const WorldDef open_world={
    0,0,{168,168},{328,328},
    {{808,648},{840,648},{872,648},{904,648}},
    {{808,600},{840,600},{872,600},{904,600},{936,600},{968,600}},
    {{808,568},{840,568},{872,568}}
};
/* First 88 EEPROM bytes from a completed, button-played original DS release.
 * Kept as a fixed fixture so migration cannot pass by using only new saves. */
static const unsigned char original_release_save[88]={
    0x52,0x54,0x53,0x41,0x01,0x00,0x00,0x00,0x94,0x25,0x2d,0xac,0xff,0x01,0x00,0x00,
    0x52,0x54,0x53,0x41,0x2d,0x03,0xbd,0x00,0x03,0x02,0x00,0x00,0x06,0x07,0x2f,0x09,
    0x01,0x00,0x00,0x00,0x02,0x00,0x32,0x07,0x00,0x03,0x00,0x02,0x2f,0x00,0x03,0x00,
    0x03,0x03,0x00,0x02,0x2b,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00
};
static Game fresh(void) {
    Game g;
    game_init(&g,1); game_set_world(&g,&open_world);
    g.mode=GM_FIELD; g.save.flags=PF_STARTED;
    return g;
}
static void input(Game *g,unsigned held,unsigned pressed) {
    GameInput in={0}; in.held=held; in.pressed=pressed; game_update(g,&in);
}
static void ticks(Game *g,unsigned n,unsigned held) {
    while(n--) input(g,held,0);
}
static int at(const Game *g,int x,int y) {
    return g->save.player_x==x && g->save.player_y==y;
}

int main(void) {
    Game g,walking,running,before,loaded;
    GameSave save,legacy,expected;
    WorldDef blocked_world=open_world;
    unsigned i;

    /* A tap commits one smooth tile and finishes after the button releases. */
    g=fresh(); input(&g,GK_RIGHT,GK_RIGHT);
    CHECK(at(&g,172,168) && g.move_remaining==12 && g.walking);
    ticks(&g,3,0);
    CHECK(at(&g,184,168) && !g.move_remaining);
    ticks(&g,10,0); CHECK(at(&g,184,168) && !g.walking);
    CHECK(g.save.steps==1 && g.save.facing==2);

    /* Simultaneous cardinal buttons never make a diagonal or a wall slide. */
    g=fresh();
    for(i=0;i<12;i++) {
        int x=g.save.player_x,y=g.save.player_y;
        input(&g,GK_RIGHT|GK_UP,i?0:GK_RIGHT);
        CHECK((g.save.player_x==x) != (g.save.player_y==y));
    }
    CHECK(g.save.player_y==168);
    blocked_world.solids=wall; blocked_world.solid_count=1;
    g=fresh(); game_set_world(&g,&blocked_world);
    input(&g,GK_RIGHT|GK_UP,GK_RIGHT);
    CHECK(at(&g,168,168) && !g.move_remaining && !g.walking);
    CHECK(g.facing==2 && !g.save.steps);
    /* Preflight checks the swept foot box, including sub-tile obstacles. */
    blocked_world.solids=thin_wall;
    g=fresh(); game_set_world(&g,&blocked_world);
    input(&g,GK_RIGHT,GK_RIGHT);
    CHECK(at(&g,168,168) && !g.move_remaining);

    /* A newly pressed turn is retained until the current tile finishes. */
    g=fresh(); input(&g,GK_RIGHT,GK_RIGHT);
    input(&g,GK_UP,GK_UP);
    CHECK(at(&g,176,168) && g.facing==2);
    ticks(&g,2,0);
    CHECK(at(&g,184,168) && !g.move_remaining);
    input(&g,0,0);
    CHECK(at(&g,184,164) && g.facing==3);
    ticks(&g,3,0);
    CHECK(at(&g,184,152) && !g.move_remaining && g.save.steps==2);

    /* Run speed is twice walking speed and is fixed for each started tile. */
    walking=fresh(); running=fresh();
    ticks(&walking,8,GK_RIGHT); ticks(&running,8,GK_RIGHT|GK_B);
    CHECK(walking.save.player_x-168==32);
    CHECK(running.save.player_x-168==64);
    CHECK(!walking.move_remaining && !running.move_remaining);
    g=fresh(); input(&g,GK_RIGHT,GK_RIGHT);
    input(&g,GK_RIGHT|GK_B,0);
    CHECK(at(&g,176,168) && g.move_speed==4);
    ticks(&g,2,0); CHECK(at(&g,184,168));

    /* Select freezes even a partial step. D-pad and shoulder input navigate
     * the notebook; returning resumes exactly that step without a teleport. */
    g=fresh(); input(&g,GK_RIGHT,GK_RIGHT);
    input(&g,0,GK_SELECT);
    CHECK(g.ui_focus && g.ui_tab==0 && at(&g,172,168));
    CHECK(g.move_remaining==12);
    ticks(&g,30,GK_DOWN);
    CHECK(at(&g,172,168) && g.move_remaining==12);
    input(&g,0,GK_R); CHECK(g.ui_focus && g.ui_tab==1);
    input(&g,0,GK_SELECT); CHECK(!g.ui_focus && g.mode==GM_FIELD);
    ticks(&g,3,0); CHECK(at(&g,184,168) && !g.move_remaining);
    /* Changing focus cancels older intentions, while retaining the physical
     * step. A menu queued before opening the notebook must not reopen later. */
    g=fresh(); input(&g,GK_RIGHT,GK_RIGHT);
    input(&g,0,GK_START);
    CHECK(g.action_queued && at(&g,176,168));
    input(&g,0,GK_SELECT);
    CHECK(g.ui_focus && !g.action_queued && g.move_remaining==8);
    input(&g,0,GK_SELECT); ticks(&g,6,0);
    CHECK(g.mode==GM_FIELD && !g.ui_focus && at(&g,184,168));
    CHECK(!g.move_remaining && !g.action_queued);
    g=fresh(); input(&g,GK_RIGHT,GK_RIGHT);
    input(&g,GK_UP,GK_UP);
    CHECK(g.move_queued==3 && at(&g,176,168));
    input(&g,0,GK_SELECT); CHECK(g.ui_focus && g.move_queued==4);
    input(&g,0,GK_SELECT); ticks(&g,6,0);
    CHECK(g.mode==GM_FIELD && at(&g,184,168) && !g.move_remaining);

    /* Menus wait for a safe tile boundary, then prevent further movement. */
    g=fresh(); input(&g,GK_RIGHT,GK_RIGHT);
    input(&g,0,GK_START);
    CHECK(g.mode==GM_FIELD && at(&g,176,168));
    ticks(&g,2,0);
    CHECK(g.mode==GM_MENU && at(&g,184,168) && !g.move_remaining);
    ticks(&g,20,GK_RIGHT); CHECK(at(&g,184,168));
    input(&g,0,GK_B); CHECK(g.mode==GM_FIELD);
    /* Touch input remains ignored in either screen focus. */
    {
        GameInput touch={0}; touch.touch_pressed=1; touch.touch_x=120; touch.touch_y=80;
        before=g; game_update(&g,&touch);
        CHECK(at(&g,before.save.player_x,before.save.player_y));
        CHECK(g.mode==before.mode && g.ui_focus==before.ui_focus);
    }

    /* A save made mid-step records its legal endpoint without changing the
     * live motion, and can be loaded independently into a fresh instance. */
    g=fresh(); input(&g,GK_RIGHT,GK_RIGHT); before=g;
    game_make_save(&g,&save);
    CHECK(!memcmp(&g,&before,sizeof(g)));
    CHECK(save.player_x==184 && save.player_y==168 && save.reserved[0]==2);
    CHECK(game_save_valid(&save));
    game_init(&loaded,2); game_set_world(&loaded,&open_world);
    CHECK(game_load(&loaded,&save));
    CHECK(at(&loaded,184,168) && !loaded.move_remaining);
    CHECK(loaded.has_save && loaded.mode==GM_TITLE);
    /* Existing companions, quest progress and supplies migrate intact; the
     * old free-position map coordinate returns to the rebuilt harbor. */
    memcpy(&legacy,original_release_save,sizeof(legacy));
    CHECK(game_save_valid(&legacy));
    game_init(&loaded,2); game_set_world(&loaded,&open_world);
    CHECK(game_load(&loaded,&legacy));
    expected=legacy; expected.player_x=open_world.spawn.x;
    expected.player_y=open_world.spawn.y; expected.reserved[0]=2;
    CHECK(!memcmp(&loaded.save,&expected,sizeof(expected)));
    CHECK(!loaded.move_remaining && loaded.has_save);
    game_make_save(&loaded,&save); CHECK(game_save_valid(&save));
    printf("Top-down movement: %u checks passed (cardinal tiles, swept collision, queued turns, run, notebook, safe saves).\n",checks);
    return 0;
}
