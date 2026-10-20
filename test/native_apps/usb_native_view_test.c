/* Run the actual app-local view and hit functions with instrumented primitives.
 * The lifecycle matrix separately links the real raster and adapter. */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include "../../Apps/usb_sd_transfer.c"
#include "../../lib/PortableApps/settings_fonts/text-faces-012.inc"
static unsigned labels,rects,presented;
static bool saw_removed,saw_keep;
static char visible_text[2048];
static int32_t test_width(void){return usb_layout.width;}
static int32_t test_height(void){return usb_layout.height;}
static void test_present(bool full){(void)full;++presented;}
static const t5_app_api_v1 test_app={.screen_width=test_width,.screen_height=test_height,.present=test_present};
void portable_nova_begin(void){}
int portable_nova_measure(unsigned face,const char *s){
 unsigned source=face==6?0:face,scale=face==6?183:256;assert(source<3);
 int q4=0;for(unsigned i=0;s[i];i++){assert(i<96);unsigned c=(unsigned char)s[i];assert(c>=32&&c<=126);q4+=(int)(rps_text[source*95+c-32].advance_q4*scale/256);}
 return (q4+15)/16;
}
void portable_nova_native_text(unsigned face,int x,int y,int width,const char *s,uint32_t rgb){
 (void)rgb;assert(s&&*s&&width>0&&x>=0&&y>=0);
 assert(portable_nova_measure(face,s)<=width);
 unsigned source=face==6?0:face,scale=face==6?183:256;
 int pen=0;for(unsigned i=0;s[i];i++){
  const rps_glyph*g=&rps_text[source*95+(unsigned char)s[i]-32];
  const int right=x+pen/16+g->left*(int)scale/256+g->width*(int)scale/256;
  const int bottom=y+g->top*(int)scale/256+g->height*(int)scale/256;
  assert(right<=usb_layout.width&&bottom<=usb_layout.height);
  if(y<usb_layout.primary.y)assert(bottom<=usb_layout.primary.y-4);
  pen+=(int)(g->advance_q4*scale/256);
 }
 assert(strlen(visible_text)+strlen(s)+2<sizeof(visible_text));strcat(visible_text,s);strcat(visible_text," ");
 saw_removed|=!strcmp(s,"CABLE REMOVED");saw_keep|=!strcmp(s,"KEEP TRANSFERRING");++labels;
}
void portable_nova_native_round(int x,int y,int w,int h,int radius,uint32_t rgb){
 (void)rgb;assert(x>=0&&y>=0&&w>0&&h>0&&x+w<=usb_layout.width&&y+h<=usb_layout.height&&radius>=0);++rects;
}
static void check_hits(usb_native_rect r,int y){
 for(int row=0;row<usb_layout.height;row++)for(int x=0;x<usb_layout.width;x++){
  t5_app_input_t in={.tapped=true,.touch_x=x,.touch_y=row};
  bool expected=x>=r.x&&x<r.x+r.w&&row>=r.y&&row<r.y+r.h;
  assert(hit(&in,y)==expected);in.tapped=false;assert(!hit(&in,y));
 }
 t5_app_input_t in={.tapped=true,.touch_x=-1,.touch_y=-1};assert(!hit(&in,y));
}
int main(void){
 app=&test_app;usb_native=true;
 const int sizes[][2]={{160,160},{240,240},{240,280},{320,240},{410,502}};
 assert(!usb_native_layout_open(NULL,160,160));assert(!usb_native_layout_open(&usb_layout,159,160));
 assert(!usb_native_layout_open(&usb_layout,160,159));assert(!usb_native_layout_open(&usb_layout,INT_MAX,160));
 for(int w=160;w<=420;w+=13)for(int h=160;h<=520;h++)assert(usb_native_layout_open(&usb_layout,w,h));
 for(unsigned d=0;d<sizeof(sizes)/sizeof(sizes[0]);d++){
  assert(usb_native_layout_open(&usb_layout,sizes[d][0],sizes[d][1]));
  assert(usb_layout.primary.h==(sizes[d][1]<224?32:44));
  assert(usb_layout.secondary.y+usb_layout.secondary.h==sizes[d][1]-usb_layout.margin);
  assert(usb_layout.primary.y+usb_layout.primary.h<usb_layout.secondary.y);
  check_hits(usb_layout.primary,612);check_hits(usb_layout.secondary,704);
  for(unsigned s=0;s<=RISC_USB_MSC_PREPARING;s++)for(unsigned confirmed=0;confirmed<2;confirmed++){
   state=s;token=(s==RISC_USB_MSC_IDLE||terminal(s))?0:42;confirm_removed=confirmed;
   local_ready=terminal(s);host_reads=s==RISC_USB_MSC_CONNECTED;host_writes=0;
   strcpy(detail,"Fixture cleanup / media status");
   visible_text[0]=0;saw_removed=saw_keep=false;usb_native_render();
   if(confirmed)assert(strstr(visible_text,"lose files.")&&strstr(visible_text,"unplug")&&saw_removed&&saw_keep);
   assert(!dirty);assert(preparing_presented==(s==RISC_USB_MSC_PREPARING));
  }
 }
 printf("Native USB view: %u labels, %u rectangle calls, %u frames; full hit maps and complete confirmation warning pass\n",labels,rects,presented);
}
