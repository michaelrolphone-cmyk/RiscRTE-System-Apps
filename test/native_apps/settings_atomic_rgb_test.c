/* Actual shared RGB Settings controller, explicit user intents and checked custody. */
#define PORTABLE_NATIVE_CUSTODY_FENCE
#define PORTABLE_CHECKED_STORAGE_RUNTIME
#define PORTABLE_SLEEP_SETTINGS
#define PORTABLE_ALARM_SETTINGS
#define PORTABLE_SETTINGS_ATOMIC_EXPERIMENTAL
#define TEST_SETTINGS_CUSTOM_RUNTIME
#define main original_fixture_main
#include "portable_settings_test.c"
#undef main

static risc_runtime_api_v1 checked_runtime;
static unsigned mode, submissions, collections, releases;
static uint64_t atomic_revision=7, token;
static char saved_key[16], staged_key[16];
static uint8_t saved[64], staged[64];
static uint32_t saved_size, staged_size;
static bool retained;
static int32_t astate(void*c,risc_key_value_atomic_state_v1*s){
 (void)c;*s=(risc_key_value_atomic_state_v1){sizeof(*s),3,1,16,64,4096,1024,0,atomic_revision};return RISC_ATOMIC_OK;
}
static int32_t aget(void*c,uint64_t rev,const char*key,void*out,uint32_t cap,uint32_t*used){
 (void)c;*used=0;if(rev!=atomic_revision)return RISC_ATOMIC_STALE;
 if(strcmp(key,saved_key)||!saved_size)return RISC_ATOMIC_NOT_FOUND;
 *used=saved_size;if(cap<saved_size)return RISC_ATOMIC_BUFFER_SMALL;memcpy(out,saved,saved_size);return RISC_ATOMIC_OK;
}
static int32_t asubmit(void*c,uint64_t rev,const risc_key_value_atomic_mutation_v1*m,uint32_t count,const void*intent,uint32_t bytes,uint64_t*out){
 (void)c;*out=0;assert(count==1&&m->kind==RISC_ATOMIC_PUT&&bytes==PORTABLE_PREFERENCE_INTENT_BYTES);
 const uint8_t*p=intent;assert(!memcmp(p,"PFI0001",8)&&p[8]==PORTABLE_PREFERENCE_USER&&!strcmp((const char*)p+12,m->key));
 if(mode==3||rev!=atomic_revision)return RISC_ATOMIC_STALE;
 assert(!token&&m->size<=sizeof(staged));strcpy(staged_key,m->key);memcpy(staged,m->value,m->size);staged_size=m->size;
 *out=token=++submissions;return RISC_ATOMIC_OK;
}
static int32_t acollect(void*c,uint64_t t,risc_key_value_atomic_receipt_v1*r){
 (void)c;assert(t==token);if(++collections%3)return RISC_ATOMIC_PENDING;
 int status=mode==4?RISC_ATOMIC_IO:RISC_ATOMIC_OK;
 if(!status){strcpy(saved_key,staged_key);memcpy(saved,staged,staged_size);saved_size=staged_size;++atomic_revision;}
 *r=(risc_key_value_atomic_receipt_v1){sizeof(*r),status,atomic_revision,status?0:1};token=0;return RISC_ATOMIC_OK;
}
static int32_t acancel(void*c,uint64_t t){(void)c;assert(t==token);return RISC_ATOMIC_OK;}
static const risc_key_value_atomic_v1 atomic_api={.api_version=1,.struct_size=sizeof(atomic_api),.state=astate,.get=aget,.submit=asubmit,.collect=acollect,.cancel=acancel};
static bool battery_sample(void*c,risc_battery_sample_v1*out){(void)c;*out=(risc_battery_sample_v1){.millivolts=4000,.percent=80};return true;}
static const risc_battery_gauge_api_v1 battery_api={.api_version=1,.struct_size=sizeof(battery_api),.read=battery_sample};
static bool acquire_checked(const char*name,uint32_t version,uint64_t id,risc_runtime_capability_v1*g){
 bool ok;if(!strcmp(name,RISC_KEY_VALUE_ATOMIC_CAPABILITY)){assert(id==1&&version==1);g->api=&atomic_api;++grants;ok=true;}
 else if(!strcmp(name,"board.battery")){g->api=&battery_api;++grants;ok=true;}
 else ok=test_acquire(name,version,id,g);
 if(ok){g->slot=grants;g->generation=1;}return ok;
}
static bool release_checked(risc_runtime_capability_v1*g){++releases;assert(!token);bool ok=test_release(g);if(ok){g->slot=0;g->generation=0;}return ok;}
static bool retain_checked(void){retained=true;return true;}
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){return v==1?&checked_runtime:NULL;}
int main(int argc,char**argv){
 assert(argc==2);mode=(unsigned)atoi(argv[1]);assert(mode<=5);scenario=100;
 checked_runtime=runtime_api;checked_runtime.acquire=acquire_checked;checked_runtime.release=release_checked;checked_runtime.retain_invocation=retain_checked;
 assert(app_module_init()==0);assert(settings_atomic_api&&settings_store);settings_render(0,0);polls=0;
 tap(3,100,126);tap(7,mode==2?60:170,213);if(mode==3||mode==4)tap(40,60,213);
 uint8_t result=settings_activate(0,mode==1?2:mode==5?SETTINGS_ALERT_ROW:4);
 assert((result==T5_APP_SETTING_UPDATED)==(mode<2||mode==5));
 assert(!format_writes&&!kv_writes&&!writes&&!retained);
 assert(submissions==((mode==2||mode==3)?0u:1u));
 if(mode==5){assert(!strcmp(saved_key,"alert_mode")&&saved_size==1&&saved[0]==PORTABLE_ALERT_SOUND);}
 else if(mode<2){assert(!strcmp(saved_key,mode==1?"time_format":"sleep_mode")&&saved_size==4&&saved[2]==1);}
 else assert(!saved_size);
 app_module_fini();assert(!grants&&!subscriptions&&!frame_count&&!token);
 polls=0;input_count=0;assert(app_module_init()==0);
 unsigned loaded;
 if(mode==5){assert(portable_alert_load(settings_store,&loaded)==PORTABLE_ALERT_LOADED&&loaded==PORTABLE_ALERT_SOUND);}
 else if(mode==1){assert(portable_time_format_load(settings_store,&loaded)==PORTABLE_TIME_FORMAT_LOADED&&loaded==PORTABLE_TIME_FORMAT_24);}
 else {assert(settings_sleep_load(&loaded)==(mode==0?PORTABLE_SLEEP_LOADED:PORTABLE_SLEEP_MISSING));if(mode==0)assert(loaded==PORTABLE_SLEEP_DEEP);}
 app_module_fini();assert(!grants&&!retained);
 printf("RGB atomic Settings %u: explicit intent, legacy-write fence, failure/cancel, reentry PASS\n",mode);
}
