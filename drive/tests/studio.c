/* Rules and actual-renderer coverage for the interface/garage revision. */
#include "game.h"
#include "render.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"studio:%d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void press(Game*g,unsigned key){Input in={key,key};game_update(g,&in);}
static void profile(Game*g){game_init(g,123);g->save.cash=1000000;g->save.owned=0x7fffffffu;g->save.unlocked_locations=(1u<<LOCATION_COUNT)-1;g->save.paints=63;for(int i=0;i<31;i++)g->save.mastery_xp[i]=3200;}
static void transactions(void){
 Game g,loaded;profile(&g);g.mode=M_GARAGE;g.garage_tab=2;g.cursor=5;
 int values[4]={2,3,4,2},full[4];
 for(int kind=0;kind<4;kind++){full[kind]=game_cosmetic_cost(&g,kind,values[kind]);CHECK(full[kind]>0);}
 g.livery_choice=2;g.wheel_choice=3;g.paint_choice=4;g.smoke_choice=2;
 unsigned cost=(unsigned)(full[0]+full[1]+full[2]+full[3]);g.save.cash=cost-1;Save before=g.save;
 CHECK(!game_apply_cosmetics(&g));CHECK(!memcmp(&before,&g.save,sizeof before));
 g.save.cash=cost;CHECK(game_apply_cosmetics(&g));CHECK(!g.save.cash);
 CHECK(g.save.paint[0]==4 && g.save.smoke[0]==2 && g.save.livery[0]==2 && g.save.wheels[0]==3);
 CHECK(game_apply_cosmetics(&g) && !g.save.cash);
 g.save.cash=100000;g.livery_choice=g.wheel_choice=g.paint_choice=g.smoke_choice=0;CHECK(game_apply_cosmetics(&g));
 for(int kind=0;kind<4;kind++)CHECK(game_cosmetic_cost(&g,kind,values[kind])==(full[kind]+1)/2);
 g.livery_choice=2;g.wheel_choice=3;g.paint_choice=4;g.smoke_choice=2;CHECK(game_apply_cosmetics(&g));
 g.garage_tab=0;g.cursor=6;g.save.selected_location=8;press(&g,K_A);CHECK(g.save.preset_mask==1);
 g.garage_tab=2;g.livery_choice=g.wheel_choice=g.paint_choice=g.smoke_choice=0;CHECK(game_apply_cosmetics(&g));
 int refill=game_preset_cost(&g,0);CHECK(refill==(int)((full[0]+1)/2+(full[1]+1)/2+(full[2]+1)/2+(full[3]+1)/2));
 g.garage_tab=0;g.cursor=5;g.save.cash=(unsigned)refill-1;before=g.save;press(&g,K_A);CHECK(!memcmp(&before,&g.save,sizeof before));
 g.save.cash=(unsigned)refill;press(&g,K_A);CHECK(!g.save.cash && g.save.smoke[0]==2 && g.save.selected_location==8);
 uint8_t disk[512];memset(disk,0xa5,sizeof disk);CHECK(game_encode_save(&g,disk,512));
 for(int i=500;i<512;i++)CHECK(disk[i]==0xa5);
 game_init(&loaded,3);CHECK(game_load_bytes(&loaded,disk,512));Save a,b;game_make_save(&g,&a);game_make_save(&loaded,&b);CHECK(!memcmp(&a,&b,sizeof a));
 for(int v=0;v<31;v++){g.save.paint_seen[v]=63;g.save.smoke_seen[v]=31;g.save.smoke[v]=(uint8_t)(v%5);}
 CHECK(game_encode_save(&g,disk,500));CHECK(game_load_bytes(&loaded,disk,500));
 for(int v=0;v<31;v++)CHECK(loaded.save.smoke[v]==v%5 && loaded.save.paint_seen[v]==63 && loaded.save.smoke_seen[v]==31);
 game_init(&g,3);g.mode=M_GARAGE;g.selection=0;press(&g,K_RIGHT);CHECK(g.selection==0);
 g.save.owned|=1u<<10;press(&g,K_RIGHT);CHECK(g.selection==10);press(&g,K_RIGHT);CHECK(g.selection==0);
 unsigned seen=0;
 for(int c=0;c<5;c++){int n=game_dealer_count(c);CHECK(n>=5);g.garage_tab=3;g.garage_filter=(uint8_t)c;g.cursor=0;
  for(int i=0;i<n;i++){press(&g,K_DOWN);int v=game_dealer_vehicle(c,i);CHECK(g.selection==v && game_vehicle_class(v)==c);CHECK(!(seen&(1u<<v)));seen|=1u<<v;}}
 CHECK(seen==0x7fffffffu);
}
static struct{unsigned guard1;uint16_t top[256*192],bottom[256*192];unsigned guard2;} screen;
static void image(const char*dir,const char*name){
 char path[512];snprintf(path,sizeof path,"%s/%s.ppm",dir,name);FILE*f=fopen(path,"wb");CHECK(f!=NULL);fprintf(f,"P6\n256 384\n255\n");
 for(int s=0;s<2;s++)for(int i=0;i<256*192;i++){unsigned c=s?screen.bottom[i]:screen.top[i];fputc((c&31)*255/31,f);fputc(((c>>5)&31)*255/31,f);fputc(((c>>10)&31)*255/31,f);}
 fclose(f);
}
int main(int argc,char**argv){transactions();Game g;profile(&g);screen.guard1=screen.guard2=0xa5a5a5a5u;
 unsigned modes[]={M_HOME,M_GARAGE,M_SETTINGS,M_CHALLENGES,M_CAREER,M_CONTRACTS,M_CONTROLS,M_PAUSE,M_RECEIPT,M_DEPARTURES};
 for(int skin=0;skin<3;skin++){
  g.save.skin=(uint8_t)skin;g.frame=400;g.run.total_cash=1234;g.run.total_points=12345;
  for(unsigned m=0;m<sizeof modes/sizeof modes[0];m++){g.mode=(uint8_t)modes[m];g.cursor=0;g.selection=0;g.garage_tab=0;g.paint_choice=0;g.livery_choice=0;g.wheel_choice=0;g.smoke_choice=0;
   render_game(&g,screen.top,screen.bottom);CHECK(screen.guard1==0xa5a5a5a5u&&screen.guard2==0xa5a5a5a5u);
   if(argc>1){char name[64];snprintf(name,sizeof name,"skin%d-mode%d",skin,g.mode);image(argv[1],name);}}
  g.mode=M_GARAGE;for(int tab=0;tab<4;tab++){g.garage_tab=(uint8_t)tab;g.cursor=tab==3?1:0;render_game(&g,screen.top,screen.bottom);if(argc>1){char name[48];snprintf(name,sizeof name,"skin%d-tab%d",skin,tab);image(argv[1],name);}}
 }
 g.save.skin=2;g.mode=M_GARAGE;g.garage_tab=0;g.cursor=0;
 for(int region=0;region<13;region++){g.save.selected_location=(uint8_t)region;render_game(&g,screen.top,screen.bottom);if(argc>1){char name[48];snprintf(name,sizeof name,"room-%02d",region);image(argv[1],name);}}
 printf("studio: %u checks; repeat-install discounts, atomic payments, paid presets, 500B saves, class lists, owned showroom and all interface/room renders pass\n",checks);
 return 0;
}
