/* The actual renderer, checked natively.  Cartridge cadence is tested in the
 * emulator separately; these checks cover projection and world invariants. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../src/render.c"

static unsigned checks;
static uint16_t test_top[256*192],test_bottom[256*192];
static void require(int condition,const char *message)
{
 checks++;
 if(!condition){fprintf(stderr,"renderer geometry: %s (check %u)\n",message,checks);exit(1);}
}
static int absolute_value(int n){return n<0?-n:n;}

static View fixture(unsigned seed,int biome,int position,int motion)
{
 View v={0};v.seed=seed;v.start=biome;v.position=position;v.motion=motion;
 v.visual_scale=motion==0?205:motion==2?384:307;
 v.visual_position=visual_travel(position,motion);
 RoadSample road;road_sample(seed,position,biome,&road);v.center=road.center;
 return v;
}

static void projection_checks(void)
{
 require(HORIZON==20 && CAMERA==190 && PLAYER_Y==160,"established road camera preserved");
 View v=fixture(0xC0A571u,CITY,800,1);
 for(int side=-1;side<=1;side+=2)for(int offset=120;offset<=240;offset+=30)
 for(int height=0;height<=90;height+=10)for(int near=60;near<=800;near+=37) {
  int x=v.center+side*offset;
  SceneryPoint a=scenery_point(&v,x,near,height);
  SceneryPoint b=scenery_point(&v,x,near+150,height);
  int determinant=(a.x-128)*(b.y-HORIZON)-(b.x-128)*(a.y-HORIZON);
  int rounding=2*(absolute_value(a.x-128)+absolute_value(a.y-HORIZON)+
                  absolute_value(b.x-128)+absolute_value(b.y-HORIZON));
  require(absolute_value(determinant)<=rounding,"world depth edges converge on road vanishing point");
  SceneryPoint high=scenery_point(&v,x,near,height+20);
  require(a.x==high.x,"building verticals remain vertical");
  require(high.y<=a.y,"upright height rises above road plane");
  require(absolute_value(b.x-128)<=absolute_value(a.x-128),"distance compresses roadside geometry");
 }
}

static void grammar_checks(void)
{
 unsigned docks=0,boats=0,houses=0,plants=0;
 unsigned min_count=1000,max_count=0;
 for(unsigned seed=1;seed<=24;seed++)for(int biome=0;biome<LOCATION_COUNT;biome++)
 for(int motion=0;motion<3;motion++)for(int position=800;position<24000;position+=1400) {
  View v=fixture(seed*0x817bu,biome,position,motion);
  DrawItem a[DRAW_MAX],b[DRAW_MAX];int na=0,nb=0;
  queue_scenery(a,&na,&v);queue_scenery(b,&nb,&v);
  require(na==nb && memcmp(a,b,(size_t)na*sizeof a[0])==0,"world regeneration is deterministic");
  require(na<DRAW_MAX,"individual spawn budgets never truncate visible objects");
  if((unsigned)na<min_count)min_count=(unsigned)na;
  if((unsigned)na>max_count)max_count=(unsigned)na;
  for(int i=0;i<na;i++) {
   const DrawItem *p=&a[i];int absolute=p->z+v.visual_position;
   int water;int material=scenery_material(&v,absolute,p->side,(uint32_t)p->variant,&water);
   int floating=p->id==SC_SAIL || p->id==SC_PIER;
   if(floating)require(water==256,"water objects never appear on transition dry land");
   else if(p->id!=SC_MANGROVE && p->id!=SC_REEDS && !(p->id==SC_LAMP && bridge_region(material)))
    require(water==0,"dry scenery never appears in transition water");
   require(p->id!=SC_GANTRY && p->id!=SC_BILLBOARD && p->id!=SC_BARRIER,"overhead signs and bridge billboards remain removed");
   RoadSample road;road_sample(v.seed,v.position+p->z,v.start,&road);
   if(building_id(p->id)) {
    houses++;
    int offset=p->side*(p->x-road.center);
    require(offset>=ROAD_HALF+33 && offset<=ROAD_HALF+40,"houses share frontage setbacks clear of road");
    for(int j=i+1;j<na;j++)if(building_id(a[j].id) && a[j].side==p->side)
     require(absolute_value(p->z-a[j].z)>=148,"adjacent house lots have room for complete depth");
   }
   if(p->id==SC_PIER) {
    docks++;
    require((absolute-52)%520==0,"dock anchors follow sparse independent 520-unit slots");
    require(p->side*(p->x-road.center)==ROAD_HALF+7,"dock first endpoint meets actual shoreline");
    int sections=1+(((unsigned)p->variant>>8)&1);
    require(sections==1 || sections==2,"dock contains one or two connected sections");
    for(int section=0;section<sections;section++) {
     int start=p->x+p->side*section*60;
     int end=start+p->side*60;
     if(section+1<sections)require(end==p->x+p->side*(section+1)*60,"dock sections have identical shared endpoints");
    }
    for(int j=i+1;j<na;j++)if(a[j].id==SC_PIER && a[j].side==p->side)
     require(absolute_value(p->z-a[j].z)>=520,"large dock structures have independent minimum spacing");
   }
   if(p->id==SC_SAIL) {
    boats++;require(p->side*(p->x-road.center)>=190,"boats stay offshore");
    for(int j=0;j<na;j++)if(a[j].id==SC_PIER && a[j].side==p->side)
     require(absolute_value(p->z-a[j].z)>=58,"boats do not cut through dock decks");
   }
   if(p->id==SC_TREE || p->id==SC_PINE || p->id==SC_PALM || p->id==SC_CITRUS)plants++;
  }
  /* The same absolute object survives a small camera movement both ways.
   * Only its camera-relative depth changes; identities never get rerolled. */
  View next=fixture(v.seed,biome,position+4,motion);DrawItem c[DRAW_MAX];int nc=0;
  queue_scenery(c,&nc,&next);
  for(int i=0;i<na;i++)if(a[i].z>=32 && a[i].z<ROAD_VIEW-32) {
   int found=0;
   for(int j=0;j<nc;j++)if(a[i].z+v.visual_position==c[j].z+next.visual_position &&
       a[i].id==c[j].id && a[i].variant==c[j].variant && a[i].side==c[j].side)found++;
   require(found==1,"visible world anchors persist while camera advances or reverses");
  }
 }
 require(docks>200 && boats>300 && houses>1000 && plants>1000,"all individual spawn categories exercised");
 require(max_count+TRAFFIC_COUNT+PICKUP_COUNT+1<DRAW_MAX && max_count>min_count,
         "densest scenery leaves room for complete traffic/pickup pools and player");
 printf("Scenery draw items: %u..%u, %u slots remain with both pools full.\n",
        min_count,max_count,DRAW_MAX-max_count-TRAFFIC_COUNT-PICKUP_COUNT-1);
}

static void shadow_checks(void)
{
 Surface surface={test_top,256,192};
 const uint16_t backdrop=C(151,183,194),contact=C(36,51,56);
 for(int pose=0;pose<50;pose++) {
  for(int i=0;i<256*192;i++)test_top[i]=backdrop;
  contact_shadow(&surface,20,20,48,72,vehicle_body_spans[pose],48,72);
  for(int row=0;row<72;row++)for(int x=0;x<50;x++) {
   int left=vehicle_body_spans[pose][row][0],right=vehicle_body_spans[pose][row][1];
   int covered=right>left && x>=left && x<right+2;
   require(test_top[(21+row)*256+19+x]==(covered?contact:backdrop),
           "contact shadow follows each opaque row without a rectangular backdrop");
  }
 }
}

static void reference_wall(Surface *s,const View *v,
 int ax,int az,int bx,int bz,int height,const uint16_t *texture,int tw,int th)
{
 if(az < -44 && bz < -44)return;
 int ua=0,ub=tw-1;
 if(az < -44) {ua=((-44-az)*(tw-1))/(bz-az);az=-44;}
 if(bz < -44) {ub=((az+44)*(tw-1))/(az-bz);bz=-44;}
 SceneryPoint a=scenery_point(v,ax,az,0),b=scenery_point(v,bx,bz,0);
 SceneryPoint at=scenery_point(v,ax,az,height),bt=scenery_point(v,bx,bz,height);
 int sa=project_scale(az),sb=project_scale(bz);
 if(a.x>b.x) {
  SceneryPoint swap=a;a=b;b=swap;swap=at;at=bt;bt=swap;
  int temp=sa;sa=sb;sb=temp;temp=ua;ua=ub;ub=temp;
 }
 int span=b.x-a.x;
 int x0=clampi(a.x,0,VW),x1=clampi(b.x+1,0,VW);
 if(span<1 || x1<=x0)return;
 typedef struct {int16_t top,bottom;uint16_t u;uint32_t step;} WallColumn;
 WallColumn columns[VW];
 int top_step=((bt.y-at.y)*65536)/span,bottom_step=((b.y-a.y)*65536)/span;
 int scale_step=((sb-sa)*65536)/span;
 int u_step=((ub*sb-ua*sa)*65536)/span;
 int offset=x0-a.x;
 int top=at.y*65536+top_step*offset,bottom=a.y*65536+bottom_step*offset;
 int scale=sa*65536+scale_step*offset,u=ua*sa*65536+u_step*offset;
 int first=VH,last=0;
 for(int x=x0;x<x1;x++,top+=top_step,bottom+=bottom_step,scale+=scale_step,u+=u_step) {
  int t=top/65536,base=bottom/65536;
  if(base<=t || scale<=0){columns[x]=(WallColumn){0,0,0,0};continue;}
  int tex=clampi(u/scale,0,tw-1);
  columns[x]=(WallColumn){(int16_t)t,(int16_t)base,(uint16_t)tex,
                           (uint32_t)((th*65536)/(base-t))};
  int y0=clampi(t,0,VH),y1=clampi(base,0,VH);
  if(y0<first)first=y0;
  if(y1>last)last=y1;
 }
 /* Deliberately slow reference: the original bounding-box coverage test.
  * Keep it independent of the optimized monotone row cursors. */
 for(int y=first;y<last;y++)for(int x=x0;x<x1;x++) {
  const WallColumn *column=&columns[x];
  if(y<column->top || y>=column->bottom)continue;
  unsigned sy=((unsigned)(y-column->top)*column->step)>>16;
  s->pixels[y*VW+x]=texture[sy*(unsigned)tw+column->u];
 }
}

static void wall_equivalence_checks(void)
{
 Surface optimized={test_top,256,192},reference={test_bottom,256,192};
 const int distances[]={-80,20,90,250,700};
 for(int side=-1;side<=1;side+=2)for(int offset=125;offset<=200;offset+=25)
 for(unsigned z=0;z<sizeof distances/sizeof distances[0];z++)
 for(int camera=-20;camera<=20;camera+=20)for(int height=43;height<=76;height+=33)
 for(int tw=16;tw<=64;tw*=2) {
  View v=fixture(0x2c7fbae5u,CITY,1897,1);v.cam_x=camera;
  int x=v.center+side*offset;
  for(int i=0;i<256*192;i++)test_top[i]=test_bottom[i]=C(147,176,183);
  building_wall(&optimized,&v,x,distances[z],x,distances[z]+104,height,building_wall_art,tw,tw);
  reference_wall(&reference,&v,x,distances[z],x,distances[z]+104,height,building_wall_art,tw,tw);
  require(memcmp(test_top,test_bottom,sizeof test_top)==0,
          "optimized wall spans preserve every original texture/coverage pixel");
 }
 for(int style=0;style<5;style++)for(int side=-1;side<=1;side+=2)
 for(int z=0;z<=800;z+=53)for(int camera=-20;camera<=20;camera+=20) {
  View v=fixture(0x2c7fbae5u,CITY,1897,1);v.cam_x=camera;
  int inner=v.center+side*132,outer=inner+side*60,height=48;
  for(int i=0;i<256*192;i++)test_top[i]=test_bottom[i]=C(147,176,183);
  building_end_wall(&optimized,&v,inner,outer,z,height,style);
  SceneryPoint a=scenery_point(&v,inner,z,0),b=scenery_point(&v,outer,z,height);
  int left=a.x<b.x?a.x:b.x,width=b.x-a.x;
  if(width<0)width=-width;
  if(width>0 && a.y>b.y) {
   int dest_width=width+1,dest_height=a.y-b.y;
   int source_width=32;
   if(dest_width<=16 && dest_height<=32)source_width=16;
   if(dest_width<=8 && dest_height<=16)source_width=8;
   gfx_scaled_sprite(&reference,left,b.y,
      building_end_texture(style,a.x>b.x,source_width),source_width,source_width*2,dest_width,dest_height);
  }
  require(memcmp(test_top,test_bottom,sizeof test_top)==0,
          "cacheless end-wall mapper matches the original nearest-neighbor rectangle");
 }
}

static void raster_checks(void)
{
 Game game;game_init(&game,0xC0A571u);game_begin_run(&game);
 Surface surface={test_top,256,192};
 for(int style=0;style<5;style++)for(int side=-1;side<=1;side+=2)
 for(int z=-120;z<=1000;z+=47)for(int camera=-20;camera<=20;camera+=20) {
  View v=fixture(0xC0A571u,CITY,800,1);v.cam_x=camera;
  memset(test_top,0,sizeof test_top);
  int ids[5]={SC_BUILDING,SC_SHOP,SC_WAREHOUSE,SC_HOTEL,SC_VILLA};
  DrawItem item={z,v.center+side*132,ITEM_PROP,ids[style],0,side};
  draw_prop(&surface,&v,&item);checks++;
 }
 for(int biome=0;biome<LOCATION_COUNT;biome++)for(int p=0;p<4;p++) {
  game.save.selected_location=(uint8_t)biome;game_begin_run(&game);
  game.position_q8=game.furthest_q8=(800+p*6500)*Q;
  game.speed_q8=180*Q;game.frame=300;game.police_pressure=(uint8_t)(p*33);
  render_game(&game,test_top,test_bottom);checks++;
 }
}

int main(void)
{
 gfx_init();init_projection();projection_checks();grammar_checks();shadow_checks();wall_equivalence_checks();raster_checks();
 printf("Renderer geometry: %u checks passed.\n",checks);return 0;
}
