/* Selected demand-root Clock only. This file is absent from default builds.
 * Retained scenes use UTC minutes plus explicit app-owned timezone metadata.
 * Local guards fence known failures; they do not attest Runtime graph safety. */
#if !defined(PORTABLE_DESK_CLOCK) || !defined(PORTABLE_ALARM_CLIENT) || !defined(PORTABLE_APP_SLEEP_LOCAL)
#error "Sparse Clock requires desk Clock, alarms and deployment-local sleep"
#endif
#include "PortableDeskClockApp.h"
#include "PortableDeskClockFaces.h"
#include "PortableDeskClockSettings.h"
#include "PortableRealtimeClient.h"
#include "PortableTimeZonePreference.h"
#include "RiscProviderPromotionV1.h"
#include "../lib/PortableApps/boot_logo/RiscRteLogo.h"
#define DESK_PREPARATION_BUDGET_US UINT32_C(65000000)
#define DESK_PROMOTION_ATTEMPTS 2u
static portable_desk_record desk_boot_record;
static portable_desk_config desk_configuration;
static portable_timezone_rule desk_rule;
static portable_rtc_basis desk_basis;
static bool desk_zone_known,desk_basis_known,desk_boot_timer,desk_returned,desk_retained;
static bool desk_radios_off,desk_held,desk_promoted,desk_promotion_attempted,desk_first_promotion;
static const portable_desk_sleep_ops *desk_ops;
static unsigned desk_rectangles,desk_sleep_mode;
static uint32_t desk_started_at;
static int desk_fence(void) {
 desk_retained=true;portable_desk_adapter_retain();return -2;
}
/* Stage diagnostics use only this boot's monotonic milliseconds. They are
 * never emitted with holds, uncertain custody, or a retained result. */
static bool desk_trace(const char *stage,int status) {
 if(desk_held || desk_retained || portable_app_sleep_retained())return false;
 risc_runtime_health_v1 now={.struct_size=sizeof(now)};char line[112];
 if(!rt->health(&now)){desk_fence();return false;}
 snprintf(line,sizeof(line),"SPARSE_CLOCK stage=%s status=%d elapsed_ms=%lu",stage,status,
          (unsigned long)(uint32_t)(now.uptime_ms-desk_started_at));
 if(!rt->diagnostic(line)){desk_fence();return false;}
 return true;
}
static int desk_phase_guard(void *unused) {
 (void)unused;
 if(desk_retained || portable_app_sleep_retained())return PORTABLE_REALTIME_GUARD_RETAINED;
 return desk_held?PORTABLE_REALTIME_GUARD_UNCONFIRMED:PORTABLE_REALTIME_GUARD_SAFE;
}
static bool desk_halted(int status) {
 return status==PORTABLE_REALTIME_CONTEXT || status==PORTABLE_REALTIME_UNCERTAIN ||
        status==PORTABLE_REALTIME_RETAINED;
}
/* Every native client is invocation-local and cleanly closed before returning.
 * A retained client must not be released, retried, or replaced by another one. */
static int desk_sample(bool recover,risc_realtime_snapshot_v1 *out) {
 if(desk_phase_guard(NULL)!=PORTABLE_REALTIME_GUARD_SAFE)return desk_fence();
 portable_realtime_client client={0};
 int status=portable_realtime_open(&client,rt,PORTABLE_REALTIME_CONTROL,
     desk_promoted?PORTABLE_REALTIME_NORMAL_START:PORTABLE_REALTIME_TIMER_ONLY,
     desk_phase_guard,NULL);
 if(desk_halted(status))return desk_fence();
 if(status!=PORTABLE_REALTIME_OK)return status;
 if(recover && desk_promoted && desk_zone_known && desk_basis_known) {
  portable_realtime_recovery_policy policy={.rtc_instance=0,.rule=desk_rule,
      .basis=desk_basis,.fold_choice=-1,.allow_basis_change=false};
  portable_realtime_recovery_result result;
  status=portable_realtime_recover_rtc(&client,&policy,&result);
  if(status==PORTABLE_REALTIME_OK || status==PORTABLE_REALTIME_RECOVERED)*out=result.snapshot;
 } else status=portable_realtime_read(&client,out);
 if(desk_halted(status))return desk_fence();
 int closed=portable_realtime_close(&client);
 if(closed!=PORTABLE_REALTIME_OK)return desk_fence();
 return status==PORTABLE_REALTIME_RECOVERED?PORTABLE_REALTIME_OK:status;
}
static bool desk_local(const portable_desk_config *config,int64_t utc,twatch_rtc_time_v1 *out) {
 portable_timezone_rule rule;portable_timezone_civil local;
 if(portable_timezone_resolve(config->time_zone,sizeof(config->time_zone),&rule)!=PORTABLE_TIMEZONE_OK ||
    portable_timezone_utc_to_local(&rule,utc,&local,NULL)!=PORTABLE_TIMEZONE_OK ||
    local.year<0 || local.year>65535)return false;
 *out=(twatch_rtc_time_v1){(uint16_t)local.year,local.month,local.day,local.weekday,
                         local.hour,local.minute,local.second};return true;
}
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
static int64_t desk_home_epoch;
static bool desk_home_valid;
#endif
static bool read_clock(twatch_rtc_time_v1 *out) {
 risc_realtime_snapshot_v1 sample;
 bool valid=desk_zone_known && desk_sample(false,&sample)==PORTABLE_REALTIME_OK &&
        desk_local(&desk_configuration,sample.epoch_seconds,out);
#ifdef PORTABLE_HOME_POINTS_NATIVE_UTC
 desk_home_valid=valid;
 if(valid)desk_home_epoch=sample.epoch_seconds;
#endif
 return valid;
}
bool portable_desk_clock_time(uint8_t *hour,uint8_t *minute) {
 twatch_rtc_time_v1 local;
 if(!hour || !minute || !read_clock(&local))return false;
 *hour=local.hour;*minute=local.minute;return true;
}
static void close_clock(void) { /* No persistent clock/battery grants. */ }
static void open_clock(void) { /* Each read takes and closes one native grant. */ }
void portable_desk_clock_refused(void) {desk_returned=true;desk_radios_off=false;}
void portable_desk_clock_radios_off(void) {desk_radios_off=true;}
static const char *desk_notice(void) {
 return desk_radios_off?"DESK CLOCK STOPPED; RADIOS OFF":"DESK CLOCK DID NOT SLEEP";
}
/* Absent records use Reader defaults; invalid/unavailable records never do.
 * No storage repair, chip choice or RTC write occurs here. */
static bool desk_config(portable_desk_config *out,unsigned *mode) {
 if(!desk_promoted || desk_held || desk_retained)return false;
 portable_desk_config config={0};
 risc_runtime_capability_v1 grant={.struct_size=sizeof(grant)};
 if(!rt->acquire(RISC_KEY_VALUE_CAPABILITY,1,1,&grant)){desk_fence();return false;}
 if(!grant.api || !grant.slot || !grant.generation){desk_fence();return false;}
 unsigned face,time_format,language,flip;
 portable_perf_count(PORTABLE_PERF_KV_READS);
 (void)portable_time_format_load(grant.api,&time_format);
 portable_perf_count(PORTABLE_PERF_KV_READS);
 (void)portable_desk_face_load(grant.api,&face);
 portable_perf_count(PORTABLE_PERF_KV_READS);
 (void)portable_reader_preference_load(grant.api,true,&language);
 portable_perf_count(PORTABLE_PERF_KV_READS);
 (void)portable_reader_preference_load(grant.api,false,&flip);
 portable_perf_count(PORTABLE_PERF_KV_READS);
 (void)portable_sleep_load_profile(grant.api,3u,PORTABLE_SLEEP_LIGHT,mode);
 portable_perf_count(PORTABLE_PERF_KV_READS);
 int zone=portable_timezone_preference_load(grant.api,config.time_zone);
 portable_perf_count(PORTABLE_PERF_KV_READS);
 int basis=portable_rtc_basis_load(grant.api,&desk_basis);
 if(!rt->release(&grant) || grant.api || grant.slot || grant.generation){desk_fence();return false;}
 desk_zone_known=(zone==PORTABLE_TIMEZONE_LOADED || zone==PORTABLE_TIMEZONE_MISSING) &&
     portable_timezone_resolve(config.time_zone,sizeof(config.time_zone),&desk_rule)==PORTABLE_TIMEZONE_OK;
 desk_basis_known=basis==PORTABLE_RTC_BASIS_LOADED || basis==PORTABLE_RTC_BASIS_MISSING;
 if(!desk_zone_known)memset(config.time_zone,0,sizeof(config.time_zone));
 config.face=(uint8_t)face;config.time_format=(uint8_t)time_format;
 config.language=(uint8_t)language;config.flip_ui=(uint8_t)flip;
 if(desk_basis_known){config.rtc_stores_utc=desk_basis.stores_utc;config.rtc_reference_epoch=desk_basis.reference_epoch;}
 *out=config;return true;
}
int portable_desk_clock_mode(void) {
 /* TIMER adapter bypasses this function. No promotion attempt can re-admit it. */
 if(!desk_promoted)return PORTABLE_SLEEP_LIGHT;
 return desk_config(&desk_configuration,&desk_sleep_mode)?(int)desk_sleep_mode:-2;
}
static bool desk_cancelled(void) {
 if(desk_retained || !portable_desk_adapter_ready() || !desk_ops)return true;
 int status=desk_ops->cancelled(desk_ops->context);
 if(status==-2)desk_fence();
 return status!=0;
}
static bool desk_rect(void *unused,int x,int y,int w,int h,bool black) {
 (void)unused;
 if((desk_rectangles++&63u)==0 && desk_cancelled())return false;
 app->fill_rect(x,y,w,h,black);return portable_desk_adapter_ready();
}
static bool desk_scene(const portable_desk_config *config,int64_t minute) {
 twatch_rtc_time_v1 time={0};
 bool valid=minute>=PORTABLE_TIMEZONE_VALID_EPOCH && desk_local(config,minute,&time);
 if(!portable_desk_adapter_flip(config->flip_ui))return false;
 portable_desk_adapter_begin();desk_rectangles=0;
 portable_desk_canvas canvas={NULL,app->screen_width(),app->screen_height(),desk_rect};
 if(!portable_desk_draw_face(&canvas,config->face,time.hour,time.minute,
     config->time_format==PORTABLE_TIME_FORMAT_12,valid))return false;
 char date[32];if(valid)snprintf(date,sizeof(date),"%04u-%02u-%02u",time.year,time.month,time.day);
 portable_desk_adapter_caption(32,valid?date:portable_reader_clock_set_time(config->language));
 if(valid && config->time_format==PORTABLE_TIME_FORMAT_12)
  portable_desk_adapter_caption(canvas.height-78,time.hour<12?"AM":"PM");
 portable_desk_adapter_caption(canvas.height-44,portable_reader_clock_wake(config->language));
 return !desk_cancelled();
}
static int desk_restore(void) {
 if(desk_retained)return -2;
 if(desk_ops->resume(desk_ops->context)!=1)return desk_fence();
 desk_held=false;portable_desk_adapter_invalidate();return 0;
}
/* Pinned Runtime health uses the same esp_timer boot domain as realtime,
 * rounded down to milliseconds. Use its upper microsecond endpoint; at most
 * 999 us quantization error plus the native sampling bracket remains. Low-word
 * arithmetic avoids a target 64-bit divide and handles the millis wrap. */
static bool desk_project(const risc_realtime_snapshot_v1 *sample,
                         portable_realtime_estimate *out,uint32_t *sampled_at) {
 risc_runtime_health_v1 health={.struct_size=sizeof(health)};
 if(!rt->health(&health)){desk_fence();return false;}
 if(sample->monotonic_after_us-sample->monotonic_before_us>100000u)return false;
 uint32_t elapsed=health.uptime_ms*1000u+999u-(uint32_t)sample->monotonic_after_us;
 if(portable_realtime_project(sample,elapsed,DESK_PREPARATION_BUDGET_US,out)!=PORTABLE_REALTIME_OK)return false;
 *sampled_at=health.uptime_ms;return true;
}
int portable_desk_clock_run(const risc_runtime_api_v1 *runtime,const portable_desk_sleep_ops *ops) {
 if(!runtime || !ops || !ops->cancelled || !ops->stage || !ops->prepare ||
    !ops->resume || !ops->stage_enter || desk_retained || desk_held)return 0;
 if(portable_desk_adapter_timer_only() && desk_promotion_attempted)return 0;
 desk_ops=ops;desk_returned=true;desk_radios_off=false;
 portable_desk_config config=desk_boot_timer?desk_boot_record.config:desk_configuration;
 if(!desk_boot_timer && (!desk_promoted || desk_sleep_mode!=PORTABLE_SLEEP_DEEP || !desk_zone_known))return 0;
 portable_desk_cycle cycle;
 if(!portable_desk_begin(&cycle,&config,&desk_boot_record,desk_boot_timer))return 0;
 desk_boot_timer=false;
 for(unsigned attempt=0;attempt<PORTABLE_DESK_CLOCK_MAX_PRESENTS;attempt++) {
  risc_realtime_snapshot_v1 sample;portable_desk_frame frame;
  if(desk_cancelled() || desk_sample(false,&sample)!=PORTABLE_REALTIME_OK ||
     !portable_desk_plan_frame(&cycle,sample.epoch_seconds,&frame))break;
  if(frame.has_previous) {
   if(!desk_scene(&cycle.current.config,frame.previous_minute))break;
   int seeded=portable_desk_adapter_seed();if(seeded<0)return desk_fence();
   if(!seeded)frame.full=true;
  }
  if(!desk_scene(&config,frame.minute))break;
  if(!portable_desk_presented(&cycle,portable_desk_adapter_present(frame.full)))return desk_fence();
  if(desk_cancelled() || desk_sample(false,&sample)!=PORTABLE_REALTIME_OK)break;
  portable_desk_record proposal;uint32_t duration=0,sampled_at=0;
  portable_realtime_estimate now;
  if(!desk_project(&sample,&now,&sampled_at))break;
  int plan=portable_desk_prepare_record(&cycle,now.epoch_seconds,(int32_t)(now.nanoseconds/1000u),&proposal,&duration);
  if(plan==PORTABLE_DESK_REPAINT)continue;
  if(plan!=PORTABLE_DESK_READY)break;
  int staged=ops->stage(ops->context,&proposal);
  if(staged==-2)return desk_fence();
  if(staged!=1)break;
  /* Every native/control grant is closed. From here through confirmed resume,
   * no native read/recovery/release, KV I/O, or cooperative yield is allowed. */
  desk_held=true;
  int prepared=ops->prepare(ops->context);
  if(prepared==-2)return desk_fence();
  if(prepared!=1){desk_held=false;break;} /* hook already restored */
  if(desk_cancelled() || !desk_project(&sample,&now,&sampled_at))return desk_restore();
  plan=portable_desk_prepare_record(&cycle,now.epoch_seconds,(int32_t)(now.nanoseconds/1000u),&proposal,&duration);
  if(plan==PORTABLE_DESK_READY && !desk_cancelled()) {
   if(ops->stage_enter(ops->context,&proposal,duration,sampled_at)!=0)return desk_fence();
   return desk_restore();
  }
  if(desk_restore()==-2)return -2;
  if(plan!=PORTABLE_DESK_REPAINT)break;
  cycle.full=true;
 }
 if(desk_retained || !portable_desk_adapter_ready())return desk_fence();
 portable_desk_adapter_invalidate();return 0;
}
static int desk_promote(void) {
 desk_promotion_attempted=true;desk_boot_timer=false;
 risc_runtime_capability_v1 grant={.struct_size=sizeof(grant)};
 if(!rt->acquire(RISC_PROVIDER_PROMOTION_CAPABILITY,1,0,&grant))return desk_fence();
 if(!grant.api || !grant.slot || !grant.generation)return desk_fence();
 uint32_t header[2];memcpy(header,grant.api,sizeof(header));
 if(header[0]!=1 || header[1]<sizeof(risc_provider_promotion_api_v1))return desk_fence();
 risc_provider_promotion_api_v1 promotion;memcpy(&promotion,grant.api,sizeof(promotion));
 if(!promotion.context || !promotion.promote)return desk_fence();
 int32_t status=RISC_PROVIDER_PROMOTION_FAILED;
 for(unsigned i=0;i<DESK_PROMOTION_ATTEMPTS && status==RISC_PROVIDER_PROMOTION_FAILED;i++)
  status=promotion.promote(promotion.context);
 if(status==RISC_PROVIDER_PROMOTION_RETAINED || status==RISC_PROVIDER_PROMOTION_CONTEXT ||
    (status!=RISC_PROVIDER_PROMOTION_OK && status!=RISC_PROVIDER_PROMOTION_ALREADY_READY &&
     status!=RISC_PROVIDER_PROMOTION_FAILED && status!=RISC_PROVIDER_PROMOTION_BUSY))return desk_fence();
 if(!rt->release(&grant) || grant.api || grant.slot || grant.generation)return desk_fence();
 if(!desk_trace("promotion",status))return -2;
 if(status!=RISC_PROVIDER_PROMOTION_OK && status!=RISC_PROVIDER_PROMOTION_ALREADY_READY)return 0;
 desk_promoted=true;
 /* Demand activation changes once per Runtime boot, across app reloads. */
 desk_first_promotion=status==RISC_PROVIDER_PROMOTION_OK;
 return portable_desk_adapter_timer_only()?portable_desk_adapter_upgrade(status):
        portable_desk_adapter_start(PORTABLE_DESK_START_FOREGROUND);
}
static bool desk_bind(void) {
 app=t5_app_get_api(1);paper=paper_presentation_get();
 return app && app->abi_version==1 && app->struct_size>=sizeof(*app) && app->poll &&
     app->millis && app->fill_rect && app->draw_icon && app->set_back_exits_app &&
     paper && paper->struct_size>=sizeof(*paper) && paper->begin && paper->text &&
     paper->measure && paper->contact;
}
static bool desk_logo_rect(void *context,int x,int y,int w,int h) {
 (void)context;
 if(!portable_desk_adapter_ready())return false;
 app->fill_rect(x,y,w,h,true);return portable_desk_adapter_ready();
}
static bool desk_boot_logo(void) {
 if(!desk_first_promotion || !portable_desk_clock_boot_is_cold())return true;
 portable_desk_adapter_begin();
 if(!portable_desk_adapter_ready() ||
    !riscrte_logo_draw(NULL,desk_logo_rect,app->screen_width(),app->screen_height()) ||
    !portable_desk_adapter_present(true)){desk_fence();return false;}
 return true;
}
static bool sparse_boot(void) {
 rt=risc_runtime_get_api(1);
 if(!rt || rt->api_version!=1 || rt->struct_size<RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE || !rt->retain_invocation ||
    !rt->acquire || !rt->release || !rt->health || !rt->yield_ms || !rt->request_launch || !rt->diagnostic)return false;
 risc_runtime_health_v1 begun={.struct_size=sizeof(begun)};
 if(!rt->health(&begun)){desk_fence();return false;}desk_started_at=begun.uptime_ms;
 /* This read is native authority; no provider acquisition has happened in init. */
 portable_perf_span(PORTABLE_PERF_SPAN_CLASSIFICATION,true);
 int boot=portable_desk_clock_boot_read(rt,&desk_boot_record);
 if(boot==-2){desk_fence();return false;}
 portable_perf_span(PORTABLE_PERF_SPAN_CLASSIFICATION,false);
 if(!desk_trace("classification",boot))return false;
 portable_timezone_rule retained_rule;risc_realtime_snapshot_v1 sample;
 bool timer=boot==1 && portable_timezone_resolve(desk_boot_record.config.time_zone,
     sizeof(desk_boot_record.config.time_zone),&retained_rule)==PORTABLE_TIMEZONE_OK;
 if(timer) {
  int sampled=desk_sample(false,&sample);
  if(desk_retained || !desk_trace("native-sample",sampled))return false;
  timer=sampled==PORTABLE_REALTIME_OK;
 }
 if(desk_retained)return false;
 if(timer) {
  if(portable_desk_adapter_start(PORTABLE_DESK_START_TIMER)!=1 || !desk_bind())return false;
  if(!desk_trace("timer-ready",1))return false;
  desk_boot_timer=true;
  if(!portable_desk_adapter_sleep())return false;
  desk_boot_timer=false;
  if(desk_retained || portable_app_sleep_retained())return false;
 }
 /* Cold/GPIO/corrupt/UNSET and ordinary timer refusal all promote first. The
  * display, if already open, stays pinned across this one-way transition. */
 portable_perf_span(PORTABLE_PERF_SPAN_PROMOTION,true);
 if(desk_promote()!=1 || !desk_bind())return false;
 portable_perf_span(PORTABLE_PERF_SPAN_PROMOTION,false);
 portable_perf_span(PORTABLE_PERF_SPAN_CONFIG,true);
 if(!desk_config(&desk_configuration,&desk_sleep_mode))return false;
 portable_perf_span(PORTABLE_PERF_SPAN_CONFIG,false);
 format=desk_configuration.time_format;
 if(!portable_desk_adapter_flip(desk_configuration.flip_ui))return false;
 portable_perf_span(PORTABLE_PERF_SPAN_LOGO,true);
 if(!desk_boot_logo())return false;
 portable_perf_span(PORTABLE_PERF_SPAN_LOGO,false);
 portable_perf_span(PORTABLE_PERF_SPAN_RECOVERY,true);
 int recovered=desk_sample(true,&sample);
 if(desk_retained)return false;
 portable_perf_span(PORTABLE_PERF_SPAN_RECOVERY,false);
 if(desk_retained || !desk_trace("native-recovery",recovered))return false;
 app->set_back_exits_app(false);return portable_desk_adapter_foreground();
}
