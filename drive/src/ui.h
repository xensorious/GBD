#ifndef COASTLINE_UI_H
#define COASTLINE_UI_H
#include "game.h"
#include "gfx.h"
/* Presentation only: all actions are handled by the button input state machine. */
void ui_draw_career_map(Surface*,const Game*);
void ui_draw_bottom(Surface *surface,const Game *game);
#endif
