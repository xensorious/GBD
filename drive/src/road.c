#include "road.h"
#include "game.h"

uint32_t road_hash(uint32_t seed,uint32_t index)
{
 uint32_t h=seed ^ (index*0x9e3779b9u+0x85ebca6bu);
 h^=h>>16; h*=0x7feb352du;
 h^=h>>15; h*=0x846ca68bu;
 return h^(h>>16);
}

uint32_t road_landmark(uint32_t seed,uint32_t segment,int side)
{
 return road_hash(seed ^ (side<0 ? 0x6a09e667u : 0xbb67ae85u),segment);
}

/* C division truncates towards zero. Correct it once to make the road and
 * scenery work in both directions, including the small reversing apron. */
static int32_t floor_section(int32_t value,int32_t span,int32_t *within)
{
 int32_t section=value/span;
 int32_t remainder=value%span;
 if(remainder<0) { --section; remainder+=span; }
 *within=remainder;
 return section;
}

static int road_center(uint32_t seed,int32_t node)
{
 /* All four lanes remain comfortably inside the authored roadside bounds. */
 return (int)(road_hash(seed ^ 0xa54ff53au,(uint32_t)node)%97u)-48;
}

static uint16_t feature_for(int biome,int32_t within)
{
 switch(biome) {
 case CITY:
  /* Short, spaced overpasses leave long sight lines and never remove lanes. */
  return ROAD_F_CITY | ((within%6400)>=4700 && (within%6400)<5100 ? ROAD_F_OVERPASS : 0);
 case COAST: return ROAD_F_PALMS | ROAD_F_HARBOR;
 case CAUSEWAY: return ROAD_F_BRIDGE;
 case PINES: return ROAD_F_PINES;
 case CANYON: return ROAD_F_ROCKS;
 case MARINA: return ROAD_F_MARINA | ROAD_F_HARBOR | ROAD_F_CITY | ROAD_F_PALMS;
 case WETLANDS: return ROAD_F_WETLANDS | ROAD_F_BRIDGE | ROAD_F_PALMS;
 case GROVES: return ROAD_F_GROVES | ROAD_F_PALMS;
 case NEON: return ROAD_F_NEON | ROAD_F_CITY;
 case JUNGLE: return ROAD_F_JUNGLE | ROAD_F_PALMS;
 case SWAMP: return ROAD_F_SWAMP | ROAD_F_WETLANDS;
 case AURORA: return ROAD_F_AURORA | ROAD_F_PINES;
 case SALT: return ROAD_F_SALT | ROAD_F_ROCKS;
 default: return 0;
 }
}

void road_sample(uint32_t seed,int32_t distance,int start_biome,RoadSample *out)
{
 int32_t local,within;
 int32_t node=floor_section(distance,ROAD_NODE_LENGTH,&local);
 int a=road_center(seed,node),b=road_center(seed,node+1);
 /* Q12 cubic smoothstep has zero slope at each node. Its products fit in
  * signed 32 bits, avoiding soft float or 64-bit arithmetic on the ARM9. */
 int t=(local*4096)/ROAD_NODE_LENGTH;
 int t2=(t*t)/4096;
 int smooth=(t2*(12288-2*t))/4096;
 int delta=(b-a)*smooth;
 int center=a+(delta>=0 ? (delta+2048)/4096 : (delta-2048)/4096);
 /* 6*t*(1-t) in Q12, then scale the actual world slope to Q8. */
 int slope_shape=(6*t*(4096-t))/4096;
 int curve=((b-a)*slope_shape)/(ROAD_NODE_LENGTH*16);

 int32_t section=floor_section(distance,ROAD_BIOME_LENGTH,&within);
 int base=start_biome%LOCATION_COUNT;
 if(base<0) base+=LOCATION_COUNT;
 int biome=(int)((section%LOCATION_COUNT+base)%LOCATION_COUNT);
 if(biome<0) biome+=LOCATION_COUNT;
 int blend=0;
 if(within>=ROAD_BIOME_LENGTH-ROAD_BLEND_LENGTH)
  blend=((within-(ROAD_BIOME_LENGTH-ROAD_BLEND_LENGTH))*255)/(ROAD_BLEND_LENGTH-1);

 out->center=(int16_t)center;
 out->curve=(int16_t)curve;
 out->biome=(uint8_t)biome;
 out->next_biome=(uint8_t)((biome+1)%LOCATION_COUNT);
 out->blend=(uint8_t)blend;
 out->lanes=4;
 out->feature=feature_for(biome,within);
}
