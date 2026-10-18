/* The v1 object is deliberately only 100 bytes: ASan catches a migration
 * that accidentally treats it as the larger v2 save before checking version. */
#include "game.h"
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
 uint32_t magic,version,checksum,cash,owned,unlocked_locations,paints;
 uint32_t best_score,best_distance,best_near,best_drift,best_speed_hold,best_wheelie,best_coins;
 uint32_t total_runs,total_distance,total_cash,seed;
 uint8_t selected_vehicle,selected_location,paint[6],engine[6],grip[6],armor[6],reserved[2];
} LegacySave;
static unsigned checks;
#define CHECK(c) do{++checks;if(!(c)){fprintf(stderr,"legacy_save:%d: %s\n",__LINE__,#c);exit(1);}}while(0)
static void sign_legacy(LegacySave *s) {
 s->checksum=0;uint32_t h=2166136261u;const uint8_t *bytes=(const uint8_t*)s;
 for(size_t i=0;i<sizeof(*s);i++) h=(h^bytes[i])*16777619u;
 s->checksum=h;
}
static LegacySave profile(void) {
 LegacySave s={0};s.magic=0x434c4452u;s.version=1;s.cash=12345;
 s.owned=63;s.unlocked_locations=31;s.paints=63;s.best_score=34567;s.best_distance=7000;
 s.best_near=12;s.best_drift=1600;s.best_speed_hold=300;s.best_wheelie=1200;s.best_coins=37;
 s.total_runs=17;s.total_distance=45678;s.total_cash=22345;s.seed=0x12345abcu;
 s.selected_vehicle=3;s.selected_location=1;
 for(int i=0;i<6;i++){s.paint[i]=(uint8_t)i;s.engine[i]=(uint8_t)(i%4);s.grip[i]=(uint8_t)((i+1)%4);s.armor[i]=(uint8_t)((i+2)%4);}
 sign_legacy(&s);return s;
}
int main(void) {
 CHECK(sizeof(LegacySave)==100);CHECK(sizeof(Save)>sizeof(LegacySave));
 LegacySave old=profile();Game g,defaults,reloaded;Save migrated;
 game_init(&defaults,123);game_init(&g,7);
 CHECK(game_load(&g,(const Save*)(const void*)&old));
 CHECK(g.mode==M_HOME && g.has_save && g.save_requested && g.save.version==2);
 CHECK(!memcmp((const uint8_t*)&g.save+12,(const uint8_t*)&old+12,60));
 CHECK(g.save.selected_vehicle==old.selected_vehicle && g.save.selected_location==old.selected_location);
 for(int i=0;i<6;i++) {
  CHECK(g.save.paint[i]==old.paint[i]);CHECK(g.save.engine[i]==old.engine[i]);
  CHECK(g.save.grip[i]==old.grip[i]);CHECK(g.save.armor[i]==old.armor[i]);
 }
 for(int i=6;i<VEHICLE_COUNT;i++) CHECK(!g.save.paint[i] && !g.save.engine[i] && !g.save.grip[i] && !g.save.armor[i]);
 CHECK(g.save.music_on==defaults.save.music_on && g.save.sfx_on==defaults.save.sfx_on);
 CHECK(g.save.music_volume==defaults.save.music_volume && g.save.sfx_volume==defaults.save.sfx_volume);
 CHECK(g.save.skin==defaults.save.skin && g.save.units==defaults.save.units);
 CHECK(g.save.shake==defaults.save.shake && g.save.motion==defaults.save.motion);
 for(int i=0;i<VEHICLE_COUNT;i++) {
  CHECK(g.save.power_tune[i]==defaults.save.power_tune[i]);
  CHECK(g.save.handling_tune[i]==defaults.save.handling_tune[i]);
  CHECK(g.save.chassis_tune[i]==defaults.save.chassis_tune[i]);
 }
 game_make_save(&g,&migrated);CHECK(migrated.version==2 && game_save_valid(&migrated));
 game_init(&reloaded,77);CHECK(game_load(&reloaded,&migrated));
 CHECK(!reloaded.save_requested && reloaded.save.cash==old.cash && reloaded.save.selected_vehicle==3);
 for(size_t i=0;i<sizeof(old);i++) {
  Save padded;memset(&padded,0xff,sizeof padded);memcpy(&padded,&old,sizeof old);
  ((uint8_t*)&padded)[i]^=0x40;game_init(&reloaded,77);
  CHECK(!game_load(&reloaded,&padded));CHECK(!reloaded.has_save && !reloaded.save.cash);
 }
 LegacySave invalid=old;invalid.selected_vehicle=6;sign_legacy(&invalid);
 CHECK(!game_load(&reloaded,(const Save*)(const void*)&invalid));
 invalid=old;invalid.engine[2]=4;sign_legacy(&invalid);CHECK(!game_load(&reloaded,(const Save*)(const void*)&invalid));
 invalid=old;invalid.owned=0;sign_legacy(&invalid);CHECK(!game_load(&reloaded,(const Save*)(const void*)&invalid));
 printf("legacy_save: %u checks; exact 100-byte v1 migration preserves cash, six-car upgrades and unlocks, adds defaults, rejects corrupt/invalid saves and emits valid v2\n",checks);
 return 0;
}
