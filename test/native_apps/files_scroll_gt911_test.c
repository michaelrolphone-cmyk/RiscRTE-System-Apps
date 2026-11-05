/* Production GT911 wire reports -> ordered consumer -> Files scroll primitive.
 * GPIO/I2C and time are deterministic fixtures, not physical hardware. */
#define main gt911_base_main
#include X4_GT911_FIXTURE
#undef main
#include "PortableTouchScroll.h"
static unsigned retained_calls;
void portable_adapter_retain(void){++retained_calls;}
void portable_adapter_touch_issue(const char *op,int status_code,int terminal){
 (void)op;(void)status_code;(void)terminal;
}
static portable_touch consumer;
static portable_touch_scroll scroll;
static portable_touch_sample latest;
static uint64_t witness;
static uint16_t last_raw_x,last_raw_y;
static unsigned reports(unsigned count,unsigned x,unsigned y,unsigned elapsed) {
 uint16_t raw_x=(uint16_t)x,raw_y=(uint16_t)y;
 portable_touch_orient(480,800,&raw_x,&raw_y);
 uint64_t when=time_ms+elapsed;
 packet((uint8_t)count,raw_x,raw_y,false);time_ms=when;
 assert(portable_touch_collect(&consumer));
 risc_touch_event_v1 event={0};int n=api->next(NULL,witness,&event);
 if(!count&&n==1){
  assert(event.kind==RISC_TOUCH_EVENT_UP);
  /* Neutral hardware reports carry no coordinates. UP must keep last MOVE. */
  assert(event.x==last_raw_x&&event.y==last_raw_y);
 }
 if(count){last_raw_x=raw_x;last_raw_y=raw_y;}
 bool progressed=portable_touch_next(&consumer,&latest);
 assert(progressed==(n==1)&&latest.valid&&!latest.cancelled);
 if(count)assert(latest.down&&latest.x==x&&latest.y==y);
 if(!count&&n==1)assert(latest.released&&latest.release_position_valid);
 return portable_scroll_touch(&scroll,&latest,(uint32_t)time_ms);
}
int main(int argc,char **argv){
 assert(argc==2);driver=t5_driver_get(2);assert(driver);api=driver->capability;assert(start());
 consumer=(portable_touch){.api=api,.subscription=api->subscribe(NULL)};assert(consumer.subscription);
 witness=api->subscribe(NULL);assert(witness);
 assert(!portable_touch_next(&consumer,&latest)&&consumer.neutral);
 portable_scroll_configure(&scroll,(portable_scroll_viewport){32,112,416,520},88,96);
 scroll.position_q8=200*256;
 bool held=!strcmp(argv[1],"held-release"),reverse=!strcmp(argv[1],"reverse"),bounds=!strcmp(argv[1],"bounds");
 assert(held||reverse||bounds||!strcmp(argv[1],"fling"));
 if(bounds)scroll.position_q8=(scroll.limit-20)*256;
 reports(1,240,500,20);assert(latest.began&&scroll.contact);
 reports(1,240,400,20);reports(1,240,300,20);
 if(reverse)reports(1,240,450,20);
 if(held)reports(1,240,300,180);
 int released=scroll.position_q8,velocity=scroll.velocity_q8;
 unsigned result=reports(0,0,0,20);
 assert(!(result&(PORTABLE_SCROLL_TAP|PORTABLE_SCROLL_BACK|PORTABLE_SCROLL_CHANGED)));
 assert(scroll.position_q8==released&&!scroll.contact);
 if(held||bounds)assert(!scroll.velocity_q8);
 else {
  assert(scroll.velocity_q8==velocity&&(reverse?velocity<0:velocity>0));
  unsigned steps=0;
  while(scroll.velocity_q8){
   int previous=scroll.position_q8;time_ms+=16;
   portable_scroll_animate(&scroll,(uint32_t)time_ms);
   assert(reverse?scroll.position_q8<=previous:scroll.position_q8>=previous);
   assert(++steps<200);
  }
  assert(reverse?scroll.position_q8<released:scroll.position_q8>released);
  /* A fresh stationary touch after settling still taps; scrolling did not
   * permanently fence ordinary Files row activation. */
  reports(1,240,300,20);
  assert(reports(0,0,0,20)&PORTABLE_SCROLL_TAP);
 }
 assert(api->unsubscribe(NULL,witness)&&!retained_calls);done(consumer.subscription);
 printf("GT911 last-position UP / Files %s rotation %u PASS\n",argv[1],PORTABLE_TOUCH_ROTATION);
 return 0;
}
