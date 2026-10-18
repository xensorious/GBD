#include "ui.h"

#define VOID C(8,13,18)
#define PANEL C(20,29,37)
#define PANEL_HI C(35,47,56)
#define RIM C(74,91,100)
#define MUTED C(132,152,158)
#define WHITE C(242,244,225)
#define AMBER C(255,190,76)
#define MINT C(112,231,201)
#define RED C(248,105,96)
#define PAPER C(243,235,209)
#define INK C(37,48,51)
#define PAPER_RULE C(196,193,169)

static const char *const paint_names[6]={"Factory","Coral","Lagoon","Sunburst","Orchid","Graphite"};
static const uint16_t paint_colors[6]={C(231,230,215),C(240,105,92),C(65,189,185),C(248,186,54),C(164,121,217),C(91,105,118)};
static const int16_t needle_xy[31][2]={
 {-724,724},{-828,602},{-912,465},{-974,316},{-1011,160},{-1024,0},
 {-1011,-160},{-974,-316},{-912,-465},{-828,-602},{-724,-724},{-602,-828},
 {-465,-912},{-316,-974},{-160,-1011},{0,-1024},{160,-1011},{316,-974},
 {465,-912},{602,-828},{724,-724},{828,-602},{912,-465},{974,-316},
 {1011,-160},{1024,0},{1011,160},{974,316},{912,465},{828,602},{724,724}
};
static int clamp(int n,int lo,int hi){return n<lo?lo:n>hi?hi:n;}
static void append(char *b,const char *s,int cap){int n=0;while(b[n]&&n<cap-1)n++;while(*s&&n<cap-1)b[n++]=*s++;b[n]=0;}
static void number(char *b,uint32_t value,int cap){char rev[12];int n=0;do{rev[n++]=(char)('0'+value%10);value/=10;}while(value&&n<11);while(n){char c[2]={rev[--n],0};append(b,c,cap);}}
static void money(char *b,uint32_t value,int cap){append(b,"$",cap);number(b,value,cap);}
static void time_text(char *b,uint32_t ticks,int cap){uint32_t sec=ticks/DRIVE_HZ;number(b,sec/60,cap);append(b,":",cap);if(sec%60<10)append(b,"0",cap);number(b,sec%60,cap);}
static void right(Surface *s,int x,int y,const char *text,uint16_t col){gfx_text(s,x-gfx_text_width(text),y,text,col);}
static void center(Surface *s,int x,int y,int width,const char *text,uint16_t col){gfx_text(s,x+(width-gfx_text_width(text))/2,y,text,col);}
static void limited(Surface*s,int x,int y,int width,const char*text,uint16_t col){char b[100]="";int n=0;while(text[n]&&n<95){b[n]=text[n];n++;b[n]=0;if(gfx_text_width(b)>width){b[--n]=0;while(n&&gfx_text_width(b)+gfx_text_width("...")>width)b[--n]=0;append(b,"...",sizeof b);break;}}gfx_text(s,x,y,b,col);}
static void disc(Surface*s,int x,int y,int radius,uint16_t color){int r2=radius*radius;for(int dy=-radius;dy<=radius;dy++){int dx=radius;while(dx&&dx*dx+dy*dy>r2)dx--;gfx_rect(s,x-dx,y+dy,dx*2+1,1,color);}}
static void base(Surface*s){gfx_rect(s,0,0,256,192,VOID);for(int y=0;y<192;y+=3)gfx_rect(s,0,y,256,1,(y%6)?C(13,20,26):C(16,23,29));gfx_rect(s,0,0,256,2,C(57,76,84));gfx_rect(s,0,190,256,2,C(4,8,12));}
static void header(Surface*s,const char*title,const Game*g){gfx_text(s,12,9,title,WHITE);char b[24]="";money(b,g->save.cash,sizeof b);right(s,244,9,b,AMBER);gfx_rect(s,12,26,232,1,PANEL_HI);gfx_rect(s,12,26,32,1,AMBER);}
static void footer(Surface*s,const char*l,const char*r){gfx_rect(s,9,177,238,1,PANEL_HI);gfx_text_small(s,11,181,l,MUTED);if(r)right(s,245,180,r,MUTED);}
static void row(Surface*s,int y,int h,const char*label,const char*value,int selected,int locked){gfx_card(s,11,y+2,234,h,C(5,11,16),C(5,11,16));gfx_card(s,11,y,234,h,selected?C(39,58,62):PANEL,selected?AMBER:PANEL_HI);if(selected){gfx_rect(s,12,y+5,3,h-10,AMBER);gfx_rect(s,19,y+4,218,1,C(81,101,101));}limited(s,22,y+(h-10)/2,146,label,locked?MUTED:WHITE);if(value)right(s,235,y+(h-10)/2,value,selected?AMBER:locked?MUTED:MINT);}
static void bar(Surface*s,int x,int y,int w,int v,int max,uint16_t col){if(max<1)max=1;v=clamp(v,0,max);gfx_rect(s,x,y,w,5,C(8,16,22));int fill=(w-2)*v/max;if(fill){gfx_rect(s,x+1,y+1,fill,3,col);gfx_rect(s,x+1,y+1,fill,1,WHITE);}}
static void small_car(Surface*s,int x,int y,int paint,int bike){uint16_t col=paint_colors[clamp(paint,0,5)];if(bike){gfx_rect(s,x+5,y,4,22,C(3,8,12));gfx_card(s,x+3,y+5,8,12,col,RIM);gfx_rect(s,x+1,y+4,12,2,RIM);gfx_rect(s,x+6,y+8,2,7,VOID);}else{gfx_rect(s,x,y+4,3,5,VOID);gfx_rect(s,x+13,y+4,3,5,VOID);gfx_rect(s,x,y+16,3,5,VOID);gfx_rect(s,x+13,y+16,3,5,VOID);gfx_card(s,x+2,y,12,25,col,RIM);gfx_rect(s,x+4,y+6,8,5,C(34,61,73));gfx_rect(s,x+4,y+15,8,4,C(26,43,53));gfx_rect(s,x+4,y+1,2,2,WHITE);gfx_rect(s,x+10,y+1,2,2,WHITE);gfx_rect(s,x+4,y+23,2,1,RED);gfx_rect(s,x+10,y+23,2,1,RED);}}

/* Seven segment numerals stay crisp at the DS's native resolution. */
static void digit(Surface*s,int x,int y,int n,int scale,uint16_t col){static const unsigned char masks[10]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};unsigned bits=masks[clamp(n,0,9)];int a=scale,b=scale*4;uint16_t off=C(29,43,44);for(int i=0;i<7;i++){uint16_t c=(bits&(1u<<i))?col:off;switch(i){case 0:gfx_rect(s,x+a,y,b,a,c);break;case 1:gfx_rect(s,x+a+b,y+a,a,b,c);break;case 2:gfx_rect(s,x+a+b,y+2*a+b,a,b,c);break;case 3:gfx_rect(s,x+a,y+2*(a+b),b,a,c);break;case 4:gfx_rect(s,x,y+2*a+b,a,b,c);break;case 5:gfx_rect(s,x,y+a,a,b,c);break;default:gfx_rect(s,x+a,y+a+b,b,a,c);break;}}}
static void dial_base(Surface*s,int x,int y,int speed){disc(s,x,y+2,49,C(3,7,10));disc(s,x,y,48,RIM);disc(s,x,y,46,C(16,28,35));disc(s,x,y,44,C(56,72,79));disc(s,x,y,42,C(10,19,25));for(int dy=-38;dy<-12;dy++){int w=35-(dy+38)/2;gfx_rect(s,x-w,y+dy,w*2,1,C(16,28+(dy+38)/8,35+(dy+38)/8));}for(int i=0;i<=30;i++){int major=i%5==0;int outer=40,inner=major?34:37;int x1=x+needle_xy[i][0]*inner/1024,y1=y+needle_xy[i][1]*inner/1024;int x2=x+needle_xy[i][0]*outer/1024,y2=y+needle_xy[i][1]*outer/1024;gfx_line(s,x1,y1,x2,y2,i>24&&speed?RED:major?WHITE:RIM);}if(speed){gfx_text_small(s,x-34,y+17,"0",MUTED);gfx_text_small(s,x-32,y-26,"100",MUTED);gfx_text_small(s,x+15,y-26,"200",MUTED);gfx_text_small(s,x+21,y+17,"300",MUTED);}else{gfx_text_small(s,x-36,y+17,"-40",MUTED);gfx_text_small(s,x-3,y-33,"0",MUTED);gfx_text_small(s,x+20,y+17,"+40",MUTED);}}
static void needle(Surface*s,int x,int y,int index,uint16_t color){index=clamp(index,0,30);int dx=needle_xy[index][0],dy=needle_xy[index][1];int ex=x+dx*32/1024,ey=y+dy*32/1024;gfx_line(s,x-dx*6/1024+1,y-dy*6/1024+1,ex+1,ey+1,VOID);gfx_line(s,x-dx*6/1024,y-dy*6/1024,ex,ey,color);gfx_line(s,x,y+1,ex,ey+1,color);disc(s,x,y,5,RIM);disc(s,x,y,3,VOID);gfx_rect(s,x-1,y-1,2,2,WHITE);}
/* The cluster casing, glass and fixed legends never change during a run.
 * Build them once in main RAM; refreshing the dashboard then costs one linear
 * word copy plus the genuinely moving instruments. */
static uint16_t dashboard_art[256*192] __attribute__((aligned(4)));
static unsigned char dashboard_ready;
static void dashboard_background(void){
 Surface cache={dashboard_art,256,192};Surface*s=&cache;
 base(s);gfx_text_small(s,11,5,"C O A S T L I N E",MUTED);
 dial_base(s,65,67,1);dial_base(s,191,67,0);
 gfx_text_small(s,52,105,"km/h",MUTED);
 center(s,150,105,82,"m/s2",MUTED);
 gfx_card(s,116,57,24,25,PANEL,RIM);
 gfx_rect(s,11,119,234,1,RIM);gfx_rect(s,11,120,234,1,VOID);
 gfx_text_small(s,12,129,"ODOMETER",MUTED);
 for(int i=0;i<6;i++){
  int x=78+i*12;gfx_card(s,x,126,11,17,C(2,9,12),PANEL_HI);
  gfx_rect(s,x+1,127,9,1,C(23,37,42));gfx_rect(s,x+1,141,9,1,C(1,5,8));
 }
 gfx_text_small(s,153,131,"m",MUTED);
 gfx_text_small(s,12,151,"POINTS",MUTED);
 gfx_text_small(s,164,151,"CASH",MUTED);
 gfx_text_small(s,12,166,"ARMOR",MUTED);
 gfx_card(s,155,163,89,14,PANEL,PANEL_HI);
 gfx_text_small(s,162,166,"COMBO",MUTED);
 gfx_text_small(s,12,182,"Start: Pause",MUTED);
 dashboard_ready=1;
}
static void dashboard(Surface*s,const Game*g){
 if(!dashboard_ready)dashboard_background();
 gfx_copy(s->pixels,dashboard_art,256*192);
 limited(s,141,4,105,locations[g->biome<LOCATION_COUNT?g->biome:0].name,MINT);
 int speed=g->speed_q8<0?-g->speed_q8:g->speed_q8;speed=(speed+Q/2)/Q;
 needle(s,65,67,speed/10,AMBER);
 /* accel_q8 is measured km/h per second, including braking and impacts. */
 int accel=g->accel_q8*100/(Q*36);accel=clamp(accel,-999,999);
 needle(s,191,67,15+accel*15/400,accel<0?RED:MINT);
 int v=clamp(speed,0,999);digit(s,45,82,v/100,2,AMBER);digit(s,59,82,v/10%10,2,AMBER);digit(s,73,82,v%10,2,AMBER);
 char a[16]="";append(a,accel<0?"-":"+",sizeof a);int av=accel<0?-accel:accel;number(a,av/10,sizeof a);append(a,".",sizeof a);number(a,av%10,sizeof a);center(s,150,90,82,a,accel<0?RED:MINT);
 /* This legend can intersect the needle at extreme acceleration, so retain
  * its original foreground order rather than baking it behind the needle. */
 center(s,150,78,82,"ACCEL",MUTED);
 center(s,116,64,24,g->speed_q8<0?"R":"D",g->speed_q8<0?RED:WHITE);
 if(g->turbo)center(s,108,89,40,"BOOST",AMBER);else if(g->slow)center(s,106,89,44,"CRUISE",MINT);else if(g->drifting)center(s,106,89,44,"DRIFT",AMBER);else if(g->wheelie)center(s,105,89,46,"WHEELIE",AMBER);
 uint32_t metres=g->run.distance_m;int div=100000;
 for(int i=0;i<6;i++){int x=78+i*12;char d[2]={(char)('0'+(metres/div)%10),0};center(s,x,129,11,d,WHITE);div/=10;}
 char score[24]="";number(score,g->run.total_points,sizeof score);right(s,140,149,score,WHITE);
 char cash[24]="";money(cash,g->run.total_cash,sizeof cash);right(s,244,149,cash,AMBER);
 bar(s,52,169,81,g->health,100,g->health<30?RED:MINT);
 char combo[16]="x";number(combo,g->combo_q8/Q,sizeof combo);append(combo,".",sizeof combo);number(combo,(g->combo_q8%Q)*10/Q,sizeof combo);right(s,237,165,combo,AMBER);
 char coins[24]="COINS ";number(coins,g->run.count[B_COIN],sizeof coins);right(s,244,181,coins,AMBER);
 if(g->crash_timer){gfx_card(s,85,30,86,16,C(68,22,24),RED);center(s,85,33,86,"IMPACT",WHITE);}
}

static void home(Surface*s,const Game*g){header(s,"C O A S T L I N E",g);gfx_text_small(s,13,32,"ENDLESS DRIVE",MINT);char best[32]="BEST ";number(best,g->save.best_score,sizeof best);right(s,244,31,best,MUTED);static const char*const labels[5]={"Drive","Garage","Departures","Challenges","Controls"};for(int i=0;i<5;i++){const char*value=i==0?"START":i==1?"TUNE":i==2?"ROUTES":i==3?"UNLOCK":"BUTTONS";row(s,49+i*24,21,labels[i],value,g->cursor==i,0);}footer(s,"Up / Down: Choose","A: Open");}
static void garage(Surface*s,const Game*g){header(s,"GARAGE",g);int id=clamp(g->selection,0,VEHICLE_COUNT-1),owned=(g->save.owned&(1u<<id))!=0;const VehicleDef*v=&vehicles[id];char index[16]="";number(index,id+1,sizeof index);append(index," / ",sizeof index);number(index,VEHICLE_COUNT,sizeof index);row(s,33,27,v->name,index,g->cursor==0,0);small_car(s,181,34,g->cursor==5?g->paint_choice:g->save.paint[id],v->bike);char equip[32]="";if(owned)append(equip,g->save.selected_vehicle==id?"EQUIPPED":"SELECT",sizeof equip);else if(game_can_vehicle(g,id))money(equip,v->price,sizeof equip);else append(equip,"LOCKED",sizeof equip);row(s,64,20,owned?"Use this vehicle":"Purchase vehicle",equip,g->cursor==1,!owned&&!game_can_vehicle(g,id));
 const uint8_t*levels[3]={g->save.engine,g->save.grip,g->save.armor};static const char*const names[3]={"Engine","Grip","Armor"};for(int i=0;i<3;i++){char label[30]="";append(label,names[i],sizeof label);append(label,"  ",sizeof label);number(label,levels[i][id],sizeof label);append(label," / 3",sizeof label);char cost[24]="";if(levels[i][id]>=3)append(cost,"MAX",sizeof cost);else{money(cost,game_upgrade_cost(g,i),sizeof cost);}row(s,88+i*22,20,label,cost,g->cursor==i+2,!owned);}
 int paint=clamp(g->paint_choice,0,5);row(s,154,20,"Paint",paint_names[paint],g->cursor==5,!(g->save.paints&(1u<<paint)));gfx_rect(s,126,160,9,8,paint_colors[paint]);footer(s,"Left/Right: Browse","A: Apply   B: Back");}
static void departures(Surface*s,const Game*g){header(s,"DEPARTURES",g);for(int i=0;i<LOCATION_COUNT;i++){char cost[24]="";int owned=(g->save.unlocked_locations&(1u<<i))!=0,available=game_can_location(g,i);if(owned)append(cost,g->save.selected_location==i?"SELECTED":"OPEN",sizeof cost);else if(available)money(cost,locations[i].price,sizeof cost);else append(cost,"LOCKED",sizeof cost);row(s,33+i*24,21,locations[i].name,cost,g->cursor==i,!owned&&!available);}int id=clamp(g->cursor,0,LOCATION_COUNT-1);const char*desc=(!game_can_location(g,id)&&!(g->save.unlocked_locations&(1u<<id)))?locations[id].requirement:locations[id].description;limited(s,14,158,228,desc,MUTED);footer(s,"Up / Down: Choose","A: Select   B: Back");}
static void challenges(Surface*s,const Game*g){
 header(s,"ROAD CHALLENGES",g);
 static const char*const units[5]={" near","m"," drift","s"," wheelie"};
 static const char*const rewards[5]={"Skywater Causeway","Pine Ridge departure","Sundown / Redrock / Graphite","Sunburst paint","Orchid paint"};
 for(int i=0;i<5;i++){
  int goal=1,progress=game_challenge_progress(g,i,&goal),done=progress>=goal;
  if(i==3){progress/=DRIVE_HZ;goal/=DRIVE_HZ;}
  int y=33+i*28;
  gfx_card(s,11,y,234,25,g->cursor==i?C(35,48,51):PANEL,g->cursor==i?AMBER:done?C(56,104,93):PANEL_HI);
  limited(s,18,y+2,127,game_challenge_name(i),done?MINT:WHITE);
  char count[32]="";number(count,progress,sizeof count);append(count,"/",sizeof count);number(count,goal,sizeof count);append(count,units[i],sizeof count);
  right(s,236,y+2,count,done?MINT:AMBER);
  limited(s,18,y+14,178,rewards[i],MUTED);bar(s,202,y+17,34,progress,goal,done?MINT:AMBER);
 }
 footer(s,"Unlocks need one run","B: Back");
}
static void controls(Surface*s,const Game*g){header(s,"DRIVING CONTROLS",g);static const char*const keys[8]={"R","L","Left / Right","Up + R","Down + R","B","A","Start"};static const char*const acts[8]={"Accelerate","Brake / reverse","Steer","Turbo (hold)","Slow (hold)","Handbrake / drift","Horn / bike wheelie","Pause"};for(int i=0;i<8;i++){int y=34+i*17;gfx_card(s,13,y,74,15,PANEL,PANEL_HI);center(s,13,y+2,74,keys[i],AMBER);gfx_text(s,97,y+2,acts[i],WHITE);}footer(s,"Button controls only","B: Back");}
static void pause_menu(Surface*s,const Game*g){header(s,"PAUSED",g);char details[60]="";number(details,g->run.distance_m,sizeof details);append(details," m  /  ",sizeof details);time_text(details,g->run_ticks,sizeof details);center(s,11,41,234,details,MUTED);static const char*const labels[3]={"Resume driving","End this run","Controls"};for(int i=0;i<3;i++)row(s,64+i*31,26,labels[i],i==0?"GO":i==1?"BANK":"VIEW",g->cursor==i,0);footer(s,"Up / Down: Choose","A: Open   B: Resume");}
static void receipt(Surface*s,const Game*g){
 gfx_rect(s,0,0,256,192,C(17,26,31));for(int y=0;y<192;y+=4)gfx_rect(s,0,y,256,1,C(20,30,35));gfx_rect(s,13,8,236,173,C(5,12,16));gfx_rect(s,8,4,236,173,PAPER);for(int y=5;y<177;y+=3)gfx_rect(s,10,y,232,1,C(240,232,207));for(int x=8;x<244;x+=8){gfx_rect(s,x,177,7,1,PAPER);gfx_rect(s,x+1,178,5,1,PAPER);gfx_rect(s,x+2,179,3,1,PAPER);gfx_rect(s,x+3,180,1,1,PAPER);}center(s,8,9,236,"C O A S T L I N E",INK);center(s,8,21,236,"RUN RECEIPT",C(110,118,108));char info[70]="";number(info,g->run.distance_m,sizeof info);append(info," m  |  ",sizeof info);time_text(info,g->run_ticks,sizeof info);append(info,"  |  ",sizeof info);number(info,g->run.max_speed,sizeof info);append(info," km/h",sizeof info);center(s,8,34,236,info,INK);gfx_rect(s,17,48,218,1,PAPER_RULE);gfx_text_small(s,18,50,"BONUS",INK);right(s,163,49,"QTY",INK);right(s,234,49,"POINTS",INK);
 static const char*const labels[BONUS_COUNT]={"Distance","Survival","Near miss","Top speed","Drift","Wheelie","Coins"};for(int i=0;i<BONUS_COUNT;i++){int y=64+i*11;gfx_text(s,18,y,labels[i],INK);char qty[20]="";number(qty,g->run.count[i],sizeof qty);append(qty,i==B_DISTANCE?"m":i==B_SURVIVAL||i==B_SPEED||i==B_DRIFT||i==B_WHEELIE?"s":"x",sizeof qty);right(s,163,y,qty,C(91,108,100));char pts[24]="";number(pts,g->run.points[i],sizeof pts);right(s,234,y,pts,INK);}gfx_rect(s,17,142,218,1,INK);gfx_text(s,18,147,"TOTAL POINTS",INK);char total[30]="";number(total,g->run.total_points,sizeof total);right(s,234,147,total,INK);gfx_text(s,18,162,"CASH EARNED",INK);char cash[30]="";money(cash,g->run.total_cash,sizeof cash);right(s,234,162,cash,INK);center(s,0,181,256,"A: Continue",WHITE);
}
void ui_draw_bottom(Surface*s,const Game*g){if(g->mode==M_RUN){dashboard(s,g);return;}if(g->mode==M_RECEIPT){receipt(s,g);return;}base(s);switch(g->mode){case M_HOME:home(s,g);break;case M_GARAGE:garage(s,g);break;case M_DEPARTURES:departures(s,g);break;case M_CHALLENGES:challenges(s,g);break;case M_CONTROLS:controls(s,g);break;case M_PAUSE:pause_menu(s,g);break;default:home(s,g);break;}if(g->notice_timer&&g->notice[0]){gfx_card(s,10,159,236,17,C(47,63,61),AMBER);limited(s,17,163,220,g->notice,WHITE);}}
