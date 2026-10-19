/* Road contract and adversarial-coordinate checks, independent of rendering. */
#include "road.h"
#include "game.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static unsigned long checks;
#define CHECK(c) do { ++checks; if(!(c)) { fprintf(stderr,"road:%d: %s\n",__LINE__,#c); exit(1); } } while(0)

static RoadSample sample(uint32_t seed,int32_t z,int start)
{
 RoadSample out={0}; road_sample(seed,z,start,&out); return out;
}
int main(void)
{
 unsigned seen=0,features=0,changed=0;
 for(unsigned seed=0;seed<11;seed++) {
  RoadSample previous=sample(seed,-12000,seed%LOCATION_COUNT);
  for(int32_t z=-11999;z<210000;z++) {
   RoadSample a=sample(seed,z,seed%LOCATION_COUNT),b=sample(seed,z,seed%LOCATION_COUNT);
   CHECK(!memcmp(&a,&b,sizeof a));
   CHECK(a.center>=-48 && a.center<=48);
   CHECK(abs(a.center-previous.center)<=1);
   CHECK(a.curve>=-32 && a.curve<=32);
   CHECK(a.biome<LOCATION_COUNT && a.next_biome==(a.biome+1)%LOCATION_COUNT);
   CHECK(a.lanes==4);
   seen|=1u<<a.biome; features|=a.feature;
   if((z%97)==0 && a.center!=sample(seed+99,z,seed%LOCATION_COUNT).center) ++changed;
   previous=a;
  }
 }
 CHECK(seen==(1u<<LOCATION_COUNT)-1u); CHECK(changed>10000);
 CHECK(features==(ROAD_F_BRIDGE|ROAD_F_OVERPASS|ROAD_F_PALMS|ROAD_F_HARBOR|ROAD_F_PINES|ROAD_F_ROCKS|ROAD_F_CITY|ROAD_F_MARINA|ROAD_F_WETLANDS|ROAD_F_GROVES|ROAD_F_NEON|ROAD_F_JUNGLE|ROAD_F_SWAMP|ROAD_F_AURORA|ROAD_F_SALT));
 for(int start=0;start<LOCATION_COUNT;start++) {
  for(int cycle=-3;cycle<=3;cycle++) {
   for(int section=0;section<LOCATION_COUNT;section++) {
    int32_t z=(cycle*LOCATION_COUNT+section)*ROAD_BIOME_LENGTH;
    RoadSample a=sample(123,z,start);
    CHECK(a.biome==(start+section)%LOCATION_COUNT && !a.blend);
    RoadSample end=sample(123,z+ROAD_BIOME_LENGTH-1,start);
    RoadSample next=sample(123,z+ROAD_BIOME_LENGTH,start);
    CHECK(end.blend==255 && !next.blend && end.next_biome==next.biome);
    /* Use distinct scalar palette values to prove exact endpoint color
     * continuity, independent of which palettes the renderer chooses. */
    int end_material=(end.biome*31*(255-end.blend)+end.next_biome*31*end.blend)/255;
    int next_material=(next.biome*31*(255-next.blend)+next.next_biome*31*next.blend)/255;
    CHECK(end_material==next_material);
    unsigned last=0;
    for(int d=ROAD_BIOME_LENGTH-ROAD_BLEND_LENGTH;d<ROAD_BIOME_LENGTH;d++) {
     unsigned blend=sample(123,z+d,start).blend; CHECK(blend>=last); last=blend;
    }
   }
  }
 }
 for(int n=-10000;n<=10000;n++) {
  int32_t z=n*ROAD_NODE_LENGTH;
  RoadSample a=sample(0xabcdef01u,z-1,0),b=sample(0xabcdef01u,z,0),c=sample(0xabcdef01u,z+1,0);
  CHECK(abs(a.center-b.center)<=1 && abs(b.center-c.center)<=1);
  CHECK(abs(a.curve)<=1 && abs(b.curve)<=1 && abs(c.curve)<=1);
 }
 const int32_t extremes[]={INT32_MIN,INT32_MIN+1,-1,0,1,INT32_MAX-1,INT32_MAX};
 for(unsigned i=0;i<sizeof extremes/sizeof *extremes;i++) {
  RoadSample a=sample(UINT32_MAX,extremes[i],INT_MIN);
  CHECK(a.biome<LOCATION_COUNT && a.next_biome<LOCATION_COUNT && a.lanes==4);
 }
 for(uint32_t i=0;i<100000;i++) {
  CHECK(road_hash(77,i)==road_hash(77,i));
  CHECK(road_landmark(77,i,-1)==road_landmark(77,i,-1));
 }
 printf("road: %lu checks, 2,441,989 consecutive samples; all %d biomes, reverse travel, exact transition endpoints, boundary continuity and extreme coordinates pass\n",checks,LOCATION_COUNT);
 return 0;
}
