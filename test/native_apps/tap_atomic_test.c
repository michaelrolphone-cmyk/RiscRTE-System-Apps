#define PORTABLE_SETTINGS_ATOMIC_EXPERIMENTAL
#define PORTABLE_SETTINGS_RUNTIME_OVERRIDE
#define TAP_ATOMIC_FIXTURE
#include "tap_settings_test.c"
static risc_runtime_api_v1 atomic_runtime;
static uint64_t revision=7,atomic_token;
static uint8_t staged_tap[8];
static unsigned atomic_submits;
static int32_t atomic_state(void*c,risc_key_value_atomic_state_v1*s){(void)c;*s=(risc_key_value_atomic_state_v1){sizeof(*s),3,1,16,64,4096,1024,0,revision};return 0;}
static int32_t atomic_get(void*c,uint64_t rev,const char*k,void*out,uint32_t cap,uint32_t*size){
 *size=0;if(rev!=revision)return RISC_ATOMIC_STALE;
 if(strcmp(k,PORTABLE_TAP_KEY))return RISC_ATOMIC_NOT_FOUND;
 uint8_t value[8];uint32_t n;int rc=tap_get(c,k,value,8,&n);
 if(rc==RISC_KEY_VALUE_NOT_FOUND)return RISC_ATOMIC_NOT_FOUND;
 if(rc)return RISC_ATOMIC_IO;
 *size=n;if(cap<n)return RISC_ATOMIC_BUFFER_SMALL;memcpy(out,value,n);return 0;
}
static int32_t atomic_submit(void*c,uint64_t rev,const risc_key_value_atomic_mutation_v1*m,uint32_t count,const void*intent,uint32_t bytes,uint64_t*t){
 (void)c;*t=0;assert(!sensor_live&&!tap_cleanup_pending&&!atomic_token);
 assert(count==1&&m->kind==RISC_ATOMIC_PUT&&!strcmp(m->key,PORTABLE_TAP_KEY)&&m->size==8&&bytes==176);
 const uint8_t*p=intent;assert(!memcmp(p,"PFI0001",8)&&p[8]==PORTABLE_PREFERENCE_USER&&!strcmp((const char*)p+12,PORTABLE_TAP_KEY));
 if(rev!=revision)return RISC_ATOMIC_STALE;
 memcpy(staged_tap,m->value,8);*t=atomic_token=++atomic_submits;return 0;
}
static int32_t atomic_collect(void*c,uint64_t token,risc_key_value_atomic_receipt_v1*r){
 assert(token&&token==atomic_token&&!sensor_live);
 tap_atomic_publishing=true;
 int rc=tap_put(c,PORTABLE_TAP_KEY,staged_tap,8)?RISC_ATOMIC_IO:RISC_ATOMIC_OK;
 tap_atomic_publishing=false;
 if(!rc)revision++;
 *r=(risc_key_value_atomic_receipt_v1){sizeof(*r),rc,revision,rc?0:revision};atomic_token=0;return 0;
}
static int32_t atomic_cancel(void*c,uint64_t token){(void)c;assert(token==atomic_token);return 0;}
static const risc_key_value_atomic_v1 atomic_api={1,sizeof(atomic_api),NULL,atomic_state,atomic_get,atomic_submit,atomic_collect,atomic_cancel};
static bool atomic_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){
 if(strcmp(n,RISC_KEY_VALUE_ATOMIC_CAPABILITY))return test_acquire(n,v,id,g);
 assert(v==1&&id==1);g->api=&atomic_api;g->slot=1;g->generation=1;grants++;return true;
}
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){return v==1?&atomic_runtime:NULL;}
int main(int argc,char**argv){
 atomic_runtime=runtime_api;atomic_runtime.acquire=atomic_acquire;
 int rc=tap_fixture_main(argc,argv);assert(!atomic_token&&atomic_submits==tap_writes);return rc;
}
