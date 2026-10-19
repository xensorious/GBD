#ifndef COASTLINE_RENDER_H
#define COASTLINE_RENDER_H
#include "game.h"
#include "opening.h"
int render_game(const Game*,uint16_t *top,uint16_t *bottom);
int render_opening(const Game*,const Opening*,uint16_t *scratch,uint16_t *top,uint16_t *bottom);
#endif
