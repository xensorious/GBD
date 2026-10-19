#ifndef COASTLINE_OPENING_H
#define COASTLINE_OPENING_H
#include "game.h"
#define SAVE_SLOT_COUNT 3
#define SAVE_RECORD_SIZE 1024
#define SAVE_BANK_SIZE (SAVE_SLOT_COUNT*SAVE_RECORD_SIZE)
#define OPENING_SLIDE_TICKS 36
enum OpeningPhase { OPEN_TITLE,OPEN_SLOTS,OPEN_SLIDE,OPEN_DONE };
typedef struct {uint32_t cash,distance,runs;uint8_t valid,vehicle,location,skin;} SlotInfo;
typedef struct {
 uint8_t records[SAVE_BANK_SIZE];
 SlotInfo slots[SAVE_SLOT_COUNT];
 uint16_t slide;
 uint8_t phase,cursor,active_slot,migrate;
} Opening;
void opening_init(Opening*,Game*,const void*,unsigned);
void opening_update(Opening*,Game*,const Input*);
int opening_store(Opening*,const Game*);
#endif
