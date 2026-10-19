/* Traffic safety checks run the actual fixed-pool NPC update for long mixed
 * speed runs. This is deliberately independent of player damage/economy. */
#include "traffic.h"
#include "road.h"
#include "vehicle_projection.h"
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

/* Pixel oracle loads the actual packed artwork, independently of the compact
 * contact metadata. It samples the same drawing rectangles as the renderer,
 * then looks for a two-by-two patch of overlapping opaque car pixels. */
static uint16_t player_pixels[VEHICLE_COUNT*5*48*72],npc_pixels[6*40*64];
static void read_pixels(const char *path,uint16_t *pixels,size_t count) {
 FILE *file=fopen(path,"rb");CHECK(file!=NULL);
 CHECK(fread(pixels,sizeof(*pixels),count,file)==count);CHECK(fgetc(file)==EOF);
 CHECK(fclose(file)==0);
}
static int opaque(const uint16_t *pixels,VehicleScreenRect r,int sw,int sh,int x,int y) {
 if(r.width<=0 || r.height<=0 || x<r.left || y<r.top || x>=r.left+r.width || y>=r.top+r.height)return 0;
 return pixels[((y-r.top)*sh/r.height)*sw+(x-r.left)*sw/r.width]!=0;
}
static int pixel_oracle(const Game *g,const Traffic *t) {
 int z=(t->z_q8-g->position_q8)/Q;if(z < -44 || z>ROAD_VIEW)return 0;
 RoadSample here,ahead;
 road_sample(g->run_seed,g->position_q8/Q,g->start_biome,&here);
 road_sample(g->run_seed,t->z_q8/Q,g->start_biome,&ahead);
 int cam_x=g->x_q8/(Q*8);
 if(g->save.shake && g->shake)cam_x+=(g->frame&1?1:-1)*(g->shake/4<2?g->shake/4:2);
 int px=128+((g->x_q8/Q-cam_x)*276)/256;
 int scale=276*190/(z+190);
 int nx=128+((ahead.center+t->x_q8/Q-here.center-cam_x)*scale)/256;
 int vehicle=g->vehicle<VEHICLE_COUNT?g->vehicle:0;
 int pose=vehicle_yaw_pose(g->yaw);
 VehicleScreenRect p=vehicle_player_rect(px,vehicles[vehicle].bike,g->wheelie?3+(g->frame/6&1):0);
 VehicleScreenRect n=vehicle_traffic_rect(nx,z);
 const uint16_t *pp=player_pixels+(vehicle*5+pose)*48*72,*np=npc_pixels+(t->kind%6)*40*64;
 int left=p.left>n.left?p.left:n.left,right=p.left+p.width<n.left+n.width?p.left+p.width:n.left+n.width;
 int top=p.top>n.top?p.top:n.top,bottom=p.top+p.height<n.top+n.height?p.top+p.height:n.top+n.height;
 if(left<0)left=0;
 if(right>256)right=256;
 if(top<0)top=0;
 if(bottom>192)bottom=192;
 int previous=0;
 for(int y=top;y<bottom;++y) {
  int run=0,hit=0;
  for(int x=left;x<right;++x) {
   if(opaque(pp,p,48,72,x,y) && opaque(np,n,40,64,x,y)) {if(++run>=2)hit=1;}
   else run=0;
  }
  if(hit && previous)return 1;
  previous=hit;
 }
 return 0;
}
static void visible_contacts(void) {
 read_pixels("build/vehicle_art.bin",player_pixels,sizeof player_pixels/sizeof *player_pixels);
 read_pixels("build/traffic_art.bin",npc_pixels,sizeof npc_pixels/sizeof *npc_pixels);
 static const int yaws[]={-100,-40,0,40,100};
 static const int depths[]={-44,-38,-30,-22,-14,-6,0,6,14,22,30,38,46,54,62,70,82,100};
 for(int vehicle=0;vehicle<VEHICLE_COUNT;++vehicle)for(int kind=0;kind<6;++kind)
 for(int yaw=0;yaw<5;++yaw)for(int depth=0;depth<(int)(sizeof depths/sizeof *depths);++depth)
 for(int x=-36;x<=36;x+=6) {
  Game g;memset(&g,0,sizeof g);g.run_seed=0x26473a71u;g.vehicle=(uint8_t)vehicle;
  g.position_q8=(1337+kind*819)*Q;g.x_q8=(vehicle%2?24:-24)*Q;g.yaw=(int16_t)yaws[yaw];
  g.save.shake=(uint8_t)(kind&1);g.shake=8;g.frame=(unsigned)yaw;
  g.wheelie=(uint8_t)(vehicles[vehicle].bike && (kind&1));
  Traffic t=car(1,0,0);t.z_q8=g.position_q8+depths[depth]*Q;t.x_q8=g.x_q8+x*Q;t.kind=(uint8_t)kind;
  int expected=pixel_oracle(&g,&t),actual=traffic_body_contact(&g,&t);
  if(expected!=actual)fprintf(stderr,"body mismatch vehicle=%d kind=%d yaw=%d z=%d dx=%d expected%d actual%d\n",
                            vehicle,kind,g.yaw,depths[depth],x,expected,actual);
  CHECK(expected==actual);
 }
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
 /* Rear/front impact ranges are asymmetric in this chase camera. Detect the
  * actual visible contact threshold separately for each model and approach;
  * the old fixed +/-58/70 box incorrectly penalized several clear cut-ins. */
 for(int kind=0;kind<6;++kind) {
  for(int dir=-1;dir<=1;dir+=2) {
   int threshold=0;
   for(int depth=1;depth<110;++depth) {
    memset(&g,0,sizeof g);g.traffic[0]=car(2,depth*dir,0);
    g.traffic[0].kind=(uint8_t)kind;g.traffic[0].x_q8=0;g.speed_q8=150*Q;
    int expected=traffic_body_contact(&g,&g.traffic[0]);
    near_count=damage_count=0;traffic_tick(&g,0,hit,award);
    CHECK(damage_count==(unsigned)expected && !near_count);
    if(expected)threshold=depth;
   }
   CHECK(threshold>10 && threshold<90);
  }
 }
 /* A whole-car cut-in cannot tunnel across the player between two clear
  * endpoints; a clear adjacent pass does not hit just because dz is small. */
 memset(&g,0,sizeof g);g.traffic[0]=car(2,0,0);g.traffic[0].x_q8=0;
 g.x_q8=40*Q;near_count=damage_count=0;
 CHECK(!traffic_body_contact(&g,&g.traffic[0]));
 traffic_tick_from(&g,0,-40*Q,0,hit,award);CHECK(damage_count==1);
 memset(&g,0,sizeof g);g.traffic[0]=car(2,1,0);g.traffic[0].x_q8=25*Q;
 g.position_q8=5*Q;g.speed_q8=280*Q;near_count=damage_count=0;
 traffic_tick_from(&g,0,0,0,hit,award);CHECK(!damage_count && near_count==1);
 /* The last visible rear slice is still collidable; a vehicle beyond the
  * drawing plane cannot cause an invisible impact. */
 memset(&g,0,sizeof g);g.traffic[0]=car(2,-45,0);g.traffic[0].x_q8=0;
 near_count=damage_count=0;traffic_tick_from(&g,0,0,0,hit,award);CHECK(!damage_count);
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
 visible_contacts();interactions();following();horn();pool_fairness();long_runs();
 printf("traffic: %llu checks; pixel-accurate chassis contacts, swept cut-ins, deterministic spawning, 384,000 mixed-speed frames, minimum headway, smooth following, safe horn changes and one-time near misses pass\n",checks);
 return 0;
}
