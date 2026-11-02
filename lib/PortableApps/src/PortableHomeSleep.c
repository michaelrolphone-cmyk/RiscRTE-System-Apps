/* Common Home sleep controller. Records, alarm/minute deadlines and policy
 * are app-owned; physical preparation is delegated to standard capabilities. */
#include "PortableAppSleep.h"
#include "PortableDeskClockApp.h"
#include "PortableSleepPolicy.h"
#include "PortableHomePower.h"
#include "PortableHomeBoot.h"
#include "PortableNativeCustody.h"
#include "PortableBluetoothControl.h"
#include "WifiApi.h"
#include <AlarmServiceV2.h>
#include <stdio.h>
#include <string.h>
#ifdef PORTABLE_SHARED_HOME_CANVAS
extern int portable_home_canvas_prepare_sleep(void);
extern int portable_home_canvas_resume_sleep(void);
extern bool portable_home_canvas_sleep_available(void);
#else
static int portable_home_canvas_prepare_sleep(void){return 1;}
static int portable_home_canvas_resume_sleep(void){return 1;}
static bool portable_home_canvas_sleep_available(void){return true;}
#endif
static risc_runtime_capability_v1 home_sleep_grants[3];
static bool home_sleep_owned[3];
static const risc_runtime_api_v1 *home_sleep_runtime;
static const risc_power_sleep_api_v1 *home_power;
static const risc_display_power_api_v1 *home_display_power;
static const risc_retained_wake_api_v1 *home_wake;
static unsigned home_modes;
static bool home_sleep_retained;
static bool home_context_safe(void *unused){(void)unused;return !portable_adapter_retained()&&risc_runtime_get_api(1)!=NULL;}
unsigned portable_home_sleep_modes(void){return home_sleep_retained||!portable_home_canvas_sleep_available()?0:home_modes;}
static int home_sleep_fence(void){home_sleep_retained=true;home_modes=0;portable_adapter_retain_silent();return -2;}
int portable_home_sleep_close(void){
 if(home_sleep_retained||portable_adapter_retained())return -2;
 for(unsigned i=3;i;i--)if(home_sleep_owned[i-1]){
  if(!home_sleep_runtime->release(&home_sleep_grants[i-1]))return home_sleep_fence();
  home_sleep_owned[i-1]=false;
 }
 home_sleep_runtime=NULL;home_power=NULL;home_display_power=NULL;home_wake=NULL;home_modes=0;return 1;
}
int portable_home_sleep_admit(const risc_runtime_api_v1 *runtime){
 if(home_sleep_retained||portable_adapter_retained())return -2;
 if(home_sleep_runtime)return 1;
 if(!runtime||runtime->api_version!=1||runtime->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE||!runtime->acquire||!runtime->release)return 0;
 home_sleep_runtime=runtime;
 const char*names[]={RISC_POWER_SLEEP_CAPABILITY,RISC_DISPLAY_POWER_CAPABILITY,RISC_RETAINED_WAKE_CAPABILITY};
 for(unsigned i=0;i<3;i++){
  home_sleep_grants[i]=(risc_runtime_capability_v1){.struct_size=sizeof(home_sleep_grants[i])};
  if(!runtime->acquire(names[i],1,0,&home_sleep_grants[i])){
   if(portable_adapter_retained()||home_sleep_grants[i].api||home_sleep_grants[i].slot||home_sleep_grants[i].generation)return home_sleep_fence();
   if(i==2)break; /* Light does not require a retained checkpoint. */
   return portable_home_sleep_close(); /* Explicit unavailable, not fake readiness. */
  }
  home_sleep_owned[i]=true;
 }
 home_power=home_sleep_grants[0].api;home_display_power=home_sleep_grants[1].api;home_wake=home_sleep_grants[2].api;
 const risc_retained_wake_api_v1_extended *wake=home_wake&&home_wake->api_version==1&&
  home_wake->struct_size>=sizeof(*home_wake)&&home_wake->read&&home_wake->stage&&home_wake->clear?
  risc_retained_wake_extended(home_wake):NULL;
 portable_home_power physical={.power=home_power,.display=home_display_power};
 home_modes=portable_home_power_modes(&physical,wake&&wake->max_payload_bytes>=PORTABLE_DESK_CLOCK_RECORD_BYTES)&
  (PORTABLE_HOME_MODE_LIGHT|PORTABLE_HOME_MODE_DEEP); /* This overview policy has no Hybrid controller. */
 return 1;
}
/* Foreground policy only: checked shutdown without changing persisted intent.
 * Missing optional radios do not turn unrelated controls off. */
static int home_radios_off(const risc_runtime_api_v1*rt) {
 if(portable_desk_adapter_timer_only())return 1;
 const char*names[]={"net.wifi","bluetooth.hci"};
 for(unsigned i=0;i<2;i++){
  risc_runtime_capability_v1 grant={.struct_size=sizeof(grant)};
  if(!rt->acquire(names[i],1,0,&grant)){
   if(portable_adapter_retained()||grant.api||grant.slot||grant.generation)return home_sleep_fence();
   continue;
  }
  bool ok=false;
  if(i==0){
   const wifi_api_v1*w=grant.api;
   ok=w&&w->api_version==1&&w->struct_size>=WIFI_MANAGEMENT_V1_SIZE&&w->disconnect_checked&&w->status;
   if(ok){ok=w->disconnect_checked(w->context);if(!home_context_safe(NULL))return home_sleep_fence();}
   if(ok){ok=w->status(w->context)==WIFI_LINK_DOWN;if(!home_context_safe(NULL))return home_sleep_fence();}
  }else{
   const portable_bluetooth_control_v1*b=grant.api;uint8_t state=PORTABLE_BLUETOOTH_RETAINED;
   ok=b&&b->api_version==1&&b->struct_size>=sizeof(*b)&&b->set_enabled&&b->status;
   if(ok){ok=b->set_enabled(b->context,false);if(!home_context_safe(NULL))return home_sleep_fence();}
   if(ok){ok=b->status(b->context,&state)&&state==PORTABLE_BLUETOOTH_OFF;if(!home_context_safe(NULL))return home_sleep_fence();}
  }
  if(!ok||portable_adapter_retained())return home_sleep_fence();
  if(!rt->release(&grant))return home_sleep_fence();
 }
 portable_desk_clock_radios_off();return 1;
}
typedef struct {
 const risc_runtime_api_v1 *rt;
 const alarm_service_v1 *alarms;
 const risc_retained_wake_api_v1 *wake;
 portable_home_power physical;
 bool retained,record_staged,timer_only,input_touched;
 uint8_t staged_value[PORTABLE_DESK_CLOCK_RECORD_BYTES];
 uint32_t alarm_duration_ms,alarm_sampled_at;
 alarm_sleep_v1 alarm_ticket;
 const char *timer_exit_stage;int32_t timer_exit_status;
} home_desk_sleep;
static void desk_exit_stage(home_desk_sleep*s,const char*stage,int32_t status){s->timer_exit_stage=stage;s->timer_exit_status=status;}
static bool desk_runtime_valid(const risc_runtime_api_v1 *rt) {
    return rt && rt->api_version==RISC_RUNTIME_API_V1 &&
        rt->struct_size>=RISC_RUNTIME_CAPABILITIES_V1_SIZE && rt->acquire && rt->release;
}
static bool desk_wake_valid(const risc_retained_wake_api_v1 *wake) {
    return wake && wake->api_version==RISC_RETAINED_WAKE_API_V1 &&
        wake->struct_size>=sizeof(*wake) && wake->read && wake->stage && wake->clear;
}
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
static bool desk_boot_is_cold;
static bool desk_boot_is_interactive;
bool portable_desk_clock_boot_is_cold(void) { return desk_boot_is_cold; }
bool portable_desk_clock_boot_is_interactive(void) { return desk_boot_is_interactive; }
#endif
int portable_desk_clock_boot_read(const risc_runtime_api_v1 *rt,portable_desk_record *out) {
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
    desk_boot_is_cold=false;
    desk_boot_is_interactive=false;
#endif
    if(!out || !desk_runtime_valid(rt))return 0;
    risc_runtime_capability_v1 grant={.struct_size=sizeof(grant)};
    bool acquired=rt->acquire(RISC_RETAINED_WAKE_CAPABILITY,1,0,&grant);
    if(risc_runtime_get_api(1)!=rt)return -2;
    if(!acquired){
        if(grant.struct_size!=sizeof(grant)||grant.api||grant.slot||grant.generation)return -2;
        return 0; /* Clean optional absence: no checkpoint, not retained custody. */
    }
    const risc_retained_wake_api_v1 *wake=grant.api;
    portable_desk_record record={0};int valid=0;
#if defined(PORTABLE_DESK_CLOCK_SPARSE_START) && defined(PORTABLE_STAGE_LOGS)
    int32_t read_status=RISC_RETAINED_WAKE_INVALID;uint32_t boot_cause=UINT32_MAX;
#endif
    if(desk_wake_valid(wake)) {
        uint8_t bytes[PORTABLE_DESK_CLOCK_RECORD_BYTES]={0};
        uint32_t used=0,cause=UINT32_MAX;
        risc_retained_wake_record_v1 value={.struct_size=sizeof(value)};
#if PORTABLE_DESK_CLOCK_RECORD_BYTES > RISC_RETAINED_WAKE_PAYLOAD_MAX
        const risc_retained_wake_api_v1_extended *extended=risc_retained_wake_extended(wake);
        int32_t rc;
        if(extended && extended->max_payload_bytes>=sizeof(bytes))
            rc=portable_home_boot_read_bytes(extended,PORTABLE_DESK_CLOCK_RECORD_TYPE,
                PORTABLE_DESK_CLOCK_RECORD_SCHEMA,bytes,sizeof(bytes),&used,&cause);
        else {
            /* An older native can still classify the boot. Its smaller record
             * is never interpreted as the selected larger schema. */
            rc=portable_home_boot_read(wake,PORTABLE_DESK_CLOCK_RECORD_TYPE,
                         PORTABLE_DESK_CLOCK_RECORD_SCHEMA,&value,&cause);
            if(rc==RISC_RETAINED_WAKE_OK)rc=RISC_RETAINED_WAKE_MISMATCH;
        }
#else
        int32_t rc=portable_home_boot_read(wake,PORTABLE_DESK_CLOCK_RECORD_TYPE,
                             PORTABLE_DESK_CLOCK_RECORD_SCHEMA,&value,&cause);
        if(rc==RISC_RETAINED_WAKE_OK && value.struct_size>=sizeof(value) &&
           value.type==PORTABLE_DESK_CLOCK_RECORD_TYPE &&
           value.schema_version==PORTABLE_DESK_CLOCK_RECORD_SCHEMA && value.size<=sizeof(bytes)) {
            used=value.size;memcpy(bytes,value.payload,used);
        }
#endif
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
        /* CONTEXT may indicate retained native custody. Unknown outcomes also
         * give no authority to release, clear, yield or try another provider. */
        if(rc!=RISC_RETAINED_WAKE_OK && rc!=RISC_RETAINED_WAKE_ABSENT &&
           rc!=RISC_RETAINED_WAKE_MISMATCH && rc!=RISC_RETAINED_WAKE_INVALID)return -2;
        desk_boot_is_cold=rc!=RISC_RETAINED_WAKE_INVALID &&
            (cause==RISC_BOOT_POWER_ON || cause==RISC_BOOT_RESET);
        desk_boot_is_interactive=rc!=RISC_RETAINED_WAKE_INVALID && cause==RISC_BOOT_DEEP_GPIO;
#endif
        valid=rc==RISC_RETAINED_WAKE_OK && cause==RISC_BOOT_DEEP_TIMER &&
            portable_desk_decode(bytes,used,&record);
#if defined(PORTABLE_DESK_CLOCK_SPARSE_START) && defined(PORTABLE_STAGE_LOGS)
        read_status=rc;boot_cause=cause;
#endif
    }
    if(!rt->release(&grant))return -2;
#if defined(PORTABLE_DESK_CLOCK_SPARSE_START) && defined(PORTABLE_STAGE_LOGS)
    if(rt->diagnostic) {
        char line[112];
        snprintf(line,sizeof(line),"SPARSE_CLOCK wake cause=%lu read=%ld record=%d schema=%u",
            (unsigned long)boot_cause,(long)read_status,valid,PORTABLE_DESK_CLOCK_RECORD_SCHEMA);
        (void)rt->diagnostic(line);
    }
#endif
    if(valid)*out=record;
    return valid;
}
static int desk_retain(home_desk_sleep*s){s->retained=true;return home_sleep_fence();}
static int desk_cancelled(void*context){
 home_desk_sleep*s=context;if(s->retained)return -2;
 bool pending=true;
 if(!s->physical.power->sleep_wake_pending(s->physical.power->context,&pending)||!home_context_safe(NULL))return desk_retain(s);
 return pending?1:0;
}
static int desk_resume(void*context){
 home_desk_sleep*s=context;if(s->retained)return -2;
 if(portable_home_power_restore(&s->physical)!=1)return desk_retain(s);
 if(s->input_touched){if(portable_home_canvas_resume_sleep()!=1)return desk_retain(s);s->input_touched=false;}
 if(s->record_staged){
  if(!s->wake||s->wake->clear(s->wake->context)!=RISC_RETAINED_WAKE_OK)return desk_retain(s);
  s->record_staged=false;
 }
 return 1;
}
static int desk_refuse(home_desk_sleep*s){return desk_resume(s)==1?0:-2;}
static bool desk_alarm_terminal(int32_t rc){
 return rc==ALARM_OUTPUT||rc<ALARM_FOREGROUND||rc>ALARM_PENDING;
}
static int desk_alarm_state(home_desk_sleep *s,int32_t rc) {
    if(desk_alarm_terminal(rc)||!risc_runtime_get_api(1))return desk_retain(s);
#ifdef ALARM_SERVICE_TAGGED_V2
    if(rc==ALARM_RETAINED)return desk_retain(s);
#endif
    if(rc==ALARM_OUTPUT)return desk_retain(s);
    alarm_status_v1 status={.struct_size=sizeof(status)};
    int32_t got=s->alarms->status(s->alarms->context,&status);
    if(desk_alarm_terminal(got)||!risc_runtime_get_api(1)||desk_alarm_terminal(status.error))return desk_retain(s);
#ifdef ALARM_SERVICE_TAGGED_V2
    if(got==ALARM_RETAINED || status.error==ALARM_RETAINED)return desk_retain(s);
#endif
    if(got==ALARM_OUTPUT || status.output_uncertain)return desk_retain(s);
    if(got!=ALARM_OK || (rc!=ALARM_OK && rc!=ALARM_PENDING)) {
        desk_exit_stage(s,"alarm-state",got!=ALARM_OK?got:rc);return 0;
    }
    return 1;
}
static int desk_alarm_prepare(home_desk_sleep *s) {
    alarm_sleep_v1 decision={.struct_size=sizeof(decision)};
    int32_t ready=ALARM_PENDING;
    risc_runtime_health_v1 sampled={.struct_size=sizeof(sampled)};
    for(unsigned n=0;n<64 && ready==ALARM_PENDING;++n) {
        if(desk_cancelled(s))return s->retained?-2:0;
        if(!s->rt->health(&sampled))return 0;
        decision=(alarm_sleep_v1){.struct_size=sizeof(decision)};
        ready=s->alarms->prepare_sleep(s->alarms->context,&decision);
        if(desk_alarm_terminal(ready)||!risc_runtime_get_api(1))return desk_retain(s);
        desk_exit_stage(s,"alarm-prepare",ready);
        /* No further alarm-service/writer calls after OK. Only reversible
         * peripheral work and clock/key reads follow on the serialized owner. */
        if(ready==ALARM_OK)break;
        int checked=desk_alarm_state(s,ready);
        if(checked!=1)return checked;
        if(ready==ALARM_PENDING) {
            checked=desk_alarm_state(s,s->alarms->step(s->alarms->context));
            if(checked!=1)return checked;
        }
    }
    if(ready!=ALARM_OK)return 0;
    if(decision.deadline && decision.deadline<=decision.rtc_seconds) {
        desk_exit_stage(s,"alarm-due",0);return 0;
    }
    /* Timestamp precedes the final reconciliation: any service-call latency
     * is charged to the alarm bound, never added back as a fresh deadline. */
    s->alarm_ticket=decision;
    s->alarm_sampled_at=sampled.uptime_ms;
    s->alarm_duration_ms=RISC_TIMED_SLEEP_MAX_MS;
    if(decision.deadline) {
        /* Whole seconds do not expose the fractional clock sample. Subtract
         * one second conservatively, then subtract preparation/staging age. */
        uint32_t seconds=decision.deadline-decision.rtc_seconds;
        s->alarm_duration_ms=seconds>RISC_TIMED_SLEEP_MAX_MS/1000u?RISC_TIMED_SLEEP_MAX_MS:
            seconds>1u?(seconds-1u)*1000u:1u;
    }
    return 1;
}
static int desk_prepare(void*context){
 home_desk_sleep*s=context;
 if(home_radios_off(s->rt)!=1)return desk_retain(s);
 int rc=portable_home_power_prepare(&s->physical);
 if(rc==-2)return desk_retain(s);
 if(rc!=1)return desk_refuse(s);
 rc=desk_alarm_prepare(s);
 if(rc!=1)return rc==-2?-2:desk_refuse(s);
 if(desk_cancelled(s))return s->retained?-2:desk_refuse(s);
 s->input_touched=true;rc=portable_home_canvas_prepare_sleep();
 if(rc==-2)return desk_retain(s);
 if(rc!=1)return desk_refuse(s);
 rc=portable_home_power_hold(&s->physical);
 if(rc==-2)return desk_retain(s);
 return rc==1?1:desk_refuse(s);
}
static int desk_stage(void *context,const portable_desk_record *record) {
    home_desk_sleep *s=context;
    if(s->retained)return -2;
    if(s->physical.held || s->physical.display_touched || s->physical.power_touched)return desk_retain(s);
    uint8_t bytes[PORTABLE_DESK_CLOCK_RECORD_BYTES];
    if(!portable_desk_encode(record,bytes,sizeof(bytes)))return desk_refuse(s);
    /* A confirmed completed image is staged while no provider pad holds exist.
     * Nothing is committed to RTC until the native terminal-entry boundary. */
    s->record_staged=true;
#if PORTABLE_DESK_CLOCK_RECORD_BYTES > RISC_RETAINED_WAKE_PAYLOAD_MAX
    const risc_retained_wake_api_v1_extended *extended=risc_retained_wake_extended(s->wake);
    int32_t rc=extended && extended->max_payload_bytes>=sizeof(bytes)?
        extended->stage_bytes(s->wake->context,PORTABLE_DESK_CLOCK_RECORD_TYPE,
            PORTABLE_DESK_CLOCK_RECORD_SCHEMA,bytes,sizeof(bytes)):RISC_RETAINED_WAKE_INVALID;
#else
    risc_retained_wake_record_v1 value={.struct_size=sizeof(value),
        .type=PORTABLE_DESK_CLOCK_RECORD_TYPE,.schema_version=PORTABLE_DESK_CLOCK_RECORD_SCHEMA,
        .size=sizeof(bytes)};
    memcpy(value.payload,bytes,sizeof(bytes));
    int32_t rc=s->wake->stage(s->wake->context,&value);
#endif
#ifdef PORTABLE_DESK_CLOCK_SPARSE_START
    if(rc!=RISC_RETAINED_WAKE_OK) {
        /* INVALID is a clean argument refusal. No other non-OK stage outcome
         * proves storage-safe custody, so do not even attempt clear/release. */
        if(rc!=RISC_RETAINED_WAKE_INVALID)return desk_retain(s);
        return desk_refuse(s);
    }
#else
    if(rc!=RISC_RETAINED_WAKE_OK)return desk_refuse(s);
#endif
    memcpy(s->staged_value,bytes,sizeof(bytes));return 1;
}
static int desk_stage_enter(void *context,const portable_desk_record *record,
                            uint32_t duration,uint32_t sampled_at) {
    home_desk_sleep *s=context;
    if(s->retained)return -2;
    if(!duration || duration>RISC_TIMED_SLEEP_MAX_MS || desk_cancelled(s))return s->retained?-2:0;
    uint8_t encoded[PORTABLE_DESK_CLOCK_RECORD_BYTES];
    if(!s->record_staged || !portable_desk_encode(record,encoded,sizeof(encoded)) ||
       memcmp(encoded,s->staged_value,sizeof(encoded))) {
        desk_exit_stage(s,"record-changed",0);return 0;
    }
    risc_runtime_health_v1 now={.struct_size=sizeof(now)};
    if(!s->rt->health(&now))return 0;
    uint32_t elapsed=now.uptime_ms-sampled_at;
    uint32_t alarm_elapsed=now.uptime_ms-s->alarm_sampled_at;
    uint32_t alarm_ms=s->alarm_duration_ms;
    if(elapsed>=duration || alarm_elapsed>=alarm_ms) {
        desk_exit_stage(s,elapsed>=duration?"minute-expired":"alarm-expired",0);return 0;
    }
    duration-=elapsed;alarm_ms-=alarm_elapsed;
    if(alarm_ms<duration)duration=alarm_ms;
    int rc=portable_home_power_enter(&s->physical,duration);
    desk_exit_stage(s,"native-entry",rc);
    if(rc==PORTABLE_HOME_POWER_RETAINED)return desk_retain(s);
    return 0; /* Shared caller restores before native checkpoint clear. */
}
/* The provider's Light callback owns any late physical closure specific to
 * its installation. App code never substitutes Deep preparation for Light. */
int portable_home_light_sleep_with_ui(const risc_runtime_api_v1*rt,const risc_display_output_api_v1*display,
 const risc_battery_gauge_api_v1*gauge,const alarm_service_v1*alarms,const portable_idle_sleep_ui*ui){
 (void)display;(void)gauge;
 const alarm_service_descriptor_v2*descriptor=alarm_service_descriptor(alarms);
 if(!desk_runtime_valid(rt)||!rt->health||!descriptor||!(descriptor->features&ALARM_DESCRIPTOR_RESUME_SLEEP)||
    (ui&&(!ui->begin||!ui->restore)))return 0;
 if(portable_home_sleep_admit(rt)!=1)return -2;
 if(!(portable_home_sleep_modes()&PORTABLE_HOME_MODE_LIGHT))return 0;
 if(home_radios_off(rt)!=1)return -2;
 home_desk_sleep s={.rt=rt,.alarms=alarms,.physical={.power=home_power,.display=home_display_power,.call_safe=home_context_safe}};
 bool shown=false;int result=0;
 s.physical.display_touched=true;
 bool physical_ready=home_display_power->prepare_sleep(home_display_power->context);
 if(!home_context_safe(NULL))return desk_retain(&s);
 if(!physical_ready)goto restore;
 s.physical.power_touched=true;
 physical_ready=home_power->prepare_sleep(home_power->context);
 if(!home_context_safe(NULL))return desk_retain(&s);
 if(!physical_ready)goto restore;
 int prepared=desk_alarm_prepare(&s);
 if(prepared==-2)return -2;
 if(prepared!=1)goto restore;
 if(ui){portable_idle_ui_result rc=ui->begin(ui->context);if(rc==PORTABLE_IDLE_UI_REFUSED)goto restore;if(rc!=PORTABLE_IDLE_UI_READY)return desk_retain(&s);shown=true;}
 s.input_touched=true;prepared=portable_home_canvas_prepare_sleep();
 if(prepared==-2)return desk_retain(&s);
 if(prepared!=1)goto restore;
 if(desk_cancelled(&s)){if(s.retained)return -2;goto restore;}
 risc_runtime_health_v1 now={.struct_size=sizeof(now)};
 if(!rt->health(&now))goto restore;
 uint32_t elapsed=now.uptime_ms-s.alarm_sampled_at;
 if(elapsed>=s.alarm_duration_ms)goto restore;
 uint32_t duration=s.alarm_ticket.deadline?s.alarm_duration_ms-elapsed:0;
 risc_light_sleep_result_v1 wake={.struct_size=sizeof(wake)};
 int32_t status=home_power->light_sleep_for(home_power->context,duration,&wake);
 if(status==RISC_LIGHT_SLEEP_RETAINED||status>0||status<RISC_LIGHT_SLEEP_UNSUPPORTED)return desk_retain(&s);
 if(!home_context_safe(NULL))return desk_retain(&s);
 if(desk_resume(&s)!=1)return -2;
 result=0;
 if(status==RISC_LIGHT_SLEEP_OK){
  int32_t resumed=alarm_service_resume(alarms,&s.alarm_ticket);
  if(desk_alarm_terminal(resumed)||!risc_runtime_get_api(1))return desk_retain(&s);
  result=resumed==ALARM_OK?1:-1;
 }
restore:
 if(desk_resume(&s)!=1)return -2;
 if(shown&&ui->restore(ui->context)!=PORTABLE_IDLE_UI_READY)return desk_retain(&s);
 return result==1?1:result<0?-1:0;
}
int portable_app_alarm_sleep(const risc_runtime_api_v1*rt,const risc_display_output_api_v1*display,
 const risc_battery_gauge_api_v1*gauge,const alarm_service_v1*alarms){
 if(!desk_runtime_valid(rt)||!rt->health||!alarm_service_descriptor(alarms))return 0;
 if(portable_home_sleep_admit(rt)!=1)return -2;
 bool timer=portable_desk_adapter_timer_only();
 unsigned mode=timer||portable_desk_clock_lock_requested()?PORTABLE_SLEEP_DEEP:PORTABLE_SLEEP_LIGHT;
 if(!timer&&!portable_desk_clock_lock_requested()){
  risc_runtime_capability_v1 prefs={.struct_size=sizeof(prefs)};
  if(!rt->acquire("storage.key-value",1,PORTABLE_SLEEP_STORE_INSTANCE,&prefs))return home_sleep_fence();
  unsigned allowed=(home_modes&PORTABLE_HOME_MODE_LIGHT?PORTABLE_SLEEP_MASK_LIGHT:0)|(home_modes&PORTABLE_HOME_MODE_DEEP?PORTABLE_SLEEP_MASK_DEEP:0);
  (void)portable_sleep_load_profile(prefs.api,allowed,PORTABLE_SLEEP_LIGHT,&mode);
  if(!rt->release(&prefs))return home_sleep_fence();
 }
 if(mode!=PORTABLE_SLEEP_DEEP)return portable_home_light_sleep_with_ui(rt,display,gauge,alarms,NULL);
 if(!(portable_home_sleep_modes()&PORTABLE_HOME_MODE_DEEP)){portable_desk_clock_refused();return 0;}
 home_desk_sleep state={.rt=rt,.alarms=alarms,.wake=home_wake,.timer_only=timer,
  .physical={.power=home_power,.display=home_display_power,.call_safe=home_context_safe}};
 const portable_desk_sleep_ops ops={.context=&state,.cancelled=desk_cancelled,.stage=desk_stage,
  .prepare=desk_prepare,.resume=desk_resume,.stage_enter=desk_stage_enter};
 int result=portable_desk_clock_run(rt,&ops);
 if(result==-2||state.retained)return -2;
 if(desk_resume(&state)!=1)return -2;
 return 0;
}
