/* The production owner6 reducer with explicit display/provider state doubles. */
#define PORTABLE_BLE_SCANNER_NATIVE_UI
#define PORTABLE_QUICK_ACTIONS
#include "PortableTouch.h"
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
#include "ble_scanner_native_input.inc"
static void portable_native_present(unsigned owner,bool full){(void)full;assert(owner==6);native_token_owner=owner;paper_token++;ble_scanner_native_token_generation=ble_scanner_native_submit_generation;ble_scanner_native_token_layout=ble_scanner_native_submit_layout;}
static void complete(void){assert(paper_token);paper_token=0;native_token_owner=0;ble_scanner_native_presented();}
static void settle_neutral(void){
 assert(!portable_ble_scanner_native_ready(NULL,NULL));
 touch.cancel_pending=false;quick.ui.neutral_gate=true;
 assert(!portable_ble_scanner_native_ready(NULL,NULL));
 quick.ui.neutral_gate=false;
}
static void fresh(void){touch.cancel_pending=false;quick.ui.neutral_gate=false;state.sequence++;touch.sequence=state.sequence;portable_touch_sample s={.valid=true,.began=true,.down=true,.contact_id=1};assert(ble_scanner_native_input_accept(&s));}
static void release(void){portable_touch_sample s={.valid=true,.released=true,.contact_id=1};assert(ble_scanner_native_input_accept(&s));}
int main(void){
 state=(risc_touch_snapshot_v1){.width=410,.height=502,.sequence=10};touch=(portable_touch){.api=&api,.subscription=1};
 /* Native selector alone cannot change the legacy paper contact stream. */
 portable_touch_sample paper={.valid=true,.began=true,.down=true,.contact_id=1};
 assert(ble_scanner_native_input_accept(&paper));
 assert(!portable_ble_scanner_native_ready(NULL,NULL)&&!snapshot_reads&&!event_reads);
 portable_ble_scanner_native_begin();portable_ble_scanner_native_layout(7);assert(!ble_scanner_native_visible);
 uint64_t generation,watermark;unsigned reads=event_reads,shots=snapshot_reads;
 assert(!portable_ble_scanner_native_ready(&generation,&watermark));assert(event_reads==reads&&snapshot_reads==shots&&generation);
 portable_touch_sample down={.valid=true,.began=true,.down=true,.contact_id=1};assert(!ble_scanner_native_input_accept(&down));
 portable_ble_scanner_native_present(false);assert(!portable_ble_scanner_native_ready(NULL,NULL));state.sequence=20;complete();settle_neutral();assert(portable_ble_scanner_native_ready(&generation,&watermark)&&watermark==20);
 touch.sequence=20;assert(!ble_scanner_native_input_accept(&down));fresh();
 portable_touch_sample moved={.valid=true,.down=true,.moved=true,.contact_id=1};
 assert(ble_scanner_native_input_accept(&moved));
 portable_touch_sample other={.valid=true,.released=true,.contact_id=2};
 assert(!ble_scanner_native_input_accept(&other));
 fresh();
 /* A visible unchanged map accepts fresh gestures throughout its repaint. */
 release();portable_ble_scanner_native_present(false);
 assert(portable_ble_scanner_native_ready(NULL,NULL));fresh();complete();release();fresh();
 /* A whole queued contact during same-map paint keeps its original watermark. */
 release();assert(portable_ble_scanner_native_ready(NULL,&watermark));
 portable_ble_scanner_native_present(false);state.sequence+=2;complete();
 uint64_t preserved;assert(portable_ble_scanner_native_ready(NULL,&preserved)&&preserved==watermark);
 touch.sequence=state.sequence-1;assert(ble_scanner_native_input_accept(&down));release();fresh();
 /* Held release survives same-layout telemetry and unchanged control identity. */
 portable_ble_scanner_native_layout(7);portable_ble_scanner_native_present(false);assert(ble_scanner_native_owned());complete();assert(ble_scanner_native_owned());release();
 fresh();portable_ble_scanner_native_present(false);uint64_t old_gen=ble_scanner_native_token_generation,old_layout=ble_scanner_native_token_layout;
 portable_ble_scanner_native_layout(8);complete();assert(!ble_scanner_native_visible&&!ble_scanner_native_owned());
 portable_touch_sample up={.valid=true,.released=true,.contact_id=1};assert(!ble_scanner_native_input_accept(&up));
 portable_ble_scanner_native_present(false);state.sequence=30;complete();settle_neutral();assert(portable_ble_scanner_native_ready(NULL,&watermark)&&watermark==30);
 /* Reset during an outstanding frame keeps that frame's identity intact. */
 portable_ble_scanner_native_present(false);uint64_t retained_token=paper_token;
 uint64_t retained_generation=ble_scanner_native_token_generation;
 portable_ble_scanner_native_begin();
 assert(paper_token==retained_token&&ble_scanner_native_token_generation==retained_generation);
 complete();assert(!ble_scanner_native_visible);
 /* A delayed old owner/generation callback cannot establish visibility. */
 portable_ble_scanner_native_begin();ble_scanner_native_token_generation=old_gen;ble_scanner_native_token_layout=old_layout;ble_scanner_native_presented();assert(!ble_scanner_native_visible);
 portable_ble_scanner_native_layout(9);portable_ble_scanner_native_present(false);complete();fresh();
 portable_ble_scanner_native_present(false);state.width=0;complete();assert(!ble_scanner_native_visible&&touch.resync);state.width=410;
 portable_ble_scanner_native_layout(10);portable_ble_scanner_native_present(false);complete();fresh();
 portable_ble_scanner_native_present(false);snapshot_ok=false;complete();assert(!ble_scanner_native_visible&&touch.terminal);snapshot_ok=true;
 /* Readiness never samples input or opens/releases provider custody. */
 reads=event_reads;shots=snapshot_reads;
 (void)portable_ble_scanner_native_ready(&generation,&watermark);
 assert(event_reads==reads&&snapshot_reads==shots);
 ble_scanner_native_generation=UINT64_MAX;portable_ble_scanner_native_begin();assert(failed&&!portable_ble_scanner_native_ready(NULL,NULL));
 puts("BLE Scanner owner6: paper bypass, visibility/neutral readiness, same-map repaint input, read-only watermark, drag ownership, held release, layout cancellation, pending-frame reset, stale completion, snapshot faults and generation exhaustion PASS");
}
