/* Production controller, renderer and input adapter at native watch sizes. */
#define PORTABLE_NOVA_UI
#ifndef TEST_LEGACY_WATCH_LAYOUT
#define PORTABLE_WATCH_NATIVE_HEIGHT
#endif
#define PORTABLE_TOUCH_ROTATION 0
#define WIFI_WORKFLOW_LIBRARY
#include "wifi_workflow_test.c"

static void visible_touch(unsigned at,int x,int y,bool footer){
#ifdef PORTABLE_WATCH_NATIVE_HEIGHT
 watch_layout l;watch_layout_rect r,p;
 assert(watch_layout_open(&l,width(),height(),NULL));
 if(footer)y+=watch_view_footer_offset(width(),height());
 r=(watch_layout_rect){x,y,1,1};assert(watch_layout_native_rect(&l,&r,&p));
 held_touch(at,p.x,p.y);
#else
 (void)footer;held_touch(at,x+(width()-240)/2,y+(height()-240)/2);
#endif
}
int main(int argc,char **argv){
 assert(argc==2);unsigned which=(unsigned)atoi(argv[1]);assert(which<4);
 start();render();assert(portable_wifi_close());unsigned base=poll_count;
 unsigned at=base+40;
 if(which==1){ /* Next then Prev must restore the initial selection. */
  visible_touch(at,192,210,true);at+=40;
  visible_touch(at,48,210,true);at+=40;
 }else if(which==2){ /* The gap between Prev and Select has no action. */
  visible_touch(at,84,210,true);at+=40;
 }else if(which==3){ /* Upper content and the native-height gutter stay inert. */
  visible_touch(at,120,180,false);at+=40;
 }
 visible_touch(at,192,210,true);at+=40;
 visible_touch(at,192,210,true);at+=40;
 visible_touch(at,192,210,true);at+=40;
 visible_touch(at,120,210,true);at+=40;
 visible_touch(at,120,210,true);at+=40;
 visible_touch(at,24,24,false);
 app_main();
 printf("watch case=%u %dx%d starts=%u cancels=%u launches=%u polls=%u\n",which,width(),height(),scan_starts,scan_cancels,launches,poll_count);fflush(stdout);
 assert(scan_starts==1 && scan_cancels==1 && launches==1 && !opened && !writes);
 finish();return 0;
}
