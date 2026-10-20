/* Fixed, connected road network. Coordinates and street identities do not
 * depend on the run seed. Only traffic colors and collectible phase vary. */
City city;
const CityRoad city_roads[]={
 {640,512,3392,512,56,1},{640,2048,3392,2048,56,1},
 {640,512,640,3584,56,1},{3392,512,3392,3584,56,1},
 {640,3584,3392,3584,56,1},{640,1024,3392,1024,26,0},
 {640,1408,1664,1408,26,0},{2496,1408,3392,1408,26,0},
 {1280,512,1280,3584,26,0},{1664,1024,1664,2048,26,0},
 {2496,512,2496,3584,26,0},{2912,1024,2912,3584,26,0},
 {640,2688,3392,2688,26,0},{640,3104,3392,3104,26,0},
 {896,2048,896,3104,26,0},{896,2432,1280,2432,26,0}
};
const int city_road_count=sizeof city_roads/sizeof city_roads[0];
static const int16_t city_wave[65]={0,6,13,19,25,31,37,44,50,56,62,68,74,80,86,92,98,104,109,115,121,126,132,137,142,147,152,157,162,167,172,177,181,185,190,194,198,202,206,210,213,217,220,223,226,229,231,234,237,239,241,243,245,246,248,249,251,252,253,254,255,255,256,256,256};
int city_sin(int angle){unsigned a=(unsigned)angle&255u,q=a>>6,p=a&63u;return q==0?city_wave[p]:q==1?city_wave[64-p]:q==2?-city_wave[p]:-city_wave[64-p];}
int city_cos(int angle){return city_sin(angle+64);}
CityShape city_vehicle_shape(int model){
 if((unsigned)model>=VEHICLE_COUNT)model=0;
 if(model==25)return (CityShape){3,6,6,9};
 if(model==28)return (CityShape){4,7,7,10};
 if(model==29)return (CityShape){3,7,6,10};
 if(model==30)return (CityShape){5,11,8,15};
 if(vehicles[model].bike)return (CityShape){3,10,7,13};
 if(model==20||model==24||model==46)return (CityShape){8,15,12,19};
 if(model==2||model==12||model==13||model==34||model==42)return (CityShape){7,14,11,18};
 if(model==7||model==35||model==40||model==41)return (CityShape){7,13,11,17};
 if(model==0||model==10||model==11)return (CityShape){6,12,9,15};
 return (CityShape){7,13,10,16};
}
int city_vehicle_overlap(int ax,int ay,int angle,int model,int bx,int by,int bangle,int bmodel,int margin){
 CityShape a=city_vehicle_shape(model),b=city_vehicle_shape(bmodel);
 int ac=city_cos(angle),as=city_sin(angle),bc=city_cos(bangle),bs=city_sin(bangle);
 int aw=a.half_width+margin,al=a.half_length+margin,bw=b.half_width+margin,bl=b.half_length+margin;
 int dx=bx-ax,dy=by-ay;
 int dot=absi(ac*bc+as*bs),cross=absi(ac*bs-as*bc);
 int norm_a=ac*ac+as*as,norm_b=bc*bc+bs*bs;
 /* Four separating axes of the two oriented chassis rectangles. Keep the
  * support terms in Q16 until after comparison, including sine-table rounding. */
 if(absi(dx*ac+dy*as)*256>=aw*norm_a+bw*dot+bl*cross)return 0;
 if(absi(-dx*as+dy*ac)*256>=al*norm_a+bw*cross+bl*dot)return 0;
 if(absi(dx*bc+dy*bs)*256>=bw*norm_b+aw*dot+al*cross)return 0;
 if(absi(-dx*bs+dy*bc)*256>=bl*norm_b+aw*cross+al*dot)return 0;
 return 1;
}
int city_on_road(int x,int y){
 for(int i=0;i<city_road_count;i++){const CityRoad*r=&city_roads[i];if(x>=r->x1-r->half&&x<=r->x2+r->half&&y>=r->y1-r->half&&y<=r->y2+r->half)return r->highway?2:1;}return 0;
}
int city_water(int x,int y){return (x>=1920&&x<2240&&y>1152)||(x>3712)||(y<256);}
CityLot city_lot_bounds(int col,int row){
 CityLot lot={0,0,0,0,0};
 if(col<2||col>28||row<3||row>29)return lot;
 /* The fixed map is immutable. Cache local byte-sized bounds instead of
  * walking every road again for each collision corner and garden test. */
 typedef struct {uint8_t x,y,w,h,kind;} CachedLot;
 static CachedLot lots[32*32];CachedLot*cached=&lots[row*32+col];
 if(cached->kind&128u)return (cached->kind&127u)?
   (CityLot){col*128+cached->x,row*128+cached->y,cached->w,cached->h,cached->kind&127u}:lot;
 cached->kind=128;
 int x=col*128,y=row*128;
 if(city_water(x+64,y+64))return lot;
 unsigned h=road_hash(0x50414c4d,col+row*32);
 if(h%7==0)return lot;
 int left=x+24,right=x+104,top=y+24,bottom=y+104;
 for(int i=0;i<city_road_count;i++){const CityRoad*r=&city_roads[i];int p=r->half+10;
  if(r->x1==r->x2&&bottom>=r->y1-p&&top<=r->y2+p){
   if(r->x1<=x+64&&r->x1+p>left)left=r->x1+p;
   if(r->x1>x+64&&r->x1-p<right)right=r->x1-p;
  }
  if(r->y1==r->y2&&right>=r->x1-p&&left<=r->x2+p){
   if(r->y1<=y+64&&r->y1+p>top)top=r->y1+p;
   if(r->y1>y+64&&r->y1-p<bottom)bottom=r->y1-p;
  }
 }
 if(right-left<30||bottom-top<30)return lot;
 lot=(CityLot){left,top,right-left,bottom-top,1+(int)(h%6)};
 *cached=(CachedLot){(uint8_t)(left-x),(uint8_t)(top-y),(uint8_t)lot.w,(uint8_t)lot.h,(uint8_t)(lot.kind|128)};
 return lot;
}
int city_lot(int col,int row){return city_lot_bounds(col,row).kind;}
int city_solid(int x,int y){
 if(x<160||y<320||x>3648||y>3800)return 1;
 if(city_on_road(x,y))return 0;
 if(city_water(x,y))return 1;
 CityLot lot=city_lot_bounds(x/128,y/128);return lot.kind&&x>=lot.x&&x<=lot.x+lot.w&&y>=lot.y&&y<=lot.y+lot.h;
}
const char *city_district(int x,int y){return y<800?"OCEAN BOULEVARD":x>2304?(y>2304?"PALMETTO HEIGHTS":"GLASS QUAY"):(y>2304?"CANAL GARDENS":"DOWNTOWN WEST");}
static void city_car_position(CityCar*c){
 static const int16_t loops[3][4]={ {640,512,3392,2048},{1280,1024,1664,1408},{2496,2688,2912,3104} };
 const int16_t*r=loops[c->route];int w=r[2]-r[0],h=r[3]-r[1],length=2*(w+h);
 int p=(int)(c->progress/Q)%length;if(c->reverse)p=(length-p)%length;
 int angle;
 if(p<w){c->x=r[0]+p;c->y=r[1];angle=64;}
 else if((p-=w)<h){c->x=r[2];c->y=r[1]+p;angle=128;}
 else if((p-=h)<w){c->x=r[2]-p;c->y=r[3];angle=192;}
 else{p-=w;c->x=r[0];c->y=r[3]-p;angle=0;}
 if(c->reverse)angle=(angle+128)&255;
 int lane=c->route==0&&((c->model/2)&1)?41:13;
 c->angle=(uint16_t)angle;c->x+=(int16_t)(city_cos(angle)*lane/256);c->y+=(int16_t)(city_sin(angle)*lane/256);
}
void city_begin(Game*g){
 game_begin_run(g);memset(&city,0,sizeof city);city.active=1;city.x=654*Q;city.y=1664*Q;city.camera_x=654*Q;city.camera_y=1664*Q;
 memset(g->traffic,0,sizeof g->traffic);memset(g->pickups,0,sizeof g->pickups);
 g->active_contract.kind=DC_NONE;g->active_contract.goal=0;g->biome=CITY;
 for(int i=0;i<CITY_TRAFFIC;i++){CityCar*c=&city.cars[i];c->route=(uint8_t)(i<48?0:i<56?1:2);c->reverse=i&1;c->model=(uint8_t)((i*7+2)%VEHICLE_COUNT);
  int length=c->route==0?8576:c->route==1?1536:1664;c->progress=(unsigned)((i*length/(c->route==0?48:8)+319)%length)*Q;city_car_position(c);}
}
static int city_body_solid(int x,int y,int angle,int model){
 CityShape b=city_vehicle_shape(model);
 int sx=city_sin(angle)*b.half_length/256,sy=city_cos(angle)*b.half_length/256;
 int rx=city_cos(angle)*b.half_width/256,ry=city_sin(angle)*b.half_width/256;
 return city_solid(x+sx+rx,y-sy+ry)||city_solid(x+sx-rx,y-sy-ry)||city_solid(x-sx+rx,y+sy+ry)||city_solid(x-sx-rx,y+sy-ry)||
        city_solid(x+sx,y-sy)||city_solid(x-sx,y+sy)||city_solid(x+rx,y+ry)||city_solid(x-rx,y-ry)||city_solid(x,y);
}
static void city_tick(Game*g,const Input*in){
 if(in->pressed&K_START){g->mode=M_PAUSE;g->cursor=0;return;}
 ++g->run_ticks;g->run.elapsed_ticks=g->run_ticks;
 if(g->damage_cooldown)--g->damage_cooldown;
 if(g->crash_timer)--g->crash_timer;
 if(g->horn_timer)--g->horn_timer;
 if(g->shake)--g->shake;
 int gas=(in->held&K_R)&&!(in->held&K_L),brake=(in->held&K_L)!=0,steer=((in->held&K_RIGHT)!=0)-((in->held&K_LEFT)!=0),old=g->speed_q8;
 g->slow=(uint8_t)(gas&&(in->held&K_DOWN));g->turbo_recharging=g->turbo_source=0;
 if(!(in->held&K_UP))g->turbo_latched=0;
 g->turbo=(uint8_t)(gas&&!g->slow&&(in->held&K_UP)&&!g->turbo_latched&&(g->turbo||g->turbo_charge_q8>=TURBO_START_Q8)&&g->turbo_charge_q8>=Q);
 if(g->turbo){g->turbo_charge_q8-=Q;if(!g->turbo_charge_q8)g->turbo_latched=1;}
 int top=game_top_speed(g)*(g->slow?62:g->turbo?125:100)/100;
 if(!city_on_road(city.x/Q,city.y/Q))top=top*48/100;
 if(gas){int rate=game_vehicle_stat(g,g->vehicle,STAT_ACCEL)*(g->turbo?140:g->slow?65:100)/100;g->speed_q8=clampi(g->speed_q8+rate*Q/DRIVE_HZ,-34*Q,top*Q);}
 else if(brake)g->speed_q8=clampi(g->speed_q8-(g->speed_q8>0?115:25)*Q/DRIVE_HZ,-34*Q,600*Q);
 else{int drag=18*Q/DRIVE_HZ;g->speed_q8+=g->speed_q8>drag?-drag:g->speed_q8 < -drag?drag:-g->speed_q8;}
 g->steering=(int16_t)steer;
 g->drifting=(uint8_t)((in->held&K_B)&&steer&&g->speed_q8>45*Q&&!g->crash_timer);
 g->wheelie=(uint8_t)(vehicles[g->vehicle].bike&&(in->held&K_A)&&g->speed_q8>50*Q&&!g->drifting&&!g->crash_timer);
 if((in->pressed&K_A)&&!vehicles[g->vehicle].bike)g->horn_timer=30;
 if((in->held&K_B)&&g->speed_q8>0)g->speed_q8=clampi(g->speed_q8-12*Q/DRIVE_HZ,0,600*Q);
 if(absi(g->speed_q8)>3*Q){int turn=absi(g->speed_q8)<65*Q?4:absi(g->speed_q8)<140*Q?3:2;turn+=g->drifting;city.angle=(uint16_t)((city.angle+steer*turn*(g->speed_q8<0?-1:1))&255);}
 int travel=g->speed_q8/38,sx=city_sin(city.angle),cy=city_cos(city.angle);
 int target_x=sx*travel/256,target_y=-cy*travel/256,handling=game_vehicle_stat(g,g->vehicle,STAT_GRIP);
 int gain=clampi(g->drifting?70+handling/3:110+handling/2,90,230);
 city.vx+=(target_x-city.vx)*gain/256;city.vy+=(target_y-city.vy)*gain/256;
 int steps=1+(absi(city.vx)+absi(city.vy))/(3*Q),moved=0;
 for(int s=0;s<steps;s++){int nx=city.x+city.vx/steps,ny=city.y+city.vy/steps;
  if(city_body_solid(nx/Q,ny/Q,city.angle,g->vehicle)){damage(g,17,1);g->speed_q8=g->speed_q8*55/100;city.vx=city.vy=0;break;}
  city.x=nx;city.y=ny;moved+=absi(travel)/steps;
 }
 city.travelled+=(uint32_t)moved;
 uint32_t distance=city.travelled/(2*Q);if(distance>g->run.distance_m){unsigned delta=distance-g->run.distance_m;g->run.distance_m=distance;points(g,B_DISTANCE,delta,delta*2);}
 g->position_q8=g->furthest_q8=(int32_t)distance*Q;g->odometer_q8=g->position_q8;
 if(g->run_ticks%DRIVE_HZ==0&&absi(g->speed_q8)>5*Q)points(g,B_SURVIVAL,1,5);
 if(g->speed_q8/Q>(int)g->run.max_speed)g->run.max_speed=(uint32_t)(g->speed_q8/Q);
 g->accel_q8=(g->speed_q8-old)*DRIVE_HZ;
 if(g->speed_q8>=game_top_speed(g)*Q*95/100)++g->speed_hold;else g->speed_hold=0;
 if(g->drifting)++g->drift_hold;else g->drift_hold=0;
 if(g->wheelie)++g->wheelie_hold;else g->wheelie_hold=0;
 g->combo_q8=(uint16_t)combo_for(g,g->speed_hold,SPEED_COMBO_TICKS);
 int combo=combo_for(g,g->drift_hold,SKILL_COMBO_TICKS);if(combo>g->combo_q8)g->combo_q8=(uint16_t)combo;
 combo=combo_for(g,g->wheelie_hold,SKILL_COMBO_TICKS);if(combo>g->combo_q8)g->combo_q8=(uint16_t)combo;
 if(g->combo_q8>g->run.max_combo_q8)g->run.max_combo_q8=g->combo_q8;
 if(g->speed_hold>g->run.best_speed_hold)g->run.best_speed_hold=g->speed_hold;
 if(g->run_ticks%15==0){int award=g->combo_q8*10/Q;
  if(g->drifting){points(g,B_DRIFT,1,(unsigned)award);g->run.drift_points+=(unsigned)award;}
  if(g->wheelie){points(g,B_WHEELIE,1,(unsigned)award);g->run.wheelie_points+=(unsigned)award;}
  if(g->speed_hold)points(g,B_SPEED,1,(unsigned)award);
 }
 g->x_q8=city_on_road(city.x/Q,city.y/Q)?0:ROAD_HALF*Q;
 g->vx_q8=g->drifting&&moved?steer*Q:0;
 turbo_recharge(g);
 CityShape traffic_body[CITY_TRAFFIC];
 for(int i=0;i<CITY_TRAFFIC;i++)traffic_body[i]=city_vehicle_shape(city.cars[i].model);
 for(int i=0;i<CITY_TRAFFIC;i++){
  CityCar*c=&city.cars[i];int stop=0,cs=city_cos(c->angle),sn=city_sin(c->angle);
  CityShape body=traffic_body[i];
  for(int j=0;j<CITY_TRAFFIC;j++)if(j!=i){CityCar*d=&city.cars[j];int dx=d->x-c->x,dy=d->y-c->y;
   if(absi(dx)>90||absi(dy)>90)continue;
   int forward=(dx*sn-dy*cs)/256,side=(dx*cs+dy*sn)/256;
   CityShape other=traffic_body[j];
   if(absi(side)<body.half_width+other.half_width+2&&forward>0&&forward<body.half_length+other.half_length+22)stop=1;
  }
  if(!stop)c->progress+=(unsigned)(c->route?200:320+(i%3)*32);
  city_car_position(c);
  int dx=city.x/Q-c->x,dy=city.y/Q-c->y;
  if(absi(dx)<52&&absi(dy)<52&&city_vehicle_overlap(city.x/Q,city.y/Q,city.angle,g->vehicle,c->x,c->y,c->angle,c->model,0)){
   damage(g,27,0);city.vx=city.vx*3/5;city.vy=city.vy*3/5;
  }else if(!c->near&&absi(dx)<62&&absi(dy)<62&&g->speed_q8>70*Q&&
           city_vehicle_overlap(city.x/Q,city.y/Q,city.angle,g->vehicle,c->x,c->y,c->angle,c->model,5)){
   points(g,B_NEAR,1,(unsigned)(100*g->combo_q8/Q));c->near=1;
  }
  if(absi(dx)+absi(dy)>260)c->near=0;
 }
 for(int i=0;i<64;i++)if(!city.tokens[i]){
  const CityRoad*r=&city_roads[i%city_road_count];int span=(r->x2-r->x1)+(r->y2-r->y1),p=(int)(road_hash(0x434f494eu,(unsigned)i)%(unsigned)span);
  int x=r->x1+(r->x2!=r->x1?p:12),y=r->y1+(r->y2!=r->y1?p:12);
  if(absi(city.x/Q-x)<16&&absi(city.y/Q-y)<16){city.tokens[i]=1;points(g,B_COIN,1,10);}
 }
 city.camera_x+=(city.x+sx*absi(travel)/32-city.camera_x)/5;city.camera_y+=(city.y-cy*absi(travel)/32-city.camera_y)/5;
 /* Free roam permits parking and studying junctions. Only impacts or an
  * explicit bank end this mode; parked time never earns points. */
 if(!g->health){g->end_reason=END_CRASH;game_finish_run(g);}
}
