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
typedef struct {
 uint32_t magic,version,checksum,cash,owned,unlocked_locations,paints;
 uint32_t best_score,best_distance,best_near,best_drift,best_speed_hold,best_wheelie,best_coins;
 uint32_t total_runs,total_distance,total_cash,seed;
 uint8_t selected_vehicle,selected_location,paint[10],engine[10],grip[10],armor[10];
 uint32_t mastery_xp[10],career_xp;
 uint8_t power_tune[10],handling_tune[10],chassis_tune[10],perks[3];
 uint8_t music_on,sfx_on,music_volume,sfx_volume,skin,shake,units,motion,reserved[3];
} V2Save;
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
static void sign_bytes(void *raw,size_t size) {
 uint8_t *bytes=raw;memset(bytes+8,0,4);uint32_t hash=2166136261u;
 for(size_t i=0;i<size;++i)hash=(hash^bytes[i])*16777619u;
 memcpy(bytes+8,&hash,4);
}
static void check_new_defaults(const Save *s,int first_new) {
 for(int i=0;i<VEHICLE_COUNT;++i) {
  CHECK(!s->recovery[i] && !s->livery[i] && !s->wheels[i]);
  CHECK(s->livery_owned[i]==1 && s->wheels_owned[i]==1);
  if(i>=first_new)CHECK(!s->mastery_xp[i] && !s->power_tune[i] && !s->handling_tune[i] && !s->chassis_tune[i]);
 }
 CHECK(s->dispatch_choice==CONTRACT_OFFERS && !s->contracts_completed);
}
static void v2_and_compact(void) {
 CHECK(sizeof(V2Save)==204 && sizeof(Save)==1032 && SAVE_DISK_SIZE==1000);
 V2Save old={0};old.magic=0x434c4452u;old.version=2;old.cash=34567;old.owned=1023;old.unlocked_locations=255;
 old.paints=63;old.best_score=123456;old.best_distance=12000;old.best_near=25;
 old.best_drift=3400;old.best_speed_hold=1500;old.best_wheelie=1800;old.best_coins=100;
 old.total_runs=91;old.total_distance=900000;old.total_cash=45678;old.seed=0x721fa32u;
 old.selected_vehicle=9;old.selected_location=7;old.career_xp=22000;
 old.perks[0]=old.perks[1]=old.perks[2]=3;
 old.music_on=0;old.sfx_on=1;old.music_volume=35;old.sfx_volume=90;old.skin=2;old.shake=0;old.units=1;old.motion=2;
 for(int i=0;i<10;++i) {
  old.paint[i]=(uint8_t)(i%6);old.engine[i]=(uint8_t)(i%7);old.grip[i]=(uint8_t)((i+1)%7);old.armor[i]=(uint8_t)((i+2)%7);
  old.mastery_xp[i]=3200u+(unsigned)i;old.power_tune[i]=(uint8_t)(i%3);
  old.handling_tune[i]=(uint8_t)((i+1)%3);old.chassis_tune[i]=(uint8_t)((i+2)%3);
 }
 sign_bytes(&old,sizeof(old));Game imported,loaded;game_init(&imported,5);
 /* The raw source really is only204B, so ASan catches accidental v3 reads. */
 CHECK(game_load_bytes(&imported,&old,sizeof(old)) && imported.save_requested);
 uint8_t unaligned_v2[sizeof(old)+1];memcpy(unaligned_v2+1,&old,sizeof(old));
 CHECK(game_load_bytes(&imported,unaligned_v2+1,sizeof(old)));
 CHECK(!memcmp((const uint8_t *)&imported.save+12,(const uint8_t *)&old+12,60));
 CHECK(imported.save.selected_vehicle==9 && imported.save.selected_location==7);
 CHECK(imported.save.career_xp==old.career_xp && !memcmp(imported.save.perks,old.perks,3));
 CHECK(!memcmp(&imported.save.music_on,&old.music_on,4));
 CHECK(imported.save.skin==SKIN_FUSION && !memcmp(&imported.save.shake,&old.shake,3));
 for(int i=0;i<10;++i) {
  CHECK(imported.save.paint[i]==old.paint[i] && imported.save.engine[i]==old.engine[i]);
  CHECK(imported.save.grip[i]==old.grip[i] && imported.save.armor[i]==old.armor[i]);
  CHECK(imported.save.mastery_xp[i]==old.mastery_xp[i] && imported.save.power_tune[i]==old.power_tune[i]);
  CHECK(imported.save.handling_tune[i]==old.handling_tune[i] && imported.save.chassis_tune[i]==old.chassis_tune[i]);
 }
 check_new_defaults(&imported.save,10);
 V2Save invalid_paint=old;invalid_paint.paint[0]=255;sign_bytes(&invalid_paint,sizeof invalid_paint);
 CHECK(!game_load_bytes(&loaded,&invalid_paint,sizeof invalid_paint));
 LegacySave invalid_legacy=profile();invalid_legacy.paint[0]=255;sign_legacy(&invalid_legacy);
 CHECK(!game_load_bytes(&loaded,&invalid_legacy,sizeof invalid_legacy));
 CHECK(game_can_vehicle(&imported,9)); /* Grandfathered S keys stay usable. */
 uint8_t disk[SAVE_DISK_SIZE+12];memset(disk,0x6e,sizeof(disk));CHECK(game_encode_save(&imported,disk,sizeof(disk)));
 for(int i=SAVE_DISK_SIZE;i<(int)sizeof disk;++i)CHECK(disk[i]==0x6e);
 game_init(&loaded,9);CHECK(game_load_bytes(&loaded,disk,sizeof(disk)) && !loaded.save_requested);
 Save a,b;game_make_save(&imported,&a);game_make_save(&loaded,&b);CHECK(!memcmp(&a,&b,sizeof(a)));
 for(size_t i=0;i<sizeof(old);++i) {
  V2Save bad=old;((uint8_t *)&bad)[i]^=0x40;game_init(&loaded,8);
  CHECK(!game_load_bytes(&loaded,&bad,sizeof(bad)));
 }
 /* All31 models survive compact packing, including the highest mask bit. */
 game_init(&imported,5);imported.save.owned=0x7fffffffu;imported.save.unlocked_locations=(1u<<LOCATION_COUNT)-1;
 imported.save.selected_vehicle=30;imported.save.selected_location=12;imported.save.paints=63;
 imported.save.career_xp=22000;imported.save.perks[0]=imported.save.perks[1]=imported.save.perks[2]=3;
 imported.save.dispatch_choice=2;imported.save.contracts_completed=123;
 for(int i=0;i<VEHICLE_COUNT;++i) {
  imported.save.mastery_xp[i]=3200u+(unsigned)i;imported.save.paint[i]=(uint8_t)(i%6);
  imported.save.engine[i]=(uint8_t)(i%7);imported.save.grip[i]=(uint8_t)((i+1)%7);imported.save.armor[i]=(uint8_t)((i+2)%7);
  imported.save.recovery[i]=(uint8_t)((i+3)%7);imported.save.power_tune[i]=(uint8_t)(i%3);
  imported.save.handling_tune[i]=(uint8_t)((i/3)%3);imported.save.chassis_tune[i]=(uint8_t)((i/9)%3);
  imported.save.livery[i]=(uint8_t)(i%5);imported.save.wheels[i]=(uint8_t)(i%4);
  imported.save.paint_seen[i]=63;imported.save.smoke_seen[i]=31;imported.save.smoke[i]=(uint8_t)(i%SMOKE_COUNT);imported.save.livery_owned[i]=31;imported.save.wheels_owned[i]=15;
 }
 CHECK(game_encode_save(&imported,disk,sizeof(disk)));game_init(&loaded,11);
 CHECK(game_load_bytes(&loaded,disk,sizeof(disk)));game_make_save(&imported,&a);game_make_save(&loaded,&b);
 CHECK(!memcmp(&a,&b,sizeof(a)) && loaded.save.selected_vehicle==30 && loaded.save.selected_location==12);
 uint8_t unaligned_v3[SAVE_DISK_SIZE+1];memcpy(unaligned_v3+1,disk,SAVE_DISK_SIZE);
 CHECK(game_load_bytes(&loaded,unaligned_v3+1,SAVE_DISK_SIZE));
 for(unsigned size=0;size<SAVE_DISK_SIZE;++size)CHECK(!game_load_bytes(&loaded,disk,size));
 for(int i=0;i<SAVE_DISK_SIZE;++i) {
  uint8_t bad[SAVE_DISK_SIZE];memcpy(bad,disk,sizeof(bad));bad[i]^=0x40;
  CHECK(!game_load_bytes(&loaded,bad,sizeof(bad)));
 }
 /* Re-signing reserved bits, invalid packed fields, and unowned selections
  * must still fail semantic validation rather than just the checksum. */
 static const unsigned offsets[]={72,73,274,275,276,280,281,282,285,286,999};
 static const uint8_t values[]={48,13,2,2,101,2,3,4,4,8,1};
 for(unsigned i=0;i<sizeof(offsets)/sizeof(offsets[0]);++i) {
  uint8_t bad[SAVE_DISK_SIZE];memcpy(bad,disk,sizeof(bad));bad[offsets[i]]=values[i];
  if(offsets[i]==496)memset(bad+496,0xff,4);
  sign_bytes(bad,sizeof(bad));CHECK(!game_load_bytes(&loaded,bad,sizeof(bad)));
 }
 CHECK(!game_encode_save(&imported,disk,SAVE_DISK_SIZE-1));
 CHECK(!game_load_bytes(&loaded,NULL,500));
}
int main(void) {
 CHECK(sizeof(LegacySave)==100);CHECK(sizeof(Save)>sizeof(LegacySave));
 LegacySave old=profile();Game g,defaults,reloaded;Save migrated;
 game_init(&defaults,123);game_init(&g,7);
 CHECK(game_load(&g,(const Save*)(const void*)&old));
 uint8_t unaligned_v1[sizeof(old)+1];memcpy(unaligned_v1+1,&old,sizeof(old));
 CHECK(game_load_bytes(&g,unaligned_v1+1,sizeof(old)));
 CHECK(g.mode==M_HOME && g.has_save && g.save_requested && g.save.version==5);
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
 game_make_save(&g,&migrated);CHECK(migrated.version==5 && game_save_valid(&migrated));
 check_new_defaults(&g.save,6);
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
 v2_and_compact();
 printf("legacy_save: %u checks; exact v1/v2 imports, 48-car v5 roundtrip in1000B, defaults, truncation and corrupt/invalid-save rejection pass\n",checks);
 return 0;
}
