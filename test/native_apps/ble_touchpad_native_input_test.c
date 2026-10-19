/* Production owner7 reducer; provider queue remains the sole event queue. */
#define PORTABLE_BLE_TOUCHPAD_NATIVE_UI
#include "PortableTouch.h"
#include "RiscDisplayOutputV1.h"
#include <assert.h>
#include <stdio.h>
static bool failed,input_pending;
static portable_touch touch;
static portable_touch_sample input_sample;
static uint32_t surface_format=RISC_DISPLAY_FORMAT_RGB565;
static uint64_t paper_token;
static unsigned snapshot_reads,event_reads,present_owner;
static risc_touch_snapshot_v1 state;
static risc_touch_event_v1 events[32];
static unsigned head,count;
static bool snapshot_ok=true;
static bool snapshot(void*c,risc_touch_snapshot_v1*s){(void)c;snapshot_reads++;if(!snapshot_ok)return false;*s=state;return true;}
static int32_t next(void*c,uint64_t token,risc_touch_event_v1*e){(void)c;assert(token==1);event_reads++;if(!count)return 0;*e=events[head++];--count;return 1;}
static const risc_touch_api_v1 api={.api_version=1,.struct_size=sizeof(api),.snapshot=snapshot,.next=next};
static void input_reset(void){input_pending=false;input_sample=(portable_touch_sample){0};if(!failed)portable_touch_reset(&touch);}
static void portable_native_present(unsigned owner,bool full);
#include "ble_touchpad_native_input.inc"
static void portable_native_present(unsigned owner,bool full){(void)full;present_owner=owner;paper_token++;ble_touchpad_native_token_generation=ble_touchpad_native_submit_generation;ble_touchpad_native_token_layout=ble_touchpad_native_submit_layout;}
static void complete(void){assert(paper_token);paper_token=0;if(present_owner==7)ble_touchpad_native_presented();}
static portable_touch_sample take(void){portable_touch_sample s;assert(portable_touch_next(&touch,&s));return s;}
static void cancel(void){portable_touch_sample s=take();assert(s.cancelled);assert(!ble_touchpad_native_input_accept(&s));}
static void edge(unsigned kind,uint8_t id){
 assert(head+count<32);events[head+count++]=(risc_touch_event_v1){.sequence=++state.sequence,.timestamp_ms=++state.timestamp_ms,.kind=kind,.id=id,.x=120,.y=120};
 if(kind==RISC_TOUCH_EVENT_DOWN){assert(state.contact_count<RISC_TOUCH_MAX_CONTACTS);state.contacts[state.contact_count++]=(risc_touch_contact_v1){.id=id,.x=120,.y=120};}
 if(kind==RISC_TOUCH_EVENT_UP){unsigned at=0;while(at<state.contact_count&&state.contacts[at].id!=id)at++;assert(at<state.contact_count);state.contacts[at]=state.contacts[--state.contact_count];}
}
static void tap(void){edge(RISC_TOUCH_EVENT_DOWN,1);edge(RISC_TOUCH_EVENT_UP,1);}
static void accepted_tap(void){portable_touch_sample s=take();assert(s.began&&ble_touchpad_native_input_accept(&s));s=take();assert(s.released&&ble_touchpad_native_input_accept(&s));}
static void reset(void){
 failed=false;snapshot_ok=true;input_pending=false;head=count=0;paper_token=0;
 state=(risc_touch_snapshot_v1){.width=410,.height=502,.sequence=10};
 touch=(portable_touch){.api=&api,.subscription=1};surface_format=RISC_DISPLAY_FORMAT_RGB565;
 portable_ble_touchpad_native_begin();portable_ble_touchpad_native_layout(7);
}
static void visible(void){portable_ble_touchpad_native_present(false);complete();assert(portable_ble_touchpad_native_ready(NULL,NULL));cancel();}
static void visibility_and_repaint(void){
 reset();uint64_t generation,watermark;unsigned reads=event_reads,shots=snapshot_reads;
 assert(!portable_ble_touchpad_native_ready(&generation,&watermark)&&generation);
 assert(event_reads==reads&&snapshot_reads==shots);
 portable_ble_touchpad_native_present(false);assert(!portable_ble_touchpad_native_ready(NULL,NULL));state.sequence=20;complete();
 assert(portable_ble_touchpad_native_ready(&generation,&watermark)&&watermark==20);cancel();
 portable_ble_touchpad_native_layout(7);portable_ble_touchpad_native_present(false);
 assert(paper_token&&portable_ble_touchpad_native_ready(NULL,NULL));
 /* Fresh contact begins and releases while the unchanged frame is pending. */
 tap();accepted_tap();
 /* A complete queued contact must survive completion without watermark skip. */
 tap();unsigned queued=count;reads=event_reads;complete();
 assert(count==queued&&event_reads==reads&&portable_ble_touchpad_native_ready(NULL,&watermark)&&watermark==20);accepted_tap();
 /* A reducer-delivered sample awaiting the app also survives completion. */
 edge(RISC_TOUCH_EVENT_DOWN,2);input_sample=take();input_pending=true;
 portable_ble_touchpad_native_present(false);complete();assert(input_pending&&input_sample.began);
 assert(ble_touchpad_native_input_accept(&input_sample));input_pending=false;
 portable_ble_touchpad_native_present(false);complete();assert(ble_touchpad_native_owned());
 edge(RISC_TOUCH_EVENT_UP,2);portable_touch_sample s=take();assert(s.released&&ble_touchpad_native_input_accept(&s));
 shots=snapshot_reads;reads=event_reads;assert(portable_ble_touchpad_native_ready(NULL,NULL));assert(shots==snapshot_reads&&reads==event_reads);
}
static void layout_and_reset_fences(void){
 reset();visible();edge(RISC_TOUCH_EVENT_DOWN,1);portable_touch_sample s=take();assert(ble_touchpad_native_input_accept(&s));
 portable_ble_touchpad_native_present(false);uint64_t old_generation=ble_touchpad_native_token_generation,old_layout=ble_touchpad_native_token_layout;
 portable_ble_touchpad_native_layout(8);assert(!ble_touchpad_native_owned()&&!portable_ble_touchpad_native_ready(NULL,NULL));complete();assert(!ble_touchpad_native_visible);cancel();
 edge(RISC_TOUCH_EVENT_UP,1);s=take();assert(!s.released&&!ble_touchpad_native_input_accept(&s));
 portable_ble_touchpad_native_present(false);complete();cancel();tap();accepted_tap();
 portable_ble_touchpad_native_present(false);uint64_t token=paper_token;portable_ble_touchpad_native_begin();
 assert(paper_token==token);portable_ble_touchpad_native_layout(9);complete();assert(!ble_touchpad_native_visible);
 ble_touchpad_native_token_generation=old_generation;ble_touchpad_native_token_layout=old_layout;ble_touchpad_native_presented();assert(!ble_touchpad_native_visible);
 portable_ble_touchpad_native_present(false);complete();cancel();tap();accepted_tap();
 /* Held through first visibility, including multiple contacts, never taps. */
 portable_ble_touchpad_native_layout(10);edge(RISC_TOUCH_EVENT_DOWN,1);edge(RISC_TOUCH_EVENT_DOWN,2);
 portable_ble_touchpad_native_present(false);complete();cancel();
 while(count){s=take();assert(s.cancelled&&!ble_touchpad_native_input_accept(&s));}
 edge(RISC_TOUCH_EVENT_UP,1);s=take();assert(!ble_touchpad_native_input_accept(&s));
 edge(RISC_TOUCH_EVENT_UP,2);s=take();assert(!ble_touchpad_native_input_accept(&s));tap();accepted_tap();
 /* A foreign owner invalidates the mapping and delayed same-layout frame. */
 portable_ble_touchpad_native_present(false);ble_touchpad_native_frame(5);complete();assert(!portable_ble_touchpad_native_ready(NULL,NULL));
 portable_ble_touchpad_native_present(false);complete();cancel();tap();accepted_tap();
}
static void faults(void){
 reset();visible();portable_ble_touchpad_native_present(false);state.width=0;complete();assert(!ble_touchpad_native_visible&&touch.resync);
 reset();visible();portable_ble_touchpad_native_present(false);state.sequence=touch.sequence-1;complete();assert(!ble_touchpad_native_visible&&touch.resync);
 reset();visible();portable_ble_touchpad_native_present(false);snapshot_ok=false;complete();assert(!ble_touchpad_native_visible&&touch.terminal);
 reset();visible();touch.poll_failed=true;assert(!portable_ble_touchpad_native_ready(NULL,NULL));touch.poll_failed=false;
 ble_touchpad_native_generation=UINT64_MAX;portable_ble_touchpad_native_begin();assert(failed&&!portable_ble_touchpad_native_ready(NULL,NULL));
}
static void paper_parity(void){
 failed=false;surface_format=RISC_DISPLAY_FORMAT_MONO1;unsigned shots=snapshot_reads,reads=event_reads;
 uint64_t generation=ble_touchpad_native_generation;portable_touch prior=touch;
 input_pending=true;portable_ble_touchpad_native_begin();portable_ble_touchpad_native_layout(55);
 assert(input_pending&&!memcmp(&touch,&prior,sizeof(touch))&&ble_touchpad_native_generation==generation);
 portable_touch_sample s={0};assert(ble_touchpad_native_input_accept(&s));
 portable_ble_touchpad_native_present(false);assert(present_owner==0&&!portable_ble_touchpad_native_ready(NULL,NULL));
 assert(shots==snapshot_reads&&reads==event_reads);
}
int main(void){visibility_and_repaint();layout_and_reset_fences();faults();paper_parity();puts("Touchpad owner7: repaint-ready, queued/pending contacts, held neutral fences, layout/reset custody, stale completion, foreign owner, read-only watermark, faults, exhaustion and paper parity PASS");}
