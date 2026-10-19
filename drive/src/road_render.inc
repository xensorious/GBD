/* Shared scanline kernel: specialize the ordinary DS view at compile time. */
static ROAD_SPEED __attribute__((noinline)) void ROAD_FUNCTION(Surface *s,const Game *g,const View *v)
{
 RoadSample knots[ROAD_KNOTS];
 RoadSample materials[ROAD_KNOTS];
 int knot_x[ROAD_KNOTS];
 uint16_t grounds[ROAD_KNOTS],verges[ROAD_KNOTS],seas[ROAD_KNOTS],asphalts[ROAD_KNOTS];
 int left_water[ROAD_KNOTS],right_water[ROAD_KNOTS],bridges[ROAD_KNOTS],rails[ROAD_KNOTS];
 for(int k=0;k<=ROAD_HEIGHT/8;k++) {
  int y=k*8;if(y>=ROAD_HEIGHT)y=ROAD_HEIGHT-1;
  road_sample(v->seed,v->position+row_z[y],v->start,&knots[k]);
  road_sample(v->seed,physical_anchor(v->visual_position+row_z[y],v->visual_scale),v->start,&materials[k]);
  knot_x[k]=knots[k].center;
  const RoadSample *r=&materials[k];
  int transition=material_blend(r->blend);
  grounds[k]=blend_color(terrain_color(r->biome),terrain_color(r->next_biome),transition);
  verges[k]=blend_color(verge_color(r->biome),verge_color(r->next_biome),transition);
  seas[k]=blend_color(sea_color(r->biome),sea_color(r->next_biome),transition);
  left_water[k]=(biome_water(r->biome,-1)*(256-transition)+biome_water(r->next_biome,-1)*transition)/256;
  right_water[k]=(biome_water(r->biome,1)*(256-transition)+biome_water(r->next_biome,1)*transition)/256;
  bridges[k]=bridge_region(r->biome)*(256-transition)+bridge_region(r->next_biome)*transition;
  rails[k]=(biome_water(r->biome,-1)!=0)*(256-transition)+(biome_water(r->next_biome,-1)!=0)*transition;
  uint16_t road_a=r->biome==NEON?C(28,36,56):r->biome==AURORA?C(65,83,102):
                   r->biome==SALT?C(91,98,116):r->biome==MARINA?C(49,61,78):C(64,77,85);
  uint16_t road_b=r->next_biome==NEON?C(28,36,56):r->next_biome==AURORA?C(65,83,102):
                   r->next_biome==SALT?C(91,98,116):r->next_biome==MARINA?C(49,61,78):C(64,77,85);
  asphalts[k]=blend_color(road_a,road_b,transition);
 }
 for(int y=ROAD_HORIZON+1;y<ROAD_HEIGHT;y++) {
  int k=y/8,t=y&7;
  int center=knot_x[k]+((knot_x[k+1]-knot_x[k])*t)/8;
  int scale=row_scale[y];
  int cx=128+((center-v->center-v->cam_x)*scale)/256;
  int half=(ROAD_HALF*scale)/256;
  road_x[y]=cx;road_half[y]=half;
  const RoadSample *r=&materials[k];
  int biome=r->blend>=128?r->next_biome:r->biome;
  row_biome[y]=(uint8_t)biome;row_feature[y]=r->feature;
  uint16_t ground=blend_color(grounds[k],grounds[k+1],t*32);
  uint16_t sea=blend_color(seas[k],seas[k+1],t*32);
  uint16_t verge=blend_color(verges[k],verges[k+1],t*32);
  uint16_t asphalt=blend_color(asphalts[k],asphalts[k+1],t*32);
  int haze=clampi(ROAD_HORIZON+52-y,0,50);
  int transition=material_blend(r->blend);
  uint16_t haze_color=blend_color(sky_themes[r->biome].low,sky_themes[r->next_biome].low,transition);
  if(haze){
   ground=blend_color(ground,haze_color,haze);
   sea=blend_color(sea,haze_color,haze);
   verge=blend_color(verge,haze_color,haze);
   asphalt=blend_color(asphalt,haze_color,haze);
  }
  int world=v->visual_position+row_z[y];
  /* Hot flats use a moving warm haze in existing terrain spans, rather than
   * an expensive screen filter or distortion that would hide traffic. */
  if((biome==CANYON || biome==SALT) && y<ROAD_HORIZON+66) {
   int wave=(int)((v->frame/11+(unsigned)y/3)&15u);
   int warmth=(ROAD_HORIZON+66-y)*(wave<8?wave:15-wave)/28;
   ground=blend_color(ground,biome==SALT?C(245,206,226):C(241,186,130),clampi(warmth,0,28));
   sea=blend_color(sea,C(222,201,229),clampi(warmth,0,24));
  }
  int shoulder=clampi(scale*3/256,1,5),bank=clampi(scale*3/256,1,4);
  int outside_left=cx-half-shoulder-bank;
  int outside_right=cx+half+shoulder+bank;
  int water_spans[2];
  for(int side=-1;side<=1;side+=2) {
   int water=side<0?left_water[k]+(left_water[k+1]-left_water[k])*t/8:
                     right_water[k]+(right_water[k+1]-right_water[k])*t/8;
   int edge=cx+side*(half+7*scale/256);
   int span=side<0?edge:VW-edge;
   water_spans[(side+1)/2]=clampi((span*water)/256,0,VW);
  }
  road_ground_spans(s,y,outside_left,outside_right,water_spans[0],water_spans[1],ground);

  /* Water only occupies the appropriate side of a coastal or bridge road.
   * The shoreline itself approaches gradually during an environment blend. */
  for(int side=-1;side<=1;side+=2) {
   int span=water_spans[(side+1)/2];
   if(span) {
    int x=side<0?0:VW-span;
    road_span(s,x,y,span,sea);
    if(span>0)road_span(s,side<0?span-1:x,y,1,blend_color(sea,C(180,223,197),100));
    if(span>12 && (world%43+43)%43<2) {
     unsigned wave=road_hash(v->seed,(unsigned)floor_div(world,43));
     int wx=x+3+(int)(wave%(unsigned)(span-9));
     road_span(s,wx,y,clampi(scale*7/256,2,10),blend_color(sea,C(161,220,222),90));
    }
   }
  }

  int bridge=bridges[k]+(bridges[k+1]-bridges[k])*t/8;
  if(bridge) {
   uint16_t cast=blend_color(sea,C(22,65,83),bridge*3/4);
   road_span(s,outside_left-bank,y,bank,cast);
   road_span(s,outside_right,y,bank,cast);
  }
  road_span(s,outside_left,y,bank,verge);
  road_span(s,cx+half+shoulder,y,bank,verge);
  road_span(s,cx-half-shoulder,y,shoulder,C(76,88,91));
  road_span(s,cx+half,y,shoulder,C(76,88,91));
  road_span(s,cx-half,y,half*2,asphalt);
  if(biome==NEON && (world%144+144)%144<28) {
   int reach=clampi(scale*10/256,1,7);
   road_span(s,cx-half+2,y,reach,C(48,119,147));
   road_span(s,cx+half-reach-1,y,reach,C(126,64,136));
  }
  if(biome==SALT && y<ROAD_HORIZON+55 && (world%97+97)%97<17) {
   int width=clampi(scale*120/256,8,55);
   road_span(s,outside_left-width-8,y,width,C(226,204,223));
   road_span(s,outside_right+8,y,width,C(207,221,224));
  }

  /* A little broken aggregate avoids a featureless slab, without turning the
   * road into television noise.  All positions are attached to the road. */
  if(half>8 && (world&15)==0) {
   unsigned h=road_hash(v->seed^0x40a1u,(unsigned)floor_div(world,8));
   int gx=cx-half+6+(int)(h%(unsigned)(half*2-12));
   road_span(s,gx,y,1,C(81,92,96));
  }
  int white=scale>246?2:1;
  road_span(s,cx-half+3,y,white,C(225,225,201));
  road_span(s,cx+half-4,y,white,C(225,225,201));
  int dash=(world%96+96)%96;
  int paint_length=34+clampi((v->speed-55)/6,0,v->motion==0?10:24);
  if(dash<paint_length)for(int lane=1;lane<4;lane++) {
   int x=cx-half+(half*lane)/2;
   road_span(s,x,y,white,dash<34?C(225,229,213):C(183,198,193));
  }
  if((world%66+66)%66<24) {
   road_span(s,cx-half-shoulder-2,y,2,C(237,208,148));
   road_span(s,cx+half+shoulder,y,2,C(237,208,148));
  }
  int rail=rails[k]+(rails[k+1]-rails[k])*t/8;
  if(rail) {
   for(int side=-1;side<=1;side+=2) {
    int rx=cx+side*(half+shoulder+bank);
    road_span(s,rx,y,clampi(scale*2/256,1,3),blend_color(verge,C(215,228,211),rail));
    road_span(s,rx+side*2,y,1,blend_color(verge,C(76,118,122),rail));
    if((world%84+84)%84<7)road_span(s,rx-1,y,4,C(244,235,202));
   }
  }
 }

 /* Old tyre marks are world-anchored, and disappear after a short while. */
 if(!v->attract)for(unsigned i=0;i<24;i++) {
  const TireMark *m=&tiremarks[i];
  if(!m->tick || g->run_ticks-m->tick>42)continue;
  int z=m->z-v->visual_position;
  if(z < -44 || z>ROAD_VIEW)continue;
  int y=project_y(z),x=item_x(v,m->x,z);
  int separation=clampi(project_scale(z)*8/256,4,11);
  gfx_rect(s,x-separation,y,2,4,C(43,55,62));
  gfx_rect(s,x+separation,y,2,4,C(43,55,62));
 }
}

