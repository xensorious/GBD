#ifndef COASTLINE_CITY_RENDER_H
#define COASTLINE_CITY_RENDER_H
#include "city.h"
#include "gfx.h"
void city_draw_map(Surface*,int x,int y,int w,int h,const Game*);
void city_draw_vehicle(Surface*,int x,int y,int angle,int model,const Save*);
#endif
