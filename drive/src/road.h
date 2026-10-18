#ifndef COASTLINE_ROAD_H
#define COASTLINE_ROAD_H

#include <stdint.h>

/* World distances use three units per metre. These lengths do not depend on
 * frame rate, so driving backwards retraces exactly the same road. */
#define ROAD_NODE_LENGTH 1536
#define ROAD_BIOME_LENGTH 21000
#define ROAD_BLEND_LENGTH 1600

enum RoadFeature {
 ROAD_F_BRIDGE   = 1u << 0,
 ROAD_F_OVERPASS = 1u << 1,
 ROAD_F_PALMS    = 1u << 2,
 ROAD_F_HARBOR   = 1u << 3,
 ROAD_F_PINES    = 1u << 4,
 ROAD_F_ROCKS    = 1u << 5,
 ROAD_F_CITY     = 1u << 6
};

typedef struct {
 /* center is a lateral world coordinate, not a screen position. curve is
  * d(center)/d(distance), scaled by 256. Subtract the center at the player's
  * distance before projecting a point ahead. Both remain smooth at nodes. */
 int16_t center,curve;
 uint8_t biome,next_biome,blend,lanes,feature;
} RoadSample;

/* Stateless hashes: neither function changes gameplay RNG state. */
uint32_t road_hash(uint32_t seed,uint32_t index);
uint32_t road_landmark(uint32_t seed,uint32_t segment,int side);

/* Integer distance may be negative. biome is the source environment;
 * next_biome is its successor and blend rises from 0 to 255 during the final
 * 1,600 units of a section. Blend terrain and scenery palettes using these
 * fields; lanes and usable width never change during a transition.
 *
 * Continuity checks: sample +/-1 around every ROAD_NODE_LENGTH boundary.
 * Centers differ by at most one unit and tangent tends to zero at the node.
 * A full five-biome cycle returns to the starting biome after 105,000 units.
 */
void road_sample(uint32_t seed,int32_t distance,int start_biome,RoadSample *out);

#endif
