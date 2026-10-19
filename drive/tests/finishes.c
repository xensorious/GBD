/* Independent color/material checks and paid-install/preset/save regressions. */
#include "game.h"
#include "gfx.h"
#include "assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{++checks;if(!(x)){fprintf(stderr,"finishes:%d: %s\n",__LINE__,#x);exit(1);}}while(0)
#define N (48*72)
static uint16_t a[N],b[N],c[N];
static void draw(uint16_t*p,int v,int paint,int pattern,int color){memset(p,0,N*2);Surface s={p,48,72};gfx_vehicle_style(&s,0,0,gfx_vehicle_pixels(v,2),48,72,48,72,paint,pattern,0,color);}
static unsigned hash(const uint16_t*p){unsigned h=2166136261u;for(int i=0;i<N;i++)h=(h^p[i])*16777619u;return h;}
static void press(Game*g,unsigned key){Input in={key,key};game_update(g,&in);}
int main(void){
 for(int v=0;v<VEHICLE_COUNT;v++)for(int pattern=1;pattern<LIVERY_COUNT;pattern++){
  draw(a,v,8,pattern,3);draw(b,v,8,pattern,4);draw(c,v,11,pattern,3);unsigned decals=0,body=0;
  const uint8_t*indices=vehicle_indexed+(v*5+2)*N,*flags=vehicle_palette_flags+v*256;const uint16_t*palette=vehicle_palette+v*256;
  for(int i=0;i<N;i++){
   CHECK((a[i]!=0)==(palette[indices[i]]!=0));CHECK((b[i]!=0)==(a[i]!=0));CHECK((c[i]!=0)==(a[i]!=0));
   if(!(flags[indices[i]]&1u))CHECK(a[i]==palette[indices[i]] && b[i]==a[i] && c[i]==a[i]);
   if(a[i]!=b[i]){CHECK(c[i]==a[i]);decals++;}else body+=a[i]!=c[i];
  }
  CHECK(decals>0 && body>0);
 }
 unsigned paints[PAINT_COUNT],colors[LIVERY_COLOR_COUNT];
 for(int p=0;p<PAINT_COUNT;p++){draw(a,31,p,0,0);paints[p]=hash(a);for(int old=0;old<p;old++)CHECK(paints[p]!=paints[old]);}
 for(int pattern=1;pattern<LIVERY_COUNT;pattern++)for(int color=0;color<LIVERY_COLOR_COUNT;color++){
  draw(a,31,8,pattern,color);colors[color]=hash(a);for(int old=0;old<color;old++)CHECK(colors[color]!=colors[old]);
 }
 Game g,loaded;game_init(&g,8);CHECK((g.save.paints&NEW_PAINT_MASK)==NEW_PAINT_MASK);g.save.owned=UINT32_MAX;g.save.owned_hi=0xffff;g.save.paints=PAINT_MASK;g.save.cash=1000000;
 g.selection=47;g.save.mastery_xp[47]=10000;g.livery_choice=3;g.paint_choice=15;g.livery_color_choice=11;g.wheel_choice=3;g.smoke_choice=4;
 int values[5]={3,3,15,4,11},full[5],cost=0;
 for(int i=0;i<5;i++){full[i]=game_cosmetic_cost(&g,i,values[i]);CHECK(full[i]>0);cost+=full[i];}
 g.save.cash=(unsigned)cost-1;Save before=g.save;CHECK(!game_apply_cosmetics(&g));CHECK(!memcmp(&before,&g.save,sizeof before));
 g.save.cash=(unsigned)cost;CHECK(game_apply_cosmetics(&g) && !g.save.cash);
 CHECK(g.save.paint[47]==15 && (g.save.paint_seen[47]&(1u<<15)) && g.save.livery_color[47]==11 && (g.save.livery_color_seen[47]&(1u<<11)));
 g.save.cash=100000;g.mode=M_GARAGE;g.garage_tab=0;g.cursor=4;press(&g,K_A);unsigned preset=g.save.presets[0];
 CHECK((preset>>28)==11 && (((preset>>9)&7)|((preset>>24)&8))==15 && ((preset&31)|((preset>>21)&32))==47);
 g.livery_color_choice=4;unsigned cash=g.save.cash,price=(unsigned)game_cosmetic_cost(&g,4,4);CHECK(game_apply_cosmetics(&g));
 CHECK(g.save.cash==cash-price && g.save.paint[47]==15 && g.save.livery[47]==3 && g.save.livery_color[47]==4);
 CHECK(game_cosmetic_cost(&g,4,11)==(full[4]+1)/2);
 g.cursor=3;cash=g.save.cash;press(&g,K_A);CHECK(g.save.cash==cash-(unsigned)((full[4]+1)/2) && g.save.livery_color[47]==11);
 uint8_t disk[SAVE_DISK_SIZE];CHECK(game_encode_save(&g,disk,sizeof disk));game_init(&loaded,4);CHECK(game_load_bytes(&loaded,disk,sizeof disk));
 Save s,t;game_make_save(&g,&s);game_make_save(&loaded,&t);CHECK(!memcmp(&s,&t,sizeof s));
 g.mode=M_CONTROLS;g.controls_page=0;press(&g,K_L);CHECK(g.controls_page==2);press(&g,K_R);CHECK(g.controls_page==0);
 for(int skin=SKIN_FUSION;skin<SKIN_STORAGE_LIMIT;skin++){g.save.skin=(uint8_t)skin;g.mode=M_SETTINGS;g.cursor=4;press(&g,K_RIGHT);CHECK(g.save.skin==SKIN_FUSION+(skin-SKIN_FUSION+1)%SKIN_COUNT);}
 printf("finishes: %u checks; 16 body paints, 12 independent decal colors, protected materials, all patterns, atomic pricing, returning discounts, high-bit presets and v6 saves pass\n",checks);
 return 0;
}
