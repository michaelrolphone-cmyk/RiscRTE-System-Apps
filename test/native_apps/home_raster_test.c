/* Compare original immediate spans with immutable deferred Home operations.
 * Pixel equality includes scaling, row boundaries and post-record input changes. */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "../../lib/PortableApps/include/PortableRaster.h"
static int sw=480,sh=800;
static uint8_t canvas[800*1600],reference[sizeof(canvas)];
static unsigned spans,commands;
static bool recording;
typedef struct {portable_raster_draw fn;int top,bottom;union {max_align_t align;uint8_t bytes[129];} data;} command;
static command queue[512];
static int screen_width(void){return sw;}
static int screen_height(void){return sh;}
static int xscale(int x){return x*sw/480;}
static int yscale(int y){return y*sh/800;}
static void fill_rect(int x,int y,int w,int h,bool black){
 ++spans;int right=x+w,bottom=y+h;if(x<0)x=0;if(y<0)y=0;if(right>sw)right=sw;if(bottom>sh)bottom=sh;
 for(int row=y;row<bottom;row++)for(int col=x;col<right;col++)canvas[row*sw+col]=black;
}
static const struct {int (*screen_width)(void);int (*screen_height)(void);void (*fill_rect)(int,int,int,int,bool);} api={screen_width,screen_height,fill_rect},*app=&api;
static struct {bool valid,charging;unsigned percent;} battery_status={true,false,84};
#include "../../Apps/paper_home_type.inc"
#include "../../lib/PortableApps/home_fonts/reference.inc"
bool portable_raster_defer(portable_raster_draw draw,const void *data,size_t bytes,int top,int bottom){
 if(!recording)return false;
 assert(bytes<=129&&commands<512);command *c=&queue[commands++];c->fn=draw;c->top=top;c->bottom=bottom;memcpy(c->data.bytes,data,bytes);return true;
}
static void scene(unsigned minute){
 home_dial(true,minute);home_type(&HOME_REF_TIME,240,190,"09:41",0,2,0,416);
 home_type(&HOME_R700_26,32,44,"SAT 10 OCT",3,0,200,0);home_battery();
 home_type_ink(&HOME_REF_NEXT,48,291,"NEXT POINT",6,0,0,0,false);
 home_type(&HOME_REF_TITLE,32,366,"LONG HOME TITLE",0,0,416,0);
 home_type(&HOME_R700_26,32,408,"TAP TO ADD ONE",2,0,416,0);
 home_round(30,422,420,28,14,true);home_round(34,426,412,20,10,false);
 home_type(&HOME_REF_THEN,32,504,"THEN",8,0,0,0);
 home_type(&HOME_REF_FOLLOW,32,568,"FIRST FOLLOWING EVENT",2,0,198,0);
 home_type(&HOME_REF_FOLLOW,250,568,"SECOND FOLLOWING EVENT",2,0,198,0);
 const char *names[]={"FILES","POINTS","CONTEXTS","SETTINGS"};
 for(unsigned i=0;i<4;i++)home_type(&HOME_REF_DOCK,84+(int)i*104,756,names[i],1,2,100,0);
 /* Clipped, compressed and inverted glyphs plus split scaled shape rows. */
 home_type_ink(&HOME_R700_30,-12,12,"AVgy@_",1,0,32,0,false);
 home_round(-8,785,88,32,16,true);
}
int main(void){
 (void)home_pending;(void)home_refresh_pending;(void)home_press_begin;(void)home_svg_fit;
 const int sizes[][2]={{480,800},{360,600},{600,1000},{800,1600}};
 unsigned baseline=0,deferred=0,cases=0;
 for(unsigned size=0;size<4;size++)for(unsigned press=0;press<=HOME_SETTINGS;press++)for(unsigned band=1;band<=7;band+=3){
  sw=sizes[size][0];sh=sizes[size][1];home_pressed=press;recording=false;spans=commands=0;
  memset(canvas,0xa5,sizeof(canvas));scene(press*7);unsigned old=spans;memcpy(reference,canvas,sizeof(canvas));
  memset(canvas,0xa5,sizeof(canvas));recording=true;spans=0;scene(press*7);assert(!spans&&commands<256);
  home_pressed=HOME_NONE;recording=false;
  for(unsigned n=0;n<commands;n++){command*c=&queue[n];for(int row=c->top;row<c->bottom;row+=(int)band)c->fn(c->data.bytes,row,row+(int)band<c->bottom?row+(int)band:c->bottom);}
  assert(!memcmp(canvas,reference,sizeof(canvas)));
  cases++;
  if(!size&&!press&&band==1){baseline=old;deferred=commands;}
 }
 assert(baseline>4096&&deferred<256);
 printf("Home pixel equality: %u scaled/highlight/band cases; immediate spans=%u deferred commands=%u PASS\n",cases,baseline,deferred);return 0;
}
