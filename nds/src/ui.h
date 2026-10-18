#ifndef ASTERWILD_UI_H
#define ASTERWILD_UI_H

#include "game.h"
#include "gfx.h"

/* The bottom screen is a view of the game, never a second source of state. */
void ui_draw_bottom(Surface *surface, const Game *game);

#endif
