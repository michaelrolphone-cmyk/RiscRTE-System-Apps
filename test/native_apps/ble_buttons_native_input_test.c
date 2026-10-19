/* The production owner5 reducer with explicit display/provider state doubles. */
#define PORTABLE_BLE_BUTTONS_NATIVE_UI
#include "PortableTouch.h"
#include <assert.h>
#include <stdio.h>
static bool failed,input_pending;
static portable_touch touch;
static portable_touch_sample input_sample;
static uint64_t paper_token;
static unsigned snapshot_reads,event_reads;
static risc_touch_snapshot_v1 state;
static bool snapshot_ok=true;
static bool snapshot(void*c,risc_touch_snapshot_v1*s){(void)c;snapshot_reads++;if(!snapshot_ok)return false;*s=state;return true;}
static int32_t next(void*c,uint64_t token,risc_touch_event_v1*e){(void)c;(void)token;(void)e;event_reads++;return 0;}
static const risc_touch_api_v1 api={.api_version=1,.struct_size=sizeof(api),.snapshot=snapshot,.next=next};
static void input_reset(void){input_pending=false;input_sample=(portable_touch_sample){0};if(!failed)portable_touch_reset(&touch);}
static void portable_native_present(unsigned owner,bool full);
#include "ble_buttons_native_input.inc"
static void portable_native_present(unsigned owner,bool full){(void)full;assert(owner==5);paper_token++;ble_buttons_native_token_generation=ble_buttons_native_submit_generation;ble_buttons_native_token_layout=ble_buttons_native_submit_layout;}
static void complete(void){assert(paper_token);paper_token=0;ble_buttons_native_presented();}
static void fresh(void){state.sequence++;touch.sequence=state.sequence;portable_touch_sample s={.valid=true,.began=true,.down=true,.contact_id=1};assert(ble_buttons_native_input_accept(&s));}
static void release(void){portable_touch_sample s={.valid=true,.released=true,.contact_id=1};assert(ble_buttons_native_input_accept(&s));}
int main(void){
 state=(risc_touch_snapshot_v1){.width=410,.height=502,.sequence=10};touch=(portable_touch){.api=&api,.subscription=1};
 portable_ble_buttons_native_begin();portable_ble_buttons_native_layout(7);assert(!ble_buttons_native_visible);
 uint64_t generation,watermark;unsigned reads=event_reads,shots=snapshot_reads;
 assert(!portable_ble_buttons_native_ready(&generation,&watermark));assert(event_reads==reads&&snapshot_reads==shots&&generation);
 portable_touch_sample down={.valid=true,.began=true,.down=true,.contact_id=1};assert(!ble_buttons_native_input_accept(&down));
 portable_ble_buttons_native_present(false);assert(!portable_ble_buttons_native_ready(NULL,NULL));state.sequence=20;complete();assert(portable_ble_buttons_native_ready(&generation,&watermark)&&watermark==20);
 touch.sequence=20;assert(!ble_buttons_native_input_accept(&down));fresh();
 /* Held release survives same-layout telemetry and unchanged control identity. */
 portable_ble_buttons_native_layout(7);portable_ble_buttons_native_present(false);assert(ble_buttons_native_owned());complete();assert(ble_buttons_native_owned());release();
 fresh();portable_ble_buttons_native_present(false);uint64_t old_gen=ble_buttons_native_token_generation,old_layout=ble_buttons_native_token_layout;
 portable_ble_buttons_native_layout(8);complete();assert(!ble_buttons_native_visible&&!ble_buttons_native_owned());
 portable_touch_sample up={.valid=true,.released=true,.contact_id=1};assert(!ble_buttons_native_input_accept(&up));
 portable_ble_buttons_native_present(false);state.sequence=30;complete();assert(portable_ble_buttons_native_ready(NULL,&watermark)&&watermark==30);
 /* A delayed old owner/generation callback cannot establish visibility. */
 portable_ble_buttons_native_begin();ble_buttons_native_token_generation=old_gen;ble_buttons_native_token_layout=old_layout;ble_buttons_native_presented();assert(!ble_buttons_native_visible);
 portable_ble_buttons_native_layout(9);portable_ble_buttons_native_present(false);complete();fresh();
 portable_ble_buttons_native_present(false);state.width=0;complete();assert(!ble_buttons_native_visible&&touch.resync);state.width=410;
 portable_ble_buttons_native_layout(10);portable_ble_buttons_native_present(false);complete();fresh();
 portable_ble_buttons_native_present(false);snapshot_ok=false;complete();assert(!ble_buttons_native_visible&&touch.terminal);snapshot_ok=true;
 ble_buttons_native_generation=UINT64_MAX;portable_ble_buttons_native_begin();assert(failed&&!portable_ble_buttons_native_ready(NULL,NULL));
 puts("BLE Buttons owner5: visibility, read-only watermark, held release, layout cancellation, stale completion, snapshot faults and generation exhaustion PASS");
}
