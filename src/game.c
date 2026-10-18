/* Bramblebound: an original, freestanding Game Boy Advance game. */
#include <stdint.h>
#include <stddef.h>
#include "assets.h"
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32;
#define REG16(a) (*(volatile u16 *)(a))
#define REG32(a) (*(volatile u32 *)(a))
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))
#define CLAMP(x,a,b) MIN(MAX(x,a),b)
enum { A=1,B=2,SELECT=4,START=8,RIGHT=16,LEFT=32,UP=64,DOWN=128,R=256,L=512 };
enum { TITLE,FIELD,DIALOG,CHOOSE,MENU,TEAM,GUIDE,JOURNAL,BATTLE,GROWTH,ENDING,CONFIRM };
enum { FIELD_BACK,TO_CHOOSE,TO_FIELD,TO_GROW,TO_BATTLE };
enum { LEAF,EMBER,TIDE,STONE,GLOW };
typedef struct { u8 species,level,path,stationed,hp,energy,xp,bond; } Friend;
typedef struct {
    u32 magic,serial;
    u8 version,map,x,y,dir,count,lead,starter,treats,tonics,scout,job,rival,seen,pickups,mute;
    Friend friends[6];
    u32 steps,checksum;
} Save;
typedef struct {const char *name,*move,*trait,*hint; u8 type,hp,attack,defense,color;} Species;
static const Species species[6]={
 {"MOSSPRIG","LEAF DART","ROOTED","SETTLE restores extra health.",LEAF,25,7,6,30},
 {"CINDLET","EMBER","SPARK","Full energy boosts its strikes.",EMBER,23,9,5,20},
 {"RILLIP","BUBBLE","FLOW","SETTLE restores extra energy.",TIDE,27,6,6,25},
 {"PEBBLIT","STONE ROLL","SHELL","A sturdy, naturally tough friend.",STONE,29,6,9,34},
 {"LUMA","GLIMMER","SOFTLIGHT","GLIMMER also calms wild foes.",GLOW,23,7,5,37},
 {"BURRBIT","BURR SHOT","THORNS","A hit on it stings for 1 HP.",LEAF,25,8,6,29}
};
static const char *types[]={"LEAF","EMBER","TIDE","STONE","GLOW"};
static const char *places[]={"MOSSBELL COMMONS","FERNWAY TRAIL"};
static const char saveType[] __attribute__((used,section(".rodata.save_type")))="SRAM_V113";
Save g;
volatile int screen=TITLE;
u8 world[2][32*24];
static u8 fb[240*160] __attribute__((aligned(4)));
static u32 frame;
static u16 keys,pressed,previous;
static int cursor,hasSave,confirmCursor;
static int camx,camy,moveDelay,walkFrame,followx,followy,growIndex;
static int visualx,visualy,visualfx,visualfy;
static int musicTick,musicNote,musicArea=-1,sfxFrames;
static int menuPage;
static char dialog[1800],speaker[40];
static int dialogPos,dialogAfter,dialogReturn;
static const char *menuItems[]={"COMPANIONS","FIELD GUIDE","TRAIL JOURNAL","SAVE JOURNEY","SOUND","BACK"};
static const u16 notes[]={1046,1175,1319,1568,1397,1319,1175,988,1046,1319,1568,1760,1568,1319,1175,988};
typedef struct {u8 x,y,species,level,cooldown;} Encounter;
Encounter encounters[6]={{7,17,3,2,0},{10,14,5,3,0},{11,7,4,3,0},{19,10,3,3,0},{24,13,5,4,0},{27,8,4,4,0}};
typedef struct {
 Friend foes[2];
 int trainer,foe,count,active,menu,cursor,turn,calm,guard,encounter,shake,result;
 int logCount,logPos;
 char logs[12][112];
} Battle;
Battle battle;

void *memcpy(void *d,const void *s,size_t n){u8 *a=d;const u8*b=s;if(!(((u32)a|(u32)b)&3)){while(n>=4){*(u32*)a=*(const u32*)b;a+=4;b+=4;n-=4;}}while(n--)*a++=*b++;return d;}
void *memset(void *d,int v,size_t n){u8*a=d;while(n--)*a++=v;return d;}
void __aeabi_memcpy(void*d,const void*s,size_t n){memcpy(d,s,n);}
void __aeabi_memcpy4(void*d,const void*s,size_t n){memcpy(d,s,n);}
void __aeabi_memclr(void*d,size_t n){memset(d,0,n);}
void __aeabi_memclr4(void*d,size_t n){memset(d,0,n);}
static int length(const char*s){int n=0;while(s[n])n++;return n;}
static void copy(char*d,const char*s){while((*d++=*s++));}
static void append(char*d,const char*s){copy(d+length(d),s);}
static void number(char*d,int v){char b[12];int n=0;if(v==0){d[0]='0';d[1]=0;return;}if(v<0){*d++='-';v=-v;}while(v){b[n++]='0'+v%10;v/=10;}while(n)*d++=b[--n];*d=0;}
static void addnum(char*d,int n){number(d+length(d),n);}
static int absval(int n){return n<0?-n:n;}
static int near(int x,int y){return absval((int)g.x-x)+absval((int)g.y-y)<=1;}
static int maxhp(const Friend*f){return species[f->species].hp+(f->level-1)*3+(f->path==2?6:0);}
static int maxenergy(const Friend*f){return 6+(f->path==1?1:0);}
static int power(const Friend*f){return species[f->species].attack+f->level+(f->path==1?2:0);}
static int defense(const Friend*f){return species[f->species].defense+f->level+(f->path==2?2:0);}
static void heal(Friend*f){f->hp=maxhp(f);f->energy=maxenergy(f);}
static void healall(void){for(int i=0;i<g.count;i++)heal(&g.friends[i]);}
static Friend makefriend(int s,int lv){Friend f={(u8)s,(u8)lv,0,0,0,0,0,1};heal(&f);return f;}
static int owned(int s){for(int i=0;i<g.count;i++)if(g.friends[i].species==s)return 1;return 0;}
static int available(int i){return i<g.count&&!g.friends[i].stationed&&g.friends[i].hp;}
static int firstalive(void){for(int i=0;i<g.count;i++)if(available(i))return i;return -1;}
static int fieldcamp(void){return g.map==0||near(7,12);}

static void rect(int x,int y,int w,int h,int c){
 int x1=MAX(0,x),y1=MAX(0,y),x2=MIN(240,x+w),y2=MIN(160,y+h);
 if(x2<=x1||y2<=y1)return;
 u32 pattern=(u8)c*0x01010101u;
 for(int j=y1;j<y2;j++){u8*p=fb+j*240+x1;int n=x2-x1;if(n<=0)continue;while(((u32)p&3)&&n){*p++=c;n--;}while(n>=4){*(u32*)p=pattern;p+=4;n-=4;}while(n--)*p++=c;}
}
static void panel(int x,int y,int w,int h,int accent){
 rect(x+1,y+2,w,h,1);rect(x,y,w,h,2);rect(x,y,w,1,1);rect(x,y+h-1,w,1,1);rect(x,y,1,h,1);rect(x+w-1,y,1,h,1);rect(x+2,y+2,w-4,2,accent);
}
static void text(int x,int y,const char*s,int color){
 while(*s){unsigned ch=(unsigned char)*s++;if(ch>='a'&&ch<='z')ch-=32;
  if(ch>=32&&ch<128)for(int j=0;j<7;j++){int bits=font_art[(ch-32)*7+j];for(int i=0;i<5;i++)if(bits&(16>>i))if(x+i>=0&&x+i<240&&y+j>=0&&y+j<160)fb[(y+j)*240+x+i]=color;}
  x+=6;
 }
}
static void smallnum(int x,int y,int v,int c){char b[12];number(b,v);text(x,y,b,c);}
static void center(int y,const char*s,int c){text((240-length(s)*6)/2,y,s,c);}
static void wrap(int x,int y,const char*s,int cols,int rows,int color){
 char out[48];int row=0,n=0;while(*s&&*s!='|'&&row<rows){
  if(*s=='\n'){out[n]=0;text(x,y+row*10,out,color);n=0;row++;s++;continue;}
  if(*s==' '){if(n)out[n++]=' ';s++;continue;}
  int word=0;while(s[word]&&s[word]!=' '&&s[word]!='\n'&&s[word]!='|')word++;
  if(n+word>cols&&n){while(n&&out[n-1]==' ')n--;out[n]=0;text(x,y+row*10,out,color);n=0;row++;if(row>=rows)break;}
  for(int i=0;i<word;i++){if(n>=cols){out[n]=0;text(x,y+row*10,out,color);n=0;row++;if(row>=rows)return;}out[n++]=*s++;}
 }if(n&&row<rows){out[n]=0;text(x,y+row*10,out,color);}
}
static void art(int x,int y,const u8*s,int w,int h,int scale,int flip){
 int x0=MAX(0,-x/scale),y0=MAX(0,-y/scale),x1=MIN(w,(240-x+scale-1)/scale),y1=MIN(h,(160-y+scale-1)/scale);
 if(scale==1){for(int j=y0;j<y1;j++)for(int i=x0;i<x1;i++){int c=s[j*w+(flip?w-1-i:i)];if(c&&x+i>=0&&y+j>=0)fb[(y+j)*240+x+i]=c;}}
 else for(int j=y0;j<y1;j++)for(int i=x0;i<x1;i++){int c=s[j*w+(flip?w-1-i:i)];if(c)rect(x+i*scale,y+j*scale,scale,scale,c);}
}
static void creature(int x,int y,int s,int scale,int flip){art(x,y,creature_art+s*1024,32,32,scale,flip);}
static void person(int x,int y,int id,int dir,int step){art(x,y,people_art+(id*8+dir*2+(step&1))*384,16,24,1,0);}
static void ellipse(int x,int y,int w,int h,int color){for(int j=0;j<h;j++)for(int i=0;i<w;i++){int a=2*i-w,b=2*j-h;if(a*a*h*h+b*b*w*w<=w*w*h*h)rect(x+i,y+j,1,1,color);}}
static void bar(int x,int y,int w,int amount,int total,int c){rect(x,y,w,4,1);rect(x+1,y+1,(w-2)*MAX(0,amount)/MAX(1,total),2,c);}

static u32 checksum(const Save*s){const u8*b=(const u8*)s;u32 h=2166136261u;for(unsigned i=0;i<offsetof(Save,checksum);i++){h^=b[i];h*=16777619u;}return h;}
static int valid(const Save*s){return s->magic==0x42424e44&&s->version==1&&s->count>0&&s->count<=6&&s->map<2&&s->x<32&&s->y<24&&s->checksum==checksum(s);}
static void readslot(Save*s,int base){volatile u8*ram=(volatile u8*)0x0e000000;for(unsigned i=0;i<sizeof(Save);i++)((u8*)s)[i]=ram[base+i];}
static int loadgame(void){Save a,b;readslot(&a,0);readslot(&b,512);int va=valid(&a),vb=valid(&b);if(!va&&!vb)return 0;g=vb&&(!va||b.serial>a.serial)?b:a;return 1;}
static void savegame(void){
 if(!g.count)return;g.magic=0x42424e44;g.version=1;g.serial++;g.checksum=checksum(&g);
 volatile u8*ram=(volatile u8*)0x0e000000;const u8*b=(const u8*)&g;
 for(int slot=1;slot>=0;slot--){int base=slot*512;ram[base]=0;for(unsigned i=1;i<sizeof(Save);i++)ram[base+i]=b[i];ram[base]=b[0];}hasSave=1;
}
static void tone(int hz,int duration,int soft){
 if(g.mute)return;int freq=2048-131072/MAX(64,hz);REG16(0x04000068)=(soft?0x3000:0x7000)|0x0080;REG16(0x0400006c)=0x8000|(freq&2047);sfxFrames=duration;
}
static void audio(void){
 if(g.mute){REG16(0x04000084)=0;return;}REG16(0x04000084)=0x80;REG16(0x04000080)=0x3377;REG16(0x04000082)=2;
 if(sfxFrames){if(!--sfxFrames)REG16(0x04000068)=0;}
 int area=screen==BATTLE?2:g.map;
 if(area!=musicArea){musicArea=area;musicTick=0;musicNote=0;}
 if(++musicTick>=(area==2?14:24)){musicTick=0;int hz=notes[musicNote++&15];if(area==1)hz=hz*3/4;if(area==2)hz=hz*5/4;
  REG16(0x04000060)=0;REG16(0x04000062)=0x2000|0x0080;REG16(0x04000064)=0x8000|((2048-131072/hz)&2047);
 }
}
static void say(const char*who,const char*message,int after){copy(speaker,who);copy(dialog,message);dialogPos=0;dialogAfter=after;dialogReturn=FIELD;screen=DIALOG;tone(700,3,1);}
static void showgrowth(int i){growIndex=i;cursor=g.friends[i].path==2?1:0;screen=GROWTH;}
static void dialogue_input(void){
 if(pressed&(A|B)){int n=dialogPos;while(dialog[n]&&dialog[n]!='|')n++;if(dialog[n]=='|'){dialogPos=n+1;tone(750,2,1);}else{
  screen=dialogReturn;if(dialogAfter==TO_CHOOSE){screen=CHOOSE;cursor=0;}else if(dialogAfter==TO_GROW)showgrowth(growIndex);else if(dialogAfter==TO_BATTLE)screen=BATTLE;
 }}
}

static void initworld(void){
 memcpy(world,map_tiles,sizeof(world));
}
static int blocking(int map,int x,int y){
 if(x<0||x>=32||y<0||y>=24)return 1;int t=world[map][y*32+x];return t==2||t==3||t==8||t==10||t==11||t==12||t==13||t==15;
}
static const char*objective(void){
 if(!g.count)return "MEET MARA IN THE SQUARE";
 if(!g.scout)return "FIND TAVI ON FERNWAY TRAIL";
 if(g.count<2)return "BEFRIEND A WILD CREATURE";
 if(!g.friends[0].path)return "CHOOSE A GROWTH PATH AT LV3";
 if(!g.rival)return "MEET IRIS AT THE LOOKOUT";
 return "TRAIL COMPLETE - EXPLORE + SAVE";
}
static void newgame(void){
 memset(&g,0,sizeof(g));g.x=15;g.y=14;g.dir=3;g.treats=5;g.tonics=3;followx=g.x*16;followy=g.y*16;screen=FIELD;
 for(int i=0;i<6;i++)encounters[i].cooldown=0;
 say("MOSSBELL COMMONS","Today is your first day on the trail.|Mara is waiting in the stone square, just north of you.|Walk with the D-PAD. Press A beside a person to talk. START opens your journal.",FIELD_BACK);
}
static void choosepartner(void){
 g.starter=cursor;g.friends[0]=makefriend(cursor,2);g.count=1;g.lead=0;g.seen=1<<cursor;followx=g.x*16;followy=g.y*16;
 char b[400];copy(b,species[cursor].name);append(b," chose to travel with you!|Take these Trail Treats and Tonics. Friends grow through shared adventures, not hunger meters.|Head north to Fernway. Tavi will show you how to meet a wild friend.|Rest at any camp for free. Your journal always records the next step.");savegame();say("MARA / TRAILKEEPER",b,FIELD_BACK);tone(1319,12,0);
}

static void battlelog(const char*s){if(battle.logCount<12)copy(battle.logs[battle.logCount++],s);}
static void lognamed(const char*name,const char*verb){char b[112];copy(b,name);append(b,verb);battlelog(b);}
static void battledamage(const char*name,int n,int strong){char b[112];copy(b,name);append(b," lost ");addnum(b,n);append(b," HP.");if(strong)append(b," A strong match!");battlelog(b);}
static void startbattle(int trainer,int id){
 memset(&battle,0,sizeof(battle));battle.trainer=trainer;battle.encounter=id;battle.active=available(g.lead)?g.lead:firstalive();if(battle.active<0){healall();battle.active=0;}
 if(trainer){healall();battle.count=2;battle.foes[0]=makefriend((g.starter+1)%3,3);battle.foes[1]=makefriend(3,3);g.seen|=(1<<battle.foes[0].species)|(1<<3);battlelog("Iris: Let's see what your team can do!");}
 else {Encounter*e=&encounters[id];battle.count=1;battle.foes[0]=makefriend(e->species,e->level);g.seen|=1<<e->species;lognamed(species[e->species].name," watches you with curiosity.");}
 screen=BATTLE;cursor=0;tone(880,6,0);
}
static void givexp(int xp){
 for(int i=0;i<g.count;i++)if(!g.friends[i].stationed){Friend*f=&g.friends[i];f->xp+=xp;f->bond=MIN(9,f->bond+1);
  while(f->xp>=f->level*4&&f->level<12){int old=maxhp(f);f->xp-=f->level*4;f->level++;f->hp=MIN(maxhp(f),f->hp+maxhp(f)-old+3);f->energy=MIN(maxenergy(f),f->energy+2);
   char b[112];copy(b,species[f->species].name);append(b," grew to level ");addnum(b,f->level);append(b,"!");battlelog(b);
  }
 }
}
static void finishbattle(int result){
 battle.result=result;if(result==1){battlelog(battle.trainer?"Iris: You earned your Trail Pin!":"A good spar. Your friends learned together.");givexp(battle.trainer?12:6);}
 if(result==2){givexp(8);battlelog("You made a new friend! No catching lottery.");}
 if(result==3)battlelog("You head back to safety. No items are lost.");
 if(result==4)battlelog("You give each other space and leave safely.");
}
static int effectiveness(int at,int de){
 static const u8 chart[5][5]={{10,7,15,15,10},{15,10,7,7,10},{7,15,10,15,10},{7,15,7,10,10},{10,10,10,10,10}};
 return chart[at][de];
}
static int damage(Friend*a,Friend*d,int base,int typed){
 int n=base+a->level+power(a)/3-defense(d)/3;
 if(typed)n=n*effectiveness(species[a->species].type,species[d->species].type)/10;return MAX(2,n);
}
static void enemyturn(void){
 if(battle.result)return;Friend*p=&g.friends[battle.active],*e=&battle.foes[battle.foe];int special=(battle.turn%3==1&&e->energy>=2);
 int n=damage(e,p,special?7:3,special);if(e->species==1&&e->energy==maxenergy(e))n++;if(special)e->energy-=2;else e->energy=MIN(maxenergy(e),e->energy+1);
 if(battle.guard){n=(n+1)/2;battle.guard=0;}p->hp=MAX(0,p->hp-n);
 char msg[112];copy(msg,species[e->species].name);append(msg," used ");append(msg,special?species[e->species].move:"NUDGE");append(msg,".");battlelog(msg);
 battledamage(species[p->species].name,n,special&&effectiveness(species[e->species].type,species[p->species].type)>10);
 battle.shake=12;if(p->species==5){e->hp=MAX(0,e->hp-1);battlelog("Burrbit's thorns sting for 1 HP.");}
 if(!p->hp){lognamed(species[p->species].name," needs a rest.");int next=firstalive();if(next<0){finishbattle(3);return;}battle.active=next;lognamed(species[g.friends[next].species].name," steps forward!");}
 if(!e->hp){battle.foe++;if(battle.foe>=battle.count)finishbattle(1);else{battle.calm=0;lognamed(species[battle.foes[battle.foe].species].name," joins Iris!");}}
 battle.turn++;
}
static void usemove(int move){
 Friend*p=&g.friends[battle.active],*e=&battle.foes[battle.foe];
 int cost=move==1?2:move==3?(p->path==1?3:p->path==2?2:1):0;
 if(p->energy<cost){battlelog("Not enough energy. SETTLE restores it.");return;}
 int full=p->energy==maxenergy(p);p->energy-=cost;
 if(move==2){p->hp=MIN(maxhp(p),p->hp+(p->species==0?5:3));p->energy=MIN(maxenergy(p),p->energy+(p->species==2?4:3));battlelog("A quiet breath. Health and energy return.");}
 else if(move==3&&p->path==2){p->hp=MIN(maxhp(p),p->hp+8);battle.guard=1;battlelog("SHELTER restores 8 HP and halves the next hit.");}
 else if(move==3&&!p->path){battle.calm=1;battlelog("SOOTHE calms this creature. A treat can now work.");}
 else{
  int n=damage(p,e,move==0?3:move==1?7:13,move!=0);if(full&&p->species==1)n++;e->hp=MAX(0,e->hp-n);
  char msg[112];copy(msg,species[p->species].name);append(msg," used ");append(msg,move==0?"NUDGE":move==1?species[p->species].move:"BURST");append(msg,".");battlelog(msg);battledamage(species[e->species].name,n,move!=0&&effectiveness(species[p->species].type,species[e->species].type)>10);
  if(p->species==4&&move==1){battle.calm=1;battlelog("Luma's gentle light calms the wild creature.");}battle.shake=12;tone(1500,5,0);
 }
 battle.menu=0;battle.cursor=0;
 if(!e->hp){lognamed(species[e->species].name," takes a breather.");battle.foe++;if(battle.foe>=battle.count)finishbattle(1);else{battle.calm=0;lognamed(species[battle.foes[battle.foe].species].name," joins Iris!");}}
 else enemyturn();
}
static void recruit(void){
 Friend*e=&battle.foes[battle.foe];
 if(battle.trainer){battlelog("Iris's partners already have a home!");return;}
 if(owned(e->species)){battlelog("Already in your team. Spar to practice instead.");return;}
 if(g.count>=6){battlelog("Your team is full.");return;}
 if(!g.treats){battlelog("No treats. Tavi and Mara can restock you.");return;}
 if(e->hp>maxhp(e)/2&&!battle.calm){battlelog("Still wary! Reach half HP, or use SOOTHE.");return;}
 g.treats--;g.friends[g.count++]=makefriend(e->species,e->level);lognamed(species[e->species].name," accepts your treat and joins you!");finishbattle(2);tone(1760,15,0);
}
static void afterbattle(void){
 int result=battle.result;if(!battle.trainer)encounters[battle.encounter].cooldown=35;
 if(result==3){healall();g.map=0;g.x=6;g.y=12;followx=g.x*16;followy=g.y*16;savegame();say("MARA","Everyone is safe and rested. A setback costs no friends, treats, or money.|Try SETTLE to recover energy, and switch partners when a matchup feels tough.",FIELD_BACK);return;}
 if(battle.trainer&&result==1){g.rival=1;healall();savegame();screen=ENDING;cursor=0;tone(1760,18,0);return;}
 screen=FIELD;savegame();
 for(int i=0;i<g.count;i++)if(g.friends[i].level>=3&&!g.friends[i].path){growIndex=i;say("A NEW POSSIBILITY","Your friend is ready to choose a growth path.|STRIDER adds power and BURST. WARDEN adds toughness and SHELTER.|Neither changes who your friend is. You can revise paths for free at camp.",TO_GROW);break;}
}
static void battleinput(void){
 if(battle.shake)battle.shake--;
 if(battle.logPos<battle.logCount){if(pressed&(A|B)){battle.logPos++;tone(650,2,1);if(battle.logPos>=battle.logCount){battle.logPos=battle.logCount=0;if(battle.result)afterbattle();}}return;}
 if(battle.menu==0){
  if(pressed&(LEFT|RIGHT))battle.cursor^=1;if(pressed&(UP|DOWN))battle.cursor^=2;
  if(pressed&A){int s=battle.cursor;battle.cursor=0;if(s==0)battle.menu=1;else if(s==1)recruit();else if(s==2)battle.menu=2;else battle.menu=3;}
  if(pressed&B){if(battle.trainer)battlelog("Iris: No pressure. We'll finish this practice!");else finishbattle(4);}
 }else if(battle.menu==1){
  if(pressed&(LEFT|RIGHT))battle.cursor^=1;if(pressed&(UP|DOWN))battle.cursor^=2;if(pressed&B){battle.menu=0;battle.cursor=0;}else if(pressed&A)usemove(battle.cursor);
 }else if(battle.menu==2){
  if(pressed&UP)battle.cursor=(battle.cursor+g.count-1)%g.count;if(pressed&DOWN)battle.cursor=(battle.cursor+1)%g.count;
  if(pressed&B){battle.menu=0;battle.cursor=0;}else if(pressed&A){int i=battle.cursor;if(i==battle.active)battlelog("That friend is already leading.");else if(!available(i))battlelog("That friend is resting or helping in town.");else{battle.active=i;g.lead=i;lognamed(species[g.friends[i].species].name," steps forward!");battle.menu=0;battle.cursor=0;enemyturn();}}
 }else if(battle.menu==3){
  if(pressed&(UP|DOWN))battle.cursor^=1;if(pressed&B){battle.menu=0;battle.cursor=0;}else if(pressed&A){
   if(!battle.cursor){if(!g.tonics)battlelog("No tonics left. Camps heal everyone for free.");else{g.tonics--;Friend*p=&g.friends[battle.active];p->hp=MIN(maxhp(p),p->hp+18);battlelog("Tonic restores 18 HP.");battle.menu=0;enemyturn();}}
   else{if(battle.trainer)battlelog("You cannot leave a friendly duel.");else finishbattle(4);}
  }
 }
}

static void talk(void){
 if(g.map==0){
  if(near(15,10)){
   if(!g.count)say("MARA / TRAILKEEPER","A good partner is more than a strong attack.|Choose someone you want to travel with. Every starter can finish this trail.|Leaf, Ember, or Tide? Let's meet them.",TO_CHOOSE);
   else{healall();if(g.treats<3)g.treats=3;savegame();say("MARA","Your team is rested. I topped up your treats if you were running low.|LEAF beats TIDE. TIDE beats EMBER. EMBER beats LEAF.|A favorite can stay useful. Try a growth path, a different move, or a new teammate.",FIELD_BACK);}return;
  }
  if(near(6,11)){healall();if(g.tonics<2)g.tonics=2;savegame();say("ELI / SPRING KEEPER","A sip, a stretch, and a fresh start. Health and energy restored!|Rest is always free. Nobody goes hungry while you're away.|I put two Tonics in your pack if you needed them.",FIELD_BACK);return;}
  if(near(24,14)){
   int stationed=-1;for(int i=0;i<g.count;i++)if(g.friends[i].stationed)stationed=i;
   if(stationed>=0){g.friends[stationed].stationed=0;heal(&g.friends[stationed]);savegame();say("NELL / GARDENER","Your friend is back with you, happy and rested.|The garden stays healthy. Helping wasn't a timer you needed to maintain.",FIELD_BACK);}
   else if(g.count<2)say("NELL / GARDENER","The garden could use a gentle helper.|After you meet a wild friend, bring them here. They'll have a real place in town.|This is optional. Your adventure doesn't depend on leaving a friend behind.",FIELD_BACK);
   else{int i=g.count-1;if(i==g.lead)i=0;g.friends[i].stationed=1;if(g.lead==i)g.lead=firstalive();char b[600];copy(b,species[g.friends[i].species].name);append(b," will help tend our garden. You can see them beside the beds!|While helping, they won't battle. Talk to me any time to bring them back.");if(!g.job){g.job=1;g.treats+=3;g.tonics++;append(b,"|Thank you! Here are 3 Treats and a Tonic. The garden will bloom now.");}savegame();say("NELL / GARDENER",b,FIELD_BACK);}return;
  }
  if(near(11,16)){say("FINN","A path is a choice, not a replacement.|At level 3, open COMPANIONS and press SELECT. Camps let you revise a path for free.|A sets a healthy friend as leader. The rest still learn alongside you.",FIELD_BACK);return;}
  if(near(17,15)){say("TRAIL SIGN","D-PAD: walk. Hold B: run. A: talk or inspect.|START: team, field guide, journal, save.|In battle, B backs out. From the main battle menu, B leaves a wild encounter.",FIELD_BACK);return;}
  if(near(13,3)){say("NORTH GATE","FERNWAY TRAIL - north.|Rest camp, wild companions, and Iris's lookout duel.|Your first Trail Pin is waiting.",FIELD_BACK);return;}
 }else{
  if(near(4,19)){
   if(!g.scout){g.scout=1;g.treats=MAX(g.treats,5);savegame();say("TAVI / FIELD SCOUT","See the little stone-shell friend east of here? Walk up and press A to meet it.|NUDGE is gentle. Your signature move uses energy. SETTLE restores health and energy.|At half HP, BEFRIEND always works with one Trail Treat. SOOTHE also makes a creature ready.|You can leave any wild encounter. Take your time and find someone you like.",FIELD_BACK);}
   else{if(g.treats<3)g.treats=3;savegame();say("TAVI","Treats topped up if you needed them.|Wild creatures return after you've walked a while. You choose when to spar.|Iris waits across the bridge, northeast at the lookout. Bring a second friend!",FIELD_BACK);}return;
  }
  if(near(7,12)){healall();for(int i=0;i<g.count;i++)g.friends[i].bond=MIN(9,g.friends[i].bond+1);savegame();say("REST CAMP","Your friends settle beside the lantern. Everyone's health and energy return.|A quiet pause, not a hunger clock.|To revise a growth path here, open COMPANIONS and press SELECT.",FIELD_BACK);return;}
  if(near(26,5)){
   if(g.rival){say("IRIS / YOUR RIVAL","That's your first Trail Pin. You earned it!|This draft ends here, but you can still explore, meet friends, and try different paths.|Save from your journal before you leave.",FIELD_BACK);return;}
   if(g.count<2){say("IRIS / YOUR RIVAL","You made it! Before our practice duel, try befriending a wild companion.|You don't need a perfect team. I just want you to try having a teammate.|Tavi can help, and the camp will rest everyone.",FIELD_BACK);return;}
   if(firstalive()<0)healall();startbattle(1,0);say("IRIS / YOUR RIVAL","Let's make this a friendly first challenge! I'll rest your team before we begin.|Read the next move, use SETTLE, and swap friends when you need to.|Win or lose, your friends are safe. Ready?",TO_BATTLE);return;
  }
  if(near(6,20)){say("FIELD NOTICE","Meet wild creatures by walking up to them.|No surprise battles while you explore.|You can see HP, energy costs, and the opponent's next move during battle.",FIELD_BACK);return;}
  if(near(23,5)){say("LOOKOUT MARKER","IRIS'S PRACTICE DUEL|Two opponents. Free healing before the duel.|Choose a growth path at level 3. Any starter can win.",FIELD_BACK);return;}
  for(int i=0;i<6;i++)if(!encounters[i].cooldown&&near(encounters[i].x,encounters[i].y)){startbattle(0,i);return;}
 }
 const u8 px[3]={9,8,22},py[3]={16,20,12},pm[3]={0,1,1};
 for(int i=0;i<3;i++)if(g.map==pm[i]&&!(g.pickups&(1<<i))&&near(px[i],py[i])){g.pickups|=1<<i;g.treats+=2;g.tonics++;savegame();say("TRAIL SATCHEL","You found 2 Trail Treats and a Tonic!|They were left by the trailkeepers for a new explorer.",FIELD_BACK);return;}
 if(g.map==1&&near(26,3)&&g.rival){say("FIRST LIGHT LOOKOUT","Mossbell's roofs peek through the trees. Your friends gather beside you.|A little journey. A lasting beginning.|Thanks for playing BRAMBLEBOUND: FIRST LIGHT.",FIELD_BACK);return;}
}
static int npcblock(int x,int y){
 if(!g.map)return(x==15&&y==10)||(x==6&&y==11)||(x==24&&y==14)||(x==11&&y==16);
 return(x==4&&y==19)||(x==7&&y==12)||(x==26&&y==5);
}
static void fieldinput(void){
 if(pressed&START){screen=MENU;cursor=0;return;}if(pressed&A){talk();return;}
 if(moveDelay){moveDelay--;return;}int dx=0,dy=0;
 if(keys&UP){dy=-1;g.dir=3;}else if(keys&DOWN){dy=1;g.dir=0;}else if(keys&LEFT){dx=-1;g.dir=1;}else if(keys&RIGHT){dx=1;g.dir=2;}
 if(!dx&&!dy)return;moveDelay=keys&B?3:6;int x=g.x+dx,y=g.y+dy;
 if(blocking(g.map,x,y)||npcblock(x,y))return;
 if(!g.map&&y==0&&(x==15||x==16)){
  if(!g.count){say("MARA","Choose your first companion before heading onto the trail. I'm in the stone square.",FIELD_BACK);return;}
  g.map=1;g.x=4;g.y=21;g.dir=3;followx=g.x*16;followy=g.y*16;savegame();return;
 }
 if(g.map&&y==23&&x>=3&&x<=5){g.map=0;g.x=15;g.y=1;g.dir=0;followx=g.x*16;followy=g.y*16;savegame();return;}
 if(g.map)for(int i=0;i<6;i++)if(!encounters[i].cooldown&&x==encounters[i].x&&y==encounters[i].y){startbattle(0,i);return;}
 followx=g.x*16;followy=g.y*16;g.x=x;g.y=y;g.steps++;walkFrame^=1;
 for(int i=0;i<6;i++)if(encounters[i].cooldown)encounters[i].cooldown--;
}
static void menuinput(void){
 if(pressed&(B|START)){screen=FIELD;return;}
 if(pressed&UP)cursor=(cursor+5)%6;if(pressed&DOWN)cursor=(cursor+1)%6;
 if(pressed&A){
  if(cursor==0){if(g.count){screen=TEAM;cursor=0;}else say("COMPANIONS","Mara has three partners waiting to meet you in the square.",FIELD_BACK);}
  else if(cursor==1){screen=GUIDE;menuPage=0;cursor=0;}
  else if(cursor==2){screen=JOURNAL;cursor=0;}
  else if(cursor==3){if(g.count){savegame();say("JOURNEY SAVED","Your companions and progress are safe.|CONTINUE on the title screen picks up here.|Your emulator stores this in its battery-save file. Keep that file with your ROM.",FIELD_BACK);}else say("SAVE JOURNEY","Choose a partner with Mara first. Your journey will save automatically then.",FIELD_BACK);}
  else if(cursor==4){g.mute^=1;if(g.mute)REG16(0x04000084)=0;else tone(880,5,1);}
  else screen=FIELD;
 }
}
static void teaminput(void){
 if(pressed&(B|START)){screen=MENU;cursor=0;return;}
 if(pressed&UP)cursor=(cursor+g.count-1)%g.count;if(pressed&DOWN)cursor=(cursor+1)%g.count;
 if(pressed&A){if(available(cursor)){g.lead=cursor;tone(1100,4,1);}else tone(250,4,1);}
 if(pressed&SELECT){if(g.friends[cursor].level<3){say("GROWTH PATHS","Paths unlock at level 3. A short spar or a new friendship will help your team grow.",FIELD_BACK);}
  else if(g.friends[cursor].path&&!fieldcamp())say("GROWTH PATHS","Your friend can revise their path at Mossbell or the trail camp. It's always free.",FIELD_BACK);
  else showgrowth(cursor);
 }
}
static void growthinput(void){
 if(pressed&(LEFT|RIGHT|UP|DOWN))cursor^=1;
 if(pressed&B){screen=FIELD;return;}
 if(pressed&A){Friend*f=&g.friends[growIndex];int old=maxhp(f);f->path=cursor+1;f->hp=MIN(maxhp(f),f->hp+MAX(0,maxhp(f)-old));f->energy=maxenergy(f);savegame();char b[400];copy(b,species[f->species].name);append(b,cursor?" chose WARDEN!|More health and defense. SHELTER heals and softens the next hit.":" chose STRIDER!|More attack and energy. BURST is a powerful signature strike.");append(b,"|The same friend, a new way to help. Revise for free at camp.");say("GROWING TOGETHER",b,FIELD_BACK);tone(1568,12,0);}
}

static void marker(int x,int y,int color){rect(x+5,y-8,5,5,1);rect(x+6,y-7,3,3,color);rect(x+7,y-2,1,1,1);}
static void drawfield(void){
 if(absval(visualx-g.x*16)>32||absval(visualy-g.y*16)>32){visualx=g.x*16;visualy=g.y*16;visualfx=followx;visualfy=followy;}
 visualx+=CLAMP(g.x*16-visualx,-4,4);visualy+=CLAMP(g.y*16-visualy,-4,4);
 visualfx+=CLAMP(followx-visualfx,-4,4);visualfy+=CLAMP(followy-visualfy,-4,4);
 camx=CLAMP(visualx-112,0,512-240);camy=CLAMP(visualy-72,0,384-160);
 const u8*bg=map_art+g.map*512*384+camy*512+camx;
 for(int y=0;y<160;y++)memcpy(fb+y*240,bg+y*512,240);
 if(!g.map){
  person(15*16-camx,10*16-camy-8,1,0,0);person(6*16-camx,11*16-camy-8,2,0,0);person(24*16-camx,14*16-camy-8,3,1,0);person(11*16-camx,16*16-camy-8,4,2,0);
  if(!g.count){marker(15*16-camx,10*16-camy-8,37);for(int i=0;i<3;i++)creature((13+i*2)*16-camx,8*16-camy-12,i,1,0);}
  if(g.job)for(int x=22;x<28;x++){int sx=x*16-camx,sy=11*16-camy;rect(sx+4,sy+3,2,8,29);rect(sx+2,sy+2,6,3,28);rect(sx+4,sy,2,7,37);}
  for(int i=0;i<g.count;i++)if(g.friends[i].stationed)creature(25*16-camx,12*16-camy-10,g.friends[i].species,1,0);
 }else{
  for(int i=0;i<6;i++)if(!encounters[i].cooldown){Encounter*e=&encounters[i];creature(e->x*16-camx-8,e->y*16-camy-16+(frame/35%2),e->species,1,0);}
  person(4*16-camx,19*16-camy-8,2,0,0);if(!g.scout)marker(4*16-camx,19*16-camy-8,37);
  int cx=7*16-camx,cy=12*16-camy;rect(cx-5,cy-12,2,24,16);rect(cx-8,cy-12,8,8,37);rect(cx-7,cy-11,6,6,38);
  person(26*16-camx,5*16-camy-8,4,0,0);if(!g.rival)marker(26*16-camx,5*16-camy-8,37);
  rect(26*16-camx,3*16-camy,16,2,36);rect(26*16-camx+7,3*16-camy-12,2,12,16);rect(26*16-camx+5,3*16-camy-14,6,5,37);
 }
 const u8 px[3]={9,8,22},py[3]={16,20,12},pm[3]={0,1,1};
 for(int i=0;i<3;i++)if(g.map==pm[i]&&!(g.pickups&(1<<i))){int x=px[i]*16-camx,y=py[i]*16-camy;rect(x+4,y+5,9,8,16);rect(x+5,y+4,7,7,37);rect(x+7,y+2,3,4,36);}
 if(g.count){int fx=visualfx-camx,fy=visualfy-camy;const u8*s=creature_art+g.friends[g.lead].species*1024;
  for(int j=0;j<16;j++)for(int i=0;i<16;i++){int c=s[(j*2)*32+i*2];if(c)rect(fx+i,fy+j-3,1,1,c);}}
 person(visualx-camx,visualy-camy-8,0,g.dir,walkFrame);
 panel(3,3,234,17,29);text(9,8,places[g.map],1);
 if(g.count){text(180,8,"TEAM",1);smallnum(210,8,g.count,1);}
 rect(0,148,240,12,1);text(4,151,objective(),2);
 if(!g.count||screen==MENU){text(4,137,"A TALK   B RUN   START JOURNAL",1);}
}
static void drawdialog(void){panel(4,103,232,54,29);text(11,110,speaker,4);wrap(11,122,dialog+dialogPos,35,3,1);if(frame/20%2)text(226,146,">",4);}
static void drawtitle(void){
 memcpy(fb,title_art,sizeof(fb));
 for(int i=0;i<19;i++)rect((i*47+frame/10)%240,(i*29)%92,1,1,i%2?6:37);
 panel(13,12,214,53,36);center(23,"BRAMBLEBOUND",1);center(39,"F I R S T  L I G H T",4);center(53,"AN ORIGINAL GBA ADVENTURE",9);
 creature(15,78,0,1,0);creature(192,77,1,1,1);creature(106,60,2,1,0);
 panel(52,95,136,49,29);
 if(hasSave){text(70,104,"CONTINUE",1);text(70,117,"NEW JOURNEY",1);text(70,130,"CONTROLS",1);text(59,104+cursor*13,">",4);}
 else{text(70,106,"NEW JOURNEY",1);text(70,126,"CONTROLS",1);text(59,106+cursor*20,">",4);}
 center(151,"D-PAD CHOOSE     A CONFIRM",2);
}
static void drawchoose(void){
 rect(0,0,240,160,41);panel(4,4,232,25,29);center(13,"WHO WILL WALK WITH YOU?",1);
 for(int i=0;i<3;i++){int x=5+i*78;panel(x,35,74,83,cursor==i?36:5);creature(x+20,44,i,1,0);text(x+7,82,species[i].name,1);text(x+7,96,types[species[i].type],4);if(cursor==i)text(x+32,106,">",1);}
 const char*h[]={"Steady and gentle. SETTLE restores extra health.","A bold little spark. Strong attacks, lighter defenses.","A patient river friend. SETTLE restores extra energy."};
 wrap(9,124,h[cursor],37,2,1);center(151,"LEFT/RIGHT CHOOSE   A PARTNER",1);
}
static void drawmenu(void){drawfield();panel(69,25,164,116,29);text(80,34,"TRAIL JOURNAL",4);for(int i=0;i<6;i++){text(91,50+i*14,menuItems[i],1);if(i==cursor)text(79,50+i*14,">",4);}text(177,106,g.mute?"OFF":"ON",4);}
static void drawteam(void){
 rect(0,0,240,160,41);panel(3,3,234,19,29);text(10,9,"YOUR COMPANIONS",1);text(181,9,"A LEAD",4);
 for(int i=0;i<g.count;i++){Friend*f=&g.friends[i];int y=27+i*18;rect(4,y,105,17,cursor==i?7:13);if(i==g.lead)text(6,y+5,">",4);text(14,y+2,species[f->species].name,1);text(15,y+10,f->stationed?"IN TOWN":"LV",9);if(!f->stationed)smallnum(33,y+10,f->level,9);}
 Friend*f=&g.friends[cursor];panel(113,26,123,117,29);creature(157,32,f->species,1,0);text(121,69,types[species[f->species].type],4);
 text(121,80,"HP",1);smallnum(145,80,f->hp,1);text(163,80,"/",1);smallnum(173,80,maxhp(f),1);
 text(121,91,"ENERGY",1);smallnum(169,91,f->energy,1);
 text(121,102,f->path==1?"STRIDER":f->path==2?"WARDEN":"PATH AT LV3",4);
 text(121,113,"BOND",1);smallnum(158,113,f->bond,1);text(121,125,species[f->species].trait,9);
 center(151,"SELECT PATH   B BACK   A LEAD",1);
}
static void drawgrowth(void){
 Friend*f=&g.friends[growIndex];rect(0,0,240,160,41);panel(4,4,232,23,29);center(13,"GROW WITHOUT LOSING YOURSELF",1);
 creature(104,32,f->species,1,0);center(68,species[f->species].name,1);
 panel(5,82,112,57,cursor==0?36:29);panel(123,82,112,57,cursor==1?36:29);
 text(17,91,"STRIDER",1);text(135,91,"WARDEN",1);text(13,104,"+2 ATK +1 ENERGY",9);text(131,104,"+6 HP +2 DEF",9);
 text(13,117,"BURST: BIG HIT",4);text(131,117,"SHELTER: HEAL",4);text(131,127,"+ SOFTEN A HIT",4);
 center(146,"A CHOOSE  B LATER  FREE AT CAMP",1);
}
static void drawguide(void){
 rect(0,0,240,160,41);panel(3,3,234,20,29);text(10,10,"FIELD GUIDE",1);text(157,10,"L/R PAGES",4);
 if(menuPage==0){
  for(int i=0;i<6;i++){int y=28+i*19;rect(5,y,94,18,cursor==i?7:13);text(11,y+5,(g.seen&(1<<i))?species[i].name:"????",1);}
  panel(104,28,132,115,29);if(g.seen&(1<<cursor)){creature(152,33,cursor,1,0);text(113,70,types[species[cursor].type],4);text(113,83,species[cursor].trait,1);wrap(113,96,species[cursor].hint,19,4,9);}else wrap(114,60,"An unknown neighbor. Explore Fernway to meet them.",18,6,1);
 }else{wrap(10,33,"LEAF > TIDE > EMBER > LEAF\n\nSTONE resists EMBER, but LEAF and TIDE crack its shell.\n\nNUDGE saves energy. SETTLE restores it. Read the next move before choosing.\n\nBEFRIEND works at half HP or after SOOTHE. One treat, no luck roll.",36,12,1);}
 center(151,"UP/DOWN CREATURES  B BACK",1);
}
static void drawjournal(void){
 rect(0,0,240,160,41);panel(4,4,232,23,29);center(12,"YOUR FIRST TRAIL",1);
 const char*tasks[]={"CHOOSE A PARTNER","MEET TAVI","BEFRIEND A WILD CREATURE","CHOOSE A GROWTH PATH","EARN IRIS'S TRAIL PIN","OPTIONAL: HELP NELL'S GARDEN"};
 int done[6]={g.count>0,g.scout,g.count>=2,g.count&&g.friends[0].path,g.rival,g.job};
 for(int i=0;i<6;i++){text(10,38+i*15,done[i]?"+":"-",done[i]?4:9);text(24,38+i*15,tasks[i],done[i]?4:1);}
 text(10,133,"TREATS",1);smallnum(58,133,g.treats,1);text(89,133,"TONICS",1);smallnum(138,133,g.tonics,1);
 center(151,"REST IS FREE. YOUR FRIENDS ARE SAFE.",1);
}
static const char*movename(Friend*f,int m){return m==0?"NUDGE":m==1?species[f->species].move:m==2?"SETTLE":f->path==1?"BURST":f->path==2?"SHELTER":"SOOTHE";}
static void drawbattle(void){
 rect(0,0,240,160,41);rect(0,32,240,75,5);rect(0,32,240,12,6);
 for(int i=0;i<16;i++)rect((i*43)%240,37+(i*17)%65,2,1,30);
 int f=MIN(battle.foe,battle.count-1);Friend*p=&g.friends[battle.active],*e=&battle.foes[f];int shake=battle.shake?(battle.shake&2?2:-2):0;
 ellipse(147,76,81,16,4);ellipse(18,94,83,13,4);creature(158+shake,22,e->species,2,0);creature(25-shake,48,p->species,2,1);
 panel(4,3,139,29,29);text(10,10,species[e->species].name,1);text(105,10,"LV",9);smallnum(120,10,e->level,9);bar(10,22,103,e->hp,maxhp(e),e->hp>maxhp(e)/2?29:19);smallnum(117,21,e->hp,1);
 text(152,6,battle.trainer?"IRIS / DUEL":"WILD / MEETING",1);text(152,17,battle.calm?"CALM: TREAT OK":"",4);
 panel(110,70,126,35,29);text(117,77,species[p->species].name,1);text(202,77,"L",9);smallnum(210,77,p->level,9);bar(117,89,74,p->hp,maxhp(p),p->hp>maxhp(p)/3?29:19);smallnum(196,88,p->hp,1);text(117,96,"ENERGY",9);smallnum(159,96,p->energy,4);
 if(battle.logPos<battle.logCount){panel(3,108,234,49,29);wrap(10,118,battle.logs[battle.logPos],36,3,1);if(frame/20%2)text(226,146,">",4);return;}
 panel(3,108,234,49,29);
 if(battle.menu==2){for(int i=0;i<g.count;i++){int x=i<3?10:126,y=118+(i%3)*11;text(x,y,battle.cursor==i?">":" ",4);text(x+8,y,species[g.friends[i].species].name,available(i)?1:9);smallnum(x+76,y,g.friends[i].hp,9);}return;}
 if(battle.menu==3){text(15,120,"TONIC / RESTORE 18 HP",1);smallnum(181,120,g.tonics,4);text(15,137,battle.trainer?"DUEL: NO RUNNING":"LEAVE SAFELY",1);text(7,120+battle.cursor*17,">",4);return;}
 for(int i=0;i<4;i++){int x=12+(i%2)*114,y=117+(i/2)*16;const char*label=battle.menu==0?(i==0?"MOVES":i==1?"BEFRIEND":i==2?"SWAP":"PACK"):movename(p,i);text(x,y,label,1);if(battle.cursor==i)text(x-7,y,">",4);
  if(battle.menu==1){int cost=i==1?2:i==3?(p->path==1?3:p->path==2?2:1):0;smallnum(x+91,y,cost,9);}}
 if(battle.menu==0){text(6,147,"NEXT:",9);text(42,147,battle.turn%3==1&&e->energy>=2?species[e->species].move:"NUDGE",9);smallnum(180,147,g.treats,9);text(192,147,"TREATS",9);}
}
static void drawending(void){
 rect(0,0,240,160,40);panel(12,10,216,137,36);center(21,"YOUR FIRST TRAIL PIN",1);
 ellipse(104,39,32,32,36);ellipse(110,45,20,20,37);rect(118,47,3,16,29);rect(113,53,13,3,29);
 center(80,"IRIS: THAT WAS A REAL TEAM.",4);center(95,"YOU FOUND YOUR OWN WAY HERE.",1);
 text(27,111,"FRIENDS MET",9);smallnum(180,111,g.count,4);text(27,123,"GARDEN HELPED",9);text(180,123,g.job?"YES":"NOT YET",4);
 center(151,"A KEEP EXPLORING  /  SAVED",2);
}
static void render(void){
 if(screen==TITLE||screen==CONFIRM){drawtitle();if(screen==CONFIRM){panel(15,52,210,67,19);center(62,"START A NEW JOURNEY?",1);center(78,"THIS REPLACES YOUR SAVE.",9);text(64,99,confirmCursor?"  CANCEL     > NEW":"> CANCEL       NEW",1);}}
 else if(screen==CHOOSE)drawchoose();else if(screen==BATTLE)drawbattle();else if(screen==TEAM)drawteam();else if(screen==GROWTH)drawgrowth();else if(screen==GUIDE)drawguide();else if(screen==JOURNAL)drawjournal();else if(screen==ENDING)drawending();else if(screen==MENU)drawmenu();else{if(screen==DIALOG&&dialogReturn==TITLE)drawtitle();else drawfield();if(screen==DIALOG)drawdialog();}
}
static void input(void){
 if(screen==TITLE){int n=hasSave?3:2;if(pressed&UP)cursor=(cursor+n-1)%n;if(pressed&DOWN)cursor=(cursor+1)%n;if(pressed&(A|START)){
  if(hasSave&&cursor==0){loadgame();screen=FIELD;followx=g.x*16;followy=g.y*16;}
  else if((hasSave&&cursor==1)||(!hasSave&&cursor==0)){if(hasSave){screen=CONFIRM;confirmCursor=0;}else newgame();}
  else{say("HOW TO PLAY","D-PAD walks. Hold B to run. A talks or inspects. START opens your journal.|In battle, A chooses and B goes back. NUDGE is free. SETTLE restores energy.|Wild recruitment works at half HP or after SOOTHE. Save from your journal.",FIELD_BACK);dialogReturn=TITLE;}
 }}else if(screen==CONFIRM){if(pressed&(LEFT|RIGHT))confirmCursor^=1;if(pressed&B)screen=TITLE;else if(pressed&A){if(confirmCursor)newgame();else screen=TITLE;}}
 else if(screen==FIELD)fieldinput();else if(screen==DIALOG)dialogue_input();else if(screen==CHOOSE){if(pressed&LEFT)cursor=(cursor+2)%3;if(pressed&RIGHT)cursor=(cursor+1)%3;if(pressed&A)choosepartner();}
 else if(screen==MENU)menuinput();else if(screen==TEAM)teaminput();else if(screen==GROWTH)growthinput();
 else if(screen==GUIDE){if(pressed&(B|START)){screen=MENU;cursor=1;}else{if(pressed&(L|R|LEFT|RIGHT))menuPage^=1;if(pressed&UP)cursor=(cursor+5)%6;if(pressed&DOWN)cursor=(cursor+1)%6;}}
 else if(screen==JOURNAL){if(pressed&(A|B|START)){screen=MENU;cursor=2;}}
 else if(screen==BATTLE)battleinput();else if(screen==ENDING){if(pressed&(A|START)){screen=FIELD;savegame();}}
}
int main(void){
 (void)saveType[0];REG16(0x04000208)=0;REG16(0x04000204)=0x4317;
 for(unsigned i=0;i<sizeof(palette)/2;i++)((volatile u16*)0x05000000)[i]=palette[i];
 REG16(0x04000000)=0x0404;REG16(0x04000084)=0x80;REG16(0x04000088)=0x0200;
 initworld();hasSave=loadgame();screen=TITLE;cursor=0;
 while(1){
  keys=(~REG16(0x04000130))&1023;pressed=keys&~previous;previous=keys;input();audio();render();
  while(REG16(0x04000006)>=160);while(REG16(0x04000006)<160);
  u32 dst=(REG16(0x04000000)&16)?0x06000000:0x0600a000;
  REG32(0x040000d4)=(u32)fb;REG32(0x040000d8)=dst;REG32(0x040000dc)=0x84000000|(sizeof(fb)/4);
  REG16(0x04000000)^=16;frame++;
 }
}
