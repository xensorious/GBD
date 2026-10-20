/* Independent ray/volume tests. Production projects convex silhouettes;
 * this oracle instead clips a ray toward the sun against a 3D roof prism. */
static double sun_min(double a,double b){return a<b?a:b;}
static double sun_max(double a,double b){return a>b?a:b;}
static int sun_clip(double a,double b,double*lo,double*hi){
 /* a*t <= b */
 if(a>0)*hi=sun_min(*hi,b/a);
 else if(a<0)*lo=sun_max(*lo,b/a);
 else if(b<0)return 0;
 return *lo<=*hi;
}
static int sun_ray(const CitySunShape*s,int xq,int yq,int z){
 double x=xq/256.0-s->x,y=yq/256.0-s->y,lo=0,hi=10000;
 if(!sun_clip(1.25,x,&lo,&hi)||!sun_clip(-1.25,s->w-x,&lo,&hi)||
    !sun_clip(.75,y,&lo,&hi)||!sun_clip(-.75,s->d-y,&lo,&hi))return 0;
 if(!s->ridge)return sun_clip(1,s->height-z,&lo,&hi);
 double slope=2.0*s->ridge/s->d;
 return sun_clip(1+slope*.75,s->height+slope*y-z,&lo,&hi)&&
        sun_clip(1-slope*.75,s->height+slope*(s->d-y)-z,&lo,&hi);
}
static void sunlight_checks(void){
 unsigned rays=0,maximum=0,boundaries=0;
 for(int kind=0;kind<4;kind++){
  CitySunShape s={1040,1560,56,48,(uint8_t)(kind<2?24:42),(uint8_t)(kind&1?12:0),0,0};
  for(int z=0;z<=18;z+=3)for(int y=-6;y<122;y+=2)for(int x=-6;x<156;x+=2){
   int xq=(s.x+x)*Q+32,yq=(s.y+y)*Q+96;
   int expected=sun_ray(&s,xq,yq,z),actual=city_sun_shape_shadow(&s,xq,yq,z,0);
   if(actual!=expected)fprintf(stderr,"sun ray %d @%d,%d z%d: %d != %d\n",kind,x,y,z,actual,expected);
   CHECK(actual==expected);rays++;
  }
 }
 /* Tree shadows reuse the actual alpha silhouette, including transparent
  * canopy holes; wind changes the canopy and its shadow together. */
 for(int tree=0;tree<5;tree++)for(int pose=0;pose<2;pose++)for(int z=0;z<=6;z+=3){
  CitySunShape s={1040,1560,48,48,18,0,1,(uint8_t)tree};
  for(int v=-1;v<=48;v++)for(int u=-1;u<=48;u++){
   int xq=s.x*Q+(18-z)*320+(u+pose)*Q+64,yq=s.y*Q+(18-z)*192+v*Q+64;
   int expected=(unsigned)u<48&&(unsigned)v<48&&city_mesh_idx[(256+v)*256+tree*48+u]!=0;
   CHECK(city_sun_shape_shadow(&s,xq,yq,z,(unsigned)pose*30)==expected);
  }
 }
 /* Visit every cache chunk, including map edges. Never lose a caster to a
  * fixed pool limit, or change a world point when its camera chunk changes. */
 CitySunShape shapes[192];
 for(int row=0;row<32;row++)for(int col=0;col<32;col++){
  int cx=col*128+127,cy=row*128+64;
  city_sun_prepare(cx,cy,120);unsigned count=city_sun_shapes(shapes,192);CHECK(count<192);
  extern volatile uint32_t city_sun_bin_stats[2];CHECK(!city_sun_bin_stats[1]);
  /* A complete uncached union catches bucket omissions and double shading
   * where silhouettes overlap. This never uses the bucket lookup. */
  static const int offsets[5][2]={{0,0},{-96,0},{96,0},{0,-96},{0,96}};
  int expected=0;
  for(int tap=0;tap<5;tap++)for(unsigned i=0;i<count;i++){
   if(city_sun_shape_shadow(&shapes[i],cx*Q+64+offsets[tap][0],cy*Q+64+offsets[tap][1],3,120)){expected++;break;}
  }
  CHECK(city_sun_occlusion(cx*Q+64,cy*Q+64,3)==expected*256/5);
  if(count>maximum)maximum=count;
  int coverage[9];
  for(int tap=0;tap<9;tap++)coverage[tap]=city_sun_occlusion((cx+(tap%3-1)*96)*Q+64,(cy+(tap/3-1)*96)*Q+64,3);
  city_sun_prepare(cx+1,cy,120);
  for(int tap=0;tap<9;tap++){
   CHECK(city_sun_occlusion((cx+(tap%3-1)*96)*Q+64,(cy+(tap/3-1)*96)*Q+64,3)==coverage[tap]);boundaries++;
  }
 }
 CHECK(maximum>40);printf("sunlight: %u independent 3D ray/roof cases, alpha-tested tree silhouettes, 1024 world chunks (max %u/192 casters), %u cache-boundary receiver checks pass\n",rays,maximum,boundaries);
}
