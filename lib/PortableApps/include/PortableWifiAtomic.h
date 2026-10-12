#ifndef PORTABLE_WIFI_ATOMIC_H
#define PORTABLE_WIFI_ATOMIC_H
/* App-owned local operation staging. This is not the SD rebase Stage.
 * Numeric slots live only in a revision-checked local scan; the persisted
 * WFI0001 intent contains SSID, desired value and expected record fingerprint.
 * No source put is called. One copied native submission publishes all keys
 * and intent. The caller must collect every accepted submission before exit.
 */
#include "PortableWifiProfiles.h"
#include "RiscKeyValueAtomicV1.h"
#ifndef NDEBUG
#define PORTABLE_WIFI_ATOMIC_RESTORE_ASSERTS
#endif
#include "sha2.h"
#ifdef PORTABLE_WIFI_ATOMIC_RESTORE_ASSERTS
#undef NDEBUG
#undef PORTABLE_WIFI_ATOMIC_RESTORE_ASSERTS
#endif
#define PORTABLE_WIFI_ATOMIC_INTENT_BYTES 160u
#define PORTABLE_WIFI_ATOMIC_MUTATIONS 8u
enum { PORTABLE_WIFI_ATOMIC_SAVE=1,PORTABLE_WIFI_ATOMIC_FORGET=2 };
enum { PORTABLE_WIFI_ATOMIC_AGAIN=10,PORTABLE_WIFI_ATOMIC_READY=11 };
typedef struct portable_wifi_atomic_cell {char key[16];uint32_t size;uint8_t value[64];} portable_wifi_atomic_cell;
typedef struct portable_wifi_atomic {
 const risc_key_value_atomic_v1 *api;
 uint64_t revision,token;
 uint32_t operation,phase,count,next,empty,slot,matches,mutation_count;
 int status,read_error;bool cancelled;
 portable_wifi_credentials desired,expected;
 uint8_t intent[PORTABLE_WIFI_ATOMIC_INTENT_BYTES];
 portable_wifi_atomic_cell mutations[PORTABLE_WIFI_ATOMIC_MUTATIONS];
 risc_key_value_atomic_receipt_v1 receipt;
} portable_wifi_atomic;
static inline bool portable_wifi_atomic_api_valid(const risc_key_value_atomic_v1 *api) {
 return api&&api->api_version==1&&api->struct_size>=sizeof(*api)&&api->state&&api->get&&api->submit&&api->collect&&api->cancel;
}
static inline void portable_wifi_atomic_clear(portable_wifi_atomic *s) {if(s)portable_wifi_credentials_wipe(s,sizeof(*s));}
static inline int portable_wifi_atomic_finish(portable_wifi_atomic *s,int status) {
 risc_key_value_atomic_receipt_v1 receipt=s->receipt;portable_wifi_atomic_clear(s);s->receipt=receipt;s->status=status;return status;
}
static inline bool portable_wifi_atomic_pending(const portable_wifi_atomic*s){return s&&s->token!=0;}
static inline int portable_wifi_atomic_begin(portable_wifi_atomic*s,const risc_key_value_atomic_v1*api,unsigned op,const portable_wifi_credentials*desired) {
 if(!s||s->token)return RISC_ATOMIC_INVALID;
 portable_wifi_credentials copy={{0},{0}};if(desired)copy=*desired;
 portable_wifi_atomic_clear(s);
 if(!portable_wifi_atomic_api_valid(api)||(op!=PORTABLE_WIFI_ATOMIC_SAVE&&op!=PORTABLE_WIFI_ATOMIC_FORGET)||!portable_wifi_credentials_validate(&copy)) {portable_wifi_credentials_clear(&copy);return s->status=RISC_ATOMIC_INVALID;}
 s->api=api;s->desired=copy;s->operation=op;s->empty=s->slot=UINT32_MAX;s->status=PORTABLE_WIFI_ATOMIC_AGAIN;
 portable_wifi_credentials_clear(&copy);return s->status;
}
static inline void portable_wifi_atomic_fingerprint(const portable_wifi_credentials *v,uint8_t out[32]) {
 dtls_sha256_ctx h;dtls_sha256_init(&h);static const uint8_t domain[]="RTE-WIFI-RECORD-1";
 uint8_t ssid=(uint8_t)strlen(v->ssid),password=(uint8_t)strlen(v->password);
 dtls_sha256_update(&h,domain,sizeof(domain));dtls_sha256_update(&h,&ssid,1);
 dtls_sha256_update(&h,(const uint8_t*)v->ssid,ssid);dtls_sha256_update(&h,&password,1);
 dtls_sha256_update(&h,(const uint8_t*)v->password,password);dtls_sha256_final(out,&h);portable_wifi_credentials_wipe(&h,sizeof(h));
}
static inline void portable_wifi_atomic_make_intent(portable_wifi_atomic*s) {
 memset(s->intent,0,sizeof(s->intent));memcpy(s->intent,"WFI0001",7);s->intent[8]=(uint8_t)s->operation;s->intent[9]=s->matches?1:0;
 memcpy(s->intent+16,s->desired.ssid,strlen(s->desired.ssid));
 if(s->operation==PORTABLE_WIFI_ATOMIC_SAVE)memcpy(s->intent+49,s->desired.password,strlen(s->desired.password));
 if(s->matches)portable_wifi_atomic_fingerprint(&s->expected,s->intent+113);
 portable_wifi_credentials_put_u32(s->intent+156,portable_wifi_credentials_crc(s->intent,156));
}
static inline int32_t portable_wifi_atomic_read(void*p,const char*k,void*out,uint32_t cap,uint32_t*n) {
 portable_wifi_atomic*s=(portable_wifi_atomic*)p;if(n)*n=0;
 for(uint32_t i=0;i<s->mutation_count;++i)if(!strcmp(k,s->mutations[i].key)) {
  portable_wifi_atomic_cell*m=&s->mutations[i];if(!n)return RISC_KEY_VALUE_INVALID;*n=m->size;
  if(cap<m->size)return RISC_KEY_VALUE_BUFFER_SMALL;
  if(!out)return RISC_KEY_VALUE_INVALID;
  memcpy(out,m->value,m->size);return RISC_KEY_VALUE_OK;
 }
 int rc=s->api->get(s->api->context,s->revision,k,out,cap,n);
 if(rc==RISC_ATOMIC_OK)return RISC_KEY_VALUE_OK;
 if(rc==RISC_ATOMIC_NOT_FOUND)return RISC_KEY_VALUE_NOT_FOUND;
 if(!s->read_error)s->read_error=rc;
 return rc==RISC_ATOMIC_BUSY?RISC_KEY_VALUE_BUSY:rc==RISC_ATOMIC_BUFFER_SMALL?RISC_KEY_VALUE_BUFFER_SMALL:RISC_KEY_VALUE_IO;
}
static inline int32_t portable_wifi_atomic_write(void*p,const char*k,const void*data,uint32_t n) {
 portable_wifi_atomic*s=(portable_wifi_atomic*)p;size_t length=0;while(length<16&&k[length])++length;
 if(!length||length>=16||!data||!n||n>64){s->read_error=RISC_ATOMIC_INVALID;return RISC_KEY_VALUE_INVALID;}
 uint32_t i=0;while(i<s->mutation_count&&strcmp(k,s->mutations[i].key))++i;
 if(i==PORTABLE_WIFI_ATOMIC_MUTATIONS){s->read_error=RISC_ATOMIC_CAPACITY;return RISC_KEY_VALUE_IO;}
 if(i==s->mutation_count)++s->mutation_count;
 portable_wifi_atomic_cell*m=&s->mutations[i];portable_wifi_credentials_wipe(m,sizeof(*m));memcpy(m->key,k,length+1);m->size=n;memcpy(m->value,data,n);return RISC_KEY_VALUE_OK;
}
static inline risc_key_value_v1 portable_wifi_atomic_view(portable_wifi_atomic*s) {
 return (risc_key_value_v1){1,sizeof(risc_key_value_v1),s,portable_wifi_atomic_read,portable_wifi_atomic_write};
}
static inline int portable_wifi_atomic_step(portable_wifi_atomic*s) {
 if(!s)return RISC_ATOMIC_INVALID;
 if(s->token){
  risc_key_value_atomic_receipt_v1 receipt={.struct_size=sizeof(receipt)};
  int rc=s->api->collect(s->api->context,s->token,&receipt);
  if(rc==RISC_ATOMIC_PENDING||rc==RISC_ATOMIC_BUSY)return PORTABLE_WIFI_ATOMIC_AGAIN;
  if(rc!=RISC_ATOMIC_OK)return portable_wifi_atomic_finish(s,RISC_ATOMIC_RETAINED);
  s->receipt=receipt;return portable_wifi_atomic_finish(s,receipt.status);
 }
 if(s->status!=PORTABLE_WIFI_ATOMIC_AGAIN)return s->status;
 if(!s->phase){
  risc_key_value_atomic_state_v1 state={.struct_size=sizeof(state)};
  int rc=s->api->state(s->api->context,&state);if(rc==RISC_ATOMIC_BUSY)return s->status;
  if(rc)return portable_wifi_atomic_finish(s,rc);
  if(state.namespace_id!=PORTABLE_WIFI_CREDENTIALS_STORE_INSTANCE||!state.revision||state.max_mutations<PORTABLE_WIFI_ATOMIC_MUTATIONS||state.max_value_bytes<64||state.max_group_bytes<1024||state.max_intent_bytes<sizeof(s->intent)||
     (state.flags&(RISC_KEY_VALUE_ATOMIC_NATIVE_DURABLE|RISC_KEY_VALUE_ATOMIC_COPIED_ASYNC))!=(RISC_KEY_VALUE_ATOMIC_NATIVE_DURABLE|RISC_KEY_VALUE_ATOMIC_COPIED_ASYNC))return portable_wifi_atomic_finish(s,RISC_ATOMIC_INVALID);
  s->revision=state.revision;s->phase=1;return s->status;
 }
 risc_key_value_v1 view=portable_wifi_atomic_view(s);s->read_error=0;
 if(s->phase==1){
  int rc=portable_wifi_profile_count(&view,&s->count);
  if(s->read_error==RISC_ATOMIC_BUSY)return s->status;
  if(s->read_error)return portable_wifi_atomic_finish(s,s->read_error);
  if(rc)return portable_wifi_atomic_finish(s,RISC_ATOMIC_CONFLICT);
  s->phase=2;return s->status;
 }
 if(s->phase==2&&s->next<s->count){
  portable_wifi_credentials row={{0},{0}};int rc=portable_wifi_profile_load(&view,s->next,&row);
  if(s->read_error){portable_wifi_credentials_clear(&row);return s->read_error==RISC_ATOMIC_BUSY?s->status:portable_wifi_atomic_finish(s,s->read_error);}
  if(rc!=PORTABLE_WIFI_CREDENTIALS_LOADED&&rc!=PORTABLE_WIFI_CREDENTIALS_EMPTY){portable_wifi_credentials_clear(&row);return portable_wifi_atomic_finish(s,RISC_ATOMIC_CONFLICT);}
  if(rc==PORTABLE_WIFI_CREDENTIALS_EMPTY&&s->empty==UINT32_MAX)s->empty=s->next;
  if(rc==PORTABLE_WIFI_CREDENTIALS_LOADED&&!strcmp(row.ssid,s->desired.ssid)){++s->matches;s->slot=s->next;s->expected=row;}
  portable_wifi_credentials_clear(&row);++s->next;return s->status;
 }
 if(s->phase==2){
  if(s->matches>1)return portable_wifi_atomic_finish(s,RISC_ATOMIC_CONFLICT);
  if(!s->matches)s->slot=s->empty==UINT32_MAX?s->count:s->empty;
  if(s->slot==UINT32_MAX)return portable_wifi_atomic_finish(s,RISC_ATOMIC_CAPACITY);
  portable_wifi_atomic_make_intent(s);s->phase=3;
 }
 portable_wifi_credentials_wipe(s->mutations,sizeof(s->mutations));s->mutation_count=0;
 int rc=s->operation==PORTABLE_WIFI_ATOMIC_SAVE?portable_wifi_profile_save(&view,s->slot,&s->desired):
        s->matches?portable_wifi_profile_forget(&view,s->slot):PORTABLE_WIFI_CREDENTIALS_FORGOTTEN;
 if(s->read_error){portable_wifi_credentials_wipe(s->mutations,sizeof(s->mutations));s->mutation_count=0;return s->read_error==RISC_ATOMIC_BUSY?s->status:portable_wifi_atomic_finish(s,s->read_error);}
 if(rc!=(s->operation==PORTABLE_WIFI_ATOMIC_SAVE?PORTABLE_WIFI_CREDENTIALS_SAVED:PORTABLE_WIFI_CREDENTIALS_FORGOTTEN))return portable_wifi_atomic_finish(s,RISC_ATOMIC_CONFLICT);
 return s->status=PORTABLE_WIFI_ATOMIC_READY;
}
static inline int portable_wifi_atomic_submit(portable_wifi_atomic*s) {
 if(!s||s->status!=PORTABLE_WIFI_ATOMIC_READY||s->token)return RISC_ATOMIC_INVALID;
 risc_key_value_atomic_mutation_v1 mutations[PORTABLE_WIFI_ATOMIC_MUTATIONS];memset(mutations,0,sizeof(mutations));
 for(uint32_t i=0;i<s->mutation_count;++i)mutations[i]=(risc_key_value_atomic_mutation_v1){s->mutations[i].key,RISC_ATOMIC_PUT,s->mutations[i].value,s->mutations[i].size};
 uint64_t token=0;int rc=s->api->submit(s->api->context,s->revision,mutations,s->mutation_count,s->intent,sizeof(s->intent),&token);
 portable_wifi_credentials_wipe(mutations,sizeof(mutations));
 if(rc==RISC_ATOMIC_BUSY&&!token)return PORTABLE_WIFI_ATOMIC_READY;
 if(rc!=RISC_ATOMIC_OK||!token)return portable_wifi_atomic_finish(s,token?RISC_ATOMIC_RETAINED:rc==RISC_ATOMIC_OK?RISC_ATOMIC_INVALID:rc);
 s->token=token;s->status=PORTABLE_WIFI_ATOMIC_AGAIN;
 /* Native owns a copy; plaintext app staging is no longer required. */
 portable_wifi_credentials_clear(&s->desired);portable_wifi_credentials_clear(&s->expected);portable_wifi_credentials_wipe(s->intent,sizeof(s->intent));portable_wifi_credentials_wipe(s->mutations,sizeof(s->mutations));s->mutation_count=0;
 return s->status;
}
static inline int portable_wifi_atomic_cancel(portable_wifi_atomic*s) {
 if(!s)return RISC_ATOMIC_INVALID;
 if(!s->token)return portable_wifi_atomic_finish(s,RISC_ATOMIC_CANCELLED);
 if(!s->cancelled){int rc=s->api->cancel(s->api->context,s->token);if(rc!=RISC_ATOMIC_OK&&rc!=RISC_ATOMIC_PENDING&&rc!=RISC_ATOMIC_CANCELLED)return RISC_ATOMIC_RETAINED;s->cancelled=true;}
 return portable_wifi_atomic_step(s);
}
#endif
