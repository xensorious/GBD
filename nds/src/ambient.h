#ifndef ASTERWILD_AMBIENT_H
#define ASTERWILD_AMBIENT_H

#include "gfx.h"
#include <stdint.h>

/* Apply the second wind pose to both the painted world and its occlusion
   plane. The base pose is copied by the world renderer every frame. */
void ambient_world(Surface *surface, uint8_t *viewport_depth,
                   int cam_x, int cam_y, unsigned frame);

/* Small, occasional, world-anchored foreground details; call before the HUD. */
void ambient_foreground(Surface *surface, int cam_x, int cam_y,
                        unsigned frame);

#endif
