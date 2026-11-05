#include "PortablePreferenceRollback.h"
#include <assert.h>
#include <stdio.h>
static uint64_t revision=1,op=0,token;
static uint8_t stored[64],pending[64],intent[192];static unsigned stored_size,pending_size,kind;
static int32_t state(void*c,risc_key_value_atomic_state_v1*s){(void)c;*s=(risc_key_value_atomic_state_v1){.struct_size=sizeof(*s),.flags=3,.namespace_id=1,.max_mutations=8,.max_value_bytes=64,.max_group_bytes=4096,.max_intent_bytes=192,.revision=revision};return 0;}
static int32_t get(void*c,uint64_t r,const char*k,void*p,uint32_t cap,uint32_t*n){(void)c;assert(!strcmp(k,"quick_radio"));if(r!=revision)return RISC_ATOMIC_STALE;*n=stored_size;if(!stored_size)return RISC_ATOMIC_NOT_FOUND;assert(cap>=stored_size);memcpy(p,stored,stored_size);return 0;}
static int32_t submit(void*c,uint64_t r,const risc_key_value_atomic_mutation_v1*m,uint32_t count,const void*p,uint32_t size,uint64_t*t){(void)c;*t=0;if(r!=revision)return RISC_ATOMIC_STALE;assert(!token&&count==1&&size<=192&&!strcmp(m->key,"quick_radio"));kind=m->kind;pending_size=m->size;if(pending_size)memcpy(pending,m->value,pending_size);memcpy(intent,p,size);*t=token=10;return 0;}
static int32_t collect(void*c,uint64_t t,risc_key_value_atomic_receipt_v1*r){(void)c;assert(t==token);stored_size=kind==RISC_ATOMIC_DELETE?0:pending_size;if(stored_size)memcpy(stored,pending,stored_size);*r=(risc_key_value_atomic_receipt_v1){.struct_size=sizeof(*r),.status=0,.revision=++revision,.operation_id=++op};token=0;return 0;}
static int32_t cancel(void*c,uint64_t t){(void)c;assert(t==token);return 0;}
static bool guard(void*c){(void)c;return true;}
static const risc_key_value_atomic_v1 api={1,sizeof(api),NULL,state,get,submit,collect,cancel};
static portable_preference_atomic user(void){portable_preference_atomic s={0};uint8_t b[]={0x51,1,3,0xa6};assert(portable_preference_atomic_begin(&s,&api,"quick_radio",PORTABLE_PREFERENCE_USER,guard,NULL)==PORTABLE_PREFERENCE_READY);assert(portable_preference_atomic_prepare(&s,b,4)==PORTABLE_PREFERENCE_READY);assert(portable_preference_atomic_submit(&s)==RISC_ATOMIC_PENDING);assert(portable_preference_atomic_collect(&s)==0);return s;}
int main(void){
 assert(portable_preference_atomic_key("ble_broadcast")&&portable_preference_atomic_key("context_sources")&&portable_preference_atomic_key("context_timing"));
 assert(portable_preference_atomic_key("bs00123456789ab")&&portable_preference_atomic_key("bs3ffffffffffff"));
 assert(!portable_preference_atomic_key("bs40123456789ab")&&!portable_preference_atomic_key("bs00123456789AB")&&!portable_preference_atomic_key("bs00123456789a")&&!portable_preference_atomic_key("quick_bright"));
 portable_preference_atomic origin=user();portable_preference_rollback r={0};assert(portable_preference_rollback_begin(&r,&origin,guard,NULL)==PORTABLE_PREFERENCE_READY);
 assert(r.kind==RISC_ATOMIC_DELETE&&r.intent[8]==PORTABLE_PREFERENCE_ROLLBACK&&!memcmp(r.intent,"PFR0001",7)&&r.origin_operation==1&&r.origin_revision==2);
 assert(portable_preference_rollback_submit(&r)==RISC_ATOMIC_PENDING);memset(r.edit.desired,0xff,sizeof(r.edit.desired));assert(portable_preference_atomic_collect(&r.edit)==0&&!stored_size);
 stored[0]=0x51;stored[1]=1;stored[2]=1;stored[3]=0xa4;stored_size=4;
 origin=user();r=(portable_preference_rollback){0};assert(portable_preference_rollback_begin(&r,&origin,guard,NULL)==PORTABLE_PREFERENCE_READY&&r.kind==RISC_ATOMIC_PUT&&r.edit.desired[2]==1);
 revision++;assert(portable_preference_rollback_submit(&r)==RISC_ATOMIC_STALE&&stored[2]==3);
 r=(portable_preference_rollback){0};assert(portable_preference_rollback_begin(&r,&origin,guard,NULL)==RISC_ATOMIC_STALE);
 origin=user();origin.receipt.operation_id=0;r=(portable_preference_rollback){0};assert(portable_preference_rollback_begin(&r,&origin,guard,NULL)==RISC_ATOMIC_INVALID);
 puts("Utilities admitted keys and receipt-bound rollback: absent, exact prior, copied, stale and invalid origin PASS");return 0;
}
