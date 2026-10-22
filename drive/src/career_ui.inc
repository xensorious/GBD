/* Original vector atlas. No extra framebuffer or touchscreen hit targets. */
static void career_dots(Surface*s,int x,int y,int xx,int yy,uint16_t color){
 int dx=xx-x,dy=yy-y,steps=(dx<0?-dx:dx)>(dy<0?-dy:dy)?(dx<0?-dx:dx):(dy<0?-dy:dy);
 for(int n=0;n<=steps;n+=4)if(steps)gfx_rect(s,x+dx*n/steps,y+dy*n/steps,2,1,color);
}
static void career_digit(Surface*s,int x,int y,int n,uint16_t color){
 static const uint8_t d[10][5]={{7,5,5,5,7},{2,6,2,2,7},{7,1,7,4,7},{7,1,7,1,7},{5,5,7,1,1},{7,4,7,1,7},{7,4,7,5,7},{7,1,1,1,1},{7,5,7,5,7},{7,5,7,1,7}};
 for(int yy=0;yy<5;yy++)for(int xx=0;xx<3;xx++)if(d[n][yy]&(4>>xx))gfx_rect(s,x+xx,y+yy,1,1,color);
}
void ui_draw_career_map(Surface*s,const Game*g){
 interface=g->save.skin>=SKIN_FUSION&&g->save.skin<SKIN_STORAGE_LIMIT?g->save.skin:SKIN_FUSION;t=&themes[interface-SKIN_FUSION];
 int chapter=campaign.chapter;const CareerChapter*c=&career_chapters[chapter];
 uint16_t land=mix(t->shore,C(40,85,83),80),street=mix(t->shadow,C(49,65,76),90),block=mix(land,t->edge,52);
 gradient(s,0,0,256,192,mix(land,t->sky,130),land);
 for(int y=28;y<158;y++){int shore=239+(y/17)%5;gfx_rect(s,shore,y,256-shore,1,C(28,92,119));int river=113+(y/18%3)*3;gfx_rect(s,river,y,10,1,C(31,91,106));}
 for(int row=0;row<7;row++)for(int col=0;col<9;col++){
  int x=5+col*27,y=30+row*19;unsigned h=(unsigned)(row*19+col*31+chapter*11);
  if(x>233)continue;
  if(h%7==0){gfx_rect(s,x+2,y+2,20,12,mix(land,C(66,140,100),125));disc(s,x+7,y+7,3,mix(land,C(104,175,117),110));}
  else{gfx_rect(s,x+2,y+3,19,11,mix(block,t->shadow,72));gfx_rect(s,x+1,y+1,18,10,mix(block,t->edge,(int)(h%4)*23));gfx_rect(s,x+3,y+2,12,1,mix(block,t->ink,50));}
 }
 for(int x=0;x<240;x+=27){gfx_rect(s,x,28,3,129,street);gfx_rect(s,x,28,1,129,mix(street,t->edge,65));}
 for(int y=28;y<158;y+=19){gfx_rect(s,0,y,239,3,street);gfx_rect(s,0,y,239,1,mix(street,t->edge,65));}
 if(chapter>=3){
  const int p[12][2]={{31,64},{61,46},{164,47},{205,62},{212,91},{186,103},{194,132},{143,144},{90,125},{60,137},{37,119},{31,64}};
  for(int i=0;i<11;i++){for(int k=-3;k<=3;k++)gfx_line(s,p[i][0],p[i][1]+k,p[i+1][0],p[i+1][1]+k,mix(land,t->edge,80));gfx_line(s,p[i][0],p[i][1],p[i+1][0],p[i+1][1],street);}
  gfx_rect(s,77,51,66,4,C(193,193,174));gfx_rect(s,83,56,52,5,mix(block,t->ink,60));
 }
 for(int i=0;i<24;i++){int x,y,xx,yy;career_point(chapter,i,&x,&y);career_point(chapter,i+1,&xx,&yy);career_dots(s,x,y,xx,yy,i<story_completed(g)-chapter*25?C(97,209,171):mix(t->muted,land,80));}
 for(int i=0;i<25;i++){
  int x,y,state=career_status(g,chapter,i),kind=career_event_kind(i);career_point(chapter,i,&x,&y);
  int r=kind==2?7:kind?6:5;uint16_t color=state==2?C(81,184,148):state==1?GOLD:C(100,126,139);
  if(i==campaign.event){gfx_circle(s,x,y,r+2,t->ink);gfx_circle(s,x,y,r+3,mix(c->accent,land,100));}
  disc(s,x,y,r+1,t->shadow);
  if(kind==2){gfx_line(s,x,y-r,x+r,y,color);gfx_line(s,x+r,y,x,y+r,color);gfx_line(s,x,y+r,x-r,y,color);gfx_line(s,x-r,y,x,y-r,color);}
  else{disc(s,x,y,r,state?color:mix(land,t->shadow,120));gfx_circle(s,x,y,r,color);}
  int n=i+1,tx=x-(n>9?3:1);uint16_t ink=state&&kind!=2?t->shadow:state==1?GOLD:t->ink;
  if(n>9){career_digit(s,tx,y-2,n/10,ink);tx+=4;}career_digit(s,tx,y-2,n%10,ink);
 }
 gfx_rect(s,0,0,256,27,t->shadow);gfx_text(s,9,3,c->name,t->ink);
 char title[40]="CLASS ";append(title,game_class_name(chapter),sizeof title);append(title," / 25 EVENTS",sizeof title);gfx_text_small(s,10,16,title,c->accent);right_small(s,247,16,story_completed(g)<chapter*25?"LOCKED":"STORY",t->muted);
 gfx_rect(s,0,159,256,33,t->shadow);
 disc(s,11,165,2,C(81,184,148));gfx_text_small(s,18,159,"CLEARED",t->muted);disc(s,84,165,2,GOLD);gfx_text_small(s,91,159,"CURRENT",t->muted);gfx_circle(s,167,165,2,C(100,126,139));gfx_text_small(s,174,159,"LOCKED",t->muted);
 char label[18]="";number(label,campaign.event+1,sizeof label);append(label," / ",sizeof label);gfx_text_small(s,9,176,label,c->accent);limited_small(s,38,176,209,career_event_name(chapter,campaign.event),t->ink);
}
static void campaign_row(Surface*s,const Game*g,int row_id,const char*label,const char*value){
 int y=39+row_id*23,sel=g->cursor==row_id;glass(s,5,y,246,21,sel);
 int width=value?small_width(value):0;limited_small(s,13,y+6,225-width-8,label,sel?t->selected_ink:t->ink);
 if(value)right_small(s,242,y+6,value,sel?t->selected_detail:t->accent);
}
static void campaign_portrait(Surface*s,int x,int y,int boss){
 uint16_t accent=career_chapters[campaign.chapter].accent;glass(s,x,y,48,52,0);
 disc(s,x+24,y+20,11,boss?C(176,135,112):C(213,164,126));disc(s,x+23,y+13,11,mix(t->shadow,accent,50));gfx_rect(s,x+14,y+18,20,4,boss?t->shadow:mix(accent,t->ink,110));
 gfx_rect(s,x+16,y+25,4,2,t->shadow);gfx_rect(s,x+29,y+25,4,2,t->shadow);gfx_rect(s,x+19,y+32,10,5,C(176,135,112));
 for(int j=0;j<12;j++)gfx_rect(s,x+11-j/2,y+38+j,27+j,1,mix(accent,t->shadow,100));
 gfx_line(s,x+17,y+39,x+24,y+48,t->edge);gfx_line(s,x+31,y+39,x+24,y+48,t->edge);
}
static void campaign_modal(Surface*s,const Game*g){
 int ch=campaign.chapter,event=campaign.event,page=campaign.page;const CareerChapter*c=&career_chapters[ch];
 const char*title="DESIGN NOTES",*text="";char meta[64]="";
 if(campaign.modal==1){
  title=page==0?"RACE BRIEFING":page==1?"STORY CONTEXT":"ENTRY PLAN";
  text=page==0?(event==11?"Proposed objective: beat the chapter's first named rival on a closed circuit. A scouting report introduces their driving style.":event==24?"The chapter final closes the story arc and opens the next chapter. This build plays its dialogue and story montage; the driving race is not implemented.":"Proposed objective: finish the club event cleanly. Time trials, practice and cash-building races give the crew room to prepare between major story beats."):
   page==1?(ch==0&&event==5?"Proposed story-only setback: a faster visitor wins an exhibition. No money is removed. The following practice nights set up a fair return to racing.":career_story(ch,event==11?2:event==24?4:event/5)):
   "Your garage is shared across every mode. Story scenes unlock the next event; they do not award racing cash or change your vehicles. Full races are the next development step.";
  number(meta,event+1,sizeof meta);append(meta," / ",sizeof meta);append(meta,career_event_name(ch,event),sizeof meta);
 }else if(campaign.modal==2){
  title=page==0?"CREW JOURNAL":page==1?"NIA / GARAGE RADIO":"CREW NOTES";
  text=page==0?career_story(ch,g->cursor):page==1?"We do not need every night to be a legend. Put in clean laps, pay the bills, and come home with a better plan. The name on this garage stays ours.":"Write down what went well, not just what went wrong. Bring the car home, pay people on time, and leave a chair for whoever comes through the door next.";
  append(meta,c->name,sizeof meta);
 }else if(campaign.modal==3){
  int boss=g->cursor==1;title=page==0?"RIVAL DOSSIER":page==1?"CHALLENGE PROPOSAL":"FAIR RACE RULES";
  text=page==0?(boss?c->boss_style:c->mini_style):page==1?(boss?"The chapter boss is event 25. Proposed stakes: a sporting invitation and a crew milestone. Cash and vehicle purchases still belong to the shared garage economy.":"The mini boss is event 12. Proposed stakes: a new ally, a scouting lesson and preparation for the area's final. No rival race is playable in this draft."):"Named rival encounters are part of the story. Exhibition setbacks explain practice blocks without taking cash, cars or upgrades. Future races will use fair, readable strengths.";
  append(meta,boss?c->boss:c->mini,sizeof meta);
 }else if(campaign.modal==4){
  title=page==0?"MECHANIC PROPOSAL":page==1?"WHERE IT FITS":"SHARED GARAGE";
  text=page==0?career_plan(ch,g->cursor):page==1?(ch<3?"D, C and B chapters stay on closed street circuits. Club races and free practice provide preparation before the two named rivals.":"A and S move onto permanent road courses. Qualifying, longer stints and pit decisions could add depth without replacing the core driving system."):"There is one fleet and one set of installed upgrades and cosmetics across every mode. Career adds event structure, story and sporting goals, not a duplicate customization shop.";
  append(meta,career_plan_name(g->cursor),sizeof meta);
 }else{
  title=page==0?"STORY RECORD":page==1?"SHARED PROGRESSION":"NEXT BUILD";
  text=page==0?"Rising Tide has 125 playable dialogue scenes. Finishing each scene unlocks the next event. Chapter finals open the following class chapter. Racing is not implemented yet.":page==1?"Story progress saves independently in each of your three profiles. Replaying a completed scene never grants more progress or money. Cars, installed parts and finishes remain shared across modes.":"Next decisions: circuit handling, race length, AI behavior, payouts, difficulty and which planning cards deserve full mechanics. Existing driving modes remain available.";
  append(meta,"RISING TIDE / STORY EDITION",sizeof meta);
 }
 glass(s,5,37,246,148,0);gfx_text_small(s,14,44,title,t->accent);limited(s,14,60,226,meta,t->ink);
 if(campaign.modal==3&&page==0){campaign_portrait(s,14,82,g->cursor==1);gfx_wrap(s,71,84,text,169,14,5,t->ink);}
 else gfx_wrap(s,14,79,text,226,12,7,t->ink);
 if(campaign.modal==1&&page==0){char cash[38]="PROPOSED PAYOUT ";money(cash,(uint32_t)((ch+1)*120+(event+1)*15),sizeof cash);gfx_text_small(s,14,161,cash,t->selected_detail);}
 for(int i=0;i<3;i++)disc(s,117+i*11,179,2,i==page?t->accent:t->muted);
}
static __attribute__((noinline)) void campaign_menu(Surface*s,const Game*g){
 static const char*const tabs[5]={"Route","Story","Rivals","Plans","Office"};
 for(int i=0;i<5;i++){glass(s,i*51,0,50,20,campaign.tab==i);center_small(s,i*51,6,50,tabs[i],campaign.tab==i?t->selected_ink:t->ink);}
 char money_text[24]="";money(money_text,g->save.cash,sizeof money_text);gfx_text_small(s,8,25,"RISING TIDE / STORY",t->muted);right_small(s,247,25,money_text,t->accent);
 if(campaign.modal){campaign_modal(s,g);return;}
 const CareerChapter*c=&career_chapters[campaign.chapter];
 if(campaign.tab==0){
  char level[20]="";number(level,campaign.event+1,sizeof level);append(level," / 25",sizeof level);
  campaign_row(s,g,0,c->name,game_class_name(campaign.chapter));campaign_row(s,g,1,career_event_name(campaign.chapter,campaign.event),level);
  campaign_row(s,g,2,"Play story scene",story_status(g,campaign.chapter,campaign.event)==2?"REPLAY":story_status(g,campaign.chapter,campaign.event)==1?"CONTINUE":"LOCKED");campaign_row(s,g,3,"Shared garage","OPEN");
  glass(s,5,135,246,50,0);gfx_text_small(s,13,140,c->surface,t->accent);limited_small(s,13,153,222,vehicles[g->save.selected_vehicle].name,t->ink);gfx_text_small(s,13,169,"Story scenes playable. Racing comes later.",t->muted);
 }else if(campaign.tab==1){
  static const char*const beats[5]={"Opening nights","Training / cash block","Event 12 / turning point","Preparing the final","Event 25 / next chapter"};
  for(int i=0;i<5;i++)campaign_row(s,g,i,beats[i],!career_journal_open(g,campaign.chapter,i)?"LOCKED":i==2?"12":i==4?"25":"READ");
  limited_small(s,12,163,232,c->premise,t->muted);
 }else if(campaign.tab==2){
  campaign_row(s,g,0,c->mini,"12");campaign_row(s,g,1,c->boss,"25");campaign_row(s,g,2,"Scouting / fair-race rules","IDEA");campaign_portrait(s,16,115,g->cursor==1);gfx_wrap(s,76,120,g->cursor==1?c->boss_style:c->mini_style,165,14,4,t->muted);
 }else if(campaign.tab==3){
  for(int i=0;i<5;i++)campaign_row(s,g,i,career_plan_name(i),"IDEA");
  gfx_text_small(s,12,164,"Proposals to shape the racing later.",t->muted);
 }else{
  char done[24]="";number(done,story_completed(g),sizeof done);append(done," / 125",sizeof done);
  campaign_row(s,g,0,"Shared garage","OPEN");campaign_row(s,g,1,"Campaign record",done);campaign_row(s,g,2,"Continue the story","OPEN");campaign_row(s,g,3,"What this build includes","READ");campaign_row(s,g,4,"Cast studio","28 PEOPLE");
  glass(s,5,158,246,27,0);gfx_text_small(s,13,164,"One shared garage. Your story stays saved.",t->muted);
 }
}
