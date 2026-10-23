#ifndef COASTLINE_STORY_H
#define COASTLINE_STORY_H
#include "game.h"
#include "gfx.h"
#define STORY_TOTAL 125
#define STORY_TEXT_MAX 192
#define STORY_MAX_LINES 16
#define STORY_SCENE_HEIGHT 288
#define STORY_DIALOG_Y 96
#define STORY_DIALOG_TEXT_X 15
#define STORY_DIALOG_TEXT_Y 124
#define STORY_DIALOG_TEXT_WIDTH 226
#define STORY_DIALOG_ROWS 3
#define STORY_DIALOG_LINE_HEIGHT 14
#include "story_limits.h"
#define STORY_FOCUS_MOVE 0x8000u
#define STORY_FOCUS_SUBJECT 0x4000u
#define STORY_FOCUS_X 0x01ffu
#define STORY_FOCUS_POSE (3u<<9)
#define STORY_FOCUS_COMPANION_POSE (3u<<11)
enum StoryPhase {ST_FADEIN,ST_READING,ST_SHOT,ST_ENDING,ST_CLEAR};
enum StoryStage {ST_FACE,ST_PAN,ST_RADIO,ST_MONTAGE,ST_PLATE};
enum StoryCast {SC_NIA,SC_JULES,SC_CALDER,SC_IMANI,SC_MARISOL,SC_KADE,SC_ROOK,SC_REN,SC_ASTER,SC_SABLE,SC_VOSS,SC_MINA,SC_ELI,SC_VALE,SC_YOU,SC_NARRATOR,SC_NONE=255};
/* Byte seven used to be the always-zero high byte of the text length.
 * Its two nibbles extend the lead/companion pose indices without growing a line. */
typedef struct {uint8_t speaker,companion,stage,background;uint16_t focus;uint8_t length,pose_flags;char text[STORY_TEXT_MAX];} StoryLine;
static inline unsigned story_pose(const StoryLine*line,int companion){
 return ((line->focus>>(companion?11:9))&3u)|(((line->pose_flags>>(companion?4:0))&15u)<<2);
}
typedef struct {
 uint32_t offsets[STORY_MAX_LINES],revision;
 StoryLine current,previous;
 uint16_t revealed,ticks,age;
 uint8_t active,chapter,event,line,count,phase,phase_tick,replay,pause,cursor,failed;
 /* Consume the old tail padding so existing RAM/save fixture offsets stay put. */
 uint8_t page_start,page_end,page_index;
} StoryState;
extern StoryState story;
extern const char*const story_cast_names[STORY_SPEAKERS];
void story_reset(void);
int story_completed(const Game*);
int story_status(const Game*,int chapter,int event);
int story_open(Game*,int chapter,int event);
void story_tick(Game*,const Input*);
int story_read(unsigned offset,void*destination,unsigned bytes);
int story_asset(int id,uint8_t*destination,uint16_t*palette,unsigned capacity,int*w,int*h);
void story_draw(const Game*,uint16_t*top,uint16_t*bottom);
void story_gallery_draw(const Game*,uint16_t*top,uint16_t*bottom);
void story_render_release(void);
int story_text_lines(const char*,int width);
unsigned story_text_advance(unsigned character);
unsigned story_text_page_end(const char*,unsigned start);
/* One shared scratch lease: the ordinary sprite cache is invalidated on both edges. */
void *gfx_story_scratch(unsigned*size);
void gfx_story_release(void);
#endif
