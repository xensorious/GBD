#ifndef COASTLINE_GFX_H
#define COASTLINE_GFX_H
#include <stdint.h>
#define C(r,g,b) ((uint16_t)(0x8000|((r)>>3)|(((g)>>3)<<5)|(((b)>>3)<<10)))
typedef struct {uint16_t *pixels;int w,h;} Surface;
void gfx_init(void);
void gfx_copy(uint16_t*,const uint16_t*,int);
void gfx_rect(Surface*,int x,int y,int w,int h,uint16_t);
void gfx_card(Surface*,int x,int y,int w,int h,uint16_t fill,uint16_t border);
void gfx_text(Surface*,int x,int y,const char*,uint16_t);
void gfx_text_small(Surface*,int x,int y,const char*,uint16_t);
int gfx_text_width(const char*);
void gfx_wrap(Surface*,int x,int y,const char*,int width,int lineheight,int maxlines,uint16_t);
void gfx_sprite(Surface*,int x,int y,const uint16_t*,int w,int h,int flip);
void gfx_scaled_sprite(Surface*,int x,int y,const uint16_t*,int srcw,int srch,int destw,int desth);
void gfx_indexed_sprite(Surface*,int x,int y,const uint8_t*,const uint16_t *palette,int srcw,int srch,int destw,int desth);
void gfx_vehicle(Surface*,int x,int y,const uint16_t*,int srcw,int srch,int destw,int desth,int paint);
/* One reusable decoded pose. Consume it before requesting another vehicle or
 * pose; palette index zero is transparent in every packed source. */
const uint16_t *gfx_vehicle_pixels(int vehicle,int pose);
/* Player cosmetics use authored material flags, preserving skin, hair, glass,
 * horse fur, tires and every transparent pixel. */
void gfx_vehicle_style(Surface*,int x,int y,const uint16_t*,int srcw,int srch,
                       int destw,int desth,int paint,int livery,int wheels);
void gfx_line(Surface*,int x1,int y1,int x2,int y2,uint16_t);
void gfx_circle(Surface*,int x,int y,int radius,uint16_t);
void gfx_number(Surface*,int x,int y,int,uint16_t);
#endif
