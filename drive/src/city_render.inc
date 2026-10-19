volatile uint32_t city_profile[8],city_last_profile[8];
static int city_abs(int x){return x<0?-x:x;}
typedef struct {int model,angle,paint,pattern,color,wheels;unsigned used;uint16_t pixels[54*54];} CitySprite;
static CitySprite city_sprites[8];
static unsigned city_sprite_clock;
/* Pre-fitted indexed artwork uses the same finishes as the chase camera.
 * Decode one 1.5 KiB model on cache misses, retaining eight rotated sprites. */
void city_vehicle_pixels(uint16_t*out,int model,const Save*save){
 uint8_t art[32*48];
 const uint8_t*src=overhead_lz+overhead_offsets[model];unsigned in=4,pos=0;
 while(pos<sizeof art){unsigned flags=src[in++];
  for(unsigned bit=128;bit&&pos<sizeof art;bit>>=1){
   if(flags&bit){unsigned code=(unsigned)src[in]<<8|src[in+1];in+=2;
    unsigned n=(code>>12)+3,back=(code&4095)+1;
    while(n--){art[pos]=art[pos-back];pos++;}
   }else art[pos++]=src[in++];
  }
 }
 int paint=save?save->paint[model]:0,livery=save?save->livery[model]:0,wheels=save?save->wheels[model]:0;
 const FinishColor*body=body_finish(paint),*decal=livery_finish(save?save->livery_color[model]:0);
 const uint16_t*palette=overhead_palette+model*16;
 static const uint8_t shade[8]={0,10,17,24,28,30,34,40};
 static const uint8_t rims[4][3]={{16,18,20},{9,12,14},{28,30,31},{31,24,8}};
 for(unsigned at=0;at<sizeof art;at++){
  unsigned index=art[at]&15;uint16_t c=palette[index];
  if(index&&index<=7){
   const FinishColor*color=livery>0&&livery<5&&(art[at]&(1u<<(livery+3)))?decal:paint?body:0;
   if(color){int light=shade[index];c=(uint16_t)(0x8000|clampi(color->r*light/31,0,31)|(clampi(color->g*light/31,0,31)<<5)|(clampi(color->b*light/31,0,31)<<10));}
  }else if(index==11&&wheels>0&&wheels<4)c=(uint16_t)(0x8000|rims[wheels][0]|rims[wheels][1]<<5|rims[wheels][2]<<10);
  out[at]=c;
 }
}
void city_draw_vehicle(Surface*s,int x,int y,int angle,int model,const Save*save){
 int paint=save?save->paint[model]:0,pattern=save?save->livery[model]:0,color=save?save->livery_color[model]:0,wheels=save?save->wheels[model]:0;
 unsigned call_start=profile_clock();
 CitySprite*cached=&city_sprites[0];++city_sprite_clock;
 for(int n=0;n<8;n++){CitySprite*c=&city_sprites[n];
  if(c->used&&c->model==model&&c->angle==angle&&c->paint==paint&&c->pattern==pattern&&c->color==color&&c->wheels==wheels){c->used=city_sprite_clock;gfx_shadow_sprite(s,x-25,y-24,c->pixels,54,54);gfx_sprite(s,x-27,y-27,c->pixels,54,54,0);city_profile[7]+=(profile_clock()-call_start)&65535;return;}
  if(c->used<cached->used)cached=c;
 }
 cached->model=model;cached->angle=angle;cached->paint=paint;cached->pattern=pattern;cached->color=color;cached->wheels=wheels;cached->used=city_sprite_clock;
 unsigned render_start=profile_clock();
 uint16_t pixels[32*48];city_vehicle_pixels(pixels,model,save);
 city_profile[4]+=(profile_clock()-render_start)&65535;render_start=profile_clock();
 int sn=city_sin(angle),cs=city_cos(angle);uint16_t*rotated=cached->pixels;
 for(int yy=-27;yy<27;yy++){
  int u=(16<<8)-27*cs+yy*sn,v=(24<<8)+27*sn+yy*cs;
  uint16_t*dest=rotated+(yy+27)*54;
  for(int xx=-27;xx<27;xx++,u+=cs,v-=sn){
   uint16_t c=(unsigned)u<32u*256&&(unsigned)v<48u*256?pixels[(v>>8)*32+(u>>8)]:0;*dest++=c;

  }
 }
 gfx_shadow_sprite(s,x-25,y-24,rotated,54,54);
 gfx_sprite(s,x-27,y-27,rotated,54,54,0);
 city_profile[5]+=(profile_clock()-render_start)&65535;
 city_profile[7]+=(profile_clock()-call_start)&65535;

}
void city_draw_map(Surface*s,int x,int y,int w,int h,const Game*g){
 gfx_rect(s,x,y,w,h,C(31,67,64));gfx_rect(s,x+w*1920/CITY_SIZE,y+h*1152/CITY_SIZE,w*320/CITY_SIZE,h*(CITY_SIZE-1152)/CITY_SIZE,C(37,100,131));
 gfx_rect(s,x+w*3712/CITY_SIZE,y,w*384/CITY_SIZE,h,C(37,100,131));
 for(int i=0;i<city_road_count;i++){const CityRoad*r=&city_roads[i];gfx_rect(s,x+r->x1*w/CITY_SIZE-1,y+r->y1*h/CITY_SIZE-1,(r->x2-r->x1)*w/CITY_SIZE+3,(r->y2-r->y1)*h/CITY_SIZE+3,r->highway?C(199,200,164):C(101,142,140));}
 if(city.active&&g){int px=x+(city.x/Q)*w/CITY_SIZE,py=y+(city.y/Q)*h/CITY_SIZE;gfx_circle(s,px,py,3,C(255,203,98));gfx_line(s,px,py,px+city_sin(city.angle)*7/256,py-city_cos(city.angle)*7/256,C(255,244,196));}
 (void)g;
}
static void city_building(Surface*s,int col,int row,int kind,int camx,int camy){
 CityLot lot=city_lot_bounds(col,row);int x=lot.x-camx,y=lot.y-camy,w=lot.w,h=lot.h;
 int home=row>18,height=home?8:kind==1?34:kind==2?23:12;
 uint16_t wall=home?C(218,189,148):col>17?C(164,193,198):C(158,162,162);
 uint16_t roof=home?C(166,91,69):kind==3?C(73,115,131):kind==4?C(178,168,140):C(101,119,130);
 gfx_rect(s,x+w,y-height+7,8,h+height,C(52,78,69));gfx_rect(s,x+7,y+h,w,7,C(52,78,69));
 int styles[6]={BL_OFFICE,BL_APARTMENT,BL_SHOP,BL_HOTEL,BL_CAFE,BL_WAREHOUSE};
 int homes[6]={BL_HOUSE,BL_VILLA,BL_DUPLEX,BL_POOL_VILLA,BL_MOTEL,BL_HOUSE};
 int style=home?homes[kind-1]:styles[kind-1];
 gfx_rect(s,x,y+h-height,w,height,wall);
 gfx_indexed_sprite(s,x,y+h-height,building_wall_idx+style*4096,building_palette+style*256,64,64,w,height);
 gfx_rect(s,x-1,y-height-1,w+2,h+1,C(53,66,73));gfx_rect(s,x,y-height,w,h-1,roof);
 gfx_indexed_sprite(s,x+1,y-height+1,building_roof_idx+style*1024,building_palette+style*256,32,32,w-2,h-3);
 gfx_rect(s,x+3,y-height+3,w-6,1,C(214,213,184));gfx_rect(s,x+3,y-height+3,1,h-7,C(214,213,184));
 if(home){
  gfx_line(s,x+1,y-height+h/2,x+w-2,y-height+h/2,C(112,62,52));
  gfx_rect(s,x+w-18,y-height+7,9,12,C(218,184,149));gfx_rect(s,x+w/2-6,y+h-3,12,5,C(214,210,180));
 }else{
  if(kind==2||kind==4){gfx_rect(s,x+9,y-height+11,17,19,C(74,87,96));gfx_rect(s,x+10,y-height+10,15,17,C(158,172,172));
   for(int n=0;n<3;n++)gfx_rect(s,x+12,y-height+13+n*5,11,1,C(92,111,119));}
  if(kind==3&&w>45){gfx_rect(s,x+4,y+h-height-7,w-8,5,C(226,178,83));gfx_text_small(s,x+8,y+h-height-9,"MARKET",C(31,52,61));}
 }

}
static void city_tree(Surface*s,int x,int y,int frame){
 static uint16_t leaves[2][48*56];static int ready;
 if(!ready){
  for(int pose=0;pose<2;pose++){
   Surface leaf={leaves[pose],48,56};int cx=24,cy=28;
   gfx_circle(&leaf,28,43,10,C(62,94,69));gfx_rect(&leaf,24,29,3,14,C(127,99,61));
   for(int branch=0;branch<8;branch++){
    int a=branch*32+2*pose,sn=city_sin(a),cs=city_cos(a);
    for(int n=0;n<17;n++){int xx=cx+sn*n/256,yy=cy-cs*n/256,width=n<9?n/3+1:(17-n)/3+1;
     gfx_line(&leaf,xx-cs*width/256,yy-sn*width/256,xx+cs*width/256,yy+sn*width/256,n&1?C(44,114,76):C(78,145,82));}
    gfx_line(&leaf,cx,cy,cx+sn*14/256,cy-cs*14/256,C(114,164,87));
   }
   gfx_circle(&leaf,cx,cy,2,C(161,148,86));
  }
  ready=1;
 }
 gfx_sprite(s,x-24,y-42,leaves[(frame/20)&1],48,56,0);
}
static void city_scene(Surface*s,const Game*g){
 for(int k=0;k<8;k++)city_profile[k]=0;
 unsigned stage=profile_clock();
 int camx=city.camera_x/Q-128,camy=city.camera_y/Q-112;
 gfx_rect(s,0,0,256,192,C(91,129,86));
 /* Pavement grain and planted verges are tied to world coordinates. */
 for(int y=-(camy&31);y<192;y+=32)for(int x=-(camx&31);x<256;x+=32){gfx_rect(s,x+7,y+11,2,1,C(117,149,94));gfx_rect(s,x+22,y+25,3,1,C(75,116,78));}
 gfx_rect(s,1912-camx,1152-camy,336,4096,C(190,187,139));
 gfx_rect(s,1920-camx,1152-camy,320,4096,C(36,119,145));gfx_rect(s,3712-camx,-camy,384,4096,C(30,126,155));
 if(city_water(camx+128,camy+96)|| (camx<2240&&camx+256>1920))for(int y=-(camy&31);y<192;y+=32)for(int x=-(camx&63);x<256;x+=64){int wx=camx+x,wy=camy+y;if(city_water(wx,wy))gfx_rect(s,x+(g->frame/8)%9,y,13,1,C(82,165,176));}
 for(int i=0;i<city_road_count;i++){
  const CityRoad*r=&city_roads[i];int rx=r->x1-r->half-camx,ry=r->y1-r->half-camy,w=r->x2-r->x1+r->half*2,h=r->y2-r->y1+r->half*2;
  if(rx>256||ry>192||rx+w<0||ry+h<0)continue;
  gfx_rect(s,rx-7,ry-7,w+14,h+14,C(179,183,165));gfx_rect(s,rx,ry,w,h,C(60,71,79));
 }
 for(int i=0;i<city_road_count;i++){
  const CityRoad*r=&city_roads[i];int hor=r->y1==r->y2,x=r->x1-camx,y=r->y1-camy;
  if(hor){gfx_rect(s,x,y-2,r->x2-r->x1,1,C(229,191,100));gfx_rect(s,x,y+2,r->x2-r->x1,1,C(229,191,100));
   if(r->highway)for(int xx=-(camx&47);xx<256;xx+=48){gfx_rect(s,xx,y-28,23,2,C(203,211,202));gfx_rect(s,xx,y+28,23,2,C(203,211,202));}
   if(r->y1>1152){gfx_rect(s,1920-camx,y-r->half-5,320,4,C(225,229,208));gfx_rect(s,1920-camx,y+r->half+2,320,4,C(225,229,208));for(int bx=1920;bx<=2240;bx+=32){gfx_rect(s,bx-camx,y-r->half-7,4,7,C(123,147,155));gfx_rect(s,bx-camx,y+r->half,4,7,C(123,147,155));}}
  }else{gfx_rect(s,x-2,y,1,r->y2-r->y1,C(229,191,100));gfx_rect(s,x+2,y,1,r->y2-r->y1,C(229,191,100));
   if(r->highway)for(int yy=-(camy&47);yy<192;yy+=48){gfx_rect(s,x-28,yy,2,23,C(203,211,202));gfx_rect(s,x+28,yy,2,23,C(203,211,202));}
  }
 }
 city_profile[0]=(profile_clock()-stage)&65535;stage=profile_clock();
 /* Waterfront craft are true overhead silhouettes, with small moving wakes. */
 for(int boat=0;boat<3;boat++){
  int bx=2016+boat*67-camx,by=1954-boat*134-camy+(int)((g->frame/16+boat*7)%9);
  gfx_rect(s,bx-4,by+14,9,4,C(81,159,175));
  for(int n=-14;n<=14;n++){int half=6-city_abs(n)/4;gfx_rect(s,bx-half,by+n,half*2+1,1,C(234,231,208));}
  gfx_rect(s,bx-3,by-6,7,16,C(53,90,118));gfx_rect(s,bx-2,by-5,5,6,C(157,204,208));
 }
 /* Junctions clear overlapping lane paint and mark neighborhood crossings. */
 for(int i=0;i<city_road_count;i++)if(city_roads[i].y1==city_roads[i].y2)for(int j=0;j<city_road_count;j++)if(city_roads[j].x1==city_roads[j].x2){
  const CityRoad*a=&city_roads[i],*b=&city_roads[j];int wx=b->x1,wy=a->y1;
  if(wx<a->x1||wx>a->x2||wy<b->y1||wy>b->y2)continue;
  int x=wx-camx,y=wy-camy;gfx_rect(s,x-b->half,y-a->half,2*b->half,2*a->half,C(60,71,79));
  if(!a->highway&&!b->highway)for(int n=-18;n<20;n+=7){gfx_rect(s,x+n,y-34,4,8,C(218,220,194));gfx_rect(s,x+n,y+27,4,8,C(218,220,194));}
 }
 for(int i=0;i<64;i++)if(!city.tokens[i]){const CityRoad*r=&city_roads[i%city_road_count];int span=r->x2-r->x1+r->y2-r->y1,p=(int)(road_hash(0x434f494eu,(unsigned)i)%(unsigned)span);
  int x=r->x1+(r->x2!=r->x1?p:12)-camx,y=r->y1+(r->y2!=r->y1?p:12)-camy;
  gfx_circle(s,x,y,3,C(246,194,73));gfx_rect(s,x,y-2,1,4,C(255,242,159));
 }
 city_profile[1]=(profile_clock()-stage)&65535;stage=profile_clock();
 /* A tiny depth queue lets projecting roofs occlude vehicles behind them. */
 struct CityDraw{int y,type,a,b;} items[96];int count=0;
 for(int row=clampi(camy/128-1,0,31);row<=clampi((camy+260)/128,0,31);row++)for(int col=clampi(camx/128-1,0,31);col<=clampi((camx+300)/128,0,31);col++){
  int kind=city_lot(col,row);if(kind&&count<90)items[count++]=(struct CityDraw){city_lot_bounds(col,row).y+city_lot_bounds(col,row).h,0,col,row};
  else if(!city_water(col*128+64,row*128+64)&&!city_on_road(col*128+60,row*128+60)&&count<90)items[count++]=(struct CityDraw){row*128+80,2,col*128+60,row*128+80};
 }
 for(int i=0;i<CITY_TRAFFIC;i++)if(city_abs(city.cars[i].x-camx-128)<180&&city_abs(city.cars[i].y-camy-96)<150&&count<94)items[count++]=(struct CityDraw){city.cars[i].y+17,1,i,0};
 items[count++]=(struct CityDraw){city.y/Q+17,3,0,0};
 for(int i=1;i<count;i++){struct CityDraw a=items[i];int j=i;while(j&&items[j-1].y>a.y){items[j]=items[j-1];--j;}items[j]=a;}
 city_profile[2]=(profile_clock()-stage)&65535;stage=profile_clock();
 for(int i=0;i<count;i++){struct CityDraw*d=&items[i];
  if(d->type==0){unsigned start=profile_clock();city_building(s,d->a,d->b,city_lot(d->a,d->b),camx,camy);city_profile[6]+=(profile_clock()-start)&65535;}
  else if(d->type==2)city_tree(s,d->a-camx,d->b-camy,(int)g->frame);
  else if(d->type==1){CityCar*c=&city.cars[d->a];city_draw_vehicle(s,c->x-camx,c->y-camy,c->angle,c->model,0);}
  else{
   if(g->drifting&&g->save.smoke[g->vehicle]){static const uint16_t smoke[5]={C(199,211,212),C(241,146,138),C(83,205,188),C(172,133,225),C(231,194,105)};int px=city.x/Q-camx-city_sin(city.angle)*21/256,py=city.y/Q-camy+city_cos(city.angle)*21/256;
    gfx_circle(s,px+3,py+2,4,smoke[g->save.smoke[g->vehicle]]);gfx_circle(s,px-3,py,3,smoke[g->save.smoke[g->vehicle]]);}
   city_draw_vehicle(s,city.x/Q-camx,city.y/Q-camy,city.angle,g->vehicle,&g->save);
  }
 }
 city_profile[3]=(profile_clock()-stage)&65535;
 const StudioTheme*theme=theme_for(g);overlay_panel(s,6,4,244,15,theme->pane);gfx_text_small(s,12,8,city_district(city.x/Q,city.y/Q),theme->ink);
 if(g->crash_timer){gfx_text_small(s,198,8,"IMPACT",C(255,145,104));}
 for(int k=0;k<8;k++)city_last_profile[k]=city_profile[k];
}
