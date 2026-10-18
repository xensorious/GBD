#include "traffic.h"
#include "road.h"
#include <string.h>

#define WORLD_LIMIT (7000000*Q)
#define LANE_CHANGE_STEP (3*Q/4)
#define TRAFFIC_ACCEL (Q/6)
#define TRAFFIC_BRAKE (Q/2)

static int absi(int n) { return n<0?-n:n; }
static int lane_x(int lane) { return (-72+48*lane)*Q; }
static int free_traffic(const Game *g) {
 for(int i=0;i<TRAFFIC_COUNT;++i) if(!g->traffic[i].active) return i;
 /* Slow driving can leave the fixed pool full of cars far ahead of the
  * camera. Recycle the farthest invisible one rather than starving new rows;
  * a visible vehicle is never removed to make room for another spawn. */
 int best=-1;int32_t far=g->position_q8+(ROAD_VIEW+96)*Q;
 for(int i=0;i<TRAFFIC_COUNT;++i) if(g->traffic[i].z_q8>far) {
  best=i;far=g->traffic[i].z_q8;
 }
 return best;
}
static int free_pickup(const Game *g) { for(int i=0;i<PICKUP_COUNT;++i) if(!g->pickups[i].active) return i; return -1; }
static int reserves(const Traffic *t,int lane) { return t->lane==lane || t->target_lane==lane; }
static int shared_path(const Traffic *a,const Traffic *b) {
 return reserves(a,b->lane) || reserves(a,b->target_lane) || absi(a->x_q8-b->x_q8)<35*Q;
}

static int spawn_safe(const Game *g,int lane,int32_t z) {
 for(int i=0;i<TRAFFIC_COUNT;++i) {
  const Traffic *t=&g->traffic[i];
  if(t->active && (reserves(t,lane) || absi(t->x_q8-lane_x(lane))<35*Q) &&
     absi(t->z_q8-z)<TRAFFIC_SPAWN_GAP) return 0;
 }
 return 1;
}

static void spawn_row(Game *g,int32_t z) {
 uint32_t h=road_hash(g->run_seed,(uint32_t)g->spawn_serial+0x4157u);
 int first=(int)(h&3u),step=(h&16u)?3:1;
 int n=((h>>9)&3u)?2:1;
 unsigned occupied=0;
 for(int j=0;j<n;++j) {
  int slot=free_traffic(g); if(slot<0) break;
  uint32_t car=road_hash(h,(uint32_t)j+97u);
  /* Stagger side-by-side arrivals so the highway does not resemble a series
   * of artificial walls. A blocked lane is skipped, never squeezed into. */
  int32_t car_z=z+(j*64+(int)(car%25u))*Q;
  int lane=-1;
  for(int attempt=0;attempt<4;++attempt) {
   int candidate=(first+(j+attempt)*step)&3;
   if(!(occupied&(1u<<candidate)) && spawn_safe(g,candidate,car_z)) { lane=candidate; break; }
  }
  if(lane<0) continue;
  Traffic *t=&g->traffic[slot]; memset(t,0,sizeof(*t));
  int jitter=(int)((car>>5)%7u)-3;
  t->x_q8=lane_x(lane)+jitter*Q; t->z_q8=car_z;
  t->speed_q8=t->cruise_q8=(80+(int)((car>>11)%46u))*Q;
  t->kind=(uint8_t)((car>>18)%6u); t->paint=(uint8_t)((car>>23)%5u);
  t->serial=g->spawn_serial; t->active=1;
  t->lane=t->target_lane=(uint8_t)lane;
  occupied|=1u<<lane;
 }
 int coin_lane=(int)((h>>17)&3u);
 for(int tries=0;tries<4 && (occupied&(1u<<coin_lane));++tries) coin_lane=(coin_lane+1)&3;
 for(int j=0;j<3;++j) {
  int slot=free_pickup(g); if(slot<0) break;
  Pickup *p=&g->pickups[slot]; memset(p,0,sizeof(*p));
  p->x_q8=lane_x(coin_lane); p->z_q8=z-(60+j*24)*Q;
  p->serial=g->spawn_serial; p->active=1;
 }
 ++g->spawn_serial;
}

void traffic_spawn_ahead(Game *g) {
 while(g->next_spawn_q8<=g->furthest_q8+ROAD_VIEW*Q && g->next_spawn_q8<WORLD_LIMIT) {
  spawn_row(g,g->next_spawn_q8);
  uint32_t h=road_hash(g->run_seed,(uint32_t)g->spawn_serial+0xaceu);
  g->next_spawn_q8+=(170+(int)(h%61u))*Q;
 }
}

static int lane_change_safe(const Game *g,int index,int lane) {
 const Traffic *t=&g->traffic[index];
 if(lane<0 || lane>3) return 0;
 for(int j=0;j<TRAFFIC_COUNT;++j) {
  const Traffic *other=&g->traffic[j];
  if(j==index || !other->active) continue;
  if(!reserves(other,lane) && absi(other->x_q8-lane_x(lane))>=35*Q) continue;
  /* Enough headway for the entire ~two-second lane change, including a
   * faster car approaching from behind. Source and target stay reserved. */
  int gap=TRAFFIC_MIN_GAP+60*Q+2*absi(t->speed_q8-other->speed_q8);
  if(absi(other->z_q8-t->z_q8)<gap) return 0;
 }
 return 1;
}

static void consider_horn(Game *g,int index) {
 Traffic *t=&g->traffic[index];
 if(t->lane!=t->target_lane || !g->horn_timer) return;
 int32_t dz=t->z_q8-g->position_q8;
 if(dz<=65*Q || dz>=240*Q || absi(t->x_q8-g->x_q8)>=26*Q) return;
 int dir=t->x_q8<g->x_q8?-1:t->x_q8>g->x_q8?1:t->x_q8<0?-1:1;
 int lane=(int)t->lane+dir;
 if(!lane_change_safe(g,index,lane)) lane=(int)t->lane-dir;
 if(lane_change_safe(g,index,lane)) t->target_lane=(uint8_t)lane;
}

void traffic_tick(Game *g,int32_t old_position,TrafficDamage damage,TrafficPoints points) {
 int order[TRAFFIC_COUNT],count=0;
 int32_t before[TRAFFIC_COUNT];
 /* Snapshot pass positions before any movement. Culling and ordering are
  * independent of storage slot reuse, so a follower always sees the leader's
  * new position and reservations, even when the leader has a higher slot. */
 for(int i=0;i<TRAFFIC_COUNT;++i) {
  Traffic *t=&g->traffic[i]; before[i]=t->z_q8-old_position;
  if(!t->active) continue;
  int32_t dz=t->z_q8-g->position_q8;
  if(dz < -120*Q || dz > 3*ROAD_VIEW*Q) { t->active=0; continue; }
  int at=count;
  while(at>0 && g->traffic[order[at-1]].z_q8<t->z_q8) { order[at]=order[at-1]; --at; }
  order[at]=i; ++count;
 }
 for(int at=0;at<count;++at) {
  int i=order[at]; Traffic *t=&g->traffic[i];
  consider_horn(g,i);
  int leader=-1; int32_t leader_z=0;
  for(int ahead=0;ahead<at;++ahead) {
   int j=order[ahead]; const Traffic *other=&g->traffic[j];
   if(!shared_path(t,other)) continue;
   if(leader<0 || other->z_q8<leader_z) { leader=j; leader_z=other->z_q8; }
  }
  int target=t->cruise_q8;
  if(leader>=0) {
   const Traffic *front=&g->traffic[leader];
   int32_t gap=front->z_q8-t->z_q8;
   int desired=130*Q+t->speed_q8/3;
   int allowed=front->speed_q8+(gap-desired)/2;
   if(allowed<target) target=allowed>0?allowed:0;
  }
  if(t->speed_q8<target) {
   t->speed_q8+=TRAFFIC_ACCEL; if(t->speed_q8>target) t->speed_q8=target;
  } else if(t->speed_q8>target) {
   t->speed_q8-=TRAFFIC_BRAKE; if(t->speed_q8<target) t->speed_q8=target;
  }
  int32_t advance=t->speed_q8/32;
  if(leader>=0) {
   /* A final swept-space guard also protects deliberately adversarial test
    * fixtures. Authored spawn spacing lets ordinary braking happen earlier. */
   int32_t available=leader_z-TRAFFIC_MIN_GAP-t->z_q8;
   if(available<0) available=0;
   if(advance>available) { advance=available; t->speed_q8=advance*32; }
  }
  t->z_q8+=advance;
  if(t->lane!=t->target_lane) {
   int target_x=lane_x(t->target_lane),delta=target_x-t->x_q8;
   if(absi(delta)<=LANE_CHANGE_STEP) { t->x_q8=target_x; t->lane=t->target_lane; }
   else t->x_q8+=delta>0?LANE_CHANGE_STEP:-LANE_CHANGE_STEP;
  }
 }
 /* Keep collision/near-miss accounting in the original slot order. */
 for(int i=0;i<TRAFFIC_COUNT;++i) {
  Traffic *t=&g->traffic[i]; if(!t->active) continue;
  int32_t dz=t->z_q8-g->position_q8;
  if(dz < -120*Q || dz > 3*ROAD_VIEW*Q) { t->active=0; continue; }
  int dx=absi(t->x_q8-g->x_q8);
  int half_width=t->kind>=4?19:17;
  /* These lengths match the close chase projection's visible silhouettes,
   * including the player body, rather than just the NPC centre point. */
  int half_length=t->kind>=4?TRAFFIC_LARGE_HALF_LENGTH:TRAFFIC_CAR_HALF_LENGTH;
  if(absi(dz)<half_length*Q && dx<half_width*Q) {
   t->near_awarded=1;
   if(damage) damage(g,43+absi(g->speed_q8-t->speed_q8)/(10*Q),0);
  }
  if(!t->near_awarded && before[i]>0 && dz<=0) {
   t->near_awarded=1;
   if(dx>=half_width*Q && dx<=36*Q && g->speed_q8>t->speed_q8 && g->speed_q8>40*Q && points)
    points(g,B_NEAR,1,100);
  }
 }
}
