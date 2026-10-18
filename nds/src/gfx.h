#ifndef ASTER_GFX_H
#define ASTER_GFX_H
#include <stdint.h>
#include "game.h"
#define C(r,g,b) ((uint16_t)(0x8000|((r)>>3)|(((g)>>3)<<5)|(((b)>>3)<<10)))
typedef struct {uint16_t *pixels;int w,h;} Surface;
void gfx_copy(uint16_t *dest,const uint16_t *src,int count);
void gfx_rect(Surface*,int x,int y,int w,int h,uint16_t color);
void gfx_card(Surface*,int x,int y,int w,int h,uint16_t fill,uint16_t border);
void gfx_text(Surface*,int x,int y,const char*,uint16_t color);
void gfx_text_small(Surface*,int x,int y,const char*,uint16_t color);
int gfx_text_width(const char*);
void gfx_wrap(Surface*,int x,int y,const char*,int width,int lineheight,int maxlines,uint16_t color);
void gfx_sprite(Surface*,int x,int y,const uint16_t*,int w,int h,int flip);
void gfx_creature(Surface*,int x,int y,int species,int size,int back,int frame);
void gfx_line(Surface*,int x1,int y1,int x2,int y2,uint16_t color);
void gfx_circle(Surface*,int x,int y,int radius,uint16_t color);
void gfx_map_marker(Surface*,const Game*,int old_x,int old_y);
void gfx_map(Surface*,int x,int y,int w,int h,const Game*);
void gfx_number(Surface*,int x,int y,int value,uint16_t color);
#endif
