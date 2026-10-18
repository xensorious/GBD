#include "ui.h"

#ifndef C
#define C(r,g,b) ((uint16_t)(0x8000u|((r)>>3)|(((g)>>3)<<5)|(((b)>>3)<<10)))
#endif

#define INK C(21,44,66)
#define NIGHT C(15,34,49)
#define DEEP C(30,59,76)
#define PAPER C(251,240,216)
#define WHITE C(255,250,236)
#define PALE C(224,221,198)
#define TEAL C(38,126,115)
#define MINT C(145,217,181)
#define GOLD C(237,189,104)
#define CORAL C(218,120,99)
#define MUTED C(113,135,142)
#define SOFT C(187,200,190)

enum { ICON_LEAF, ICON_FLAME, ICON_DROP, ICON_STONE, ICON_SPARK,
       ICON_TEAM, ICON_BOOK, ICON_COMPASS, ICON_MENU, ICON_ATTACK,
       ICON_SHIELD, ICON_HEART, ICON_LANTERN, ICON_SAVE, ICON_STAR };

static void append(char *out, const char *s, int capacity) {
    int n=0;
    while(out[n] && n<capacity-1) n++;
    while(*s && n<capacity-1) out[n++]=*s++;
    out[n]=0;
}
static void append_number(char *out, int value, int capacity) {
    char digits[12]; int n=0;
    if(value<0) { append(out,"-",capacity); value=-value; }
    do { digits[n++]=(char)('0'+value%10); value/=10; } while(value && n<11);
    while(n) { char c[2]={digits[--n],0}; append(out,c,capacity); }
}
static void centered(Surface *s,int x,int y,int width,const char *text,uint16_t color) {
    gfx_text(s,x+(width-gfx_text_width(text))/2,y,text,color);
}
static void limited(Surface *s,int x,int y,int width,const char *text,uint16_t color) {
    char buffer[100]=""; int n=0;
    while(text[n] && n<95) { buffer[n]=text[n]; buffer[++n]=0; if(gfx_text_width(buffer)>width) break; }
    if(text[n] || (n && gfx_text_width(buffer)>width)) {
        if(n) n--;
        buffer[n]=0;
        while(n && gfx_text_width(buffer)+gfx_text_width("...")>width) buffer[--n]=0;
        append(buffer,"...",sizeof buffer);
    }
    gfx_text(s,x,y,buffer,color);
}
static uint16_t element_color(int element) {
    static const uint16_t colors[5]={MINT,CORAL,C(132,197,223),C(207,185,144),GOLD};
    return colors[(element>=0 && element<5)?element:0];
}
static void disc(Surface *s,int x,int y,int radius,uint16_t color) {
    for(int dy=-radius;dy<=radius;dy++) {
        int dx=radius;
        while(dx>0 && dx*dx+dy*dy>radius*radius) dx--;
        gfx_rect(s,x-dx,y+dy,dx*2+1,1,color);
    }
}
static void diagonal(Surface *s,int x,int y,int length,int down,uint16_t color) {
    for(int i=0;i<length;i++) gfx_rect(s,x+i,y+(down?i:-i),2,2,color);
}
static void icon(Surface *s,int x,int y,int kind,uint16_t color) {
    switch(kind) {
    case ICON_LEAF:
        for(int i=0;i<10;i++) { int width=i<5?i+2:11-i; gfx_rect(s,x+7-width/2,y+2+i,width,1,color); }
        diagonal(s,x+3,y+13,8,0,color); break;
    case ICON_FLAME:
        gfx_rect(s,x+7,y+1,2,5,color); gfx_rect(s,x+5,y+5,6,3,color);
        gfx_rect(s,x+3,y+8,10,5,color); gfx_rect(s,x+5,y+13,6,2,color);
        gfx_rect(s,x+7,y+9,2,4,PAPER); break;
    case ICON_DROP:
        for(int i=0;i<8;i++) { int w=i/2+1; gfx_rect(s,x+8-w,y+1+i,w*2,1,color); }
        disc(s,x+7,y+10,5,color); gfx_rect(s,x+5,y+9,1,3,PAPER); break;
    case ICON_STONE:
        gfx_rect(s,x+4,y+3,8,2,color); gfx_rect(s,x+2,y+5,12,7,color);
        gfx_rect(s,x+4,y+12,8,2,color); diagonal(s,x+5,y+6,4,1,PAPER); break;
    case ICON_SPARK:
        for(int i=0;i<7;i++) { int w=i<4?i+1:7-i; gfx_rect(s,x+8-w,y+1+i,w*2-1,1,color); }
        for(int i=0;i<7;i++) { int w=i<3?i+1:7-i; gfx_rect(s,x+8-w,y+8+i,w*2-1,1,color); }
        gfx_rect(s,x+1,y+7,14,2,color); break;
    case ICON_TEAM:
        disc(s,x+6,y+5,3,color); disc(s,x+12,y+6,2,color);
        gfx_rect(s,x+2,y+10,8,5,color); gfx_rect(s,x+11,y+10,4,4,color); break;
    case ICON_BOOK:
        gfx_rect(s,x+2,y+3,12,11,color); gfx_rect(s,x+3,y+2,4,11,PAPER);
        gfx_rect(s,x+9,y+2,4,11,PAPER); gfx_rect(s,x+7,y+3,2,12,color);
        gfx_rect(s,x+4,y+5,2,1,color); gfx_rect(s,x+10,y+5,2,1,color); break;
    case ICON_COMPASS:
        disc(s,x+8,y+8,7,color); disc(s,x+8,y+8,5,NIGHT);
        diagonal(s,x+4,y+12,8,0,color); gfx_rect(s,x+7,y+1,2,3,color); break;
    case ICON_MENU:
        for(int i=0;i<3;i++) { gfx_rect(s,x+3,y+3+i*5,2,2,color); gfx_rect(s,x+7,y+3+i*5,7,2,color); } break;
    case ICON_ATTACK:
        diagonal(s,x+4,y+13,10,0,color); diagonal(s,x+5,y+12,9,0,color);
        diagonal(s,x+3,y+8,6,1,color); gfx_rect(s,x+11,y+1,3,2,color); break;
    case ICON_SHIELD:
        gfx_rect(s,x+3,y+2,10,7,color); gfx_rect(s,x+4,y+9,8,3,color);
        gfx_rect(s,x+6,y+12,4,2,color); gfx_rect(s,x+7,y+4,2,6,PAPER); break;
    case ICON_HEART:
        disc(s,x+5,y+5,3,color); disc(s,x+11,y+5,3,color);
        for(int i=0;i<8;i++) gfx_rect(s,x+2+i/2,y+6+i,13-i,1,color);
        break;
    case ICON_LANTERN:
        gfx_rect(s,x+6,y+1,5,2,color); gfx_rect(s,x+4,y+4,9,2,color);
        gfx_rect(s,x+4,y+6,2,7,color); gfx_rect(s,x+11,y+6,2,7,color);
        gfx_rect(s,x+7,y+7,3,5,GOLD); gfx_rect(s,x+4,y+13,9,2,color); break;
    case ICON_SAVE:
        gfx_rect(s,x+2,y+2,13,13,color); gfx_rect(s,x+5,y+2,6,5,PAPER);
        gfx_rect(s,x+5,y+10,7,5,PAPER); gfx_rect(s,x+9,y+3,1,3,color); break;
    default:
        gfx_rect(s,x+7,y+1,2,13,color); gfx_rect(s,x+1,y+7,14,2,color);
        diagonal(s,x+3,y+3,10,1,color); diagonal(s,x+3,y+12,10,0,color); break;
    }
}
static void backdrop(Surface *s) {
    gfx_rect(s,0,0,256,192,NIGHT);
    for(int y=0;y<192;y+=4) gfx_rect(s,0,y,256,1,(y&4)?C(17,37,52):C(19,39,54));
    gfx_rect(s,0,0,3,192,DEEP); gfx_rect(s,253,0,3,192,DEEP);
    gfx_rect(s,10,189,236,1,DEEP);
}
static void header(Surface *s,const char *title,const char *right) {
    gfx_text(s,13,9,title,PAPER);
    if(right) gfx_text(s,243-gfx_text_width(right),10,right,MINT);
    gfx_rect(s,13,26,230,1,DEEP); gfx_rect(s,13,26,29,1,GOLD);
}
static void footer(Surface *s,const char *left,const char *right) {
    if(left) gfx_text(s,11,181,left,SOFT);
    if(right) gfx_text(s,245-gfx_text_width(right),181,right,SOFT);
}
static void meter(Surface *s,int x,int y,int width,int value,int max,uint16_t color) {
    if(max<1) max=1;
    if(value<0) value=0;
    if(value>max) value=max;
    gfx_card(s,x,y,width,5,DEEP,DEEP);
    int filled=(width-2)*value/max;
    if(filled) gfx_rect(s,x+1,y+1,filled,3,color);
    if(filled>2) gfx_rect(s,x+1,y+1,filled,1,PAPER);
}
static void button(Surface *s,int x,int y,int w,int h,const char *label,int symbol,int selected,uint16_t accent) {
    gfx_card(s,x,y+2,w,h,NIGHT,NIGHT);
    gfx_card(s,x,y,w,h,selected?TEAL:DEEP,selected?GOLD:C(62,93,103));
    gfx_rect(s,x+5,y+3,w-10,1,selected?MINT:C(73,105,111));
    icon(s,x+8,y+(h-16)/2,symbol,selected?PAPER:accent);
    gfx_text(s,x+29,y+(h-10)/2,label,PAPER);
}
static void pill(Surface *s,int x,int y,int w,const char *label,uint16_t color) {
    gfx_card(s,x,y,w,15,color,color); centered(s,x,y+2,w,label,INK);
}
static void tabs(Surface *s,const Game *g) {
    static const char *const names[5]={"Map","Party","Journal","Guide","System"};
    static const int widths[5]={43,43,52,44,58};
    int x=8;
    for(int i=0;i<5;i++) {
        int chosen=g->ui_tab==i;
        gfx_card(s,x,4,widths[i]-2,23,chosen?TEAL:DEEP,chosen?GOLD:DEEP);
        centered(s,x,11,widths[i]-2,names[i],chosen?PAPER:SOFT);
        if(chosen) gfx_rect(s,x+9,24,widths[i]-20,1,MINT);
        x+=widths[i];
    }
}
static void panel_header(Surface *s,const Game *g,const char *title,const char *right) {
    if(g->ui_focus) tabs(s,g);
    else header(s,title,right);
}

static void title_screen(Surface *s,const Game *g) {
    header(s,"The Lantern Isles","ASTERWILD");
    disc(s,128,56,21,DEEP); disc(s,128,56,17,INK);
    icon(s,120,48,ICON_LANTERN,GOLD);
    centered(s,0,84,256,"Every bond begins somewhere.",PAPER);
    button(s,24,108,208,31,g->has_save?"Continue journey":"New expedition",g->has_save?ICON_BOOK:ICON_COMPASS,g->title_cursor==0,GOLD);
    if(g->has_save) button(s,24,146,208,29,"New expedition",ICON_COMPASS,g->title_cursor==1,MINT);
    else centered(s,0,153,256,"A: Begin your journey",SOFT);
    if(g->new_confirm) {
        gfx_card(s,16,91,224,87,PAPER,GOLD);
        centered(s,20,101,216,"Replace your saved journey?",INK);
        centered(s,20,119,216,"This begins a new expedition.",TEAL);
        button(s,27,144,97,24,"A: New",ICON_COMPASS,1,GOLD);
        button(s,132,144,97,24,"B: Keep",ICON_BOOK,0,MINT);
    }
    footer(s,"A: Choose",g->has_save?"Up / Down: Select":0);
}

static void field_screen(Surface *s,const Game *g) {
    panel_header(s,g,game_location(g),"ISLAND MAP");
    gfx_card(s,33,28,190,137,PAPER,C(184,163,121));
    gfx_map(s,38,29,180,135,g);
    if(g->ui_focus) {
        gfx_card(s,51,33,154,16,INK,TEAL);
        centered(s,53,36,150,game_location(g),PAPER);
    }
    icon(s,227,36,ICON_COMPASS,GOLD);
    gfx_text_small(s,232,55,"N",MINT);
    for(int i=0;i<7;i++) {
        gfx_rect(s,20-(i%2?0:4),57+i*12,i%2?3:7,1,C(68,94,99));
        gfx_rect(s,234,72+i*10,i%2?3:7,1,C(68,94,99));
    }
    gfx_card(s,9,148,238,29,NIGHT,DEEP);
    gfx_rect(s,15,154,2,16,GOLD);
    gfx_wrap(s,23,153,game_objective(g),216,11,2,PAPER);
    if(g->toast_timer) {
        gfx_card(s,12,137,232,27,TEAL,MINT);
        centered(s,16,146,224,g->toast,PAPER);
    }
    footer(s,g->ui_focus?"L / R: Tabs":"Select: Focus map",g->ui_focus?"Select: World":"A: Interact");
}

static void dialogue_screen(Surface *s,const Game *g) {
    header(s,g->dialog.speaker[0]?g->dialog.speaker:"Lantern Isles","CONVERSATION");
    gfx_card(s,12,38,232,99,PAPER,C(192,170,127));
    gfx_rect(s,20,47,3,13,TEAL);
    if(g->dialog.count) gfx_wrap(s,31,49,g->dialog.lines[g->dialog.index],201,13,6,INK);
    int dots=g->dialog.count<8?g->dialog.count:8;
    for(int i=0;i<dots;i++) disc(s,20+i*9,151,2,i==g->dialog.index?GOLD:DEEP);
    button(s,139,145,105,30,g->dialog.index+1<g->dialog.count?"Continue":"Let's go",ICON_SPARK,1,MINT);
    footer(s,"A: Continue",0);
}

static void starter_screen(Surface *s,const Game *g) {
    header(s,"Meet your first companion","01 / BEGIN");
    for(int i=0;i<3;i++) {
        int x=i==2?172:7+i*82,chosen=g->cursor==i;
        const Species *p=&game_species[i];
        gfx_card(s,x,36,76,115,chosen?PAPER:DEEP,chosen?GOLD:C(64,96,104));
        gfx_rect(s,x+5,39,66,2,chosen?TEAL:element_color(p->element));
        gfx_creature(s,x+3,43,i,70,0,(g->frame/28)&1);
        centered(s,x,115,76,p->name,chosen?INK:PAPER);
        pill(s,x+9,132,58,game_element_names[p->element],element_color(p->element));
    }
    button(s,47,158,162,24,"Choose companion",ICON_HEART,1,GOLD);
    footer(s,"Left / Right: Meet","A: Choose");
}

static void battle_screen(Surface *s,const Game *g) {
    const Battle *b=&g->battle;
    if(b->phase!=BP_COMMAND) {
        header(s,b->is_trial?"The lantern trial":"A wild encounter","BATTLE");
        gfx_card(s,12,39,232,92,PAPER,C(192,170,127));
        icon(s,23,50,b->result==BR_RECRUIT?ICON_HEART:ICON_SPARK,TEAL);
        if(b->log_count) gfx_wrap(s,49,51,b->logs[b->log_index],181,13,5,INK);
        button(s,139,144,105,31,"Continue",ICON_SPARK,1,MINT);
        footer(s,"A: Continue",0);
        return;
    }
    header(s,"Choose your move",b->is_trial?"TRIAL":"WILD");
    gfx_card(s,8,32,240,16,DEEP,DEEP);
    gfx_text(s,15,35,"Next:",GOLD);
    limited(s,47,35,192,game_intent_name(b),PAPER);
    static const char *const names[4]={"Strike","Skill","Guard","Befriend"};
    static const char *const notes[4]={"Reliable attack","Spend 2 energy","Recover energy","Offer a bond"};
    static const int icons[4]={ICON_ATTACK,ICON_SPARK,ICON_SHIELD,ICON_HEART};
    for(int i=0;i<4;i++) {
        int x=i&1?132:8,y=i&2?110:52;
        int selected=g->cursor==i;
        uint16_t accent=i==0?GOLD:i==1?MINT:i==2?C(132,197,223):CORAL;
        gfx_card(s,x,y+2,116,50,NIGHT,NIGHT);
        gfx_card(s,x,y,116,50,selected?PAPER:DEEP,selected?GOLD:C(65,99,106));
        disc(s,x+18,y+17,11,selected?TEAL:INK);
        icon(s,x+10,y+9,icons[i],selected?PAPER:accent);
        const char *name=names[i];
        if(i==1 && g->save.party_count) name=game_species[g->save.party[g->save.lead].species].skill;
        limited(s,x+34,y+12,77,name,selected?INK:PAPER);
        limited(s,x+9,y+34,99,notes[i],selected?TEAL:SOFT);
    }
    if(g->save.party_count) {
        const Companion *c=&g->save.party[g->save.lead];
        gfx_text(s,11,166,"Energy",SOFT);
        for(int i=0;i<5;i++) gfx_card(s,46+i*8,168,5,5,i<c->energy?GOLD:DEEP,i<c->energy?GOLD:MUTED);
    }
    footer(s,"X: Team",b->is_trial?"A: Choose":"B: Retreat");
}

static void menu_screen(Surface *s,const Game *g) {
    panel_header(s,g,"Expedition satchel","PAUSED");
    static const char *const labels[5]={"Your companions","Travel journal","Field guide","Save your journey","Return to the trail"};
    static const int icons[5]={ICON_TEAM,ICON_BOOK,ICON_COMPASS,ICON_SAVE,ICON_LANTERN};
    for(int i=0;i<5;i++) button(s,16,40+i*26,224,22,labels[i],icons[i],g->cursor==i,i==3?GOLD:MINT);
    footer(s,"L / R: Tabs   A: Open","Select: World");
}

static void party_screen(Surface *s,const Game *g) {
    char stock[24]="Tonics: "; append_number(stock,g->save.tonics,sizeof stock);
    if(g->menu_from_battle) header(s,"Choose your next companion",stock);
    else panel_header(s,g,"Your companions",stock);
    for(int i=0;i<GAME_PARTY_MAX;i++) {
        int y=32+i*22,selected=g->cursor==i;
        gfx_card(s,9,y,238,21,selected?PAPER:DEEP,selected?GOLD:DEEP);
        if(i>=g->save.party_count) { gfx_text(s,29,y+6,"A new friendship awaits",C(91,117,123)); continue; }
        const Companion *c=&g->save.party[i];
        gfx_creature(s,10,y-1,c->species,25,0,0);
        if(i==g->save.lead) { gfx_rect(s,10,y+6,2,7,TEAL); gfx_rect(s,12,y+8,2,3,TEAL); }
        limited(s,38,y+5,92,game_species[c->species].name,selected?INK:PAPER);
        char level[16]="Lv."; append_number(level,c->level,sizeof level);
        gfx_text(s,135,y+5,level,selected?TEAL:MINT);
        meter(s,168,y+6,67,c->hp,game_max_hp(c),c->hp*3<game_max_hp(c)?CORAL:MINT);
        char hp[20]=""; append_number(hp,c->hp,sizeof hp); append(hp,"/",sizeof hp); append_number(hp,game_max_hp(c),sizeof hp);
        gfx_text(s,235-gfx_text_width(hp),y+12,hp,selected?INK:SOFT);
    }
    char tonic[24]="X: Tonic ("; append_number(tonic,g->save.tonics,sizeof tonic); append(tonic,")",sizeof tonic);
    footer(s,tonic,g->menu_from_battle?"A: Switch  B: Back":"A: Lead  Select: World");
}

static void journal_screen(Surface *s,const Game *g) {
    panel_header(s,g,"Travel journal","LANTERN ISLES");
    gfx_card(s,12,35,232,131,PAPER,C(192,170,127));
    gfx_rect(s,24,43,2,113,C(223,168,141));
    for(int y=57;y<162;y+=16) gfx_rect(s,18,y,220,1,C(227,218,191));
    static const char *const tasks[6]={"Meet Mira at the conservatory","Choose your first companion","Speak with Jun on the trail","Befriend a wild creature","Help Edda's moonflower garden","Complete Vale's lantern trial"};
    const unsigned flags[6]={PF_STARTED,PF_CHOSEN,PF_MET_JUN,PF_FIRST_FRIEND,PF_GARDEN_DONE,PF_TRIAL_WON};
    for(int i=0;i<6;i++) {
        int y=43+i*19,done=(g->save.flags&flags[i])!=0;
        disc(s,37,y+5,4,done?TEAL:C(217,209,180));
        if(done) { gfx_rect(s,35,y+5,2,2,PAPER); gfx_rect(s,37,y+3,2,3,PAPER); }
        limited(s,48,y+1,185,tasks[i],done?TEAL:INK);
    }
    footer(s,"L / R: Tabs","Select: World");
}

static void guide_screen(Surface *s,const Game *g) {
    int species=g->cursor%GAME_SPECIES_COUNT;
    const Species *p=&game_species[species];
    char count[20]=""; append_number(count,species+1,sizeof count); append(count," / 6",sizeof count);
    panel_header(s,g,"The living field guide",count);
    gfx_card(s,10,35,236,132,PAPER,C(192,170,127));
    int seen=(g->save.seen&(1u<<species))!=0;
    if(seen) gfx_creature(s,10,36,species,85,0,(g->frame/28)&1);
    else { disc(s,53,78,32,PALE); icon(s,45,70,ICON_COMPASS,TEAL); }
    gfx_text(s,99,45,seen?p->name:"Unknown creature",INK);
    pill(s,99,63,70,seen?game_element_names[p->element]:"Unknown",seen?element_color(p->element):PALE);
    limited(s,99,84,135,seen?p->epithet:"A page still unwritten",TEAL);
    gfx_text(s,99,103,(g->save.recruited&(1u<<species))?"Bond recorded":seen?"Seen in the wild":"Yet to meet",MUTED);
    gfx_rect(s,23,123,210,1,C(218,206,174));
    gfx_wrap(s,22,132,seen?p->description:"Meet the island's wild creatures to fill these pages with discoveries.",214,11,3,INK);
    footer(s,"Left / Right: Browse","Select: World");
}

static void growth_screen(Surface *s,const Game *g) {
    header(s,"A new way to grow","A CHOICE, NOT A LOCK");
    int slot=g->growth_slot<g->save.party_count?g->growth_slot:0;
    int species=g->save.party[slot].species;
    for(int i=0;i<2;i++) {
        int x=i?132:8,selected=g->cursor==i;
        gfx_card(s,x,47,116,106,selected?PAPER:DEEP,selected?GOLD:C(64,96,104));
        gfx_creature(s,x+30,52,species,56,0,(g->frame/28)&1);
        centered(s,x,111,116,game_growth_name(i+1),selected?INK:PAPER);
        gfx_text(s,x+9,128,i==0?"Power +3":"HP +8 / Defense +3",selected?TEAL:SOFT);
        gfx_text(s,x+9,140,i==0?"Decisive attacks":"Steady protection",selected?TEAL:SOFT);
    }
    button(s,50,160,156,22,"Choose this path",ICON_LEAF,1,MINT);
    footer(s,"Left / Right: Explore","A: Choose");
}

static void ending_screen(Surface *s,const Game *g) {
    header(s,"The first lantern is yours","FIRST LIGHT");
    gfx_card(s,15,36,226,105,PAPER,C(192,170,127));
    disc(s,128,65,20,TEAL); disc(s,128,65,16,INK); icon(s,120,57,ICON_LANTERN,GOLD);
    centered(s,20,94,216,"A beginning worth remembering.",INK);
    char record[80]=""; append_number(record,g->save.party_count,sizeof record); append(record," companions. ",sizeof record);
    append_number(record,g->save.battles_won,sizeof record); append(record," battles won.",sizeof record);
    centered(s,20,113,216,record,TEAL);
    button(s,42,150,172,29,"Return to the island",ICON_COMPASS,1,MINT);
    footer(s,"A: Keep exploring",0);
}

void ui_draw_bottom(Surface *s,const Game *g) {
    backdrop(s);
    switch(g->mode) {
    case GM_TITLE: title_screen(s,g); break;
    case GM_FIELD: field_screen(s,g); break;
    case GM_DIALOG: dialogue_screen(s,g); break;
    case GM_STARTER: starter_screen(s,g); break;
    case GM_BATTLE: battle_screen(s,g); break;
    case GM_MENU: menu_screen(s,g); break;
    case GM_PARTY: party_screen(s,g); break;
    case GM_JOURNAL: journal_screen(s,g); break;
    case GM_GUIDE: guide_screen(s,g); break;
    case GM_GROWTH: growth_screen(s,g); break;
    case GM_ENDING: ending_screen(s,g); break;
    }
}
