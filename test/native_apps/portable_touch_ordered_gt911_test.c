/* Current production GT911 source with its register-level physical-I/O fixture. */
#define main gt911_fixture_main
#include X4_GT911_FIXTURE
#undef main
#include "PortableTouch.h"
static unsigned retained_calls,touch_issues;
void portable_adapter_retain(void) {++retained_calls;}
void portable_adapter_touch_issue(const char *operation,int status_code,int terminal) {
  (void)status_code;(void)terminal;if(operation)++touch_issues;
}
static portable_touch touch;
static portable_touch_sample sample;
static bool step(void) {return portable_touch_next(&touch,&sample);}
static void drain(void) {
  for(unsigned i=0;i<RISC_TOUCH_QUEUE_LENGTH+8u;++i)if(!step())return;
  assert(!"ordered stream never reached empty");
}
static bool tap(void) {return sample.valid && sample.released && sample.tap_eligible && !sample.moved && !sample.cancelled;}
static void report(unsigned contacts,unsigned x,unsigned y,bool home) {
  packet((uint8_t)contacts,(uint16_t)x,(uint16_t)y,home);assert(portable_touch_collect(&touch));
}
static void fresh_tap(void) {
  report(1,120,300,false);report(0,0,0,false);
  assert(step() && sample.began && sample.tap_eligible && sample.contact_id==1);
  assert(step() && tap());assert(!step());
}
static void neutral(void) {
  report(0,0,0,false);drain();assert(touch.neutral && touch.home_neutral);
}
static int32_t fault_next(void *c,uint64_t sub,risc_touch_event_v1 *out) {(void)c;(void)sub;(void)out;return -2;}
static bool fault_snapshot(void *c,risc_touch_snapshot_v1 *out) {(void)c;(void)out;return false;}
int main(int argc,char **argv) {
  assert(argc==2);driver=t5_driver_get(2);assert(driver);api=driver->capability;assert(start());
  touch=(portable_touch){.api=api,.subscription=api->subscribe(NULL)};assert(touch.subscription);
  assert(!step() && touch.neutral);neutral();
  const char *scenario=argv[1];
  if(!strcmp(scenario,"bursts")) {
    for(unsigned i=0;i<8;++i){report(1,100+i,200+i,false);report(0,0,0,false);}
    unsigned taps=0,begins=0;uint64_t previous=0;
    while(step()){assert(!sample.cancelled && sample.timestamp_ms>previous);previous=sample.timestamp_ms;taps+=tap();begins+=sample.began;}
    assert(taps==8 && begins==8 && !touch_issues);
  }else if(!strcmp(scenario,"subscribers")) {
    portable_touch other={.api=api,.subscription=api->subscribe(NULL)};
    portable_touch_sample other_sample;assert(other.subscription);
    assert(!portable_touch_next(&other,&other_sample));
    for(unsigned i=0;i<8;++i){report(1,100+i,200+i,false);report(0,0,0,false);}
    unsigned taps=0;while(step())taps+=tap();assert(taps==8);
    unsigned other_taps=0;while(portable_touch_next(&other,&other_sample)) {
      assert(!other_sample.cancelled);
      other_taps+=other_sample.released && other_sample.tap_eligible && !other_sample.moved;
    }
    assert(other_taps==8 && !touch_issues);assert(api->unsubscribe(NULL,other.subscription));
  }else if(!strcmp(scenario,"inherited")) {
    report(1,100,200,true);assert(api->unsubscribe(NULL,touch.subscription));
    touch=(portable_touch){.api=api,.subscription=api->subscribe(NULL)};assert(touch.subscription);
    assert(step() && sample.began && sample.down && !sample.tap_eligible && !sample.home_pressed);
    assert(portable_touch_adopt_drag(&touch));report(1,130,230,true);report(0,0,0,false);
    assert(step() && sample.moved && !sample.tap_eligible);
    assert(step() && sample.released && !tap());assert(step() && !sample.home_pressed);
    assert(!step());fresh_tap();
  }else if(!strcmp(scenario,"full-queue")) {
    for(unsigned i=0;i<RISC_TOUCH_QUEUE_LENGTH/2u;++i){report(1,100,200,false);report(0,0,0,false);}
    unsigned taps=0;while(step()){assert(!sample.cancelled);taps+=tap();}assert(taps==16 && !touch_issues);
  }else if(!strcmp(scenario,"multi-home")) {
    report(1,100,200,false);assert(step() && sample.began);
    packet(2,105,205,true);wire_point(1,4,150,250);assert(portable_touch_collect(&touch));
    assert(step() && sample.down && sample.moved);assert(step() && sample.cancelled);
    assert(step() && sample.home_pressed && !sample.down);assert(!step());
    /* Keep hardware ID 4, including a reordered contact record; no new gesture. */
    packet(1,150,250,true);wire_point(0,4,150,250);assert(portable_touch_collect(&touch));
    assert(step() && !sample.began && !sample.released);assert(!step());neutral();fresh_tap();
  }else if(!strcmp(scenario,"identity")) {
    report(1,100,200,false);assert(step() && sample.began && sample.contact_id==1);
    packet(1,110,220,false);wire_point(0,4,110,220);assert(portable_touch_collect(&touch));
    assert(step() && tap() && sample.contact_id==1);uint64_t when=sample.timestamp_ms;
    assert(step() && sample.began && sample.contact_id==5 && sample.timestamp_ms==when);
    report(0,0,0,false);assert(step() && tap() && sample.contact_id==5);assert(!step());
  }else if(!strcmp(scenario,"move-return")) {
    report(1,100,200,false);report(1,140,250,false);report(1,100,200,false);report(0,0,0,false);
    assert(step() && sample.began && !sample.moved);assert(step() && sample.moved);
    assert(step() && sample.moved);assert(step() && sample.released && sample.moved && !tap());assert(!step());
  }else if(!strcmp(scenario,"reset")) {
    report(1,100,200,false);report(0,0,0,false);portable_touch_reset(&touch);
    report(1,140,250,false);report(0,0,0,false);
    unsigned taps=0,begins=0;while(step()){taps+=tap();begins+=sample.began;}assert(taps==1 && begins==1);
    report(1,100,200,true);drain();portable_touch_reset(&touch);drain();
    report(1,130,230,true);while(step())assert(!sample.began && !sample.home_pressed && !tap());neutral();fresh_tap();
  }else if(!strcmp(scenario,"overflow")) {
    report(1,100,200,false);assert(step());
    for(unsigned i=0;i<RISC_TOUCH_QUEUE_LENGTH+1u;++i)report(1,110+i,200,false);
    assert(step() && sample.cancelled && !touch.neutral && !retained_calls);drain();neutral();fresh_tap();
  }else if(!strcmp(scenario,"read-failure") || !strcmp(scenario,"ack-failure") || !strcmp(scenario,"partial-read")) {
    report(1,100,200,false);assert(step());packet(1,130,230,true);
    if(!strcmp(scenario,"read-failure"))read_ok=false;
    else if(!strcmp(scenario,"partial-read")){point_ok=false;partial_point_bytes=3;}
    else ack_ok=false;
    assert(!portable_touch_collect(&touch));assert(step() && sample.cancelled && !tap() && !sample.home_pressed);
    read_ok=point_ok=ack_ok=true;assert(portable_touch_collect(&touch));
    while(step())assert(!tap() && !sample.home_pressed);
    assert(!retained_calls && !touch.neutral);neutral();fresh_tap();
  }else if(!strcmp(scenario,"malformed-report")) {
    report(1,100,200,false);assert(step());packet(2,110,210,true); /* Duplicate wire ID 0. */
    assert(!portable_touch_collect(&touch));assert(step() && sample.cancelled && !retained_calls);drain();neutral();fresh_tap();
  }else if(!strcmp(scenario,"terminal-next") || !strcmp(scenario,"terminal-snapshot") || !strcmp(scenario,"terminal-unlock")) {
    risc_touch_api_v1 injected=*api;
    if(!strcmp(scenario,"terminal-next")){injected.next=fault_next;touch.api=&injected;}
    else if(!strcmp(scenario,"terminal-snapshot")){injected.snapshot=fault_snapshot;touch.api=&injected;}
    else {packet(1,100,200,true);unlock_ok=false;assert(!portable_touch_collect(&touch));}
    assert(!step() && sample.cancelled && touch.terminal && retained_calls==1);
    unsigned frozen=operations,frozen_takes=takes,frozen_transacts=transacts;
    for(unsigned i=0;i<4;++i){assert(!portable_touch_collect(&touch));assert(!step());portable_touch_reset(&touch);assert(!portable_touch_close(&touch,NULL));}
    assert(operations==frozen && takes==frozen_takes && transacts==frozen_transacts && retained_calls==1);
    printf("Actual GT911 ordered touch: %s PASS\n",scenario);return 0;
  }else assert(!"unknown scenario");
  assert(!retained_calls);done(touch.subscription);
  printf("Actual GT911 ordered touch: %s PASS\n",scenario);return 0;
}
