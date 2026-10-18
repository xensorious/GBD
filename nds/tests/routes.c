/* Every destination is reached through real game_update button movement.
 * The path planner uses the documented foot box only to choose a route. */
#include "game.h"
#include "world.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TILE 16
#define CENTER 8
#define GW (GAME_WORLD_W/TILE)
#define GH (GAME_WORLD_H/TILE)
#define CELLS (GW*GH)
static int previous[CELLS],queue[CELLS],path[CELLS];
static unsigned char free_cell[CELLS];

static int passable(int x,int y) {
    unsigned i;
    if(x<8 || y<8 || x>GAME_WORLD_W-8 || y>GAME_WORLD_H-8) return 0;
    for(i=0;i<aster_world.solid_count;i++) {
        const WorldSolid *r=&aster_world.solids[i];
        if(x+6>r->x && x-6<r->x+r->w && y+2>r->y && y-7<r->y+r->h) return 0;
    }
    for(i=0;i<GAME_NPC_COUNT;i++) {
        int dx=x-aster_world.npcs[i].x,dy=y-aster_world.npcs[i].y;
        if(dx*dx+dy*dy<144) return 0;
    }
    return 1;
}
static int step_clear(int x,int y,int dx,int dy) {
    int distance;
    for(distance=1;distance<=TILE;distance++)
        if(!passable(x+dx*distance,y+dy*distance)) return 0;
    return 1;
}
static Game fresh(void) {
    Game g;
    game_init(&g,1); game_set_world(&g,&aster_world);
    g.mode=GM_FIELD; g.save.flags=PF_STARTED|PF_CHOSEN;
    g.save.party_count=1; g.save.party[0].level=1;
    g.save.party[0].hp=(unsigned)game_max_hp(&g.save.party[0]);
    g.save.party[0].energy=3;
    return g;
}
int main(void) {
    const int dx[4]={-1,1,0,0},dy[4]={0,0,-1,1};
    const unsigned keys[4]={GK_LEFT,GK_RIGHT,GK_UP,GK_DOWN};
    const int targets[14]={0,1,2,3,10,11,12,13,14,15,20,21,22,30};
    int head=0,tail=0,start=(aster_world.spawn.y/TILE)*GW+aster_world.spawn.x/TILE;
    unsigned i;
    Game base=fresh();
    for(i=0;i<CELLS;i++) {
        int x=(int)(i%GW)*TILE+CENTER,y=(int)(i/GW)*TILE+CENTER;
        free_cell[i]=(unsigned char)passable(x,y); previous[i]=-1;
    }
    assert(free_cell[start]);
    assert(aster_world.spawn.x%TILE==CENTER && aster_world.spawn.y%TILE==CENTER);
    queue[tail++]=start; previous[start]=start;
    while(head<tail) {
        int cell=queue[head++],dir;
        for(dir=0;dir<4;dir++) {
            int x=cell%GW+dx[dir],y=cell/GW+dy[dir],next=y*GW+x;
            if(x<0 || y<0 || x>=GW || y>=GH) continue;
            if(free_cell[next] && previous[next]<0 &&
               step_clear((cell%GW)*TILE+CENTER,(cell/GW)*TILE+CENTER,dx[dir],dy[dir])) {
                previous[next]=cell; queue[tail++]=next;
            }
        }
    }
    for(i=0;i<14;i++) {
        int j,goal=-1,length=0;
        Game g=base;
        for(j=0;j<tail;j++) {
            int cell=queue[j];
            g.save.player_x=(short)((cell%GW)*TILE+CENTER);
            g.save.player_y=(short)((cell/GW)*TILE+CENTER);
            if(game_nearest_interaction(&g)==targets[i]) { goal=cell; break; }
        }
        if(goal<0) { fprintf(stderr,"Unreachable interaction %d\n",targets[i]); return 1; }
        for(j=goal;j!=start;j=previous[j]) path[length++]=j;
        g=base;
        for(j=length-1;j>=0;j--) {
            int target=path[j],tx=(target%GW)*TILE+CENTER,ty=(target/GW)*TILE+CENTER;
            GameInput input={0};
            int direction=tx<g.save.player_x?0:tx>g.save.player_x?1:ty<g.save.player_y?2:3;
            int frame;
            input.held=keys[direction];
            for(frame=0;frame<4;frame++) game_update(&g,&input);
            assert(g.save.player_x==tx && g.save.player_y==ty);
            assert(g.move_remaining==0);
        }
        assert(game_nearest_interaction(&g)==targets[i]);
        printf("Route %2d: %d completed cardinal tiles (%d movement ticks)\n",targets[i],length,length*4);
    }
    /* The defeat recovery point is also on traversable, connected ground. */
    {
        int x=aster_world.camp.x,y=aster_world.camp.y+32;
        assert(passable(x,y));
        assert(x%TILE==CENTER && y%TILE==CENTER);
        assert(previous[(y/TILE)*GW+x/TILE]>=0);
    }
    printf("World routes: all 14 interactions and safe recovery reachable (%d cells).\n",tail);
    return 0;
}
