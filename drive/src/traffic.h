#ifndef COASTLINE_TRAFFIC_H
#define COASTLINE_TRAFFIC_H
#include "game.h"

/* Centre-to-centre world distances; three units are one HUD metre. Fixed
 * pools and stateless row hashes make identical runs replay identically. */
#define TRAFFIC_MIN_GAP (110*Q)
#define TRAFFIC_SPAWN_GAP (180*Q)
#define TRAFFIC_CAR_HALF_LENGTH 58
#define TRAFFIC_LARGE_HALF_LENGTH 70

/* Scoring/collision callbacks retain the core's damage cooldown and receipt rules. */
typedef void (*TrafficDamage)(Game *g,int raw,int rail);
typedef void (*TrafficPoints)(Game *g,int category,uint32_t count,uint32_t amount);
void traffic_spawn_ahead(Game *g);
/* Updates front to back, reserves both lanes during a horn manoeuvre, and
 * smoothly follows the nearest leader before applying player interactions.
 * cruise_q8 remains each driver's seeded 80..125 km/h preference; speed_q8
 * may drop below it while following. Static fixtures may set both to zero. */
void traffic_tick(Game *g,int32_t old_position,TrafficDamage damage,TrafficPoints points);
#endif
