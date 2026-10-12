/* Production explicit Quick controller staging + session/radio controllers.
 * Deterministic provider doubles exercise copied transactions and custody,
 * never claim physical hardware or a product-selected Runtime. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "PortableQuickSession.h"
#include "PortableQuickRadios.h"
#include "PortableContextPreferences.h"
#include "PortablePreferenceAtomic.h"
#include "RiscDisplayOutputFrontlightV1.h"
static pqa_session quick;
static pqa_radios quick_radios;
static bool native_custody_retained,failed,quick_atomic_running,home_pending,handoff_requested;
static unsigned quick_atomic_contact,navigation_pending;
#define T5_APP_BUTTON_BACK 1u
static const risc_runtime_api_v1 *rt;
static unsigned retains,legacy_puts,atomic_acquires,atomic_releases,submits,collects,cancels,reads,hardware_calls;
static unsigned tick,brightness=40,tone=50,ble_state;
static bool cancel_requested,refuse_release,hidden;
static uint64_t revision=7,owned_token;
static const char *test;
static uint8_t records[6][64],pending_values[2][64],intent_copy[176];
static uint32_t sizes[6],pending_sizes[2],pending_count;
static int pending_indices[2];
static const char *const keys[]={"brightness","quick_bright","frontlight_tone","alert_dnd","contexts_on","quick_radio"};
static bool which(const char *name){return !strcmp(test,name);}
static void io(void){assert(!hidden);}
static void portable_adapter_retain(void){++retains;hidden=true;native_custody_retained=failed=true;}
static void input_service(void){io();if(which("cancel")||which("cancel-race")||which("cancel-error"))navigation_pending=T5_APP_BUTTON_BACK;}
static void input_dispatch(void){io();}
#include "../../lib/PortableApps/src/quick_atomic.inc"
static int index_of(const char *key){for(unsigned i=0;i<6;++i)if(!strcmp(keys[i],key))return (int)i;return -1;}
static int32_t get_record(void *c,const char *key,void *out,uint32_t cap,uint32_t *size){
 (void)c;io();int i=index_of(key);*size=i<0?0:sizes[i];if(!*size)return RISC_KEY_VALUE_NOT_FOUND;
 if(cap<*size)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(out,records[i],*size);return RISC_KEY_VALUE_OK;
}
static int32_t deny_put(void *c,const char *key,const void *data,uint32_t n){
 (void)c;(void)key;(void)data;(void)n;io();++legacy_puts;return RISC_KEY_VALUE_INVALID;
}
static const risc_key_value_v1 kv={1,sizeof(kv),NULL,get_record,deny_put};
static uint32_t word(const uint8_t *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int32_t atomic_state(void *c,risc_key_value_atomic_state_v1 *s){
 (void)c;io();*s=(risc_key_value_atomic_state_v1){sizeof(*s),3,1,16,64,4096,1024,0,revision};
 if(which("small-group"))s->max_mutations=1;
 if(which("wrong-namespace"))s->namespace_id=6;
 return RISC_ATOMIC_OK;
}
static int32_t atomic_get(void *c,uint64_t rev,const char *key,void *out,uint32_t cap,uint32_t *size){
 io();++reads;*size=0;if(which("read-io"))return RISC_ATOMIC_IO;
 if(rev!=revision)return RISC_ATOMIC_STALE;
 int rc=get_record(c,key,out,cap,size);return rc==RISC_KEY_VALUE_OK?RISC_ATOMIC_OK:rc==RISC_KEY_VALUE_NOT_FOUND?RISC_ATOMIC_NOT_FOUND:RISC_ATOMIC_BUFFER_SMALL;
}
static int32_t atomic_submit(void *c,uint64_t rev,const risc_key_value_atomic_mutation_v1 *m,uint32_t count,const void *intent,uint32_t n,uint64_t *token){
 (void)c;io();*token=0;++submits;
 if(which("busy")&&submits<4)return RISC_ATOMIC_BUSY;
 if(rev!=revision)return RISC_ATOMIC_STALE;
 if(which("submit-io"))return RISC_ATOMIC_IO;
 assert(!owned_token&&count>=1&&count<=2&&n==176);
 memcpy(intent_copy,intent,n);assert(!memcmp(intent_copy,"PFI0001",8));
 assert(intent_copy[8]==PORTABLE_PREFERENCE_USER&&intent_copy[9]==RISC_ATOMIC_PUT);
 assert(!strcmp((const char*)intent_copy+12,m[0].key));
 int primary=index_of(m[0].key);assert(primary>=0);
 assert(intent_copy[10]==!!sizes[primary]&&word(intent_copy+28)==sizes[primary]);
 assert(word(intent_copy+32)==m[0].size&&word(intent_copy+36)==revision);
 assert(!memcmp(intent_copy+44,records[primary],sizes[primary]));
 assert(!memcmp(intent_copy+108,m[0].value,m[0].size));
 assert(word(intent_copy+172)==portable_preference_crc(intent_copy,172));
 pending_count=count;
 for(unsigned i=0;i<count;++i){pending_indices[i]=index_of(m[i].key);assert(pending_indices[i]>=0&&m[i].kind==RISC_ATOMIC_PUT);pending_sizes[i]=m[i].size;memcpy(pending_values[i],m[i].value,m[i].size);}
 if(count==2)assert(primary==0&&pending_indices[1]==1&&m[0].size==1&&pending_values[0][0]==0&&m[1].size==1);
 *token=owned_token=81;tick=0;
 return which("dirty-submit")?RISC_ATOMIC_IO:RISC_ATOMIC_OK;
}
static int32_t atomic_collect(void *c,uint64_t token,risc_key_value_atomic_receipt_v1 *receipt){
 (void)c;io();assert(token==owned_token&&token);++collects;
 if(which("collect-io"))return RISC_ATOMIC_IO;
 if(which("close-pending"))return RISC_ATOMIC_PENDING;
 if(++tick<3)return RISC_ATOMIC_PENDING;
 int status=which("uncertain")?RISC_ATOMIC_UNCERTAIN:which("capacity")?RISC_ATOMIC_CAPACITY:
  which("conflict")?RISC_ATOMIC_CONFLICT:cancel_requested&&!which("cancel-race")?RISC_ATOMIC_CANCELLED:RISC_ATOMIC_OK;
 if(status==RISC_ATOMIC_OK){for(unsigned i=0;i<pending_count;++i){sizes[pending_indices[i]]=pending_sizes[i];memcpy(records[pending_indices[i]],pending_values[i],pending_sizes[i]);}++revision;}
 *receipt=(risc_key_value_atomic_receipt_v1){sizeof(*receipt),status,revision,status==RISC_ATOMIC_OK?123:0};owned_token=0;
 if(which("bad-receipt"))receipt->operation_id=0;
 return RISC_ATOMIC_OK;
}
static int32_t atomic_cancel(void *c,uint64_t token){(void)c;io();assert(token==owned_token);++cancels;cancel_requested=true;return which("cancel-error")?RISC_ATOMIC_IO:RISC_ATOMIC_PENDING;}
static risc_key_value_atomic_v1 atomic_api={1,sizeof(atomic_api),NULL,atomic_state,atomic_get,atomic_submit,atomic_collect,atomic_cancel};
static bool light(void *c,uint16_t value,uint16_t max){(void)c;io();assert(value<=100&&max==100);brightness=value;++hardware_calls;return true;}
static int32_t tone_get(void *c,uint16_t *v,uint16_t *max){(void)c;io();*v=tone;*max=100;return RISC_DISPLAY_TONE_OK;}
static bool tone_set(void *c,uint16_t v,uint16_t max){(void)c;io();assert(v<=100&&max==100);tone=v;++hardware_calls;return true;}
static risc_display_output_api_v1_frontlight display_api;
static bool ble_set(void *c,bool enabled){(void)c;io();ble_state=enabled;return true;}
static bool ble_get(void *c,uint8_t *v){(void)c;io();*v=ble_state;return true;}
static bool wifi_down(void *c){(void)c;io();return true;}
static wifi_link_t wifi_status(void *c){(void)c;io();return WIFI_LINK_DOWN;}
static const portable_bluetooth_control_v1 ble={.api_version=1,.struct_size=sizeof(ble),.set_enabled=ble_set,.status=ble_get};
static const wifi_api_v1 wifi={.api_version=1,.struct_size=sizeof(wifi),.status=wifi_status,.disconnect_checked=wifi_down};
static bool acquire(const char *name,uint32_t v,uint64_t ns,risc_runtime_capability_v1 *g){
 io();assert(v==1);g->slot=1;g->generation=1;
 if(!strcmp(name,RISC_KEY_VALUE_ATOMIC_CAPABILITY)){assert(ns==1);++atomic_acquires;if(which("missing")){g->slot=g->generation=0;return false;}g->api=&atomic_api;}
 else if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY))g->api=&kv;
 else if(!strcmp(name,"net.wifi"))g->api=&wifi;
 else if(!strcmp(name,"bluetooth.hci"))g->api=&ble;
 else assert(!"unexpected capability");return true;
}
static bool release(risc_runtime_capability_v1 *g){io();if(g->api==&atomic_api){++atomic_releases;if(refuse_release)return false;}memset(g,0,sizeof(*g));return true;}
static void yield_ms(uint32_t ms){(void)ms;io();if(which("copy")){quick.ui.action_brightness=13;quick.ui.action_tone=4;}}
static bool diagnostic(const char *s){(void)s;io();return true;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release,.yield_ms=yield_ms,.diagnostic=diagnostic};
static bool apply(uint32_t action){
 if(!quick_atomic_capture(action))return false;
 bool changed=false;
 bool ok=action&(PQA_WIFI|PQA_BLUETOOTH|PQA_AIRPLANE)?pqa_radios_apply_user(&quick_radios,&quick.ui,rt,action,quick_atomic_save,NULL,quick_atomic_guard):
  pqa_session_apply(&quick,rt,&display_api.snapshot.metrics.power.history.base,action,&changed);
 quick_atomic_show_queue_error();return ok;
}
static bool seed(void *context,risc_display_frame_v1 frame) {(void)context;(void)frame;assert(false);return false;}
static int32_t power(void *context,uint32_t timeout) {(void)context;(void)timeout;assert(false);return -1;}
static bool metrics(void *context,risc_display_present_metrics_v1 *out) {(void)context;(void)out;assert(false);return false;}
static bool copy(void *context,uint32_t format,void *pixels,size_t size,uint32_t stride) {
 (void)context;(void)format;(void)pixels;(void)size;(void)stride;assert(false);return false;
}
int main(int argc,char **argv){
 assert(argc==2);test=argv[1];rt=&runtime;pqa_session_init(&quick);quick.ui.paper=true;quick.ui.brightness_valid=quick.ui.tone_valid=quick.ui.dnd_valid=true;
 quick.ui.contexts_controls=quick.ui.contexts_valid=true;quick.save_user=quick_atomic_save;quick_radios=(pqa_radios){PORTABLE_RADIO_WIFI,true,true};
 display_api.snapshot.metrics.power.history.base=(risc_display_output_api_v1){.api_version=1,.struct_size=sizeof(display_api),.set_brightness=light};
 display_api.snapshot.metrics.power.history.extension_tag=RISC_DISPLAY_HISTORY_TAG;
 display_api.snapshot.metrics.power.history.extension_version=1;
 display_api.snapshot.metrics.power.history.seed_previous=seed;
 display_api.snapshot.metrics.power.power_tag=RISC_DISPLAY_POWER_TAG;
 display_api.snapshot.metrics.power.power_version=1;
 display_api.snapshot.metrics.power.prepare=power;display_api.snapshot.metrics.power.resume=power;
 display_api.snapshot.metrics.metrics_tag=RISC_DISPLAY_METRICS_TAG;
 display_api.snapshot.metrics.metrics_version=RISC_DISPLAY_METRICS_VERSION;
 display_api.snapshot.metrics.snapshot=metrics;
 display_api.snapshot.snapshot_tag=RISC_DISPLAY_SNAPSHOT_TAG;
 display_api.snapshot.snapshot_version=RISC_DISPLAY_SNAPSHOT_VERSION;
 display_api.snapshot.copy_completed=copy;
 display_api.frontlight_tag=RISC_DISPLAY_FRONTLIGHT_TAG;display_api.frontlight_version=1;display_api.get_tone=tone_get;display_api.set_tone=tone_set;
 if(which("off-already")||which("restore-stale")) {
  sizes[0]=sizes[1]=1;records[0][0]=0;records[1][0]=60;
  quick.brightness=0;quick.restore_brightness=60;brightness=0;
 }
 ++quick_atomic_contact;
 if(which("origins")){
  for(unsigned origin=2;origin<=4;++origin){portable_preference_atomic s={0};assert(portable_preference_atomic_begin(&s,&atomic_api,"brightness",origin,quick_atomic_guard,NULL)==RISC_ATOMIC_INVALID);}
  portable_preference_atomic s={0};assert(portable_preference_atomic_begin(&s,&atomic_api,"quick_bright",1,quick_atomic_guard,NULL)==RISC_ATOMIC_INVALID);
  assert(!submits&&!reads);puts("quick origins PASS");return 0;
 }
 if(which("automatic")){
  assert(!pqa_preference_save(&kv,PQA_BRIGHTNESS_KEY,70,10));assert(!pqa_dnd_save(&kv,true));assert(!portable_radio_save(&kv,3));
  assert(legacy_puts==3&&!submits&&!atomic_acquires);puts("quick automatic setters not USER PASS");return 0;
 }
 if(which("restore")){
  quick.ui.torch=true;assert(apply(PQA_TORCH)&&brightness==100);quick.ui.torch=false;assert(apply(PQA_TORCH)&&brightness==40);
  assert(pqa_session_restore(&quick,&display_api.snapshot.metrics.power.history.base));assert(!atomic_acquires&&!submits&&!legacy_puts);puts("quick torch restore PASS");return 0;
 }
 assert(quick_atomic_modal_begin());
 if(which("queued-controls")||which("queue-overflow")) {
  quick.ui.position_q8=quick.ui.target_q8=PQA_OPEN_Q8;quick.ui.audio_controls=false;quick.ui.tone_controls=true;
  quick.ui.radio_controls=quick.ui.radios_valid=true;quick.ui.wifi_enabled=true;
  pqa_set_levels(&quick.ui,true,40,false,50);
  assert(pqa_input(&quick.ui,1,true,0,0,0,0,true));
  unsigned now=2;
  if(which("queue-overflow")) {
   for(unsigned i=0;i<QUICK_ATOMIC_QUEUE_CAPACITY+4;++i){++quick_atomic_contact;quick.ui.pending=PQA_DND;quick.ui.action_dnd=!(i%2);quick_atomic_enqueue();}
   assert(quick_atomic_queue_count==QUICK_ATOMIC_QUEUE_CAPACITY&&quick_atomic_queue_overflow);
  } else {
   const unsigned controls[]={1,3,4,9,1};
   for(unsigned i=0;i<sizeof(controls)/sizeof(*controls);++i) {
    int x,y;assert(pqa_paper_tile(&quick.ui,controls[i],&x,&y));++quick_atomic_contact;
    assert(pqa_input(&quick.ui,now++,true,1,quick_atomic_contact,x+80,y+40,true));quick_atomic_enqueue();
    assert(pqa_input(&quick.ui,now++,true,0,0,0,0,true));quick_atomic_enqueue();
   }
   ++quick_atomic_contact;int y=pqa_paper_slider_y(&quick.ui,0)+15;
   assert(pqa_input(&quick.ui,now++,true,1,quick_atomic_contact,364,y,true));quick_atomic_enqueue();
   assert(pqa_input(&quick.ui,now++,true,0,0,0,0,true));quick_atomic_enqueue();
   assert(quick_atomic_queue_count==7);quick.ui.action_brightness=13;quick.ui.action_dnd=true;
  }
  while(quick_atomic_queue_count){uint32_t action=quick_atomic_take_action();assert(action);assert(apply(action));}
  if(which("queued-controls")){assert(submits==6&&records[0][0]==100&&records[3][0]==0&&records[4][2]==1&&records[5][2]==2);}
  else assert(submits==QUICK_ATOMIC_QUEUE_CAPACITY&&(quick.ui.error_flags&PQA_ERROR_SAVE));
  assert(!legacy_puts&&quick_atomic_close());printf("quick %s ordered copied reducer actions PASS\n",test);return 0;
 }
 if(!strncmp(test,"raw-",4)) {
  bool tone_edit=which("raw-tone");
  quick.ui.position_q8=quick.ui.target_q8=PQA_OPEN_Q8;quick.ui.audio_controls=false;quick.ui.tone_controls=true;
  pqa_set_levels(&quick.ui,true,40,false,50);quick.ui.tone=50;
  assert(pqa_input(&quick.ui,1,true,0,0,0,0,true));
  int y=pqa_paper_slider_y(&quick.ui,tone_edit?2:0)+15;
  assert(pqa_input(&quick.ui,2,true,1,7,364,y,true));
  if(!which("raw-coalesced")){assert(apply(pqa_take_action(&quick.ui)));assert(!submits);}
  if(which("raw-stale")||which("raw-coalesced"))++revision;
  if(which("raw-cancel")){pqa_close(&quick.ui);assert(apply(pqa_take_action(&quick.ui)));assert(!submits);}
  else {assert(pqa_input(&quick.ui,3,true,0,0,0,0,true));assert(apply(pqa_take_action(&quick.ui)));}
  bool saved=which("raw-brightness")||tone_edit;
  assert(submits==(unsigned)(saved||which("raw-stale")));assert(sizes[tone_edit?2:0]==(unsigned)saved);
  if(saved)assert(records[tone_edit?2:0][0]==100);
  assert(quick_atomic_close()&&!legacy_puts);printf("quick %s actual input PASS\n",test);return 0;
 }
 if(which("repeated")) {
  for(unsigned i=0;i<3;++i){++quick_atomic_contact;quick.ui.action_brightness=60+i*10;assert(apply(PQA_BRIGHTNESS_PREVIEW));assert(apply(PQA_BRIGHTNESS_COMMIT));assert(records[0][0]==60+i*10);}
  assert(submits==3&&revision==10&&quick_atomic_revision==10);assert(quick_atomic_close());puts("quick repeated independent gestures PASS");return 0;
 }
 if(which("tone")){quick.ui.action_tone=80;assert(apply(PQA_TONE_PREVIEW));assert(!submits&&tone==80);assert(apply(PQA_TONE_COMMIT));assert(sizes[2]==1&&records[2][0]==80&&quick.tone==80);}
 else if(which("dnd")||which("same-value")){quick.ui.action_dnd=!which("same-value");assert(apply(PQA_DND));assert(sizes[3]==1&&records[3][0]==quick.ui.action_dnd);}
 else if(which("contexts")){quick.ui.action_contexts=true;assert(apply(PQA_CONTEXTS));assert(sizes[4]==4&&records[4][2]==1&&quick.ui.contexts_enabled);}
 else if(which("radio")||which("radio-cancel")){quick_radios.flags=3;sizes[5]=4;memcpy(records[5],(uint8_t[]){0x51,1,3,0xa6},4);if(which("radio-cancel"))navigation_pending=1;assert(apply(PQA_AIRPLANE));assert(sizes[5]==4&&records[5][2]==28&&quick_radios.flags==28&&!ble_state);}
 else {
  quick.ui.action_brightness=70;
  if(which("display-stale")){sizes[0]=1;records[0][0]=90;}
  bool preview=apply(PQA_BRIGHTNESS_PREVIEW);assert(preview||hidden);
  assert(!submits&&!legacy_puts);
  if(which("preview")){assert(quick_atomic_close());assert(!submits&&brightness==70);puts("quick preview PASS");return 0;}
  if(which("stale"))++revision;
  if(which("off")||which("off-already")||which("restore-stale")||which("small-group"))quick.ui.action_brightness=0;
  if(which("restore-stale"))quick.restore_brightness=50;
  if(which("release-failed"))refuse_release=true;
  if(which("close-pending")){
   uint8_t b=70;assert(portable_preference_atomic_prepare(&quick_atomic_edit,&b,1)==PORTABLE_PREFERENCE_READY);
   assert(portable_preference_atomic_submit(&quick_atomic_edit)==RISC_ATOMIC_PENDING);
   assert(!quick_atomic_close()&&hidden&&quick_atomic_edit.token&&atomic_releases==0);puts("quick close pending retains PASS");return 0;
  }
  bool ok=apply(PQA_BRIGHTNESS_COMMIT);
  if(which("release-failed"))ok=quick_atomic_close();
  if(which("dirty-submit")||which("collect-io")||which("bad-receipt")||which("cancel-error")||which("release-failed")){
   assert(!ok&&hidden&&retains==1&&quick.retained);assert(quick_atomic_edit.token||which("release-failed"));puts("quick terminal custody PASS");return 0;
  }
  assert(ok&&!hidden);
  bool saved=which("brightness")||which("copy")||which("busy")||which("off")||which("off-already")||which("cancel-race");
  if(saved){assert(sizes[0]==1&&records[0][0]==(which("off")||which("off-already")?0:70));assert(quick.brightness==records[0][0]);}
  else if(which("restore-stale")){assert(quick.brightness==0&&brightness==0&&records[1][0]==60&&!submits);}
  else {assert(sizes[0]==(unsigned)which("display-stale"));assert(quick.brightness==40&&brightness==40);}
  if(which("off"))assert(pending_count==2&&sizes[1]==1&&records[1][0]==40);
  if(which("busy"))assert(submits==4);
  if(which("cancel")||which("cancel-race"))assert(cancels==1);
 }
 assert(!legacy_puts&&!owned_token&&!quick_atomic_owned());assert(quick_atomic_close());
 printf("quick %s PASS: submits=%u collects=%u cancels=%u legacy-puts=%u\n",test,submits,collects,cancels,legacy_puts);return 0;
}
