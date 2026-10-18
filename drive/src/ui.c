#include "ui.h"
#include "assets.h"

/* Small, original frosted-glass instruments and menus for the DS's native
 * resolution. Nothing in this file handles input or modifies game state. */
typedef struct {
 uint16_t sky,shore,card_top,card_bottom,edge,shadow,ink,muted,accent,coral;
 uint16_t selected_top,selected_bottom,selected_ink,selected_detail;
 uint16_t cockpit_top,cockpit_bottom,light;
} Theme;
static const Theme themes[SKIN_COUNT]={
 {C(180,230,233),C(248,236,224),C(247,255,250),C(206,235,234),C(250,255,251),C(115,165,175),C(28,66,83),C(76,121,135),C(4,137,151),C(238,112,104),C(42,167,174),C(10,126,146),C(250,255,246),C(255,221,166),C(23,58,72),C(10,28,43),C(107,241,222)},
 {C(248,195,181),C(218,185,223),C(255,235,225),C(239,208,212),C(255,244,230),C(166,121,151),C(76,50,78),C(133,94,122),C(186,80,111),C(235,130,91),C(198,113,141),C(151,73,120),C(255,245,231),C(255,222,161),C(70,42,65),C(29,24,48),C(255,182,128)},
 {C(17,35,61),C(41,62,82),C(49,78,99),C(27,54,77),C(102,148,165),C(6,20,36),C(236,250,247),C(155,198,208),C(105,233,220),C(250,146,155),C(50,128,147),C(34,89,121),C(248,255,249),C(255,218,163),C(28,44,76),C(9,20,40),C(145,206,255)}
};
static const Theme *t=&themes[0];
#define DARK C(6,16,25)
#define LIGHT C(241,252,244)
#define SOFT C(155,188,195)
#define WARN C(255,120,112)
#define GOLD C(255,207,123)
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
static void center_small(Surface*s,int x,int y,int w,const char*text,uint16_t color){
 int width=0;for(const char*p=text;*p;p++){unsigned c=(unsigned char)*p;width+=font_small_advance[(c>=32&&c<=127?c:'?')-32];}
 gfx_text_small(s,x+(w-width)/2,y,text,color);
}
static void limited(Surface*s,int x,int y,int width,const char*text,uint16_t color){
 char b[100]="";int n=0;if(width<5)return;
 while(text[n]&&n<95){b[n]=text[n];n++;b[n]=0;if(gfx_text_width(b)>width){b[--n]=0;while(n&&gfx_text_width(b)+gfx_text_width("...")>width)b[--n]=0;append(b,"...",sizeof b);break;}}
 gfx_text(s,x,y,b,color);
}
static void disc(Surface*s,int x,int y,int r,uint16_t color){int r2=r*r;for(int dy=-r;dy<=r;dy++){int dx=r;while(dx&&dx*dx+dy*dy>r2)dx--;gfx_rect(s,x-dx,y+dy,dx*2+1,1,color);}}
static void gradient(Surface*s,int x,int y,int w,int h,uint16_t top,uint16_t bottom){for(int j=0;j<h;j++)gfx_rect(s,x,y+j,w,1,mix(top,bottom,h>1?j*256/(h-1):0));}
static void glass(Surface*s,int x,int y,int w,int h,int selected){
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
 if(menu_skin!=skin){Surface bg={menu_art,256,192};gradient(&bg,0,0,256,192,t->sky,t->shore);
  disc(&bg,236,12,42,mix(t->sky,t->edge,34));disc(&bg,236,12,32,mix(t->sky,t->edge,62));disc(&bg,236,12,24,mix(t->sky,t->edge,100));
  for(int j=0;j<5;j++)gfx_line(&bg,-20,151+j*8,278,118+j*9,mix(t->shore,t->edge,55+j*12));
  palm(&bg,-6,91,3,mix(t->sky,t->accent,36));palm(&bg,249,123,2,mix(t->shore,t->accent,32));
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
 default:disc(s,x,y,4,c);disc(s,x,y,2,bg);for(int a=-1;a<=1;a+=2){gfx_rect(s,x-5,y+a*4,11,1,c);gfx_rect(s,x+a*4,y-5,1,11,c);}break;
 }
}
static void home(Surface*s,const Game*g){
 large_text(s,12,2,"Coastline",t->ink);char cash[24]="";money(cash,g->save.cash,sizeof cash);glass(s,176,5,68,22,0);right(s,235,11,cash,t->accent);
 gfx_text_small(s,14,30,"ENDLESS DRIVE CLUB",t->muted);char best[28]="BEST ";number(best,g->save.best_score,sizeof best);right(s,243,29,best,t->muted);
 static const char*const labels[6]={"Drive","Garage","Departures","Challenges","Career","Settings"};
 static const char*const hints[6]={"GO","COLLECTION","ROUTES","REWARDS","PROGRESS","PERSONALIZE"};
 for(int i=0;i<6;i++){int y=46+i*21,sel=g->cursor==i;glass(s,11,y,234,18,sel);home_icon(s,25,y+9,i,sel);gfx_text(s,40,y+4,labels[i],sel?t->selected_ink:t->ink);right(s,234,y+4,hints[i],sel?t->selected_detail:t->accent);}
 footer(s,"Up / Down: Choose","A: Open");
}
static void vehicle_card(Surface*s,const Game*g,int id){
 glass(s,11,51,234,27,g->cursor==0);gfx_text(s,22,54,vehicles[id].name,g->cursor==0?t->selected_ink:t->ink);if(g->cursor==0)gfx_rect(s,13,56,2,17,t->selected_detail);int rank=game_mastery_level(g,id);char r[24]="M";number(r,rank,sizeof r);append(r,"  ",sizeof r);number(r,id+1,sizeof r);append(r,"/10",sizeof r);right(s,234,54,r,g->cursor==0?t->selected_detail:t->accent);
 char xp[32]="";uint32_t next=game_mastery_next(g,id);number(xp,g->save.mastery_xp[id],sizeof xp);if(next){append(xp," / ",sizeof xp);number(xp,next,sizeof xp);}else append(xp," MAX",sizeof xp);
 gfx_text_small(s,22,67,"MASTERY",g->cursor==0?t->selected_ink:t->muted);meter(s,73,71,88,(int)g->save.mastery_xp[id],next?(int)next:1,g->cursor==0?t->selected_detail:t->accent,g->cursor==0?t->selected_bottom:t->shadow);right(s,234,66,xp,g->cursor==0?t->selected_ink:t->muted);
}
static void garage_detail(Surface*s,const char*line1,const char*line2){
 glass(s,11,154,234,21,0);limited(s,18,156,219,line1,t->ink);if(line2)limited(s,18,166,219,line2,t->muted);
}
static void garage(Surface*s,const Game*g){
 header(s,"GARAGE",g);static const char*const tabs[3]={"Showroom","Parts","Setup"};for(int i=0;i<3;i++){int x=11+i*79;glass(s,x,30,76,17,g->garage_tab==i);center(s,x,34,76,tabs[i],g->garage_tab==i?t->selected_ink:t->ink);}
 int id=clamp(g->selection,0,VEHICLE_COUNT-1),owned=(g->save.owned&(1u<<id))!=0,rank=game_mastery_level(g,id);vehicle_card(s,g,id);
 if(g->garage_tab==0){
  char value[28]="";if(owned)append(value,g->save.selected_vehicle==id?"EQUIPPED":"EQUIP",sizeof value);else if(game_can_vehicle(g,id))money(value,vehicles[id].price,sizeof value);else append(value,"CHALLENGE",sizeof value);
  row(s,84,21,owned?"Take the keys":"Buy vehicle",value,g->cursor==1,!owned&&!game_can_vehicle(g,id));
  int paint=clamp(g->paint_choice,0,5),unlocked=(g->save.paints&(1u<<paint))!=0;row(s,108,21,"Paint",paint_names[paint],g->cursor==2,!unlocked);gfx_rect(s,149,115,10,7,paint_colors[paint]);
  char stats[70]="TOP ";number(stats,game_vehicle_stat(g,id,STAT_TOP),sizeof stats);append(stats,"  ACCEL ",sizeof stats);number(stats,game_vehicle_stat(g,id,STAT_ACCEL),sizeof stats);append(stats,"  GRIP ",sizeof stats);number(stats,game_vehicle_stat(g,id,STAT_GRIP),sizeof stats);append(stats,"  ARMOR ",sizeof stats);number(stats,game_vehicle_stat(g,id,STAT_ARMOR),sizeof stats);limited(s,16,137,225,stats,t->ink);
  const char*detail=vehicles[id].description;const char*sub=owned?"Earn mastery XP by driving this vehicle.":vehicles[id].requirement;
  if(g->cursor==0){detail="Left / Right: browse the collection.";sub=owned?vehicles[id].description:vehicles[id].requirement;}
  if(g->cursor==2){detail=unlocked?"Left / Right previews a finish. A applies.":"This paint needs a driving challenge.";sub=paint==3?"Hold maximum speed for 8 seconds.":paint==4?"Earn 1000 wheelie points in one run.":paint==5?"Earn 1500 drift points in one run.":"Factory, Coral and Lagoon are always open.";}
  garage_detail(s,detail,sub);
 }else if(g->garage_tab==1){
  const uint8_t*levels[3]={g->save.engine,g->save.grip,g->save.armor};static const char*const names[3]={"Engine","Grip","Armor"};
  for(int i=0;i<3;i++){char label[30]="";append(label,names[i],sizeof label);append(label,"  ",sizeof label);number(label,levels[i][id],sizeof label);append(label,"/6",sizeof label);char val[28]="";int required=game_upgrade_rank(g,i),locked=rank<required;
   if(levels[i][id]>=UPGRADE_MAX)append(val,"MAX",sizeof val);else if(locked){append(val,"MASTERY ",sizeof val);number(val,required,sizeof val);}else money(val,game_upgrade_cost(g,i),sizeof val);
   row(s,84+i*23,20,label,val,g->cursor==i+1,!owned||locked);
  }
  int kind=clamp(g->cursor-1,0,2);char detail[80]="";if(g->cursor==0)append(detail,"Left / Right: browse your collection.",sizeof detail);else{
   Game preview=*g;uint8_t*level=kind==0?preview.save.engine:kind==1?preview.save.grip:preview.save.armor;if(level[id]<UPGRADE_MAX)level[id]++;
   if(kind==0){append(detail,"Top ",sizeof detail);number(detail,game_vehicle_stat(g,id,STAT_TOP),sizeof detail);append(detail," > ",sizeof detail);number(detail,game_vehicle_stat(&preview,id,STAT_TOP),sizeof detail);append(detail,"   Accel ",sizeof detail);number(detail,game_vehicle_stat(g,id,STAT_ACCEL),sizeof detail);append(detail," > ",sizeof detail);number(detail,game_vehicle_stat(&preview,id,STAT_ACCEL),sizeof detail);}
   else{append(detail,kind==1?"Grip ":"Armor ",sizeof detail);number(detail,game_vehicle_stat(g,id,kind==1?STAT_GRIP:STAT_ARMOR),sizeof detail);append(detail," > ",sizeof detail);number(detail,game_vehicle_stat(&preview,id,kind==1?STAT_GRIP:STAT_ARMOR),sizeof detail);}
  }
  garage_detail(s,detail,!owned?"Own this vehicle before installing parts.":"Parts remain installed when changing setup.");
 }else{
  static const char*const names[3]={"Power","Handling","Chassis"};const uint8_t*values[3]={g->save.power_tune,g->save.handling_tune,g->save.chassis_tune};
  for(int i=0;i<3;i++){int val=g->cursor==i+1?g->tune_choice:values[i][id];char text[44]="";if(rank<i+1){append(text,"M",sizeof text);number(text,i+1,sizeof text);append(text," / ",sizeof text);}append(text,game_tune_name(i,val),sizeof text);row(s,84+i*23,20,names[i],text,g->cursor==i+1,!owned||rank<i+1);}
  int kind=clamp(g->cursor-1,0,2),val=g->cursor?g->tune_choice:0;const char*desc=g->cursor?game_tune_description(kind,val):"Left / Right: browse the collection.";
  garage_detail(s,desc,!owned?"Own this vehicle before applying a setup.":rank<kind+1&&g->cursor?"Drive this vehicle to raise its mastery rank.":"Left / Right previews. A applies for free.");
 }
 footer(s,"L / R: Tabs","A: Apply   B: Back");
}
static void departures(Surface*s,const Game*g){
 header(s,"DEPARTURES",g);int id=clamp(g->cursor,0,LOCATION_COUNT-1),first=clamp(id-2,0,LOCATION_COUNT>6?LOCATION_COUNT-6:0);
 char page[18]="";number(page,id+1,sizeof page);append(page," / ",sizeof page);number(page,LOCATION_COUNT,sizeof page);gfx_text_small(s,104,10,page,t->muted);
 int visible=LOCATION_COUNT<6?LOCATION_COUNT:6;
 for(int r=0;r<visible;r++){int i=first+r;char value[24]="";int owned=(g->save.unlocked_locations&(1u<<i))!=0,available=game_can_location(g,i);if(owned)append(value,g->save.selected_location==i?"SELECTED":"OPEN",sizeof value);else if(available)money(value,locations[i].price,sizeof value);else append(value,"LOCKED",sizeof value);row(s,34+r*21,18,locations[i].name,value,id==i,!owned&&!available);}
 limited(s,15,164,227,!game_can_location(g,id)?locations[id].requirement:locations[id].description,t->muted);footer(s,"A: Set departure","B: Back");
}
static void challenges(Surface*s,const Game*g){
 header(s,"ROAD CHALLENGES",g);static const char*const units[5]={" near","m"," drift","s"," wheelie"};static const char*const rewards[5]={"Skywater Causeway","Pine Ridge departure","Sundown / Redrock / Graphite","Sunburst paint","Orchid paint"};
 for(int i=0;i<5;i++){int goal=1,progress=game_challenge_progress(g,i,&goal),done=progress>=goal;if(i==3){progress/=DRIVE_HZ;goal/=DRIVE_HZ;}int y=34+i*28;glass(s,11,y,234,25,g->cursor==i);uint16_t ink=g->cursor==i?t->selected_ink:t->ink,muted=g->cursor==i?t->selected_ink:t->muted;limited(s,18,y+2,126,game_challenge_name(i),ink);char val[32]="";number(val,progress,sizeof val);append(val,"/",sizeof val);number(val,goal,sizeof val);append(val,units[i],sizeof val);right(s,236,y+2,val,g->cursor==i?t->selected_detail:done?t->accent:t->coral);limited(s,18,y+14,180,rewards[i],muted);meter(s,202,y+17,34,progress,goal,g->cursor==i?t->selected_detail:t->accent,t->shadow);}
 footer(s,"Complete in one run","B: Back");
}
static void career(Surface*s,const Game*g){
 header(s,"CAREER",g);glass(s,11,34,234,33,0);char level[30]="LEVEL ";number(level,game_career_level(g),sizeof level);gfx_text(s,20,40,level,t->ink);char points[28]="";number(points,game_perk_available(g),sizeof points);append(points," POINTS",sizeof points);right(s,235,40,points,t->accent);uint32_t next=game_career_next(g);meter(s,20,56,117,(int)g->save.career_xp,next?(int)next:1,t->accent,t->shadow);char xp[28]="";number(xp,g->save.career_xp,sizeof xp);if(next){append(xp,"/",sizeof xp);number(xp,next,sizeof xp);}else append(xp," MAX",sizeof xp);right(s,235,51,xp,t->muted);
 static const char*const names[4]={"Recovery","Focus","Payout","Reset perks"};for(int i=0;i<4;i++){char value[24]="";if(i<3){number(value,g->save.perks[i],sizeof value);append(value," / 3",sizeof value);}else append(value,"FREE",sizeof value);row(s,74+i*22,19,names[i],value,g->cursor==i,0);}
 static const char*const desc[4]={"Once/run at low health: +10/18/26.","Reach 5x combos 15% sooner per rank.","Earn 8% more run cash per rank.","Return every spent career point, free."};limited(s,15,165,226,desc[clamp(g->cursor,0,3)],t->muted);footer(s,"A: Invest  |  L2: Mangroves","B: Back");
}
static void settings(Surface*s,const Game*g){
 header(s,"PERSONAL SETTINGS",g);static const char*const names[9]={"Music","Music volume","Sound effects","Effects volume","Interface","Road motion","Impact shake","Speed units","Controls"};static const char*const skins[3]={"Miami","Sunset","Midnight"};static const char*const motion[3]={"Calm","Cruise","Express"};
 for(int i=0;i<9;i++){char val[28]="";switch(i){case 0:append(val,g->save.music_on?"On":"Off",sizeof val);break;case 1:number(val,g->save.music_volume,sizeof val);append(val,"%",sizeof val);break;case 2:append(val,g->save.sfx_on?"On":"Off",sizeof val);break;case 3:number(val,g->save.sfx_volume,sizeof val);append(val,"%",sizeof val);break;case 4:append(val,skins[g->save.skin%SKIN_COUNT],sizeof val);break;case 5:append(val,motion[g->save.motion%3],sizeof val);break;case 6:append(val,g->save.shake?"On":"Off",sizeof val);break;case 7:append(val,g->save.units?"mph":"km/h",sizeof val);break;default:append(val,"Open",sizeof val);break;}row(s,34+i*15,14,names[i],val,g->cursor==i,0);}
 footer(s,"Left/Right: Adjust","A: Cycle   B: Back");
}
static void controls(Surface*s,const Game*g){
 header(s,g->controls_page?"TURBO GUIDE  2 / 2":"DRIVING CONTROLS  1 / 2",g);
 if(!g->controls_page){
  static const char*const keys[8]={"R","L","Left / Right","Up + R","Down + R","B","A","Start"};
  static const char*const actions[8]={"Accelerate","Brake / reverse","Steer","Turbo (uses charge)","Slow / recharge","Handbrake / drift","Horn / bike wheelie","Pause"};
  for(int i=0;i<8;i++){int y=35+i*17;glass(s,12,y,75,15,0);center(s,12,y+3,75,keys[i],t->accent);gfx_text(s,96,y+3,actions[i],t->ink);}
 }else{
  glass(s,11,34,234,36,0);gfx_text(s,19,39,"Hold Up + R to boost. Full = 8 sec.",t->ink);gfx_text_small(s,19,52,"At least 10% charge starts a new boost.",t->muted);gfx_text_small(s,19,62,"Empty? Release Up before boosting again.",t->muted);
  static const char*const labels[4]={"Near miss","Drift > 0.75 sec","Wheelie > 1 sec","Slow > 40 km/h (fixed)"};
  static const char*const rates[4]={"+10%","+4% / sec","+3% / sec","+2% / sec"};
  for(int i=0;i<4;i++){int y=75+i*20;glass(s,11,y,234,18,0);gfx_text_small(s,19,y+5,labels[i],t->ink);right(s,236,y+4,rates[i],t->accent);}
  gfx_text_small(s,16,158,"5x combo = 2x skill recharge.",t->ink);
  gfx_text_small(s,16,168,"Straight gas and coins do not recharge.",t->muted);
 }
 footer(s,"L / R: Change page","B: Back");
}
static void pause_menu(Surface*s,const Game*g){
 header(s,"TAKE A BREATHER",g);char info[64]="";number(info,g->run.distance_m,sizeof info);append(info," m  /  ",sizeof info);time_text(info,g->run_ticks,sizeof info);center(s,11,37,234,info,t->muted);static const char*const names[4]={"Resume driving","Bank this run","Controls","Settings"};static const char*const values[4]={"GO","CASH OUT","VIEW","ADJUST"};for(int i=0;i<4;i++)row(s,57+i*28,24,names[i],values[i],g->cursor==i,0);footer(s,"A: Choose","B / Start: Resume");
}

/* Critical information stays next to the top game screen. Each vehicle keeps
 * its own instruments in the lower cockpit, below the shared trip computer. */
enum Cluster { CL_CLASSIC,CL_DIGITAL,CL_UTILITY,CL_BIKE,CL_LUXURY,CL_RACE };
static int cluster_for(int v){return v==0||v==5?CL_CLASSIC:v==1||v==6?CL_DIGITAL:v==2||v==8?CL_UTILITY:v==3?CL_BIKE:v==7?CL_LUXURY:CL_RACE;}
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
 int speed,accel,ratio,reverse,health,combo,location,charge,turbo,latched,recharging,source,driving,impact;
 uint32_t points,cash,distance,coins;
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
 for(int i=0;i<=30;i++){int major=i%5==0,outer=r-7,inner=outer-(major?5:2);gfx_line(s,x+needle_xy[i][0]*inner/1024,y+needle_xy[i][1]*inner/1024,x+needle_xy[i][0]*outer/1024,y+needle_xy[i][1]*outer/1024,i>25&&!accel?WARN:major?ink:muted);}
 int off=r-17;
 if(accel){gfx_text_small(s,x-off-9,y+10,"-40",muted);gfx_text_small(s,x-3,y-r+10,"0",muted);gfx_text_small(s,x+off-9,y+10,"+40",muted);}
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
 int ratio=native*100/(game_top_speed(g)>0?game_top_speed(g):1),combo_tenth=g->combo_q8*10/Q;
 int location=g->biome<LOCATION_COUNT?g->biome:0;
 DashboardReadout now={speed,accel,ratio,g->speed_q8<0,g->health,combo_tenth,location,
  g->turbo_charge_q8,g->turbo,g->turbo_latched,g->turbo_recharging,g->turbo_source,
  g->wheelie?1:g->drifting?2:g->slow?3:g->turbo?4:0,g->crash_timer!=0,
  g->run.total_points,g->run.total_cash,g->run.distance_m,g->run.count[B_COIN]};
 const DashboardReadout *old=&dashboard_previous;
 int refresh=dashboard_target!=s->pixels || dashboard_target_key!=key;
 int header_dirty=refresh || old->location!=now.location || old->impact!=now.impact;
 int instruments_dirty=refresh || old->speed!=speed || old->accel!=accel || old->ratio!=ratio ||
  old->reverse!=now.reverse || (family==CL_UTILITY && old->health!=now.health);
 int body_dirty=refresh || old->health!=now.health,combo_dirty=refresh || old->combo!=combo_tenth;
 int score_dirty=refresh || old->points!=now.points || old->cash!=now.cash;
 int turbo_dirty=refresh || old->charge!=now.charge || old->turbo!=now.turbo || old->latched!=now.latched ||
  old->recharging!=now.recharging || old->source!=now.source;
 int trip_dirty=refresh || old->distance!=now.distance || old->coins!=now.coins || old->driving!=now.driving;
 /* The caller retains this RAM bitmap between updates. Restore only fields
  * whose displayed values changed; menus and new targets force a full fill. */
 if(refresh)gfx_copy(s->pixels,dashboard_art,256*192);
 if(header_dirty){
  if(!refresh)dashboard_restore(s,0,0,256,16);
  if(location_key!=key || location_value!=location){
   field_background(location_art,130,3,113,14);Surface label={location_art,113,14};
   limited(&label,0,0,113,locations[location].name,t->light);location_key=key;location_value=location;
  }
  field_copy(s,location_art,130,3,113,14);
  if(g->crash_timer){gfx_card(s,190,1,57,13,C(81,39,44),WARN);center(s,190,3,57,"IMPACT",LIGHT);}
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
 if(combo_key!=key || combo_value!=combo_tenth){
  field_background(combo_art,84,16,68,42);Surface readout={combo_art,68,42};
  char combo[16]="x";number(combo,combo_tenth/10,sizeof combo);append(combo,".",sizeof combo);number(combo,combo_tenth%10,sizeof combo);
  large_text(&readout,6,13,combo,t->accent);combo_key=key;combo_value=combo_tenth;
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
 uint16_t paper=C(246,247,231),ink=C(33,73,80),muted=C(93,127,127);gradient(s,0,0,256,192,t->sky,t->shore);gfx_rect(s,12,7,236,170,t->shadow);gfx_rect(s,8,3,236,170,paper);for(int x=8;x<244;x+=8){gfx_rect(s,x,173,7,1,paper);gfx_rect(s,x+1,174,5,1,paper);gfx_rect(s,x+2,175,3,1,paper);gfx_rect(s,x+3,176,1,1,paper);}
 center(s,8,7,236,"C O A S T L I N E",ink);center(s,8,19,236,"DRIVE CLUB / RUN STATEMENT",muted);char info[70]="";number(info,g->run.distance_m,sizeof info);append(info," m  |  ",sizeof info);time_text(info,g->run_ticks,sizeof info);append(info,"  |  ",sizeof info);number(info,display_speed(g->run.max_speed,g->save.units),sizeof info);append(info,g->save.units?" mph":" km/h",sizeof info);center(s,8,31,236,info,ink);
 gfx_rect(s,17,45,218,1,C(171,199,192));gfx_text_small(s,18,47,"BONUS",muted);right(s,163,46,"QTY",muted);right(s,234,46,"POINTS",muted);
 static const char*const names[BONUS_COUNT]={"Distance","Survival","Near miss","Top speed","Drift","Wheelie","Coins"};for(int i=0;i<BONUS_COUNT;i++){int y=58+i*11;gfx_text(s,18,y,names[i],ink);char qty[20]="";number(qty,g->run.count[i],sizeof qty);append(qty,i==B_DISTANCE?"m":i==B_SURVIVAL||i==B_SPEED||i==B_DRIFT||i==B_WHEELIE?"s":"x",sizeof qty);right(s,163,y,qty,muted);char pts[24]="";number(pts,g->run.points[i],sizeof pts);right(s,234,y,pts,ink);}
 gfx_rect(s,17,137,218,1,C(124,165,160));gfx_text(s,18,141,"TOTAL POINTS",ink);char total[28]="";number(total,g->run.total_points,sizeof total);right(s,234,141,total,ink);char label[50]="CASH";if(g->run.cash_bonus){append(label," (+",sizeof label);money(label,g->run.cash_bonus,sizeof label);append(label," perk)",sizeof label);}else append(label," EARNED",sizeof label);gfx_text(s,18,153,label,ink);char cash[24]="";money(cash,g->run.total_cash,sizeof cash);right(s,234,153,cash,ink);char xp[70]="MASTERY +";number(xp,g->run.mastery_xp,sizeof xp);append(xp,"  |  CAREER +",sizeof xp);number(xp,g->run.career_xp,sizeof xp);center_small(s,8,164,236,xp,muted);center(s,0,181,256,"A: Continue",t->ink);
}
void ui_draw_bottom(Surface*s,const Game*g){
 t=&themes[g->save.skin%SKIN_COUNT];if(g->mode==M_RUN){dashboard(s,g);return;}
 dashboard_target=0;
 if(g->mode==M_RECEIPT){receipt(s,g);return;}
 backdrop(s,g->save.skin%SKIN_COUNT);
 switch(g->mode){case M_HOME:home(s,g);break;case M_GARAGE:garage(s,g);break;case M_DEPARTURES:departures(s,g);break;case M_CHALLENGES:challenges(s,g);break;case M_CONTROLS:controls(s,g);break;case M_PAUSE:pause_menu(s,g);break;case M_CAREER:career(s,g);break;case M_SETTINGS:settings(s,g);break;default:home(s,g);break;}
 if(g->notice_timer&&g->notice[0]){glass(s,10,155,236,21,1);limited(s,18,160,220,g->notice,t->selected_ink);}
}
