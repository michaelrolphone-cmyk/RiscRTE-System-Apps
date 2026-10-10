/* Production reducer + one synthetic provider subscriber queue, never a replay
 * queue in the consumer. Every next() call consumes at most one raw event. */
#include "PortableTouch.h"
#include <assert.h>
#include <stdio.h>
static struct {
  risc_touch_snapshot_v1 state;
  risc_touch_event_v1 events[RISC_TOUCH_QUEUE_LENGTH];
  unsigned head,count,polls,nexts,snapshots,unsubscribes,releases;
  bool gap,poll_ok,snapshot_ok,race;
  int next_error;
} provider;
static unsigned retains,issues;
static const char *last_issue;
void portable_adapter_retain(void) {++retains;}
void portable_adapter_touch_issue(const char *op,int status,int terminal) {
  (void)status;(void)terminal;if(op){++issues;last_issue=op;}
}
static void event(unsigned kind,unsigned id,unsigned x,unsigned y,uint64_t stamp) {
  risc_touch_snapshot_v1 *s=&provider.state;
  int at=-1;for(unsigned i=0;i<s->contact_count;++i)if(s->contacts[i].id==id)at=(int)i;
  if(kind==RISC_TOUCH_EVENT_DOWN) {
    assert(at<0 && s->contact_count<RISC_TOUCH_MAX_CONTACTS);
    s->contacts[s->contact_count++]=(risc_touch_contact_v1){.id=id,.x=x,.y=y};
  }else if(kind==RISC_TOUCH_EVENT_MOVE) {
    assert(at>=0);s->contacts[at].x=x;s->contacts[at].y=y;
  }else if(kind==RISC_TOUCH_EVENT_UP) {
    assert(at>=0);for(unsigned i=(unsigned)at+1u;i<s->contact_count;++i)s->contacts[i-1u]=s->contacts[i];--s->contact_count;
  }else if(kind==RISC_TOUCH_EVENT_BUTTON_DOWN)s->buttons|=(uint32_t)1u<<id;
  else if(kind==RISC_TOUCH_EVENT_BUTTON_UP)s->buttons&=~((uint32_t)1u<<id);
  risc_touch_event_v1 e={.sequence=++s->sequence,.timestamp_ms=stamp,.kind=kind,.id=id,.x=x,.y=y};
  s->timestamp_ms=stamp;
  if(provider.gap)return;
  if(provider.count==RISC_TOUCH_QUEUE_LENGTH) {provider.gap=true;provider.head=provider.count=0;return;}
  provider.events[(provider.head+provider.count)%RISC_TOUCH_QUEUE_LENGTH]=e;++provider.count;
}
static uint64_t subscribe(void *c) {(void)c;provider.head=provider.count=0;provider.gap=false;return 1;}
static bool unsubscribe(void *c,uint64_t s) {(void)c;assert(s==1);++provider.unsubscribes;return true;}
static bool poll(void *c,size_t n) {(void)c;assert(n==1);++provider.polls;return provider.poll_ok;}
static int32_t next(void *c,uint64_t s,risc_touch_event_v1 *out) {
  (void)c;assert(s==1);++provider.nexts;if(provider.next_error)return provider.next_error;
  if(provider.gap){provider.gap=false;provider.head=provider.count=0;return -1;}
  if(!provider.count)return 0;
  *out=provider.events[provider.head];provider.head=(provider.head+1u)%RISC_TOUCH_QUEUE_LENGTH;--provider.count;return 1;
}
static bool snapshot(void *c,risc_touch_snapshot_v1 *out) {
  (void)c;++provider.snapshots;if(!provider.snapshot_ok)return false;
  if(provider.race){provider.race=false;event(RISC_TOUCH_EVENT_DOWN,7,123,234,provider.state.timestamp_ms+1u);}
  *out=provider.state;return true;
}
static const risc_touch_api_v1 api={1,sizeof(api),NULL,subscribe,unsubscribe,poll,next,snapshot};
static bool acquire(const char *id,uint32_t version,uint64_t flags,risc_runtime_capability_v1 *out) {
  assert(!strcmp(id,"input.touch.raw") && version==1 && !flags);out->api=&api;return true;
}
static bool release(risc_runtime_capability_v1 *grant) {(void)grant;++provider.releases;return true;}
static const risc_runtime_api_v1 runtime={.acquire=acquire,.release=release};
static portable_touch touch;
static portable_touch_sample sample;
static bool step(void) {
  unsigned before=provider.count,n=provider.nexts;
  bool progress=portable_touch_next(&touch,&sample);
  assert(provider.nexts<=n+1u);
  if(!provider.gap && before && provider.count)assert(provider.count>=before-1u);
  return progress;
}
static void drain(void) {
  for(unsigned i=0;i<RISC_TOUCH_QUEUE_LENGTH+8u;++i)if(!step())return;
  assert(!"reducer did not reach empty");
}
static void reset(void) {
  memset(&provider,0,sizeof(provider));provider.poll_ok=provider.snapshot_ok=true;
  provider.state.width=480;provider.state.height=800;retains=issues=0;last_issue=NULL;
  assert(portable_touch_open(&touch,&runtime));assert(touch.neutral && touch.home_neutral);
  assert(!step() && sample.valid && !sample.down);
}
static bool tap(void) {return sample.valid && sample.released && sample.tap_eligible && !sample.moved && !sample.cancelled;}
static void point(unsigned x,unsigned y) {
#if PORTABLE_TOUCH_ROTATION == 180
  x=479u-x;y=799u-y;
#endif
  assert(sample.x==x && sample.y==y);
}
static void fresh_tap(unsigned id,uint64_t when) {
  event(RISC_TOUCH_EVENT_DOWN,id,90,210,when);event(RISC_TOUCH_EVENT_UP,id,90,210,when+1u);
  assert(step() && sample.began && sample.tap_eligible && sample.contact_id==id);
  assert(step() && tap() && sample.contact_id==id && sample.timestamp_ms==when+1u);point(90,210);
  assert(!step());
}
static void bursts(void) {
  reset();unsigned before=provider.nexts,snaps=provider.snapshots;
  event(RISC_TOUCH_EVENT_DOWN,1,100,200,10);event(RISC_TOUCH_EVENT_UP,1,100,200,11);
  event(RISC_TOUCH_EVENT_DOWN,2,150,300,12);event(RISC_TOUCH_EVENT_UP,2,150,300,13);
  for(unsigned i=0;i<20;++i)assert(portable_touch_collect(&touch));
  assert(provider.count==4 && provider.nexts==before && provider.snapshots==snaps);
  assert(step() && sample.began && sample.contact_id==1 && sample.timestamp_ms==10);point(100,200);
  assert(step() && tap() && sample.timestamp_ms==11);point(100,200);
  assert(step() && sample.began && sample.contact_id==2 && sample.timestamp_ms==12);point(150,300);
  assert(step() && tap() && sample.timestamp_ms==13);point(150,300);
  assert(!step() && !issues && !retains);
  /* A full but intact provider queue is not an artificial event-budget fault. */
  reset();for(unsigned i=0;i<RISC_TOUCH_QUEUE_LENGTH/2u;++i) {
    event(RISC_TOUCH_EVENT_DOWN,1,100,200,2u*i);event(RISC_TOUCH_EVENT_UP,1,100,200,2u*i+1u);
  }
  unsigned taps=0,begins=0;while(step()){taps+=tap();begins+=sample.began;assert(!sample.cancelled);}
  assert(taps==RISC_TOUCH_QUEUE_LENGTH/2u && begins==taps && !issues);
  reset();event(RISC_TOUCH_EVENT_DOWN,5,45,55,20);event(RISC_TOUCH_EVENT_UP,5,45,55,21);
  portable_touch_read(&touch,&sample);assert(sample.began && provider.count==1);
  portable_touch_read(&touch,&sample);assert(tap() && !provider.count);
}
static void ordered_recontacts(void) {
  /* Millisecond equality does not identify a report. In particular, the
   * selected GT911 emits same-ID UP/DOWN only across distinct reports. Every
   * ordered UP still completes its gesture even when the snapshot is ahead. */
  for(unsigned different=0;different<2;++different)for(unsigned home=0;home<2;++home)
  for(unsigned page=0;page<2;++page) {
    unsigned id=different?2u:1u;
    reset();event(RISC_TOUCH_EVENT_DOWN,1,100,200,40);assert(step() && sample.began);
    uint64_t sequence=touch.sequence;
    event(RISC_TOUCH_EVENT_UP,1,100,200,61);
    event(RISC_TOUCH_EVENT_DOWN,id,100,200,61);
    if(home)event(RISC_TOUCH_EVENT_BUTTON_DOWN,0,0,0,61);
    assert(provider.state.contact_count==1 && provider.state.timestamp_ms==61);
    unsigned snapshots=provider.snapshots;
    assert(step() && tap() && sample.contact_id==1 && sample.timestamp_ms==61);
    assert(touch.sequence==sequence+1u && provider.count==1u+home);
    assert(provider.snapshots==snapshots); /* No speculative snapshot of UP. */
    if(page) {
      unsigned nexts=provider.nexts,polls=provider.polls,count=provider.count;
      portable_touch_cancel_gesture(&touch);
      assert(provider.nexts==nexts && provider.polls==polls && provider.snapshots==snapshots);
      assert(step() && sample.cancelled && provider.count==count);
    }
    assert(step() && sample.began && sample.tap_eligible && sample.contact_id==id);
    assert(touch.sequence==sequence+2u && sample.timestamp_ms==61);
    if(home)assert(step() && sample.home_pressed && sample.down);
    assert(!step() && sample.down && !sample.cancelled);
    event(RISC_TOUCH_EVENT_UP,id,100,200,103);
    if(home)event(RISC_TOUCH_EVENT_BUTTON_UP,0,0,0,103);
    assert(step() && tap() && sample.contact_id==id);
    if(home)assert(step() && !sample.home_pressed && touch.home_neutral);
    assert(!step() && !issues && !retains);fresh_tap(3,104);
  }
  /* Even an entire intact queue at one millisecond must retain every tap. */
  reset();for(unsigned i=0;i<RISC_TOUCH_QUEUE_LENGTH/2u;++i) {
    unsigned id=1u+i%2u;
    event(RISC_TOUCH_EVENT_DOWN,id,100,200,1);event(RISC_TOUCH_EVENT_UP,id,100,200,1);
  }
  unsigned taps=0,begins=0;
  while(step()){assert(!sample.cancelled && sample.timestamp_ms==1);taps+=tap();begins+=sample.began;}
  assert(taps==RISC_TOUCH_QUEUE_LENGTH/2u && begins==taps && !issues && !retains);
}
static void malformed_recontact(void) {
  /* A repeated DOWN without a preceding UP remains malformed, independent
   * of the timestamp. It cannot be reclassified as a valid rapid tap. */
  reset();event(RISC_TOUCH_EVENT_DOWN,1,100,200,40);assert(step() && sample.began);
  event(RISC_TOUCH_EVENT_MOVE,1,100,200,61);
  provider.events[provider.head].kind=RISC_TOUCH_EVENT_DOWN;
  event(RISC_TOUCH_EVENT_BUTTON_DOWN,0,0,0,61);
  assert(step() && sample.cancelled && !tap() && !touch.neutral);
  assert(step() && sample.cancelled && !sample.home_pressed);assert(!step());
  event(RISC_TOUCH_EVENT_UP,1,100,200,103);event(RISC_TOUCH_EVENT_BUTTON_UP,0,0,0,103);
  assert(step() && !sample.released && !tap());assert(step() && !sample.home_pressed);
  assert(!step() && touch.neutral && touch.home_neutral && !retains);fresh_tap(2,104);
}
static void moves_and_identity(void) {
  reset();event(RISC_TOUCH_EVENT_DOWN,4,100,200,1);event(RISC_TOUCH_EVENT_MOVE,4,140,250,2);
  event(RISC_TOUCH_EVENT_MOVE,4,100,200,3);event(RISC_TOUCH_EVENT_UP,4,100,200,4);
  assert(step() && sample.began && !sample.moved);assert(step() && sample.moved);point(140,250);
  assert(step() && sample.moved);point(100,200);assert(step() && sample.released && sample.moved && !tap());
#ifdef PORTABLE_TOUCH_SCROLL
  assert(sample.release_position_valid);
#endif
  fresh_tap(4,5);fresh_tap(8,7);
}
static void multiple_contacts(void) {
  reset();event(RISC_TOUCH_EVENT_DOWN,1,100,200,1);event(RISC_TOUCH_EVENT_DOWN,2,150,250,1);
  event(RISC_TOUCH_EVENT_MOVE,1,110,210,2);event(RISC_TOUCH_EVENT_BUTTON_DOWN,0,0,0,2);
  event(RISC_TOUCH_EVENT_UP,1,110,210,3);event(RISC_TOUCH_EVENT_UP,2,150,250,3);
  event(RISC_TOUCH_EVENT_BUTTON_UP,0,0,0,3);
  assert(step() && sample.began);assert(step() && sample.cancelled && !sample.released && !touch.neutral);
  assert(step() && !sample.down && !sample.began);assert(step() && sample.home_pressed);
  assert(step() && !sample.released && !touch.neutral);assert(step() && !sample.released && touch.neutral);
  assert(step() && !sample.home_pressed && touch.home_neutral);assert(!step());fresh_tap(9,4);
}
static void same_report_motion_cancel(void) {
  reset();event(RISC_TOUCH_EVENT_DOWN,1,100,200,1);assert(step()&&sample.began);
  event(RISC_TOUCH_EVENT_MOVE,1,180,200,2);event(RISC_TOUCH_EVENT_DOWN,2,200,300,2);
  assert(step()&&sample.cancelled&&!sample.down&&!sample.released);
  assert(step()&&sample.cancelled);
  event(RISC_TOUCH_EVENT_UP,1,180,200,3);event(RISC_TOUCH_EVENT_UP,2,200,300,3);
  assert(step()&&!tap());assert(step()&&!tap());assert(!step());fresh_tap(3,4);
  /* A later independent report cannot retrospectively invalidate a MOVE. */
  reset();event(RISC_TOUCH_EVENT_DOWN,1,100,200,1);assert(step());
  event(RISC_TOUCH_EVENT_MOVE,1,180,200,2);event(RISC_TOUCH_EVENT_DOWN,2,200,300,3);
  assert(step()&&sample.down&&sample.moved&&!sample.cancelled);assert(step()&&sample.cancelled);
}
static void home_and_inherited(void) {
  reset();event(RISC_TOUCH_EVENT_BUTTON_DOWN,0,0,0,1);event(RISC_TOUCH_EVENT_BUTTON_UP,0,0,0,2);
  event(RISC_TOUCH_EVENT_BUTTON_DOWN,0,0,0,3);event(RISC_TOUCH_EVENT_BUTTON_UP,0,0,0,4);
  for(unsigned i=0;i<4;++i){assert(step());assert(sample.home_pressed==(i%2u==0));}assert(!step());
  reset();event(RISC_TOUCH_EVENT_DOWN,6,111,222,5);event(RISC_TOUCH_EVENT_BUTTON_DOWN,0,0,0,5);
  assert(portable_touch_open(&touch,&runtime));assert(step() && sample.began && sample.down && !sample.tap_eligible && !sample.home_pressed);
  event(RISC_TOUCH_EVENT_MOVE,6,115,226,6);event(RISC_TOUCH_EVENT_UP,6,115,226,7);event(RISC_TOUCH_EVENT_BUTTON_UP,0,0,0,7);
  assert(step() && !sample.tap_eligible);assert(step() && sample.released && !sample.tap_eligible && !tap());
  assert(step() && !sample.home_pressed);assert(!step());fresh_tap(7,8);
}
static void boundaries(void) {
  reset();event(RISC_TOUCH_EVENT_DOWN,1,100,200,1);event(RISC_TOUCH_EVENT_UP,1,100,200,2);
  unsigned nexts=provider.nexts,polls=provider.polls;
  portable_touch_reset(&touch);assert(provider.nexts==nexts && provider.polls==polls);
  event(RISC_TOUCH_EVENT_DOWN,2,200,300,3);event(RISC_TOUCH_EVENT_UP,2,200,300,4);
  assert(portable_touch_collect(&touch));assert(step() && sample.cancelled);
  assert(step() && sample.cancelled);assert(step() && sample.cancelled);
  assert(step() && sample.began && sample.contact_id==2 && sample.tap_eligible);
  assert(step() && tap() && sample.contact_id==2);assert(!step());
  reset();event(RISC_TOUCH_EVENT_DOWN,3,100,200,1);assert(step() && sample.began);
  portable_touch_reset(&touch);event(RISC_TOUCH_EVENT_MOVE,3,130,230,2);event(RISC_TOUCH_EVENT_UP,3,130,230,3);
  assert(step() && sample.cancelled);assert(step() && !sample.down && !sample.began);
  assert(step() && !sample.released && !tap());assert(!step());fresh_tap(4,4);
  /* Interleaved polling after next(empty) must not jump over the new edge. */
  reset();provider.race=true;assert(!step() && !sample.down && !sample.began && provider.count==1);
  assert(step() && sample.began && sample.contact_id==7 && sample.timestamp_ms==1);point(123,234);
  event(RISC_TOUCH_EVENT_UP,7,123,234,2);assert(step() && tap());assert(!step());
}
static void page_and_drag(void) {
  reset();event(RISC_TOUCH_EVENT_DOWN,1,100,200,1);event(RISC_TOUCH_EVENT_UP,1,100,200,2);
  event(RISC_TOUCH_EVENT_DOWN,2,140,240,3);event(RISC_TOUCH_EVENT_UP,2,140,240,4);
  assert(step() && sample.began);assert(step() && tap());
  unsigned nexts=provider.nexts,snaps=provider.snapshots,polls=provider.polls;
  portable_touch_cancel_gesture(&touch);
  assert(provider.nexts==nexts && provider.snapshots==snaps && provider.polls==polls);
  assert(step() && sample.cancelled);assert(step() && sample.began && sample.contact_id==2);
  assert(step() && tap() && sample.contact_id==2);assert(!step());
  event(RISC_TOUCH_EVENT_DOWN,3,100,200,5);assert(step() && sample.began);
  portable_touch_cancel_gesture(&touch);assert(step() && sample.cancelled);
  event(RISC_TOUCH_EVENT_MOVE,3,130,230,6);event(RISC_TOUCH_EVENT_UP,3,130,230,7);
  assert(step() && !sample.down && !sample.began);assert(step() && !sample.released && !tap());assert(!step());fresh_tap(4,8);
  reset();event(RISC_TOUCH_EVENT_DOWN,6,111,222,1);assert(portable_touch_open(&touch,&runtime));
  assert(step() && sample.down && !sample.tap_eligible);assert(portable_touch_adopt_drag(&touch));
  assert(!touch.neutral && touch.moved);event(RISC_TOUCH_EVENT_UP,6,111,222,2);
  assert(step() && sample.released && !sample.tap_eligible && sample.moved && !tap());assert(!step());
  assert(!portable_touch_adopt_drag(&touch));fresh_tap(7,3);
}
static void recoverable_faults(void) {
  reset();event(RISC_TOUCH_EVENT_DOWN,1,100,200,1);assert(step() && sample.began);
  event(RISC_TOUCH_EVENT_MOVE,1,120,220,2);provider.poll_ok=false;
  assert(!portable_touch_collect(&touch));assert(step() && sample.cancelled && !touch.down && !sample.tap_eligible);
  /* Continued failed collection must not starve drain/snapshot recovery. */
  assert(!portable_touch_collect(&touch));(void)step();assert(!provider.count && !touch.neutral);
  provider.poll_ok=true;assert(portable_touch_collect(&touch));drain();event(RISC_TOUCH_EVENT_UP,1,120,220,3);assert(step() && !sample.released);assert(!step());fresh_tap(2,4);
  reset();event(RISC_TOUCH_EVENT_DOWN,1,100,200,1);assert(step());
  for(unsigned i=0;i<RISC_TOUCH_QUEUE_LENGTH+1u;++i)event(RISC_TOUCH_EVENT_MOVE,1,110+i,200,2u+i);
  assert(step() && sample.cancelled && !retains && !touch.neutral);
  event(RISC_TOUCH_EVENT_UP,1,142,200,40);assert(step() && !sample.released);assert(!step());fresh_tap(3,41);
  for(unsigned bad=0;bad<3;++bad) {
    reset();event(RISC_TOUCH_EVENT_DOWN,1,100,200,1);assert(step());
    event(RISC_TOUCH_EVENT_MOVE,1,120,200,2);
    risc_touch_event_v1 *e=&provider.events[provider.head];
    if(!bad)e->sequence+=1;else if(bad==1)e->sequence-=1;else e->timestamp_ms=0;
    assert(step() && sample.cancelled && !sample.released);assert(!step());
    event(RISC_TOUCH_EVENT_UP,1,120,200,3);assert(step() && !sample.released);assert(!step());fresh_tap(2,4);
  }
  reset();event(RISC_TOUCH_EVENT_DOWN,1,100,200,1);provider.events[provider.head].x=480;
  assert(step() && sample.cancelled);assert(!step());event(RISC_TOUCH_EVENT_UP,1,100,200,2);assert(step() && !tap());assert(!step());fresh_tap(2,3);
  reset();provider.state.width=0;assert(step() && sample.cancelled);assert(!step() && sample.cancelled);
  provider.state.width=480;assert(!step() && touch.neutral);fresh_tap(2,1);
}
static void terminal_faults(void) {
  for(unsigned which=0;which<5;++which) {
    reset();if(which==0)provider.next_error=-2;else if(which==1)provider.next_error=-17;
    else {
      if(which==4) {
        event(RISC_TOUCH_EVENT_DOWN,1,100,200,1);assert(step() && sample.began);
        event(RISC_TOUCH_EVENT_MOVE,1,110,200,2);event(RISC_TOUCH_EVENT_DOWN,2,150,250,2);
      }
      provider.snapshot_ok=false;
    }
    if(which==3)portable_touch_reset(&touch);else assert(!step() && sample.cancelled);
    assert(touch.terminal);
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
    assert(retains==1);
#endif
    unsigned polls=provider.polls,nexts=provider.nexts,snaps=provider.snapshots,unsubs=provider.unsubscribes,releases=provider.releases;
    for(unsigned i=0;i<4;++i){assert(!portable_touch_collect(&touch));assert(!step() && sample.cancelled);portable_touch_reset(&touch);portable_touch_read(&touch,&sample);assert(!portable_touch_close(&touch,&runtime));}
    assert(provider.polls==polls && provider.nexts==nexts && provider.snapshots==snaps && provider.unsubscribes==unsubs && provider.releases==releases);
  }
}
int main(void) {
  bursts();ordered_recontacts();malformed_recontact();moves_and_identity();multiple_contacts();same_report_motion_cancel();home_and_inherited();boundaries();page_and_drag();recoverable_faults();terminal_faults();
  reset();assert(portable_touch_close(&touch,&runtime));assert(provider.unsubscribes==1 && provider.releases==1);
  puts("Ordered touch: bursts, equal-timestamp recontacts, full queue, compatibility, identity/time, excursion, multi-contact, Home, inherited holds, page/reset boundaries, snapshot race, malformed recontact recovery, and terminal snapshot freeze PASS");return 0;
}
