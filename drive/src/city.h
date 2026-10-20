#ifndef COASTLINE_CITY_H
#define COASTLINE_CITY_H
#include "game.h"
#define CITY_SIZE 4096
#define CITY_TRAFFIC 64
enum {CITY_ZOOM_NUM=7,CITY_ZOOM_DEN=8};
/* World-unit silhouettes are shared by rendering, traffic clearance and
 * contact tests. Larger utility vehicles retain their own proportions. */
typedef struct {uint8_t half_width,half_length,draw_half_width,draw_half_length;} CityShape;
CityShape city_vehicle_shape(int model);
int city_vehicle_overlap(int ax,int ay,int angle,int model,int bx,int by,int bangle,int bmodel,int margin);
typedef struct {int16_t x,y;uint16_t angle;uint32_t progress;uint8_t route,reverse,model,near;} CityCar;
typedef struct {int active;int32_t x,y,vx,vy,camera_x,camera_y;uint16_t angle;uint32_t travelled;CityCar cars[CITY_TRAFFIC];uint8_t tokens[64];} City;
typedef struct {int16_t x1,y1,x2,y2;uint8_t half,highway;} CityRoad;
typedef struct {int x,y,w,h,kind;} CityLot;
extern const CityRoad city_roads[];
extern const int city_road_count;
extern City city;
int city_sin(int angle);
int city_cos(int angle);
int city_on_road(int x,int y);
int city_water(int x,int y);
int city_lot(int column,int row);
CityLot city_lot_bounds(int column,int row);
int city_solid(int x,int y);
const char *city_district(int x,int y);
void city_begin(Game*);
#endif
