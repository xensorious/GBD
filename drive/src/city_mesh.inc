/* Free Roam: an orthographic 3D scene, with the camera heading locked to
 * the player's heading. Physics and the fixed map stay in world XY.
 *
 * Projection is fixed-point on both targets. DS polygons are rasterized by
 * the geometry engine; native diagnostics use the same vertices/textures
 * with a depth-buffered reference rasterizer (never shipped in the ROM).
 */
#ifdef ARM9
#include <nds.h>
#include "platform.h"
#endif
typedef struct {int x,y,z,u,v;} CityVertex;
typedef struct {int cx,cy,sn,cs,minx,miny,maxx,maxy;} CityView;
static CityView city_view;
volatile uint32_t city_mesh_stats[4]; /* submitted quads, culled, uploads, heading */
volatile uint32_t city_mesh_last_stats[6]; /* prepared scene for read-only QA */
static int city_layer;
/* Ground is a composed flat surface. It occupies a dedicated far-depth band;
 * scenery keeps physical height/depth. Subpixel rounding of a tilted road
 * must never make asphalt compete with its own curb or sidewalk. */
static int city_ground_pass;
static int city_shadow_pass;
enum {CITY_SHADOW_ATLAS=-2,CITY_SHADOW_CAR=100};
#ifndef ARM9
static int16_t city_depth[256*192];
static uint16_t city_player_texture[48*72];
static Surface *city_target;
static uint8_t city_shadow_stencil[256*192];
#endif
#include "city_sun.inc"

/* x/y and height are world units; results have four fractional bits.
 * A 60-degree depression gives real walls height without hiding the streets.
 * The fourth result is depth toward the camera, not screen Y ordering.
 */
static CityVertex city_vertex_q8(int xq,int yq,int height,int u,int v){
 int dx=xq-city_view.cx,dy=yq-city_view.cy;
 int right=(dx*city_view.cs+dy*city_view.sn)/256;
 int down=(-dx*city_view.sn+dy*city_view.cs)/256;
 CityVertex p={128*16+right*CITY_ZOOM_NUM/(16*CITY_ZOOM_DEN),112*16+(down*222/256-height*128)*CITY_ZOOM_NUM/(16*CITY_ZOOM_DEN),
               (down*128/256+height*222)/16+city_layer,u,v};
 return p;
}
static CityVertex city_vertex(int x,int y,int height,int u,int v){return city_vertex_q8(x*Q,y*Q,height,u,v);}
/* Reject offscreen cells before footprint/planting tests. The bounds include
 * every overhanging roof, canopy, raised prop and short decorative shadow. */
#ifndef ARM9
int city_unculled_diagnostic;
#endif
static int city_visible(int x,int y,int radius,int height){
#ifndef ARM9
 if(city_unculled_diagnostic)return 1;
#endif
 CityVertex p=city_vertex(x,y,0,0,0);
 int rx=radius*(city_abs(city_view.sn)+city_abs(city_view.cs))*CITY_ZOOM_NUM/(16*CITY_ZOOM_DEN)+32;
 int ry=rx*222/256+32,up=height*8*CITY_ZOOM_NUM/CITY_ZOOM_DEN;
 return p.x+rx>=0&&p.x-rx<4096&&p.y+ry>=0&&p.y-ry-up<3072;
}
void city_project_point(int x,int y,int height,int camera_x,int camera_y,int angle,int*out){
 CityView saved=city_view;int layer=city_layer;city_layer=0;
 city_view=(CityView){camera_x*Q,camera_y*Q,city_sin(angle),city_cos(angle),0,0,0,0};
 CityVertex p=city_vertex(x,y,height,0,0);out[0]=p.x;out[1]=p.y;out[2]=p.z;
 city_view=saved;city_layer=layer;
}
#ifndef ARM9
static int64_t city_edge(CityVertex a,CityVertex b,int x,int y){
 return (int64_t)(x-a.x)*(b.y-a.y)-(int64_t)(y-a.y)*(b.x-a.x);
}
static uint16_t city_texel(int texture,int u,int v){
 if(texture==CITY_SHADOW_ATLAS)return city_mesh_idx[clampi(v,0,511)*256+clampi(u,0,255)]?C(16,28,48):0;
 if(texture>=CITY_SHADOW_CAR){
  int model=texture-CITY_SHADOW_CAR;u=clampi(u,0,31)*48/32;v=clampi(v,0,63)*72/64;
  return overhead_indexed[(model*3+1)*48*72+v*48+u]?C(16,28,48):0;
 }
 if(texture<0)return 0xffff;
 if(texture==0)return city_mesh_palette[city_mesh_idx[clampi(v,0,511)*256+clampi(u,0,255)]];
 if(texture==1){
  if((unsigned)u>=48||(unsigned)v>=72)return 0;
  return city_player_texture[v*48+u];
 }
 int model=texture-2;
 u=clampi(u,0,31)*48/32;v=clampi(v,0,63)*72/64;
 return overhead_palette[model*256+overhead_indexed[(model*3+1)*48*72+v*48+u]];
}
static void city_triangle(CityVertex a,CityVertex b,CityVertex c,int texture,uint16_t ca,uint16_t cb,uint16_t cc,int alpha){
 int64_t area=city_edge(a,b,c.x,c.y);if(!area)return;
 if(area<0){CityVertex swap=b;b=c;c=swap;uint16_t color=cb;cb=cc;cc=color;area=-area;}
 int minx=clampi((a.x<b.x?(a.x<c.x?a.x:c.x):(b.x<c.x?b.x:c.x))/16,0,255);
 int miny=clampi((a.y<b.y?(a.y<c.y?a.y:c.y):(b.y<c.y?b.y:c.y))/16,0,191);
 int maxx=clampi((a.x>b.x?(a.x>c.x?a.x:c.x):(b.x>c.x?b.x:c.x))/16,0,255);
 int maxy=clampi((a.y>b.y?(a.y>c.y?a.y:c.y):(b.y>c.y?b.y:c.y))/16,0,191);
 for(int y=miny;y<=maxy;y++)for(int x=minx;x<=maxx;x++){
  int px=x*16+8,py=y*16+8;
  int64_t wa=city_edge(b,c,px,py),wb=city_edge(c,a,px,py),wc=city_edge(a,b,px,py);
  if(wa<0||wb<0||wc<0)continue;
  int z=(int)((wa*a.z+wb*b.z+wc*c.z)/area),at=y*256+x;
  if(z<city_depth[at])continue;
  if(city_shadow_pass&&city_shadow_stencil[at])continue;
  int u=(int)((wa*a.u+wb*b.u+wc*c.u)/area)/16;
  int v=(int)((wa*a.v+wb*b.v+wc*c.v)/area)/16;
  uint16_t texel=city_texel(texture,u,v);if(!texel)continue;
  int tr=(int)((wa*(ca&31)+wb*(cb&31)+wc*(cc&31))/area);
  int tg=(int)((wa*(ca>>5&31)+wb*(cb>>5&31)+wc*(cc>>5&31))/area);
  int tb=(int)((wa*(ca>>10&31)+wb*(cb>>10&31)+wc*(cc>>10&31))/area);
  int r=(texel&31)*tr/31,g=(texel>>5&31)*tg/31,bv=(texel>>10&31)*tb/31;
  if(alpha<31){
   uint16_t under=city_target->pixels[at];
   r=(r*alpha+(under&31)*(31-alpha))/31;
   g=(g*alpha+(under>>5&31)*(31-alpha))/31;
   bv=(bv*alpha+(under>>10&31)*(31-alpha))/31;
  }else city_depth[at]=(int16_t)z;
  city_target->pixels[at]=(uint16_t)(0x8000|r|(g<<5)|(bv<<10));
  if(city_shadow_pass)city_shadow_stencil[at]=1;
 }
}
#endif
static void city_quad_colors(CityVertex a,CityVertex b,CityVertex c,CityVertex d,int texture,const uint16_t*tint,int alpha){
 if((a.x<0&&b.x<0&&c.x<0&&d.x<0)||(a.x>=4096&&b.x>=4096&&c.x>=4096&&d.x>=4096)||
    (a.y<0&&b.y<0&&c.y<0&&d.y<0)||(a.y>=3072&&b.y>=3072&&c.y>=3072&&d.y>=3072)){
  city_mesh_stats[1]++;return;
 }
 city_mesh_stats[0]++;
 if(city_shadow_pass&&texture==0)texture=CITY_SHADOW_ATLAS;
 if(city_ground_pass)a.z=b.z=c.z=d.z=-7000+city_layer*32;
 if(city_shadow_pass)a.z=b.z=c.z=d.z=-5600;
#ifdef ARM9
 uint32_t format=0,palette=0;
 if(texture==0||texture==CITY_SHADOW_ATLAS)format=(5u<<20)|(6u<<23)|(GL_RGB256<<26)|GL_TEXTURE_COLOR0_TRANSPARENT;
 else if(texture==1)format=((0x20000u+98304u)>>3)|(3u<<20)|(4u<<23)|(GL_RGBA<<26);
 else if(texture>=2){unsigned model=texture>=CITY_SHADOW_CAR?(unsigned)texture-CITY_SHADOW_CAR:(unsigned)texture-2;
  format=((0x20000u+model*2048u)>>3)|(2u<<20)|(3u<<23)|(GL_RGB256<<26)|GL_TEXTURE_COLOR0_TRANSPARENT;
  palette=(512u+model*512u)>>4;
 }
 if(texture==CITY_SHADOW_ATLAS||texture>=CITY_SHADOW_CAR)palette=(512u+VEHICLE_COUNT*512u)>>4;
 GFX_TEX_FORMAT=format;GFX_PAL_FORMAT=palette;
 glPolyFmt(POLY_ALPHA(alpha)|POLY_CULL_NONE|POLY_ID(city_shadow_pass?50:a.z>7000?62:texture>=1?40:1));
 int gouraud=tint[0]!=tint[1]||tint[0]!=tint[2]||tint[0]!=tint[3];
 if(!gouraud)glColor(tint[0]&0x7fff);
 glBegin(GL_QUADS);
 const CityVertex p[4]={a,b,c,d};
 for(int i=0;i<4;i++){
  if(gouraud)glColor(tint[i]&0x7fff);
  glTexCoord2t16((t16)p[i].u,(t16)p[i].v);
  /* Keep Q4 coordinates directly in v16. Multiplying by four limited road
   * corners to +/-512 pixels and could wrap a clipped, long street during
   * rotation. The matching projection below preserves the same screen scale. */
  glVertex3v16((v16)p[i].x,(v16)p[i].y,(v16)p[i].z);
 }
 glEnd();
#else
 city_triangle(a,b,c,texture,tint[0],tint[1],tint[2],alpha);city_triangle(a,c,d,texture,tint[0],tint[2],tint[3],alpha);
#endif
}
static void city_quad(CityVertex a,CityVertex b,CityVertex c,CityVertex d,int texture,uint16_t tint,int alpha){
 uint16_t colors[4]={tint,tint,tint,tint};city_quad_colors(a,b,c,d,texture,colors,alpha);
}
static void city_plane(int x,int y,int w,int h,int height,int u,int v,int tw,int th,uint16_t tint,int alpha){
 if(w<=0||h<=0)return;
 city_quad(city_vertex(x,y,height,u*16,v*16),city_vertex(x+w,y,height,(u+tw)*16-1,v*16),
           city_vertex(x+w,y+h,height,(u+tw)*16-1,(v+th)*16-1),city_vertex(x,y+h,height,u*16,(v+th)*16-1),
           tw?0:-1,tint,alpha);
}
static void city_flat(int x,int y,int w,int h,int height,uint16_t color){
 int x2=x+w,y2=y+h;
 if(x<city_view.minx)x=city_view.minx;
 if(y<city_view.miny)y=city_view.miny;
 if(x2>city_view.maxx)x2=city_view.maxx;
 if(y2>city_view.maxy)y2=city_view.maxy;
 city_plane(x,y,x2-x,y2-y,height,0,0,0,0,color,31);
}
static void city_wall(int x1,int y1,int x2,int y2,int bottom,int top,int u,int v,int tw,int th,uint16_t tint){
 city_quad(city_vertex(x1,y1,top,u*16,v*16),city_vertex(x2,y2,top,(u+tw)*16-1,v*16),
           city_vertex(x2,y2,bottom,(u+tw)*16-1,(v+th)*16-1),city_vertex(x1,y1,bottom,u*16,(v+th)*16-1),
           tw?0:-1,tint,31);
}
static void city_box(int x,int y,int w,int d,int bottom,int top,uint16_t c){
 city_wall(x,y,x+w,y,bottom,top,0,0,0,0,c);
 city_wall(x+w,y,x+w,y+d,bottom,top,0,0,0,0,c);
 city_wall(x+w,y+d,x,y+d,bottom,top,0,0,0,0,c);
 city_wall(x,y+d,x,y,bottom,top,0,0,0,0,c);
 city_plane(x,y,w,d,top,0,0,0,0,c,31);
}
static void city_sun_ground_pads(void){
 city_ground_pass=1;city_layer=36;
 for(unsigned i=0;i<city_caster_count;i++){
  const CitySunShape*s=&city_casters[i].shape;
  if(s->kind==SUN_BUILDING)city_plane(s->x-2,s->y-2,s->w+4,s->d+4,1,224,432,32,32,0xffff,31);
  else if(s->kind==SUN_COURT)city_plane(s->x,s->y,s->w,s->d,0,224,432,32,32,0xffff,31);
 }city_ground_pass=0;city_layer=0;
}
static void city_sun_cast(void){
 city_shadow_pass=1;
 for(unsigned i=0;i<city_caster_count;i++){
  const CitySunCaster*c=&city_casters[i];const CitySunShape*s=&c->shape;
  if(s->kind==SUN_TREE){
   int xq=s->x*Q+s->height*CITY_SUN_X+(int)((city_sun_frame/30)&1u)*Q;
   int yq=s->y*Q+s->height*CITY_SUN_Y,u=s->texture*48*16,v=256*16;
   city_quad(city_vertex_q8(xq,yq,0,u,v),city_vertex_q8(xq+48*Q,yq,0,u+48*16-1,v),
    city_vertex_q8(xq+48*Q,yq+48*Q,0,u+48*16-1,v+48*16-1),city_vertex_q8(xq,yq+48*Q,0,u,v+48*16-1),
    CITY_SHADOW_ATLAS,0xffff,12);
  }else if(s->kind==SUN_BUILDING||s->kind==SUN_LAMP){
   CityVertex p[8];
   for(int j=0;j<c->count;j++)p[j]=city_vertex_q8(s->x*Q+c->hull[j].x*16,s->y*Q+c->hull[j].y*16,0,0,0);
   for(int j=1;j+1<c->count;j++)city_quad(p[0],p[j],p[j+1],p[j+1],-1,C(16,28,48),12);
  }
 }city_shadow_pass=0;
}
static void city_mesh_ground(void){
 city_ground_pass=1;
 const int x0=city_floor(city_view.minx,64),y0=city_floor(city_view.miny,32);
 city_layer=0;
 for(int y=y0;y<city_view.maxy;y+=32)for(int x=x0;x<city_view.maxx;x+=64){
  int water=city_water(x+32,y+16);
  city_plane(x,y,64,32,-2,192,water?464:400,64,32,0xffff,31);
 }
 city_layer=1;
 /* Shore paving is map geometry too, and rotates with the canal. */
 city_flat(1898,1152,364,CITY_SIZE-1152,-1,C(153,158,131));
 city_flat(1908,1152,344,CITY_SIZE-1152,0,C(159,173,163));
 city_layer=5;
 for(int y=city_floor(city_view.miny,32);y<city_view.maxy;y+=32)if(y>=1152&&y<CITY_SIZE)
  for(int x=1920;x<2240;x+=64)
   if(x+64>=city_view.minx&&x<=city_view.maxx)city_plane(x,y,64,32,0,192,464,64,32,0xffff,31);
 for(int pass=0;pass<3;pass++)for(int i=0;i<city_road_count;i++){
  const CityRoad*r=&city_roads[i];
  int x=r->x1-r->half,y=r->y1-r->half,w=r->x2-r->x1+2*r->half,h=r->y2-r->y1+2*r->half;
  city_layer=12+pass*4;
  if(pass==0)city_flat(x-10,y-10,w+20,h+20,0,C(179,184,166));
  if(pass==1)city_flat(x-2,y-2,w+4,h+4,0,C(119,145,139));
  if(pass==2)city_flat(x,y,w,h,0,C(56,68,76));
 }
 city_layer=24;
 for(int i=0;i<city_road_count;i++){
  const CityRoad*r=&city_roads[i];int hor=r->y1==r->y2,begin=hor?r->x1:r->y1,end=hor?r->x2:r->y2;
  int fixed=hor?r->y1:r->x1,view=hor?city_view.minx:city_view.miny,limit=hor?city_view.maxx:city_view.maxy;
  for(int side=-1;side<=1;side+=2){
   if(hor){city_flat(begin,fixed+side*2,end-begin,1,0,C(222,183,102));city_flat(begin,fixed+side*(r->half-3),end-begin,1,0,C(164,175,174));}
   else{city_flat(fixed+side*2,begin,1,end-begin,0,C(222,183,102));city_flat(fixed+side*(r->half-3),begin,1,end-begin,0,C(164,175,174));}
  }
  if(r->highway)for(int at=city_floor(view,48);at<limit;at+=48){
   int a=at<begin?begin:at,b=at+24>end?end:at+24;if(b<=a)continue;
   for(int side=-1;side<=1;side+=2)
    if(hor)city_flat(a,fixed+side*28,b-a,2,0,C(215,218,200));
    else city_flat(fixed+side*28,a,2,b-a,0,C(215,218,200));
  }
 }
 /* Crossroads erase lane paint on their actual shared pavement footprint. */
 city_layer=28;
 for(int i=0;i<city_road_count;i++)if(city_roads[i].y1==city_roads[i].y2)
  for(int j=0;j<city_road_count;j++)if(city_roads[j].x1==city_roads[j].x2){
   const CityRoad*a=&city_roads[i],*b=&city_roads[j];int x=b->x1,y=a->y1;
   if(x<a->x1||x>a->x2||y<b->y1||y>b->y2)continue;
   city_flat(x-b->half,y-a->half,2*b->half,2*a->half,0,C(56,68,76));
   city_layer=32;
   if(!a->highway||!b->highway)for(int side=-1;side<=1;side+=2)for(int n=-18;n<20;n+=7){
    city_flat(x+n,y+side*(a->half-9),4,7,0,C(219,216,191));
    if(!a->highway)city_flat(x+side*(b->half-9),y+n,7,4,0,C(219,216,191));
   }
   city_layer=28;
  }
 city_ground_pass=0;
 /* Both bridge parapets are real raised rails. */
 city_layer=0;
 for(int i=0;i<city_road_count;i++){
  const CityRoad*r=&city_roads[i];
  if(r->y1!=r->y2||r->y1<=1152||r->x1>1920||r->x2<2240||r->y1<city_view.miny-20||r->y1>city_view.maxy+20)continue;
  for(int side=-1;side<=1;side+=2){
   int x=1920,y=r->y1+side*(r->half+4);
   if(x+320<city_view.minx||x>city_view.maxx)continue;
   for(int at=0;at<320;at+=40)city_box(x+at,y-1,4,4,1,9,C(219,220,196));
   for(int at=0;at<320;at+=40)city_wall(1920+at,y,1960+at,y,7,9,0,0,0,0,C(197,209,198));
  }
 }
}
#ifndef ARM9
/* Test entry point: independent camera shifts can inspect the exact ground
 * mesh without vehicles, animated water, building shadows, or UI pixels. */
void city_draw_rotated_ground(Surface*s,int cx,int cy,int angle){
 city_view=(CityView){cx*Q,cy*Q,city_sin(angle),city_cos(angle),cx-256,cy-300,cx+256,cy+300};
 city_target=s;city_mesh_stats[0]=city_mesh_stats[1]=0;
 for(int i=0;i<256*192;i++){city_depth[i]=-32768;s->pixels[i]=C(40,132,155);}
 city_mesh_ground();
}
#endif
static void city_mesh_building(int col,int row){
 CityLot l=city_lot_bounds(col,row);if(!l.kind)return;
 int style=city_building_style(col,row),x=l.x,y=l.y,w=l.w,d=l.h;
 int height=sun_building_height(style),pitched=sun_pitched(style);
 city_layer=0;
 /* Wall normals stay in world space while the camera rotates. */
 city_wall(x,y,x+w,y,1,height,style%4*64,style/4*32,64,32,C(255,247,230));
 city_wall(x+w,y,x+w,y+d,1,height,style%8*32,192+style/8*32,32,32,C(174,193,213));
 city_wall(x+w,y+d,x,y+d,1,height,style%4*64,style/4*32,64,32,C(191,207,222));
 city_wall(x,y+d,x,y,1,height,style%8*32,192+style/8*32,32,32,C(255,247,230));
 int u=style%8*32,v=128+style/8*32;
 if(pitched){
  int ridge=height+12,mid=y+d/2;
  city_quad(city_vertex(x-2,y-2,height,u*16,v*16),city_vertex(x+w+2,y-2,height,(u+32)*16-1,v*16),
    city_vertex(x+w+2,mid,ridge,(u+32)*16-1,(v+16)*16),city_vertex(x-2,mid,ridge,u*16,(v+16)*16),0,0xffff,31);
  city_quad(city_vertex(x-2,mid,ridge,u*16,(v+16)*16),city_vertex(x+w+2,mid,ridge,(u+32)*16-1,(v+16)*16),
    city_vertex(x+w+2,y+d+2,height,(u+32)*16-1,(v+32)*16-1),city_vertex(x-2,y+d+2,height,u*16,(v+32)*16-1),0,C(191,208,224),31);
  /* Triangular gables close the roof, rather than leaving floating tiles. */
  city_quad(city_vertex(x,y,height,0,0),city_vertex(x,mid,ridge,0,0),
    city_vertex(x,y+d,height,0,0),city_vertex(x,y+d,height,0,0),-1,C(234,227,199),31);
  city_quad(city_vertex(x+w,y+d,height,0,0),city_vertex(x+w,mid,ridge,0,0),
    city_vertex(x+w,y,height,0,0),city_vertex(x+w,y,height,0,0),-1,C(184,201,214),31);
 }else{
  city_plane(x-2,y-2,w+4,d+4,height+1,u,v,32,32,C(255,247,230),31);
  city_wall(x-2,y-2,x+w+2,y-2,height,height+3,0,0,0,0,C(220,226,210));
  city_wall(x+w+2,y+d+2,x-2,y+d+2,height,height+3,0,0,0,0,C(184,201,214));
 }
}
static void city_mesh_garden(int x,int y,int prop,int w,int h,unsigned frame){
 if(!city_visible(x,y,(w>h?w:h)/2+8,5))return;
 if(!city_garden_clear(x-w/2,y-h/2,w,h))return;
 if(prop<8&&((frame/24)&1u))prop+=16;
 int height=prop<8||prop>=16?5:2;
 city_shadow_pass=1;
 city_plane(x-w/2+height*CITY_SUN_X/Q,y-h/2+height*CITY_SUN_Y/Q,w,h,0,prop%8*32,304+prop/8*32,32,32,0xffff,12);
 city_shadow_pass=0;
 city_layer=0;
 city_plane(x-w/2,y-h/2,w,h,height,prop%8*32,304+prop/8*32,32,32,sun_tint(city_sun_occlusion(x*Q,y*Q,height)),31);
}
static void city_mesh_tree(int x,int y,int kind,unsigned frame){
 int drift=(int)((frame/30)&1u);
 city_box(x-1,y-1,2,2,0,17,C(111,94,61));
 city_plane(x-24+drift,y-24,48,48,18,kind*48,256,48,48,sun_tint(city_sun_occlusion(x*Q,y*Q,18)),31);
}
static void city_mesh_lamp(int x,int y,int dx,int dy){
 city_box(x-2,y-2,4,4,0,2,C(106,130,126));
 city_box(x-1,y-1,2,2,2,24,C(65,89,91));
 int left=dx<0?x-7:x-1,top=dy<0?y-7:y-1;
 city_box(left,top,dx?8:2,dy?8:2,23,25,C(85,109,110));
 city_plane(x+dx*7-2,y+dy*7-2,4,4,26,0,0,0,0,C(232,219,169),31);
}
static void city_mesh_car(int x,int y,int angle,int model,int player){
 int sn=city_sin(angle),cs=city_cos(angle),texture=player?1:model+2;
 int tw=player?48:32,th=player?72:64;
 CityShape shape=city_vehicle_shape(model);
 const int side[4]={-shape.draw_half_width,shape.draw_half_width,shape.draw_half_width,-shape.draw_half_width};
 const int fore[4]={-shape.draw_half_length,-shape.draw_half_length,shape.draw_half_length,shape.draw_half_length};
 CityVertex shadow[4];
 for(int n=0;n<4;n++)shadow[n]=city_vertex_q8(x*Q+3*CITY_SUN_X+(side[n]*cs-fore[n]*sn),y*Q+3*CITY_SUN_Y+(side[n]*sn+fore[n]*cs),0,
     (n==1||n==2)?32*16-1:0,n>=2?64*16-1:0);
 city_shadow_pass=1;city_quad(shadow[0],shadow[1],shadow[2],shadow[3],CITY_SHADOW_CAR+model,0xffff,12);city_shadow_pass=0;
 enum {NX=4,NY=6};
 CityVertex grid[(NX+1)*(NY+1)];uint16_t colors[(NX+1)*(NY+1)];int low=256,high=0,total=0;
 for(int yy=0;yy<=NY;yy++)for(int xx=0;xx<=NX;xx++){
  int right=-shape.draw_half_width*Q+xx*shape.draw_half_width*2*Q/NX;
  int forward=-shape.draw_half_length*Q+yy*shape.draw_half_length*2*Q/NY;
  int xq=x*Q+(right*cs-forward*sn)/256,yq=y*Q+(right*sn+forward*cs)/256;
  int shade=city_sun_occlusion(xq,yq,3),at=yy*(NX+1)+xx;
  if(shade<low)low=shade;
  if(shade>high)high=shade;
  total+=shade;
  grid[at]=(CityVertex){xq,yq,3,xx*(tw*16-1)/NX,yy*(th*16-1)/NY};colors[at]=sun_tint(shade);
 }
 if(player){city_sun_stats[4]=(uint32_t)low;city_sun_stats[5]=(uint32_t)high;city_sun_stats[8]=(uint32_t)total/((NX+1)*(NY+1));}
 /* Keep the same fractional outline when entering/leaving a shadow. */
 if(low==high){
  const int corners[4]={0,NX,NY*(NX+1)+NX,NY*(NX+1)};CityVertex p[4];
  for(int i=0;i<4;i++){CityVertex v=grid[corners[i]];p[i]=city_vertex_q8(v.x,v.y,3,v.u,v.v);}
  city_quad(p[0],p[1],p[2],p[3],texture,sun_tint(low),31);return;
 }
 for(int i=0;i<(NX+1)*(NY+1);i++){CityVertex v=grid[i];grid[i]=city_vertex_q8(v.x,v.y,3,v.u,v.v);}
 city_sun_stats[6]++;
 for(int yy=0;yy<NY;yy++)for(int xx=0;xx<NX;xx++){
  int a=yy*(NX+1)+xx,b=a+1,c=b+NX+1,d=a+NX+1;
  uint16_t tint[4]={colors[a],colors[b],colors[c],colors[d]};
  city_quad_colors(grid[a],grid[b],grid[c],grid[d],texture,tint,31);
 }
}
static void city_screen_plane(int x,int y,int w,int h,int depth,int u,int v,int texture,uint16_t tint,int alpha){
 CityVertex a={x*16,y*16,depth,u*16,v*16},b={(x+w)*16,y*16,depth,(u+w)*16-1,v*16};
 CityVertex c={(x+w)*16,(y+h)*16,depth,(u+w)*16-1,(v+h)*16-1},d={x*16,(y+h)*16,depth,u*16,(v+h)*16-1};
 city_quad(a,b,c,d,texture,tint,alpha);
}
static void city_mesh_caption(const Game*g){
 const StudioTheme*theme=theme_for(g);const char*name=city_district(city.x/Q,city.y/Q);
 int width=10;for(const char*p=name;*p;p++)width+=*p==' '?4:7;
 city_screen_plane(7,5,width,14,7600,0,0,-1,theme->pane,25);
 int x=12;
 for(const char*p=name;*p;p++){
  if(*p>='A'&&*p<='Z')city_screen_plane(x,6,8,12,7604,(*p-'A')*8,496,0,theme->ink,31);
  x+=*p==' '?4:7;
 }
 if(g->crash_timer){
  city_screen_plane(222,6,23,12,7600,0,0,-1,C(161,54,38),25);
  city_screen_plane(232,8,2,5,7604,0,0,-1,C(255,216,151),31);
  city_screen_plane(232,15,2,1,7604,0,0,-1,C(255,216,151),31);
 }
}
#ifdef ARM9
static void city_mesh_textures(const Game*g){
 static unsigned epoch;static int vehicle=-1,pose=-1,paint=-1,livery=-1,color=-1,wheels=-1;
 int full=epoch!=ds_world_epoch(),steer=g->steering<0?0:g->steering>0?2:1,v=g->vehicle;
 if(!full&&vehicle==v&&pose==steer&&paint==g->save.paint[v]&&livery==g->save.livery[v]&&color==g->save.livery_color[v]&&wheels==g->save.wheels[v])return;
 static uint16_t pixels[48*72];
 /* Prepare a normal pose while all scenery/NPC texture banks remain mapped.
  * The short vblank window contains only the player texture row transfers. */
 if(!full){city_pixels_pose(pixels,v,steer,&g->save);DC_FlushRange(pixels,sizeof pixels);}
 ds_world_upload_begin(full);
 if(full){
  DC_FlushRange(city_mesh_idx,256*512);dmaCopyWords(0,city_mesh_idx,VRAM_A,256*512);
  DC_FlushRange(city_mesh_palette,512);dmaCopyWords(0,city_mesh_palette,VRAM_E,512);
  for(int model=0;model<VEHICLE_COUNT;model++){
   city_pixels_pose(pixels,model,1,0);
   /* VRAM only supports halfword/word stores, including indexed texels. */
   uint16_t*dest=(uint16_t*)((uint8_t*)VRAM_D+model*2048);
   const uint8_t*source=city_pose_art+48*72;
   for(int y=0;y<64;y++)for(int x=0;x<32;x+=2)
    *dest++=(uint16_t)(source[y*72/64*48+x*48/32]|(source[y*72/64*48+(x+1)*48/32]<<8));
  }
  DC_FlushRange(overhead_palette,VEHICLE_COUNT*512);dmaCopyWords(0,overhead_palette,(uint8_t*)VRAM_E+512,VEHICLE_COUNT*512);
  dmaFillHalfWords(C(16,28,48),(uint8_t*)VRAM_E+512+VEHICLE_COUNT*512,512);
  dmaFillHalfWords(0,(uint8_t*)VRAM_D+98304,64*128*2);
 }
 if(full){city_pixels_pose(pixels,v,steer,&g->save);DC_FlushRange(pixels,sizeof pixels);}
 for(int y=0;y<72;y++)dmaCopyHalfWords(0,pixels+y*48,(uint8_t*)VRAM_D+98304+y*64*2,48*2);
 ds_world_upload_end(full);city_mesh_stats[2]++;
 epoch=ds_world_epoch();vehicle=v;pose=steer;paint=g->save.paint[v];livery=g->save.livery[v];color=g->save.livery_color[v];wheels=g->save.wheels[v];
}
#endif
static void city_scene(Surface*s,const Game*g){
 for(int k=0;k<8;k++)city_profile[k]=0;
 unsigned stage=profile_clock();
 city_mesh_stats[0]=city_mesh_stats[1]=0;city_mesh_stats[3]=city.angle;
 int cx=city.camera_x/Q,cy=city.camera_y/Q;
 city_sun_prepare(cx,cy,g->frame);
 city_sun_stats[2]=city_sun_stats[3]=city_sun_stats[6]=0;city_sun_stats[7]=g->frame;
 city_view=(CityView){city.camera_x,city.camera_y,city_sin(city.angle),city_cos(city.angle),cx-256,cy-300,cx+256,cy+300};
#ifdef ARM9
 city_mesh_textures(g);
 glMatrixMode(GL_PROJECTION);glLoadIdentity();
 glOrthof32(0,1<<12,3<<10,0,-(2<<12),2<<12);
 glMatrixMode(GL_MODELVIEW);glLoadIdentity();
 glMatrixMode(GL_TEXTURE);glLoadIdentity();
 (void)s; /* Hardware polygons fill the screen; no RAM framebuffer copy. */
#else
 city_target=s;city_pixels_pose(city_player_texture,g->vehicle,g->steering<0?0:g->steering>0?2:1,&g->save);
 for(int i=0;i<256*192;i++){city_depth[i]=-32768;city_shadow_stencil[i]=0;s->pixels[i]=C(40,132,155);}
#endif
 city_mesh_ground();
 city_sun_ground_pads();
 city_sun_cast();
 city_layer=0;
 for(int y=city_floor(city_view.miny,64);y<city_view.maxy;y+=64)
  for(int x=city_floor(city_view.minx,64);x<city_view.maxx;x+=64)
   if(city_water(x,y)&&!city_on_road(x,y)&&!city_on_road(x+12,y)){
    city_plane(x+(int)(g->frame/12)%5,y,10,1,1,0,0,0,0,C(84,165,179),31);
   }
 city_profile[0]=(profile_clock()-stage)&65535;stage=profile_clock();
 for(int row=clampi(city_view.miny/128-1,0,31);row<=clampi(city_view.maxy/128+1,0,31);row++)
  for(int col=clampi(city_view.minx/128-1,0,31);col<=clampi(city_view.maxx/128+1,0,31);col++){
   if(!city_visible(col*128+64,row*128+64,80,54))continue;
   CityLot lot=city_lot_bounds(col,row);int wx=col*128+64,wy=row*128+64;
   unsigned decor=road_hash(0x47415244u,(unsigned)(row*32+col));
   if(lot.kind){
    city_mesh_building(col,row);
    int front=lot.y+lot.h+12;
    city_mesh_garden(lot.x+6,front,(int)(decor%4),20,16,g->frame);
    city_mesh_garden(lot.x+lot.w-8,front,6,24,14,g->frame);
    city_mesh_garden(lot.x-12,lot.y+lot.h/2,4+(decor&1),14,30,g->frame);
    city_mesh_garden(lot.x+lot.w+12,lot.y+12,(int)((decor>>4)%4),18,18,g->frame);
   }else if(city_garden_clear(wx-28,wy-26,56,52)){
    int park=(int)(decor%4);
    if(park<2)city_mesh_garden(wx,wy,park?8:13,42,40,g->frame);
    else{int prop=park==2?12:13,p=prop-8;city_plane(wx-22,wy-19,44,38,2,p%4*48,400+p/4*48,48,48,sun_tint(city_sun_occlusion(wx*Q,wy*Q,2)),31);}
    city_mesh_garden(wx-24,wy+23,(int)((decor>>5)%4),20,16,g->frame);
    city_mesh_garden(wx+25,wy-22,7,22,20,g->frame);
   }
   int tx=col*128+13+(int)((decor>>10)%9),ty=row*128+12+(int)((decor>>14)%9);
   if(!city_water(tx,ty)&&!city_on_road(tx,ty)&&!city_solid(tx,ty))city_mesh_tree(tx,ty,(int)((decor>>8)%5),g->frame);
   if(row>18||(decor&3)==0){
    city_mesh_garden(col*128+100,row*128+9,(int)((decor>>3)%4),24,20,g->frame);
    city_mesh_garden(col*128+12,row*128+103,7,28,22,g->frame);
   }
  }
 for(int i=0;i<city_road_count;i++){
  const CityRoad*r=&city_roads[i];int hor=r->y1==r->y2;
  int view=hor?city_view.minx:city_view.miny,limit=hor?city_view.maxx:city_view.maxy;
  int begin=hor?r->x1:r->y1,end=hor?r->x2:r->y2;
  for(int at=city_floor(view,256);at<limit;at+=256)if(at>begin+65&&at<end-65)
   for(int side=-1;side<=1;side+=2){
    int x=hor?at:r->x1+side*(r->half+13),y=hor?r->y1+side*(r->half+13):at;
    if(x<city_view.minx||x>city_view.maxx||y<city_view.miny||y>city_view.maxy)continue;
    if(city_garden_clear(x-6,y-6,12,12))city_mesh_lamp(x,y,hor?0:-side,hor?-side:0);
   }
 }
 /* Connected marina fingers and boats lie in the water plane. */
 for(int berth=0;berth<14;berth++){
  int y=1248+berth*160;if(y<city_view.miny-60||y>city_view.maxy+60)continue;
  int side=berth&1,x=side?2240:1920,boat=berth%3;
  if(x<city_view.minx-80||x>city_view.maxx+80)continue;
  int left=side?x-63:x;
  city_plane(left,y-3,63,7,2,0,0,0,0,C(151,121,80),31);
  for(int p=0;p<63;p+=5)city_flat(left+p,y-3,1,7,2,C(198,169,111));
  city_box(left,y-3,3,7,0,4,C(108,87,63));city_box(left+60,y-3,3,7,0,4,C(108,87,63));
  int boatx=side?x-42:x+42;
  city_plane(boatx-16,y+5,32,58,1,boat*48,400,48,48,sun_tint(city_sun_occlusion(boatx*Q,(y+34)*Q,1)),31);
 }
 city_profile[1]=(profile_clock()-stage)&65535;stage=profile_clock();
 city_layer=2;
 for(int i=0;i<64;i++)if(!city.tokens[i]){
  const CityRoad*r=&city_roads[i%city_road_count];int span=r->x2-r->x1+r->y2-r->y1;
  int p=(int)(road_hash(0x434f494eu,(unsigned)i)%(unsigned)span);
  int x=r->x1+(r->x2!=r->x1?p:12),y=r->y1+(r->y2!=r->y1?p:12);
  if(x<city_view.minx||x>city_view.maxx||y<city_view.miny||y>city_view.maxy)continue;
  city_plane(x-3,y-3,6,6,4,0,0,0,0,C(246,194,73),31);
  city_plane(x-1,y-2,1,4,5,0,0,0,0,C(255,242,159),31);
 }
 for(int i=0;i<CITY_TRAFFIC;i++){
  CityCar*c=&city.cars[i];
  if(city_abs(c->x-cx)>240||city_abs(c->y-cy)>280)continue;
  city_mesh_car(c->x,c->y,c->angle,c->model,0);
 }
 if(g->drifting&&g->save.smoke[g->vehicle]){
  static const uint16_t smoke[5]={C(199,211,212),C(241,146,138),C(83,205,188),C(172,133,225),C(231,194,105)};
  CityShape shape=city_vehicle_shape(g->vehicle);
  int x=city.x/Q-city_sin(city.angle)*(shape.draw_half_length-2)/256,y=city.y/Q+city_cos(city.angle)*(shape.draw_half_length-2)/256;
  city_plane(x-5,y-5,10,10,3,0,0,0,0,smoke[g->save.smoke[g->vehicle]],18);
 }
 city_mesh_car(city.x/Q,city.y/Q,city.angle,g->vehicle,1);
 /* Small world-space gulls cross the canal, with real height and wingbeats. */
 for(int gull=0;gull<2;gull++){
  int x=1960+(int)((g->frame/2+(unsigned)gull*97)%250),y=1300+gull*740+(int)((g->frame/3)%900);
  if(city_abs(x-cx)>230||city_abs(y-cy)>260)continue;
  int wing=((g->frame/7+(unsigned)gull)&1u)?3:0;
  city_quad(city_vertex(x-4,y,35+wing,0,0),city_vertex(x,y+1,35,0,0),
    city_vertex(x,y+2,35,0,0),city_vertex(x-4,y+1,35+wing,0,0),-1,C(243,243,222),31);
  city_quad(city_vertex(x,y+1,35,0,0),city_vertex(x+4,y,35+wing,0,0),
    city_vertex(x+4,y+1,35+wing,0,0),city_vertex(x,y+2,35,0,0),-1,C(243,243,222),31);
 }
 city_mesh_caption(g);
 city_profile[3]=(profile_clock()-stage)&65535;
 /* The platform submits the completed scene at the paced LCD boundary.
  * Flushing here made fast/slow preparations alternate 1/3 visible frames. */
 for(int k=0;k<4;k++)city_mesh_last_stats[k]=city_mesh_stats[k];
#ifdef ARM9
 city_mesh_last_stats[4]=GFX_CONTROL&(GL_COLOR_UNDERFLOW|GL_POLY_OVERFLOW);
#else
 city_mesh_last_stats[4]=0;
#endif
 city_mesh_last_stats[5]=g->frame;
 for(int k=0;k<8;k++)city_last_profile[k]=city_profile[k];
}
