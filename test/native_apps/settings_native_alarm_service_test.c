/* Actual native Settings Save and actual separately compiled schedule service.
 * Only Runtime native clock/KV providers are modeled. */
#define PORTABLE_SETTINGS_APP
#define PORTABLE_SETTINGS_NATIVE_TIME
#define PORTABLE_SETTINGS_NATIVE_ONLY
#define PORTABLE_SETTINGS_TIME_ZONE
#define PORTABLE_NATIVE_CUSTODY_FENCE
#define PORTABLE_ALARM_CLIENT
#define ALARM_SERVICE_TAGGED_V2
#define ALARM_SERVICE_CLOCK_EDIT_V2
#define ALARM_NATIVE_UTC
#define PORTABLE_SETTINGS_CLOCK_EDIT_V2
#include "../../lib/PortableApps/src/adapter.c"
#include "AlarmRecords.h"
#include "RiscProviderV2.h"
#include "RiscBoundKeyValueV1.h"
#include "RiscPlatformClockV1.h"
#include "RiscPlatformRealtimeV1.h"
#include <assert.h>
extern const risc_driver_v2 *t5_driver_get(uint32_t);
const t5_app_manifest_t portable_catalog[1]={{.compatible=false}};
const unsigned portable_catalog_count=0;
static int64_t epoch=1767225600;
static uint64_t mono;
static unsigned seed_calls,occ_writes,ledger_writes;
static uint8_t blobs[7][512];static uint32_t sizes[7];
static const char*keys[]={ALARM_CONFIG_KEY,ALARM_TIMER_KEY,ALARM_MODE_KEY,ALARM_OCCURRENCE_KEY,ALARM_TIMER_OCCURRENCE_KEY,"alert_dnd",ALARM_CLOCK_EDIT_KEY};
static int key(const char*k){for(int i=0;i<7;i++)if(!strcmp(k,keys[i]))return i;assert(0);return -1;}
static int32_t bound_get(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){assert(c==blobs);int i=key(k);*n=0;if(!sizes[i])return -1;if(cap<sizes[i])return -2;*n=sizes[i];memcpy(b,blobs[i],*n);return 0;}
static int32_t bound_put(void*c,const char*k,const void*b,uint32_t n){assert(c==blobs);int i=key(k);assert(i==3||i==4||i==6);assert(n<=512);memcpy(blobs[i],b,n);sizes[i]=n;if(i==6)ledger_writes++;else occ_writes++;return 0;}
static uint64_t time_ms(void*c){assert(c==&mono);return mono;}
static int32_t native_read(void*c,risc_realtime_snapshot_v1*out){assert(c==&epoch);*out=(risc_realtime_snapshot_v1){.struct_size=sizeof(*out),.epoch_seconds=epoch,.validity=RISC_REALTIME_VALID,.monotonic_before_us=mono*1000,.monotonic_after_us=mono*1000};return 0;}
static int32_t native_seed(void*c,int64_t e,uint32_t us){assert(c==&epoch&&!us);seed_calls++;epoch=e;return 0;}
static risc_realtime_control_api_v1 control={1,sizeof(control),&epoch,native_read,native_seed};
static bool acquire(const char*cap,uint32_t v,uint64_t instance,risc_runtime_capability_v1*out){assert(!strcmp(cap,"runtime.realtime-control")&&v==1&&!instance);out->api=&control;out->slot=1;out->generation=1;return true;}
static bool release(risc_runtime_capability_v1*g){*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static risc_runtime_api_v1 runtime;
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){return v==1?&runtime:NULL;}
static const risc_bound_key_value_v1 kv={2,sizeof(kv),blobs,bound_get,bound_put};
static const risc_platform_clock_api_v1 clock_table={1,sizeof(clock_table),&mono,time_ms,NULL};
static const risc_platform_realtime_api_v1 real_table={1,sizeof(real_table),&epoch,native_read};
static const risc_provider_dependency_v1 deps[]={{"storage.key-value.bound",2,&kv},{"platform.clock",1,&clock_table},{"platform.realtime",1,&real_table}};
static const risc_driver_v2*provider;
static void boot(void){provider=t5_driver_get(2);assert(provider->start(deps,3));alarms=(portable_alarm_client){0};alarms.api=provider->capability;}
static void settle(void){for(unsigned i=0;i<256;i++){assert(portable_alarm_pump(&alarms));if(alarms.status.state==ALARM_STATE_READY||portable_alarm_owned(&alarms)||alarms.status.state==ALARM_STATE_BLOCKED)return;}assert(0);}
static void draft(int which){const twatch_rtc_time_v1 values[]={{2027,2,3,3,4,5,6},{2025,12,30,2,23,58,57},{2026,1,2,5,0,1,2},{2025,12,31,3,20,30,40}};settings_draft=values[which];}
int main(int argc,char**argv){assert(argc==3);unsigned state=(unsigned)atoi(argv[1]),direction=(unsigned)atoi(argv[2]);assert(state<6&&direction<4);
 runtime=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release};rt=&runtime;snt_load_basis();snt_zone_status=PORTABLE_TIMEZONE_MISSING;assert(portable_timezone_resolve("UTC",4,&snt_rule)==0);
 const uint32_t before=(uint32_t)(epoch-946684800);uint8_t cfgbytes[32]={0},occbytes[32]={0};
 if(state){alarm_config cfg={1,before+(state==2?0:300),before-20,state==2?20:320,2,1};alarm_config_encode(&cfg,blobs[1]);sizes[1]=32;memcpy(cfgbytes,blobs[1],32);if(state==3){alarm_occurrence occ={1,cfg.deadline,7,cfg.deadline+60,2,ALARM_OCC_ACKED,1,0};alarm_occurrence_encode(&occ,blobs[4]);sizes[4]=32;memcpy(occbytes,blobs[4],32);}}
 boot();settle();
 if(state>=4){
  assert(alarms.status.schedules[1].state==ALARM_SCHEDULE_ARMED&&!sizes[4]);
  settings_draft=(twatch_rtc_time_v1){2026,1,1,4,0,state==4?5:6,state==4?30:1};
  assert(snt_save(-1));settle();assert(seed_calls==1&&ledger_writes==2);
  assert(!memcmp(cfgbytes,blobs[1],32));
  alarm_occurrence crossing;assert(sizes[4]==32&&alarm_occurrence_decode(&crossing,blobs[4],32,2));
  assert(crossing.deadline==before+300&&crossing.revision==1&&crossing.generation==1);
  assert(crossing.recovery_until==before+360);
  if(state==4){
   assert(portable_alarm_owned(&alarms)&&alarms.status.schedules[1].state==ALARM_SCHEDULE_DUE);
   assert(crossing.state==ALARM_OCC_PENDING);
   alarm_token_v1 delivered=alarms.status.occurrence;
   assert(delivered.deadline==before+300&&delivered.generation==1);
   assert(alarms.api->acknowledge(NULL,&delivered)==ALARM_PENDING);
   for(unsigned i=0;i<256;i++){assert(portable_alarm_pump(&alarms));if(alarms.status.state==ALARM_STATE_READY)break;}
   assert(alarms.status.state==ALARM_STATE_READY&&!portable_alarm_owned(&alarms));
   assert(alarms.status.schedules[1].state==ALARM_SCHEDULE_DISMISSED);
   assert(alarm_occurrence_decode(&crossing,blobs[4],32,2)&&crossing.state==ALARM_OCC_ACKED);
  }else{
   assert(!portable_alarm_owned(&alarms)&&alarms.status.state==ALARM_STATE_READY);
   assert(alarms.status.schedules[1].state==ALARM_SCHEDULE_EXPIRED&&crossing.state==ALARM_OCC_EXPIRED);
  }
  memcpy(occbytes,blobs[4],32);unsigned writes=occ_writes;
  assert(provider->quiesce());boot();settle();
  assert(!memcmp(cfgbytes,blobs[1],32)&&!memcmp(occbytes,blobs[4],32)&&occ_writes==writes);
  assert(alarms.status.schedules[1].deadline==before+300);
  assert(alarms.status.schedules[1].state==(state==4?ALARM_SCHEDULE_DISMISSED:ALARM_SCHEDULE_EXPIRED));
  printf("PASS forward Settings Save crosses armed deadline: %s, exact UTC config and durable occurrence/status survive restart without duplicate delivery\n",state==4?"within recovery delivers then exact ACK":"outside recovery EXPIRED remains inspectable");
  return 0;
 }
 draft((int)direction);bool ok=snt_save(-1);
 if(state==2){
  assert(!ok&&!seed_calls&&!ledger_writes);assert(portable_alarm_owned(&alarms));
  alarm_token_v1 token=alarms.status.occurrence;assert(alarms.api->acknowledge(NULL,&token)==ALARM_PENDING);
  for(unsigned i=0;i<256;i++){assert(portable_alarm_pump(&alarms));if(alarms.status.state==ALARM_STATE_READY)break;}
  assert(alarms.status.state==ALARM_STATE_READY&&!portable_alarm_owned(&alarms));memcpy(occbytes,blobs[4],32);
  printf("PASS actual Settings/service due alert refuses before seed; exact-token dismissal allows edit, direction=%u\n",direction);
  ok=snt_save(-1);
 }
 if(!ok)fprintf(stderr,"SAVE state=%u direction=%u failed msg=%s error=%d service=%u retained=%u\n",state,direction,editor_message?editor_message:"",alarms.status.error,alarms.status.state,native_custody_retained);
 assert(ok&&seed_calls==1&&ledger_writes==2&&!native_custody_retained);if(state==2)assert(!memcmp(occbytes,blobs[4],32));if(state)assert(!memcmp(cfgbytes,blobs[1],32));if(state==3)assert(!memcmp(occbytes,blobs[4],32)&&!occ_writes);
 alarm_clock_edit_status_v2 edit={.struct_size=sizeof(edit)};assert(alarm_clock_edit_descriptor(alarms.api)->edit_status(NULL,&edit)==0&&edit.phase==ALARM_CLOCK_EDIT_VERIFIED);
 unsigned writes_before_restart=occ_writes;assert(provider->quiesce());boot();settle();assert(occ_writes==writes_before_restart);assert(alarms.status.state==ALARM_STATE_READY&&!alarms.status.error&&!portable_alarm_owned(&alarms));if(state==3)assert(!memcmp(occbytes,blobs[4],32)&&!occ_writes);if(state==2)assert(!memcmp(occbytes,blobs[4],32));if(state)assert(!memcmp(cfgbytes,blobs[1],32));
 /* Unannounced awake jump remains blocked after authorized Save/restart. */
 for(unsigned jump=0;jump<2;jump++) {
  epoch+=jump?3600:-3600;
  for(unsigned i=0;i<256&&alarms.status.state!=ALARM_STATE_BLOCKED;i++){mono+=100;assert(portable_alarm_pump(&alarms));}
  assert(alarms.status.state==ALARM_STATE_BLOCKED&&alarms.status.error==ALARM_RTC);
  /* Only a fresh explicit user Save repairs the guarded clock domain. */
  draft((int)direction);assert(snt_save(-1));assert(seed_calls==jump+2);
  if(state)assert(!memcmp(cfgbytes,blobs[1],32));
  if(state==2||state==3)assert(!memcmp(occbytes,blobs[4],32));
 }
 printf("PASS actual Settings/service state=%u direction=%u multi-field Save, exact records, restart authorization, external forward/back jumps guarded then deliberate Save repairs\n",state,direction);
}
