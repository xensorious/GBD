#ifndef COASTLINE_STORY_H
#define COASTLINE_STORY_H
#include "game.h"
#include "gfx.h"
#define STORY_TOTAL 125
#define STORY_TEXT_MAX 192
#define STORY_MAX_LINES 16
#define STORY_BACKGROUNDS 18
#define STORY_ASSETS (STORY_BACKGROUNDS+12)
#define STORY_FOCUS_MOVE 0x8000u
#define STORY_FOCUS_SUBJECT 0x4000u
#define STORY_FOCUS_X 0x01ffu
enum StoryPhase {ST_FADEIN,ST_READING,ST_SHOT,ST_ENDING,ST_CLEAR};
enum StoryStage {ST_FACE,ST_PAN,ST_RADIO,ST_MONTAGE};
enum StoryCast {SC_NIA,SC_JULES,SC_CALDER,SC_IMANI,SC_MARISOL,SC_KADE,SC_ROOK,SC_REN,SC_ASTER,SC_SABLE,SC_VOSS,SC_MINA,SC_YOU,SC_NARRATOR,SC_NONE=255};
typedef struct {uint8_t speaker,companion,stage,background;uint16_t focus,length;char text[STORY_TEXT_MAX];} StoryLine;
typedef struct {
 uint32_t offsets[STORY_MAX_LINES],revision;
 StoryLine current,previous;
 uint16_t revealed,ticks,age;
 uint8_t active,chapter,event,line,count,phase,phase_tick,replay,pause,cursor,failed;
} StoryState;
extern StoryState story;
extern const char*const story_cast_names[14];
void story_reset(void);
int story_completed(const Game*);
int story_status(const Game*,int chapter,int event);
int story_open(Game*,int chapter,int event);
void story_tick(Game*,const Input*);
int story_read(unsigned offset,void*destination,unsigned bytes);
int story_asset(int id,uint8_t*destination,uint16_t*palette,unsigned capacity,int*w,int*h);
void story_draw(const Game*,uint16_t*top,uint16_t*bottom);
void story_render_release(void);
int story_text_lines(const char*,int width);
/* One shared scratch lease: the ordinary sprite cache is invalidated on both edges. */
void *gfx_story_scratch(unsigned*size);
void gfx_story_release(void);
#endif
