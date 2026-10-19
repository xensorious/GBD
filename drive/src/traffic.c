#include "traffic.h"
#include "road.h"
#include "vehicle_projection.h"
#include "vehicle_geometry.h"
#include <limits.h>
#include <string.h>
#ifdef ARM9
#include <nds/arm9/math.h>
#endif

#define WORLD_LIMIT (7000000*Q)
#define LANE_CHANGE_STEP (3*Q/4)
#define TRAFFIC_ACCEL (Q/6)
#define TRAFFIC_BRAKE (Q/2)

_Static_assert(sizeof vehicle_body_spans/sizeof vehicle_body_spans[0]==VEHICLE_COUNT*5,
               "Collision poses must cover the complete vehicle roster");
_Static_assert(sizeof vehicle_body_masks/sizeof vehicle_body_masks[0]==VEHICLE_COUNT*5,
               "Collision masks must cover the complete vehicle roster");

static int absi(int n) { return n<0?-n:n; }
static int mini(int a,int b) { return a<b?a:b; }
static int maxi(int a,int b) { return a>b?a:b; }
static int lane_x(int lane) { return (-72+48*lane)*Q; }
/* Gameplay and rendering run sequentially; IRQ handlers do not use the math
 * unit. Native builds retain the same signed, truncating C quotients. */
static __attribute__((noinline,noclone,const)) int traffic_divide(int numerator,int denominator) {
#ifdef ARM9
 return div32(numerator,denominator);
#else
 return numerator/denominator;
#endif
}
static int interpolate(int from,int to,int step,int steps) {
 int64_t numerator=((int64_t)to-from)*step;
 if(numerator>=INT32_MIN && numerator<=INT32_MAX)
  return from+traffic_divide((int32_t)numerator,steps);
 /* Preserve the original wide arithmetic for extreme synthetic fixtures. */
 return from+(int32_t)(numerator/steps);
}
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

typedef struct {
 VehicleScreenRect rect;
 const uint8_t (*rows)[2];
 const uint32_t (*masks)[2];
 int source_width,source_height;
 unsigned column_step;
} ContactShape;

static void contact_shapes(const Game *g,const Traffic *t,ContactShape *player,ContactShape *npc) {
 int z=(t->z_q8-g->position_q8)/Q;
 *player=(ContactShape){0};*npc=(ContactShape){0};
 /* Traffic outside the renderer's near/far planes has no visible body. */
 if(!t->active || z<DRIVE_NEAR_Z || z>ROAD_VIEW)return;
 int vehicle=g->vehicle<VEHICLE_COUNT?g->vehicle:0;
 int kind=t->kind%6;
 int bike=vehicles[vehicle].bike;
 int cam_x=g->x_q8/(Q*8);
 if(g->save.shake && g->shake) {
  int shake=mini(g->shake/4,2);
  cam_x+=(g->frame&1)?shake:-shake;
 }
 RoadSample here,ahead;
 road_sample(g->run_seed,g->position_q8/Q,g->start_biome,&here);
 road_sample(g->run_seed,t->z_q8/Q,g->start_biome,&ahead);
 int player_x=vehicle_project_x(here.center+g->x_q8/Q,here.center,cam_x,0);
 int scale=traffic_divide(276*DRIVE_CAMERA,z+DRIVE_CAMERA);
 int traffic_x=DRIVE_SCREEN_WIDTH/2+((ahead.center+t->x_q8/Q-here.center-cam_x)*scale)/256;
 int size=scale*180/256,w=DRIVE_TRAFFIC_ART_W*size/256,h=DRIVE_TRAFFIC_ART_H*size/256;
 int y=DRIVE_HORIZON+traffic_divide((DRIVE_PLAYER_Y-DRIVE_HORIZON)*DRIVE_CAMERA,z+DRIVE_CAMERA);
 int lift=g->wheelie?3+(g->frame/6&1):0;
 *player=(ContactShape){vehicle_player_rect(player_x,bike,lift),
                       vehicle_body_spans[vehicle*5+vehicle_yaw_pose(g->yaw)],
                       vehicle_body_masks[vehicle*5+vehicle_yaw_pose(g->yaw)],
                       DRIVE_PLAYER_ART_W,DRIVE_PLAYER_ART_H,0};
 VehicleScreenRect npc_rect={traffic_x-w/2,y-h/2,w,h};
 *npc=(ContactShape){npc_rect,traffic_body_spans[kind],
                    traffic_body_masks[kind],DRIVE_TRAFFIC_ART_W,DRIVE_TRAFFIC_ART_H,0};
 if(player->rect.width>0)player->column_step=(unsigned)traffic_divide(player->source_width*65536+player->rect.width-1,player->rect.width);
 if(npc->rect.width>0)npc->column_step=(unsigned)traffic_divide(npc->source_width*65536+npc->rect.width-1,npc->rect.width);
}

/* Match gfx_vehicle's nearest-neighbour source lookup exactly. Per-row opaque
 * intervals come from the same cleaned RGB555 artwork at build time. */
static int body_row(const ContactShape *shape,int y,int *left,int *right) {
 const VehicleScreenRect *r=&shape->rect;
 if(!shape->rows || r->width<=0 || r->height<=0 || y<r->top || y>=r->top+r->height)return 0;
 int source_y=(y-r->top)*shape->source_height/r->height;
 const uint8_t *row=shape->rows[source_y];
 if(row[0]>=row[1])return 0;
 *left=maxi(0,r->left+(row[0]*r->width+shape->source_width-1)/shape->source_width);
 *right=mini(DRIVE_SCREEN_WIDTH,r->left+(row[1]*r->width+shape->source_width-1)/shape->source_width);
 return *left<*right;
}

static int body_pixel(const ContactShape *shape,const uint32_t *mask,int x) {
 unsigned column=((unsigned)(x-shape->rect.left)*shape->column_step)>>16;
 return (int)((mask[column>>5]>>(column&31))&1u);
}

static int shapes_contact(const ContactShape *player,const ContactShape *npc) {
 /* Opaque bodies cannot overlap by two pixels if their complete projected
  * rectangles do not. Adjacent lanes therefore avoid per-row mask sampling. */
 int left=maxi(0,maxi(player->rect.left,npc->rect.left));
 int right=mini(DRIVE_SCREEN_WIDTH,mini(player->rect.left+player->rect.width,
                                      npc->rect.left+npc->rect.width));
 if(right-left<2)return 0;
 int top=maxi(0,maxi(player->rect.top,npc->rect.top));
 int bottom=mini(DRIVE_SCREEN_HEIGHT,mini(player->rect.top+player->rect.height,
                                          npc->rect.top+npc->rect.height));
 int touching_rows=0;
 for(int y=top;y<bottom;++y) {
  int pl,pr,nl,nr;
  if(body_row(player,y,&pl,&pr) && body_row(npc,y,&nl,&nr) &&
     mini(pr,nr)-maxi(pl,nl)>=2) {
   const uint32_t *pm=player->masks[(y-player->rect.top)*player->source_height/player->rect.height];
   const uint32_t *nm=npc->masks[(y-npc->rect.top)*npc->source_height/npc->rect.height];
   int run=0,hit=0;
   for(int x=maxi(pl,nl);x<mini(pr,nr);++x) {
    if(body_pixel(player,pm,x) && body_pixel(npc,nm,x)) {if(++run>=2){hit=1;break;}}
    else run=0;
   }
   /* One-pixel edge contact, mirrors and lone corner pixels are forgiving;
    * two adjacent rows of actual chassis overlap constitute an impact. */
   if(hit) {if(++touching_rows>=2)return 1;}
   else touching_rows=0;
  } else touching_rows=0;
 }
 return 0;
}

int traffic_body_contact(const Game *g,const Traffic *t) {
 ContactShape player,npc;contact_shapes(g,t,&player,&npc);
 return shapes_contact(&player,&npc);
}

int traffic_body_gap(const Game *g,const Traffic *t) {
 ContactShape player,npc;contact_shapes(g,t,&player,&npc);
 int top=maxi(0,maxi(player.rect.top,npc.rect.top)),gap=DRIVE_SCREEN_WIDTH;
 int bottom=mini(DRIVE_SCREEN_HEIGHT,mini(player.rect.top+player.rect.height,
                                          npc.rect.top+npc.rect.height));
 for(int y=top;y<bottom;++y) {
  int pl,pr,nl,nr;
  if(body_row(&player,y,&pl,&pr) && body_row(&npc,y,&nl,&nr)) {
   int row_gap=maxi(pl,nl)-mini(pr,nr);
   if(row_gap>=0) {if(row_gap<gap)gap=row_gap;continue;}
   const uint32_t *pm=player.masks[(y-player.rect.top)*player.source_height/player.rect.height];
   const uint32_t *nm=npc.masks[(y-npc.rect.top)*npc.source_height/npc.rect.height];
   int last_player=-DRIVE_SCREEN_WIDTH,last_npc=-DRIVE_SCREEN_WIDTH;
   for(int x=mini(pl,nl);x<maxi(pr,nr);++x) {
    int p=x>=pl && x<pr && body_pixel(&player,pm,x);
    int n=x>=nl && x<nr && body_pixel(&npc,nm,x);
    if(p && n)return -1;
    if(p) {if(last_npc>=0 && x-last_npc-1<gap)gap=x-last_npc-1;last_player=x;}
    if(n) {if(last_player>=0 && x-last_player-1<gap)gap=x-last_player-1;last_npc=x;}
   }
  }
 }
 return gap;
}

static int swept_contact(const Game *g,const Traffic *t,int32_t old_position,
                         int32_t old_x,int old_yaw,int32_t old_z,int32_t old_traffic_x) {
 int old_dz=old_z-old_position,new_dz=t->z_q8-g->position_q8;
 if(mini(old_dz,new_dz)>128*Q || maxi(old_dz,new_dz)<=(DRIVE_NEAR_Z-1)*Q)return 0;
 int old_dx=old_traffic_x-old_x,new_dx=t->x_q8-g->x_q8;
 if((old_dx>90*Q && new_dx>90*Q) || (old_dx < -90*Q && new_dx < -90*Q))return 0;
 /* Sample at sub-pixel lateral and roughly pixel-sized depth intervals. Even
  * the fastest upgraded turbo car cannot jump across a chassis in one tick.
  * The cap also keeps deliberately extreme fixtures bounded. */
 int steps=1+absi(new_dz-old_dz)/Q+2*absi(new_dx-old_dx)/Q+absi(g->yaw-old_yaw)/16;
 if(steps>64)steps=64;
 Game sample;
 /* Only fields read by contact_shapes are needed; avoid copying the entire
  * traffic/pickup pool once per candidate or per interpolation sample. */
 memset(&sample,0,sizeof sample);
 sample.vehicle=g->vehicle;sample.run_seed=g->run_seed;sample.start_biome=g->start_biome;
 sample.wheelie=g->wheelie;sample.frame=g->frame;
 sample.save.shake=g->save.shake;sample.shake=g->shake;
 Traffic moving=*t;
 for(int step=0;step<=steps;++step) {
  sample.position_q8=interpolate(old_position,g->position_q8,step,steps);
  sample.x_q8=interpolate(old_x,g->x_q8,step,steps);
  sample.yaw=(int16_t)interpolate(old_yaw,g->yaw,step,steps);
  moving.z_q8=interpolate(old_z,t->z_q8,step,steps);
  moving.x_q8=interpolate(old_traffic_x,t->x_q8,step,steps);
  if(traffic_body_contact(&sample,&moving))return 1;
 }
 return 0;
}

void traffic_tick_from(Game *g,int32_t old_position,int32_t old_x,int old_yaw,
                       TrafficDamage damage,TrafficPoints points) {
 int order[TRAFFIC_COUNT],count=0;
 int32_t before[TRAFFIC_COUNT],before_x[TRAFFIC_COUNT];
 /* Snapshot pass positions before any movement. Culling and ordering are
  * independent of storage slot reuse, so a follower always sees the leader's
  * new position and reservations, even when the leader has a higher slot. */
 for(int i=0;i<TRAFFIC_COUNT;++i) {
  Traffic *t=&g->traffic[i]; before[i]=t->z_q8-old_position;before_x[i]=t->x_q8;
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
  if(swept_contact(g,t,old_position,old_x,old_yaw,before[i]+old_position,before_x[i])) {
   t->near_awarded=1;
   if(damage) damage(g,43+absi(g->speed_q8-t->speed_q8)/(10*Q),0);
  }
  if(!t->near_awarded && before[i]>0 && dz<=0) {
   t->near_awarded=1;
   int gap=traffic_body_gap(g,t);
   if(gap>=-1 && gap<=18 && g->speed_q8>t->speed_q8 && g->speed_q8>40*Q && points)
    points(g,B_NEAR,1,100);
  }
 }
}

void traffic_tick(Game *g,int32_t old_position,TrafficDamage damage,TrafficPoints points) {
 traffic_tick_from(g,old_position,g->x_q8,g->yaw,damage,points);
}
