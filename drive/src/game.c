#include "game.h"
#include "road.h"
#include "traffic.h"
#include <stddef.h>
#include <string.h>

#define SAVE_MAGIC 0x434c4452u
#define SAVE_VERSION 5u
#define MONEY_LIMIT 9999999u
#define STAT_LIMIT 999999999u
#define WORLD_LIMIT (7000000*Q)
#define PAINT_COUNT 6

const VehicleDef vehicles[VEHICLE_COUNT] = {
 {"Pacer","A nimble everyday hatchback.","Your first set of keys",0,175,46,78,20,0,0,CLASS_D,VS_VALUE},
 {"Vela GT","A long-legged coastal coupe.","Own 2 vehicles; buy for $1800",1800,235,54,78,18,0,0,CLASS_C,VS_TOP},
 {"Nomad","A sturdy pickup that takes a hit.","Buy for $1000",1000,185,42,85,45,0,0,CLASS_D,VS_ARMOR},
 {"Kestrel","A light motorcycle. A holds a wheelie.","Buy for $650",650,205,60,92,4,1,0,CLASS_D,VS_ACCEL},
 {"Vector","A quick, planted supercar.","Own 8 vehicles; buy for $7800",7800,285,78,98,40,0,0,CLASS_A,VS_BALANCED},
 {"Sundown","A classic built for long drifts.","1500 drift points in one run",0,225,56,65,25,0,3,CLASS_C,VS_BALANCED},
 {"Mariner","An eager coastal roadster.","Own 2 vehicles; buy for $1700",1700,215,70,87,14,0,0,CLASS_C,VS_ACCEL},
 {"Regent","A composed luxury touring sedan.","Own 5 vehicles; buy for $4400",4400,240,57,94,85,0,0,CLASS_B,VS_ARMOR},
 {"Rallye","A versatile sport wagon.","Own 5 vehicles; buy for $4200",4200,245,66,100,36,0,0,CLASS_B,VS_BALANCED},
 {"Tempest","An exotic built for the longest straights.","Own 10 vehicles; buy for $16000",16000,345,78,96,30,0,0,CLASS_S,VS_TOP},
 {"Tern","A friendly, balanced compact.","Buy for $300",300,190,52,83,24,0,0,CLASS_D,VS_BALANCED},
 {"Comet","A little coupe with a big final gear.","Buy for $550",550,215,40,73,13,0,0,CLASS_D,VS_TOP},
 {"Courier","A useful van with an honest price.","Own 2 vehicles; buy for $1200",1200,205,54,82,28,0,0,CLASS_C,VS_VALUE},
 {"Sentinel","A reassuringly solid highway SUV.","Own 2 vehicles; buy for $2300",2300,210,48,80,65,0,0,CLASS_C,VS_ARMOR},
 {"Riptide","A capable sport sedan without the premium.","Own 5 vehicles; buy for $3200",3200,235,61,90,30,0,0,CLASS_B,VS_VALUE},
 {"Solstice","A grand tourer that loves long straights.","Own 5 vehicles; buy for $5000",5000,270,58,85,22,0,0,CLASS_B,VS_TOP},
 {"Flash","A light rally coupe with instant response.","Own 5 vehicles; buy for $4700",4700,245,84,96,16,0,0,CLASS_B,VS_ACCEL},
 {"Apex","The attainable entry into serious speed.","Own 8 vehicles; buy for $6500",6500,270,72,96,35,0,0,CLASS_A,VS_VALUE},
 {"Stratus","A low-slung GT that cuts cleanly through air.","Own 8 vehicles; buy for $9000",9000,310,66,90,28,0,0,CLASS_A,VS_TOP},
 {"Pulse","A small mid-engine burst of acceleration.","Own 8 vehicles; buy for $8500",8500,278,100,102,20,0,0,CLASS_A,VS_ACCEL},
 {"Bastion","A broad performance offroader with real armor.","Own 8 vehicles; buy for $8800",8800,275,64,92,115,0,0,CLASS_A,VS_ARMOR},
 {"Zenith","A focused entry into the hypercar class.","Own 10 vehicles; buy for $12000",12000,305,84,104,42,0,0,CLASS_S,VS_VALUE},
 {"Sovereign","A poised hyper-GT with no weak corner.","Own 10 vehicles; buy for $14500",14500,320,92,110,58,0,0,CLASS_S,VS_BALANCED},
 {"Ion","Electric track machinery with immediate thrust.","Own 10 vehicles; buy for $15500",15500,315,118,112,24,0,0,CLASS_S,VS_ACCEL},
 {"Atlas","A huge performance truck that refuses to quit.","Own 10 vehicles; buy for $15000",15000,310,76,100,145,0,0,CLASS_S,VS_ARMOR},
 {"Roller Skates","A very optimistic pair of highway skates.","Buy for $950",950,185,78,120,0,0,0,CLASS_D,VS_ACCEL},
 {"Bicycle","Pedal power, clean lines, and a wheelie.","Buy for $1250",1250,198,68,114,2,1,0,CLASS_D,VS_BALANCED},
 {"Moped","Small wheels, warm evenings, no hurry.","Buy for $1500",1500,222,76,102,5,1,0,CLASS_C,VS_TOP},
 {"Segway","Stand tall and keep the balance.","Complete 3 Dispatch Contracts; $2100",2100,180,92,125,3,0,5,CLASS_D,VS_ACCEL},
 {"Skateboard","Write your own line through the highway.","3000 drift points in one run; $1400",1400,190,82,124,0,0,7,CLASS_D,VS_BALANCED},
 {"Horse","Four hooves and an unusually open road.","20 near misses in one run; $3500",3500,228,80,104,8,0,6,CLASS_C,VS_BALANCED},
 {"Hellcat","Supercharged muscle: premium B pace, serious appetite.","Premium option; buy for $16000",16000,310,96,84,42,0,0,CLASS_B,VS_TOP},
 {"Supra","A rear-drive icon with a large wing and lively turbo punch.","Premium option; buy for $6400",6400,263,85,94,23,0,0,CLASS_C,VS_TOP},
 {"Convertible","Open-air cruising with a light, responsive chassis.","Premium option; buy for $2400",2400,220,65,91,12,0,0,CLASS_D,VS_BALANCED},
 {"Chevy Tahoe","A tall family SUV with exceptional impact protection.","Premium option; buy for $6900",6900,236,64,82,105,0,0,CLASS_C,VS_ARMOR},
 {"Dodge Charger","Four doors, rear-wheel drive and generous straight-line pace.","Premium option; buy for $5800",5800,258,79,86,39,0,0,CLASS_C,VS_BALANCED},
 {"Lamborghini","Angular V12 exotic: fierce speed and a firm road stance.","Premium option; buy for $25500",25500,345,102,108,30,0,0,CLASS_A,VS_TOP},
 {"Koenigsegg","A featherweight halo hypercar with immense top-end pace.","Signature option; buy for $55000",55000,405,120,113,22,0,0,CLASS_S,VS_TOP},
 {"Ferrari","A sculpted mid-engine exotic with sharp, balanced responses.","Premium option; buy for $23500",23500,332,111,116,24,0,0,CLASS_A,VS_BALANCED},
 {"Bugatti","Heavy luxury and huge power: unmatched straight-line speed.","Signature option; buy for $62000",62000,420,110,101,80,0,0,CLASS_S,VS_TOP},
 {"Cadillac","A muscular luxury sedan combining acceleration and resilience.","Premium option; buy for $13800",13800,290,94,96,88,0,0,CLASS_B,VS_BALANCED},
 {"Rolls Royce","An opulent collector cruiser: comfort and armor over agility.","Signature option; buy for $19500",19500,272,67,88,130,0,0,CLASS_B,VS_ARMOR},
 {"Ford F-150","A premium workhorse with torque and a deep reserve of armor.","Premium option; buy for $3100",3100,218,68,83,72,0,0,CLASS_D,VS_ARMOR},
 {"Ducati","A lightweight superbike with explosive acceleration. A: wheelie.","Premium option; buy for $14900",14900,302,121,112,7,1,0,CLASS_B,VS_ACCEL},
 {"Kawasaki","An upright naked bike: fast response and easy road placement.","Premium option; buy for $5900",5900,250,102,118,6,1,0,CLASS_C,VS_ACCEL},
 {"Ninja","A faired hypersport bike built for high-speed lines. A: wheelie.","Premium option; buy for $24500",24500,338,128,119,6,1,0,CLASS_A,VS_ACCEL},
 {"Insurgent","Conquest Knight XV: towering armor, heavy deliberate steering.","Signature option; buy for $22500",22500,252,57,76,220,0,0,CLASS_B,VS_ARMOR},
 {"Batmobile","A jet-age collector special with power, grip and armored bodywork.","Signature option; buy for $42000",42000,365,110,101,112,0,0,CLASS_A,VS_BALANCED}
};
const LocationDef locations[LOCATION_COUNT] = {
 {"Metro Dawn","Glass towers, warm dawn and four open lanes.","Open from the start",0,0},
 {"Palm Coast","A tropical highway beside the sea.","Buy for $600",600,0},
 {"Skywater Causeway","An endless ribbon above blue water.","10 near misses in one run",0,1},
 {"Pine Ridge","Cool mountain air and forested shoulders.","Travel 6000 m in one run",0,2},
 {"Redrock Pass","Warm sandstone and sweeping canyon roads.","Earn 1500 drift points in one run",0,3},
 {"Marina Nights","Harbor lights and a glittering waterfront.","Buy for $1200",1200,0},
 {"Mangrove Run","Tropical wetlands and quiet green channels.","Reach career level 2",0,4},
 {"Citrus Parkway","A sunny route between rolling orchards.","Buy for $450",450,0},
 {"Neon Current","Wet city lights and electric reflections.","Buy for $1800",1800,0},
 {"Jade Canopy","Tall jungle crowns above a winding road.","Buy for $2200",2200,0},
 {"Ghostwater Bayou","Cypress roots and soft bayou mist.","Complete 3 Dispatch Contracts",0,5},
 {"Aurora Reach","Cold lakes beneath a moving northern sky.","20 near misses in one run",0,6},
 {"Prism Saltway","A bright road across mirrored salt flats.","Buy for $2800",2800,0}
};
const char *const bonus_names[BONUS_COUNT] = {
 "Distance", "Time on the road", "Near miss", "Full speed", "Drift", "Wheelie", "Road coin"
};

static int clampi(int n,int lo,int hi) { return n<lo?lo:n>hi?hi:n; }
static int absi(int n) { return n<0?-n:n; }
static uint32_t add_sat(uint32_t a,uint32_t b,uint32_t limit) {
 return a>=limit || b>limit-a ? limit : a+b;
}
static void notice(Game *g,const char *s) {
 size_t n=strlen(s); if(n>=sizeof(g->notice)) n=sizeof(g->notice)-1;
 memcpy(g->notice,s,n); g->notice[n]=0; g->notice_timer=120;
}
typedef struct {
 uint32_t magic,version,checksum,cash,owned,unlocked_locations,paints;
 uint32_t best_score,best_distance,best_near,best_drift,best_speed_hold,best_wheelie,best_coins;
 uint32_t total_runs,total_distance,total_cash,seed;
 uint8_t selected_vehicle,selected_location;
 uint8_t paint[6],engine[6],grip[6],armor[6],reserved[2];
} LegacySave;
typedef struct {
 uint32_t magic,version,checksum,cash,owned,unlocked_locations,paints;
 uint32_t best_score,best_distance,best_near,best_drift,best_speed_hold,best_wheelie,best_coins;
 uint32_t total_runs,total_distance,total_cash,seed;
 uint8_t selected_vehicle,selected_location,paint[10],engine[10],grip[10],armor[10];
 uint32_t mastery_xp[10],career_xp;
 uint8_t power_tune[10],handling_tune[10],chassis_tune[10],perks[3];
 uint8_t music_on,sfx_on,music_volume,sfx_volume,skin,shake,units,motion,reserved[3];
} SaveV2;
/* Exactly 500 serialized bytes fit both existing 512-byte emulator batteries
 * and the cartridge EEPROM. Unpacked arrays remain convenient in RAM. */
typedef struct {
 uint8_t prefix[328],tunes[31],recovery[31],style[31],livery_owned[31],wheels_owned[31];
 uint8_t perks[3],settings[8],dispatch_choice,reserved;
 uint32_t contracts_completed;
} DiskSave3;
_Static_assert(sizeof(LegacySave)==100,"Version 1 saves must remain exactly 100 bytes");
_Static_assert(sizeof(SaveV2)==204,"Version 2 saves must remain exactly 204 bytes");
_Static_assert(sizeof(DiskSave3)==500,"Version 3 must fit a 512-byte battery");

static uint32_t checksum_bytes(const void *save,size_t size) {
 const uint8_t *p=(const uint8_t *)save; uint32_t h=2166136261u;
 for(size_t i=0;i<size;++i) {
  uint8_t b=(i>=offsetof(Save,checksum)&&i<offsetof(Save,checksum)+4)?0:p[i];
  h=(h^b)*16777619u;
 }
 return h;
}
static void default_settings(Save *s) {
 s->music_on=1;s->sfx_on=1;s->music_volume=70;s->sfx_volume=75;
 s->skin=SKIN_FUSION;s->shake=1;s->units=0;s->motion=1;
}
void game_make_save(const Game *g,Save *out) {
 *out=g->save; out->magic=SAVE_MAGIC; out->version=SAVE_VERSION;
 memset((uint8_t *)out+offsetof(Save,armor)+sizeof(out->armor),0,
        offsetof(Save,mastery_xp)-offsetof(Save,armor)-sizeof(out->armor));
 memset(out->reserved,0,sizeof(out->reserved));
 memset((uint8_t *)out+offsetof(Save,smoke_seen)+sizeof(out->smoke_seen),0,
        offsetof(Save,presets)-offsetof(Save,smoke_seen)-sizeof(out->smoke_seen));
 memset((uint8_t *)out+offsetof(Save,preset_mask)+sizeof(out->preset_mask),0,
        offsetof(Save,owned_hi)-offsetof(Save,preset_mask)-sizeof(out->preset_mask));
 out->checksum=0; out->checksum=checksum_bytes(out,sizeof(*out));
}
static int valid_common(const Save *s,int count,int upgrade_max,int location_count) {
 if(s->cash>MONEY_LIMIT || s->total_cash>STAT_LIMIT || !s->seed) return 0;
 uint32_t mask=count>=32?UINT32_MAX:(1u<<count)-1;
 if(s->owned_hi & ~(count<=32?0u:(1u<<(count-32))-1))return 0;
 if(!(s->owned&1u) || (s->owned&~mask) || !(s->unlocked_locations&1u) || (s->unlocked_locations&~((1u<<location_count)-1))) return 0;
 if((s->paints&7u)!=7u || (s->paints&~63u)) return 0;
 if(s->selected_vehicle>=count || !save_owns(s,s->selected_vehicle)) return 0;
 if(s->selected_location>=location_count || !(s->unlocked_locations&(1u<<s->selected_location))) return 0;
 for(int i=0;i<count;++i) {
  if(s->engine[i]>upgrade_max || s->grip[i]>upgrade_max || s->armor[i]>upgrade_max || s->paint[i]>=PAINT_COUNT || !(s->paints&(1u<<s->paint[i]))) return 0;
 }
 if(s->best_score>STAT_LIMIT || s->best_distance>STAT_LIMIT || s->best_near>STAT_LIMIT ||
    s->best_drift>STAT_LIMIT || s->best_speed_hold>STAT_LIMIT || s->best_wheelie>STAT_LIMIT ||
    s->best_coins>STAT_LIMIT || s->total_runs>STAT_LIMIT || s->total_distance>STAT_LIMIT) return 0;
 return 1;
}
static void new_save_defaults(Save *s) {
 for(int i=0;i<VEHICLE_COUNT;++i) {s->livery_owned[i]=1;s->wheels_owned[i]=1;}
 s->dispatch_choice=CONTRACT_OFFERS;
 for(int i=0;i<VEHICLE_COUNT;i++){s->paint_seen[i]=(uint8_t)(1u | (s->paint[i]<PAINT_COUNT?1u<<s->paint[i]:0u));s->smoke_seen[i]=1;}
}
static void v2_convert(const SaveV2 *old,Save *out) {
 memset(out,0,sizeof(*out));memcpy(out,old,offsetof(SaveV2,paint));
 for(int i=0;i<10;++i) {
  out->paint[i]=old->paint[i];out->engine[i]=old->engine[i];out->grip[i]=old->grip[i];out->armor[i]=old->armor[i];
  out->mastery_xp[i]=old->mastery_xp[i];out->power_tune[i]=old->power_tune[i];
  out->handling_tune[i]=old->handling_tune[i];out->chassis_tune[i]=old->chassis_tune[i];
 }
 out->career_xp=old->career_xp;memcpy(out->perks,old->perks,sizeof(out->perks));
 memcpy(&out->music_on,&old->music_on,8);out->skin=SKIN_FUSION;new_save_defaults(out);out->version=SAVE_VERSION;out->checksum=0;
}
static void legacy_convert(const LegacySave *old,Save *out) {
 memset(out,0,sizeof(*out));
 /* The first eighteen 32-bit fields and both selection bytes are unchanged. */
 memcpy(out,old,offsetof(LegacySave,paint));
 for(int i=0;i<6;++i) {
  out->paint[i]=old->paint[i];out->engine[i]=old->engine[i];
  out->grip[i]=old->grip[i];out->armor[i]=old->armor[i];
 }
 out->version=SAVE_VERSION;out->checksum=0;default_settings(out);new_save_defaults(out);
}
static int read_legacy(const Save *raw,LegacySave *old,Save *converted) {
 memcpy(old,raw,sizeof(*old));
 if(old->magic!=SAVE_MAGIC || old->version!=1u || old->checksum!=checksum_bytes(old,sizeof(*old))) return 0;
 legacy_convert(old,converted);
 return valid_common(converted,6,3,5);
}
static int valid_expanded(const Save *s) {
 if(!valid_common(s,VEHICLE_COUNT,UPGRADE_MAX,LOCATION_COUNT))return 0;
 if(s->career_xp>STAT_LIMIT || s->contracts_completed>STAT_LIMIT || s->dispatch_choice>CONTRACT_OFFERS ||
    s->music_on>1 || s->sfx_on>1 || s->music_volume>100 || s->sfx_volume>100 ||
    s->skin>=SKIN_STORAGE_LIMIT || s->motion>2 || s->shake>1 || s->units>1)return 0;
 int spent=0;
 for(int i=0;i<PERK_COUNT;++i) {if(s->perks[i]>3)return 0;spent+=s->perks[i];}
 static const uint32_t career_thresholds[9]={200,600,1300,2400,4000,6500,10000,15000,22000};
 int level=0;while(level<9&&s->career_xp>=career_thresholds[level])++level;
 if(spent>level)return 0;
 for(int i=0;i<VEHICLE_COUNT;++i) {
  if(s->mastery_xp[i]>STAT_LIMIT || s->power_tune[i]>2 || s->handling_tune[i]>2 || s->chassis_tune[i]>2 ||
     s->recovery[i]>UPGRADE_MAX || s->livery[i]>=LIVERY_COUNT || s->wheels[i]>=WHEEL_COUNT || s->smoke[i]>=SMOKE_COUNT ||
     !(s->paint_seen[i]&1u) || (s->paint_seen[i]&~63u) || !(s->paint_seen[i]&(1u<<s->paint[i])) ||
     !(s->smoke_seen[i]&1u) || (s->smoke_seen[i]&~31u) || !(s->smoke_seen[i]&(1u<<s->smoke[i])))return 0;
  if((s->power_tune[i]&&s->mastery_xp[i]<100) || (s->handling_tune[i]&&s->mastery_xp[i]<300) ||
     (s->chassis_tune[i]&&s->mastery_xp[i]<650))return 0;
  if(!(s->livery_owned[i]&1u) || (s->livery_owned[i]&~31u) || !(s->livery_owned[i]&(1u<<s->livery[i])) ||
     !(s->wheels_owned[i]&1u) || (s->wheels_owned[i]&~15u) || !(s->wheels_owned[i]&(1u<<s->wheels[i])))return 0;
 }
 if(s->preset_mask&~7u)return 0;
 for(int i=0;i<PRESET_COUNT;i++)if(s->preset_mask&(1u<<i)){
  uint32_t p=s->presets[i];if(((p&31u)|((p>>21)&32u))>=VEHICLE_COUNT || ((p>>5)&15u)>=LOCATION_COUNT ||
   ((p>>9)&7u)>=PAINT_COUNT || ((p>>12)&7u)>=LIVERY_COUNT || ((p>>17)&7u)>=SMOKE_COUNT ||
   ((p>>20)&3u)>2 || ((p>>22)&3u)>2 || ((p>>24)&3u)>2 || (p>>27))return 0;
 }
 return 1;
}
static int read_v2(const Save *raw,SaveV2 *old,Save *converted) {
 memcpy(old,raw,sizeof(*old));
 if(old->magic!=SAVE_MAGIC || old->version!=2u || old->checksum!=checksum_bytes(old,sizeof(*old)))return 0;
 v2_convert(old,converted);
 return valid_common(converted,10,UPGRADE_MAX,8) && valid_expanded(converted);
}
int game_save_valid(const Save *s) {
 if(!s)return 0;
 uint32_t header[2];memcpy(header,s,sizeof(header));
 if(header[0]!=SAVE_MAGIC)return 0;
 if(header[1]==1u) {LegacySave old;Save converted;return read_legacy(s,&old,&converted);}
 if(header[1]==2u) {SaveV2 old;Save converted;return read_v2(s,&old,&converted);}
 return header[1]==SAVE_VERSION && s->checksum==checksum_bytes(s,sizeof(*s)) && valid_expanded(s);
}
int game_load(Game *g,const Save *save) {
 if(!game_save_valid(save))return 0;
 uint32_t version;memcpy(&version,(const uint8_t *)save+4,sizeof(version));
 if(version==1u) {
  LegacySave old;Save converted;
  if(!read_legacy(save,&old,&converted))return 0;
  converted.career_xp=converted.total_distance/5;
  converted.mastery_xp[converted.selected_vehicle]=converted.career_xp;
  for(int i=0;i<6;++i)if((converted.engine[i]>=3 || converted.grip[i]>=3 || converted.armor[i]>=3) && converted.mastery_xp[i]<100)converted.mastery_xp[i]=100;
  g->save=converted;g->save_requested=1;
 } else if(version==2u) {
  SaveV2 old;Save converted;if(!read_v2(save,&old,&converted))return 0;
  g->save=converted;g->save_requested=1;
 } else {g->save=*save;g->save_requested=0;}
 if(g->save.skin<SKIN_FUSION){g->save.skin=SKIN_FUSION;g->save_requested=1;}
 g->has_save=1;g->save_ok=1;g->mode=M_HOME;g->cursor=0;
 g->selection=g->save.selected_vehicle;g->vehicle=g->save.selected_vehicle;
 g->start_biome=g->save.selected_location;g->biome=g->start_biome;
 g->paint_choice=g->save.paint[g->selection];g->livery_choice=g->save.livery[g->selection];g->wheel_choice=g->save.wheels[g->selection];
 g->smoke_choice=g->save.smoke[g->selection];
 g->garage_tab=g->garage_filter=g->tune_choice=0;g->muted=(uint8_t)!g->save.music_on;
 return 1;
}
/* Version 5 expands the roster inside the existing 8 KiB EEPROM. Legacy
 * profiles stay independently checksummed at their original 500-byte length. */
static void bit_write(uint8_t *p,unsigned *at,unsigned value,unsigned bits){
 for(unsigned i=0;i<bits;i++,(*at)++)if(value&(1u<<i))p[*at/8]|=(uint8_t)(1u<<(*at%8));
}
static unsigned bit_read(const uint8_t *p,unsigned *at,unsigned bits){
 unsigned value=0;for(unsigned i=0;i<bits;i++,(*at)++)if(p[*at/8]&(1u<<(*at%8)))value|=1u<<i;return value;
}
static void pack_vehicle(uint8_t*d,unsigned*at,const Save*e,int i){
 bit_write(d,at,e->paint[i],3);bit_write(d,at,e->engine[i],3);bit_write(d,at,e->grip[i],3);
 bit_write(d,at,e->armor[i],3);bit_write(d,at,e->recovery[i],3);
 bit_write(d,at,e->power_tune[i],2);bit_write(d,at,e->handling_tune[i],2);bit_write(d,at,e->chassis_tune[i],2);
 bit_write(d,at,e->livery[i],3);bit_write(d,at,e->wheels[i],2);bit_write(d,at,e->smoke[i],3);
 bit_write(d,at,e->paint_seen[i],6);bit_write(d,at,e->livery_owned[i],5);
 bit_write(d,at,e->wheels_owned[i],4);bit_write(d,at,e->smoke_seen[i],5);
}
static void unpack_vehicle(const uint8_t*d,unsigned*at,Save*e,int i){
 e->paint[i]=bit_read(d,at,3);e->engine[i]=bit_read(d,at,3);e->grip[i]=bit_read(d,at,3);
 e->armor[i]=bit_read(d,at,3);e->recovery[i]=bit_read(d,at,3);
 e->power_tune[i]=bit_read(d,at,2);e->handling_tune[i]=bit_read(d,at,2);e->chassis_tune[i]=bit_read(d,at,2);
 e->livery[i]=bit_read(d,at,3);e->wheels[i]=bit_read(d,at,2);e->smoke[i]=bit_read(d,at,3);
 e->paint_seen[i]=bit_read(d,at,6);e->livery_owned[i]=bit_read(d,at,5);
 e->wheels_owned[i]=bit_read(d,at,4);e->smoke_seen[i]=bit_read(d,at,5);
}
int game_encode_save(const Game*g,void*out,unsigned size){
 if(!out || size<SAVE_DISK_SIZE)return 0;
 Save e;game_make_save(g,&e);if(!valid_expanded(&e))return 0;
 uint8_t d[SAVE_DISK_SIZE]={0};memcpy(d,&e,74);memcpy(d+74,e.mastery_xp,192);
 memcpy(d+266,&e.career_xp,4);memcpy(d+270,&e.contracts_completed,4);
 memcpy(d+274,&e.music_on,8);memcpy(d+282,e.perks,3);d[285]=e.dispatch_choice;d[286]=e.preset_mask;
 memcpy(d+287,e.presets,12);memcpy(d+299,&e.owned_hi,4);unsigned at=303*8;
 for(int i=0;i<VEHICLE_COUNT;i++)pack_vehicle(d,&at,&e,i);
 uint32_t check=checksum_bytes(d,sizeof d);memcpy(d+8,&check,4);memcpy(out,d,sizeof d);return 1;
}
int game_load_bytes(Game*g,const void*raw,unsigned size){
 if(!raw || size<8)return 0;
 uint32_t header[3];memcpy(header,raw,8);if(header[0]!=SAVE_MAGIC)return 0;
 if(header[1]==1u){if(size<sizeof(LegacySave))return 0;LegacySave old;memcpy(&old,raw,sizeof old);return game_load(g,(const Save*)(const void*)&old);}
 if(header[1]==2u){if(size<sizeof(SaveV2))return 0;SaveV2 old;memcpy(&old,raw,sizeof old);return game_load(g,(const Save*)(const void*)&old);}
 if(header[1]!=3u && header[1]!=4u && header[1]!=SAVE_VERSION)return 0;
 unsigned bytes=header[1]==SAVE_VERSION?SAVE_DISK_SIZE:500;
 if(size<bytes)return 0;
 const uint8_t*d=raw;memcpy(header,d,12);if(header[2]!=checksum_bytes(d,bytes))return 0;
 Save e;memset(&e,0,sizeof e);default_settings(&e);new_save_defaults(&e);memcpy(&e,d,74);
 if(header[1]==3u){
  DiskSave3 disk;memcpy(&disk,raw,sizeof disk);if(disk.reserved)return 0;
  for(int i=0;i<31;i++){
   e.paint[i]=d[74+i];e.engine[i]=d[105+i];e.grip[i]=d[136+i];e.armor[i]=d[167+i];
   if((disk.tunes[i]&0xc0u)||(disk.style[i]&0xe0u))return 0;
   e.power_tune[i]=disk.tunes[i]&3u;e.handling_tune[i]=(disk.tunes[i]>>2)&3u;e.chassis_tune[i]=(disk.tunes[i]>>4)&3u;
   e.recovery[i]=disk.recovery[i];e.livery[i]=disk.style[i]&7u;e.wheels[i]=(disk.style[i]>>3)&3u;
   e.livery_owned[i]=disk.livery_owned[i];e.wheels_owned[i]=disk.wheels_owned[i];
   e.paint_seen[i]=(uint8_t)(1u|(e.paint[i]<PAINT_COUNT?1u<<e.paint[i]:0u));
  }
  memcpy(e.mastery_xp,d+200,124);memcpy(&e.career_xp,d+324,4);memcpy(e.perks,disk.perks,3);memcpy(&e.music_on,disk.settings,8);
  e.dispatch_choice=disk.dispatch_choice;e.contracts_completed=disk.contracts_completed;e.skin=SKIN_FUSION;
 }else{
  int count=header[1]==SAVE_VERSION?VEHICLE_COUNT:31,extra=(count-31)*4;
  memcpy(e.mastery_xp,d+74,count*4);memcpy(&e.career_xp,d+198+extra,4);memcpy(&e.contracts_completed,d+202+extra,4);
  memcpy(&e.music_on,d+206+extra,8);memcpy(e.perks,d+214+extra,3);e.dispatch_choice=d[217+extra];e.preset_mask=d[218+extra];
  memcpy(e.presets,d+219+extra,12);unsigned at=(231+extra)*8;
  if(header[1]==SAVE_VERSION){memcpy(&e.owned_hi,d+299,4);at=303*8;}
  for(int i=0;i<count;i++)unpack_vehicle(d,&at,&e,i);
  for(unsigned k=at;k<bytes*8;k++)if(d[k/8]&(1u<<(k%8)))return 0;
 }
 if(header[1]!=SAVE_VERSION && ((e.owned&0x80000000u)||e.selected_vehicle>=31))return 0;
 e.version=SAVE_VERSION;e.checksum=0;e.checksum=checksum_bytes(&e,sizeof e);
 int loaded=game_load(g,&e);if(loaded && header[1]!=SAVE_VERSION)g->save_requested=1;return loaded;
}
void game_save_result(Game *g,int okay) {
 g->save_requested=0; g->save_ok=(uint8_t)(okay!=0);
 if(okay) g->has_save=1;
 else notice(g,"Save unavailable. Keep playing; retry after a run.");
}
void game_init(Game *g,uint32_t seed) {
 memset(g,0,sizeof(*g)); g->save.magic=SAVE_MAGIC; g->save.version=SAVE_VERSION;
 g->save.seed=seed?seed:0x73bda521u; g->save.owned=1; g->save.unlocked_locations=1;
 default_settings(&g->save);new_save_defaults(&g->save);
 g->save.paints=7; g->mode=M_HOME; g->health=100; g->combo_q8=Q;
 g->turbo_charge_q8=TURBO_MAX_Q8;g->active_contract.kind=DC_NONE;
}

static int challenge_done(const Game *g,int id) {
 switch(id) {
  case 1:return g->save.best_near>=10;
  case 2:return g->save.best_distance>=6000;
  case 3:return g->save.best_drift>=1500;
  case 4:return game_career_level(g)>=2;
  case 5:return g->save.contracts_completed>=3;
  case 6:return g->save.best_near>=20;
  case 7:return g->save.best_drift>=3000;
  default:return 1;
 }
}
int game_owned_count(const Game *g) {
 int count=0;for(int i=0;i<VEHICLE_COUNT;i++)count+=save_owns(&g->save,i);return count;
}
int game_vehicle_class(int id) {return id>=0&&id<VEHICLE_COUNT?vehicles[id].class_id:-1;}
const char *game_class_name(int class_id) {
 static const char *const names[CLASS_COUNT]={"D","C","B","A","S"};
 return class_id>=0&&class_id<CLASS_COUNT?names[class_id]:"?";
}
const char *game_specialty_name(int specialty) {
 static const char *const names[5]={"Value","Balanced","Top speed","Acceleration","Armor"};
 return specialty>=0&&specialty<5?names[specialty]:"";
}
int game_class_requirement(int class_id) {
 static const uint8_t requirements[CLASS_COUNT]={0,2,5,8,10};
 return class_id>=0&&class_id<CLASS_COUNT?requirements[class_id]:VEHICLE_COUNT+1;
}
int game_class_unlocked(const Game *g,int class_id) {return game_owned_count(g)>=game_class_requirement(class_id);}
const char *game_vehicle_lock_reason(const Game *g,int id) {
 static const char *const gates[CLASS_COUNT]={"Class D is open","Own 2 vehicles to open Class C","Own 5 vehicles to open Class B","Own 8 vehicles to open Class A","Own 10 vehicles to open Class S"};
 if(id<0||id>=VEHICLE_COUNT)return "Unknown vehicle";
 if(save_owns(&g->save,id))return "Owned: ready to equip";
 if(!game_class_unlocked(g,vehicles[id].class_id))return gates[vehicles[id].class_id];
 return challenge_done(g,vehicles[id].unlock)?"Ready to buy":vehicles[id].requirement;
}
int game_can_vehicle(const Game *g,int id) {
 return id>=0&&id<VEHICLE_COUNT && ((save_owns(&g->save,id)) ||
        (game_class_unlocked(g,vehicles[id].class_id)&&challenge_done(g,vehicles[id].unlock)));
}
int game_can_location(const Game *g,int id) {
 return id>=0&&id<LOCATION_COUNT && ((g->save.unlocked_locations&(1u<<id)) || challenge_done(g,locations[id].unlock));
}
static const uint32_t mastery_thresholds[7]={0,100,300,650,1200,2000,3200};
static const uint32_t career_thresholds[10]={0,200,600,1300,2400,4000,6500,10000,15000,22000};
int game_mastery_level(const Game *g,int vehicle) {
 if(vehicle<0 || vehicle>=VEHICLE_COUNT) return 0;
 int rank=0;while(rank<6&&g->save.mastery_xp[vehicle]>=mastery_thresholds[rank+1]) ++rank;
 return rank;
}
uint32_t game_mastery_next(const Game *g,int vehicle) {
 if(vehicle<0 || vehicle>=VEHICLE_COUNT)return 0;
 int rank=game_mastery_level(g,vehicle);
 return rank<6?mastery_thresholds[rank+1]:0;
}
int game_career_level(const Game *g) {
 int level=0;while(level<9&&g->save.career_xp>=career_thresholds[level+1]) ++level;
 return level;
}
uint32_t game_career_next(const Game *g) {
 int level=game_career_level(g);return level<9?career_thresholds[level+1]:0;
}
int game_perk_available(const Game *g) {
 int spent=0;for(int i=0;i<PERK_COUNT;++i)spent+=g->save.perks[i];
 int available=game_career_level(g)-spent;return available>0?available:0;
}
const char *game_tune_name(int kind,int value) {
 static const char *const names[3][3]={{"Stock","Sprint","Overdrive"},{"Balanced","Grip","Slide"},{"Standard","Lightweight","Reinforced"}};
 return kind>=0&&kind<3&&value>=0&&value<3?names[kind][value]:"";
}
const char *game_tune_description(int kind,int value) {
 static const char *const descriptions[3][3]={
  {"Factory power delivery","+12 acceleration / -12 top speed","+24 top speed / -10 acceleration"},
  {"Neutral steering and slide recovery","+12 grip / -5 top speed","-12 grip / +6 acceleration"},
  {"Factory weight and impact protection","+8 acceleration / -16 armor","+24 armor / -7 acceleration"}};
 return kind>=0&&kind<3&&value>=0&&value<3?descriptions[kind][value]:"";
}
static int vehicle_stat(const Game *g,int v,int kind,int preview) {
 if(v<0 || v>=VEHICLE_COUNT || kind<STAT_TOP || kind>STAT_ARMOR) return 0;
 int power=g->save.power_tune[v],handling=g->save.handling_tune[v],chassis=g->save.chassis_tune[v];
 if(preview && g->mode==M_GARAGE && g->garage_tab==1 && g->selection==v && g->build_choice<5 && (g->cursor==0 || g->cursor==1)) {
  static const uint8_t builds[5][3]={{0,0,0},{1,0,0},{2,0,0},{0,1,2},{1,2,0}};
  int build=clampi(g->build_choice,0,4);power=builds[build][0];handling=builds[build][1];chassis=builds[build][2];
 }
 int top=vehicles[v].top_speed+g->save.engine[v]*12;
 int acceleration=vehicles[v].acceleration+g->save.engine[v]*5;
 int grip=vehicles[v].grip+g->save.grip[v]*7;
 int armor=vehicles[v].armor+g->save.armor[v]*20;
 if(power==1) {acceleration+=12;top-=12;} else if(power==2) {top+=24;acceleration-=10;}
 if(handling==1) {grip+=12;top-=5;} else if(handling==2) {grip-=12;acceleration+=6;}
 if(chassis==1) {acceleration+=8;armor-=16;} else if(chassis==2) {armor+=24;acceleration-=7;}
 return kind==STAT_TOP?clampi(top,100,520):kind==STAT_ACCEL?clampi(acceleration,15,160):
        kind==STAT_GRIP?clampi(grip,40,190):clampi(armor,0,300);
}
int game_vehicle_stat(const Game *g,int vehicle,int kind) {return vehicle_stat(g,vehicle,kind,0);}
int game_preview_stat(const Game *g,int vehicle,int kind) {return vehicle_stat(g,vehicle,kind,1);}
int game_top_speed(const Game *g) {
 int v=g->vehicle<VEHICLE_COUNT?g->vehicle:0;
 int creep=(int)(g->run.elapsed_ticks/(30*DRIVE_HZ))*2;
 return game_vehicle_stat(g,v,STAT_TOP) + clampi(creep,0,50);
}
static int upgrade_level(const Game *g,int kind) {
 if(kind<0 || kind>3 || g->selection>=VEHICLE_COUNT)return 0;
 int v=g->selection;return kind==0?g->save.engine[v]:kind==1?g->save.grip[v]:kind==2?g->save.armor[v]:g->save.recovery[v];
}
int game_upgrade_cost(const Game *g,int kind) {
 if(kind<0 || kind>3 || g->selection>=VEHICLE_COUNT) return 0;
 static const uint16_t costs[UPGRADE_MAX]={60,95,145,215,310,440};
 int level=upgrade_level(g,kind);
 if(level>=UPGRADE_MAX) return 0;
 return costs[level]*(100+40*vehicles[g->selection].class_id)*(kind==0?100:kind==1?90:kind==2?95:105)/10000;
}
int game_upgrade_rank(const Game *g,int kind) {
 int next=upgrade_level(g,kind)+1;return next<=2?0:clampi(next-2,1,4);
}
const char *game_challenge_name(int i) {
 static const char *const names[CHALLENGE_COUNT]={"Thread the Needle","Long Way Home","Sideways Summer","Redline Rhythm","One Wheel Wonder","Trusted Dispatch","Twenty Tight Gaps","Boardwalk Flow"};
 return i>=0&&i<CHALLENGE_COUNT?names[i]:"";
}
const char *game_challenge_reward(int i) {
 static const char *const rewards[CHALLENGE_COUNT]={"Skywater Causeway departure","Pine Ridge departure","Sundown + Redrock + Graphite","Sunburst paint","Orchid paint","Ghostwater + Segway purchase access","Aurora + Horse purchase access","Skateboard purchase access"};
 return i>=0&&i<CHALLENGE_COUNT?rewards[i]:"";
}
int game_challenge_progress(const Game *g,int i,int *goal) {
 int n=0,d=1;
 switch(i) {
  case 0:n=(int)g->save.best_near;d=10;break;
  case 1:n=(int)g->save.best_distance;d=6000;break;
  case 2:n=(int)g->save.best_drift;d=1500;break;
  case 3:n=(int)g->save.best_speed_hold;d=8*DRIVE_HZ;break;
  case 4:n=(int)g->save.best_wheelie;d=1000;break;
  case 5:n=(int)g->save.contracts_completed;d=3;break;
  case 6:n=(int)g->save.best_near;d=20;break;
  case 7:n=(int)g->save.best_drift;d=3000;break;
 }
 if(goal) *goal=d;
 return clampi(n,0,d);
}
static void reward_unlocks(Game *g) {
 if(g->save.best_near>=10) g->save.unlocked_locations|=1u<<2;
 if(g->save.best_distance>=6000) g->save.unlocked_locations|=1u<<3;
 if(g->save.best_drift>=1500) { g->save.owned|=1u<<5; g->save.unlocked_locations|=1u<<4; g->save.paints|=1u<<5; }
 if(g->save.best_speed_hold>=8*DRIVE_HZ) g->save.paints|=1u<<3;
 if(g->save.best_wheelie>=1000) g->save.paints|=1u<<4;
 if(game_career_level(g)>=2)g->save.unlocked_locations|=1u<<6;
 if(g->save.contracts_completed>=3)g->save.unlocked_locations|=1u<<SWAMP;
 if(g->save.best_near>=20)g->save.unlocked_locations|=1u<<AURORA;
}
const char *game_livery_name(int value) {
 static const char *const names[LIVERY_COUNT]={"Factory","Twin stripes","Two tone","Checkered","Coastal band"};
 return value>=0&&value<LIVERY_COUNT?names[value]:"";
}
const char *game_wheel_name(int value) {
 static const char *const names[WHEEL_COUNT]={"Factory","Graphite","Polished","Gold"};
 return value>=0&&value<WHEEL_COUNT?names[value]:"";
}
const char *game_smoke_name(int value){
 static const char *const names[SMOKE_COUNT]={"Pearl","Coral","Lagoon","Violet","Gold"};
 return value>=0&&value<SMOKE_COUNT?names[value]:"";
}
const char *game_build_name(int value){
 static const char *const names[6]={"Balanced","Acceleration","Top speed","Heavy","Drift","Custom"};
 return names[clampi(value,0,5)];
}
static const uint8_t build_tunes[5][3]={{0,0,0},{1,0,0},{2,0,0},{0,1,2},{1,2,0}};
int game_build_choice(const Game *g,int v){
 for(int i=0;i<5;i++)if(g->save.power_tune[v]==build_tunes[i][0]&&g->save.handling_tune[v]==build_tunes[i][1]&&g->save.chassis_tune[v]==build_tunes[i][2])return i;
 return 5;
}
int game_cosmetic_cost(const Game *g,int kind,int value) {
 if(g->selection>=VEHICLE_COUNT || kind<0 || kind>3 || value<0 || value>=(kind==0?LIVERY_COUNT:kind==1?WHEEL_COUNT:kind==2?PAINT_COUNT:SMOKE_COUNT))return -1;
 int v=g->selection;
 int current=kind==0?g->save.livery[v]:kind==1?g->save.wheels[v]:kind==2?g->save.paint[v]:g->save.smoke[v];
 if(value==current)return 0;
 if(kind==2 && !(g->save.paints&(1u<<value)))return -1;
 unsigned seen=kind==0?g->save.livery_owned[v]:kind==1?g->save.wheels_owned[v]:kind==2?g->save.paint_seen[v]:g->save.smoke_seen[v];
 int base=kind==0?60:kind==1?35:kind==2?25:40;
 int price=base*(value+1)*(2+vehicles[v].class_id)/2;
 return seen&(1u<<value)?(price+1)/2:price;
}
int game_apply_cosmetics(Game *g) {
 int v=g->selection;
 if(v>=VEHICLE_COUNT || !(save_owns(&g->save,v))) {notice(g,"Own this vehicle before styling it.");return 0;}
 int cost=0,values[4]={g->livery_choice,g->wheel_choice,g->paint_choice,g->smoke_choice};
 for(int i=0;i<4;i++){int price=game_cosmetic_cost(g,i,values[i]);if(price<0){notice(g,"Complete the paint's road challenge first.");return 0;}cost+=price;}
 if(g->save.cash<(uint32_t)cost){notice(g,"Not enough cash. Your style is unchanged.");return 0;}
 g->save.cash-=(uint32_t)cost;
 g->save.livery_owned[v]|=(uint8_t)(1u<<values[0]);g->save.wheels_owned[v]|=(uint8_t)(1u<<values[1]);
 g->save.paint_seen[v]|=(uint8_t)(1u<<values[2]);g->save.smoke_seen[v]|=(uint8_t)(1u<<values[3]);
 g->save.livery[v]=values[0];g->save.wheels[v]=values[1];g->save.paint[v]=values[2];g->save.smoke[v]=values[3];
 g->save_requested=1;notice(g,cost?"Finish installed. Returning styles are 50% off.":"This finish is already installed.");return 1;
}
static uint32_t preset_pack(const Game *g,int v){
 return (uint32_t)(v&31) | (uint32_t)(v>>5)<<26 | (uint32_t)g->save.selected_location<<5 | (uint32_t)g->save.paint[v]<<9 |
  (uint32_t)g->save.livery[v]<<12 | (uint32_t)g->save.wheels[v]<<15 | (uint32_t)g->save.smoke[v]<<17 |
  (uint32_t)g->save.power_tune[v]<<20 | (uint32_t)g->save.handling_tune[v]<<22 | (uint32_t)g->save.chassis_tune[v]<<24;
}
int game_preset_cost(const Game *g,int slot){
 if(slot<0 || slot>=PRESET_COUNT || !(g->save.preset_mask&(1u<<slot)))return -1;
 uint32_t p=g->save.presets[slot];int v=(p&31u)|((p>>21)&32u);
 if(v>=VEHICLE_COUNT || !(save_owns(&g->save,v)) || !(g->save.unlocked_locations&(1u<<((p>>5)&15u))))return -1;
 int rank=game_mastery_level(g,v);
 if((((p>>20)&3u)&&rank<1)||(((p>>22)&3u)&&rank<2)||(((p>>24)&3u)&&rank<3))return -1;
 Game preview=*g;preview.selection=(uint8_t)v;
 int values[4]={(p>>12)&7u,(p>>15)&3u,(p>>9)&7u,(p>>17)&7u},cost=0;
 for(int i=0;i<4;i++){int price=game_cosmetic_cost(&preview,i,values[i]);if(price<0)return -1;cost+=price;}
 return cost;
}
static void preset_load(Game *g){
 int cost=game_preset_cost(g,g->preset_choice);if(cost<0){notice(g,"Save a preset in this slot first.");return;}
 if(g->save.cash<(uint32_t)cost){notice(g,"Not enough cash to refit this preset.");return;}
 uint32_t p=g->save.presets[g->preset_choice];g->selection=(p&31u)|((p>>21)&32u);
 g->paint_choice=(p>>9)&7u;g->livery_choice=(p>>12)&7u;g->wheel_choice=(p>>15)&3u;g->smoke_choice=(p>>17)&7u;
 if(!game_apply_cosmetics(g))return;
 int v=g->selection;g->save.power_tune[v]=(p>>20)&3u;g->save.handling_tune[v]=(p>>22)&3u;g->save.chassis_tune[v]=(p>>24)&3u;
 g->save.selected_vehicle=g->vehicle=(uint8_t)v;g->save.selected_location=(p>>5)&15u;
 g->start_biome=g->biome=g->save.selected_location;g->save_requested=1;
 notice(g,"Preset loaded. Your keys and route are ready.");
}
const char *game_contract_name(int kind) {
 static const char *const names[6]={"Near-miss route","Drift exhibition","Distance delivery","Coin collection","Full-speed relay","Free drive"};
 return kind>=0&&kind<=DC_NONE?names[kind]:"";
}
void game_contract_offer(const Game *g,int slot,DispatchContract *out) {
 if(!out)return;
 memset(out,0,sizeof(*out));out->kind=DC_NONE;
 if(slot<0 || slot>=CONTRACT_OFFERS)return;
 uint32_t board=road_hash(g->save.seed,g->save.total_runs^0xd15ca7c3u);
 uint32_t hash=road_hash(board,(uint32_t)slot+31u);
 unsigned difficulty=g->save.contracts_completed<8?g->save.contracts_completed:8;
 out->kind=(uint8_t)((board%5u+(unsigned)slot*(1u+(board>>8)%4u))%5u);
 switch(out->kind) {
  case DC_NEAR:out->goal=3+difficulty/2+(hash&1u);break;
  case DC_DRIFT:out->goal=300+difficulty*80+(hash%4u)*50;break;
  case DC_DISTANCE:out->goal=1200+difficulty*250+(hash%4u)*200;break;
  case DC_COINS:out->goal=10+difficulty*2+(hash%4u)*3;break;
  default:out->goal=(8+difficulty+(hash%3u)*2)*DRIVE_HZ;break;
 }
 out->reward=90+difficulty*30+(hash%5u)*15;
}
uint32_t game_contract_progress(const Game *g) {
 uint32_t progress=0;
 switch(g->active_contract.kind) {
  case DC_NEAR:progress=g->run.count[B_NEAR];break;
  case DC_DRIFT:progress=g->run.drift_points;break;
  case DC_DISTANCE:progress=g->run.distance_m;break;
  case DC_COINS:progress=g->run.count[B_COIN];break;
  case DC_SPEED:progress=g->run.best_speed_hold;break;
  default:return 0;
 }
 return progress<g->active_contract.goal?progress:g->active_contract.goal;
}
static void run_cash(Game *g) {
 uint32_t coin_cash=g->run.count[B_COIN]>MONEY_LIMIT/5?MONEY_LIMIT:g->run.count[B_COIN]*5;
 uint32_t base=add_sat(coin_cash,g->run.total_points/60,MONEY_LIMIT);
 uint32_t bonus=base*(uint32_t)(8*g->save.perks[2])/100;
 if(bonus>MONEY_LIMIT-base)bonus=MONEY_LIMIT-base;
 g->run.cash_bonus=bonus;g->run.total_cash=base+bonus;
}
static int turbo_clean(const Game *g) {
 return g->mode==M_RUN && !g->turbo && !g->crash_timer && !g->damage_cooldown &&
        g->speed_q8>0 && absi(g->x_q8)<80*Q;
}
static int turbo_charge_factor(const Game *g) {
 /* Five times the score combo doubles fuel income rather than multiplying
  * it by five. The tank remains a resource even during long skill streaks. */
 return Q+(clampi(g->combo_q8,Q,COMBO_MAX_Q8)-Q)/4;
}
static void turbo_add(Game *g,int amount,int source) {
 if(amount<=0 || g->turbo_charge_q8>=TURBO_MAX_Q8) return;
 int fuel=g->turbo_charge_q8+amount;
 g->turbo_charge_q8=(uint16_t)(fuel>TURBO_MAX_Q8?TURBO_MAX_Q8:fuel);
 g->turbo_recharging=1;
 /* Keep a just-earned near miss visible when a continuous source also pays. */
 if(g->turbo_source!=TS_NEAR)g->turbo_source=(uint8_t)source;
 if(g->turbo_charge_q8==TURBO_MAX_Q8)g->turbo_remainder=0;
}
static void turbo_near(Game *g,uint32_t count) {
 if(!turbo_clean(g))return;
 int recovery=100+5*g->save.recovery[g->vehicle];
 int amount=(TURBO_MAX_Q8/10)*turbo_charge_factor(g)*recovery/(Q*100);
 if(count>10)count=10; /* A callback cannot overflow even with malformed input. */
 turbo_add(g,amount*(int)count,TS_NEAR);
}
static void turbo_recharge(Game *g) {
 if(!turbo_clean(g))return;
 if(g->turbo_charge_q8>=TURBO_MAX_Q8) {g->turbo_remainder=0;return;}
 int percent=0,source=TS_NONE,factor=turbo_charge_factor(g);
 /* A wall-bound handbrake or an unmoving steering sprite is not a slide. */
 if(g->drifting && g->drift_hold>=(3*DRIVE_HZ+3)/4 && absi(g->vx_q8)>Q/2) {
  percent=4;source=TS_DRIFT;
 } else if(g->wheelie && g->wheelie_hold>=DRIVE_HZ) {
  percent=3;source=TS_WHEELIE;
 } else if(g->slow && g->speed_q8>40*Q) {
  percent=2;source=TS_SLOW;factor=Q;
 }
 if(!percent)return;
 /* MAX is a whole number of Q units. Keep the remainder over a denominator
  * of 3000 so all three rates are exact over time without floating point. */
 int recovery=100+5*g->save.recovery[g->vehicle];
 int numerator=(TURBO_MAX_Q8/Q)*percent*factor*recovery/100+g->turbo_remainder;
 int amount=numerator/(100*DRIVE_HZ);
 g->turbo_remainder=(uint16_t)(numerator%(100*DRIVE_HZ));
 turbo_add(g,amount,source);
}
static void points(Game *g,int category,uint32_t count,uint32_t amount) {
 /* Keep the receipt sum exact even at the deliberately remote score cap. */
 uint32_t room=STAT_LIMIT-g->run.total_points;
 if(amount>room) amount=room;
 g->run.count[category]=add_sat(g->run.count[category],count,STAT_LIMIT);
 g->run.points[category]+=amount; g->run.total_points+=amount;
 run_cash(g);
 if(category==B_NEAR)turbo_near(g,count);
}
static int combo_for(const Game *g,uint32_t hold,uint32_t base_window) {
 uint32_t window=base_window*(uint32_t)(100-15*g->save.perks[1])/100;
 uint32_t capped=hold>window?window:hold;
 /* Sixteen deliberate quarter steps. Merely reaching top speed must not
  * grant the same quick ramp as actively holding a slide or wheelie. */
 return Q+(int)(capped*16/window)*COMBO_STEP_Q8;
}
void game_begin_run(Game *g) {
 memset(&g->run,0,sizeof(g->run)); memset(g->traffic,0,sizeof(g->traffic)); memset(g->pickups,0,sizeof(g->pickups));
 game_contract_offer(g,g->save.dispatch_choice,&g->active_contract);
 g->contract_progress=g->contract_cash=0;g->contract_complete=0;
 g->run_seed=road_hash(g->save.seed,g->save.total_runs+1+g->frame); if(!g->run_seed) g->run_seed=1;
 g->save.seed=g->run_seed; g->run_ticks=0;
 g->position_q8=g->furthest_q8=g->odometer_q8=g->x_q8=g->vx_q8=g->speed_q8=g->accel_q8=0;
 g->x_q8=-24*Q;
 g->speed_hold=g->drift_hold=g->wheelie_hold=0;
 g->combo_q8=Q; g->damage_cooldown=g->crash_timer=g->horn_timer=g->shake=g->spawn_serial=0;
 g->steering=g->yaw=0; g->health=100; g->vehicle=g->save.selected_vehicle;
 g->start_biome=g->save.selected_location; g->biome=g->start_biome;
 g->turbo=g->slow=g->drifting=g->wheelie=0; g->notice_timer=0;
 g->turbo_charge_q8=TURBO_MAX_Q8;
 g->turbo_latched=g->turbo_recharging=g->turbo_source=0;g->turbo_remainder=0;
 g->police_ticks=0;g->police_pressure=0;g->end_reason=END_NONE;
 g->repair_used=0;
 g->mode=M_RUN; g->cursor=0; g->next_spawn_q8=200*Q; traffic_spawn_ahead(g);
}
void game_finish_run(Game *g) {
 if(g->run.banked) { g->mode=M_RECEIPT; return; }
 if(g->end_reason==END_NONE)g->end_reason=END_BANKED;
 g->run.banked=1;
 run_cash(g);
 g->contract_progress=game_contract_progress(g);g->contract_cash=0;
 if(g->active_contract.kind<DC_NONE && g->active_contract.goal && g->contract_progress>=g->active_contract.goal) {
  g->contract_complete=1;g->contract_cash=g->active_contract.reward;
  if(g->contract_cash>MONEY_LIMIT-g->run.total_cash)g->contract_cash=MONEY_LIMIT-g->run.total_cash;
  g->run.total_cash=add_sat(g->run.total_cash,g->contract_cash,MONEY_LIMIT);
  g->save.contracts_completed=add_sat(g->save.contracts_completed,1,STAT_LIMIT);
 }
 uint32_t skill=g->run.points[B_NEAR]+g->run.points[B_SPEED]+g->run.points[B_DRIFT]+g->run.points[B_WHEELIE];
 uint32_t xp=g->run.distance_m/5+skill/10;
 if(xp>20000)xp=20000;
 g->run.mastery_xp=g->run.career_xp=xp;
 g->save.mastery_xp[g->vehicle]=add_sat(g->save.mastery_xp[g->vehicle],xp,STAT_LIMIT);
 g->save.career_xp=add_sat(g->save.career_xp,xp,STAT_LIMIT);
 g->save.cash=add_sat(g->save.cash,g->run.total_cash,MONEY_LIMIT);
 g->save.total_cash=add_sat(g->save.total_cash,g->run.total_cash,STAT_LIMIT);
 g->save.total_runs=add_sat(g->save.total_runs,1,STAT_LIMIT);
 g->save.total_distance=add_sat(g->save.total_distance,g->run.distance_m,STAT_LIMIT);
 if(g->run.total_points>g->save.best_score) g->save.best_score=g->run.total_points;
 if(g->run.distance_m>g->save.best_distance) g->save.best_distance=g->run.distance_m;
 if(g->run.count[B_NEAR]>g->save.best_near) g->save.best_near=g->run.count[B_NEAR];
 if(g->run.drift_points>g->save.best_drift) g->save.best_drift=g->run.drift_points;
 if(g->run.best_speed_hold>g->save.best_speed_hold) g->save.best_speed_hold=g->run.best_speed_hold;
 if(g->run.wheelie_points>g->save.best_wheelie) g->save.best_wheelie=g->run.wheelie_points;
 if(g->run.count[B_COIN]>g->save.best_coins) g->save.best_coins=g->run.count[B_COIN];
 reward_unlocks(g); g->save_requested=1; g->mode=M_RECEIPT; g->cursor=0;
 g->turbo=g->slow=g->drifting=g->wheelie=0;
 g->turbo_recharging=g->turbo_source=0;
}

static void damage(Game *g,int raw,int rail) {
 if(g->damage_cooldown || g->run_ticks<2*DRIVE_HZ) return;
 int armor=game_vehicle_stat(g,g->vehicle,STAT_ARMOR)/4;
 int hurt=raw-armor; if(hurt<5) hurt=5;
 g->health=(uint8_t)(g->health>hurt?g->health-hurt:0);
 if(g->health<=25 && !g->repair_used && g->save.perks[0]) {
  g->health=(uint8_t)(g->health+2+8*g->save.perks[0]);
  g->repair_used=1;notice(g,"Recovery: emergency repair deployed.");
 }
 g->damage_cooldown=rail?30:38; g->crash_timer=12; g->shake=10;
 g->speed_q8=g->speed_q8*(rail?88:62)/100;
 g->speed_hold=g->drift_hold=g->wheelie_hold=0; g->combo_q8=Q;
}
static void pickup_tick(Game *g,int32_t old_position) {
 int32_t lo=old_position<g->position_q8?old_position:g->position_q8;
 int32_t hi=old_position>g->position_q8?old_position:g->position_q8;
 for(int i=0;i<PICKUP_COUNT;++i) {
  Pickup *p=&g->pickups[i]; if(!p->active) continue;
  int32_t dz=p->z_q8-g->position_q8;
  if(dz < -120*Q || dz>3*ROAD_VIEW*Q) { p->active=0; continue; }
  if(p->z_q8>=lo-12*Q && p->z_q8<=hi+12*Q && absi(p->x_q8-g->x_q8)<14*Q) {
   p->active=0; points(g,B_COIN,1,10);
  }
 }
}
static void bonus_tick(Game *g,int target,int motion) {
 int speed_ok=motion && !g->slow && !g->crash_timer && g->speed_q8>=target*Q*95/100;
 g->speed_hold=speed_ok?g->speed_hold+1:0;
 g->drift_hold=g->drifting?g->drift_hold+1:0;
 g->wheelie_hold=g->wheelie?g->wheelie_hold+1:0;
 if(g->speed_hold>g->run.best_speed_hold) g->run.best_speed_hold=g->speed_hold;
 int best=combo_for(g,g->speed_hold,SPEED_COMBO_TICKS),c=combo_for(g,g->drift_hold,SKILL_COMBO_TICKS);
 if(c>best) best=c;
 c=combo_for(g,g->wheelie_hold,SKILL_COMBO_TICKS);
 if(c>best) best=c;
 g->combo_q8=(uint16_t)best;
 if(g->combo_q8>g->run.max_combo_q8) g->run.max_combo_q8=g->combo_q8;
 if(g->speed_hold && g->speed_hold%DRIVE_HZ==0) points(g,B_SPEED,1,(uint32_t)(20*combo_for(g,g->speed_hold,SPEED_COMBO_TICKS)/Q));
 if(g->drift_hold && g->drift_hold%DRIVE_HZ==0) {
  uint32_t p=(uint32_t)(35*combo_for(g,g->drift_hold,SKILL_COMBO_TICKS)/Q); points(g,B_DRIFT,1,p);
  g->run.drift_points=add_sat(g->run.drift_points,p,STAT_LIMIT);
 }
 if(g->wheelie_hold && g->wheelie_hold%DRIVE_HZ==0) {
  uint32_t p=(uint32_t)(35*combo_for(g,g->wheelie_hold,SKILL_COMBO_TICKS)/Q); points(g,B_WHEELIE,1,p);
  g->run.wheelie_points=add_sat(g->run.wheelie_points,p,STAT_LIMIT);
 }
}
static void police_tick(Game *g) {
 /* A brief launch grace makes starting, reading the dashboard and reversing
  * away from an impact forgiving. Slow mode remains well above this limit. */
 if(g->run_ticks<=POLICE_GRACE_TICKS) {
  g->police_ticks=0;g->police_pressure=0;return;
 }
 if(g->speed_q8<=2*Q) {
  if(g->police_ticks<POLICE_CATCH_TICKS)++g->police_ticks;
 } else {
  g->police_ticks=(uint16_t)(g->police_ticks>2?g->police_ticks-2:0);
 }
 g->police_pressure=(uint8_t)(g->police_ticks*100/POLICE_CATCH_TICKS);
 if(g->police_ticks==POLICE_CATCH_TICKS) {
  g->end_reason=END_STOPPED;game_finish_run(g);
 }
}
static void run_tick(Game *g,const Input *in) {
 if(in->pressed&K_START) { g->mode=M_PAUSE; g->cursor=0; return; }
 ++g->run_ticks;
 if(g->damage_cooldown) --g->damage_cooldown;
 if(g->crash_timer) --g->crash_timer;
 if(g->horn_timer) --g->horn_timer;
 if(g->shake) --g->shake;
 int v=g->vehicle,brake=(in->held&K_L)!=0;
 int gas=(in->held&K_R)!=0 && !brake;
 g->turbo_recharging=g->turbo_source=0;
 g->slow=(uint8_t)(gas && (in->held&K_DOWN));
 if(!(in->held&K_UP))g->turbo_latched=0;
 int turbo_requested=gas && !g->slow && (in->held&K_UP) && !g->turbo_latched;
 g->turbo=(uint8_t)(turbo_requested && g->turbo_charge_q8 &&
                    (g->turbo || g->turbo_charge_q8>=TURBO_START_Q8));
 if(g->turbo) {
  if(g->turbo_charge_q8<=Q) {g->turbo_charge_q8=0;g->turbo_latched=1;}
  else g->turbo_charge_q8-=Q;
  g->turbo_remainder=0;
 }
 uint16_t fuel_before_recharge=g->turbo_charge_q8;
 int base=game_top_speed(g),target=g->slow?base*62/100:g->turbo?base*125/100:base;
 int32_t oldspeed=g->speed_q8;
 int acceleration=game_vehicle_stat(g,v,STAT_ACCEL);
 if(gas) {
  int rate=g->slow?acceleration*65/100:g->turbo?acceleration*14/10:acceleration;
  if(g->speed_q8<0) rate+=70;
  int32_t step=rate*Q/DRIVE_HZ;
  if(g->speed_q8<target*Q) { g->speed_q8+=step; if(g->speed_q8>target*Q) g->speed_q8=target*Q; }
  else { g->speed_q8-=90*Q/DRIVE_HZ; if(g->speed_q8<target*Q) g->speed_q8=target*Q; }
 } else if(brake) {
  g->speed_q8-=(g->speed_q8>0?115:25)*Q/DRIVE_HZ;
  if(g->speed_q8 < -34*Q) g->speed_q8=-34*Q;
 } else {
  int drag=18*Q/DRIVE_HZ;
  if(g->speed_q8>drag) g->speed_q8-=drag;
  else if(g->speed_q8 < -drag) g->speed_q8+=drag;
  else g->speed_q8=0;
 }
 g->steering=(int16_t)(((in->held&K_RIGHT)?1:0)-((in->held&K_LEFT)?1:0));
 int32_t old_x=g->x_q8;
 int old_yaw=g->yaw;
 int hand=(in->held&K_B)!=0;
 g->drifting=(uint8_t)(hand && g->steering && g->speed_q8>45*Q && !g->crash_timer);
 g->wheelie=(uint8_t)(vehicles[v].bike && (in->held&K_A) && g->speed_q8>50*Q && !hand && !g->crash_timer);
 if((in->pressed&K_A) && !vehicles[v].bike) g->horn_timer=30;
 if(hand && g->speed_q8>0) {
  g->speed_q8-=12*Q/DRIVE_HZ;
  if(g->speed_q8<0) g->speed_q8=0;
 }
 int speed=absi(g->speed_q8)/Q;
 int steering_power=clampi(speed,0,120)*4;
 if(g->drifting) steering_power=steering_power*112/100;
 if(g->wheelie) steering_power=steering_power*7/10;
 int desired=g->steering*steering_power;
 if(g->speed_q8<0) desired=-desired;
 int grip=game_vehicle_stat(g,v,STAT_GRIP);
 /* A handbrake slide is a deliberate cornering aid, not free sideways
  * momentum. Turn-in is prompt, countersteer catches the rear immediately,
  * and releasing the steering/handbrake settles the car within a few ticks. */
 int gain=g->drifting?clampi(140+grip/2,160,232):clampi(7*grip/5-8,48,248);
 if(!g->steering && gain<200)gain=200;
 if(desired && absi(g->vx_q8)>Q/4 && ((desired<0)!=(g->vx_q8<0)) && gain<224)gain=224;
 g->vx_q8+=(desired-g->vx_q8)*gain/Q;
 RoadSample road; road_sample(g->run_seed,g->position_q8/Q,g->start_biome,&road);
 g->biome=(road.blend>=128)?road.next_biome:road.biome;
 if(speed>15) g->vx_q8-=road.curve*g->speed_q8/(Q*400);
 g->x_q8+=g->vx_q8;
 int yaw_target=g->steering*(g->drifting?74:35)+g->vx_q8/(g->drifting?18:24);
 g->yaw=(int16_t)(g->yaw+(yaw_target-g->yaw)/3);
 g->yaw=(int16_t)clampi(g->yaw,-100,100);
 if(g->x_q8 > 88*Q) { g->x_q8=88*Q; g->vx_q8=-absi(g->vx_q8)/2; if(speed>12) damage(g,17,1); }
 if(g->x_q8 < -88*Q) { g->x_q8=-88*Q; g->vx_q8=absi(g->vx_q8)/2; if(speed>12) damage(g,17,1); }
 int32_t old_position=g->position_q8;
 g->position_q8+=g->speed_q8/32;
 if(g->position_q8<0) { g->position_q8=0; if(g->speed_q8<0) g->speed_q8=0; }
 if(g->position_q8>WORLD_LIMIT) g->position_q8=WORLD_LIMIT;
 if(g->position_q8>g->furthest_q8) {
  int32_t fresh=g->position_q8-g->furthest_q8;
  g->furthest_q8=g->position_q8;
  g->odometer_q8+=fresh;
  uint32_t metres=(uint32_t)(g->odometer_q8/(3*Q));
  if(metres>g->run.distance_m) points(g,B_DISTANCE,metres-g->run.distance_m,metres-g->run.distance_m);
  g->run.distance_m=metres;
 }
 int motion=gas && g->speed_q8>20*Q;
 if(motion) {
  ++g->run.elapsed_ticks;
  if(g->run.elapsed_ticks%DRIVE_HZ==0) points(g,B_SURVIVAL,1,5);
 }
 traffic_tick_from(g,old_position,old_x,old_yaw,damage,points); pickup_tick(g,old_position); traffic_spawn_ahead(g);
 g->accel_q8=(g->speed_q8-oldspeed)*DRIVE_HZ;
 if(g->speed_q8>0 && (uint32_t)(g->speed_q8/Q)>g->run.max_speed) g->run.max_speed=(uint32_t)(g->speed_q8/Q);
 bonus_tick(g,target,motion);
 g->contract_progress=game_contract_progress(g);
 /* A near miss earlier in this update cannot pay fuel if another NPC hits
  * the player later in the same update. Scores retain their existing rules. */
 if(g->crash_timer || g->damage_cooldown) {
  g->turbo_charge_q8=fuel_before_recharge;g->turbo_recharging=g->turbo_source=0;
 } else turbo_recharge(g);
 if(!g->health) {g->end_reason=END_CRASH;game_finish_run(g);}
 else if(g->position_q8>=WORLD_LIMIT) {g->end_reason=END_LIMIT;game_finish_run(g);}
 else police_tick(g);
}

static int vertical(const Input *in) {
 return ((in->pressed&K_DOWN)?1:0)-((in->pressed&K_UP)?1:0);
}
static int horizontal(const Input *in) {
 return ((in->pressed&K_RIGHT)?1:0)-((in->pressed&K_LEFT)?1:0);
}
static void purchase_vehicle(Game *g) {
 int v=g->selection;
 if(save_owns(&g->save,v)) {
  g->save.selected_vehicle=(uint8_t)v; g->vehicle=(uint8_t)v; g->save_requested=1;
  notice(g,"Keys ready. Vehicle equipped."); return;
 }
 if(!game_can_vehicle(g,v)) { notice(g,game_vehicle_lock_reason(g,v)); return; }
 if(g->save.cash<vehicles[v].price) { notice(g,"Not enough cash. Take another drive."); return; }
 g->save.cash-=vehicles[v].price; save_add_vehicle(&g->save,v);
 g->save.selected_vehicle=(uint8_t)v; g->vehicle=(uint8_t)v; g->save_requested=1;
 notice(g,"New keys! Vehicle bought and equipped.");
}
static void upgrade(Game *g,int kind) {
 int v=g->selection;
 if(!(save_owns(&g->save,v))) { notice(g,"Own this vehicle before upgrading it."); return; }
 int cost=game_upgrade_cost(g,kind);
 if(!cost) { notice(g,"This upgrade is already at level 6."); return; }
 if(game_mastery_level(g,v)<game_upgrade_rank(g,kind)) {notice(g,"Drive this vehicle to raise its mastery rank.");return;}
 if(g->save.cash<(uint32_t)cost) { notice(g,"Not enough cash for this upgrade."); return; }
 g->save.cash-=(uint32_t)cost;
 uint8_t *level=kind==0?g->save.engine:kind==1?g->save.grip:kind==2?g->save.armor:g->save.recovery;
 ++level[v]; g->save_requested=1; notice(g,"Upgrade installed.");
}
static void refresh_vehicle(Game *g){
 int v=g->selection;g->paint_choice=g->save.paint[v];g->livery_choice=g->save.livery[v];
 g->wheel_choice=g->save.wheels[v];g->smoke_choice=g->save.smoke[v];g->build_choice=(uint8_t)game_build_choice(g,v);
}
int game_dealer_count(int class_id){int count=0;for(int i=0;i<VEHICLE_COUNT;i++)if(vehicles[i].class_id==class_id)count++;return count;}
int game_vehicle_tier(int id){
 if(id<0||id>=VEHICLE_COUNT)return 0;
 if(id<31)return vehicles[id].specialty==VS_VALUE?0:1;
 return id==37||id==39||id==41||id==46||id==47?3:2;
}
const char *game_tier_name(int id){static const char*const names[4]={"Entry","Standard","Premium","Signature"};return names[game_vehicle_tier(id)];}
int game_dealer_vehicle(int class_id,int row){
 for(int i=0;i<VEHICLE_COUNT;i++)if(vehicles[i].class_id==class_id){int rank=0;
  for(int j=0;j<VEHICLE_COUNT;j++)if(vehicles[j].class_id==class_id){int a=game_vehicle_tier(j),b=game_vehicle_tier(i);
   if(a<b || (a==b && (vehicles[j].price<vehicles[i].price || (vehicles[j].price==vehicles[i].price && j<i))))rank++;
  }if(rank==row)return i;
 }return 0;
}
static void browse_owned(Game *g,int dx){
 int id=g->selection;for(int i=0;i<VEHICLE_COUNT;i++){id=(id+VEHICLE_COUNT+dx)%VEHICLE_COUNT;if(save_owns(&g->save,id)){g->selection=(uint8_t)id;break;}}refresh_vehicle(g);
}
static void garage_tick(Game *g,const Input *in){
 if(in->pressed&K_B){g->mode=M_HOME;g->cursor=1;return;}
 int tab=((in->pressed&K_R)?1:0)-((in->pressed&K_L)?1:0);
 if(tab){g->garage_tab=(uint8_t)((g->garage_tab+4+tab)%4);g->cursor=0;g->notice_timer=0;
  if(g->garage_tab==3){g->garage_filter=vehicles[g->selection].class_id;g->selection=game_dealer_vehicle(g->garage_filter,0);}
  else if(!(save_owns(&g->save,g->selection)))g->selection=g->save.selected_vehicle;
  refresh_vehicle(g);return;}
 int dy=vertical(in),dx=horizontal(in);
 if(g->garage_tab==3){
  if(dx || (in->pressed&K_SELECT)){g->garage_filter=(uint8_t)((g->garage_filter+5+(dx?dx:1))%5);g->cursor=0;}
  int count=game_dealer_count(g->garage_filter);g->cursor=(uint8_t)((g->cursor+count+1+dy)%(count+1));
  g->selection=(uint8_t)game_dealer_vehicle(g->garage_filter,g->cursor?g->cursor-1:0);refresh_vehicle(g);
  if(in->pressed&K_A){if(!g->cursor)g->cursor=1;else if(save_owns(&g->save,g->selection)){g->garage_tab=0;g->cursor=0;notice(g,"Already in your collection. Grab your keys.");}else purchase_vehicle(g);}
  return;
 }
 int rows=g->garage_tab==1?6:5;g->cursor=(uint8_t)((g->cursor+rows+dy)%rows);
 if(g->garage_tab==0 && g->cursor==0 && dx)browse_owned(g,dx);
 if(g->garage_tab==0){
  if(g->cursor==2&&dx)g->preset_choice=(uint8_t)((g->preset_choice+PRESET_COUNT+dx)%PRESET_COUNT);
 }else if(g->garage_tab==1){if(g->cursor==0&&dx)g->build_choice=(uint8_t)((g->build_choice+5+dx)%5);}
 else if(dx){
  if(g->cursor==0)g->paint_choice=(uint8_t)((g->paint_choice+PAINT_COUNT+dx)%PAINT_COUNT);
  if(g->cursor==1)g->livery_choice=(uint8_t)((g->livery_choice+LIVERY_COUNT+dx)%LIVERY_COUNT);
  if(g->cursor==2)g->wheel_choice=(uint8_t)((g->wheel_choice+WHEEL_COUNT+dx)%WHEEL_COUNT);
  if(g->cursor==3)g->smoke_choice=(uint8_t)((g->smoke_choice+SMOKE_COUNT+dx)%SMOKE_COUNT);
 }
 if(!(in->pressed&K_A))return;
 if(g->garage_tab==0){
  if(g->cursor==0)purchase_vehicle(g);
  else if(g->cursor==1){g->mode=M_DEPARTURES;g->cursor=g->save.selected_location;g->menu_return=M_GARAGE;}
  else if(g->cursor==3){preset_load(g);refresh_vehicle(g);}
  else if(g->cursor==4){g->save.presets[g->preset_choice]=preset_pack(g,g->selection);g->save.preset_mask|=(uint8_t)(1u<<g->preset_choice);g->save_requested=1;notice(g,"Preset saved: vehicle, build, finish and route.");}
 }else if(g->garage_tab==1){
  if(g->cursor==1 && g->build_choice>=5){notice(g,"Your custom build is already installed.");}
  else if(g->cursor==1){int b=clampi(g->build_choice,0,4),rank=game_mastery_level(g,g->selection),need=build_tunes[b][2]?3:build_tunes[b][1]?2:build_tunes[b][0]?1:0;
   if(rank<need)notice(g,"Raise this vehicle's mastery to use this build.");else{g->save.power_tune[g->selection]=build_tunes[b][0];g->save.handling_tune[g->selection]=build_tunes[b][1];g->save.chassis_tune[g->selection]=build_tunes[b][2];g->save_requested=1;notice(g,"Build installed. Setups are free to change.");}}
  else if(g->cursor>=2)upgrade(g,g->cursor-2);
 }else if(g->cursor==4)game_apply_cosmetics(g);
}
static void contract_tick(Game *g,const Input *in) {
 if(in->pressed&K_B) {g->mode=M_HOME;g->cursor=5;return;}
 g->cursor=(uint8_t)((g->cursor+CONTRACT_OFFERS+1+vertical(in))%(CONTRACT_OFFERS+1));
 if(in->pressed&K_A) {
  g->save.dispatch_choice=g->cursor;g->save_requested=1;
  notice(g,g->cursor<CONTRACT_OFFERS?"Dispatch armed for your next run.":"Free drive selected. No contract required.");
  g->mode=M_HOME;g->cursor=0;
 }
}
static void departure_tick(Game *g,const Input *in) {
 if(in->pressed&K_B) {g->mode=M_GARAGE;g->garage_tab=0;g->cursor=1;return;}
 g->cursor=(uint8_t)((g->cursor+LOCATION_COUNT+vertical(in))%LOCATION_COUNT);
 if(!(in->pressed&K_A)) return;
 int id=g->cursor;
 if(!(g->save.unlocked_locations&(1u<<id))) {
  if(!game_can_location(g,id)) {notice(g,locations[id].requirement);return;}
  if(g->save.cash<locations[id].price) {notice(g,"Not enough cash for this departure.");return;}
  g->save.cash-=locations[id].price;g->save.unlocked_locations|=1u<<id;
 }
 g->save.selected_location=(uint8_t)id;g->start_biome=(uint8_t)id;g->biome=(uint8_t)id;
 g->save_requested=1;notice(g,"Departure set. Your next run starts here.");
}
static void career_tick(Game *g,const Input *in) {
 if(in->pressed&K_B) {g->mode=M_HOME;g->cursor=3;return;}
 g->cursor=(uint8_t)((g->cursor+4+vertical(in))%4);
 if(!(in->pressed&K_A))return;
 if(g->cursor==3) {memset(g->save.perks,0,sizeof(g->save.perks));g->save_requested=1;notice(g,"Perks reset. All points returned.");}
 else if(g->save.perks[g->cursor]>=3)notice(g,"This perk is already at rank 3.");
 else if(!game_perk_available(g))notice(g,"Reach the next career level for a perk point.");
 else {++g->save.perks[g->cursor];g->save_requested=1;notice(g,"Career perk assigned.");}
}
static void settings_tick(Game *g,const Input *in) {
 if(in->pressed&K_B) {g->mode=g->menu_return==M_PAUSE?M_PAUSE:M_HOME;g->cursor=g->mode==M_PAUSE?3:4;return;}
 if(in->pressed&(K_L|K_R)) {
  int section=g->cursor<4?0:g->cursor<8?1:2;
  int direction=((in->pressed&K_R)?1:0)-((in->pressed&K_L)?1:0);
  if(direction)g->cursor=(uint8_t)(((section+3+direction)%3)*4);
  return;
 }
 g->cursor=(uint8_t)((g->cursor+9+vertical(in))%9);
 int dx=horizontal(in),confirm=(in->pressed&K_A)!=0;
 if(g->cursor==8) {if(confirm){g->mode=M_CONTROLS;g->paused_from=M_SETTINGS;g->controls_page=0;}return;}
 if(!dx&&!confirm)return;
 uint8_t *value;
 int choices=2;
 switch(g->cursor) {
  case 0:value=&g->save.music_on;break;
  case 1:value=&g->save.music_volume;break;
  case 2:value=&g->save.sfx_on;break;
  case 3:value=&g->save.sfx_volume;break;
  case 4:value=&g->save.skin;choices=SKIN_COUNT;break;
  case 5:value=&g->save.motion;choices=3;break;
  case 6:value=&g->save.shake;break;
  default:value=&g->save.units;break;
 }
 if(g->cursor==1||g->cursor==3) {
  if(confirm)*value=(uint8_t)(*value>=100?0:clampi(*value+5,0,100));
  else *value=(uint8_t)clampi(*value+dx*5,0,100);
 } else if(g->cursor==4)*value=(uint8_t)(SKIN_FUSION+(*value-SKIN_FUSION+SKIN_COUNT+(dx?dx:1))%SKIN_COUNT);
 else *value=(uint8_t)((*value+choices+(dx?dx:1))%choices);
 g->muted=(uint8_t)!g->save.music_on;g->save_requested=1;
}
void game_update(Game *g,const Input *in) {
 ++g->frame;if(g->notice_timer) --g->notice_timer;
 switch(g->mode) {
  case M_RUN:run_tick(g,in);break;
  case M_RECEIPT:
   if(in->pressed&K_A) {g->mode=M_HOME;g->cursor=0;}
   break;
  case M_HOME:
   {
    int row=clampi(g->cursor/2+vertical(in),0,2),column=clampi(g->cursor%2+horizontal(in),0,1);
    g->cursor=(uint8_t)(row*2+column);
   }
   if(in->pressed&K_A) {
    switch(g->cursor) {
     case 0:game_begin_run(g);break;
     case 1:g->mode=M_GARAGE;g->cursor=0;g->garage_tab=0;g->selection=g->save.selected_vehicle;refresh_vehicle(g);break;
     case 2:g->mode=M_CHALLENGES;g->cursor=0;break;
     case 3:g->mode=M_CAREER;g->cursor=0;break;
     case 4:g->mode=M_SETTINGS;g->cursor=0;g->menu_return=M_HOME;break;
     case 5:g->mode=M_CONTRACTS;g->cursor=g->save.dispatch_choice;break;
    }
   }
   break;
  case M_GARAGE:garage_tick(g,in);break;
  case M_CONTRACTS:contract_tick(g,in);break;
  case M_DEPARTURES:departure_tick(g,in);break;
  case M_CAREER:career_tick(g,in);break;
  case M_SETTINGS:settings_tick(g,in);break;
  case M_CHALLENGES:
   g->cursor=(uint8_t)((g->cursor+CHALLENGE_COUNT+vertical(in))%CHALLENGE_COUNT);
   if(in->pressed&(K_A|K_B)) {g->mode=M_HOME;g->cursor=2;}
   break;
  case M_CONTROLS:
   if(in->pressed&(K_L|K_R))g->controls_page^=1;
   if(in->pressed&(K_A|K_B)) {
    g->mode=g->paused_from==M_PAUSE?M_PAUSE:g->paused_from==M_SETTINGS?M_SETTINGS:M_HOME;
    g->cursor=g->mode==M_PAUSE?2:g->mode==M_SETTINGS?8:4;
   }
   break;
  case M_PAUSE:
   if(in->pressed&(K_B|K_START)) {g->mode=M_RUN;break;}
   g->cursor=(uint8_t)((g->cursor+4+vertical(in))%4);
   if(in->pressed&K_A) {
    if(g->cursor==0) g->mode=M_RUN;
    else if(g->cursor==1) game_finish_run(g);
    else if(g->cursor==2) {g->mode=M_CONTROLS;g->paused_from=M_PAUSE;g->controls_page=0;}
    else {g->mode=M_SETTINGS;g->cursor=0;g->menu_return=M_PAUSE;}
   }
   break;
  default:g->mode=M_HOME;g->cursor=0;break;
 }
}
