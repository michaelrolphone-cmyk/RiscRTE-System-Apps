#include "PortableSettingsRuntime.h"
#include "PortableSleepPolicy.h"
#include "PortableDeskClockSettings.h"
#include "RiscPreferenceStoreV1.h"
#include <string.h>
#ifndef ALARM_SERVICE_TAGGED_V2
#error Shared Settings Runtime requires the tagged alarm contract
#endif
#ifndef PORTABLE_SETTINGS_ATOMIC_EXPERIMENTAL
#error Shared Settings Runtime requires clocks-only SetTime entry point
#endif
static void halt(void *v){PortableSettingsRuntime *s=v;if(!s->retained){s->retained=true;s->retain(s->context);}}
static bool live(void *v){PortableSettingsRuntime *s=v;if(!s||s->retained)return false;if(s->guard(s->context)!=PORTABLE_REALTIME_GUARD_SAFE){halt(s);return false;}return true;}
static int guard(void *v){return live(v)?PORTABLE_REALTIME_GUARD_SAFE:PORTABLE_REALTIME_GUARD_RETAINED;}
static bool clock_ok(PortableSettingsRuntime *s,int rc){if(!live(s))return false;if(rc==PORTABLE_REALTIME_UNCERTAIN||rc==PORTABLE_REALTIME_CONTEXT||rc==PORTABLE_REALTIME_RETAINED){halt(s);return false;}return true;}
static bool grant_valid(const risc_runtime_capability_v1 *g){return g->struct_size==sizeof(*g)&&g->slot&&g->generation&&g->api;}
static bool acquire(PortableSettingsRuntime *s,risc_runtime_capability_v1 *g,const char *name,unsigned version,uint64_t instance){
 if(!live(s))return false;g->struct_size=sizeof(*g);bool ok=s->runtime->acquire(name,version,instance,g);
 if(!live(s))return false;
 if(!ok){if(g->api||g->slot||g->generation||g->struct_size!=sizeof(*g))halt(s);return false;}
 if(!grant_valid(g)){halt(s);return false;}return true;
}
static bool release(PortableSettingsRuntime *s,risc_runtime_capability_v1 *g){
 if(!live(s))return false;if(!g->api)return true;bool ok=s->runtime->release(g);
 if(!live(s))return false;if(!ok||g->api||g->slot||g->generation||g->struct_size!=sizeof(*g)){halt(s);return false;}return true;
}
bool psr_policy_valid(const portable_settings_policy_v1 *p){
 if(!p||p->api_version!=1||p->struct_size<sizeof(*p)||p->clock_mode>PSR_CLOCK_RTC||p->storage_mode>PSR_STORE_PREFERENCES)return false;
 const psm_policy *m=&p->model;
 if(m->features&~PSM_ALL_FEATURES||m->writable&~m->features||p->apply_features&~m->features||(p->apply_features&&!p->applied))return false;
 if(m->sleep_modes&~7u||!portable_sleep_supported(m->sleep_modes,m->sleep_default)||m->face_default>=PORTABLE_DESK_FACE_COUNT)return false;
 if(m->idle_default_ms<5000||m->idle_default_ms>3600000||m->idle_default_ms%1000||m->deep_default_ms<60000||m->deep_default_ms>3600000||m->deep_default_ms%1000)return false;
 if(((m->features&PSM_FEATURE(PSM_TIME))!=0)!=(p->clock_mode!=PSR_CLOCK_NONE))return false;
 if(p->clock_mode!=PSR_CLOCK_RTC&&p->rtc_instance)return false;
#if defined(PORTABLE_PREFERENCE_ANY_STORE)
 /* Both contracts are validated against the selected deployment at open. */
#elif defined(PORTABLE_PREFERENCE_STORE)
 if(p->storage_mode==PSR_STORE_ATOMIC)return false;
#else
 if(p->storage_mode==PSR_STORE_PREFERENCES)return false;
#endif
 return true;
}
static int storage(void *v,unsigned *out){
 PortableSettingsRuntime *s=v;if(!live(s))return PSM_OPERATION_RETAINED;
 if(!portable_settings_availability_refresh(&s->availability)){halt(s);return PSM_OPERATION_RETAINED;}
 if(!live(s))return PSM_OPERATION_RETAINED;
 switch(s->availability.state.availability){case RISC_KEY_VALUE_AVAILABLE:*out=PSM_STORAGE_AVAILABLE;break;case RISC_KEY_VALUE_RECONCILIATION_PENDING:*out=PSM_STORAGE_RELOAD_REQUIRED;break;case RISC_KEY_VALUE_CONFLICT:*out=PSM_STORAGE_CONFLICT;break;default:*out=PSM_STORAGE_UNAVAILABLE;}
 return PSM_OK;
}
static int applied(void *v,psm_feature f,const psm_value *value){PortableSettingsRuntime *s=v;if(!live(s))return PSM_OPERATION_RETAINED;if(!(s->policy.apply_features&PSM_FEATURE(f)))return PSM_OK;int rc=s->policy.applied(s->policy.context,f,value);return live(s)?rc:PSM_OPERATION_RETAINED;}
static int begin(void *v){
 PortableSettingsRuntime *s=v;if(!live(s))return PSM_OPERATION_RETAINED;if(s->editing)return PSM_BUSY;
 unsigned state;if(storage(s,&state)!=PSM_OK||state!=PSM_STORAGE_AVAILABLE)return PSM_ERROR;
 if(s->policy.clock_mode==PSR_CLOCK_RTC&&s->atomic_store){
  int rc=portable_preference_atomic_begin(&s->metadata,s->atomic_store,"rtc_basis",PORTABLE_PREFERENCE_USER,live,s);
  if(!live(s)||rc==RISC_ATOMIC_RETAINED){halt(s);return PSM_OPERATION_RETAINED;}if(rc!=PORTABLE_PREFERENCE_READY)return PSM_ERROR;
 }
 int br=portable_rtc_basis_load(s->store,&s->captured_basis);
 if(!live(s))return PSM_OPERATION_RETAINED;
 int zr=portable_timezone_preference_load(s->store,s->captured_zone);
 if(!live(s))return PSM_OPERATION_RETAINED;
 if((br!=PORTABLE_RTC_BASIS_LOADED&&br!=PORTABLE_RTC_BASIS_MISSING)||(zr!=PORTABLE_TIMEZONE_LOADED&&zr!=PORTABLE_TIMEZONE_MISSING))return PSM_ERROR;
 s->editing=true;s->phase=0;memset(&s->result,0,sizeof(s->result));return PSM_OK;
}
static int read_clock(void *v,portable_timezone_civil *out){
 PortableSettingsRuntime *s=v;if(!live(s))return PSM_OPERATION_RETAINED;
 int rc=portable_realtime_open(&s->reader,s->runtime,PORTABLE_REALTIME_READER,PORTABLE_REALTIME_NORMAL_START,guard,s);
 if(!clock_ok(s,rc))return PSM_OPERATION_RETAINED;if(rc!=PORTABLE_REALTIME_OK)return PSM_ERROR;
 risc_realtime_snapshot_v1 sample={0};rc=portable_realtime_read(&s->reader,&sample);
 if(!clock_ok(s,rc))return PSM_OPERATION_RETAINED;
 int closed=portable_realtime_close(&s->reader);if(!clock_ok(s,closed))return PSM_OPERATION_RETAINED;
 if(rc!=PORTABLE_REALTIME_OK||closed!=PORTABLE_REALTIME_OK)return PSM_ERROR;
 char zone[PORTABLE_TIMEZONE_ID_BYTES];rc=portable_timezone_preference_load(s->store,zone);
 if(!live(s))return PSM_OPERATION_RETAINED;if(rc!=PORTABLE_TIMEZONE_LOADED&&rc!=PORTABLE_TIMEZONE_MISSING)return PSM_ERROR;
 portable_timezone_rule rule;if(portable_timezone_resolve(zone,sizeof(zone),&rule)!=PORTABLE_TIMEZONE_OK)return PSM_ERROR;
 return portable_timezone_utc_to_local(&rule,sample.epoch_seconds,out,NULL)==PORTABLE_TIMEZONE_OK?PSM_OK:PSM_ERROR;
}
static int done(PortableSettingsRuntime *s,int rc,portable_set_time_result *out){if(out)*out=s->result;if(rc!=PSM_PENDING&&rc!=PSM_BUSY)s->editing=false;return live(s)?rc:PSM_OPERATION_RETAINED;}
static int poll_clock(void *v,portable_set_time_result *out){
 PortableSettingsRuntime *s=v;if(!live(s))return PSM_OPERATION_RETAINED;if(!s->editing||s->phase!=1)return PSM_ERROR;
 int rc=s->metadata.token?portable_preference_atomic_collect(&s->metadata):portable_preference_atomic_submit(&s->metadata);
 s->result.metadata_write_accepted=s->result.metadata_write_accepted||s->metadata.token!=0;
 if(rc==RISC_ATOMIC_RETAINED){halt(s);return done(s,PSM_OPERATION_RETAINED,out);}
 if(rc==RISC_ATOMIC_PENDING||rc==PORTABLE_PREFERENCE_READY)return done(s,PSM_PENDING,out);
 s->result.metadata_outcome=rc==RISC_ATOMIC_OK?PORTABLE_SET_TIME_CONFIRMED:PORTABLE_SET_TIME_UNCONFIRMED;
 s->result.reason=rc;return done(s,rc==RISC_ATOMIC_OK?PSM_OK:PSM_ERROR,out);
}
static bool transition_equal(const portable_timezone_transition *a,const portable_timezone_transition *b){return a->month==b->month&&a->week==b->week&&a->weekday==b->weekday&&a->seconds==b->seconds;}
static bool rule_equal(const portable_timezone_rule *a,const portable_timezone_rule *b){return a->standard_offset==b->standard_offset&&a->daylight_offset==b->daylight_offset&&a->has_daylight==b->has_daylight&&a->era_year==b->era_year&&transition_equal(&a->start,&b->start)&&transition_equal(&a->end,&b->end)&&transition_equal(&a->era_start,&b->era_start)&&transition_equal(&a->era_end,&b->era_end);}
static int start(void *v,const portable_set_time_request *request,portable_set_time_result *out){
 PortableSettingsRuntime *s=v;if(!live(s))return PSM_OPERATION_RETAINED;if(!s->editing||s->phase)return PSM_ERROR;
 if(request->basis.stores_utc!=s->captured_basis.stores_utc||request->basis.reference_epoch!=s->captured_basis.reference_epoch)return done(s,PSM_ERROR,out);
 portable_timezone_rule captured_rule;
 if(portable_timezone_resolve(s->captured_zone,sizeof(s->captured_zone),&captured_rule)!=PORTABLE_TIMEZONE_OK||!rule_equal(&captured_rule,&request->rule))return done(s,PSM_ERROR,out);
 /* Legacy storage has no revision authority. Recheck the exact interpretation
  * immediately before the irreversible transaction; never repair it. Native
  * clock edits likewise must not reinterpret a draft after a zone change. */
 if(!s->atomic_store||s->policy.clock_mode==PSR_CLOCK_NATIVE){
  portable_rtc_basis current_basis;char current_zone[PORTABLE_TIMEZONE_ID_BYTES];
  int br=portable_rtc_basis_load(s->store,&current_basis);
  int zr=portable_timezone_preference_load(s->store,current_zone);
  if(!live(s))return done(s,PSM_OPERATION_RETAINED,out);
  if((br!=PORTABLE_RTC_BASIS_LOADED&&br!=PORTABLE_RTC_BASIS_MISSING)||(zr!=PORTABLE_TIMEZONE_LOADED&&zr!=PORTABLE_TIMEZONE_MISSING)||current_basis.stores_utc!=s->captured_basis.stores_utc||current_basis.reference_epoch!=s->captured_basis.reference_epoch||strcmp(current_zone,s->captured_zone))return done(s,PSM_ERROR,out);
 }
 s->request=*request;s->request.rtc_instance=s->policy.rtc_instance;
 portable_set_time_plan plan;int rc=portable_set_time_prepare(&s->request,&plan);if(rc!=PORTABLE_REALTIME_OK)return done(s,PSM_ERROR,out);
 s->result.plan=plan;s->result.planned=true;
 unsigned state;if(storage(s,&state)!=PSM_OK||state!=PSM_STORAGE_AVAILABLE)return done(s,PSM_ERROR,out);
 if(s->policy.clock_mode==PSR_CLOCK_NATIVE){
  bool ok=portable_clock_commit_apply(&s->commit,plan.utc_epoch);s->result.native=s->commit.seeded;
  return done(s,ok?PSM_OK:PSM_ERROR,out);
 }
 if(s->atomic_store){
  rc=portable_preference_atomic_check(&s->metadata);if(rc==RISC_ATOMIC_RETAINED){halt(s);return done(s,PSM_OPERATION_RETAINED,out);}if(rc!=RISC_ATOMIC_OK)return done(s,PSM_ERROR,out);
  /* Draft interpretation must match the captured value, not a concurrent reload. */
  if(s->metadata.present){uint8_t *b=s->metadata.expected;if(s->metadata.expected_size!=12||b[0]!=0x52||b[1]!=0x54||b[2]!=1||b[3]>1||portable_rtc_basis_word(b+8)!=portable_rtc_basis_check(b)||request->basis.stores_utc!=(b[3]!=0)||request->basis.reference_epoch!=portable_rtc_basis_word(b+4))return done(s,PSM_ERROR,out);}
  else if(request->basis.stores_utc||request->basis.reference_epoch)return done(s,PSM_ERROR,out);
 }
 if(!portable_clock_commit_begin(&s->commit,plan.utc_epoch))return done(s,PSM_ERROR,out);
 rc=portable_set_time_open(&s->clock,s->runtime,guard,s);
 if(!clock_ok(s,rc))return done(s,PSM_OPERATION_RETAINED,out);
 if(rc!=PORTABLE_REALTIME_OK){(void)portable_clock_commit_cancel_before_seed(&s->commit);return done(s,PSM_ERROR,out);}
 rc=portable_set_time_apply_clocks_confirmed(&s->clock,&s->request,&s->result);
 if(!clock_ok(s,rc))return done(s,PSM_OPERATION_RETAINED,out);
 int closed=portable_set_time_close(&s->clock);if(!clock_ok(s,closed))return done(s,PSM_OPERATION_RETAINED,out);
 if(rc!=PORTABLE_REALTIME_OK||closed!=PORTABLE_REALTIME_OK){
  if(s->result.rtc_write_accepted||s->result.native.attempted)halt(s);else (void)portable_clock_commit_cancel_before_seed(&s->commit);
  return done(s,PSM_ERROR,out);
 }
 if(!portable_clock_commit_finish(&s->commit))return done(s,PSM_ERROR,out);
 if(!s->atomic_store){
  rc=portable_rtc_basis_save(s->store,&s->result.plan.basis);
  s->result.metadata_write_accepted=rc==PORTABLE_RTC_BASIS_SAVED;
  s->result.metadata_outcome=(rc==PORTABLE_RTC_BASIS_SAVED||rc==PORTABLE_RTC_BASIS_UNCHANGED)?PORTABLE_SET_TIME_CONFIRMED:PORTABLE_SET_TIME_UNCONFIRMED;
  s->result.reason=rc;
  return done(s,s->result.metadata_outcome==PORTABLE_SET_TIME_CONFIRMED?PSM_OK:PSM_ERROR,out);
 }
 uint8_t bytes[12]={0x52,0x54,1,(uint8_t)s->result.plan.basis.stores_utc};portable_rtc_basis_put_word(bytes+4,s->result.plan.basis.reference_epoch);portable_rtc_basis_put_word(bytes+8,portable_rtc_basis_check(bytes));
 rc=portable_preference_atomic_prepare(&s->metadata,bytes,sizeof(bytes));if(rc!=PORTABLE_PREFERENCE_READY)return done(s,PSM_ERROR,out);
 s->phase=1;return poll_clock(s,out);
}
static int cancel(void *v,portable_set_time_result *out){
 PortableSettingsRuntime *s=v;if(!live(s))return PSM_OPERATION_RETAINED;if(!s->editing)return done(s,PSM_CANCELLED,out);
 if(s->metadata.token){int rc=portable_preference_atomic_cancel(&s->metadata);if(rc==RISC_ATOMIC_RETAINED){halt(s);return done(s,PSM_OPERATION_RETAINED,out);}if(rc==RISC_ATOMIC_PENDING)return done(s,PSM_PENDING,out);s->result.metadata_outcome=rc==RISC_ATOMIC_OK?PORTABLE_SET_TIME_CONFIRMED:PORTABLE_SET_TIME_UNCONFIRMED;return done(s,rc==RISC_ATOMIC_OK?PSM_OK:rc==RISC_ATOMIC_CANCELLED?PSM_CANCELLED:PSM_ERROR,out);}
 return done(s,PSM_CANCELLED,out);
}
static int kv_result(PortableSettingsRuntime *s,int rc){
 if(!live(s))return RISC_KEY_VALUE_CONTEXT;
 if(rc==RISC_KEY_VALUE_CONTEXT||rc>0||rc<RISC_KEY_VALUE_BUSY){halt(s);return RISC_KEY_VALUE_CONTEXT;}return rc;
}
static int32_t kv_get(void *v,const char *key,void *buffer,uint32_t capacity,uint32_t *size){PortableSettingsRuntime *s=v;if(!live(s)){if(size)*size=0;return RISC_KEY_VALUE_CONTEXT;}return kv_result(s,s->bound_store->get(s->bound_store->context,key,buffer,capacity,size));}
static int32_t kv_put(void *v,const char *key,const void *bytes,uint32_t size){PortableSettingsRuntime *s=v;if(!live(s))return RISC_KEY_VALUE_CONTEXT;if(s->policy.storage_mode!=PSR_STORE_LEGACY)return RISC_KEY_VALUE_INVALID;return kv_result(s,s->bound_store->put(s->bound_store->context,key,bytes,size));}
bool psr_open(PortableSettingsRuntime *s,const risc_runtime_api_v1 *rt,const portable_settings_policy_v1 *policy,portable_realtime_phase_guard g,void (*retain)(void *),void *ctx){
 if(!s||s->runtime||!rt||rt->api_version!=1||rt->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE||!rt->acquire||!rt->release||!g||!retain)return false;
 s->runtime=rt;s->guard=g;s->retain=retain;s->context=ctx;if(!live(s))return false;
 if(!policy){if(!acquire(s,&s->policy_grant,PORTABLE_SETTINGS_POLICY_CAPABILITY,1,0))return false;policy=s->policy_grant.api;}
 if(!psr_policy_valid(policy))return false;s->policy=*policy;
 if(!acquire(s,&s->kv_grant,RISC_KEY_VALUE_CAPABILITY,1,1))return false;s->bound_store=s->kv_grant.api;
 if(!portable_rtc_basis_api(s->bound_store))return false;
 s->guarded_store=(risc_key_value_v1){1,sizeof(s->guarded_store),s,kv_get,kv_put};s->store=&s->guarded_store;
 if(policy->storage_mode!=PSR_STORE_LEGACY){
  if(!acquire(s,&s->atomic_grant,policy->storage_mode==PSR_STORE_ATOMIC?RISC_KEY_VALUE_ATOMIC_CAPABILITY:RISC_PREFERENCE_STORE_CAPABILITY,1,1))return false;
  s->atomic_store=s->atomic_grant.api;if(!portable_preference_atomic_api_valid(s->atomic_store))return false;
  risc_key_value_atomic_state_v1 state={.struct_size=sizeof(state)};int rc=s->atomic_store->state(s->atomic_store->context,&state);
  if(!live(s))return false;
  if(rc==RISC_ATOMIC_RETAINED){halt(s);return false;}
  if(rc!=RISC_ATOMIC_OK||state.reserved||!portable_preference_atomic_state_valid(&state))return false;
  if(policy->storage_mode==PSR_STORE_ATOMIC&&(state.flags&3u)!=3u)return false;
  if(policy->storage_mode==PSR_STORE_PREFERENCES&&!(state.flags&RISC_PREFERENCE_STORE_COPIED_INPUT))return false;
 }
 if(!acquire(s,&s->availability.grant,RISC_KEY_VALUE_AVAILABILITY_CAPABILITY,1,1))return false;
 s->availability.api=s->availability.grant.api;s->availability.state=(risc_key_value_availability_state_v1){.struct_size=sizeof(s->availability.state),.namespace_id=1};
 if(s->availability.api->api_version!=1||s->availability.api->struct_size<sizeof(*s->availability.api)||!s->availability.api->snapshot)return false;
 unsigned st;if(storage(s,&st)!=PSM_OK)return false;
 if(policy->clock_mode!=PSR_CLOCK_NONE){
  if(!acquire(s,&s->alarms.grant,ALARM_SERVICE_CAPABILITY,ALARM_SERVICE_API_V2,0))return false;s->alarms.api=s->alarms.grant.api;
  if(!alarm_clock_edit_descriptor(s->alarms.api))return false;
 }
 s->commit=(portable_clock_commit){.alarms=&s->alarms,.runtime=rt,.guard=guard,.retain=halt,.context=s};
 s->services=(psm_services){.context=s,.live=live,.clock_begin=begin,.clock_read=read_clock,.clock_start=start,.clock_poll=poll_clock,.clock_cancel=cancel,.storage_state=storage,.applied=applied};
 s->opened=true;return true;
}
bool psr_init_model(PortableSettingsRuntime *s,PortableSettingsModel *m){return s&&s->opened&&live(s)&&psm_init(m,&s->policy.model,&s->services,s->store,s->atomic_store);}
bool psr_close(PortableSettingsRuntime *s,const PortableSettingsModel *m){
 if(!s||!s->runtime||!live(s)||s->editing||s->metadata.token||(m&&!psm_can_handoff(m)))return false;
 if(!release(s,&s->alarms.grant)||!release(s,&s->availability.grant)||!release(s,&s->atomic_grant)||!release(s,&s->kv_grant)||!release(s,&s->policy_grant))return false;
 memset(s,0,sizeof(*s));return true;
}

bool psr_reload(PortableSettingsRuntime *s,PortableSettingsModel *m){
 if(!s||!m||!s->opened||(m->route!=PSM_ROOT&&m->route!=PSM_STORAGE_ROUTE)||m->dirty||!psm_can_handoff(m)||!live(s))return false;
 const risc_runtime_api_v1 *rt=s->runtime;portable_realtime_phase_guard g=s->guard;void (*retain)(void *)=s->retain;void *context=s->context;
 portable_settings_policy_v1 policy=s->policy;bool provider=s->policy_grant.api!=NULL;
 if(!psr_close(s,m))return false;
 m->phase=PSM_RETAINED; /* Old borrowed tables cannot be used after release. */
 if(!psr_open(s,rt,provider?NULL:&policy,g,retain,context))return false;
 return psr_init_model(s,m);
}
