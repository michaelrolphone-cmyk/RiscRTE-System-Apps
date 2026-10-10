#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "RiscRuntimeV1.h"
#include "RiscAppDataV1.h"
#include "ContextFingerprintService.h"
typedef struct {
 const contexts_service_v1 *api;const risc_runtime_api_v1 *runtime;
 risc_runtime_capability_v1 fingerprint_store;uint64_t rules_revision;bool loaded,rules_available;
} portable_contexts_client;
#include "PortableContextRules.h"
static uint8_t disk[CR_FILE_BYTES];static uint32_t length;static uint64_t revision=1;
static bool ambiguous,retained;static unsigned writes;static cr_store live;
static int32_t fs_stat(void*c,const char*n,uint32_t*z,uint64_t*r){(void)c;assert(!strcmp(n,"context-rules.ctx"));*z=length;*r=length?revision:0;return retained?RISC_APP_DATA_RETAINED:length?0:RISC_APP_DATA_NOT_FOUND;}
static int32_t fs_read(void*c,const char*n,uint64_t expected,void*b,uint32_t cap,uint32_t*z,uint64_t*r){(void)c;(void)n;if(expected!=revision)return RISC_APP_DATA_STALE;assert(cap>=length);memcpy(b,disk,length);*z=length;*r=revision;return 0;}
static int32_t fs_replace(void*c,const char*n,uint64_t expected,const void*b,uint32_t z){(void)c;(void)n;if(expected!=(length?revision:0))return RISC_APP_DATA_STALE;assert(z==CR_FILE_BYTES);memcpy(disk,b,z);length=z;++revision;++writes;return ambiguous?RISC_APP_DATA_COMMIT_UNKNOWN:0;}
static risc_app_data_v1 files={1,sizeof(files),NULL,fs_stat,fs_read,fs_replace};
static bool acquire(const char*n,uint32_t api,uint64_t ns,risc_runtime_capability_v1*g){assert(!strcmp(n,RISC_SHARED_DATA_CAPABILITY)&&api==1&&ns==4);g->api=&files;return true;}
static bool release(risc_runtime_capability_v1*g){if(retained)return false;g->api=NULL;return true;}
static bool pause_capture(void*c){(void)c;return !retained;}
static bool request(void*c,uint32_t op,void*v){(void)c;assert(op==CONTEXTS_FP_RULES);contexts_rules_v1*q=v;if(q->set)live=q->store;else{q->store=live;q->available=true;}return true;}
int main(void){
 contexts_fingerprint_service_v1 service={.base={.api_version=1,.struct_size=sizeof(service),.pause=pause_capture},.fingerprint_abi=0x31504643u,.fingerprint=request};
 risc_runtime_api_v1 rt={.acquire=acquire,.release=release};portable_contexts_client c={.api=&service.base,.runtime=&rt};
 assert(portable_context_rules_load(&c)&&c.rules_available);cr_store s;cr_init(&s);s.generation=1;
 s.items[0]=(cr_item){.kind=CR_BATTERY,.arg0=20,.name="Battery"};
 assert(portable_context_rules_save(&c,&s)&&writes==1&&live.generation==1);
 ++revision; /* A profile checkpoint changes the global app-data token. */
 s.generation=2;s.items[0].arg0=15;assert(portable_context_rules_save(&c,&s)&&writes==2);
 ambiguous=true;s.generation=3;assert(portable_context_rules_save(&c,&s)&&writes==3);
 assert(portable_context_rules_save(&c,&s)&&writes==3); /* Lost acknowledgement retry. */
 cr_store other=s;other.generation=9;assert(cr_encode(&other,disk,sizeof(disk)));++revision;
 s.generation=4;assert(!portable_context_rules_save(&c,&s)&&writes==3);
 disk[100]^=1;assert(portable_context_rules_load(&c)&&!c.rules_available);assert(!portable_context_rules_save(&c,&s)&&writes==3);
 retained=true;assert(!portable_context_rules_load(&c)&&c.fingerprint_store.api);
 puts("Rule storage: global revision changes, uncertain-commit readback, idempotent retry, conflict/corruption preservation and retained cleanup PASS");
}
