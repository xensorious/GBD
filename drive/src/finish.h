#ifndef COASTLINE_FINISH_H
#define COASTLINE_FINISH_H
#include <stdint.h>
#define PAINT_COUNT 16
#define LIVERY_COLOR_COUNT 12
#define PAINT_MASK 0xffffu
#define NEW_PAINT_MASK 0xffc0u
#define LIVERY_COLOR_MASK 0xfffu
typedef struct {const char*name;uint8_t r,g,b;} FinishColor;
static inline const FinishColor *body_finish(int id){
 static const FinishColor colors[PAINT_COUNT]={
  {"Factory",31,31,31},{"Coral",31,12,10},{"Lagoon",7,29,27},{"Sunburst",31,25,7},
  {"Orchid",25,13,31},{"Graphite",17,20,23},{"Black",3,4,5},{"White",31,31,30},
  {"Cobalt blue",5,13,31},{"Racing red",31,5,7},{"Forest green",5,17,10},{"Orange",31,15,3},
  {"Silver",24,27,29},{"Ice blue",14,26,31},{"Sand",28,23,16},{"Magenta",31,7,23}};
 return &colors[id>=0&&id<PAINT_COUNT?id:0];
}
static inline const FinishColor *livery_finish(int id){
 static const FinishColor colors[LIVERY_COLOR_COUNT]={
  {"White",31,31,30},{"Black",3,4,5},{"Graphite",12,14,17},{"Red",31,5,7},
  {"Blue",5,13,31},{"Yellow",31,27,4},{"Purple",23,8,31},{"Forest green",5,17,10},
  {"Orange",31,15,3},{"Teal",4,26,23},{"Silver",24,27,29},{"Rose gold",31,20,16}};
 return &colors[id>=0&&id<LIVERY_COLOR_COUNT?id:0];
}
/* Coordinates are normalized to the paintable body, with v=0 at the nose.
 * These shapes affect painted panels only; glass and mechanical parts stay intact. */
static inline int livery_pixel(int pattern,int u,int v,int narrow){
 if(narrow && pattern>=3)return v%24<9;
 if(pattern==1)return (u>=7&&u<=11)||(u>=20&&u<=24)||narrow;
 if(pattern==2)return u<8||u>23||(narrow&&v>=32);
 if(pattern==3){int sweep=5+v*20/64;return (u>=sweep&&u<=sweep+4)||(v>36&&v<48&&u>24);}
 if(pattern==4){int edge=v<20?3+v/5:v>48?3+(64-v)/5:7;return u==edge||u==edge+1||u==31-edge||u==30-edge||(v>=8&&v<=11&&u>10&&u<22);}
 return 0;
}
#endif
