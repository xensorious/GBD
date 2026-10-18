/* Presentation only.  All choices retain their original input mapping. */
enum {
 UI_INK=56, UI_BLUE=57, UI_TEAL=58, UI_MINT=59, UI_PAPER=60,
 UI_WHITE=61, UI_GOLD=62, UI_BRONZE=63, UI_ICE=64, UI_MUTED=65,
 UI_LINE=66, UI_RED=67, UI_SHADE=68, UI_SEA=69, UI_SKY=70,
 UI_NIGHT=71, UI_SHADOW=72, UI_PINK=73
};

static void ui_base(void){
 rect(0,0,240,160,UI_SHADE);
 rect(3,25,234,120,UI_PAPER);
 rect(3,25,234,1,UI_LINE);
 rect(3,144,234,1,UI_LINE);
}
static void ui_header(const char *title,const char *tag){
 rect(0,0,240,23,UI_INK);rect(0,21,240,2,UI_TEAL);
 rect(7,7,2,8,UI_GOLD);text(14,8,title,UI_WHITE);
 if(tag)text(230-length(tag)*6,8,tag,UI_MINT);
}
static void ui_footer(const char *s){
 rect(0,148,240,12,UI_INK);rect(0,148,240,1,UI_TEAL);center(151,s,UI_PAPER);
}
static void ui_card(int x,int y,int w,int h,int selected){
 rect(x+2,y+2,w,h,UI_SHADOW);
 rect(x+1,y,w-2,h,UI_INK);rect(x,y+1,w,h-2,UI_INK);
 rect(x+2,y+2,w-4,h-4,UI_PAPER);
 rect(x+2,y+2,w-4,2,selected?UI_GOLD:UI_LINE);
 if(selected)rect(x+2,y+4,2,h-6,UI_TEAL);
}
static void ui_selected(int x,int y,int w,int h,int selected){
 rect(x,y,w,h,selected?UI_SEA:UI_ICE);
 if(selected){rect(x,y,2,h,UI_GOLD);rect(x+2,y,w-2,1,UI_TEAL);}
 else rect(x,y+h-1,w,1,UI_LINE);
}
static void ui_right(int x,int y,const char *s,int c){text(x-length(s)*6,y,s,c);}
static void ui_value(int x,int y,int a,int b,int c){
 char s[20];number(s,a);append(s,"/");addnum(s,b);text(x,y,s,c);
}
static void ui_number_right(int x,int y,int n,int c){char s[12];number(s,n);ui_right(x,y,s,c);}
static void ui_pill(int x,int y,const char *s,int color){
 int w=length(s)*6+10;
 rect(x+1,y,w-2,11,color);rect(x,y+1,w,9,color);text(x+5,y+2,s,UI_WHITE);
}
static void ui_meter(int x,int y,int w,int amount,int total){
 int n=(w-2)*CLAMP(amount,0,MAX(1,total))/MAX(1,total);
 int c=amount*3<=total?UI_RED:amount*2<=total?UI_GOLD:UI_TEAL;
 rect(x,y,w,5,UI_INK);rect(x+1,y+1,w-2,3,UI_SHADE);
 if(n){rect(x+1,y+1,n,3,c);rect(x+1,y+1,n,1,c==UI_TEAL?UI_MINT:UI_PAPER);}
}
static void ui_energy(int x,int y,int amount,int total){
 for(int i=0;i<total;i++){
  rect(x+i*7,y,5,4,UI_INK);
  rect(x+i*7+1,y+1,3,2,i<amount?UI_GOLD:UI_LINE);
 }
}
static void ui_miniature(int x,int y,int s){
 const u8 *p=creature_art+s*1024;
 for(int j=0;j<16;j++)for(int i=0;i<16;i++){
  int c=p[(j*2)*32+i*2];
  if(c&&x+i>=0&&x+i<240&&y+j>=0&&y+j<160)fb[(y+j)*240+x+i]=c;
 }
}
static void ui_icon(int x,int y,int kind,int c){
 if(kind==0){
  rect(x,y,2,3,c);rect(x+3,y-1,2,3,c);rect(x+6,y,2,3,c);
  rect(x+2,y+4,5,4,c);rect(x+1,y+5,7,2,c);
 }else if(kind==1){
  rect(x,y,8,8,c);rect(x+3,y+1,1,6,UI_PAPER);
  rect(x+1,y+2,1,1,UI_PAPER);rect(x+5,y+2,2,1,UI_PAPER);
 }else if(kind==2){
  rect(x+1,y,7,8,c);rect(x,y+1,2,2,c);rect(x,y+5,2,2,c);
  rect(x+3,y+2,3,1,UI_PAPER);rect(x+3,y+5,3,1,UI_PAPER);
 }else if(kind==3){
  rect(x,y,8,8,c);rect(x+2,y,4,3,UI_PAPER);rect(x+2,y+5,4,3,UI_PAPER);
 }else if(kind==4){
  rect(x,y+3,2,3,c);rect(x+2,y+2,2,5,c);rect(x+4,y+1,2,7,c);
  rect(x+7,y+2,1,5,c);
 }else{
  rect(x,y+3,8,2,c);rect(x+4,y+1,2,6,c);rect(x+2,y+2,2,4,c);
 }
}
static void ui_check(int x,int y,int done){
 rect(x,y,8,8,done?UI_TEAL:UI_LINE);rect(x+1,y+1,6,6,done?UI_TEAL:UI_PAPER);
 if(done){rect(x+1,y+3,2,2,UI_WHITE);rect(x+3,y+4,2,2,UI_WHITE);rect(x+5,y+1,2,4,UI_WHITE);}
}
static void ui_bigtext(int x,int y,const char *s,int c){
 while(*s){unsigned ch=(unsigned char)*s++;
  if(ch>='a'&&ch<='z')ch-=32;
  if(ch>=32&&ch<128)for(int j=0;j<7;j++)for(int i=0;i<5;i++)
   if(font_art[(ch-32)*7+j]&(16>>i))rect(x+i*2,y+j*2,2,2,c);
  x+=12;
 }
}

static void drawdialog(void){
 ui_card(4,102,232,55,1);
 int w=MIN(214,length(speaker)*6+14);
 rect(10,106,w,12,UI_INK);rect(10,106,2,12,UI_TEAL);
 text(17,109,speaker,UI_WHITE);
 wrap(12,122,dialog+dialogPos,35,3,UI_INK);
 if(frame/20%2){rect(224,148,7,6,UI_TEAL);text(225,148,">",UI_WHITE);}
}

static void drawtitle(void){
 memcpy(fb,title_art,sizeof(fb));
 rect(27,14,188,47,UI_SHADOW);rect(25,12,190,47,UI_INK);
 rect(29,16,182,1,UI_GOLD);rect(29,55,182,1,UI_TEAL);
 ui_bigtext(49,23,"BRAMBLEBOUND",UI_SHADOW);
 ui_bigtext(48,21,"BRAMBLEBOUND",UI_PAPER);
 center(43,"F I R S T  L I G H T",UI_GOLD);
 creature(24,66,0,1,0);creature(104,62,2,1,0);creature(184,66,1,1,1);
 ui_card(55,98,130,47,1);
 if(hasSave){
  for(int i=0;i<3;i++){
   int y=103+i*13;ui_selected(61,y,117,12,cursor==i);
   text(73,y+3,i==0?"CONTINUE":i==1?"NEW JOURNEY":"CONTROLS",cursor==i?UI_WHITE:UI_INK);
   if(cursor==i)text(64,y+3,">",UI_GOLD);
  }
 }else{
  for(int i=0;i<2;i++){
   int y=106+i*19;ui_selected(61,y,117,15,cursor==i);
   text(73,y+4,i==0?"NEW JOURNEY":"CONTROLS",cursor==i?UI_WHITE:UI_INK);
   if(cursor==i)text(64,y+4,">",UI_GOLD);
  }
 }
 ui_footer("D-PAD CHOOSE     A CONFIRM");
}

static void drawchoose(void){
 ui_base();ui_header("CHOOSE YOUR COMPANION",0);
 for(int i=0;i<3;i++){
  int x=5+i*78;ui_card(x,32,74,85,cursor==i);
  ellipse(x+13,64,49,10,UI_SHADE);
  creature(x+20,42,i,1,0);
  text(x+(74-length(species[i].name)*6)/2,82,species[i].name,UI_INK);
  int w=length(types[species[i].type])*6+10;
  ui_pill(x+(74-w)/2,96,types[species[i].type],cursor==i?UI_TEAL:UI_MUTED);
  if(cursor==i){rect(x+30,111,14,2,UI_GOLD);rect(x+34,110,6,1,UI_GOLD);}
 }
 const char *h[]={"Steady and gentle. SETTLE restores extra health.","A bold little spark. Strong attacks, lighter defenses.","A patient river friend. SETTLE restores extra energy."};
 wrap(10,123,h[cursor],37,2,UI_INK);
 ui_footer("LEFT/RIGHT CHOOSE   A PARTNER");
}

static void drawmenu(void){
 drawfield();
 ui_card(78,23,156,120,1);
 rect(83,28,146,15,UI_INK);text(91,32,"TRAIL JOURNAL",UI_WHITE);
 for(int i=0;i<6;i++){
  int y=46+i*15;ui_selected(84,y,144,14,cursor==i);
  ui_icon(91,y+3,i,cursor==i?UI_GOLD:UI_TEAL);
  text(106,y+4,menuItems[i],cursor==i?UI_WHITE:UI_INK);
  if(i==4)ui_right(221,y+4,g.mute?"OFF":"ON",cursor==i?UI_MINT:UI_MUTED);
 }
 ui_footer("A OPEN   B/START BACK");
}

static void drawteam(void){
 ui_base();ui_header("YOUR COMPANIONS",0);
 for(int i=0;i<g.count;i++){
  Friend *f=&g.friends[i];int y=28+i*19;
  ui_selected(5,y,104,18,cursor==i);
  ui_miniature(7,y+1,f->species);
  text(27,y+2,species[f->species].name,cursor==i?UI_WHITE:UI_INK);
  if(i==g.lead){rect(100,y+3,4,4,UI_GOLD);rect(101,y+2,2,6,UI_GOLD);}
  if(f->stationed)text(27,y+11,"IN TOWN",cursor==i?UI_MINT:UI_MUTED);
  else{
   text(27,y+11,"LV",cursor==i?UI_MINT:UI_MUTED);smallnum(41,y+11,f->level,cursor==i?UI_MINT:UI_MUTED);
   ui_meter(64,y+12,40,f->hp,maxhp(f));
  }
 }
 Friend *f=&g.friends[cursor];
 ui_card(115,28,120,115,0);
 ellipse(150,60,48,8,UI_SHADE);creature(157,34,f->species,1,0);
 ui_pill(123,72,types[species[f->species].type],UI_TEAL);
 text(123,88,"HP",UI_INK);ui_value(166,88,f->hp,maxhp(f),UI_INK);
 ui_meter(123,98,103,f->hp,maxhp(f));
 char s[32];copy(s,"EP ");addnum(s,f->energy);append(s,"/");addnum(s,maxenergy(f));append(s," BOND ");addnum(s,f->bond);
 text(123,108,s,UI_MUTED);
 text(123,120,f->path==1?"STRIDER":f->path==2?"WARDEN":"PATH AT LV3",UI_TEAL);
 text(123,131,species[f->species].trait,UI_MUTED);
 ui_footer("A LEAD   SELECT PATH   B BACK");
}

static void drawgrowth(void){
 Friend *f=&g.friends[growIndex];ui_base();ui_header("GROWING TOGETHER",0);
 ellipse(98,57,44,9,UI_SHADE);creature(104,28,f->species,1,0);
 center(67,species[f->species].name,UI_INK);
 ui_card(5,81,112,60,cursor==0);ui_card(123,81,112,60,cursor==1);
 text(17,90,"STRIDER",UI_INK);text(135,90,"WARDEN",UI_INK);
 rect(101,90,7,3,UI_TEAL);rect(104,87,3,9,UI_TEAL);
 rect(218,88,7,7,UI_TEAL);rect(220,95,3,2,UI_TEAL);
 text(13,104,"+2 ATK +1 ENERGY",UI_MUTED);text(131,104,"+6 HP +2 DEF",UI_MUTED);
 text(13,119,"BURST: BIG HIT",UI_TEAL);text(131,116,"SHELTER: HEAL",UI_TEAL);text(131,128,"+ SOFTEN A HIT",UI_TEAL);
 ui_footer("A CHOOSE  B LATER  FREE AT CAMP");
}

static void drawguide(void){
 ui_base();ui_header("FIELD GUIDE",menuPage?"2/2":"1/2");
 if(menuPage==0){
  for(int i=0;i<6;i++){
   int y=28+i*19;ui_selected(5,y,95,18,cursor==i);
   if(g.seen&(1<<i))ui_miniature(7,y+1,i);
   else text(12,y+6,"?",cursor==i?UI_MINT:UI_MUTED);
   text(28,y+6,(g.seen&(1<<i))?species[i].name:"????",cursor==i?UI_WHITE:UI_INK);
  }
  ui_card(106,28,129,115,0);
  if(g.seen&(1<<cursor)){
   ellipse(147,60,48,8,UI_SHADE);creature(155,34,cursor,1,0);
   ui_pill(115,73,types[species[cursor].type],UI_TEAL);
   text(115,90,species[cursor].trait,UI_INK);
   wrap(115,104,species[cursor].hint,18,4,UI_MUTED);
  }else{
   ellipse(153,41,37,31,UI_ICE);text(169,53,"?",UI_TEAL);
   wrap(115,86,"An unknown neighbor. Explore Fernway to meet them.",18,5,UI_MUTED);
  }
 }else{
  ui_card(5,29,230,114,0);
  text(13,38,"LEAF > TIDE > EMBER > LEAF",UI_TEAL);
  text(13,52,"STONE resists EMBER; LEAF and TIDE",UI_INK);
  text(13,62,"crack its shell.",UI_INK);
  rect(13,74,213,1,UI_LINE);
  text(13,82,"NUDGE saves energy. SETTLE restores",UI_INK);
  text(13,92,"it. Read the next move first.",UI_INK);
  rect(13,104,213,1,UI_LINE);
  text(13,112,"BEFRIEND: half HP or after SOOTHE.",UI_INK);
  text(13,123,"One treat. No luck roll.",UI_TEAL);
 }
 ui_footer("L/R PAGE  UP/DOWN SELECT  B BACK");
}

static void drawjournal(void){
 ui_base();ui_header("TRAIL JOURNAL","FIRST LIGHT");
 ui_card(5,29,230,28,1);text(13,35,"NEXT STEP",UI_TEAL);text(13,46,objective(),UI_INK);
 const char *tasks[]={"CHOOSE A PARTNER","MEET TAVI","BEFRIEND A WILD CREATURE","CHOOSE A GROWTH PATH","EARN IRIS'S TRAIL PIN","OPTIONAL: HELP NELL'S GARDEN"};
 int done[6]={g.count>0,g.scout,g.count>=2,g.count&&g.friends[0].path,g.rival,g.job};
 for(int i=0;i<6;i++){
  int y=65+i*12;ui_check(12,y-1,done[i]);text(27,y,tasks[i],done[i]?UI_TEAL:UI_INK);
 }
 text(12,138,"TREATS",UI_MUTED);smallnum(58,138,g.treats,UI_INK);
 text(123,138,"TONICS",UI_MUTED);smallnum(169,138,g.tonics,UI_INK);
 ui_footer("REST IS FREE. YOUR FRIENDS ARE SAFE.");
}

static const char *movename(Friend *f,int m){
 return m==0?"NUDGE":m==1?species[f->species].move:m==2?"SETTLE":f->path==1?"BURST":f->path==2?"SHELTER":"SOOTHE";
}
static void ui_battlecard(int x,int y,int selected){
 ui_selected(x,y,112,19,selected);
 rect(x,y+18,112,1,UI_INK);
}
static void drawbattle(void){
 memcpy(fb,battle_art,sizeof(fb));
 int f=MIN(battle.foe,battle.count-1);
 Friend *p=&g.friends[battle.active],*e=&battle.foes[f];
 int shake=battle.shake?(battle.shake&2?2:-2):0;
 ellipse(155,56,76,13,UI_SHADE);ellipse(17,90,79,11,UI_SHADE);
 creature(163+shake,0,e->species,2,0);
 /* A common foot line keeps every back sprite clear of the status panels. */
 art(18-shake,p->species==4?37:39,creature_back_art+p->species*1024,32,32,2,0);

 ui_card(4,3,139,38,0);
 text(11,9,species[e->species].name,UI_INK);text(111,9,"L",UI_MUTED);smallnum(119,9,e->level,UI_MUTED);
 text(11,22,"HP",UI_MUTED);ui_meter(29,23,64,e->hp,maxhp(e));ui_value(99,22,e->hp,maxhp(e),UI_INK);
 text(11,32,"EP",UI_MUTED);smallnum(29,32,e->energy,UI_TEAL);
 if(battle.trainer){char s[20];copy(s,"IRIS ");addnum(s,f+1);append(s,"/");addnum(s,battle.count);ui_right(135,32,s,UI_TEAL);}
 else ui_right(135,32,battle.calm?"WILD / CALM":"WILD",UI_TEAL);

 ui_card(106,64,129,37,0);
 text(113,70,species[p->species].name,UI_INK);text(207,70,"L",UI_MUTED);smallnum(215,70,p->level,UI_MUTED);
 text(113,82,"HP",UI_MUTED);ui_meter(131,83,48,p->hp,maxhp(p));ui_value(185,82,p->hp,maxhp(p),UI_INK);
 text(113,92,"EP",UI_MUTED);ui_energy(131,93,p->energy,maxenergy(p));ui_value(194,92,p->energy,maxenergy(p),UI_MUTED);

 rect(0,102,240,10,UI_INK);text(6,104,"NEXT:",UI_MINT);
 text(43,104,battle.turn%3==1&&e->energy>=2?species[e->species].move:"NUDGE",UI_WHITE);
 int ready=!battle.trainer&&!owned(e->species)&&g.count<6&&g.treats&&(battle.calm||e->hp<=maxhp(e)/2);
 if(ready)ui_right(233,104,"TREAT READY",UI_GOLD);
 rect(0,112,240,48,UI_SHADE);

 if(battle.logPos<battle.logCount){
  ui_card(4,114,232,43,1);wrap(12,120,battle.logs[battle.logPos],35,3,UI_INK);
  if(frame/20%2){rect(224,148,7,6,UI_TEAL);text(225,148,">",UI_WHITE);}return;
 }
 if(battle.menu==2){
  for(int i=0;i<g.count;i++){
   int x=i<3?5:123,y=114+(i%3)*15;
   ui_selected(x,y,112,14,battle.cursor==i);
   int color=battle.cursor==i?UI_WHITE:UI_INK;
   text(x+8,y+4,species[g.friends[i].species].name,available(i)?color:UI_MUTED);
   char s[20];number(s,g.friends[i].hp);append(s,"/");addnum(s,maxhp(&g.friends[i]));
   ui_right(x+108,y+4,s,available(i)?color:UI_MUTED);
  }return;
 }
 if(battle.menu==3){
  ui_selected(5,115,230,19,battle.cursor==0);ui_selected(5,137,230,19,battle.cursor==1);
  text(14,121,"TONIC",battle.cursor==0?UI_WHITE:UI_INK);
  text(66,121,"RESTORE 18 HP",battle.cursor==0?UI_MINT:UI_MUTED);
  ui_number_right(226,121,g.tonics,battle.cursor==0?UI_GOLD:UI_TEAL);
  text(14,143,battle.trainer?"DUEL: NO RUNNING":"LEAVE SAFELY",battle.cursor==1?UI_WHITE:UI_INK);return;
 }
 for(int i=0;i<4;i++){
  int x=5+(i%2)*118,y=115+(i/2)*22,selected=battle.cursor==i;
  ui_battlecard(x,y,selected);
  int color=selected?UI_WHITE:UI_INK;
  if(battle.menu==0){
   const char *label=i==0?"MOVES":i==1?"BEFRIEND":i==2?"SWAP":"PACK";
   ui_icon(x+8,y+6,i==0?0:i==1?0:i==2?5:2,selected?UI_GOLD:UI_TEAL);
   text(x+23,y+6,label,color);
   if(i==1)ui_number_right(x+105,y+6,g.treats,selected?UI_GOLD:UI_TEAL);
   if(i==3)ui_number_right(x+105,y+6,g.tonics,selected?UI_GOLD:UI_TEAL);
  }else{
   text(x+8,y+6,movename(p,i),color);
   int cost=i==1?2:i==3?(p->path==1?3:p->path==2?2:1):0;
   char s[12];number(s,cost);append(s,"E");ui_right(x+105,y+6,s,cost>p->energy?UI_RED:selected?UI_GOLD:UI_TEAL);
  }
 }
}

static void drawending(void){
 rect(0,0,240,160,UI_NIGHT);
 for(int i=0;i<12;i++){
  int x=(i*43+11)%236,y=(i*29+7)%144;
  rect(x,y,2,2,i%2?UI_MINT:UI_GOLD);
 }
 ui_card(12,12,216,132,1);center(22,"YOUR FIRST TRAIL PIN",UI_INK);
 ellipse(99,39,43,43,UI_BRONZE);ellipse(100,37,40,40,UI_GOLD);ellipse(105,42,30,30,UI_PAPER);
 rect(118,46,4,23,UI_TEAL);rect(111,51,17,5,UI_TEAL);
 rect(109,49,5,5,UI_MINT);rect(123,46,5,4,UI_MINT);rect(120,65,4,4,UI_TEAL);
 center(85,"IRIS: THAT WAS A REAL TEAM.",UI_TEAL);center(98,"YOU FOUND YOUR OWN WAY HERE.",UI_INK);
 text(26,115,"FRIENDS MET",UI_MUTED);ui_number_right(214,115,g.count,UI_TEAL);
 text(26,129,"GARDEN HELPED",UI_MUTED);ui_right(214,129,g.job?"YES":"NOT YET",UI_TEAL);
 ui_footer("A KEEP EXPLORING  /  SAVED");
}
