#include "ui.h"
#include "assets.h"

/* Small, original frosted-glass instruments and menus for the DS's native
 * resolution. Nothing in this file handles input or modifies game state. */
typedef struct {
 uint16_t sky,shore,card_top,card_bottom,edge,shadow,ink,muted,accent,coral;
 uint16_t selected_top,selected_bottom,selected_ink,selected_detail;
 uint16_t cockpit_top,cockpit_bottom,light;
} Theme;
/* Fusion shares Midnight's palette and component artwork exactly. */
static const Theme themes[3]={
 {C(236,239,242),C(220,227,232),C(255,255,255),C(245,247,249),C(208,218,225),C(193,205,214),C(22,33,46),C(91,110,125),C(0,107,174),C(228,93,55),C(20,44,64),C(20,44,64),C(255,255,255),C(102,226,215),C(22,33,46),C(12,20,29),C(102,226,215)},
 {C(8,14,25),C(14,25,42),C(16,30,48),C(16,30,48),C(48,83,111),C(0,6,14),C(231,247,255),C(127,163,185),C(180,244,75),C(255,132,85),C(26,57,68),C(26,57,68),C(224,255,187),C(180,244,75),C(12,25,42),C(4,12,22),C(180,244,75)},
 {C(17,35,61),C(41,62,82),C(49,78,99),C(27,54,77),C(102,148,165),C(6,20,36),C(236,250,247),C(155,198,208),C(105,233,220),C(250,146,155),C(50,128,147),C(34,89,121),C(248,255,249),C(255,218,163),C(28,44,76),C(9,20,40),C(145,206,255)}
};
static const Theme *t=&themes[0];
static int interface;
#define DARK C(6,16,25)
#define LIGHT C(241,252,244)
#define SOFT C(155,188,195)
#define WARN C(255,120,112)
#define GOLD C(255,207,123)
static const char *const paint_names[6]={"Factory","Coral","Lagoon","Sunburst","Orchid","Graphite"};
static const int16_t needle_xy[31][2]={
 {-724,724},{-828,602},{-912,465},{-974,316},{-1011,160},{-1024,0},
 {-1011,-160},{-974,-316},{-912,-465},{-828,-602},{-724,-724},{-602,-828},
 {-465,-912},{-316,-974},{-160,-1011},{0,-1024},{160,-1011},{316,-974},
 {465,-912},{602,-828},{724,-724},{828,-602},{912,-465},{974,-316},
 {1011,-160},{1024,0},{1011,160},{974,316},{912,465},{828,602},{724,724}
};
static int clamp(int n,int lo,int hi){return n<lo?lo:n>hi?hi:n;}
static uint16_t mix(uint16_t a,uint16_t b,int q){
 q=clamp(q,0,256);int p=256-q;
 return (uint16_t)(0x8000|((((a&31)*p+(b&31)*q)>>8)&31)|(((((a>>5&31)*p+(b>>5&31)*q)>>8)&31)<<5)|(((((a>>10&31)*p+(b>>10&31)*q)>>8)&31)<<10));
}
static void append(char*b,const char*s,int cap){int n=0;while(b[n]&&n<cap-1)n++;while(*s&&n<cap-1)b[n++]=*s++;b[n]=0;}
static void number(char*b,uint32_t value,int cap){char r[12];int n=0;do{r[n++]=(char)('0'+value%10);value/=10;}while(value&&n<11);while(n){char c[2]={r[--n],0};append(b,c,cap);}}
static void money(char*b,uint32_t value,int cap){append(b,"$",cap);number(b,value,cap);}
static void time_text(char*b,uint32_t ticks,int cap){uint32_t sec=ticks/DRIVE_HZ;number(b,sec/60,cap);append(b,":",cap);if(sec%60<10)append(b,"0",cap);number(b,sec%60,cap);}
static void right(Surface*s,int x,int y,const char*text,uint16_t color){gfx_text(s,x-gfx_text_width(text),y,text,color);}
static void center(Surface*s,int x,int y,int w,const char*text,uint16_t color){gfx_text(s,x+(w-gfx_text_width(text))/2,y,text,color);}
static int small_width(const char*text){int width=0;while(*text){unsigned c=(unsigned char)*text++;width+=font_small_advance[(c>=32&&c<=127?c:'?')-32];}return width;}
static void right_small(Surface*s,int x,int y,const char*text,uint16_t color){gfx_text_small(s,x-small_width(text),y,text,color);}
static void center_small(Surface*s,int x,int y,int w,const char*text,uint16_t color){
 gfx_text_small(s,x+(w-small_width(text))/2,y,text,color);
}
static void limited_small(Surface*s,int x,int y,int width,const char*text,uint16_t color){char b[112]="";int n=0;if(width<4)return;while(text[n]&&n<107){b[n]=text[n];n++;b[n]=0;if(small_width(b)>width){b[--n]=0;while(n&&small_width(b)+small_width("...")>width)b[--n]=0;append(b,"...",sizeof b);break;}}gfx_text_small(s,x,y,b,color);}
static void limited(Surface*s,int x,int y,int width,const char*text,uint16_t color){
 char b[100]="";int n=0;if(width<5)return;
 while(text[n]&&n<95){b[n]=text[n];n++;b[n]=0;if(gfx_text_width(b)>width){b[--n]=0;while(n&&gfx_text_width(b)+gfx_text_width("...")>width)b[--n]=0;append(b,"...",sizeof b);break;}}
 gfx_text(s,x,y,b,color);
}
static void disc(Surface*s,int x,int y,int r,uint16_t color){int r2=r*r;for(int dy=-r;dy<=r;dy++){int dx=r;while(dx&&dx*dx+dy*dy>r2)dx--;gfx_rect(s,x-dx,y+dy,dx*2+1,1,color);}}
static void gradient(Surface*s,int x,int y,int w,int h,uint16_t top,uint16_t bottom){for(int j=0;j<h;j++)gfx_rect(s,x,y+j,w,1,mix(top,bottom,h>1?j*256/(h-1):0));}
static void glass(Surface*s,int x,int y,int w,int h,int selected){
 if(interface!=SKIN_MIDNIGHT && interface!=SKIN_FUSION){
  uint16_t fill=selected?t->selected_top:t->card_top;
  gfx_rect(s,x,y,w,h,fill);
  if(interface==0){gfx_rect(s,x,y+h-1,w,1,t->edge);if(selected)gfx_rect(s,x,y,3,h,t->selected_detail);}
  else{gfx_rect(s,x,y,w,1,selected?t->accent:t->edge);gfx_rect(s,x,y+h-1,w,1,selected?t->accent:t->edge);gfx_rect(s,x,y,1,h,t->edge);gfx_rect(s,x+w-1,y,1,h,t->edge);if(selected)gfx_rect(s,x+3,y+3,2,h-6,t->accent);}
  return;
 }
 uint16_t a=selected?t->selected_top:t->card_top,b=selected?t->selected_bottom:t->card_bottom;
 gfx_card(s,x+1,y+2,w,h,t->shadow,t->shadow);
 for(int j=0;j<h;j++){
  int e=j<h-1-j?j:h-1-j,cut=e==0?5:e==1?3:e==2?2:e<5?1:0;
  uint16_t col=mix(a,b,j*256/(h>1?h-1:1));
  gfx_rect(s,x+cut,y+j,w-2*cut,1,col);
  if(!j||j==h-1)gfx_rect(s,x+cut,y+j,w-2*cut,1,selected?t->selected_detail:t->edge);
  else {gfx_rect(s,x+cut,y+j,1,1,mix(col,t->edge,100));gfx_rect(s,x+w-cut-1,y+j,1,1,mix(col,t->shadow,90));}
 }
 gfx_rect(s,x+7,y+2,w-14,1,mix(a,t->edge,110));
}
static void palm(Surface*s,int x,int y,int scale,uint16_t color){
 for(int i=0;i<47;i++){int dx=i*i/230;gfx_rect(s,x+dx,y+i*scale/2,scale,scale,color);}
 for(int side=-1;side<=1;side+=2)for(int branch=0;branch<4;branch++)for(int i=0;i<18;i++){
  int xx=x+side*i*scale/2,yy=y-branch*2-i/3+(i*i/55);gfx_rect(s,xx,yy,scale,scale,color);
 }
 gfx_line(s,x,y,x+scale*5,y-scale*8,color);gfx_line(s,x,y,x-scale*3,y-scale*9,color);
}
static uint16_t menu_art[256*192] __attribute__((aligned(4)));
static int menu_skin=-1;
static void backdrop(Surface*s,int skin){
 if(menu_skin!=skin){Surface bg={menu_art,256,192};
  if(skin==SKIN_MIDNIGHT || skin==SKIN_FUSION){gradient(&bg,0,0,256,192,t->sky,t->shore);
   disc(&bg,236,12,42,mix(t->sky,t->edge,34));disc(&bg,236,12,32,mix(t->sky,t->edge,62));disc(&bg,236,12,24,mix(t->sky,t->edge,100));
   for(int j=0;j<5;j++)gfx_line(&bg,-20,151+j*8,278,118+j*9,mix(t->shore,t->edge,55+j*12));
   palm(&bg,-6,91,3,mix(t->sky,t->accent,36));palm(&bg,249,123,2,mix(t->shore,t->accent,32));
  }else if(skin==0){
   gfx_rect(&bg,0,0,256,192,t->sky);gfx_rect(&bg,0,0,256,31,t->card_top);
   gfx_rect(&bg,0,179,256,13,t->card_top);gfx_rect(&bg,0,31,256,1,t->edge);
   for(int y=48;y<176;y+=32)gfx_rect(&bg,8,y,240,1,mix(t->sky,t->edge,32));
  }else{
   gfx_rect(&bg,0,0,256,192,t->sky);gfx_rect(&bg,0,0,7,192,t->accent);
   gfx_rect(&bg,9,0,247,3,t->edge);
   for(int x=22;x<256;x+=16)for(int y=34;y<180;y+=16)gfx_rect(&bg,x,y,1,1,t->edge);
   gfx_rect(&bg,9,177,247,1,t->accent);
  }
  gfx_rect(&bg,0,0,256,1,t->edge);menu_skin=skin;
 }
 gfx_copy(s->pixels,menu_art,256*192);
}
static void large_text(Surface*s,int x,int y,const char*text,uint16_t color){
 while(*text){unsigned c=(unsigned char)*text++;if(c<32||c>127)c='?';const uint8_t *a=font_alpha+(c-32)*12*14;
  for(int j=0;j<14;j++)for(int i=0;i<12;i++)if(a[j*12+i]){int xx=x+i*2,yy=y+j*2;if(xx>=0&&xx<s->w&&yy>=0&&yy<s->h)gfx_rect(s,xx,yy,2,2,mix(s->pixels[yy*s->w+xx],color,a[j*12+i]));}
  x+=font_advance[c-32]*2;
 }
}
static void header(Surface*s,const char*title,const Game*g){gfx_text(s,13,9,title,t->ink);char b[24]="";money(b,g->save.cash,sizeof b);right(s,243,9,b,t->accent);gfx_rect(s,13,27,230,1,mix(t->accent,t->edge,160));gfx_rect(s,13,27,26,2,t->coral);}
static void footer(Surface*s,const char*l,const char*r){gfx_rect(s,11,178,234,1,mix(t->accent,t->edge,140));gfx_text_small(s,12,182,l,t->ink);if(r)right(s,244,181,r,t->ink);}
static void row(Surface*s,int y,int h,const char*label,const char*value,int selected,int locked){
 glass(s,11,y,234,h,selected);int vx=value?gfx_text_width(value):0,w=211-vx-10;
 limited(s,22,y+(h-10)/2,w,label,selected?t->selected_ink:locked?t->muted:t->ink);
 if(value)right(s,234,y+(h-10)/2,value,selected?t->selected_detail:locked?t->muted:t->accent);
 if(selected)gfx_rect(s,13,y+5,2,h-10,t->selected_detail);
}
static void meter(Surface*s,int x,int y,int w,int v,int max,uint16_t color,uint16_t bg){if(max<1)max=1;v=clamp(v,0,max);gfx_rect(s,x,y,w,5,bg);int fill=(w-2)*v/max;if(fill){gfx_rect(s,x+1,y+1,fill,3,color);gfx_rect(s,x+1,y+1,fill,1,mix(color,LIGHT,120));}}
static void home_icon(Surface*s,int x,int y,int i,int selected){
 uint16_t c=selected?t->selected_ink:t->accent,bg=selected?t->selected_bottom:t->card_bottom;
 switch(i){
 case 0:disc(s,x,y,5,c);disc(s,x,y,3,bg);gfx_line(s,x-4,y,x+4,y,c);gfx_line(s,x,y,x,y+4,c);break;
 case 1:gfx_line(s,x-5,y-1,x,y-5,c);gfx_line(s,x,y-5,x+5,y-1,c);gfx_rect(s,x-4,y-1,9,6,c);gfx_rect(s,x-2,y+1,5,4,bg);break;
 case 2:gfx_rect(s,x-4,y-5,1,11,c);gfx_rect(s,x-3,y-5,8,5,c);gfx_rect(s,x-4,y+5,7,1,c);break;
 case 3:gfx_line(s,x-5,y,x+5,y,c);gfx_line(s,x,y-5,x,y+5,c);gfx_line(s,x-3,y-3,x+3,y+3,c);gfx_line(s,x-3,y+3,x+3,y-3,c);break;
 case 4:gfx_line(s,x,y-5,x+5,y,c);gfx_line(s,x+5,y,x,y+5,c);gfx_line(s,x,y+5,x-5,y,c);gfx_line(s,x-5,y,x,y-5,c);break;
 case 6:gfx_rect(s,x-4,y-5,9,11,c);gfx_rect(s,x-3,y-4,7,9,bg);gfx_rect(s,x-2,y-2,5,1,c);gfx_rect(s,x-2,y,5,1,c);gfx_rect(s,x-2,y+2,3,1,c);break;
 default:disc(s,x,y,4,c);disc(s,x,y,2,bg);for(int a=-1;a<=1;a+=2){gfx_rect(s,x-5,y+a*4,11,1,c);gfx_rect(s,x+a*4,y-5,1,11,c);}break;
 }
}
static void home(Surface*s,const Game*g){
 static const char*const labels[6]={"Drive","Garage","Challenges","Career","Settings","Dispatch"};
 static const char*const hints[6]={"START RUN","YOUR COLLECTION","ROAD REWARDS","DRIVER PERKS","PREFERENCES","BONUS CONTRACT"};
 if(interface==SKIN_APEX || interface==SKIN_FUSION){
  gfx_text(s,12,8,"C O A S T L I N E",t->ink);char cash[24]="";money(cash,g->save.cash,sizeof cash);right(s,244,8,cash,t->accent);
  glass(s,10,37,236,40,0);gfx_text_small(s,20,42,"READY IN YOUR BAY",t->muted);limited(s,20,55,120,vehicles[g->save.selected_vehicle].name,t->ink);right_small(s,238,56,locations[g->save.selected_location].name,t->accent);
  for(int i=0;i<6;i++){int x=10+(i%2)*120,y=84+(i/2)*30,sel=g->cursor==i;glass(s,x,y,116,26,sel);home_icon(s,x+12,y+13,i,sel);gfx_text(s,x+25,y+7,labels[i],sel?t->selected_ink:t->ink);}
  footer(s,interface==SKIN_FUSION?"FUSION / D-PAD: CHOOSE":"APEX  /  DRIVE CLUB","A: Open");return;
 }
 if(interface==1){
  gfx_text_small(s,17,8,"SIGNAL / COASTLINE",t->accent);char cash[24]="";money(cash,g->save.cash,sizeof cash);right(s,244,8,cash,t->ink);
  gfx_text(s,18,31,"DRIVER TERMINAL",t->ink);limited_small(s,18,46,222,locations[g->save.selected_location].name,t->muted);
  for(int i=0;i<6;i++){int y=62+i*18,sel=g->cursor==i;glass(s,16,y,230,16,sel);char n[8]="0";number(n,i+1,sizeof n);gfx_text_small(s,23,y+4,n,sel?t->accent:t->muted);gfx_text(s,47,y+3,labels[i],sel?t->selected_ink:t->ink);if(sel)right_small(s,238,y+4,"ONLINE >",t->accent);}
  footer(s,"SIGNAL LINK READY","A: Open");return;
 }
 large_text(s,12,2,"Coastline",t->ink);char cash[24]="";money(cash,g->save.cash,sizeof cash);glass(s,176,5,68,22,0);right(s,235,11,cash,t->accent);
 gfx_text_small(s,14,30,"ENDLESS DRIVE CLUB",t->muted);char best[28]="BEST ";number(best,g->save.best_score,sizeof best);right(s,243,29,best,t->muted);
 for(int i=0;i<6;i++){int y=43+i*21,sel=g->cursor==i;glass(s,11,y,234,19,sel);home_icon(s,25,y+9,i,sel);gfx_text(s,40,y+4,labels[i],sel?t->selected_ink:t->ink);right_small(s,234,y+5,hints[i],sel?t->selected_detail:t->accent);}
 footer(s,"Up / Down: Choose","A: Open");
}
static void garage_line(Surface*s,const Game*g,int row_id,const char*label,const char*value,int locked){
 int y=62+row_id*18,selected=g->cursor==row_id;
 glass(s,4,y,248,17,selected);
 uint16_t ink=selected?t->selected_ink:locked?t->muted:t->ink,detail=selected?t->selected_detail:locked?t->coral:t->accent;
 limited_small(s,11,y+5,96,label,ink);right_small(s,244,y+5,value?value:"",detail);
}
static void garage(Surface*s,const Game*g){
 static const char*const tabs[4]={"Showroom","Tuning","Style","Dealer"};
 for(int i=0;i<4;i++){glass(s,i*64,0,63,20,g->garage_tab==i);center_small(s,i*64,6,63,tabs[i],g->garage_tab==i?t->selected_ink:t->ink);}
 char cash[24]="";money(cash,g->save.cash,sizeof cash);gfx_text_small(s,6,25,g->garage_tab==3?"SELECT CLASS":"SELECT VEHICLE",t->muted);right_small(s,179,25,cash,t->accent);
 char own[24]="OWNED ";number(own,game_owned_count(g),sizeof own);append(own,"/31",sizeof own);right_small(s,251,25,own,t->muted);
 int id=clamp(g->selection,0,VEHICLE_COUNT-1);
 glass(s,4,37,248,22,0);limited(s,11,40,150,vehicles[id].name,t->ink);char meta[32]="";append(meta,game_class_name(vehicles[id].class_id),sizeof meta);append(meta," / ",sizeof meta);append(meta,game_specialty_name(vehicles[id].specialty),sizeof meta);right_small(s,244,44,meta,t->accent);
 if(g->garage_tab==3){
  for(int c=0;c<5;c++){int x=5+c*50;glass(s,x,63,47,18,c==g->garage_filter);center_small(s,x,68,47,game_class_name(c),c==g->garage_filter?t->selected_ink:t->muted);}
  int count=game_dealer_count(g->garage_filter),selected=g->cursor?g->cursor-1:0,first=clamp(selected-2,0,count>5?count-5:0);
  for(int r=0;r<5 && first+r<count;r++){
   int v=game_dealer_vehicle(g->garage_filter,first+r),sel=g->cursor==first+r+1,owned=(g->save.owned&(1u<<v))!=0,y=85+r*19;
   glass(s,4,y,248,18,sel);limited(s,12,y+3,127,vehicles[v].name,sel?t->selected_ink:t->ink);
   char price[24]="";if(owned)append(price,"IN COLLECTION",sizeof price);else if(!game_can_vehicle(g,v))append(price,game_class_unlocked(g,g->garage_filter)?"CHALLENGE":"CLASS LOCKED",sizeof price);else money(price,vehicles[v].price,sizeof price);
   right_small(s,244,y+5,price,sel?t->selected_detail:t->accent);
  }
  const char *detail=game_can_vehicle(g,id)?vehicles[id].description:game_vehicle_lock_reason(g,id);limited_small(s,8,184,240,detail,t->muted);return;
 }
 if(g->garage_tab==0){
  garage_line(s,g,0,"Vehicle",vehicles[id].name,0);garage_line(s,g,1,"Grab your keys",g->save.selected_vehicle==id?"EQUIPPED":"EQUIP",0);
  garage_line(s,g,2,"Departure",locations[g->save.selected_location].name,0);garage_line(s,g,3,"Route library","VIEW / UNLOCK",0);
  char preset[24]="PRESET ";number(preset,g->preset_choice+1,sizeof preset);append(preset,g->save.preset_mask&(1u<<g->preset_choice)?" / SAVED":" / EMPTY",sizeof preset);garage_line(s,g,4,"Saved setup",preset,0);
  int cost=game_preset_cost(g,g->preset_choice);char value[24]="";if(cost<0)append(value,"EMPTY",sizeof value);else if(cost)money(value,(uint32_t)cost,sizeof value);else append(value,"READY",sizeof value);
  garage_line(s,g,5,"Load preset",value,cost<0);garage_line(s,g,6,"Save current setup","SAVE",0);
 }else if(g->garage_tab==1){
  garage_line(s,g,0,"Vehicle",vehicles[id].name,0);garage_line(s,g,1,"Build",game_build_name(g->build_choice),0);garage_line(s,g,2,"Install build","APPLY",0);
  const uint8_t*levels[4]={g->save.engine,g->save.grip,g->save.armor,g->save.recovery};static const char*const names[4]={"Engine","Road grip","Armor","Turbo recovery"};
  for(int i=0;i<4;i++){char label[36]="",value[24]="";append(label,names[i],sizeof label);append(label," ",sizeof label);number(label,levels[i][id],sizeof label);append(label,"/6",sizeof label);
   int locked=game_mastery_level(g,id)<game_upgrade_rank(g,i);if(levels[i][id]>=6)append(value,"MAX",sizeof value);else if(locked){append(value,"MASTERY ",sizeof value);number(value,game_upgrade_rank(g,i),sizeof value);}else money(value,game_upgrade_cost(g,i),sizeof value);garage_line(s,g,3+i,label,value,locked);}
 }else{
  garage_line(s,g,0,"Vehicle",vehicles[id].name,0);
  const char *labels[4]={"Paint","Livery",id>=25?"Hardware":"Tire finish","Tire smoke"};
  const char *values[4]={paint_names[g->paint_choice%6],game_livery_name(g->livery_choice),game_wheel_name(g->wheel_choice),game_smoke_name(g->smoke_choice)};
  int kinds[4]={2,0,1,3},choices[4]={g->paint_choice,g->livery_choice,g->wheel_choice,g->smoke_choice},total=0,locked=0;
  for(int i=0;i<4;i++){int cost=game_cosmetic_cost(g,kinds[i],choices[i]);char value[64]="";append(value,values[i],sizeof value);append(value," / ",sizeof value);if(cost<0){append(value,"LOCKED",sizeof value);locked=1;}else if(!cost)append(value,"FITTED",sizeof value);else{money(value,(uint32_t)cost,sizeof value);total+=cost;}garage_line(s,g,i+1,labels[i],value,cost<0);}
  char price[24]="";if(locked)append(price,"LOCKED",sizeof price);else if(total)money(price,(uint32_t)total,sizeof price);else append(price,"FITTED",sizeof price);garage_line(s,g,5,"Install finish",price,locked);
  limited_small(s,9,175,238,"Returning finishes: 50% off installation.",t->muted);
 }
}
static void contract_goal(char*text,const DispatchContract*contract,int cap){
 switch(contract->kind){
 case DC_NEAR:append(text,"Pass ",cap);number(text,contract->goal,cap);append(text," near misses",cap);break;
 case DC_DRIFT:append(text,"Earn ",cap);number(text,contract->goal,cap);append(text," drift points",cap);break;
 case DC_DISTANCE:append(text,"Travel ",cap);number(text,contract->goal,cap);append(text," m",cap);break;
 case DC_COINS:append(text,"Collect ",cap);number(text,contract->goal,cap);append(text," coins",cap);break;
 default:append(text,"Hold top speed for ",cap);number(text,contract->goal/DRIVE_HZ,cap);append(text," sec",cap);break;
 }
}
static void contracts(Surface*s,const Game*g){
 header(s,"DISPATCH",g);gfx_text_small(s,15,33,"OPTIONAL / COMPLETE FOR A CASH BONUS",t->muted);
 for(int slot=0;slot<3;slot++){
  DispatchContract offer;game_contract_offer(g,slot,&offer);int selected=g->cursor==slot,y=46+slot*35;
  glass(s,11,y,234,32,selected);uint16_t ink=selected?t->selected_ink:t->ink,detail=selected?t->selected_detail:t->accent;
  limited(s,20,y+3,175,game_contract_name(offer.kind),ink);if(g->save.dispatch_choice==slot)right_small(s,235,y+5,"ARMED",detail);
  char goal[80]="";contract_goal(goal,&offer,sizeof goal);limited_small(s,20,y+20,163,goal,selected?t->selected_ink:t->muted);char reward[24]="+";money(reward,offer.reward,sizeof reward);right(s,235,y+18,reward,detail);
 }
 row(s,153,20,"Drive without a contract",g->save.dispatch_choice==3?"ARMED":"SKIP",g->cursor==3,0);footer(s,"A: Arm next-run job","B: Back");
}
static void departures(Surface*s,const Game*g){
 header(s,"DEPARTURES",g);int id=clamp(g->cursor,0,LOCATION_COUNT-1),first=clamp(id-2,0,LOCATION_COUNT>6?LOCATION_COUNT-6:0);
 char page[18]="";number(page,id+1,sizeof page);append(page," / ",sizeof page);number(page,LOCATION_COUNT,sizeof page);gfx_text_small(s,104,10,page,t->muted);
 int visible=LOCATION_COUNT<6?LOCATION_COUNT:6;
 for(int r=0;r<visible;r++){int i=first+r;char value[24]="";int owned=(g->save.unlocked_locations&(1u<<i))!=0,available=game_can_location(g,i);if(owned)append(value,g->save.selected_location==i?"SELECTED":"OPEN",sizeof value);else if(available)money(value,locations[i].price,sizeof value);else append(value,"LOCKED",sizeof value);row(s,34+r*21,18,locations[i].name,value,id==i,!owned&&!available);}
 limited(s,15,164,227,!game_can_location(g,id)?locations[id].requirement:locations[id].description,t->muted);footer(s,"A: Set departure","B: Back");
}
static void challenges(Surface*s,const Game*g){
 header(s,"ROAD CHALLENGES",g);static const char*const units[8]={" near","m"," drift","s"," wheelie"," jobs"," near"," drift"};
 int cursor=clamp(g->cursor,0,CHALLENGE_COUNT-1),first=clamp(cursor-2,0,CHALLENGE_COUNT>5?CHALLENGE_COUNT-5:0),visible=CHALLENGE_COUNT<5?CHALLENGE_COUNT:5;
 char page[16]="";number(page,cursor+1,sizeof page);append(page,"/",sizeof page);number(page,CHALLENGE_COUNT,sizeof page);gfx_text_small(s,140,11,page,t->muted);
 for(int row_id=0;row_id<visible;row_id++){int i=first+row_id,goal=1,progress=game_challenge_progress(g,i,&goal),done=progress>=goal;if(i==3){progress/=DRIVE_HZ;goal/=DRIVE_HZ;}int y=34+row_id*28,selected=cursor==i;glass(s,11,y,234,25,selected);uint16_t ink=selected?t->selected_ink:t->ink,muted=selected?t->selected_ink:t->muted;limited(s,18,y+2,126,game_challenge_name(i),ink);char val[32]="";number(val,progress,sizeof val);append(val,"/",sizeof val);number(val,goal,sizeof val);append(val,units[i],sizeof val);right(s,236,y+2,val,selected?t->selected_detail:done?t->accent:t->coral);limited_small(s,18,y+15,180,game_challenge_reward(i),muted);meter(s,202,y+17,34,progress,goal,selected?t->selected_detail:t->accent,t->shadow);}
 footer(s,cursor==5?"Dispatch completions persist":"Complete in one run","B: Back");
}
static void career(Surface*s,const Game*g){
 header(s,"CAREER",g);glass(s,11,34,234,33,0);char level[30]="LEVEL ";number(level,game_career_level(g),sizeof level);gfx_text(s,20,40,level,t->ink);char points[28]="";number(points,game_perk_available(g),sizeof points);append(points," POINTS",sizeof points);right(s,235,40,points,t->accent);uint32_t next=game_career_next(g);meter(s,20,56,117,(int)g->save.career_xp,next?(int)next:1,t->accent,t->shadow);char xp[28]="";number(xp,g->save.career_xp,sizeof xp);if(next){append(xp,"/",sizeof xp);number(xp,next,sizeof xp);}else append(xp," MAX",sizeof xp);right(s,235,51,xp,t->muted);
 static const char*const names[4]={"Recovery","Focus","Payout","Reset perks"};for(int i=0;i<4;i++){char value[24]="";if(i<3){number(value,g->save.perks[i],sizeof value);append(value," / 3",sizeof value);}else append(value,"FREE",sizeof value);row(s,74+i*22,19,names[i],value,g->cursor==i,0);}
 static const char*const desc[4]={"Once/run at low health: +10/18/26.","Reach 5x combos 15% sooner per rank.","Earn 8% more run cash per rank.","Return every spent career point, free."};limited(s,15,165,226,desc[clamp(g->cursor,0,3)],t->muted);footer(s,"A: Invest  |  L2: Mangroves","B: Back");
}
static void settings(Surface*s,const Game*g){
 header(s,"PERSONAL SETTINGS",g);static const char*const names[9]={"Music","Music volume","Sound effects","Effects volume","Interface","Road motion","Impact shake","Speed units","Controls"};static const char*const skins[SKIN_COUNT]={"Apex","Signal","Midnight","Fusion"};static const char*const motion[3]={"Calm","Cruise","Express"};
 if(interface==0){gfx_text_small(s,15,34,g->cursor<4?"01 / AUDIO":g->cursor<8?"02 / DISPLAY":"03 / BUTTON GUIDE",t->accent);}
 if(interface==SKIN_SIGNAL || interface==SKIN_FUSION){
  int section=g->cursor<4?0:g->cursor<8?1:2;static const char*const groups[3]={"AUDIO","DISPLAY","GUIDE"};
  for(int j=0;j<3;j++){glass(s,13,40+j*38,64,32,section==j);center_small(s,13,52+j*38,64,groups[j],section==j?t->selected_ink:t->muted);}
  if(interface==SKIN_FUSION)gfx_text_small(s,15,31,"L/R: Pages  |  Up/Down: Scroll",t->muted);
 }
 for(int i=0;i<9;i++){char val[28]="";switch(i){case 0:append(val,g->save.music_on?"On":"Off",sizeof val);break;case 1:number(val,g->save.music_volume,sizeof val);append(val,"%",sizeof val);break;case 2:append(val,g->save.sfx_on?"On":"Off",sizeof val);break;case 3:number(val,g->save.sfx_volume,sizeof val);append(val,"%",sizeof val);break;case 4:append(val,skins[g->save.skin%SKIN_COUNT],sizeof val);break;case 5:append(val,motion[g->save.motion%3],sizeof val);break;case 6:append(val,g->save.shake?"On":"Off",sizeof val);break;case 7:append(val,g->save.units?"mph":"km/h",sizeof val);break;default:append(val,"Open",sizeof val);break;}
  if(interface==2)row(s,34+i*15,14,names[i],val,g->cursor==i,0);
  else if(interface==0){int first=clamp((int)g->cursor-2,0,3);if(i>=first && i<first+6)row(s,49+(i-first)*21,19,names[i],val,g->cursor==i,0);}
  else{int first=g->cursor<4?0:g->cursor<8?4:8;if(i>=first&&i<first+4){int y=39+(i-first)*33,sel=g->cursor==i;glass(s,84,y,161,29,sel);gfx_text_small(s,94,y+4,names[i],sel?t->selected_ink:t->muted);gfx_text(s,94,y+15,val,sel?t->selected_detail:t->ink);if(sel)right_small(s,236,y+16,"< >",t->accent);}}
 }
 footer(s,"Left/Right: Adjust","A: Cycle   B: Back");
}
static void controls(Surface*s,const Game*g){
 header(s,g->controls_page?"TURBO GUIDE  2 / 2":"DRIVING CONTROLS  1 / 2",g);
 if(!g->controls_page){
  static const char*const keys[8]={"R","L","Left / Right","Up + R","Down + R","B","A","Start"};
  static const char*const actions[8]={"Accelerate","Brake / reverse","Steer","Turbo (uses charge)","Slow / recharge","Hold + steer: drift","Horn / bike wheelie","Pause"};
  for(int i=0;i<8;i++){int y=33+i*15;glass(s,12,y,75,14,0);center(s,12,y+2,75,keys[i],t->accent);gfx_text(s,96,y+2,actions[i],t->ink);}
  gfx_text_small(s,16,157,"Slow driving is safe. Stay off zero!",t->ink);
  gfx_text_small(s,16,167,"Stopped or reversing? Police catch up.",t->coral);
 }else{
  glass(s,11,34,234,36,0);gfx_text(s,19,39,"Hold Up + R to boost. Full = 8 sec.",t->ink);gfx_text_small(s,19,52,"Start at 10%. Empty? Release Up to rearm.",t->muted);gfx_text_small(s,19,62,"Rates include this vehicle's Recovery parts.",t->muted);
  static const char*const labels[4]={"Near miss","Drift > 0.75 sec","Wheelie > 1 sec","Slow > 40 km/h (no combo)"};static const int base[4]={10,4,3,2};int vehicle=clamp(g->save.selected_vehicle,0,VEHICLE_COUNT-1),recovery=100+5*g->save.recovery[vehicle];
  for(int i=0;i<4;i++){int y=75+i*20;glass(s,11,y,234,18,0);gfx_text_small(s,19,y+5,labels[i],t->ink);int tenth=base[i]*recovery/10;char rate[24]="+";number(rate,tenth/10,sizeof rate);if(tenth%10){append(rate,".",sizeof rate);number(rate,tenth%10,sizeof rate);}append(rate,i?"% / sec":"%",sizeof rate);right(s,236,y+4,rate,t->accent);}
  gfx_text_small(s,16,158,"Combo: +0.25 steps. Speed builds slowly.",t->ink);
  gfx_text_small(s,16,168,"5x combo = 2x skill recharge. No idle fill.",t->muted);
 }
 footer(s,"L / R: Change page","B: Back");
}
static void pause_menu(Surface*s,const Game*g){
 header(s,"TAKE A BREATHER",g);char info[64]="";number(info,g->run.distance_m,sizeof info);append(info," m  /  ",sizeof info);time_text(info,g->run_ticks,sizeof info);center(s,11,37,234,info,t->muted);static const char*const names[4]={"Resume driving","Bank this run","Controls","Settings"};static const char*const values[4]={"GO","CASH OUT","VIEW","ADJUST"};for(int i=0;i<4;i++)row(s,57+i*28,24,names[i],values[i],g->cursor==i,0);footer(s,"A: Choose","B / Start: Resume");
}

/* Critical information stays next to the top game screen. Each vehicle keeps
 * its own instruments in the lower cockpit, below the shared trip computer. */
enum Cluster { CL_CLASSIC,CL_DIGITAL,CL_UTILITY,CL_BIKE,CL_LUXURY,CL_RACE,CL_ACTIVE,CL_BALANCE,CL_HORSE };
static int cluster_for(int v){
 switch(v){
 case 0:case 5:case 10:return CL_CLASSIC;
 case 1:case 6:case 11:case 14:case 17:return CL_DIGITAL;
 case 2:case 8:case 12:case 13:case 20:case 24:return CL_UTILITY;
 case 3:case 27:return CL_BIKE;
 case 7:case 15:case 18:case 22:return CL_LUXURY;
 case 25:case 26:case 29:return CL_ACTIVE;
 case 28:return CL_BALANCE;
 case 30:return CL_HORSE;
 default:return CL_RACE;
 }
}
static uint16_t dashboard_art[256*192] __attribute__((aligned(4)));
static int dashboard_key=-1;
/* Small opaque field caches keep the doubled, antialiased readouts exact
 * without rebuilding unchanged glyphs every dashboard update. */
static uint16_t body_art[68*42] __attribute__((aligned(4)));
static uint16_t combo_art[68*42] __attribute__((aligned(4)));
static uint16_t location_art[113*14] __attribute__((aligned(4)));
static int body_key=-1,body_value=-1,combo_key=-1,combo_value=-1;
static int location_key=-1,location_value=-1;
typedef struct {
 int speed,accel,ratio,reverse,health,combo,location,charge,turbo,latched,recharging,source,driving,impact,police,police_seconds,contract_kind,contract_complete,contract_fill;
 uint32_t points,cash,distance,coins,contract_progress,contract_goal;
} DashboardReadout;
static DashboardReadout dashboard_previous;
static uint16_t *dashboard_target;
static int dashboard_target_key=-1;
static void field_background(uint16_t *pixels,int x,int y,int w,int h){
 for(int row=0;row<h;row++)gfx_copy(pixels+row*w,dashboard_art+(y+row)*256+x,w);
}
static void field_copy(Surface*s,const uint16_t*pixels,int x,int y,int w,int h){
 for(int row=0;row<h;row++)gfx_copy(s->pixels+(y+row)*s->w+x,pixels+row*w,w);
}
static void dashboard_restore(Surface*s,int x,int y,int w,int h){
 for(int row=0;row<h;row++)gfx_copy(s->pixels+(y+row)*s->w+x,dashboard_art+(y+row)*256+x,w);
}
static void digit(Surface*s,int x,int y,int n,int scale,uint16_t color,uint16_t off){
 static const unsigned char masks[10]={0x3f,0x06,0x5b,0x4f,0x66,0x6d,0x7d,0x07,0x7f,0x6f};unsigned bits=masks[clamp(n,0,9)];int a=scale,b=scale*4;
 for(int i=0;i<7;i++){uint16_t c=(bits&(1u<<i))?color:off;switch(i){case 0:gfx_rect(s,x+a,y,b,a,c);break;case 1:gfx_rect(s,x+a+b,y+a,a,b,c);break;case 2:gfx_rect(s,x+a+b,y+2*a+b,a,b,c);break;case 3:gfx_rect(s,x+a,y+2*(a+b),b,a,c);break;case 4:gfx_rect(s,x,y+2*a+b,a,b,c);break;case 5:gfx_rect(s,x,y+a,a,b,c);break;default:gfx_rect(s,x+a,y+a+b,b,a,c);break;}}
}
static void digits(Surface*s,int x,int y,int value,int scale,uint16_t color,uint16_t off){int step=scale*7;value=clamp(value,0,999);digit(s,x,y,value/100,scale,color,off);digit(s,x+step,y,value/10%10,scale,color,off);digit(s,x+step*2,y,value%10,scale,color,off);}
static void bolt(Surface*s,int x,int y){disc(s,x,y,3,C(88,109,118));disc(s,x,y,1,DARK);gfx_line(s,x-1,y+1,x+1,y-1,C(160,180,185));}
static void analog_face(Surface*s,int x,int y,int r,int accel,int cream,int units){
 uint16_t face=cream?C(235,230,207):C(9,24,34),ink=cream?C(40,62,67):LIGHT,muted=cream?C(117,133,129):SOFT;
 disc(s,x,y+2,r+3,DARK);disc(s,x,y,r+2,C(122,145,153));disc(s,x,y,r,C(31,53,65));disc(s,x,y,r-2,C(164,182,181));disc(s,x,y,r-4,face);
 for(int i=0;i<=30;i++){int major=i%5==0,outer=r-7,inner=outer-(major?5:2);gfx_line(s,x+needle_xy[i][0]*inner/1024,y+needle_xy[i][1]*inner/1024,x+needle_xy[i][0]*outer/1024,y+needle_xy[i][1]*outer/1024,i>25&&accel!=1?WARN:major?ink:muted);}
 int off=r-17;
 if(accel==1){gfx_text_small(s,x-off-9,y+10,"-40",muted);gfx_text_small(s,x-3,y-r+10,"0",muted);gfx_text_small(s,x+off-9,y+10,"+40",muted);}
 else{gfx_text_small(s,x-off-3,y+10,"0",muted);gfx_text_small(s,x-off-2,y-24,units?"60":"100",muted);gfx_text_small(s,x+off-17,y-24,units?"120":"200",muted);gfx_text_small(s,x+off-5,y+10,units?"180":"300",muted);}
}
static void needle(Surface*s,int x,int y,int radius,int index,uint16_t color){index=clamp(index,0,30);int dx=needle_xy[index][0],dy=needle_xy[index][1],ex=x+dx*radius/1024,ey=y+dy*radius/1024;gfx_line(s,x+1,y+1,ex+1,ey+1,DARK);gfx_line(s,x-dx*5/1024,y-dy*5/1024,ex,ey,color);gfx_line(s,x,y+1,ex,ey+1,color);disc(s,x,y,4,C(150,172,178));disc(s,x,y,2,DARK);}
static void cockpit_panel(Surface*s,int x,int y,int w,int h){gfx_card(s,x,y+2,w,h,DARK,DARK);gfx_card(s,x,y,w,h,C(8,27,38),C(88,120,133));gfx_rect(s,x+6,y+2,w-12,1,C(61,91,107));}
static void dashboard_background(const Game*g,int family,int key){
 Surface cache={dashboard_art,256,192},instruments={dashboard_art+76*256,256,116};Surface*s=&cache;int v=g->vehicle;
 gradient(s,0,0,256,192,t->cockpit_top,t->cockpit_bottom);
 gfx_text_small(s,12,4,vehicles[v].name,LIGHT);
 glass(s,11,16,68,42,0);glass(s,84,16,68,42,0);glass(s,157,16,88,42,0);
 gfx_text_small(s,18,20,"BODY",t->muted);gfx_text_small(s,91,20,"COMBO",t->muted);
 gfx_text_small(s,164,20,"POINTS",t->muted);gfx_text_small(s,164,45,"CASH",t->muted);
 cockpit_panel(s,11,62,234,21);gfx_text_small(s,18,65,"TURBO",SOFT);
 gfx_rect(s,11,96,234,1,mix(t->light,DARK,150));
 gfx_text_small(s,12,86,"TRIP",SOFT);gfx_text_small(s,114,86,"COINS",SOFT);
 s=&instruments;
 if(v==5){for(int y=21;y<116;y++){uint16_t c=C(72+(y*13%11),49+(y*7%8),40+(y%6));gfx_rect(s,0,y,256,1,mix(c,t->cockpit_top,75));}for(int y=25;y<115;y+=19)gfx_line(s,0,y,256,y+5,C(51,37,34));}
 if(family==CL_HORSE){gradient(s,0,21,256,95,C(105,76,48),C(52,39,29));for(int x=6;x<256;x+=7){gfx_rect(s,x,24,3,1,C(179,150,100));gfx_rect(s,x,112,3,1,C(179,150,100));}}
 if(family==CL_RACE){for(int y=21;y<116;y+=5)for(int x=(y&4)?0:-4;x<256;x+=8)gfx_line(s,x,y,x+4,y+4,mix(t->cockpit_top,DARK,85));}
 const char*unit=g->save.units?"mph":"km/h";
 switch(family){
 case CL_CLASSIC:
  analog_face(s,65,67,44,0,v==5,g->save.units);analog_face(s,191,67,44,1,v==5,g->save.units);
  cockpit_panel(s,116,54,24,25);center(s,24,103,82,unit,v==5?C(63,81,78):SOFT);center(s,150,103,82,"m/s2",v==5?C(63,81,78):SOFT);break;
 case CL_DIGITAL:
  cockpit_panel(s,12,36,139,79);cockpit_panel(s,159,36,85,79);gfx_text_small(s,24,42,"VELOCITY",SOFT);center(s,20,99,124,unit,SOFT);gfx_text_small(s,171,42,"ACCEL",SOFT);gfx_text_small(s,177,99,"m/s2",SOFT);
  for(int i=0;i<28;i++){gfx_rect(s,15+i*8,24,6,6,C(25,51,64));}gfx_rect(s,23,89,115,1,C(32,71,85));break;
 case CL_UTILITY:
  cockpit_panel(s,12,37,144,78);cockpit_panel(s,165,37,79,35);cockpit_panel(s,165,80,79,35);
  gfx_text_small(s,24,42,"ROAD SPEED",SOFT);center(s,26,99,116,unit,SOFT);gfx_text_small(s,172,41,"ACCEL m/s2",SOFT);gfx_text_small(s,172,84,"BODY",SOFT);
  for(int x=18;x<242;x+=7){gfx_rect(s,x,25,4,3,C(99,121,130));}bolt(s,8,43);bolt(s,249,43);bolt(s,8,108);bolt(s,249,108);break;
 case CL_BIKE:
  cockpit_panel(s,12,37,232,78);gfx_line(s,17,35,34,21,C(141,162,166));gfx_line(s,239,35,222,21,C(141,162,166));
  for(int i=0;i<24;i++){gfx_rect(s,25+i*9,24,7,7,C(28,55,65));}gfx_text_small(s,23,44,"ACCEL",SOFT);gfx_text_small(s,23,74,"m/s2",SOFT);center(s,86,100,86,unit,SOFT);gfx_text_small(s,209,44,"GEAR",SOFT);bolt(s,18,107);bolt(s,239,107);break;
 case CL_LUXURY:
  for(int y=21;y<118;y+=9){gfx_line(s,3,y,17,y+8,mix(t->cockpit_top,LIGHT,35));gfx_line(s,239,y+8,253,y,mix(t->cockpit_top,LIGHT,35));}
  analog_face(s,128,66,45,0,1,g->save.units);cockpit_panel(s,22,40,42,60);cockpit_panel(s,192,40,42,60);center(s,22,45,42,"ACCEL",SOFT);center(s,22,82,42,"m/s2",SOFT);center(s,192,45,42,"GEAR",SOFT);center(s,88,103,80,unit,C(70,85,79));break;
 case CL_RACE:
  cockpit_panel(s,12,42,43,58);cockpit_panel(s,190,42,54,58);gfx_text_small(s,20,46,"GEAR",SOFT);gfx_text_small(s,196,46,"ACCEL",SOFT);gfx_text_small(s,200,85,"m/s2",SOFT);
  for(int i=0;i<28;i++){gfx_rect(s,16+i*8,25,6,9,C(30,47,58));}center(s,64,105,124,unit,SOFT);break;
 case CL_ACTIVE:
  gfx_text_small(s,21,25,v==26?"CYCLE COMPUTER":v==25?"SKATE COMPUTER":"BOARD COMPUTER",SOFT);right_small(s,235,25,"RIDE",t->light);
  cockpit_panel(s,12,37,232,78);gfx_text_small(s,23,44,"ACCEL",SOFT);gfx_text_small(s,23,75,"m/s2",SOFT);gfx_text_small(s,205,44,"MODE",SOFT);center(s,87,100,83,unit,SOFT);
  gfx_rect(s,23,94,44,2,C(39,80,91));gfx_rect(s,189,94,44,2,C(39,80,91));break;
 case CL_BALANCE:
  gfx_text_small(s,20,25,"BALANCE COMPUTER",SOFT);cockpit_panel(s,12,37,232,78);gfx_text_small(s,23,44,"ACCEL",SOFT);gfx_text_small(s,23,75,"m/s2",SOFT);gfx_text_small(s,204,44,"MODE",SOFT);center(s,87,100,83,unit,SOFT);
  gfx_line(s,60,94,196,94,C(102,168,174));disc(s,61,94,4,C(89,129,144));disc(s,195,94,4,C(89,129,144));break;
 case CL_HORSE:
  analog_face(s,128,67,44,0,1,g->save.units);cockpit_panel(s,18,40,47,60);cockpit_panel(s,190,40,51,60);center_small(s,18,46,47,"ACCEL",SOFT);center_small(s,18,85,47,"m/s2",SOFT);center_small(s,190,46,51,"GAIT",SOFT);center(s,88,103,80,unit,C(70,85,79));break;
 }
 dashboard_key=key;
}
static int display_speed(int speed,int units){return units?(speed*621+500)/1000:speed;}
static void accel_text(char*b,int accel,int cap){append(b,accel<0?"-":"+",cap);int n=accel<0?-accel:accel;number(b,n/10,cap);append(b,".",cap);number(b,n%10,cap);}
static void dynamic_tach(Surface*s,int count,int width,int y,int h,int ratio){
 int n=clamp(ratio,0,100)*count/100;
 for(int i=0;i<n;i++)gfx_rect(s,16+i*width,y,width-2,h,i>count*4/5?WARN:i>count*3/5?GOLD:t->light);
}
static void dashboard(Surface*s,const Game*g){
 int v=clamp(g->vehicle,0,VEHICLE_COUNT-1),family=cluster_for(v),key=v+VEHICLE_COUNT*(g->save.skin+SKIN_COUNT*g->save.units);
 if(dashboard_key!=key)dashboard_background(g,family,key);
 int native=g->speed_q8<0?-g->speed_q8:g->speed_q8;native=(native+Q/2)/Q;
 int speed=display_speed(native,g->save.units),accel=clamp(g->accel_q8*100/(Q*36),-999,999);
 int ratio=native*100/(game_top_speed(g)>0?game_top_speed(g):1),combo_quarters=clamp(g->combo_q8/(Q/4),4,20);
 int location=g->biome<LOCATION_COUNT?g->biome:0;
 int police=clamp(g->police_pressure,0,100);
 int police_seconds=(POLICE_CATCH_TICKS-clamp(g->police_ticks,0,POLICE_CATCH_TICKS)+DRIVE_HZ-1)/DRIVE_HZ;
 uint32_t goal=g->active_contract.goal,progress=game_contract_progress(g);if(progress>goal)progress=goal;
 int contract_kind=goal?g->active_contract.kind:-1,contract_fill=goal?(int)(111*progress/goal):0,contract_complete=goal && progress>=goal;
 if(contract_kind==DC_SPEED){progress/=DRIVE_HZ;goal/=DRIVE_HZ;}
 DashboardReadout now={speed,accel,ratio,g->speed_q8<0,g->health,combo_quarters,location,
  g->turbo_charge_q8,g->turbo,g->turbo_latched,g->turbo_recharging,g->turbo_source,
  g->wheelie?1:g->drifting?2:g->slow?3:g->turbo?4:0,g->crash_timer!=0,police,police_seconds,contract_kind,contract_complete,contract_fill,
  g->run.total_points,g->run.total_cash,g->run.distance_m,g->run.count[B_COIN],progress,goal};
 const DashboardReadout *old=&dashboard_previous;
 int refresh=dashboard_target!=s->pixels || dashboard_target_key!=key;
 int header_dirty=refresh || old->location!=now.location || old->impact!=now.impact ||
  old->police!=police || (police && old->police_seconds!=police_seconds) || old->contract_kind!=contract_kind ||
  old->contract_progress!=progress || old->contract_goal!=goal || old->contract_fill!=contract_fill || old->contract_complete!=now.contract_complete;
 int instruments_dirty=refresh || old->speed!=speed || old->accel!=accel || old->ratio!=ratio ||
  old->reverse!=now.reverse || (family==CL_UTILITY && old->health!=now.health);
 int body_dirty=refresh || old->health!=now.health,combo_dirty=refresh || old->combo!=combo_quarters;
 int score_dirty=refresh || old->points!=now.points || old->cash!=now.cash;
 int turbo_dirty=refresh || old->charge!=now.charge || old->turbo!=now.turbo || old->latched!=now.latched ||
  old->recharging!=now.recharging || old->source!=now.source;
 int trip_dirty=refresh || old->distance!=now.distance || old->coins!=now.coins || old->driving!=now.driving;
 /* The caller retains this RAM bitmap between updates. Restore only fields
  * whose displayed values changed; menus and new targets force a full fill. */
 if(refresh)gfx_copy(s->pixels,dashboard_art,256*192);
 if(header_dirty){
  if(!refresh)dashboard_restore(s,0,0,256,16);
  if(police){
   uint16_t warning=police>=67?WARN:GOLD;char pursuit[20]="POLICE  ";number(pursuit,police_seconds,sizeof pursuit);append(pursuit,"s",sizeof pursuit);
   gfx_text_small(s,133,3,pursuit,warning);gfx_rect(s,132,12,111,2,DARK);gfx_rect(s,132,12,111*police/100,2,warning);
  }else if(contract_kind>=0){
   static const char*const labels[5]={"NEAR","DRIFT","DIST","COINS","TOP"};char objective[64]="";
   if(contract_complete)append(objective,"DISPATCH COMPLETE",sizeof objective);else{append(objective,labels[clamp(contract_kind,0,4)],sizeof objective);append(objective," ",sizeof objective);number(objective,progress,sizeof objective);append(objective,"/",sizeof objective);number(objective,goal,sizeof objective);if(contract_kind==DC_DISTANCE)append(objective,"m",sizeof objective);if(contract_kind==DC_SPEED)append(objective,"s",sizeof objective);}
   limited_small(s,133,3,109,objective,contract_complete?GOLD:t->light);gfx_rect(s,132,12,111,2,DARK);gfx_rect(s,132,12,contract_fill,2,contract_complete?GOLD:t->light);
  }else{
  if(location_key!=key || location_value!=location){
   field_background(location_art,130,3,113,14);Surface label={location_art,113,14};
   limited(&label,0,0,113,locations[location].name,t->light);location_key=key;location_value=location;
  }
  field_copy(s,location_art,130,3,113,14);
  }
  if(g->crash_timer && !police){gfx_card(s,190,1,57,13,C(81,39,44),WARN);center(s,190,3,57,"IMPACT",LIGHT);}
 }
 if(instruments_dirty){
 if(!refresh)dashboard_restore(s,0,97,256,95);
 char a[16]="";accel_text(a,accel,sizeof a);const char*gear=g->speed_q8<0?"R":"D";uint16_t off=C(27,52,60);
 Surface instruments={s->pixels+76*s->w,s->w,s->h-76};Surface*full=s;s=&instruments;
 switch(family){
 case CL_CLASSIC:
  needle(s,65,67,30,g->save.units?speed/6:speed/10,v==5?C(145,48,31):t->light);needle(s,191,67,30,15+accel*15/400,v==5?C(145,48,31):t->light);
  digits(s,45,82,speed,2,v==5?C(16,33,41):t->light,v==5?C(228,225,205):off);center(s,150,88,82,a,v==5?C(16,33,41):t->light);center(s,150,77,82,"ACCEL",v==5?C(91,104,95):SOFT);center(s,116,62,24,gear,LIGHT);break;
 case CL_DIGITAL:
  digits(s,46,55,speed,3,t->light,off);center(s,159,69,85,a,accel<0?WARN:t->light);dynamic_tach(s,28,8,24,6,ratio);break;
 case CL_UTILITY:
  digits(s,46,56,speed,3,t->light,off);center(s,165,55,79,a,accel<0?WARN:t->light);char health[16]="";number(health,g->health,sizeof health);append(health,"%",sizeof health);right(s,233,95,health,g->health<30?WARN:LIGHT);break;
 case CL_BIKE:
  digits(s,87,48,speed,4,t->light,off);center(s,16,59,53,a,accel<0?WARN:t->light);center(s,208,63,29,gear,LIGHT);for(int i=0;i<clamp(ratio,0,100)*24/100;i++)gfx_rect(s,25+i*9,24,7,7,i>19?WARN:i>15?GOLD:t->light);break;
 case CL_LUXURY:
  needle(s,128,66,31,g->save.units?speed/6:speed/10,C(145,58,26));digits(s,108,82,speed,2,C(16,33,41),C(228,225,205));center(s,22,63,42,a,t->light);center(s,192,65,42,gear,GOLD);break;
 case CL_RACE:
  digits(s,79,48,speed,4,t->light,off);center(s,12,70,43,gear,GOLD);center(s,190,67,54,a,accel<0?WARN:t->light);dynamic_tach(s,28,8,25,9,ratio);break;
 case CL_ACTIVE:case CL_BALANCE:
  digits(s,87,48,speed,4,t->light,off);center(s,16,59,53,a,accel<0?WARN:t->light);center_small(s,197,64,42,g->speed_q8<0?"REVERSE":family==CL_BALANCE?"GLIDE":"ROLL",GOLD);break;
 case CL_HORSE:
  needle(s,128,67,30,g->save.units?speed/6:speed/10,C(145,58,26));digits(s,108,83,speed,2,C(16,33,41),C(228,225,205));center(s,18,64,47,a,t->light);center_small(s,190,65,51,g->speed_q8<0?"BACK":native<12?"WALK":native<25?"TROT":native<40?"CANTER":"GALLOP",GOLD);break;
 }
 s=full;
 }
 if(body_dirty){
 if(body_key!=key || body_value!=g->health){
  field_background(body_art,11,16,68,42);Surface body={body_art,68,42};
  char health[12]="";number(health,g->health,sizeof health);append(health,"%",sizeof health);
  large_text(&body,6,13,health,g->health<30?t->coral:t->ink);
  meter(&body,7,35,54,g->health,100,g->health<30?WARN:t->accent,mix(t->card_bottom,t->shadow,110));
  body_key=key;body_value=g->health;
 }
 field_copy(s,body_art,11,16,68,42);
 }
 if(combo_dirty){
 if(combo_key!=key || combo_value!=combo_quarters){
  field_background(combo_art,84,16,68,42);Surface readout={combo_art,68,42};
  char combo[16]="x";number(combo,combo_quarters/4,sizeof combo);append(combo,".",sizeof combo);int fraction=combo_quarters%4*25;if(fraction<10)append(combo,"0",sizeof combo);number(combo,fraction,sizeof combo);
  large_text(&readout,6,13,combo,t->accent);combo_key=key;combo_value=combo_quarters;
 }
 field_copy(s,combo_art,84,16,68,42);
 }
 if(score_dirty){
 if(!refresh)dashboard_restore(s,157,30,88,28);
 char score[24]="";number(score,g->run.total_points,sizeof score);right(s,237,32,score,t->ink);char cash[24]="";money(cash,g->run.total_cash,sizeof cash);right(s,237,44,cash,t->accent);
 }
 if(turbo_dirty){
 if(!refresh)dashboard_restore(s,11,62,234,22);
 int charge=clamp(g->turbo_charge_q8,0,TURBO_MAX_Q8),percent=charge*100/TURBO_MAX_Q8;
 uint16_t turbo_color=g->turbo?t->light:charge<TURBO_START_Q8?WARN:g->turbo_recharging?C(104,226,221):t->light;
 meter(s,61,66,139,charge,TURBO_MAX_Q8,turbo_color,DARK);char turbo[12]="";number(turbo,percent,sizeof turbo);append(turbo,"%",sizeof turbo);right(s,237,64,turbo,turbo_color);
 const char*status=g->turbo?"BOOST ACTIVE":g->turbo_latched?"RELEASE UP TO REARM":charge<TURBO_START_Q8?"BUILD CHARGE TO 10%":g->turbo_recharging?g->turbo_source==TS_NEAR?"RECHARGE / NEAR MISS":g->turbo_source==TS_DRIFT?"RECHARGE / DRIFT":g->turbo_source==TS_WHEELIE?"RECHARGE / WHEELIE":"RECHARGE / SLOW":"READY / HOLD UP + R";
 gfx_text_small(s,18,75,status,g->turbo_latched||charge<TURBO_START_Q8?WARN:SOFT);
 char seconds[12]="";number(seconds,charge/(DRIVE_HZ*Q),sizeof seconds);append(seconds,".",sizeof seconds);number(seconds,(charge%(DRIVE_HZ*Q))*10/(DRIVE_HZ*Q),sizeof seconds);append(seconds,"s",sizeof seconds);right(s,237,74,seconds,turbo_color);
 }
 if(trip_dirty){
 if(!refresh)dashboard_restore(s,11,85,234,12);
 char trip[24]="";number(trip,g->run.distance_m,sizeof trip);append(trip," m",sizeof trip);right(s,105,85,trip,LIGHT);
 char coins[24]="";number(coins,g->run.count[B_COIN],sizeof coins);right(s,177,85,coins,GOLD);
 const char*mode=g->wheelie?"WHEELIE":g->drifting?"DRIFT":g->slow?"CRUISE":g->turbo?"TURBO":"DRIVE";
 right(s,244,85,mode,g->turbo||g->drifting||g->wheelie?GOLD:t->light);
 }
 dashboard_previous=now;dashboard_target=s->pixels;dashboard_target_key=key;
}
static void receipt(Surface*s,const Game*g){
 if(interface!=SKIN_MIDNIGHT && interface!=SKIN_FUSION){
  backdrop(s,interface);header(s,interface==0?"RUN STATEMENT":"CITATION / CLOSED",g);
  char info[64]="";number(info,g->run.distance_m,sizeof info);append(info," m / ",sizeof info);time_text(info,g->run_ticks,sizeof info);gfx_text_small(s,16,35,info,t->muted);
  for(int i=0;i<BONUS_COUNT;i++){char val[40]="";number(val,g->run.count[i],sizeof val);append(val," x / ",sizeof val);number(val,g->run.points[i],sizeof val);row(s,49+i*12,11,bonus_names[i],val,0,0);}
  char pts[24]="",cash[24]="+";number(pts,g->run.total_points,sizeof pts);money(cash,g->run.total_cash,sizeof cash);row(s,136,17,"TOTAL POINTS",pts,0,0);row(s,156,18,"CASH AWARDED",cash,1,0);
  if(g->active_contract.goal){char award[24]="DISPATCH +";money(award,g->contract_cash,sizeof award);right_small(s,239,126,award,g->contract_complete?t->accent:t->muted);}
  footer(s,g->end_reason==END_STOPPED?"CAUGHT BY PATROL":"REWARDS SECURED","A: Continue");return;
 }
 uint16_t paper=C(250,246,223),ink=C(30,53,66),muted=C(104,118,116),stamp=C(178,67,61);
 gradient(s,0,0,256,192,t->sky,t->shore);gfx_rect(s,12,7,236,171,t->shadow);gfx_rect(s,8,3,236,173,paper);
 for(int x=8;x<244;x+=8){gfx_rect(s,x,176,7,1,paper);gfx_rect(s,x+1,177,5,1,paper);gfx_rect(s,x+2,178,3,1,paper);}
 gfx_rect(s,8,3,236,18,ink);center_small(s,8,8,236,"COASTLINE TRAFFIC DIVISION",paper);
 gfx_text(s,18,25,"DRIVING CITATION",ink);char citation[24]="#";number(citation,g->save.total_runs,sizeof citation);right(s,234,25,citation,muted);
 const char*reason=g->end_reason==END_STOPPED?"CAUGHT / STOPPED ON THE HIGHWAY":g->end_reason==END_CRASH?"VEHICLE DISABLED / RUN CLOSED":g->end_reason==END_BANKED?"VOLUNTARY CHECKOUT / RUN BANKED":g->end_reason==END_LIMIT?"ROUTE COMPLETE / RUN CLOSED":"DRIVING RECORD / RUN CLOSED";
 center_small(s,8,39,236,reason,stamp);
 char info[70]="";number(info,g->run.distance_m,sizeof info);append(info," m  |  ",sizeof info);time_text(info,g->run_ticks,sizeof info);append(info,"  |  ",sizeof info);number(info,display_speed(g->run.max_speed,g->save.units),sizeof info);append(info,g->save.units?" mph":" km/h",sizeof info);center_small(s,8,50,236,info,muted);
 gfx_rect(s,17,60,218,1,C(171,177,158));gfx_text_small(s,18,62,"VIOLATION / BONUS",muted);gfx_text_small(s,145,62,"QTY",muted);gfx_text_small(s,200,62,"POINTS",muted);
 static const char*const names[BONUS_COUNT]={"Distance","Survival","Near miss","Top speed","Drift","Wheelie","Coins"};int dispatch=g->active_contract.goal!=0;
 for(int i=0;i<BONUS_COUNT;i++){int y=dispatch?69+i*9:70+i*10;if(dispatch)gfx_text_small(s,18,y,names[i],ink);else gfx_text(s,18,y,names[i],ink);char qty[20]="";number(qty,g->run.count[i],sizeof qty);append(qty,i==B_DISTANCE?"m":i==B_SURVIVAL||i==B_SPEED||i==B_DRIFT||i==B_WHEELIE?"s":"x",sizeof qty);char pts[24]="";number(pts,g->run.points[i],sizeof pts);if(dispatch){right_small(s,163,y,qty,muted);right_small(s,234,y,pts,ink);}else{right(s,163,y,qty,muted);right(s,234,y,pts,ink);}}
 int totals_y=dispatch?136:144;gfx_rect(s,17,dispatch?133:140,218,1,C(124,145,134));gfx_text(s,18,totals_y,"TOTAL POINTS",ink);char total[28]="";number(total,g->run.total_points,sizeof total);right(s,234,totals_y,total,ink);
 if(dispatch){gfx_text_small(s,18,147,g->contract_complete?"DISPATCH COMPLETE":"DISPATCH INCOMPLETE",g->contract_complete?ink:muted);char award[24]="+";money(award,g->contract_cash,sizeof award);right_small(s,234,147,award,ink);}
 gfx_text(s,18,156,"CASH AWARDED",ink);char cash[24]="+";money(cash,g->run.total_cash,sizeof cash);right(s,234,156,cash,ink);
 char xp[88]="MASTERY +";number(xp,g->run.mastery_xp,sizeof xp);append(xp,"  CAREER +",sizeof xp);number(xp,g->run.career_xp,sizeof xp);if(g->run.cash_bonus){append(xp,"  PERK +",sizeof xp);money(xp,g->run.cash_bonus,sizeof xp);}center_small(s,8,167,236,xp,muted);center(s,0,182,256,"A: Continue",t->ink);
}
void ui_draw_bottom(Surface*s,const Game*g){
 interface=g->save.skin%SKIN_COUNT;t=&themes[interface==SKIN_FUSION?SKIN_MIDNIGHT:interface];if(g->mode==M_RUN){dashboard(s,g);return;}
 dashboard_target=0;
 if(g->mode==M_RECEIPT){receipt(s,g);return;}
 backdrop(s,g->save.skin%SKIN_COUNT);
 switch(g->mode){case M_HOME:home(s,g);break;case M_GARAGE:garage(s,g);break;case M_DEPARTURES:departures(s,g);break;case M_CHALLENGES:challenges(s,g);break;case M_CONTROLS:controls(s,g);break;case M_PAUSE:pause_menu(s,g);break;case M_CAREER:career(s,g);break;case M_SETTINGS:settings(s,g);break;case M_CONTRACTS:contracts(s,g);break;default:home(s,g);break;}
 if(g->mode!=M_GARAGE && g->notice_timer&&g->notice[0]){glass(s,10,155,236,21,1);limited(s,18,160,220,g->notice,t->selected_ink);}
}
