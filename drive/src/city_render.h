#ifndef COASTLINE_CITY_RENDER_H
#define COASTLINE_CITY_RENDER_H
#include "city.h"
#include "gfx.h"
int city_garden_clear(int x,int y,int w,int h);
int city_building_style(int column,int row);
void city_draw_ground(Surface*,int camx,int camy);
/* Q4 screen X/Y/depth; camera heading shares the simulation's 0..255 units. */
void city_project_point(int x,int y,int height,int camera_x,int camera_y,int angle,int*out);
#ifndef ARM9
void city_draw_rotated_ground(Surface*,int camera_x,int camera_y,int angle);
#endif
void city_draw_map(Surface*,int x,int y,int w,int h,const Game*);
/* Output buffer holds the full 48x72 source painting. */
void city_draw_vehicle_steering(Surface*,int x,int y,int angle,int steer,int model,const Save*);
void city_vehicle_pixels(uint16_t*out,int model,const Save*save);
void city_draw_vehicle(Surface*,int x,int y,int angle,int model,const Save*);
#endif
