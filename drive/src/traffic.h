#ifndef COASTLINE_TRAFFIC_H
#define COASTLINE_TRAFFIC_H
#include "game.h"

/* Centre-to-centre world distances; three units are one HUD metre. Fixed
 * pools and stateless row hashes make identical runs replay identically. */
#define TRAFFIC_MIN_GAP (110*Q)
#define TRAFFIC_SPAWN_GAP (180*Q)

/* Scoring/collision callbacks retain the core's damage cooldown and receipt rules. */
typedef void (*TrafficDamage)(Game *g,int raw,int rail);
typedef void (*TrafficPoints)(Game *g,int category,uint32_t count,uint32_t amount);
void traffic_spawn_ahead(Game *g);
/* Updates front to back, reserves both lanes during a horn manoeuvre, and
 * smoothly follows the nearest leader before applying player interactions.
 * cruise_q8 remains each driver's seeded 80..125 km/h preference; speed_q8
 * may drop below it while following. Static fixtures may set both to zero. */
void traffic_tick(Game *g,int32_t old_position,TrafficDamage damage,TrafficPoints points);
/* Main simulation supplies the pre-steering X position for swept cut-ins.
 * Contact tests use the cleaned, projected opaque chassis; shadows, exhaust
 * and sprite padding are not part of either vehicle's collision shape. */
void traffic_tick_from(Game *g,int32_t old_position,int32_t old_x,int old_yaw,
                       TrafficDamage damage,TrafficPoints points);
int traffic_body_contact(const Game *g,const Traffic *t);
/* Signed horizontal gap between opaque bodies on their shared screen rows;
 * negative means overlap. A large positive value means no shared rows. */
int traffic_body_gap(const Game *g,const Traffic *t);
#endif
