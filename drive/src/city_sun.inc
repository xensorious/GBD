/* One world-space lighting model serves projected ground silhouettes and
 * every receiver. Geometry is cached by a world chunk, never camera angle.
 * All sizes are bounded; no shadow framebuffer or texture streaming on DS. */
enum {CITY_SUN_X=320,CITY_SUN_Y=192,CITY_SUN_MAX=192};
enum {SUN_BUILDING,SUN_TREE,SUN_LAMP,SUN_COURT};
typedef struct {int16_t x,y;} CitySunPoint; /* Q4, relative to caster origin */
typedef struct {CitySunShape shape;CitySunPoint hull[8];int16_t maxx,maxy;uint8_t count;} CitySunCaster;
static CitySunCaster city_casters[CITY_SUN_MAX];
static unsigned city_caster_count,city_sun_frame;
static int city_sun_chunk_x=-100,city_sun_chunk_y=-100;
/* Local 32-unit buckets turn receiver queries into a handful of nearby
 * silhouettes, rather than scanning the whole visible district per vertex. */
static uint8_t city_sun_bins[32*32][8],city_sun_bin_count[32*32];
static int city_sun_bin_x,city_sun_bin_y;
volatile uint32_t city_sun_bin_stats[2]; /* max occupancy, overflow buckets */
volatile uint32_t city_sun_stats[9]; /* casters, rebuilds, queries, dark, player min/max, partial cars, frame, mean */
static int sun_building_height(int style){return style==9||style==10||style==11?42:style==3||style==8?32:24;}
static int sun_pitched(int style){return style==0||style==1||style==2||style==7||style==11||style==12;}
static int sun_cross(CitySunPoint a,CitySunPoint b,CitySunPoint c){
 return (b.x-a.x)*(c.y-a.y)-(b.y-a.y)*(c.x-a.x);
}
static void sun_hull(CitySunCaster*c){
 CitySunShape s=c->shape;CitySunPoint p[10],hull[20];int n=0;
 for(int yy=0;yy<=1;yy++)for(int xx=0;xx<=1;xx++){
  p[n++]=(CitySunPoint){(int16_t)(xx*s.w*16),(int16_t)(yy*s.d*16)};
  p[n++]=(CitySunPoint){(int16_t)(xx*s.w*16+s.height*CITY_SUN_X/16),(int16_t)(yy*s.d*16+s.height*CITY_SUN_Y/16)};
 }
 if(s.ridge)for(int xx=0;xx<=1;xx++)p[n++]=(CitySunPoint){
  (int16_t)(xx*s.w*16+(s.height+s.ridge)*CITY_SUN_X/16),
  (int16_t)(s.d*8+(s.height+s.ridge)*CITY_SUN_Y/16)};
 for(int i=1;i<n;i++){CitySunPoint a=p[i];int j=i;while(j&&(p[j-1].x>a.x||(p[j-1].x==a.x&&p[j-1].y>a.y))){p[j]=p[j-1];j--;}p[j]=a;}
 int k=0;
 for(int i=0;i<n;i++){while(k>=2&&sun_cross(hull[k-2],hull[k-1],p[i])<=0)k--;hull[k++]=p[i];}
 for(int i=n-2,t=k+1;i>=0;i--){while(k>=t&&sun_cross(hull[k-2],hull[k-1],p[i])<=0)k--;hull[k++]=p[i];}
 c->count=(uint8_t)(k-1);c->maxx=c->maxy=0;
 for(int i=0;i<c->count;i++){c->hull[i]=hull[i];if(hull[i].x>c->maxx)c->maxx=hull[i].x;if(hull[i].y>c->maxy)c->maxy=hull[i].y;}
}
static int sun_contains(const CitySunCaster*c,int xq,int yq,int z,unsigned frame){
 const CitySunShape*s=&c->shape;if(s->kind==SUN_COURT||z>=s->height+s->ridge)return 0;
 if(s->kind==SUN_TREE){
  int u=xq-s->x*Q-(s->height-z)*CITY_SUN_X-(int)((frame/30)&1u)*Q;
  int v=yq-s->y*Q-(s->height-z)*CITY_SUN_Y;
  if((unsigned)u>=48*Q||(unsigned)v>=48*Q)return 0;
  return city_mesh_idx[(256+v/Q)*256+s->texture*48+u/Q]!=0;
 }
 int x=xq-s->x*Q,y=yq-s->y*Q;
 if(x<0||y<0)return 0;
 /* Moving the receiver up shortens the roof's projected reach. Points in
  * the solid footprint are naturally below the structure's roof. */
 if(x<s->w*Q&&y<s->d*Q)return 1;
 x=(x+z*CITY_SUN_X)/16;y=(y+z*CITY_SUN_Y)/16;
 if(x>c->maxx||y>c->maxy)return 0;
 CitySunPoint p={(int16_t)x,(int16_t)y};
 for(int i=0;i<c->count;i++)if(sun_cross(c->hull[i],c->hull[(i+1)%c->count],p)<0)return 0;
 return 1;
}
int city_sun_shape_shadow(const CitySunShape*s,int xq,int yq,int height,unsigned frame){
 CitySunCaster caster={0};caster.shape=*s;
 if(s->kind!=SUN_TREE&&s->kind!=SUN_COURT)sun_hull(&caster);
 return sun_contains(&caster,xq,yq,height,frame);
}
static void sun_add(CitySunShape shape){
 if(city_caster_count>=CITY_SUN_MAX)return;
 CitySunCaster*c=&city_casters[city_caster_count++];c->shape=shape;c->count=0;
 if(shape.kind==SUN_BUILDING||shape.kind==SUN_LAMP)sun_hull(c);
}
void city_sun_prepare(int cx,int cy,unsigned frame){
 city_sun_frame=frame;int chunkx=cx/128,chunky=cy/128;
 if(chunkx==city_sun_chunk_x&&chunky==city_sun_chunk_y)return;
 city_sun_chunk_x=chunkx;city_sun_chunk_y=chunky;city_caster_count=0;
 int minx=chunkx*128-384,miny=chunky*128-432,maxx=chunkx*128+448,maxy=chunky*128+496;
 for(int row=clampi(miny/128,0,31);row<=clampi(maxy/128,0,31);row++)
  for(int col=clampi(minx/128,0,31);col<=clampi(maxx/128,0,31);col++){
   CityLot lot=city_lot_bounds(col,row);
   unsigned decor=road_hash(0x47415244u,(unsigned)(row*32+col));
   if(lot.kind){int style=city_building_style(col,row),h=sun_building_height(style),pitched=sun_pitched(style);
    sun_add((CitySunShape){(int16_t)lot.x,(int16_t)lot.y,(int16_t)lot.w,(int16_t)lot.h,
       (uint8_t)(h+(pitched?0:3)),(uint8_t)(pitched?12:0),SUN_BUILDING,(uint8_t)style});
   }else{
    int x=col*128+64,y=row*128+64;
    if(city_garden_clear(x-28,y-26,56,52))sun_add((CitySunShape){(int16_t)(x-28),(int16_t)(y-26),56,52,0,0,SUN_COURT,0});
   }
   int x=col*128+13+(int)((decor>>10)%9),y=row*128+12+(int)((decor>>14)%9);
   if(!city_water(x,y)&&!city_on_road(x,y)&&!city_solid(x,y))
    sun_add((CitySunShape){(int16_t)(x-24),(int16_t)(y-24),48,48,18,0,SUN_TREE,(uint8_t)((decor>>8)%5)});
  }
 for(int i=0;i<city_road_count;i++){
  const CityRoad*r=&city_roads[i];int hor=r->y1==r->y2;
  int begin=hor?r->x1:r->y1,end=hor?r->x2:r->y2;
  for(int at=city_floor(hor?minx:miny,256);at<(hor?maxx:maxy);at+=256)if(at>begin+65&&at<end-65)
   for(int side=-1;side<=1;side+=2){
    int x=hor?at:r->x1+side*(r->half+13),y=hor?r->y1+side*(r->half+13):at;
    if(x<minx||x>maxx||y<miny||y>maxy)continue;
    if(city_garden_clear(x-6,y-6,12,12))sun_add((CitySunShape){(int16_t)(x-1),(int16_t)(y-1),2,2,26,0,SUN_LAMP,0});
   }
 }
 city_sun_bin_x=minx;city_sun_bin_y=miny;
 memset(city_sun_bin_count,0,sizeof city_sun_bin_count);
 city_sun_bin_stats[0]=city_sun_bin_stats[1]=0;
 for(unsigned i=0;i<city_caster_count;i++){
  const CitySunShape*s=&city_casters[i].shape;if(s->kind==SUN_COURT)continue;
  int reach=(s->height+s->ridge)*CITY_SUN_X/Q+2;
  int x0=clampi((s->x-minx-1)/32,0,31),y0=clampi((s->y-miny-1)/32,0,31);
  int x1=clampi((s->x+s->w+reach-minx)/32,0,31),y1=clampi((s->y+s->d+reach-miny)/32,0,31);
  for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++){
   int bin=y*32+x,count=city_sun_bin_count[bin];
   if(count<8){city_sun_bins[bin][count]=(uint8_t)i;city_sun_bin_count[bin]++;}
   else if(count!=255){city_sun_bin_count[bin]=255;city_sun_bin_stats[1]++;}
   if(count<8&&(unsigned)(count+1)>city_sun_bin_stats[0])city_sun_bin_stats[0]=(unsigned)count+1;
  }
 }
 city_sun_stats[0]=city_caster_count;city_sun_stats[1]++;
}
unsigned city_sun_shapes(CitySunShape*out,unsigned capacity){
 unsigned count=city_caster_count<capacity?city_caster_count:capacity;
 for(unsigned i=0;i<count;i++)out[i]=city_casters[i].shape;
 return city_caster_count;
}
int city_sun_occlusion(int xq,int yq,int height){
 city_sun_stats[2]++;
 /* A half-world-unit transition at silhouette edges prevents an entire
  * vehicle from snapping between two palettes at the shadow boundary. */
 static const int8_t taps[5][2]={{0,0},{-96,0},{96,0},{0,-96},{0,96}};
 int hit[5]={0},covered=0;
 int bx=(xq-city_sun_bin_x*Q)/(32*Q),by=(yq-city_sun_bin_y*Q)/(32*Q);
 int bin=(unsigned)bx<32&&(unsigned)by<32?by*32+bx:-1;
 unsigned count=bin<0||city_sun_bin_count[bin]==255?city_caster_count:city_sun_bin_count[bin];
 for(unsigned i=0;i<count;i++){
  unsigned index=bin<0||city_sun_bin_count[bin]==255?i:city_sun_bins[bin][i];
  const CitySunCaster*c=&city_casters[index];const CitySunShape*s=&c->shape;
  if(s->kind==SUN_COURT||height>=s->height+s->ridge)continue;
  int reach=(s->height+s->ridge-height)*CITY_SUN_X;
  if(xq<s->x*Q-128||yq<s->y*Q-128||xq>(s->x+s->w)*Q+reach+128||yq>(s->y+s->d)*Q+reach+128)continue;
  for(int tap=0;tap<5;tap++)if(!hit[tap]&&sun_contains(c,xq+taps[tap][0],yq+taps[tap][1],height,city_sun_frame)){hit[tap]=1;covered++;}
  if(covered==5)break;
 }
 if(covered)city_sun_stats[3]++;
 return covered*256/5;
}
static uint16_t sun_tint(int occlusion){
 int r=31-12*occlusion/256,g=31-9*occlusion/256,b=30-5*occlusion/256;
 return (uint16_t)(0x8000|r|(g<<5)|(b<<10));
}
