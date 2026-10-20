/* Rising Tide: a browsable campaign proposal, not a second vehicle economy.
 * Race launch, payouts, unlocks and scripted outcomes are deliberately absent. */
#include "career.h"
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
 {"Nia lends you a bay, not a fortune. Small club circuits introduce the crew and the bills waiting at home.",
  "An exhibition against a much faster visitor ends in a story defeat. No money is taken. Events 7-11 are a proposed practice-and-cash block, not five separate crises.",
  "Jules tests whether you can finish a clean race under pressure. A courier who knows every corner becomes the crew's first ally.",
  "Regular club nights build a cushion for the captain's entry fee. The garage becomes a meeting place, not just a repair bill.",
  "Calder grants the neighborhood its own club spot. A waterfront invitation arrives with Voss's name at the bottom."},
 {"Marisol opens the harbor series to your garage. Imani notices that your first result was no accident.",
  "A string of night circuits is ordinary work: entry money, practice and introductions. The crew can breathe between bigger moments.",
  "Imani runs the shift-change duel. Her respect earns a seat at the sponsor's table, not a free new car.",
  "The sponsor wants clean results. Club appearances fund the harbor final while Nia investigates the Cup's exclusive contracts.",
  "Marisol backs your independent entry into Foundry Ring. A fast driver named Kade asks to see what you can really do."},
 {"Kade wins the benchmark exhibition in a story scene. The gap is racecraft, not a stolen engine or a surprise cash penalty.",
  "Practice nights revisit braking points and exits. A training block gives the crew time to earn money without pretending every race changes the city.",
  "The event-12 rematch is a fair test. Kade becomes a sparring partner and reveals how Rook's club feeds Voss's recruitment list.",
  "Voss offers to buy the garage name. The crew refuses. Ordinary industrial races now finance an independent circuit license.",
  "Rook yields the regional berth. The victory earns a sporting invitation; the same garage, cars and upgrades travel with you."},
 {"Meridian is a permanent racing facility, with runoff, pit buildings and proper curbs. The crew learns a new environment, not a new inventory.",
  "Sessions focus on qualifying and repeatable sectors. Planned strategy ideas live in the Plans tab for you to revise before gameplay exists.",
  "Ren's sector duel earns a recommendation. The fastest lap matters, but the story is about earning a crew's trust.",
  "Aster's invitation is conditional on keeping your operation independent. Preparation races pay the costs of the final meeting.",
  "Aster opens the Cup gate. Voss makes one final offer: a guaranteed seat in exchange for the garage's identity."},
 {"The permanent road-course series begins. Nia manages the crew while you decide what kind of champion the garage will become.",
  "Longer events propose weather and pit-window decisions. Practice sessions keep the story grounded between headline races.",
  "Sable asks for a fair duel before the business gets ugly. Mutual respect turns a rival into a witness to Voss's terms.",
  "The final series is work, pressure and preparation. Your shared vehicles remain the tools; no exclusive career paint or upgrade shop appears.",
  "The final against Voss is about independence. The proposed ending returns to Saltblock, where the crew opens its doors to the next unknown driver."}
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
int career_status(int chapter,int event){int done=campaign.preview_done[chapter%5];return event<done?2:event==done?1:0;}
int career_rows(int tab){return tab==0?4:tab==2?3:tab==4?4:5;}
void career_reset(void){memset(&campaign,0,sizeof campaign);campaign.revision=1;}
void career_enter(Game*g){g->mode=M_STORY;g->cursor=0;campaign.modal=0;campaign.garage_return=0;g->notice_timer=0;campaign.revision++;}
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
  if(g->cursor==0&&(dx||(in->pressed&K_A))){campaign.chapter=(uint8_t)((campaign.chapter+5+(dx?dx:1))%5);campaign.event=campaign.preview_done[campaign.chapter]<25?campaign.preview_done[campaign.chapter]:24;}
  if(g->cursor==1&&dx)campaign.event=(uint8_t)((campaign.event+25+dx)%25);
  if((in->pressed&K_A)&&(g->cursor==1||g->cursor==2)){campaign.modal=1;campaign.page=0;}
 }
 if(campaign.tab==4&&g->cursor==1&&(dx||(in->pressed&K_A))){int n=campaign.preview_done[campaign.chapter];n=(n+26+(dx?dx:1))%26;campaign.preview_done[campaign.chapter]=(uint8_t)n;campaign.event=(uint8_t)(n<25?n:24);}
 if(campaign.tab==4&&g->cursor==2&&(in->pressed&K_A)){memset(campaign.preview_done,0,5);campaign.event=0;}
 if(in->pressed&K_A){
  if((campaign.tab==0&&g->cursor==3)||(campaign.tab==4&&g->cursor==0)){
   campaign.garage_return=1;g->mode=M_GARAGE;g->garage_tab=0;g->cursor=0;g->selection=g->save.selected_vehicle;refresh_vehicle(g);
  }else if(campaign.tab==1||campaign.tab==2||campaign.tab==3||(campaign.tab==4&&g->cursor==3)){campaign.modal=(uint8_t)(campaign.tab+1);campaign.page=0;}
 }
 campaign.revision++;
}
