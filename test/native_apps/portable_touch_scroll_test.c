#define PORTABLE_TOUCH_SCROLL
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include "PortableTouchScroll.h"
static unsigned feed(portable_touch_scroll *s,uint32_t now,int x,int y,bool down,bool began,bool released,bool eligible,bool moved) {
  portable_touch_sample in={.valid=true,.down=down,.began=began,.released=released,
    .tap_eligible=eligible,.moved=moved,.x=(uint16_t)x,.y=(uint16_t)y};
  return portable_scroll_touch(s,&in,now);
}
static void drag(portable_touch_scroll *s,uint32_t now) {
  feed(s,now,80,300,true,true,false,true,false);
  assert(feed(s,now+20,80,250,true,false,false,true,true)&PORTABLE_SCROLL_CHANGED);
  feed(s,now+40,80,200,true,false,false,true,true);
  /* A snapshot-only release carries the DOWN coordinate. */
  assert(!(feed(s,now+60,80,300,false,false,true,true,true)&PORTABLE_SCROLL_TAP));
  assert(portable_scroll_offset(s)==100&&s->velocity_q8>0);
}
int main(void) {
  portable_touch_scroll s={0};
  portable_scroll_configure(&s,(portable_scroll_viewport){32,100,416,500},72,419);
  drag(&s,UINT32_MAX-30u);
  unsigned now=40;
  for(unsigned i=0;i<80;++i){feed(&s,now,0,0,false,false,false,true,false);now+=20;}
  assert(portable_scroll_offset(&s)>100&&!s.velocity_q8);
  int at=portable_scroll_offset(&s);
  int row=portable_scroll_row(&s,100,101);
  assert(row==at/72&&portable_scroll_row(&s,20,101)==-1&&portable_scroll_row(&s,100,600)==-1);
  int x=0,y=90,w=480,h=100;assert(portable_scroll_clip(&s.view,&x,&y,&w,&h));
  assert(x==32&&y==100&&w==416&&h==90);
  s.position_q8=0;drag(&s,now);now+=100;
  feed(&s,now,80,300,true,true,false,true,false);
  assert(!s.velocity_q8);
  assert(!(feed(&s,now+20,80,300,false,false,true,true,false)&PORTABLE_SCROLL_TAP));
  feed(&s,now+40,80,300,true,true,false,true,false);
  assert(feed(&s,now+60,80,300,false,false,true,true,false)&PORTABLE_SCROLL_TAP);
  portable_scroll_cancel(&s);
  feed(&s,now+80,80,280,true,false,false,false,true);
  assert(!s.contact&&!s.velocity_q8);
  assert(!(feed(&s,now+100,80,300,false,false,true,false,true)&PORTABLE_SCROLL_TAP));
  s.position_q8=0;drag(&s,now+120);
  int before=s.position_q8;
  assert(!portable_scroll_animate(&s,now+500)&&s.position_q8==before&&!s.velocity_q8);
  portable_scroll_reveal(&s,418);
  assert(portable_scroll_offset(&s)==s.limit);
  feed(&s,now+520,80,300,true,true,false,true,false);
  feed(&s,now+540,80,50,true,false,false,true,true);
  feed(&s,now+560,80,300,false,false,true,true,true);
  assert(portable_scroll_offset(&s)==s.limit&&!s.velocity_q8);
  s.position_q8=0;
  feed(&s,now+600,80,300,true,true,false,true,false);
  portable_touch_sample up={.valid=true,.released=true,.tap_eligible=true,.moved=true,
    .x=80,.y=180,.release_position_valid=true};
  portable_scroll_touch(&s,&up,now+620);
  assert(portable_scroll_offset(&s)==120&&!s.contact&&s.velocity_q8>0);
  portable_scroll_configure(&s,(portable_scroll_viewport){32,100,416,500},72,2);
  assert(!s.limit&&!s.position_q8);
  portable_scroll_configure(&s,(portable_scroll_viewport){32,100,416,500},INT_MAX,INT_MAX);
  assert(!s.count&&!s.limit);
  puts("Bounded touch scrolling: wraparound clock, snapshot release, bounds, clipping, momentum interruption and suspension passed");
  return 0;
}
