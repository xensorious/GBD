/* Presentation only: precomposed scenery stores a ground-contact depth per
 * pixel. Dynamic actors sort by their feet and draw only in front of scenery
 * no closer than themselves, avoiding a costly redraw of every tree. */
typedef struct {short x,y,depth;unsigned short kind,arg;} SceneNode;
static SceneNode scene_nodes[256];
static int scene_count;
enum { PERSON, FIELD_CREATURE, SATCHEL, FLOWERS };

static void scene_add(int x,int y,int foot,int priority,int kind,int arg,int w,int h){
 if(x+w<=camx||x>=camx+240||y+h<=camy||y>=camy+160||scene_count>=256)return;
 SceneNode *n=&scene_nodes[scene_count++];
 n->x=x;n->y=y;n->depth=foot*4+priority;n->kind=kind;n->arg=arg;
}
static void scene_actor(int x,int y,int id,int dir,int step,int hero){
 scene_add(x,y-8,y+16,hero?3:2,PERSON,id|(dir<<3)|((step&1)<<5),16,24);
}
static void scene_friend(int x,int y,int s){
 scene_add(x,y-4,y+16,2,FIELD_CREATURE,s,16,20);
}
static void scene_draw(void){
 for(int i=1;i<scene_count;i++){
  SceneNode n=scene_nodes[i];int j=i;
  while(j&&scene_nodes[j-1].depth>n.depth){scene_nodes[j]=scene_nodes[j-1];j--;}
  scene_nodes[j]=n;
 }
 for(int i=0;i<scene_count;i++){
  SceneNode *n=&scene_nodes[i];int x=n->x-camx,y=n->y-camy,k=n->arg;
  scene_cutoff=n->depth/64;
  if(n->kind==PERSON){
   ellipse(x+2,y+21,13,3,72);person(x,y,k&7,(k>>3)&3,(k>>5)&1);
  }else if(n->kind==FIELD_CREATURE){
   ellipse(x+2,y+17,13,3,72);art(x,y,creature_field_art+k*320,16,20,1,0);
  }else if(n->kind==SATCHEL){
   ellipse(x+2,y+12,13,3,72);rect(x+5,y+3,6,3,56);rect(x+6,y+4,4,2,63);
   rect(x+3,y+6,11,8,56);rect(x+4,y+6,9,6,62);rect(x+4,y+6,9,2,60);
   rect(x+5,y+11,7,2,63);rect(x+7,y+7,3,4,58);rect(x+8,y+8,1,2,59);
  }else{
   rect(x+4,y+4,2,10,29);rect(x+2,y+8,3,2,30);rect(x+6,y+6,3,2,30);
   rect(x+3,y+2,4,5,73);rect(x+2,y+3,6,3,73);rect(x+4,y+4,2,2,62);
   rect(x+10,y+8,1,6,29);rect(x+8,y+7,5,3,62);rect(x+10,y+8,1,1,60);
  }
 }
}
static void marker(int x,int y,int color){
 int dy=(frame/24)&1;
 rect(x+5,y-14-dy,6,1,56);rect(x+3,y-12-dy,10,8,56);rect(x+5,y-14-dy,6,12,56);
 rect(x+5,y-12-dy,6,8,color);rect(x+4,y-10-dy,8,4,color);
 rect(x+7,y-11-dy,2,4,56);rect(x+7,y-6-dy,2,1,56);rect(x+7,y-2-dy,2,2,56);
}
static void drawfield(void){
 if(absval(visualx-g.x*16)>32||absval(visualy-g.y*16)>32){visualx=g.x*16;visualy=g.y*16;visualfx=followx;visualfy=followy;}
 visualx+=CLAMP(g.x*16-visualx,-4,4);visualy+=CLAMP(g.y*16-visualy,-4,4);
 visualfx+=CLAMP(followx-visualfx,-4,4);visualfy+=CLAMP(followy-visualfy,-4,4);
 camx=CLAMP(visualx-112,0,512-240);camy=CLAMP(visualy-72,0,384-160);
 const u8*bg=map_scene_art+g.map*512*384+camy*512+camx;
 for(int y=0;y<160;y++){REG32(0x040000d4)=(u32)(bg+y*512);REG32(0x040000d8)=(u32)(fb+y*240);REG32(0x040000dc)=0x84000000|60;}
 scene_count=0;
 scene_depth=map_scene_depth+g.map*512*384+camy*512+camx;
 if(!g.map){
  scene_actor(15*16,10*16,1,0,0,0);scene_actor(6*16,11*16,2,0,0,0);
  scene_actor(24*16,14*16,3,1,0,0);scene_actor(11*16,16*16,4,2,0,0);
  if(!g.count)for(int i=0;i<3;i++)scene_friend((13+i*2)*16,8*16,i);
  if(g.job)for(int x=22;x<28;x++)scene_add(x*16,11*16,12*16+1,1,FLOWERS,0,16,16);
  for(int i=0;i<g.count;i++)if(g.friends[i].stationed)scene_friend(25*16,12*16,g.friends[i].species);
 }else{
  for(int i=0;i<6;i++)if(!encounters[i].cooldown){Encounter*e=&encounters[i];scene_friend(e->x*16,e->y*16,e->species);}
  scene_actor(4*16,19*16,2,0,0,0);scene_actor(26*16,5*16,4,0,0,0);
 }
 const u8 px[3]={9,8,22},py[3]={16,20,12},pm[3]={0,1,1};
 for(int i=0;i<3;i++)if(g.map==pm[i]&&!(g.pickups&(1<<i)))scene_add(px[i]*16,py[i]*16,py[i]*16+16,1,SATCHEL,0,16,16);
 if(g.count)scene_friend(visualfx,visualfy,g.friends[g.lead].species);
 scene_actor(visualx,visualy,0,g.dir,walkFrame,1);
 scene_draw();
 scene_depth=0;
 if(!g.map&&!g.count)marker(15*16-camx,10*16-camy-8,62);
 if(g.map&&!g.scout)marker(4*16-camx,19*16-camy-8,62);
 if(g.map&&!g.rival)marker(26*16-camx,5*16-camy-8,62);
 /* Compact map label leaves the landscape visible. */
 rect(4,5,142,17,72);rect(3,3,142,17,56);rect(3,3,2,17,58);
 rect(10,9,7,1,59);rect(13,6,1,7,59);rect(12,8,3,3,62);
 text(22,8,places[g.map],61);
 if(g.count){rect(181,5,55,17,72);rect(180,3,55,17,56);text(187,8,"TEAM",59);smallnum(219,8,g.count,62);}
 rect(0,148,240,12,56);rect(0,148,240,1,58);text(5,151,objective(),60);
 if(!g.count&&screen!=MENU){rect(2,134,183,12,60);text(5,137,"A TALK   B RUN   START JOURNAL",56);}
}
