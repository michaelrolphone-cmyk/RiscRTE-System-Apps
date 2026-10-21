/* Native test app only. The production helper never polls. The producer
 * harness below runs synchronously on the same checked native owner thread. */
#include <PortableMouseConsumer.h>
#include "mouse_runtime_fixture.h"
#include <assert.h>
#include <string.h>

static portable_mouse_consumer consumer;
static const risc_runtime_api_v1 *runtime;
static unsigned calls(void) {return mouse_runtime_control()->lower_calls;}
static bool mode(const char *s) {return !strcmp(mouse_runtime_mode(),s);}
static void event_empty(const portable_mouse_frame *f) {
 risc_usb_mouse_event_v1 empty={0};assert(!memcmp(&f->event,&empty,sizeof(empty)));
}
static void report(uint8_t buttons, int x, int y, int wheel, int pan) {
 mouse_runtime_controls *c=mouse_runtime_control();assert(!c->remaining);
 c->report[0]=buttons;c->report[1]=(uint8_t)x;c->report[2]=(uint8_t)y;
 c->report[3]=(uint8_t)wheel;c->report[4]=(uint8_t)pan;c->remaining=1;
}
static void produce(const risc_usb_mouse_api_v1 *api) {
 assert(mouse_runtime_test_only_poll(api->poll,api->context,1));
}
static void next_local(int expected, portable_mouse_frame *f) {
 unsigned before=calls();assert(portable_mouse_next(&consumer,f)==expected);
 assert(calls()==before);
}
static void focus_local(bool focused, int expected, portable_mouse_frame *f) {
 unsigned before=calls();assert(portable_mouse_focus(&consumer,focused,f)==expected);
 assert(calls()==before);
}
__attribute__((visibility("default"))) int app_module_init(void) {
 runtime=risc_runtime_get_api(1);assert(portable_mouse_init(&consumer,runtime,0,0));
 mouse_runtime_event("app:init");return 0;
}
__attribute__((visibility("default"))) void app_main(void) {
 /* A second real generic Runtime grant seeds a nonempty initial baseline.
  * The seed is released while the helper's grant still pins the provider. */
 risc_runtime_capability_v1 seed={.struct_size=sizeof(seed)};
 assert(runtime->acquire("usb.hid.mouse",1,0,&seed));
 const risc_usb_mouse_api_v1 *producer=seed.api;
 report(3,0,0,0,0);produce(producer);
 portable_mouse_frame f;unsigned before=calls();
 assert(portable_mouse_open(&consumer,&f)==PMC_BASELINE && calls()==before);
 assert(f.current_count==1 && f.current[0].device==100 && f.current[0].session &&
        f.current[0].buttons==3 && f.current[0].connected && !f.previous_count);
 event_empty(&f);uint64_t session=f.current[0].session;
 before=calls();assert(runtime->release(&seed) && calls()==before && !seed.api);
 mouse_runtime_event("case:initial-baseline");

 report(1,7,-9,2,-3);produce(producer);
 next_local(PMC_EVENT,&f);
 assert(f.event.kind==RISC_USB_MOUSE_REPORT && f.event.x==7 && f.event.y==-9 &&
        f.event.wheel==2 && f.event.pan==-3 && !f.event.pressed && f.event.released==2);
 assert(f.previous_count==1 && f.previous[0].buttons==3 &&
        f.current_count==1 && f.current[0].buttons==1);
 risc_usb_mouse_event_v1 copied=f.event;
 next_local(PMC_EMPTY,&f);event_empty(&f);
 assert(copied.x==7 && copied.y==-9 && copied.state.session==session);
 mouse_runtime_event("case:copied-once");

 if(mode("connected-close")) {
  before=calls();
  assert(portable_mouse_close(&consumer,&f)==PMC_CLOSED && !consumer.grant.api && !consumer.subscription);
  assert(f.previous_count==1 && f.previous[0].buttons==1 && !f.current_count);event_empty(&f);
  if(mouse_runtime_demand())assert(calls()==before+1 && !mouse_runtime_control()->raw && mouse_runtime_control()->closes==1);
  else assert(calls()==before && mouse_runtime_control()->raw && !mouse_runtime_control()->closes);
  mouse_runtime_event("case:connected-close");mouse_runtime_event("app:complete");return;
 }
 if(mode("close-retained")) {
  mouse_runtime_control()->close_fails=true;
  const risc_runtime_capability_v1 exact=consumer.grant;
  before=calls();int rc=portable_mouse_close(&consumer,&f);
  assert(f.previous_count==1 && f.previous[0].buttons==1 && !f.current_count);
  event_empty(&f);
  if(mouse_runtime_demand()) {
   assert(rc==PMC_RETAINED && consumer.cleanup_required && consumer.fence_accepted);
   assert(consumer.grant.slot==exact.slot && consumer.grant.generation==exact.generation &&
          consumer.grant.api==exact.api && consumer.mouse==producer && !consumer.subscription);
   assert(calls()==before+1 && mouse_runtime_control()->closes==1 && mouse_runtime_control()->raw);
   before=calls();
   next_local(PMC_RETAINED,&f);focus_local(true,PMC_RETAINED,&f);
   assert(portable_mouse_close(&consumer,&f)==PMC_RETAINED && calls()==before);
   mouse_runtime_event("case:demand-helper-retained");
  } else {
   /* The eager boot grant still owns the graph. Failure is deferred to the
    * real Runtime teardown, NOT misreported as helper release quiescence. */
   assert(rc==PMC_CLOSED && !consumer.cleanup_required && !consumer.grant.api);
   assert(calls()==before && mouse_runtime_control()->raw);
   mouse_runtime_event("case:eager-boot-grant-remains");
  }
  return;
 }

 /* One queued focused event and one unfocused event must both be discarded.
  * Focus loss releases held state locally; resume takes a fresh held baseline. */
 report(1,11,0,0,0);produce(producer);
 focus_local(false,PMC_RESET,&f);
 assert(f.previous_count==1 && f.previous[0].buttons==1 && !f.current_count);event_empty(&f);
 next_local(PMC_PAUSED,&f);event_empty(&f);
 report(2,23,0,0,0);produce(producer);
 focus_local(true,PMC_RESET,&f);event_empty(&f);
 next_local(PMC_BASELINE,&f);
 assert(f.current_count==1 && f.current[0].buttons==2 && f.current[0].session==session);event_empty(&f);
 next_local(PMC_EMPTY,&f);event_empty(&f);
 report(0,-5,4,0,0);produce(producer);next_local(PMC_EVENT,&f);
 assert(f.event.x==-5 && f.event.y==4 && f.event.released==2 && !f.event.pressed);
 next_local(PMC_EMPTY,&f);event_empty(&f);
 mouse_runtime_event("case:focus-discard");

 for(unsigned i=0;i<RISC_USB_INPUT_QUEUE_LENGTH+8;i++) {
  report((uint8_t)(i&1),1,0,0,0);produce(producer);
 }
 next_local(PMC_GAP,&f);assert(f.previous_count==1 && !f.current_count);event_empty(&f);
 next_local(PMC_BASELINE,&f);
 assert(f.current_count==1 && f.current[0].buttons==1 && f.current[0].session==session);event_empty(&f);
 next_local(PMC_EMPTY,&f);event_empty(&f);
 mouse_runtime_event("case:overflow-resnapshot");

 mouse_runtime_control()->present=false;produce(producer);
 next_local(PMC_EVENT,&f);
 assert(f.event.kind==RISC_USB_MOUSE_DISCONNECTED && f.event.state.session==session &&
        f.event.released==1 && !f.event.state.connected && !f.event.state.buttons &&
        !f.event.x && !f.event.y && !f.current_count);
 assert(f.previous_count==1 && f.previous[0].buttons==1);
 next_local(PMC_EMPTY,&f);event_empty(&f);
 assert(!mouse_runtime_control()->raw && mouse_runtime_control()->closes==1);
 mouse_runtime_event("case:disconnect-once");
 /* usb.host device tokens are generation-qualified. Re-enumeration creates
  * a new device token and a fresh mouse semantic session. */
 mouse_runtime_control()->device=101;mouse_runtime_control()->present=true;
 report(0,0,0,0,0);produce(producer);
 next_local(PMC_EVENT,&f);
 assert(f.event.kind==RISC_USB_MOUSE_CONNECTED && f.event.state.device==101 &&
        f.event.state.session!=session && f.event.state.session && !f.event.state.buttons);
 uint64_t replacement=f.event.state.session;
 next_local(PMC_EMPTY,&f);event_empty(&f);
 report(1,6,0,0,0);produce(producer);next_local(PMC_EVENT,&f);
 assert(f.event.kind==RISC_USB_MOUSE_REPORT && f.event.state.session==replacement &&
        f.event.x==6 && f.event.pressed==1 && !f.event.released);
 mouse_runtime_event("case:reconnect-session");
 before=calls();
 assert(portable_mouse_close(&consumer,&f)==PMC_CLOSED && !consumer.grant.api &&
        !consumer.subscription && !consumer.cleanup_required);
 if(mouse_runtime_demand())assert(calls()==before+1 && !mouse_runtime_control()->raw && mouse_runtime_control()->closes==2);
 else assert(calls()==before && mouse_runtime_control()->raw && mouse_runtime_control()->closes==1);
 mouse_runtime_event("app:complete");
}
__attribute__((visibility("default"))) void app_module_fini(void) {mouse_runtime_event("app:fini");}
