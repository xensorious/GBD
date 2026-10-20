#include "opening.h"
#include <string.h>
#define SLOT_MAGIC 0x534c5443u
static uint32_t checksum(const uint8_t*p,unsigned n){uint32_t h=2166136261u;for(unsigned i=0;i<n;i++)h=(h^p[i])*16777619u;return h;}
static void metadata(SlotInfo*s,const Game*g){s->valid=1;s->cash=g->save.cash;s->distance=g->save.best_distance;s->runs=g->save.total_runs;s->vehicle=g->save.selected_vehicle;s->location=g->save.selected_location;s->skin=g->save.skin;}
static int load_slot(const Opening*o,Game*g,int slot){
 const uint8_t*p=o->records+slot*SAVE_RECORD_SIZE;
 if(slot){uint32_t tag[3];memcpy(tag,p+SAVE_DISK_SIZE,sizeof tag);if(tag[0]!=SLOT_MAGIC || tag[1]!=(unsigned)slot || tag[2]!=checksum(p,SAVE_DISK_SIZE))return 0;}
 return game_load_bytes(g,p,SAVE_DISK_SIZE);
}
static void preview(Opening*o,Game*g){
 uint32_t frame=g->frame;game_init(g,0x434f4153u+(unsigned)o->cursor*0x10203u);
 if(o->slots[o->cursor].valid)load_slot(o,g,o->cursor);
 g->save_requested=0;g->frame=frame;
}
void opening_init(Opening*o,Game*g,const void*raw,unsigned size){
 memset(o,0,sizeof *o);memset(o->records,0xff,sizeof o->records);o->active_slot=255;
 if(raw){
  const uint8_t*data=raw;int current=0;
  for(int i=0;i<SAVE_SLOT_COUNT;i++)if(size>=(unsigned)i*SAVE_RECORD_SIZE+8){uint32_t h[2];memcpy(h,data+i*SAVE_RECORD_SIZE,8);if(h[0]==0x434c4452u && (h[1]==5u || h[1]==6u || h[1]==7u))current=1;}
  if(current){if(size>sizeof o->records)size=sizeof o->records;memcpy(o->records,raw,size);}
  else for(int i=0;i<SAVE_SLOT_COUNT;i++){
   unsigned offset=i*512;if(size<offset+8)continue;
   const uint8_t*p=data+offset;unsigned available=size-offset;
   if(i){if(available<512)continue;uint32_t tag[3];memcpy(tag,p+500,12);if(tag[0]!=SLOT_MAGIC || tag[1]!=(unsigned)i || tag[2]!=checksum(p,500))continue;}
   Game imported;game_init(&imported,1);
   if(game_load_bytes(&imported,p,available<500?available:500)){
    uint8_t*record=o->records+i*SAVE_RECORD_SIZE;
    if(game_encode_save(&imported,record,SAVE_RECORD_SIZE)){
     uint32_t tag[3]={SLOT_MAGIC,(unsigned)i,checksum(record,SAVE_DISK_SIZE)};
     memcpy(record+SAVE_DISK_SIZE,tag,sizeof tag);o->migrate=1;
    }
   }
  }
 }
 for(int i=0;i<SAVE_SLOT_COUNT;i++){Game test;game_init(&test,1);if(load_slot(o,&test,i))metadata(&o->slots[i],&test);}
 for(int i=0;i<SAVE_SLOT_COUNT;i++)if(o->slots[i].valid){o->cursor=(uint8_t)i;break;}
 preview(o,g);
}
void opening_update(Opening*o,Game*g,const Input*in){
 if(o->phase==OPEN_DONE){
  if(g->mode==M_HOME && (in->pressed&K_B)){o->phase=OPEN_SLOTS;o->cursor=o->active_slot;return;}
  game_update(g,in);return;
 }
 ++g->frame;
 if(o->phase==OPEN_TITLE){if(in->pressed)o->phase=OPEN_SLOTS;return;}
 if(o->phase==OPEN_SLIDE){if(++o->slide>=OPENING_SLIDE_TICKS)o->phase=OPEN_DONE;return;}
 if(in->pressed&K_B){o->phase=OPEN_TITLE;return;}
 int dy=((in->pressed&K_DOWN)?1:0)-((in->pressed&K_UP)?1:0);
 if(dy){o->cursor=(uint8_t)((o->cursor+SAVE_SLOT_COUNT+dy)%SAVE_SLOT_COUNT);preview(o,g);}
 if(in->pressed&(K_A|K_START)){
  preview(o,g);o->active_slot=o->cursor;g->save_requested=1;o->slide=0;o->phase=OPEN_SLIDE;
 }
}
int opening_store(Opening*o,const Game*g){
 if(o->active_slot>=SAVE_SLOT_COUNT)return 0;
 uint8_t record[SAVE_RECORD_SIZE];memset(record,0xff,sizeof record);
 if(!game_encode_save(g,record,sizeof record))return 0;
 uint32_t tag[3]={SLOT_MAGIC,o->active_slot,checksum(record,SAVE_DISK_SIZE)};
 memcpy(record+SAVE_DISK_SIZE,tag,sizeof tag);
 memcpy(o->records+o->active_slot*SAVE_RECORD_SIZE,record,sizeof record);metadata(&o->slots[o->active_slot],g);return 1;
}
