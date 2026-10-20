/* Rising Tide: shared-garage story progression. Racing remains a future step. */
#include "career.h"
#include "story.h"
#include "gfx.h"
CareerUI campaign;
const CareerChapter career_chapters[5]={
 {"Saltblock Quarter","D / THE FIRST SET OF KEYS","CLOSED STREET CIRCUITS",
  "Nia's neighborhood garage is losing its lease. You enter local club nights to keep the lights on. The Coastline Cup offers a way out, but its promoter owns the ladder.",
  "Jules / The Courier","Calder / Block Captain","Late braking, short routes. Learn clean exits.","Defends the inside. Patience beats contact.",C(102,230,202)},
 {"Lantern Mile","C / A NAME AFTER DARK","CLOSED STREET CIRCUITS",
  "Your crew reaches the waterfront invitationals. Marisol offers a sponsor introduction, but only if you can prove the garage is more than one lucky driver.",
  "Imani / Night Shift","Marisol / Harbor Queen","Strong launches. Keep the first sector tidy.","Smooth in the wet. Save grip for the last lap.",C(255,180,106)},
 {"Foundry Ring","B / THE PRICE OF ENTRY","CLOSED STREET CIRCUITS",
  "Kade's exhibition win exposes a gap in your racecraft. Work the industrial clubs, study the lines, and discover why Voss keeps independent crews off the official grid.",
  "Kade / The Benchmark","Rook / Iron Crown","Fast exits. A clean rematch, no tricks.","Relentless pace. Build a plan, not a miracle.",C(239,139,170)},
 {"Meridian Park","A / THROUGH THE GATES","PERMANENT ROAD COURSES",
  "A club license opens permanent circuits. Ren helps the crew learn qualifying and strategy. Aster guards the final invitation while Voss tries to buy your team name.",
  "Ren / Sector Ghost","Aster / The Gatekeeper","Precise sectors. Consistency wins respect.","Controls race pace. Think beyond one fast lap.",C(153,198,255)},
 {"Pelican Grand Prix","S / A PLACE ON THE GRID","PERMANENT ROAD COURSES",
  "The Coastline Cup reaches its final road courses. Sable wants a fair fight. Voss wants ownership. Win on your own terms and return the garage to the people who built it.",
  "Sable / Silver Apex","Voss / The Promoter","Complete driver. Every sector matters.","The final contract. Keep the crew independent.",C(231,210,144)}
};
static const char*const career_names[5][25]={
 {"First Light","Block Party","Corner Store","Borrowed Time","After Hours","Wrong League","Back to Basics","A Clean Exit","Rent Money","Night Deposit","Courier's Call","Special Delivery","The Long Way","Open Shutters","Small Victories","Dockside Double","Two Good Laps","Crew Business","Lamp Post Run","Local Favorite","The Last Envelope","Home Advantage","One More Night","Before the Captain","Our Block"},
 {"Lantern Welcome","Harbor Lights","First Impression","Window Shopping","The Invitation","No Shortcuts","Rain Check","Neon Receipt","Quiet Confidence","A Better Line","Night Shift Call","Change of Shift","Sponsor's Table","Earn Your Seat","Bridge Club","Pocket Change","Clean Credentials","Coast After Dark","Closing the Gap","The Harbor List","A Place to Belong","Last Ferry","Queen's Invitation","One Calm Lap","Harbor Crown"},
 {"Industrial Action","The Benchmark","A Heavy Lesson","Work the Line","Cold Workshop","Training Nights","Brake Markers","Factory Reset","Steel and Patience","Honest Pace","Kade Calls Back","No Excuses","A Handshake","Closed Doors","The Buyout","Independent Work","The Long Shift","Proof of Pace","Crew Before Cash","Paper Trail","Iron Invitation","Hold the Line","Last Practice","No More Favors","Break the Ring"},
 {"New Credentials","First Session","Sector School","Out Lap","The Grid","A Different Rhythm","Late Apex","Clean Sectors","Practice Pays","Paddock Pass","Ren's Reference","Sector Ghost","The Offer","Crew Meeting","Decline to Sign","Strategy School","Tire Window","Long Game","Race Engineer","Private Test","Aster's List","One Clear Lap","The Gate Opens","Final Inspection","Through the Gates"},
 {"Grand Arrival","First Qualifying","Full Commitment","A Clean Start","Silver Standard","Pressure Test","Long Stint","Changing Skies","Under Review","Crew Radio","Sable's Challenge","Silver Apex","Terms and Conditions","No Sale","The Final Series","Fuel for Thought","Track Position","Late Safety Car","Every Sector","The Open Contract","The Last Invitation","Cup Qualifying","Our Own Name","Before the Flag","Rising Tide"}
};
static const char*const career_beats[5][5]={
 {"Sunline Garage needs steady work and another driver. Nia lends you the small keys; Calder's club offers a first evening on the grid.",
  "Kade's exhibition exposed the way one untidy corner spoiled the next. Five short club heats give the crew time to practice and pay the ordinary bills.",
  "Jules became the crew's first racing ally. His courier circuit taught you to join the corners instead of treating each one as a separate argument.",
  "People are gathering at Sunline after the heats. The garage is becoming somewhere to belong, as well as somewhere that needs its bills paid.",
  "Calder recommended Sunline to Lantern Mile. Nia chose to keep the garage open. The waterfront invitation starts the next part of the journey."},
 {"Marisol's waterfront clubs run on work, favors, and people who return borrowed equipment. Imani expects a little common sense before she offers her respect.",
  "The next club appearances are ordinary preparation: repeatable laps, rent money, and introductions. A sponsor conversation has to be earned before it can become a promise.",
  "Imani noticed the room you left her in the shift-change duel. Respect earned a conversation at the supplier's table, not a shortcut around the work.",
  "Reliable appearances matter to the harbor crews. Small heats and small favors keep Sunline ready for the final, without turning every evening into a crisis.",
  "Marisol signed the Foundry recommendation. The licensing application asks who controls Sunline's identity. Nia plans to read that part carefully."},
 {"Foundry Ring is a step beyond the harbor clubs. Kade invited a benchmark exhibition, and Rook asked Sunline to learn how her clubs work.",
  "Kade's second exhibition made the problem clear: a fast start was being wasted by an untidy next corner. Regular practice sessions make improvement repeatable.",
  "The clean rematch earned Kade's respect and a copy of the licensing rules. The next gate requires a financial guarantee, not just a fast lap.",
  "Voss offered security in exchange for control of the team name. The crew is weighing the actual budget while ordinary club work pays for tomorrow.",
  "Rook's club joined Calder and Marisol in guaranteeing an independent application. Sunline earned a Meridian invitation while keeping its door open."},
 {"Meridian has pit lanes, marked procedures, and a very practical mentor named Ren. The crew has a different environment to learn, not a different garage to buy.",
  "Practice and qualifying sessions are teaching repeatable sectors. Mina's paid photography work is her own; being part of Sunline doesn't make anyone its property.",
  "Ren's sector duel earned a sporting recommendation. The crew now has someone on the pit wall who can turn a driver's observations into a useful plan.",
  "The crew declined the team-name agreement after reading the budget together. Longer sessions now prepare them for Aster's final and the real costs of a Cup entry.",
  "Aster signed the independent Cup entry. A sporting license and a commercial contract are separate documents. Sunline will arrive under its own name."},
 {"Sunline has a place in the Cup paddock. Sable wants a proper contest and a more interesting weekend. Nia wants the crew to remember the ordinary work that brought them here.",
  "Longer sessions ask the crew to plan for changing conditions. The answer is preparation and rest, not a new crisis every night.",
  "Sable backed a statement asking for fair independent terms after the duel. The request concerns real guarantees and working crews, not an exemption from the costs.",
  "The final series is demanding, ordinary work: sessions, notes, paid jobs, and enough sleep to make the next decision properly.",
  "Sunline finished the Cup under its own name. The guarantee became a readable independent agreement. Back at the garage, another unknown driver is waiting by the open door."}
};
static const char*const plan_names[5]={"Race licenses","Sponsor offers","Practice / ghosts","Race-day choices","Crew reputation"};
const char*career_plan_name(int row){return plan_names[row%5];}
const char*career_plan(int chapter,int row){
 switch(row){
  case 0:return "IDEA: earn a sporting license at each chapter boss. Fleet purchase gates and campaign invitations remain separate systems.";
  case 1:return "IDEA: optional clean-driving or podium clauses for bonus cash. No sponsor-exclusive vehicle parts, cosmetics or paid currency.";
  case 2:return "IDEA: free practice, personal-best ghosts and a suggested target lap. Practice should never charge an entry fee.";
  case 3:return chapter<3?"IDEA: choose a short club event or a longer cash-building circuit. Keep traffic outside the closed racing route.":"IDEA: qualifying, weather forecasts and a simple pit window on permanent road courses. These are planning cards, not implemented mechanics.";
  default:return "IDEA: clean finishes and helping rival crews change dialogue and invitations. Reputation should not replace your shared cash or garage progression.";
 }
}
const char*career_story(int chapter,int beat){return career_beats[chapter%5][beat%5];}
const char*career_event_name(int chapter,int event){return career_names[chapter%5][event%25];}
int career_event_kind(int event){return event==24?2:event==11?1:0;}
const char*career_event_format(int chapter,int event){
 if(event==24)return "CHAPTER BOSS / CIRCUIT FINAL";
 if(event==11)return "MINI BOSS / RIVAL CIRCUIT";
 static const char*const street[5]={"CLUB CIRCUIT","CIRCUIT TIME TRIAL","CLEAN-RACE CHALLENGE","PRACTICE / CASH RUN","CLUB ENDURANCE"};
 static const char*const track[5]={"ROAD-COURSE RACE","QUALIFYING TRIAL","SECTOR CHALLENGE","OPEN PRACTICE","CIRCUIT ENDURANCE"};
 return chapter<3?street[event%5]:track[event%5];
}
void career_point(int chapter,int event,int*x,int*y){
 unsigned h=0x9e3779b9u*(unsigned)(event+17+chapter*31);h^=h>>16;h*=0x85ebca6bu;h^=h>>13;
 int row=event/5,col=event%5;if(row&1)col=4-col;
 *x=25+col*49+(int)(h%15)-7;*y=39+row*26+(int)((h>>8)%9)-4;
}
int career_status(const Game*g,int chapter,int event){return story_status(g,chapter,event);}
int career_journal_open(const Game*g,int chapter,int row){static const uint8_t limits[5]={0,6,12,18,25};int done=story_completed(g)-chapter*25;return row>=0&&row<5&&done>=limits[row];}
int career_rows(int tab){return tab==0?4:tab==2?3:tab==4?4:5;}
void career_reset(void){memset(&campaign,0,sizeof campaign);campaign.revision=1;}
void career_enter(Game*g){
 int done=story_completed(g);g->mode=M_STORY;g->cursor=0;campaign.tab=0;campaign.chapter=(uint8_t)(done<125?done/25:4);campaign.event=(uint8_t)(done<125?done%25:24);
 campaign.modal=0;campaign.garage_return=0;g->notice_timer=0;campaign.revision++;
}
static void career_message(Game*g,const char*text){size_t n=strlen(text);if(n>=sizeof g->notice)n=sizeof g->notice-1;memcpy(g->notice,text,n);g->notice[n]=0;g->notice_timer=120;campaign.revision++;}
void campaign_tick(Game*g,const Input*in){
 if(!in->pressed)return;
 if(in->pressed&K_B){if(campaign.modal)campaign.modal=0;else{g->mode=M_DRIVE;g->cursor=2;}campaign.revision++;return;}
 if(campaign.modal){
  int d=horizontal(in)+vertical(in);if(d)campaign.page=(uint8_t)((campaign.page+3+d)%3);
  if(in->pressed&K_A)campaign.page=(uint8_t)((campaign.page+1)%3);
  campaign.revision++;return;
 }
 int tab=((in->pressed&K_R)?1:0)-((in->pressed&K_L)?1:0);
 if(tab){campaign.tab=(uint8_t)((campaign.tab+5+tab)%5);g->cursor=0;campaign.revision++;return;}
 int dy=vertical(in),dx=horizontal(in),rows=career_rows(campaign.tab);
 g->cursor=(uint8_t)((g->cursor+rows+dy)%rows);
 if(campaign.tab==0){
  if(g->cursor==0&&(dx||(in->pressed&K_A))){campaign.chapter=(uint8_t)((campaign.chapter+5+(dx?dx:1))%5);int n=story_completed(g)-campaign.chapter*25;campaign.event=(uint8_t)(n<0?0:n>24?24:n);}
  if(g->cursor==1&&dx)campaign.event=(uint8_t)((campaign.event+25+dx)%25);
  if((in->pressed&K_A)&&(g->cursor==1||g->cursor==2)){
   if(!story_status(g,campaign.chapter,campaign.event)){career_message(g,"Complete the preceding story event first.");return;}
   if(!story_open(g,campaign.chapter,campaign.event))career_message(g,"Scene archive unavailable. Return and retry.");
   return;
  }
 }
 if(in->pressed&K_A){
  if((campaign.tab==0&&g->cursor==3)||(campaign.tab==4&&g->cursor==0)){
   campaign.garage_return=1;g->mode=M_GARAGE;g->garage_tab=0;g->cursor=0;g->selection=g->save.selected_vehicle;refresh_vehicle(g);
  }else if(campaign.tab==4&&g->cursor==2){career_enter(g);g->cursor=1;}
  else if(campaign.tab==1){
   if(!career_journal_open(g,campaign.chapter,g->cursor)){career_message(g,"This journal entry opens later in the story.");return;}
   campaign.modal=2;campaign.page=0;
  }else if(campaign.tab==2||campaign.tab==3||(campaign.tab==4&&(g->cursor==1||g->cursor==3))){campaign.modal=(uint8_t)(campaign.tab==4?5:campaign.tab+1);campaign.page=0;}
 }
 campaign.revision++;
}
