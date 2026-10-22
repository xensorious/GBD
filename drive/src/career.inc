/* Rising Tide: shared-garage story progression. Racing remains a future step. */
#include "career.h"
#include "story.h"
#include "gfx.h"
CareerUI campaign;
const CareerChapter career_chapters[5]={
 {"Saltblock Quarter","D / THE FIRST SET OF KEYS","CLOSED STREET CIRCUITS",
  "Sunline's lease renewal is approaching. Nia offers a garage bay and a place in Calder's club. Ordinary race nights can keep the lights on while the crew decides what to build.",
  "Jules / The Courier","Calder / Block Captain","Late braking, short routes. Learn clean exits.","Defends the inside. Patience beats contact.",C(102,230,202)},
 {"Lantern Mile","C / A NAME AFTER DARK","CLOSED STREET CIRCUITS",
  "Your crew reaches the waterfront invitationals. Marisol offers a sponsor introduction, but only if you can prove the garage is more than one lucky driver.",
  "Imani / Night Shift","Marisol / Harbor Queen","Strong launches. Keep the first sector tidy.","Smooth in the wet. Save grip for the last lap.",C(255,180,106)},
 {"Foundry Ring","B / THE PRICE OF ENTRY","CLOSED STREET CIRCUITS",
  "Kade's exhibition exposes a gap in your racecraft. Study the industrial circuits while Rook helps the crew understand the costs and guarantees behind a Meridian license.",
  "Kade / The Benchmark","Rook / Iron Crown","Fast exits. A clean rematch, no tricks.","Relentless pace. Build a plan, not a miracle.",C(239,139,170)},
 {"Meridian Park","A / THROUGH THE GATES","PERMANENT ROAD COURSES",
  "A club license opens permanent circuits. Ren helps the crew learn qualifying and strategy. Aster guards the final invitation while Voss tries to buy your team name.",
  "Ren / Sector Ghost","Aster / The Gatekeeper","Precise sectors. Consistency wins respect.","Controls race pace. Think beyond one fast lap.",C(153,198,255)},
 {"Pelican Grand Prix","S / A PLACE ON THE GRID","PERMANENT ROAD COURSES",
  "A sporting invitation opens the Cup paddock. Sable wants a fair contest; Voss offers a commercial route through the larger season costs. The crew must agree on a workable alternative.",
  "Sable / Silver Apex","Voss / The Promoter","Complete driver. Every sector matters.","Patient exits. The same marshals for both drivers.",C(231,210,144)}
};
static const char*const career_names[5][25]={
 {"First Light","Block Party","Corner Store","Borrowed Time","After Hours","Wrong League","Back to Basics","A Clean Exit","Rent Money","Night Deposit","Courier's Call","Special Delivery","The Long Way","Open Shutters","Small Victories","Dockside Double","Two Good Laps","Crew Business","Moving Markers","Local Favorite","The Last Envelope","Home Advantage","One More Night","Before the Captain","Our Block"},
 {"Lantern Welcome","Harbor Lights","First Impression","Window Shopping","The Invitation","No Shortcuts","Rain Check","Neon Receipt","Quiet Confidence","A Better Line","Night Shift Call","Change of Shift","Sponsor's Table","Earn Your Seat","Bridge Club","Pocket Change","Clean Credentials","Coast After Dark","Closing the Gap","The Harbor List","A Place to Belong","Last Ferry","Queen's Invitation","One Calm Lap","Harbor Crown"},
 {"Industrial Action","The Benchmark","A Heavy Lesson","Work the Line","Cold Workshop","Training Nights","Brake Markers","Factory Reset","Steel and Patience","Honest Pace","Kade Calls Back","No Excuses","A Handshake","Closed Doors","The Buyout","Independent Work","The Long Shift","Proof of Pace","Crew Before Cash","Paper Trail","Iron Invitation","Hold the Line","Last Practice","No More Favors","Break the Ring"},
 {"New Credentials","First Session","Sector School","Out Lap","The Grid","A Different Rhythm","Late Apex","Clean Sectors","Practice Pays","Paddock Pass","Ren's Reference","Sector Ghost","The Offer","Crew Meeting","Decline to Sign","Strategy School","Tire Window","Long Game","Race Engineer","Private Test","Aster's List","One Clear Lap","The Gate Opens","Final Inspection","Through the Gates"},
 {"Grand Arrival","First Qualifying","Full Commitment","A Clean Start","Silver Standard","Pressure Test","Long Stint","Changing Skies","Under Review","Crew Radio","Sable's Challenge","Silver Apex","Terms and Conditions","No Sale","The Final Series","Fuel for Thought","Track Position","Late Safety Car","Every Sector","The Open Contract","The Last Invitation","Cup Qualifying","Our Own Name","Before the Flag","Rising Tide"}
};
static const char*const career_beats[5][5]={
 {"Sunline needs steady work and another driver. Mrs. Vale, Eli and Calder are part of the neighborhood behind its open door.",
  "Kade's exhibition exposed the way one untidy corner spoiled the next. Five short club heats give the crew time to practice and pay the ordinary bills.",
  "Jules became the crew's first racing ally. His courier circuit taught you to join the corners instead of treating each one as a separate argument.",
  "Jules showed you the invoices behind his jokes. Mina's pictures keep the ordinary evenings. Belonging can't mean forgetting what people need outside the garage.",
  "Calder recommended Sunline to Lantern Mile. Nia chose to keep the garage open. The waterfront invitation starts the next part of the journey."},
 {"Marisol's waterfront clubs run on work, favors, and people who return borrowed equipment. Imani expects a little common sense before she offers her respect.",
  "Lantern's clubs grow out of shifts, borrowed equipment and evenings people choose to share. The work matters, but it isn't everybody's whole life.",
  "Imani noticed the room you left her in the shift-change duel. Respect earned a conversation at the supplier's table, not a shortcut around the work.",
  "Returned awnings and honest small promises earned the harbor crews' trust. Mina wants her photographs to have a name of their own.",
  "Marisol signed the Foundry recommendation. Voss's separate commercial offer asks for control of Sunline's identity. Nia plans to read it carefully."},
 {"Foundry Ring is a step beyond the harbor clubs. Kade invited a benchmark exhibition, and Rook asked Sunline to learn how her clubs work.",
  "Kade's benchmark exposed the same exit problem. Repetitions help; people staying through Sunline's power cut remind you that company isn't a prize for a good result.",
  "The clean rematch earned Kade's respect and a copy of the licensing rules. The next gate requires a financial guarantee, not just a fast lap.",
  "Voss's offer means real wages. Jules needs reliable payment, and Nia is learning to ask the crew what security means instead of answering for them.",
  "Mina took her own paid work. Voss showed you the workshop he lost. The clubs backed Sunline's independent Meridian application after reading the real obligations."},
 {"Meridian brings pit procedures, budget motel rooms and Ren's practical advice. A wider road doesn't stop the driver feeling new.",
  "Practice and qualifying sessions are teaching repeatable sectors. Mina's paid photography work is her own; being part of Sunline doesn't make anyone its property.",
  "Ren's sector duel earned a sporting recommendation. The crew now has someone on the pit wall who can turn a driver's observations into a useful plan.",
  "A motel-room meeting gave wages, personal work and the garage name equal weight. Ren is teaching rest and repeatable decisions alongside the timing sheets.",
  "Aster signed the sporting invitation. Sunline can enter the Cup paddock under its own name, but the larger season-cost guarantee still needs approval."},
 {"Sunline has a place in the Cup paddock. Sable wants a proper contest and a more interesting weekend. Nia wants the crew to remember the ordinary work that brought them here.",
  "Mina opened a small exhibition. Sable shared the imperfect afternoons behind the champion's photograph. Weather and pressure don't erase the need for an ordinary evening.",
  "Sable backed a statement asking for fair independent terms after the duel. The request concerns real guarantees and working crews, not an exemption from the costs.",
  "Back in Saltblock between rounds, the neighbors wanted company rather than proof of perfection. Work, rest and a fair budget still carry the final series.",
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
int career_rows(int tab){return tab==0?4:tab==2?3:5;}
void career_reset(void){memset(&campaign,0,sizeof campaign);campaign.revision=1;}
void career_enter(Game*g){
 int done=story_completed(g);g->mode=M_STORY;g->cursor=0;campaign.tab=0;campaign.chapter=(uint8_t)(done<125?done/25:4);campaign.event=(uint8_t)(done<125?done%25:24);
 campaign.modal=0;campaign.garage_return=0;g->notice_timer=0;campaign.revision++;
}
static void career_message(Game*g,const char*text){size_t n=strlen(text);if(n>=sizeof g->notice)n=sizeof g->notice-1;memcpy(g->notice,text,n);g->notice[n]=0;g->notice_timer=120;campaign.revision++;}
void campaign_tick(Game*g,const Input*in){
 if(!in->pressed)return;
 if(in->pressed&K_B){if(campaign.modal)campaign.modal=0;else{g->mode=M_DRIVE;g->cursor=2;}campaign.revision++;return;}
 if(campaign.modal==CAREER_CAST_MODAL){
  static const uint8_t poses[STORY_GALLERY_COUNT]=STORY_GALLERY_POSE_COUNTS_INITIALIZER;
  int direction=horizontal(in)+vertical(in);
  if(direction){campaign.page=(uint8_t)((campaign.page+STORY_GALLERY_COUNT+direction)%STORY_GALLERY_COUNT);campaign.gallery_pose=0;}
  int pose=((in->pressed&K_R)?1:0)-((in->pressed&K_L)?1:0);
  if(in->pressed&K_A)pose++;
  if(pose){unsigned count=poses[campaign.page];campaign.gallery_pose=(uint8_t)((campaign.gallery_pose+count+pose)%count);}
  campaign.revision++;return;
 }
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
  }else if(campaign.tab==4&&g->cursor==4){campaign.modal=CAREER_CAST_MODAL;campaign.page=campaign.gallery_pose=0;}
  else if(campaign.tab==4&&g->cursor==2){career_enter(g);g->cursor=1;}
  else if(campaign.tab==1){
   if(!career_journal_open(g,campaign.chapter,g->cursor)){career_message(g,"This journal entry opens later in the story.");return;}
   campaign.modal=2;campaign.page=0;
  }else if(campaign.tab==2||campaign.tab==3||(campaign.tab==4&&(g->cursor==1||g->cursor==3))){campaign.modal=(uint8_t)(campaign.tab==4?5:campaign.tab+1);campaign.page=0;}
 }
 campaign.revision++;
}
