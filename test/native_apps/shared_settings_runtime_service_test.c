#include "PortableSettingsRuntime.h"
#include "RiscPreferenceStoreV1.h"
#include "PortableRtcClock.h"
#include <stdlib.h>
#include <stdio.h>
#include "AlarmRecords.h"
#include "RiscProviderV2.h"
#include "RiscBoundKeyValueV1.h"
#include "RiscPlatformClockV1.h"
#include "RiscPlatformRealtimeV1.h"
#include <assert.h>
extern const risc_driver_v2 *t5_driver_get(uint32_t);
static PortableSettingsRuntime backend;
static portable_alarm_client alarms;
static bool native_custody_retained;
static const char *editor_message;
static portable_timezone_civil settings_draft;
static int phase_guard(void *c){(void)c;return native_custody_retained?PORTABLE_REALTIME_GUARD_RETAINED:PORTABLE_REALTIME_GUARD_SAFE;}
static void retain(void *c){(void)c;native_custody_retained=true;}
static const risc_driver_v2 *provider;
static unsigned fault,rtc_writes,rtc_reads;static char preference_key[16]="time_format";
static uint8_t preference[64];static uint32_t preference_size,atomic_flags=3;static uint64_t revision=1;static bool token_live;static unsigned release_calls;
static int32_t pref_get(void *c,const char *k,void *b,uint32_t cap,uint32_t *n){(void)c;*n=0;if(strcmp(k,preference_key)||!preference_size)return RISC_KEY_VALUE_NOT_FOUND;if(cap<preference_size)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(b,preference,preference_size);*n=preference_size;return 0;}
static int32_t pref_put(void *c,const char *k,const void *b,uint32_t n){(void)c;assert(n<=64);strcpy(preference_key,k);memcpy(preference,b,n);preference_size=n;return 0;}
static int32_t atomic_state(void *c,risc_key_value_atomic_state_v1 *out){(void)c;*out=(risc_key_value_atomic_state_v1){sizeof(*out),atomic_flags,1,1,64,512,176,0,revision};return 0;}
static int32_t atomic_get(void *c,uint64_t r,const char *k,void *b,uint32_t cap,uint32_t *n){assert(r==revision);return pref_get(c,k,b,cap,n)==RISC_KEY_VALUE_NOT_FOUND?RISC_ATOMIC_NOT_FOUND:0;}
static int32_t atomic_submit(void *c,uint64_t r,const risc_key_value_atomic_mutation_v1 *m,uint32_t count,const void *intent,uint32_t n,uint64_t *token){(void)c;assert(r==revision&&count==1&&n==176&&intent&&!token_live);if(fault==8){*token=0;return RISC_ATOMIC_STALE;}strcpy(preference_key,m->key);memcpy(preference,m->value,m->size);preference_size=m->size;token_live=true;*token=9;return 0;}
static int32_t atomic_collect(void *c,uint64_t token,risc_key_value_atomic_receipt_v1 *out){(void)c;assert(token==9&&token_live);token_live=false;*out=(risc_key_value_atomic_receipt_v1){sizeof(*out),fault==3?RISC_ATOMIC_UNCERTAIN:0,++revision,fault==3?0:77};return 0;}
static int32_t atomic_cancel(void *c,uint64_t token){(void)c;assert(token==9);return 0;}
static const risc_key_value_atomic_v1 atomic_api={1,sizeof(atomic_api),NULL,atomic_state,atomic_get,atomic_submit,atomic_collect,atomic_cancel};
static const risc_key_value_v1 prefs={1,sizeof(prefs),NULL,pref_get,pref_put};
static int32_t available(void *c,risc_key_value_availability_state_v1 *s){(void)c;*s=(risc_key_value_availability_state_v1){sizeof(*s),1,RISC_KEY_VALUE_AVAILABLE,0,1};return 0;}
static const risc_key_value_availability_v1 availability={1,sizeof(availability),NULL,available};
static int64_t epoch=1767225600;
static uint64_t mono;
static unsigned seed_calls,occ_writes,ledger_writes;
static uint8_t blobs[7][512];static uint32_t sizes[7];
static const char*keys[]={ALARM_CONFIG_KEY,ALARM_TIMER_KEY,ALARM_MODE_KEY,ALARM_OCCURRENCE_KEY,ALARM_TIMER_OCCURRENCE_KEY,"alert_dnd",ALARM_CLOCK_EDIT_KEY};
static int key(const char*k){for(int i=0;i<7;i++)if(!strcmp(k,keys[i]))return i;fprintf(stderr,"Unknown key %s\n",k);assert(0);return -1;}
static int32_t bound_get(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){assert(c==blobs);int i=key(k);*n=0;if(!sizes[i])return -1;if(cap<sizes[i])return -2;*n=sizes[i];memcpy(b,blobs[i],*n);return 0;}
static int32_t bound_put(void*c,const char*k,const void*b,uint32_t n){assert(c==blobs);int i=key(k);assert(i==3||i==4||i==6);assert(n<=512);memcpy(blobs[i],b,n);sizes[i]=n;if(i==6)ledger_writes++;else occ_writes++;return 0;}
static uint64_t time_ms(void*c){assert(c==&mono);return mono;}
static int32_t native_read(void*c,risc_realtime_snapshot_v1*out){assert(c==&epoch);*out=(risc_realtime_snapshot_v1){.struct_size=sizeof(*out),.epoch_seconds=epoch,.validity=RISC_REALTIME_VALID,.monotonic_before_us=mono*1000,.monotonic_after_us=mono*1000};return 0;}
static int32_t native_seed(void*c,int64_t e,uint32_t us){assert(c==&epoch&&!us);seed_calls++;epoch=e;return fault==5?RISC_REALTIME_IO:0;}
static risc_realtime_control_api_v1 control={1,sizeof(control),&epoch,native_read,native_seed};
static twatch_rtc_time_v1 rtc_calendar;
static bool rtc_read(void *c,twatch_rtc_time_v1 *v){(void)c;rtc_reads++;*v=rtc_calendar;if(fault==9)v->minute=(v->minute+2)%60;return true;}
static bool rtc_write(void *c,const twatch_rtc_time_v1 *v){(void)c;rtc_writes++;rtc_calendar=*v;if(fault==6)native_custody_retained=true;return fault!=4;}
static const twatch_rtc_api_v1 rtc_api={.api_version=TWATCH_RTC_API_V1,.struct_size=sizeof(rtc_api),.read=rtc_read,.write=rtc_write};
static bool acquire(const char*cap,uint32_t v,uint64_t instance,risc_runtime_capability_v1*out){
 assert(v==1||v==2);const void *api=NULL;
 if(!strcmp(cap,"runtime.realtime-control")){assert(v==1&&!instance);api=&control;}
 else if(!strcmp(cap,TWATCH_RTC_CAPABILITY)){assert(v==TWATCH_RTC_API_V1&&instance==42);api=&rtc_api;}
 else if(!strcmp(cap,RISC_KEY_VALUE_CAPABILITY)){assert(v==1&&instance==1);api=&prefs;}
 else if(!strcmp(cap,RISC_KEY_VALUE_AVAILABILITY_CAPABILITY)){assert(v==1&&instance==1);api=&availability;}
 else if(!strcmp(cap,RISC_KEY_VALUE_ATOMIC_CAPABILITY)||!strcmp(cap,RISC_PREFERENCE_STORE_CAPABILITY)){assert(v==1&&instance==1);api=&atomic_api;}
 else if(!strcmp(cap,ALARM_SERVICE_CAPABILITY)){assert(v==2&&!instance);api=provider->capability;}
 else {assert(0);}
 out->api=api;out->slot=1;out->generation=1;return true;
}
static bool release(risc_runtime_capability_v1*g){release_calls++;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static risc_runtime_api_v1 runtime;
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){return v==1?&runtime:NULL;}
static const risc_bound_key_value_v1 kv={2,sizeof(kv),blobs,bound_get,bound_put};
static const risc_platform_clock_api_v1 clock_table={1,sizeof(clock_table),&mono,time_ms,NULL};
static const risc_platform_realtime_api_v1 real_table={1,sizeof(real_table),&epoch,native_read};
static const risc_provider_dependency_v1 deps[]={{"storage.key-value.bound",2,&kv},{"platform.clock",1,&clock_table},{"platform.realtime",1,&real_table}};

static void boot(void){provider=t5_driver_get(2);assert(provider->start(deps,3));alarms=(portable_alarm_client){0};alarms.api=provider->capability;}
static void settle(void){for(unsigned i=0;i<256;i++){assert(portable_alarm_pump(&alarms));if(alarms.status.state==ALARM_STATE_READY||portable_alarm_owned(&alarms)||alarms.status.state==ALARM_STATE_BLOCKED)return;}assert(0);}
static bool snt_save(int fold){
 if(backend.services.clock_begin(&backend)!=PSM_OK)return false;
 portable_set_time_request r={.local=settings_draft,.basis={false,0},.fold_choice=fold,.native_readback_budget_us=1000000};
 assert(portable_timezone_resolve("UTC",4,&r.rule)==0);
 portable_set_time_result result;int rc=backend.services.clock_start(&backend,&r,&result);
 if(rc==PSM_OK)alarms.status=backend.alarms.status;editor_message=backend.commit.message;if(rc!=PSM_OK)fprintf(stderr,"save rc=%d retained=%d message=%s\n",rc,backend.retained,editor_message);return rc==PSM_OK;
}
static void draft(int which){const portable_timezone_civil values[]={{2027,2,3,4,5,6,3},{2025,12,30,23,58,57,2},{2026,1,2,0,1,2,5},{2025,12,31,20,30,40,3}};settings_draft=values[which];}
static void storage_contracts(void){
 for(unsigned mode=0;mode<3;mode++){
  preference_size=0;token_live=false;revision=1;atomic_flags=mode==2?4:3;
  portable_settings_policy_v1 p={.api_version=1,.struct_size=sizeof(p),.model=psm_default_policy(),.storage_mode=mode};
  p.model.features=p.model.writable=PSM_FEATURE(PSM_FORMAT);
  assert(psr_open(&backend,&runtime,&p,phase_guard,retain,NULL));PortableSettingsModel model;assert(psr_init_model(&backend,&model));
  assert(psm_open(&model,PSM_FORMAT)&&psm_set_number(&model,1)&&psm_save(&model));
  assert(!preference_size&&!token_live);assert(psm_presented(&model,model.save_revision));psm_tick(&model);
  if(mode){assert(token_live);unsigned calls=release_calls;assert(!psr_close(&backend,&model)&&release_calls==calls);psm_tick(&model);}
  assert(model.phase==PSM_IDLE&&model.last_result==PSM_OK&&preference_size==4&&!token_live);
  assert(psr_reload(&backend,&model));assert(model.confirmed[PSM_FORMAT].number==1);assert(psr_close(&backend,&model));
 }
 portable_settings_policy_v1 p={.api_version=1,.struct_size=sizeof(p),.model=psm_default_policy(),.storage_mode=PSR_STORE_ATOMIC};p.model.features=p.model.writable=PSM_FEATURE(PSM_FORMAT);
 atomic_flags=4;assert(!psr_open(&backend,&runtime,&p,phase_guard,retain,NULL));assert(psr_close(&backend,NULL));
 p.storage_mode=PSR_STORE_PREFERENCES;atomic_flags=3;assert(!psr_open(&backend,&runtime,&p,phase_guard,retain,NULL));assert(psr_close(&backend,NULL));
 p.storage_mode=PSR_STORE_ATOMIC;atomic_flags=3;fault=3;
 assert(psr_open(&backend,&runtime,&p,phase_guard,retain,NULL));PortableSettingsModel uncertain;assert(psr_init_model(&backend,&uncertain));
 assert(psm_open(&uncertain,PSM_FORMAT)&&psm_set_number(&uncertain,0)&&psm_save(&uncertain));assert(psm_presented(&uncertain,uncertain.save_revision));psm_tick(&uncertain);psm_tick(&uncertain);
 assert(uncertain.phase==PSM_IDLE&&uncertain.uncertain&&uncertain.dirty&&!token_live);psm_cancel(&uncertain);
 assert(uncertain.dirty&&!psr_reload(&backend,&uncertain));unsigned calls=release_calls;uint64_t before_revision=revision;uint8_t before[64];memcpy(before,preference,sizeof(before));
 assert(psr_close(&backend,&uncertain)&&release_calls>calls&&revision==before_revision&&!memcmp(before,preference,sizeof(before)));fault=0;
 puts("PASS terminal unconfirmed dirty draft closes without write, rollback or reload; no outstanding custody");
 puts("PASS all three semantic storage modes, exact namespace, staged save, pending-close fence, explicit reload, selected-contract mismatch refusal");
}
static void rtc_contracts(unsigned scenario){
 fault=scenario;boot();settle();
 portable_settings_policy_v1 p={.api_version=1,.struct_size=sizeof(p),.model=psm_default_policy(),.storage_mode=PSR_STORE_ATOMIC,.clock_mode=PSR_CLOCK_RTC,.rtc_instance=42};
 assert(psr_open(&backend,&runtime,&p,phase_guard,retain,NULL));
 assert(backend.services.clock_begin(&backend)==PSM_OK);portable_set_time_result result={0};
 if(scenario==1){assert(backend.services.clock_cancel(&backend,&result)==PSM_CANCELLED);assert(!rtc_writes&&!seed_calls);assert(psr_close(&backend,NULL));puts("PASS RTC cancel before write");return;}
 portable_set_time_request request={.local={2027,2,3,4,5,6,0},.basis={false,0},.fold_choice=-1,.native_readback_budget_us=1000000};assert(portable_timezone_resolve("UTC",4,&request.rule)==0);
 if(scenario==7)revision++;
 int rc=backend.services.clock_start(&backend,&request,&result);
 if(scenario==4||scenario==5||scenario==6||scenario==9){assert(rc==PSM_OPERATION_RETAINED&&backend.retained&&!token_live);unsigned releases=release_calls;assert(!psr_close(&backend,NULL)&&release_calls==releases);if(scenario!=5)assert(!seed_calls);puts("PASS RTC/native failure retains partial custody without cleanup or metadata");return;}
 if(scenario==7){assert(rc==PSM_ERROR&&!rtc_writes&&!seed_calls);assert(psr_close(&backend,NULL));puts("PASS RTC stale editor rejected before writes");return;}
 if(scenario==8){assert(rc==PSM_ERROR&&result.rtc_outcome==PORTABLE_SET_TIME_CONFIRMED&&result.native.verified&&!result.metadata_write_accepted&&!token_live);assert(psr_close(&backend,NULL));puts("PASS RTC clocks verified; stale metadata independently unconfirmed");return;}
 assert(rc==PSM_PENDING&&token_live&&result.rtc_outcome==PORTABLE_SET_TIME_CONFIRMED&&result.native.verified&&result.metadata_outcome==PORTABLE_SET_TIME_NOT_ATTEMPTED);
 assert(!psr_close(&backend,NULL));
 rc=scenario==2?backend.services.clock_cancel(&backend,&result):backend.services.clock_poll(&backend,&result);
 assert(rc==(scenario==3?PSM_ERROR:PSM_OK)&&!token_live&&result.metadata_write_accepted);
 assert(result.metadata_outcome==(scenario==3?PORTABLE_SET_TIME_UNCONFIRMED:PORTABLE_SET_TIME_CONFIRMED));
 assert(rtc_writes==1&&rtc_reads==1&&seed_calls==1);assert(psr_close(&backend,NULL));puts("PASS RTC physical/native verification then independent atomic metadata terminal receipt/cancel race");
}
int main(int argc,char**argv){assert(argc==3);unsigned state=(unsigned)atoi(argv[1]),direction=(unsigned)atoi(argv[2]);assert(state<8&&direction<10);
 runtime=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release};if(state==6){storage_contracts();return 0;}if(state==7){rtc_contracts(direction);return 0;}
 const uint32_t before=(uint32_t)(epoch-946684800);uint8_t cfgbytes[32]={0},occbytes[32]={0};
 if(state){alarm_config cfg={1,before+(state==2?0:300),before-20,state==2?20:320,2,1};alarm_config_encode(&cfg,blobs[1]);sizes[1]=32;memcpy(cfgbytes,blobs[1],32);if(state==3){alarm_occurrence occ={1,cfg.deadline,7,cfg.deadline+60,2,ALARM_OCC_ACKED,1,0};alarm_occurrence_encode(&occ,blobs[4]);sizes[4]=32;memcpy(occbytes,blobs[4],32);}}
 boot();settle();
 portable_settings_policy_v1 policy={.api_version=1,.struct_size=sizeof(policy),.model=psm_default_policy(),.clock_mode=PSR_CLOCK_NATIVE,.storage_mode=PSR_STORE_LEGACY};
 assert(psr_open(&backend,&runtime,&policy,phase_guard,retain,NULL));
 if(state>=4){
  assert(alarms.status.schedules[1].state==ALARM_SCHEDULE_ARMED&&!sizes[4]);
  settings_draft=(portable_timezone_civil){2026,1,1,0,state==4?5:6,state==4?30:1,4};
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
 assert(psr_close(&backend,NULL));
 printf("PASS shared Settings Runtime/service state=%u direction=%u multi-field Save, exact records, restart authorization\n",state,direction);
}
