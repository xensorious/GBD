/* Keep Career's atlas, UI state and redraw policy outside the driving
 * renderer. The existing render core retains its original compilation shape. */
#include "render.h"
#include "career.h"
#include "story.h"
#include "ui.h"
#include <string.h>
int render_core(const Game*,uint16_t*,uint16_t*);
void render_invalidate_bottom(void);
static uint32_t last_revision;
static uint8_t active,valid,last_skin,last_notice,dialogue_active;
static char notice_text[sizeof(((Game*)0)->notice)];
int render_game(const Game*g,uint16_t*top,uint16_t*bottom){
 if(g->mode==M_DIALOG){dialogue_active=1;story_draw(g,top,bottom);return 3;}
 if(dialogue_active){dialogue_active=0;story_render_release();render_invalidate_bottom();valid=0;}
 if(g->mode!=M_STORY){
  if(active){active=0;render_invalidate_bottom();}
  return render_core(g,top,bottom);
 }
 if(!active){active=1;valid=0;}
 Surface atlas={top,256,192},menu={bottom,256,192};
 ui_draw_career_map(&atlas,g);
 int notice=g->notice_timer!=0;
 int dirty=!valid||last_revision!=campaign.revision||last_skin!=g->save.skin||last_notice!=notice||memcmp(notice_text,g->notice,sizeof notice_text);
 if(dirty){
  ui_draw_bottom(&menu,g);valid=1;last_revision=campaign.revision;last_skin=g->save.skin;last_notice=(uint8_t)notice;
  memcpy(notice_text,g->notice,sizeof notice_text);
 }
 return 1|(dirty?2:0);
}
