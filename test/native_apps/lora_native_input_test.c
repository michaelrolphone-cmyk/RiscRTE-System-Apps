/* The production owner11 reducer with explicit display/provider state doubles. */
#define PORTABLE_LORA_NATIVE_UI
#define PORTABLE_QUICK_ACTIONS
#include "PortableTouch.h"
#include "RiscDisplayOutputV1.h"
static uint32_t surface_format=RISC_DISPLAY_FORMAT_RGB565;
#include <assert.h>
#include <stdio.h>
static bool failed,input_pending;
static struct {struct {bool neutral_gate;} ui;} quick;
static portable_touch touch;
static portable_touch_sample input_sample;
static uint64_t paper_token;
static unsigned native_token_owner;
static unsigned snapshot_reads,event_reads;
static risc_touch_snapshot_v1 state;
static bool snapshot_ok=true;
static bool snapshot(void*c,risc_touch_snapshot_v1*s){(void)c;snapshot_reads++;if(!snapshot_ok)return false;*s=state;return true;}
static int32_t next(void*c,uint64_t token,risc_touch_event_v1*e){(void)c;(void)token;(void)e;event_reads++;return 0;}
static const risc_touch_api_v1 api={.api_version=1,.struct_size=sizeof(api),.snapshot=snapshot,.next=next};
static void input_reset(void){input_pending=false;input_sample=(portable_touch_sample){0};if(!failed)portable_touch_reset(&touch);}
static void portable_native_present(unsigned owner,bool full);
#include "lora_native_input.inc"
static void portable_native_present(unsigned owner,bool full){(void)full;assert(owner==11);native_token_owner=owner;paper_token++;lora_native_token_generation=lora_native_submit_generation;lora_native_token_layout=lora_native_submit_layout;}
static void complete(void){assert(paper_token);paper_token=0;native_token_owner=0;lora_native_presented();}
static void settle_neutral(void){
 assert(!portable_lora_native_ready(NULL,NULL));
 touch.cancel_pending=false;quick.ui.neutral_gate=true;
 assert(!portable_lora_native_ready(NULL,NULL));
 quick.ui.neutral_gate=false;
}
static void fresh(void){touch.cancel_pending=false;quick.ui.neutral_gate=false;state.sequence++;touch.sequence=state.sequence;portable_touch_sample s={.valid=true,.began=true,.down=true,.contact_id=1};assert(lora_native_input_accept(&s));}
static void release(void){portable_touch_sample s={.valid=true,.released=true,.contact_id=1};assert(lora_native_input_accept(&s));}
int main(void){
 state=(risc_touch_snapshot_v1){.width=410,.height=502,.sequence=10};touch=(portable_touch){.api=&api,.subscription=1};
 surface_format=RISC_DISPLAY_FORMAT_MONO1;assert(!portable_lora_native_begin());surface_format=RISC_DISPLAY_FORMAT_RGB565;
 /* Native selector alone cannot change the legacy paper contact stream. */
 portable_touch_sample paper={.valid=true,.began=true,.down=true,.contact_id=1};
 lora_native_observe(&paper);assert(lora_native_input_accept(&paper));
 assert(!portable_lora_native_ready(NULL,NULL)&&!snapshot_reads&&!event_reads);
 portable_lora_native_begin();portable_lora_native_layout(7);assert(!lora_native_visible);
 uint64_t generation,watermark;unsigned reads=event_reads,shots=snapshot_reads;
 assert(!portable_lora_native_ready(&generation,&watermark));assert(event_reads==reads&&snapshot_reads==shots&&generation);
 portable_touch_sample down={.valid=true,.began=true,.down=true,.contact_id=1};assert(!lora_native_input_accept(&down));
 portable_lora_native_present(false);assert(!portable_lora_native_ready(NULL,NULL));state.sequence=20;complete();settle_neutral();assert(portable_lora_native_ready(&generation,&watermark)&&watermark==20);
 touch.sequence=20;assert(!lora_native_input_accept(&down));fresh();
 portable_touch_sample moved={.valid=true,.down=true,.moved=true,.contact_id=1};
 assert(lora_native_input_accept(&moved));
 portable_touch_sample other={.valid=true,.released=true,.contact_id=2};
 assert(!lora_native_input_accept(&other));
 fresh();
 /* A visible unchanged map accepts fresh gestures throughout its repaint. */
 release();portable_lora_native_present(false);
 assert(portable_lora_native_ready(NULL,NULL));fresh();complete();release();fresh();
 /* A whole queued contact during same-map paint keeps its original watermark. */
 release();assert(portable_lora_native_ready(NULL,&watermark));
 portable_lora_native_present(false);state.sequence+=2;complete();
 uint64_t preserved;assert(portable_lora_native_ready(NULL,&preserved)&&preserved==watermark);
 touch.sequence=state.sequence-1;assert(lora_native_input_accept(&down));release();fresh();
 /* Held release survives same-layout telemetry and unchanged control identity. */
 portable_lora_native_layout(7);portable_lora_native_present(false);assert(lora_native_owned());complete();assert(lora_native_owned());release();
 fresh();portable_lora_native_present(false);uint64_t old_gen=lora_native_token_generation,old_layout=lora_native_token_layout;
 portable_lora_native_layout(8);complete();assert(!lora_native_visible&&!lora_native_owned());
 portable_touch_sample up={.valid=true,.released=true,.contact_id=1};assert(!lora_native_input_accept(&up));
 portable_lora_native_present(false);state.sequence=30;complete();settle_neutral();assert(portable_lora_native_ready(NULL,&watermark)&&watermark==30);
 /* A Quick-reserved top-edge contact replays only its original generation
  * and physical origin. A release cannot invent an unseen DOWN. */
 portable_touch_sample qdown={.valid=true,.began=true,.down=true,.tap_eligible=true,.contact_id=1,.x=12,.y=14};
 portable_touch_sample qup={.valid=true,.released=true,.tap_eligible=true,.contact_id=1,.x=13,.y=14};
 state.sequence++;touch.sequence=state.sequence;lora_native_observe(&qdown);
 state.sequence++;touch.sequence=state.sequence;lora_native_observe(&qup);qup.began=true;
 assert(lora_native_input_accept(&qup));uint16_t ox=0,oy=0;assert(portable_lora_native_tap_origin(&ox,&oy)&&ox==12&&oy==14);
 state.sequence++;touch.sequence=state.sequence;lora_native_observe(&qdown);
 portable_lora_native_layout(81);portable_lora_native_present(false);complete();settle_neutral();
 qup.began=false;state.sequence++;touch.sequence=state.sequence;lora_native_observe(&qup);qup.began=true;
 assert(!lora_native_input_accept(&qup)&&!portable_lora_native_tap_origin(NULL,NULL));
 portable_lora_native_layout(8);portable_lora_native_present(false);complete();settle_neutral();
 /* Reset during an outstanding frame keeps that frame's identity intact. */
 portable_lora_native_present(false);uint64_t retained_token=paper_token;
 uint64_t retained_generation=lora_native_token_generation;
 portable_lora_native_begin();
 assert(paper_token==retained_token&&lora_native_token_generation==retained_generation);
 complete();assert(!lora_native_visible);
 /* A delayed old owner/generation callback cannot establish visibility. */
 portable_lora_native_begin();lora_native_token_generation=old_gen;lora_native_token_layout=old_layout;lora_native_presented();assert(!lora_native_visible);
 portable_lora_native_layout(9);portable_lora_native_present(false);complete();fresh();
 portable_lora_native_present(false);state.width=0;complete();assert(!lora_native_visible&&touch.resync);state.width=410;
 portable_lora_native_layout(10);portable_lora_native_present(false);complete();fresh();
 portable_lora_native_present(false);snapshot_ok=false;complete();assert(!lora_native_visible&&touch.terminal);snapshot_ok=true;
 /* Readiness never samples input or opens/releases provider custody. */
 reads=event_reads;shots=snapshot_reads;
 (void)portable_lora_native_ready(&generation,&watermark);
 assert(event_reads==reads&&snapshot_reads==shots);
 lora_native_generation=UINT64_MAX;portable_lora_native_begin();assert(failed&&!portable_lora_native_ready(NULL,NULL));
 puts("LoRa owner11: paper bypass, visibility/neutral readiness, same-map repaint input, read-only watermark, drag ownership, held release, layout cancellation, pending-frame reset, stale completion, snapshot faults and generation exhaustion PASS");
}
