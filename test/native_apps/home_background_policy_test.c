/* Actual Contexts/Broadcast clients and storage guards together. The old Home
 * KV read dispatch invalidated the other client's policy on every read. */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#define PORTABLE_NATIVE_CUSTODY_FENCE
#define PORTABLE_NATIVE_TIME_TOOLBAR
#define PORTABLE_CONTEXTS_CLIENT
#define PORTABLE_BLE_BROADCAST
#define PORTABLE_BLE_BROADCAST_DEFAULT_OFF
static bool native_custody_retained,failed,pause_failed;
static void portable_adapter_retain(void){native_custody_retained=failed=true;}
#define PORTABLE_CONTEXTS_CUSTODY_SAFE() (!native_custody_retained)
#define PORTABLE_CONTEXTS_CLEANUP_FAILURE() portable_adapter_retain()
#define PORTABLE_BROADCAST_CUSTODY_SAFE() (!native_custody_retained)
#include "PortableContextsClient.h"
#include "PortableBroadcastClient.h"
#include "PortableBackgroundServices.h"
static const risc_runtime_api_v1 runtime;
static const risc_runtime_api_v1 *rt=&runtime;
#include "../../lib/PortableApps/src/contexts_adapter.inc"
#include "../../lib/PortableApps/src/broadcast_adapter.inc"
static unsigned now=100,reads,context_steps,broadcast_steps,pauses,live;
static bool pause_service(void*c){(void)c;assert(!native_custody_retained);++pauses;return !pause_failed;}
static bool context_step(void*c,const contexts_policy_v1*p){(void)c;assert(p&&p->struct_size==sizeof(*p));++context_steps;return true;}
static bool broadcast_step(void*c,bool allow,const telemetry_broadcast_policy_v1*p){(void)c;(void)allow;assert(p&&p->settings_valid);++broadcast_steps;return true;}
static bool broadcast_status(void*c,telemetry_broadcast_status_v1*p){(void)c;memset(p,0,sizeof(*p));return true;}
static int32_t enumerate(void*c,uint32_t i,risc_telemetry_field_v1*p){(void)c;(void)i;(void)p;return 0;}
static int32_t read_value(void*c,uint32_t i,int32_t*p){(void)c;(void)i;(void)p;return 0;}
static const telemetry_broadcast_v1 service={1,sizeof(service),NULL,broadcast_step,pause_service,broadcast_status,enumerate,read_value};
static const contexts_service_v1 context_service={.api_version=1,.struct_size=CONTEXTS_SERVICE_V1_SIZE,.step=context_step,.pause=pause_service};
static bool guard_read(void){
#ifdef TEST_LEGACY_STORAGE_READ
 return contexts_before_storage()&&broadcast_before_storage();
#else
 return contexts_before_storage_read()&&broadcast_before_storage_read();
#endif
}
static int32_t get(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){
 (void)c;(void)k;(void)b;(void)cap;if(!guard_read())return RISC_KEY_VALUE_CONTEXT;
 ++reads;*n=0;return RISC_KEY_VALUE_NOT_FOUND;
}
static int32_t put(void*c,const char*k,const void*b,uint32_t n){
 (void)c;(void)k;(void)b;(void)n;
 return contexts_before_storage()&&broadcast_before_storage()?RISC_KEY_VALUE_OK:RISC_KEY_VALUE_CONTEXT;
}
static const risc_key_value_v1 kv={1,sizeof(kv),NULL,get,put};
static bool acquire(const char*n,uint32_t version,uint64_t id,risc_runtime_capability_v1*g){
 assert(!native_custody_retained&&version==1);
 if(!strcmp(n,RISC_KEY_VALUE_CAPABILITY)){assert(id==1);g->api=&kv;}
 else{assert(!strcmp(n,TELEMETRY_BROADCAST_CAPABILITY)&&!id);g->api=&service;}
 g->slot=++live;g->generation=1;return true;
}
static bool release(risc_runtime_capability_v1*g){assert(!native_custody_retained&&live&&g->api);--live;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static bool health(risc_runtime_health_v1*out){out->uptime_ms=now;return true;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.health=health,.acquire=acquire,.release=release};
static void tick(void){
 assert(broadcast_tick());contexts_policy_io=true;
 assert(portable_contexts_step(&contexts_client,false,true));contexts_policy_io=false;
 assert(!live&&!failed);
}
int main(void){
 contexts_client.runtime=rt;contexts_client.api=&context_service;
 for(unsigned i=0;i<20;i++){tick();now+=20;}
 assert(context_steps==20&&broadcast_steps==20&&pauses>0);
 printf("Home policy: 20 passes, %u KV reads, %u pauses\n",reads,pauses);
#ifdef TEST_LEGACY_STORAGE_READ
 assert(reads==100); /* 2 Broadcast + 3 Contexts reads on every pass. */
#else
 assert(reads==5&&contexts_client.loaded&&broadcast_client.loaded);
 unsigned before=reads;assert(put(NULL,"quick_radio",NULL,0)==RISC_KEY_VALUE_OK);
 assert(!contexts_client.loaded&&!broadcast_client.loaded);tick();assert(reads==before+5);
 now+=1000;before=reads;tick();assert(reads==before+2&&contexts_client.loaded);
 /* A failed quiesce still fences storage; restoring cached policy never
  * clears uncertain native custody or permits a raw read. */
 pause_failed=true;before=reads;uint32_t n=0;assert(get(NULL,"quick_radio",NULL,0,&n)==RISC_KEY_VALUE_CONTEXT);
 assert(native_custody_retained&&failed&&reads==before);
#endif
 return 0;
}
