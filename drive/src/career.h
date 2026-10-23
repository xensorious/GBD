#ifndef COASTLINE_CAREER_H
#define COASTLINE_CAREER_H
#include "game.h"
#define CAREER_CHAPTERS 5
#define CAREER_EVENTS 25
#define CAREER_CAST_MODAL 6
#define CAREER_GALLERY_POSE_MASK 7u
#define CAREER_GALLERY_LEFT 8u
#define CAREER_GALLERY_LEGACY 16u
/* Transient menu state. Persistent story frontier lives in the profile. */
typedef struct {uint8_t tab,chapter,event,modal,page,garage_return,drive_column,gallery_pose;uint32_t revision;} CareerUI;
typedef struct {const char *name,*subtitle,*surface,*premise,*mini,*boss,*mini_style,*boss_style;uint16_t accent;} CareerChapter;
extern CareerUI campaign;
extern const CareerChapter career_chapters[CAREER_CHAPTERS];
void career_reset(void);
void career_enter(Game*);
void campaign_tick(Game*,const Input*);
void career_point(int chapter,int event,int*x,int*y);
int career_status(const Game*,int chapter,int event); /* 0 locked, 1 current, 2 cleared */
int career_event_kind(int event);
const char *career_event_name(int chapter,int event);
const char *career_event_format(int chapter,int event);
const char *career_story(int chapter,int beat);
const char *career_plan(int chapter,int row);
const char *career_plan_name(int row);
int career_rows(int tab);
int career_journal_open(const Game*,int chapter,int row);
#endif
