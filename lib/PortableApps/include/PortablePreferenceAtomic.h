#ifndef PORTABLE_PREFERENCE_ATOMIC_H
#define PORTABLE_PREFERENCE_ATOMIC_H
/* Explicit controller-owned namespace-1 edit. No provider calls from generic
 * legacy put helpers and no automatic retry/rebase. All captured and submitted
 * bytes are bounded copies; collect owns the accepted token until terminal. */
#include <stdbool.h>
#include <string.h>
#include "RiscKeyValueAtomicV1.h"
#define PORTABLE_PREFERENCE_ATOMIC_BYTES 64u
#define PORTABLE_PREFERENCE_INTENT_BYTES 176u
#define PORTABLE_PREFERENCE_NAMESPACE 1u
enum { PORTABLE_PREFERENCE_USER=1,PORTABLE_PREFERENCE_AUTOMATIC=2,
       PORTABLE_PREFERENCE_ROLLBACK=3,PORTABLE_PREFERENCE_BOOT=4 };
enum { PORTABLE_PREFERENCE_READY=10 };
typedef struct portable_preference_atomic {
 const risc_key_value_atomic_v1 *api;
 bool (*guard)(void *);void *guard_context;
 uint64_t revision,token;
 bool present,cancelled,submitted,valid;
 bool brightness_off,volume_off;uint8_t restore_brightness,restore_volume;
 uint32_t expected_size,desired_size;
 char key[16];uint8_t expected[64],desired[64],intent[PORTABLE_PREFERENCE_INTENT_BYTES];
 int status;risc_key_value_atomic_receipt_v1 receipt;
} portable_preference_atomic;
static inline bool portable_preference_atomic_api_valid(const risc_key_value_atomic_v1 *api) {
 return api&&api->api_version==1&&api->struct_size>=sizeof(*api)&&api->state&&api->get&&api->submit&&api->collect&&api->cancel;
}
static inline bool portable_preference_atomic_state_valid(const risc_key_value_atomic_state_v1 *s) {
 return s->struct_size>=sizeof(*s)&&s->namespace_id==PORTABLE_PREFERENCE_NAMESPACE&&s->revision&&
  s->max_mutations>=1&&s->max_value_bytes>=64&&s->max_group_bytes>=512&&
  s->max_intent_bytes>=PORTABLE_PREFERENCE_INTENT_BYTES&&
  (s->flags&3u)==(RISC_KEY_VALUE_ATOMIC_NATIVE_DURABLE|RISC_KEY_VALUE_ATOMIC_COPIED_ASYNC);
}
static inline bool portable_preference_atomic_key(const char *key) {
 static const char *const allowed[]={"watch_face","time_format","time_zone","desk_clock_face","desk_direction",
  "reader_flip_ui","reader_language","sleep_idle","sleep_deep","sleep_mode","alert_mode","rtc_basis",
  "brightness","alarm_volume","frontlight_tone","alert_dnd","contexts_on","quick_radio",
  "ble_broadcast","context_sources","context_timing"};
 if(!key)return false;
 /* Explicit BLE alias/tombstone identities only; exactly one address type
  * digit and twelve canonical lowercase address nibbles. */
 if(strlen(key)==15&&key[0]=='b'&&key[1]=='s'&&key[2]>='0'&&key[2]<='3'){
  for(unsigned i=3;i<15;i++)if(!((key[i]>='0'&&key[i]<='9')||(key[i]>='a'&&key[i]<='f')))return false;
  return true;
 }
 for(unsigned i=0;i<sizeof(allowed)/sizeof(*allowed);++i)if(!strcmp(key,allowed[i]))return true;
 return false;
}
static inline int portable_preference_atomic_safe(portable_preference_atomic *s) {
 return s&&s->guard&&s->guard(s->guard_context)?RISC_ATOMIC_OK:RISC_ATOMIC_RETAINED;
}
static inline bool portable_preference_atomic_terminal(int rc) {
 return rc==RISC_ATOMIC_OK||rc==RISC_ATOMIC_CANCELLED||rc==RISC_ATOMIC_INVALID||
  rc==RISC_ATOMIC_CAPACITY||rc==RISC_ATOMIC_IO||rc==RISC_ATOMIC_UNCERTAIN||
  rc==RISC_ATOMIC_CONFLICT||rc==RISC_ATOMIC_STALE;
}
/* Called before loading the editor draft, never at Save time. A non-user
 * origin cannot enter this selected controller route. Future automatic writers
 * need their own policy and boundary; these enum values confer no authority. */
static inline int portable_preference_atomic_begin_at(portable_preference_atomic *s,
 const risc_key_value_atomic_v1 *api,const char *key,unsigned origin,
 bool (*guard)(void *),void *context,uint64_t captured_revision) {
 if(!s||s->token)return RISC_ATOMIC_RETAINED;
 memset(s,0,sizeof(*s));s->api=api;s->guard=guard;s->guard_context=context;
 if(origin!=PORTABLE_PREFERENCE_USER||!portable_preference_atomic_api_valid(api)||
    !portable_preference_atomic_key(key))return s->status=RISC_ATOMIC_INVALID;
 if(portable_preference_atomic_safe(s))return s->status=RISC_ATOMIC_RETAINED;
 risc_key_value_atomic_state_v1 state={.struct_size=sizeof(state)};
 int rc=api->state(api->context,&state);
 if(portable_preference_atomic_safe(s))return s->status=RISC_ATOMIC_RETAINED;
 if(rc)return s->status=rc;
 if(!portable_preference_atomic_state_valid(&state))return s->status=RISC_ATOMIC_INVALID;
 if(captured_revision&&state.revision!=captured_revision)return s->status=RISC_ATOMIC_STALE;
 s->revision=state.revision;memcpy(s->key,key,strlen(key)+1);
 rc=api->get(api->context,s->revision,key,s->expected,sizeof(s->expected),&s->expected_size);
 if(portable_preference_atomic_safe(s))return s->status=RISC_ATOMIC_RETAINED;
 if(rc!=RISC_ATOMIC_OK&&rc!=RISC_ATOMIC_NOT_FOUND)return s->status=rc;
 if((rc==RISC_ATOMIC_OK&&s->expected_size>sizeof(s->expected))||
    (rc==RISC_ATOMIC_NOT_FOUND&&s->expected_size))return s->status=RISC_ATOMIC_INVALID;
 s->present=rc==RISC_ATOMIC_OK;s->valid=true;return s->status=PORTABLE_PREFERENCE_READY;
}
static inline int portable_preference_atomic_begin(portable_preference_atomic *s,
 const risc_key_value_atomic_v1 *api,const char *key,unsigned origin,
 bool (*guard)(void *),void *context) {
 return portable_preference_atomic_begin_at(s,api,key,origin,guard,context,0);
}
static inline void portable_preference_word(uint8_t *p,uint32_t v) {
 for(unsigned i=0;i<4;++i)p[i]=(uint8_t)(v>>(i*8u));
}
static inline uint32_t portable_preference_crc(const uint8_t *p,unsigned size) {
 uint32_t crc=UINT32_MAX;
 for(unsigned i=0;i<size;++i){crc^=p[i];for(unsigned b=0;b<8;++b)crc=(crc>>1)^((0u-(crc&1u))&UINT32_C(0xedb88320));}
 return ~crc;
}
/* A controller passes a COMPLETE canonical value, even for a same-value Save.
 * PFI0001: origin USER; operation PUT; exact expected presence/value; desired
 * value; key and captured local revision. Zero padding and LE words are fixed. */
static inline int portable_preference_atomic_prepare(portable_preference_atomic *s,const void *value,uint32_t size) {
 if(!s||!s->valid||s->submitted||s->token||s->status!=PORTABLE_PREFERENCE_READY||!value||!size||size>64)return RISC_ATOMIC_INVALID;
 s->brightness_off=false;s->volume_off=false;memcpy(s->desired,value,size);s->desired_size=size;
 memset(s->intent,0,sizeof(s->intent));memcpy(s->intent,"PFI0001",7);
 s->intent[8]=PORTABLE_PREFERENCE_USER;s->intent[9]=RISC_ATOMIC_PUT;s->intent[10]=(uint8_t)s->present;
 memcpy(s->intent+12,s->key,strlen(s->key));portable_preference_word(s->intent+28,s->expected_size);
 portable_preference_word(s->intent+32,size);portable_preference_word(s->intent+36,(uint32_t)s->revision);
 portable_preference_word(s->intent+40,(uint32_t)(s->revision>>32));
 memcpy(s->intent+44,s->expected,s->expected_size);memcpy(s->intent+108,value,size);
 portable_preference_word(s->intent+172,portable_preference_crc(s->intent,172));return PORTABLE_PREFERENCE_READY;
}
/* Brightness OFF has one semantic intention and one subordinate restore
 * byte. The copied group publishes both or neither; quick_bright cannot be
 * selected as an independent USER intention. PFI0001 remains unchanged. */
static inline int portable_preference_atomic_prepare_brightness_off(portable_preference_atomic *s,
 const void *value,uint32_t size,uint8_t restore) {
 if(!s||!s->valid||s->submitted||s->token||s->status!=PORTABLE_PREFERENCE_READY||strcmp(s->key,"brightness")||!value||size!=1||*(const uint8_t*)value||restore<10||restore>100)return RISC_ATOMIC_INVALID;
 if(portable_preference_atomic_safe(s))return RISC_ATOMIC_RETAINED;
 risc_key_value_atomic_state_v1 state={.struct_size=sizeof(state)};
 int rc=s->api->state(s->api->context,&state);
 if(portable_preference_atomic_safe(s))return RISC_ATOMIC_RETAINED;
 if(rc)return rc;
 if(!portable_preference_atomic_state_valid(&state)||state.max_mutations<2)return RISC_ATOMIC_CAPACITY;
 if(state.revision!=s->revision)return RISC_ATOMIC_STALE;
 if(s->present&&(s->expected_size!=1||s->expected[0]>100||(s->expected[0]&&s->expected[0]<10)))return RISC_ATOMIC_INVALID;
 uint8_t prior=s->present?s->expected[0]:40;
 if(!prior) {
  uint32_t n=0;uint8_t stored=0;
  rc=s->api->get(s->api->context,s->revision,"quick_bright",&stored,1,&n);
  if(portable_preference_atomic_safe(s))return RISC_ATOMIC_RETAINED;
  if(rc==RISC_ATOMIC_NOT_FOUND&&!n)prior=40;
  else if(rc==RISC_ATOMIC_OK&&n==1&&stored>=10&&stored<=100)prior=stored;
  else return rc==RISC_ATOMIC_OK||rc==RISC_ATOMIC_NOT_FOUND?RISC_ATOMIC_INVALID:rc;
 }
 if(restore!=prior)return RISC_ATOMIC_STALE;
 rc=portable_preference_atomic_prepare(s,value,size);
 if(rc!=PORTABLE_PREFERENCE_READY)return rc;
 s->brightness_off=true;s->restore_brightness=restore;return rc;
}
/* Muting audio likewise groups its prior audible level with the USER Save.
 * quick_volume is never an independent intention. */
static inline int portable_preference_atomic_prepare_volume_off(portable_preference_atomic *s,
 const void *value,uint32_t size,uint8_t restore) {
 if(!s||!s->valid||s->submitted||s->token||s->status!=PORTABLE_PREFERENCE_READY||strcmp(s->key,"alarm_volume")||!value||size!=1||*(const uint8_t*)value||restore<1||restore>100)return RISC_ATOMIC_INVALID;
 if(portable_preference_atomic_safe(s))return RISC_ATOMIC_RETAINED;
 risc_key_value_atomic_state_v1 state={.struct_size=sizeof(state)};
 int rc=s->api->state(s->api->context,&state);
 if(portable_preference_atomic_safe(s))return RISC_ATOMIC_RETAINED;
 if(rc)return rc;
 if(!portable_preference_atomic_state_valid(&state)||state.max_mutations<2)return RISC_ATOMIC_CAPACITY;
 if(state.revision!=s->revision)return RISC_ATOMIC_STALE;
 if(s->present&&(s->expected_size!=1||s->expected[0]>100||(s->expected[0]&&s->expected[0]<1)))return RISC_ATOMIC_INVALID;
 uint8_t prior=s->present?s->expected[0]:50;
 if(!prior) {
  uint32_t n=0;uint8_t stored=0;
  rc=s->api->get(s->api->context,s->revision,"quick_volume",&stored,1,&n);
  if(portable_preference_atomic_safe(s))return RISC_ATOMIC_RETAINED;
  if(rc==RISC_ATOMIC_NOT_FOUND&&!n)prior=50;
  else if(rc==RISC_ATOMIC_OK&&n==1&&stored>=1&&stored<=100)prior=stored;
  else return rc==RISC_ATOMIC_OK||rc==RISC_ATOMIC_NOT_FOUND?RISC_ATOMIC_INVALID:rc;
 }
 if(restore!=prior)return RISC_ATOMIC_STALE;
 rc=portable_preference_atomic_prepare(s,value,size);
 if(rc!=PORTABLE_PREFERENCE_READY)return rc;
 s->volume_off=true;s->restore_volume=restore;return rc;
}
/* Preflight is useful before irreversible physical clock writes. It detects
 * known stale drafts; it cannot lock other writers out while hardware runs. */
static inline int portable_preference_atomic_check(portable_preference_atomic *s) {
 if(!s||!s->valid||s->submitted||s->token)return RISC_ATOMIC_INVALID;
 if(portable_preference_atomic_safe(s))return RISC_ATOMIC_RETAINED;
 risc_key_value_atomic_state_v1 state={.struct_size=sizeof(state)};int rc=s->api->state(s->api->context,&state);
 if(portable_preference_atomic_safe(s))return RISC_ATOMIC_RETAINED;
 if(rc)return rc;
 if(!portable_preference_atomic_state_valid(&state))return RISC_ATOMIC_INVALID;
 return state.revision==s->revision?RISC_ATOMIC_OK:RISC_ATOMIC_STALE;
}
static inline int portable_preference_atomic_submit(portable_preference_atomic *s) {
 if(!s||!s->valid||s->submitted||s->token||s->status!=PORTABLE_PREFERENCE_READY||!s->desired_size)return RISC_ATOMIC_INVALID;
 if(portable_preference_atomic_safe(s))return s->status=RISC_ATOMIC_RETAINED;
 risc_key_value_atomic_mutation_v1 mutations[2]={{s->key,RISC_ATOMIC_PUT,s->desired,s->desired_size},
  {s->volume_off?"quick_volume":"quick_bright",RISC_ATOMIC_PUT,s->volume_off?&s->restore_volume:&s->restore_brightness,1}};
 uint64_t token=0;int rc=s->api->submit(s->api->context,s->revision,mutations,(s->brightness_off||s->volume_off)?2:1,s->intent,sizeof(s->intent),&token);
 /* Keep ANY returned token, even a contract violation. Never release custody
  * or lose an accepted/ambiguous operation by resetting the app staging. */
 s->token=token;
 if(portable_preference_atomic_safe(s))return s->status=RISC_ATOMIC_RETAINED;
 if(rc==RISC_ATOMIC_BUSY&&!token)return s->status=PORTABLE_PREFERENCE_READY;
 s->submitted=true;
 if(rc!=RISC_ATOMIC_OK||!token)return s->status=token||rc==RISC_ATOMIC_OK?RISC_ATOMIC_RETAINED:
  portable_preference_atomic_terminal(rc)?rc:RISC_ATOMIC_RETAINED;
 return s->status=RISC_ATOMIC_PENDING;
}
static inline int portable_preference_atomic_collect(portable_preference_atomic *s) {
 if(!s||!s->token)return RISC_ATOMIC_INVALID;
 if(portable_preference_atomic_safe(s))return s->status=RISC_ATOMIC_RETAINED;
 risc_key_value_atomic_receipt_v1 receipt={.struct_size=sizeof(receipt)};
 int rc=s->api->collect(s->api->context,s->token,&receipt);
 if(portable_preference_atomic_safe(s))return s->status=RISC_ATOMIC_RETAINED;
 if(rc==RISC_ATOMIC_PENDING||rc==RISC_ATOMIC_BUSY)return s->status=RISC_ATOMIC_PENDING;
 if(rc!=RISC_ATOMIC_OK||receipt.struct_size<sizeof(receipt)||!portable_preference_atomic_terminal(receipt.status)||
    (receipt.status==RISC_ATOMIC_OK&&(!receipt.operation_id||receipt.revision<=s->revision))||
    (receipt.status!=RISC_ATOMIC_OK&&receipt.operation_id))return s->status=RISC_ATOMIC_RETAINED;
 s->receipt=receipt;s->token=0;return s->status=receipt.status;
}
static inline int portable_preference_atomic_cancel(portable_preference_atomic *s) {
 if(!s)return RISC_ATOMIC_INVALID;
 if(!s->token){s->submitted=true;return s->status=RISC_ATOMIC_CANCELLED;}
 if(portable_preference_atomic_safe(s))return s->status=RISC_ATOMIC_RETAINED;
 if(!s->cancelled){
  int rc=s->api->cancel(s->api->context,s->token);
  if(portable_preference_atomic_safe(s))return s->status=RISC_ATOMIC_RETAINED;
  if(rc!=RISC_ATOMIC_OK&&rc!=RISC_ATOMIC_PENDING&&rc!=RISC_ATOMIC_CANCELLED)return s->status=RISC_ATOMIC_RETAINED;
  s->cancelled=true;
 }
 return portable_preference_atomic_collect(s);
}
#endif
