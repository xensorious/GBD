/* Expansion regressions: high ownership bits, cross-tier buys and paid presets. */
#include "game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"premium:%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void press(Game*g,unsigned key){Input in={key,key};game_update(g,&in);}
static void funded(Game*g){game_init(g,777);g->save.cash=1000000;g->save.paints=63;g->save.unlocked_locations=8191;for(int v=0;v<VEHICLE_COUNT;v++){save_add_vehicle(&g->save,v);g->save.mastery_xp[v]=10000;}}
int main(void){
 static const char*const names[17]={"Hellcat","Supra","Convertible","Chevy Tahoe","Dodge Charger","Lamborghini","Koenigsegg","Ferrari","Bugatti","Cadillac","Rolls Royce","Ford F-150","Ducati","Kawasaki","Ninja","Insurgent","Batmobile"};
 Game g,loaded;funded(&g);CHECK(game_owned_count(&g)==48);
 uint64_t visited=0;
 for(int cls=0;cls<CLASS_COUNT;cls++){int last_tier=-1,last_price=-1;
  for(int row=0;row<game_dealer_count(cls);row++){
   int v=game_dealer_vehicle(cls,row),tier=game_vehicle_tier(v);
   CHECK(vehicles[v].class_id==cls && !(visited&(UINT64_C(1)<<v)));visited|=UINT64_C(1)<<v;
   CHECK(tier>=last_tier);if(tier==last_tier)CHECK(vehicles[v].price>=last_price);
   last_tier=tier;last_price=vehicles[v].price;
  }
 }CHECK(visited==UINT64_C(0xffffffffffff));
 CHECK(vehicles[31].class_id==CLASS_B && vehicles[31].top_speed>vehicles[18].top_speed-1);
 CHECK(vehicles[31].price>vehicles[17].price && vehicles[46].armor>vehicles[24].armor);
 for(int v=31;v<48;v++){
  CHECK(!strcmp(vehicles[v].name,names[v-31]));CHECK(game_vehicle_stat(&g,v,STAT_TOP)==vehicles[v].top_speed);CHECK(game_vehicle_tier(v)>=2 && *game_tier_name(v));
  for(int b=0;b<25;b++)if(vehicles[b].class_id==vehicles[v].class_id)CHECK(vehicles[v].price>vehicles[b].price);
  game_init(&g,3);CHECK(!save_owns(&g.save,v));
  for(int i=0;i<game_class_requirement(vehicles[v].class_id);i++)save_add_vehicle(&g.save,i);
  g.mode=M_GARAGE;g.garage_tab=3;g.garage_filter=vehicles[v].class_id;
  for(int row=0;row<game_dealer_count(g.garage_filter);row++)if(game_dealer_vehicle(g.garage_filter,row)==v)g.cursor=(uint8_t)(row+1);
  g.save.cash=vehicles[v].price-1;press(&g,K_A);CHECK(!save_owns(&g.save,v) && g.save.cash==vehicles[v].price-1u);
  g.save.cash++;press(&g,K_A);CHECK(save_owns(&g.save,v) && !g.save.cash && g.save.selected_vehicle==v);
  g.save.cash=1000000;g.save.paints=63;g.save.mastery_xp[v]=10000;g.selection=(uint8_t)v;
  g.paint_choice=2;g.livery_choice=1;g.wheel_choice=3;g.smoke_choice=4;CHECK(game_apply_cosmetics(&g));
  g.garage_tab=1;g.cursor=2;press(&g,K_A);CHECK(g.save.engine[v]==1);
  g.save.selected_location=0;g.garage_tab=0;g.cursor=4;press(&g,K_A);uint32_t preset=g.save.presets[0];
  CHECK(((preset&31u)|((preset>>21)&32u))==(unsigned)v);
  g.paint_choice=g.livery_choice=g.wheel_choice=g.smoke_choice=0;CHECK(game_apply_cosmetics(&g));
  g.cursor=3;int cost=game_preset_cost(&g,0);CHECK(cost>0);unsigned cash=g.save.cash;press(&g,K_A);
  CHECK(g.save.cash==cash-(unsigned)cost && g.save.selected_vehicle==v && g.save.paint[v]==2 && g.save.smoke[v]==4 && g.save.engine[v]==1);
  uint8_t disk[SAVE_DISK_SIZE];CHECK(game_encode_save(&g,disk,sizeof disk));game_init(&loaded,2);CHECK(game_load_bytes(&loaded,disk,sizeof disk));
  Save a,b;game_make_save(&g,&a);game_make_save(&loaded,&b);CHECK(!memcmp(&a,&b,sizeof a));
 }
 funded(&g);for(int v=0;v<48;v++){CHECK(save_owns(&g.save,v));}CHECK(!save_owns(&g.save,48) && !save_owns(&g.save,-1));
 printf("premium: %u checks; all 17 purchases, four dealer tiers, 48 ownership bits, paid cosmetics, high-ID presets, upgrades and v5 reloads pass\n",checks);return 0;
}
