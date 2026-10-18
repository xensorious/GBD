/* Traffic safety checks run the actual fixed-pool NPC update for long mixed
 * speed runs. This is deliberately independent of player damage/economy. */
#include "traffic.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned long long checks;
#define CHECK(c) do { ++checks; if(!(c)) { fprintf(stderr,"traffic:%d: %s\n",__LINE__,#c); exit(1); } } while(0)
static int absi(int n) { return n<0?-n:n; }
static unsigned near_count,damage_count;
static void hit(Game *g,int raw,int rail) { (void)g;CHECK(raw>=43 && !rail);++damage_count; }
static void award(Game *g,int category,uint32_t count,uint32_t amount) {
 (void)g;CHECK(category==B_NEAR && count==1 && amount==100);++near_count;
}
static Traffic car(int lane,int z,int speed) {
 Traffic t;memset(&t,0,sizeof t);t.active=1;t.x_q8=(-72+48*lane)*Q;
 t.z_q8=z*Q;t.speed_q8=t.cruise_q8=speed*Q;t.lane=t.target_lane=(uint8_t)lane;
 return t;
}
static void separation(const Game *g) {
 for(int i=0;i<TRAFFIC_COUNT;++i) {
  const Traffic *t=&g->traffic[i];if(!t->active) continue;
  CHECK(t->x_q8>=-75*Q && t->x_q8<=75*Q);
  CHECK(t->lane<4 && t->target_lane<4);
  CHECK(t->speed_q8>=0 && t->speed_q8<=t->cruise_q8);
  for(int j=i+1;j<TRAFFIC_COUNT;++j) {
   const Traffic *other=&g->traffic[j];if(!other->active) continue;
   if(absi(t->x_q8-other->x_q8)<35*Q) {
    if(absi(t->z_q8-other->z_q8)<TRAFFIC_MIN_GAP) {
     fprintf(stderr,"overlap: slots %d/%d z=%d/%d x=%d/%d seed=%u frame=%u\n",
      i,j,t->z_q8,other->z_q8,t->x_q8,other->x_q8,g->run_seed,g->frame);
    }
    CHECK(absi(t->z_q8-other->z_q8)>=TRAFFIC_MIN_GAP);
   }
  }
 }
}
static void interactions(void) {
 Game g;memset(&g,0,sizeof g);g.traffic[0]=car(2,1,0);
 g.traffic[0].x_q8=25*Q;g.speed_q8=150*Q;g.position_q8=5*Q;
 near_count=damage_count=0;traffic_tick(&g,0,hit,award);
 CHECK(near_count==1 && !damage_count && g.traffic[0].near_awarded);
 for(int i=0;i<10;++i) traffic_tick(&g,0,hit,award);
 CHECK(near_count==1 && !damage_count);
 memset(&g,0,sizeof g);g.traffic[0]=car(2,1,0);g.traffic[0].x_q8=0;
 g.speed_q8=150*Q;g.position_q8=5*Q;near_count=damage_count=0;
 traffic_tick(&g,0,hit,award);CHECK(damage_count==1 && !near_count);
 CHECK(g.traffic[0].near_awarded);
 /* The close chase camera must not let the visible bodies ghost through
  * one another before impact. Check both ordinary and long traffic shapes. */
 for(int kind=0;kind<6;++kind) {
  int length=kind>=4?70:58;
  for(int edge=-1;edge<=1;++edge) {
   memset(&g,0,sizeof g);g.traffic[0]=car(2,length+edge,0);
   g.traffic[0].kind=(uint8_t)kind;g.traffic[0].x_q8=0;g.speed_q8=150*Q;
   near_count=damage_count=0;traffic_tick(&g,0,hit,award);
   CHECK(damage_count==(unsigned)(edge<0) && !near_count);
  }
 }
}
static void pool_fairness(void) {
 Game g;memset(&g,0,sizeof g);g.run_seed=0x26473a71u;g.next_spawn_q8=200*Q;
 /* A depleted local view must not stay empty merely because invisible
  * forward traffic occupies every slot in the bounded pool. */
 for(int i=0;i<TRAFFIC_COUNT;++i) g.traffic[i]=car(i%4,ROAD_VIEW+800+180*(i/4),100);
 traffic_spawn_ahead(&g);
 int visible=0;
 for(int i=0;i<TRAFFIC_COUNT;++i) if(g.traffic[i].active && g.traffic[i].z_q8<=ROAD_VIEW*Q) ++visible;
 CHECK(visible>=4);separation(&g);
 /* Conversely, a full visible highway keeps every existing vehicle. */
 memset(&g,0,sizeof g);g.run_seed=7;g.next_spawn_q8=200*Q;
 for(int i=0;i<TRAFFIC_COUNT;++i) g.traffic[i]=car(i%4,100+180*(i/4),100);
 Traffic before[TRAFFIC_COUNT];memcpy(before,g.traffic,sizeof before);
 traffic_spawn_ahead(&g);CHECK(!memcmp(before,g.traffic,sizeof before));
}
static void following(void) {
 Game g;memset(&g,0,sizeof g);g.x_q8=90*Q;
 /* The slow leader deliberately occupies the last storage slot. */
 g.traffic[19]=car(1,1100,80);
 for(int i=0;i<5;++i) g.traffic[i]=car(1,920-i*180,125);
 int minimum=INT_MAX,saw_braking=0;
 for(int tick=0;tick<8000;++tick) {
  Traffic before[TRAFFIC_COUNT];memcpy(before,g.traffic,sizeof before);
  int32_t old=g.position_q8;traffic_tick(&g,old,NULL,NULL);
  for(int i=0;i<TRAFFIC_COUNT;++i) if(g.traffic[i].active) {
   CHECK(g.traffic[i].z_q8>=before[i].z_q8);
   CHECK(g.traffic[i].speed_q8>=before[i].speed_q8-Q/2);
   CHECK(g.traffic[i].speed_q8<=before[i].speed_q8+Q/6);
   if(g.traffic[i].speed_q8<before[i].speed_q8) saw_braking=1;
  }
  separation(&g);
  for(int i=0;i<4;++i) {
   int gap=g.traffic[i].z_q8-g.traffic[i+1].z_q8;if(gap<minimum) minimum=gap;
  }
  g.position_q8=g.traffic[4].z_q8-40*Q;
 }
 CHECK(saw_braking && minimum>=TRAFFIC_MIN_GAP);
 for(int i=0;i<5;++i) {
  CHECK(g.traffic[i].active && g.traffic[i].cruise_q8==125*Q);
  CHECK(absi(g.traffic[i].speed_q8-80*Q)<Q);
 }
}
static void horn(void) {
 Game g;memset(&g,0,sizeof g);g.traffic[0]=car(1,180,90);
 g.x_q8=-24*Q;g.horn_timer=30;
 traffic_tick(&g,0,NULL,NULL);
 CHECK(g.traffic[0].target_lane==0 && g.traffic[0].lane==1);
 CHECK(g.traffic[0].x_q8==-24*Q-3*Q/4);
 g.horn_timer=0;
 for(int tick=0;tick<80;++tick) traffic_tick(&g,0,NULL,NULL);
 CHECK(g.traffic[0].lane==0 && g.traffic[0].target_lane==0 && g.traffic[0].x_q8==-72*Q);
 memset(&g,0,sizeof g);g.traffic[0]=car(1,180,90);
 g.traffic[1]=car(0,180,90);g.traffic[2]=car(2,180,90);
 g.x_q8=-24*Q;g.horn_timer=30;traffic_tick(&g,0,NULL,NULL);
 CHECK(g.traffic[0].lane==1 && g.traffic[0].target_lane==1);
 /* A fast car approaching in the destination lane also prevents merging. */
 memset(&g,0,sizeof g);g.traffic[0]=car(1,220,80);
 g.traffic[1]=car(0,10,125);g.traffic[2]=car(2,220,80);
 g.x_q8=-24*Q;g.horn_timer=30;traffic_tick(&g,0,NULL,NULL);
 CHECK(g.traffic[0].lane==1 && g.traffic[0].target_lane==1);
 separation(&g);
}
static void drive_tick(Game *g,int tick) {
 static const int speeds[8]={0,50,120,200,280,90,-28,240};
 int speed=speeds[(tick/400)&7];
 int32_t old=g->position_q8;g->speed_q8=speed*Q;g->position_q8+=g->speed_q8/32;
 if(g->position_q8<0) g->position_q8=0;
 if(g->position_q8>g->furthest_q8) g->furthest_q8=g->position_q8;
 g->x_q8=(-72+48*((tick/173)&3))*Q;
 g->horn_timer=(tick%311)<30?30:0;
 traffic_tick(g,old,NULL,NULL);traffic_spawn_ahead(g);++g->frame;
 for(int i=0;i<PICKUP_COUNT;++i) if(g->pickups[i].active &&
   (g->pickups[i].z_q8<g->position_q8-120*Q || g->pickups[i].z_q8>g->position_q8+3*ROAD_VIEW*Q))
  g->pickups[i].active=0;
}
static void long_runs(void) {
 for(uint32_t seed=1;seed<=32;++seed) {
  Game a,b;memset(&a,0,sizeof a);a.run_seed=seed*0x9e3779b9u;a.next_spawn_q8=200*Q;
  traffic_spawn_ahead(&a);b=a;
  for(int tick=0;tick<12000;++tick) {
   Traffic before[TRAFFIC_COUNT];memcpy(before,a.traffic,sizeof before);
   drive_tick(&a,tick);drive_tick(&b,tick);
   CHECK(!memcmp(a.traffic,b.traffic,sizeof a.traffic));
   CHECK(!memcmp(a.pickups,b.pickups,sizeof a.pickups));
   CHECK(a.next_spawn_q8==b.next_spawn_q8 && a.spawn_serial==b.spawn_serial);
   separation(&a);
   for(int i=0;i<TRAFFIC_COUNT;++i) if(a.traffic[i].active) {
    const Traffic *t=&a.traffic[i];
    CHECK(t->cruise_q8>=80*Q && t->cruise_q8<=125*Q);
    CHECK(t->kind<6 && t->paint<5);
    if(before[i].active && before[i].serial==t->serial) {
     CHECK(t->z_q8>=before[i].z_q8);
     CHECK(t->cruise_q8==before[i].cruise_q8);
     CHECK(t->speed_q8>=before[i].speed_q8-Q/2);
     CHECK(t->speed_q8<=before[i].speed_q8+Q/6);
    }
   }
  }
 }
}
int main(void) {
 interactions();following();horn();pool_fairness();long_runs();
 printf("traffic: %llu checks; deterministic spawning, 384,000 mixed-speed frames, minimum headway, smooth following, safe horn changes and one-time near misses pass\n",checks);
 return 0;
}
