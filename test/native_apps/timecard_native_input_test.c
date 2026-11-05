/* The production owner14 reducer with explicit display/provider state doubles. */
#define PORTABLE_TIMECARD_NATIVE_UI
#define PORTABLE_QUICK_ACTIONS
#include "PortableTouch.h"
#include "RiscDisplayOutputV1.h"
#include <assert.h>
#include <stdio.h>
static bool failed,input_pending;
typedef struct {bool neutral_gate,visible,capture;} quick_state;
static struct {quick_state ui;} quick;
static bool quick_modal;
static bool pqa_visible(const quick_state *s){return s->visible;}
static bool pqa_capture(const quick_state *s){return s->capture;}
static uint32_t surface_format=RISC_DISPLAY_FORMAT_RGB565;
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
#include "timecard_native_input.inc"
static void portable_native_present(unsigned owner,bool full){(void)full;assert(owner==14);native_token_owner=owner;paper_token++;timecard_native_token_generation=timecard_native_submit_generation;timecard_native_token_layout=timecard_native_submit_layout;}
static void complete(void){assert(paper_token);paper_token=0;native_token_owner=0;timecard_native_presented();}
static void settle_neutral(void){
 assert(!portable_timecard_native_ready(NULL,NULL));
 touch.cancel_pending=false;quick.ui.neutral_gate=true;
 assert(!portable_timecard_native_ready(NULL,NULL));
 quick.ui.neutral_gate=false;
}
static void fresh(void){touch.cancel_pending=false;quick.ui.neutral_gate=false;state.sequence++;touch.sequence=state.sequence;portable_touch_sample s={.valid=true,.began=true,.down=true,.contact_id=1};assert(timecard_native_input_accept(&s));}
static void release(void){portable_touch_sample s={.valid=true,.released=true,.contact_id=1};assert(timecard_native_input_accept(&s));}
int main(void){
 /* Runtime format selection and begin are inert on paper. */
 surface_format=RISC_DISPLAY_FORMAT_MONO1;
 assert(!portable_timecard_native_available());portable_timecard_native_begin();
 portable_timecard_native_layout(1);assert(!timecard_native_enabled&&!timecard_native_layout_known);
 assert(!snapshot_reads&&!event_reads);
 surface_format=RISC_DISPLAY_FORMAT_RGB565;assert(portable_timecard_native_available());
 state=(risc_touch_snapshot_v1){.width=410,.height=502,.sequence=10};touch=(portable_touch){.api=&api,.subscription=1};
 /* Native selector alone cannot change the legacy paper contact stream. */
 portable_touch_sample paper={.valid=true,.began=true,.down=true,.contact_id=1};
 assert(timecard_native_input_accept(&paper));
 assert(!portable_timecard_native_ready(NULL,NULL)&&!snapshot_reads&&!event_reads);
 portable_timecard_native_begin();portable_timecard_native_layout(7);assert(!timecard_native_visible);
 uint64_t generation,watermark;unsigned reads=event_reads,shots=snapshot_reads;
 assert(!portable_timecard_native_ready(&generation,&watermark));assert(event_reads==reads&&snapshot_reads==shots&&generation);
 portable_touch_sample down={.valid=true,.began=true,.down=true,.tap_eligible=true,.contact_id=1};assert(!timecard_native_input_accept(&down));
 touch.sequence=11;timecard_native_quick_observe(&down);
 assert(!timecard_native_quick_eligible&&timecard_native_quick_sequence==11);
 portable_timecard_native_present(false);assert(!portable_timecard_native_ready(NULL,NULL));state.sequence=20;complete();settle_neutral();assert(portable_timecard_native_ready(&generation,&watermark)&&watermark==20);
 /* The release is newer than visible pixels; its original reserved DOWN was
  * not. A synthesized began cannot promote that contact into a fresh one. */
 portable_touch_sample replay={.valid=true,.began=true,.released=true,.tap_eligible=true,.contact_id=1};
 touch.sequence=21;assert(touch.sequence>watermark&&!timecard_native_quick_replay(&replay));
 touch.sequence=20;assert(!timecard_native_input_accept(&down));fresh();
 portable_touch_sample moved={.valid=true,.down=true,.moved=true,.contact_id=1};
 assert(timecard_native_input_accept(&moved));
 portable_touch_sample other={.valid=true,.released=true,.contact_id=2};
 assert(!timecard_native_input_accept(&other));
 fresh();
 /* A visible unchanged map accepts fresh gestures throughout its repaint. */
 release();portable_timecard_native_present(false);
 assert(portable_timecard_native_ready(NULL,NULL));fresh();complete();release();fresh();
 /* A whole queued contact during same-map paint keeps its original watermark. */
 release();assert(portable_timecard_native_ready(NULL,&watermark));
 portable_timecard_native_present(false);state.sequence+=2;complete();
 uint64_t preserved;assert(portable_timecard_native_ready(NULL,&preserved)&&preserved==watermark);
 touch.sequence=state.sequence-1;assert(timecard_native_input_accept(&down));release();fresh();
 /* Held release survives same-layout telemetry and unchanged control identity. */
 portable_timecard_native_layout(7);portable_timecard_native_present(false);assert(timecard_native_owned());complete();assert(timecard_native_owned());release();
 fresh();portable_timecard_native_present(false);uint64_t old_gen=timecard_native_token_generation,old_layout=timecard_native_token_layout;
 portable_timecard_native_layout(8);complete();assert(!timecard_native_visible&&!timecard_native_owned());
 portable_touch_sample up={.valid=true,.released=true,.contact_id=1};assert(!timecard_native_input_accept(&up));
 portable_timecard_native_present(false);state.sequence=30;complete();settle_neutral();assert(portable_timecard_native_ready(NULL,&watermark)&&watermark==30);
 /* Every other owner fences a held contact, including a native completion
  * still outstanding at entry. Reserved owners do not acquire this mapping. */
 for(unsigned owner=0;owner<14;owner++) {
  fresh();portable_timecard_native_present(false);
  uint64_t before=timecard_native_generation;
  timecard_native_frame(owner);assert(timecard_native_generation>before);
  assert(!timecard_native_owned()&&!portable_timecard_native_ready(NULL,NULL));
  complete();assert(!timecard_native_visible);
  portable_timecard_native_layout(8);portable_timecard_native_present(false);
  complete();touch.cancel_pending=false;assert(portable_timecard_native_ready(NULL,NULL));
 }
 /* Quick owns its contacts even before its overlay image has submitted. */
 quick_modal=true;assert(!portable_timecard_native_ready(NULL,NULL));quick_modal=false;
 quick.ui.visible=true;assert(!portable_timecard_native_ready(NULL,NULL));quick.ui.visible=false;
 fresh();quick.ui.capture=true;assert(!timecard_native_input_accept(&moved));
 quick.ui.capture=false;assert(!timecard_native_owned()&&!timecard_native_input_accept(&up));
 /* Eligibility follows the original generation and sequence, never release. */
 state.sequence++;touch.sequence=state.sequence;timecard_native_quick_observe(&down);
 assert(timecard_native_quick_eligible);
 portable_timecard_native_layout(80);portable_timecard_native_present(false);complete();
 touch.cancel_pending=false;touch.sequence=state.sequence+1;
 assert(portable_timecard_native_ready(NULL,NULL)&&!timecard_native_quick_replay(&replay));
 state.sequence+=2;touch.sequence=state.sequence;timecard_native_quick_observe(&down);
 assert(timecard_native_quick_eligible);
 timecard_native_visible_sequence=timecard_native_quick_sequence;
 touch.sequence++;assert(!timecard_native_quick_replay(&replay));
 /* Reset during an outstanding frame keeps that frame's identity intact. */
 portable_timecard_native_present(false);uint64_t retained_token=paper_token;
 uint64_t retained_generation=timecard_native_token_generation;
 portable_timecard_native_begin();
 assert(paper_token==retained_token&&timecard_native_token_generation==retained_generation);
 complete();assert(!timecard_native_visible);
 /* A delayed old owner/generation callback cannot establish visibility. */
 portable_timecard_native_begin();timecard_native_token_generation=old_gen;timecard_native_token_layout=old_layout;timecard_native_presented();assert(!timecard_native_visible);
 portable_timecard_native_layout(9);portable_timecard_native_present(false);complete();fresh();
 portable_timecard_native_present(false);state.width=0;complete();assert(!timecard_native_visible&&touch.resync);state.width=410;
 portable_timecard_native_layout(10);portable_timecard_native_present(false);complete();fresh();
 portable_timecard_native_present(false);snapshot_ok=false;complete();assert(!timecard_native_visible&&touch.terminal);snapshot_ok=true;
 /* Cancellation prevents a later matching UP from recovering ownership. */
 touch.terminal=false;touch.resync=false;touch.cancel_pending=false;
 portable_timecard_native_begin();portable_timecard_native_layout(11);
 portable_timecard_native_present(false);complete();fresh();
 portable_touch_sample cancelled={.valid=true,.cancelled=true,.contact_id=1};
 assert(!timecard_native_input_accept(&cancelled));assert(!timecard_native_input_accept(&up));
 /* A valid neutral sample is exposed for picker settling without a release. */
 portable_touch_sample neutral={.valid=true};assert(timecard_native_input_accept(&neutral));
 /* Readiness never samples input or opens/releases provider custody. */
 reads=event_reads;shots=snapshot_reads;
 (void)portable_timecard_native_ready(&generation,&watermark);
 assert(event_reads==reads&&snapshot_reads==shots);
 timecard_native_generation=UINT64_MAX;portable_timecard_native_begin();assert(failed&&!portable_timecard_native_ready(NULL,NULL));
 puts("Timecard owner14: runtime paper bypass, visibility/neutral readiness, same-map repaint input, read-only watermark, drag ownership, held release, layout cancellation, foreign owners0..13, Quick capture, pending-frame reset, stale completion, snapshot faults and generation exhaustion PASS");
}
