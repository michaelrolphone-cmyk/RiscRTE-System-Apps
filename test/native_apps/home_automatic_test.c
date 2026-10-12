/* Actual Home automatic binding and existing policy encoders; deterministic
 * copied-operation Core double. Hardware and SD are not emulated as verified. */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define PORTABLE_CONTEXTS_CLIENT
#define PORTABLE_LOW_BATTERY
#define PORTABLE_X4_IDLE_POLICY
#define PORTABLE_HOME_AUTOMATIC_PREFERENCES
static bool native_custody_retained,failed;
static unsigned preset_mask=255;
static bool alert_allowed=true;
#define PORTABLE_CONTEXTS_ACTION_MASK() preset_mask
#define PORTABLE_CONTEXTS_ALERT_ALLOWED(mode) alert_allowed
static void portable_adapter_retain(void);
#define PORTABLE_CONTEXTS_CUSTODY_SAFE() (!native_custody_retained)
#define PORTABLE_CONTEXTS_CLEANUP_FAILURE() portable_adapter_retain()
#include "PortableContextsClient.h"
#include "PortablePreferenceAutomatic.h"
#include "PortableContextActions.h"
#ifdef PORTABLE_SETTINGS_AVAILABILITY
#include "PortableQuickActions.h"
static struct {bool retained;pqa_state ui;} quick;
#else
static struct {bool retained;} quick;
#endif
static portable_low_battery low_battery;
static bool home_pending,handoff_requested;
static unsigned navigation_pending;
#define T5_APP_BUTTON_BACK (1u << 0)
static const risc_runtime_api_v1 *rt;
static bool contexts_before_storage(void){return !native_custody_retained;}
static bool contexts_before_storage_read(void){return !native_custody_retained;}
static void input_service(void);
static void input_dispatch(void){}
#ifdef PORTABLE_SETTINGS_AVAILABILITY
#include "../../lib/PortableApps/src/home_settings_availability.inc"
#endif
#include "../../lib/PortableApps/src/home_automatic.inc"
static const char *test;
static bool is(const char *name){return !strcmp(test,name);}
static unsigned calls,acquires,releases,legacy_writes,submits,collects,cancels,waits,claims,reports,retains;
static unsigned copied_boundary;
static char order[32][16];
static unsigned count;
static uint64_t revision=7,owned_token;
static bool cancellation;
static const char *fault_key;
static int fault_status;
static struct record {char key[16];uint8_t data[64];uint32_t size;} records[32];
static struct record pending;
static uint8_t copied_intent[176];
static void io(void){assert(!native_custody_retained);++calls;}
static void portable_adapter_retain(void){native_custody_retained=failed=true;++retains;}
static struct record *record(const char *key,bool create){
 for(unsigned i=0;i<32;++i)if(!strcmp(key,records[i].key))return &records[i];
 if(create)for(unsigned i=0;i<32;++i)if(!records[i].key[0]){strcpy(records[i].key,key);return &records[i];}
 return NULL;
}
static int32_t get(void *c,const char *key,void *out,uint32_t capacity,uint32_t *n){
 (void)c;io();*n=0;struct record *r=record(key,false);if(!r||!r->size)return RISC_KEY_VALUE_NOT_FOUND;
 *n=r->size;if(capacity<*n)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(out,r->data,*n);return 0;
}
static int32_t deny(void *c,const char *key,const void *value,uint32_t n){(void)c;(void)key;(void)value;(void)n;io();++legacy_writes;assert(false);return RISC_KEY_VALUE_INVALID;}
static risc_key_value_v1 kv={1,sizeof(kv),NULL,get,deny};
static void seed(const char *key,const void *bytes,uint32_t n){struct record *r=record(key,true);assert(r&&n<=64);r->size=n;memcpy(r->data,bytes,n);}
static int32_t state(void *c,risc_key_value_atomic_state_v1 *out){
 (void)c;io();*out=(risc_key_value_atomic_state_v1){sizeof(*out),3,1,16,64,4096,1024,0,revision};
 if(is("wrong-namespace"))out->namespace_id=6;return is("state-busy")?RISC_ATOMIC_BUSY:is("source-conflict")?RISC_ATOMIC_CONFLICT:0;
}
static int32_t atomic_get(void *c,uint64_t rev,const char *key,void *out,uint32_t cap,uint32_t *n){
 io();*n=0;
 if(is("stale-after-edge")&&count==1&&!strcmp(key,"sleep_idle"))++revision;
 if(is("edge-readback")&&count==1&&!strcmp(key,"battery_edge"))return RISC_ATOMIC_IO;
 if(rev!=revision)return RISC_ATOMIC_STALE;
 if(is("read-io")&&!strcmp(key,"battery_edge"))return RISC_ATOMIC_IO;
 int rc=get(c,key,out,cap,n);return rc==RISC_KEY_VALUE_BUFFER_SMALL?RISC_ATOMIC_BUFFER_SMALL:rc;
}
static uint32_t word(const uint8_t *p){return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static int32_t submit(void *c,uint64_t rev,const risc_key_value_atomic_mutation_v1 *m,uint32_t n,const void *intent,uint32_t bytes,uint64_t *token){
 (void)c;io();++submits;*token=0;assert(n==1&&bytes==176&&rev==home_automatic.revision);
 if((is("busy")&&submits<4)||is("cancel-busy"))return RISC_ATOMIC_BUSY;
 if(is("stale")&&!count)++revision;
 if(rev!=revision)return RISC_ATOMIC_STALE;
 assert(!owned_token&&count<32);strcpy(order[count++],m->key);
 memcpy(copied_intent,intent,176);assert(!memcmp(copied_intent,"PFA0001",8));
 assert(copied_intent[8]==PORTABLE_PREFERENCE_AUTOMATIC&&copied_intent[9]==RISC_ATOMIC_PUT);
 assert(copied_intent[11]==home_automatic.boundary);copied_boundary=copied_intent[11];
 assert(!strcmp((char*)copied_intent+12,m->key)&&word(copied_intent+32)==m->size);
 assert(word(copied_intent+36)==rev&&word(copied_intent+172)==portable_preference_crc(copied_intent,172));
 struct record *old=record(m->key,false);assert(copied_intent[10]==!!(old&&old->size));
 assert(word(copied_intent+28)==(old?old->size:0));if(old)assert(!memcmp(copied_intent+44,old->data,old->size));
 assert(!memcmp(copied_intent+108,m->value,m->size));
 if(is("submit-io"))return RISC_ATOMIC_IO;
 pending=(struct record){.size=m->size};strcpy(pending.key,m->key);memcpy(pending.data,m->value,m->size);
 *token=owned_token=100+submits;
 return is("dirty-submit")?RISC_ATOMIC_IO:RISC_ATOMIC_OK;
}
static int32_t collect(void *c,uint64_t token,risc_key_value_atomic_receipt_v1 *receipt){
 (void)c;io();++collects;assert(token&&token==owned_token);
 if(is("collect-io"))return RISC_ATOMIC_IO;
 if(collects%2)return RISC_ATOMIC_PENDING;
 int result=fault_key&&!strcmp(fault_key,pending.key)?fault_status:RISC_ATOMIC_OK;
 if(cancellation&&!is("cancel-race"))result=RISC_ATOMIC_CANCELLED;
 if(!result){seed(pending.key,pending.data,pending.size);++revision;}
 *receipt=(risc_key_value_atomic_receipt_v1){sizeof(*receipt),result,revision,!result?1000+submits:0};
 owned_token=0;if(is("bad-receipt"))receipt->operation_id=0;return 0;
}
static int32_t cancel(void *c,uint64_t token){(void)c;io();assert(token==owned_token);++cancels;cancellation=true;return is("cancel-error")?RISC_ATOMIC_IO:RISC_ATOMIC_OK;}
static const risc_key_value_atomic_v1 atomic={1,sizeof(atomic),NULL,state,atomic_get,submit,collect,cancel};
static bool acquire(const char *name,uint32_t version,uint64_t id,risc_runtime_capability_v1 *g){
 io();++acquires;assert(version==1&&id==1);
 if(!strcmp(name,RISC_KEY_VALUE_ATOMIC_CAPABILITY)){
  if(is("missing"))return false;
  *g=(risc_runtime_capability_v1){sizeof(*g),1,1,&atomic};return !is("dirty-acquire");
 }
 assert(!strcmp(name,RISC_KEY_VALUE_CAPABILITY));*g=(risc_runtime_capability_v1){sizeof(*g),2,1,&kv};return true;
}
static bool release(risc_runtime_capability_v1 *g){io();assert(g->api);if(is("release-failed"))return false;++releases;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static void yield(uint32_t ms){io();assert(ms==1);++waits;}
static void input_service(void){io();if(!strncmp(test,"cancel",6))navigation_pending=T5_APP_BUTTON_BACK;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release,.yield_ms=yield};
static unsigned observe(unsigned percent){risc_battery_sample_v1 sample={3700,(uint8_t)percent,0};return home_automatic_low_battery(&sample);}
static void no_replay(void){unsigned n=count,c=calls;assert(observe(8)==0&&count==n&&calls==c);}
static bool pause_service(void *c){(void)c;io();return true;}
static contexts_status_v1 room_status;
static bool status_service(void *c,contexts_status_v1 *out){(void)c;io();*out=room_status;return true;}
static bool claim(void *c,uint32_t source,uint32_t slot,const char *name,uint32_t generation){
 (void)c;io();assert(source==1&&slot==2&&!strcmp(name,"Study")&&generation==3);
 ++claims;room_status.audio.preset_entry=room_status.audio.room_entry;return true;
}
static uint32_t preset_result;
static bool report(void *c,uint32_t source,uint32_t generation,uint32_t result){(void)c;io();assert(source==1&&generation==3);++reports;preset_result=result;return true;}
static const contexts_service_v1 service={.api_version=1,.struct_size=sizeof(service),.pause=pause_service,.status=status_service,.claim_preset=claim,.preset_result=report};
static bool face(const risc_key_value_v1 *view,unsigned id){uint8_t bytes[]={'F',1,(uint8_t)id,(uint8_t)(id^0xa5)};return view->put(view->context,"watch_face",bytes,4)==0;}
static void room(void){
 portable_context_preset p;portable_context_preset_init(&p);p.enabled=true;p.source=1;p.slot=2;strcpy(p.name,"Study");
 p.actions=PORTABLE_CONTEXT_ACTIONS;p.volume=0;p.dnd=true;p.face=2;uint8_t bytes[64];assert(portable_context_preset_encode(&p,bytes));seed("ctx_p0",bytes,64);
 room_status.audio=(contexts_source_status_v1){.source=1,.model_generation=3,.room_slot=2,.room_valid=true,.current=true,.room_entry=1};strcpy(room_status.audio.room_name,"Study");
 portable_contexts_client c={.runtime=rt,.api=&service,.settings_valid=true,.policy={.enabled=true},.loaded_at=10};uint16_t applied=0;
 if(is("preset-partial")){fault_key="sleep_deep";fault_status=RISC_ATOMIC_IO;}
 if(is("preset-low"))c.low_battery=true;
 if(is("preset-mask"))preset_mask=PORTABLE_CONTEXT_IDLE|PORTABLE_CONTEXT_BRIGHTNESS|PORTABLE_CONTEXT_DND;
 if(is("preset-alert"))alert_allowed=false;
 assert(portable_contexts_apply_room_using(&c,&applied,face,home_automatic_preset));
 assert(claims==1&&reports==1&&copied_boundary==PORTABLE_AUTOMATIC_HOME_PRESET);
 if(is("preset-partial")){assert(count==3&&!strcmp(order[2],"sleep_deep")&&applied==(PORTABLE_CONTEXT_SLEEP|PORTABLE_CONTEXT_IDLE)&&preset_result==CONTEXTS_PRESET_PARTIAL);}
 else if(is("preset-low")){assert(count==5&&applied==240&&preset_result==CONTEXTS_PRESET_PARTIAL);}
 else if(is("preset-mask")){assert(count==3&&applied==preset_mask&&preset_result==CONTEXTS_PRESET_PARTIAL);}
 else if(is("preset-alert")){assert(count==8&&applied==191&&preset_result==CONTEXTS_PRESET_PARTIAL);}
 else {assert(count==9&&applied==255&&preset_result==CONTEXTS_PRESET_APPLIED);assert(!strcmp(order[4],"quick_volume")&&!strcmp(order[5],"alarm_volume"));}
 unsigned saved=count;c.loaded_at++;assert(portable_contexts_apply_room_using(&c,&applied,face,home_automatic_preset)&&count==saved&&claims==1);
}
int main(int argc,char **argv){
 assert(argc==2);test=argv[1];rt=&runtime;
 if(is("preset-no-callback")){
  portable_contexts_client c={0};uint16_t applied=0;
  assert(!portable_contexts_apply_room_using(&c,&applied,face,NULL)&&!calls);goto done;
 }
 if(!strncmp(test,"preset",6)){room();goto done;}
 if(is("editor-origin")){
  assert(home_automatic_begin(PORTABLE_AUTOMATIC_CONTEXTS_EDITOR));
  assert(portable_context_setting(&home_automatic.view,CR_SLEEP,30,false,face));
  assert(portable_context_setting(&home_automatic.view,CR_DND,1,false,face));
  assert(!portable_context_setting(&home_automatic.view,CR_POWER,0,false,face));
  assert(count==2&&copied_boundary==4);assert(home_automatic_close());goto done;
 }
 if(is("scope")){
  assert(home_automatic_begin(PORTABLE_AUTOMATIC_HOME_RULES));unsigned before=calls;uint8_t value=1;uint32_t n=0;
  assert(home_automatic.view.put(home_automatic.view.context,"time_format",&value,1)==RISC_KEY_VALUE_INVALID);
  assert(home_automatic.view.get(home_automatic.view.context,"contexts_on",&value,1,&n)==RISC_KEY_VALUE_INVALID);
  assert(portable_context_setting(&home_automatic.view,CR_BRIGHTNESS,CR_KEEP,true,face));
  assert(calls==before&&!count);assert(home_automatic_close());goto done;
 }
 if(is("same-controls")){
  uint8_t dnd=1,radio[]={0x51,1,1,0xa4};seed("alert_dnd",&dnd,1);seed("quick_radio",radio,4);
  assert(home_automatic_begin(PORTABLE_AUTOMATIC_HOME_RULES));assert(pqa_dnd_save(&home_automatic.view,true));assert(portable_radio_save(&home_automatic.view,1));assert(count==2&&revision==9);assert(home_automatic_close());goto done;
 }
 if(is("rearm-failed")){
  uint8_t edge[]={0x42,1,1,0xa4};seed("battery_edge",edge,4);fault_key="battery_edge";fault_status=RISC_ATOMIC_IO;
  assert(observe(10)==PORTABLE_LOW_BATTERY_ERROR&&count==1);unsigned n=calls;assert(observe(11)==0&&calls==n);
  assert(observe(9)==0&&count==1);goto done;
 }
 if(is("rules-order")||is("rules-partial")||is("rules-low")||is("rules-notify")){
  assert(home_automatic_begin(PORTABLE_AUTOMATIC_HOME_RULES));
  if(is("rules-partial")){fault_key="sleep_idle";fault_status=RISC_ATOMIC_IO;}
  if(is("rules-notify")){fault_key="alert_dnd";fault_status=RISC_ATOMIC_IO;assert(!portable_context_setting(&home_automatic.view,CR_NOTIFY,2,false,face));assert(count==1);}
  else {
   int32_t settings[CR_SETTINGS]={2,0,30,600,1,1,7,5,2,1};uint16_t succeeded=0;
   for(unsigned f=0;f<CR_SETTINGS;++f)if(portable_context_setting(&home_automatic.view,f,settings[f],is("rules-low"),face))succeeded|=1u<<f;
   if(is("rules-partial"))assert(succeeded==((1u<<CR_SETTINGS)-1u-(1u<<CR_SLEEP))&&count==11);
   else if(is("rules-low"))assert(count==7&&!(succeeded&(1u<<CR_SLEEP)));
   else assert(succeeded==1023&&count==11&&!strcmp(order[1],"sleep_mode")&&!strcmp(order[3],"sleep_deep")&&!strcmp(order[7],"alarm_volume"));
  }
  assert(home_automatic_close());goto done;
 }
 if(is("off")){uint8_t zero=0,restore=80;seed("brightness",&zero,1);seed("quick_bright",&restore,1);}
 if(is("same-timers")){
  uint8_t idle[]={0x54,1,20,0,20^0xa5},deep[]={0x54,1,60,0,60^0xa5},brightness=15;
  seed("sleep_idle",idle,5);seed("sleep_deep",deep,5);seed("brightness",&brightness,1);
 }
 if(is("partial")||is("uncertain")){fault_key="sleep_idle";fault_status=is("partial")?RISC_ATOMIC_IO:RISC_ATOMIC_UNCERTAIN;}
 if(is("edge-failed")||is("conflict")){fault_key="battery_edge";fault_status=is("conflict")?RISC_ATOMIC_CONFLICT:RISC_ATOMIC_IO;}
 if(is("invalid-sample")){assert(observe(255)==0&&!low_battery.observed&&!calls);goto done;}
 unsigned result=observe(9);
 if(is("dirty-submit")||is("collect-io")||is("bad-receipt")||is("release-failed")||is("dirty-acquire")||is("cancel-error")){
  assert(result&PORTABLE_LOW_BATTERY_RETAINED);assert(native_custody_retained&&retains);unsigned before=calls;
  assert(!home_automatic_close()&&calls==before);goto done;
 }
 if(is("source-conflict")||is("conflict")||is("missing")||is("wrong-namespace")||is("state-busy")||is("read-io")||is("stale")||is("submit-io")||is("edge-failed")||is("edge-readback")||is("cancel")||is("cancel-busy")){
  assert(result==PORTABLE_LOW_BATTERY_ERROR);no_replay();goto done;
 }
 if(is("partial")||is("uncertain")||is("stale-after-edge"))assert(result==(PORTABLE_LOW_BATTERY_ENTERED|PORTABLE_LOW_BATTERY_ERROR));
 else if(is("cancel-race"))assert(result==(PORTABLE_LOW_BATTERY_ENTERED|PORTABLE_LOW_BATTERY_ERROR)&&count==1);
 else assert(result==PORTABLE_LOW_BATTERY_ENTERED);
 if(is("stale-after-edge"))assert(count==1);
 else if(!is("cancel-race"))assert(count==(is("off")?4u:is("same-timers")?3u:5u));
 assert(!strcmp(order[0],"battery_edge")&&copied_boundary==PORTABLE_AUTOMATIC_LOW_BATTERY);no_replay();
 if(is("normal")){
  uint8_t manual=70;seed("brightness",&manual,1);++revision;low_battery=(portable_low_battery){0};assert(observe(8)==0&&count==5);
  assert(observe(10)==0&&count==6&&!strcmp(order[5],"battery_edge"));
  assert(observe(9)==PORTABLE_LOW_BATTERY_ENTERED&&count==9); /* Equal timers skip. */
 }
 if(is("busy"))assert(submits==8);
 done:assert(!legacy_writes);printf("Home automatic: %s passed (%u copied puts, %u provider calls)\n",test,count,calls);return 0;
}
