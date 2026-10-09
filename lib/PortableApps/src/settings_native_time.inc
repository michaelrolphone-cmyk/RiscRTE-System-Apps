/* Opt-in controller policy: native UTC reads, explicit IANA selection and
 * checked Save. Every helper is invocation-local and closed before UI/handoff. */
#include "PortableSetTime.h"
static portable_rtc_basis snt_basis;
static int snt_basis_status,snt_zone_status,snt_snapshot_status;
static char snt_zone_id[PORTABLE_TIMEZONE_ID_BYTES];
static portable_timezone_rule snt_rule;
static portable_set_time_result snt_last_result;
static char snt_status_text[128];
static portable_realtime_client snt_reader;
static portable_set_time_client snt_writer;
static bool snt_result_available;
static int snt_guard(void *context) {
  (void)context;
  return native_custody_retained?PORTABLE_REALTIME_GUARD_RETAINED:
    failed?PORTABLE_REALTIME_GUARD_UNCONFIRMED:PORTABLE_REALTIME_GUARD_SAFE;
}
static bool snt_halted(int status) {
  if(status==PORTABLE_REALTIME_UNCERTAIN || status==PORTABLE_REALTIME_RETAINED ||
     status==PORTABLE_REALTIME_CONTEXT) {portable_adapter_retain();return true;}
  return native_custody_retained;
}
static bool snt_load_zone(void) {
  snt_zone_status=portable_timezone_preference_load(settings_store,snt_zone_id);
  if(native_custody_retained)return false;
  return (snt_zone_status==PORTABLE_TIMEZONE_LOADED || snt_zone_status==PORTABLE_TIMEZONE_MISSING) &&
    portable_timezone_resolve(snt_zone_id,sizeof(snt_zone_id),&snt_rule)==PORTABLE_TIMEZONE_OK;
}
static void snt_load_basis(void) {
  snt_basis_status=portable_rtc_basis_load(settings_store,&snt_basis);
}
static const char *snt_basis_label(void) {
  return snt_basis_status==PORTABLE_RTC_BASIS_LOADED?(snt_basis.stores_utc?"UTC (saved)":"Local (saved)"):
    snt_basis_status==PORTABLE_RTC_BASIS_MISSING?"Local (default on Save)":
    snt_basis_status==PORTABLE_RTC_BASIS_INVALID?"Invalid - Save blocked":"Unavailable - Save blocked";
}
static bool snt_read_local(twatch_rtc_time_v1 *out) {
  if(!snt_load_zone()) {snt_snapshot_status=PORTABLE_REALTIME_INVALID;return false;}
  snt_snapshot_status=portable_realtime_open(&snt_reader,rt,PORTABLE_REALTIME_CONTROL,
    PORTABLE_REALTIME_TIMER_ONLY,snt_guard,NULL);
  if(snt_halted(snt_snapshot_status) || snt_snapshot_status!=PORTABLE_REALTIME_OK)return false;
  risc_realtime_snapshot_v1 snapshot;
  snt_snapshot_status=portable_realtime_read(&snt_reader,&snapshot);
  if(snt_halted(snt_snapshot_status))return false;
  int closed=portable_realtime_close(&snt_reader);
  if(snt_halted(closed) || closed!=PORTABLE_REALTIME_OK)return false;
  if(snt_snapshot_status!=PORTABLE_REALTIME_OK)return false;
  portable_timezone_civil civil;
  if(portable_timezone_utc_to_local(&snt_rule,snapshot.epoch_seconds,&civil,NULL)!=PORTABLE_TIMEZONE_OK)return false;
  *out=(twatch_rtc_time_v1){(uint16_t)civil.year,civil.month,civil.day,civil.weekday,civil.hour,civil.minute,civil.second};
  return true;
}
static portable_set_time_request snt_request(int fold) {
  portable_set_time_request request={.rule=snt_rule,.basis=snt_basis,.fold_choice=fold,
    .rtc_instance=0,.native_readback_budget_us=1000000};
  request.local=(portable_timezone_civil){settings_draft.year,settings_draft.month,settings_draft.day,
    settings_draft.hour,settings_draft.minute,settings_draft.second,settings_draft.weekday};
  return request;
}
static const char *snt_outcome(int state) {
  return state==PORTABLE_SET_TIME_CONFIRMED?"verified":state==PORTABLE_SET_TIME_UNCONFIRMED?"unconfirmed":"not attempted";
}
static void snt_describe_result(void) {
  snprintf(snt_status_text,sizeof(snt_status_text),"RTC %s; Native %s; Basis %s",
    snt_outcome(snt_last_result.rtc_outcome),
    snt_last_result.native.verified?"verified":snt_last_result.native.attempted?"unconfirmed":"not attempted",
    snt_outcome(snt_last_result.metadata_outcome));
  editor_message=snt_status_text;snt_result_available=true;
}
static bool snt_save(int fold) {
  if(snt_basis_status!=PORTABLE_RTC_BASIS_LOADED && snt_basis_status!=PORTABLE_RTC_BASIS_MISSING) {
    editor_message=snt_basis_label();return false;
  }
  if(snt_zone_status!=PORTABLE_TIMEZONE_LOADED && snt_zone_status!=PORTABLE_TIMEZONE_MISSING) {
    editor_message="Choose a valid Time Zone first";return false;
  }
  portable_set_time_request request=snt_request(fold);portable_set_time_plan plan;
  int status=portable_set_time_prepare(&request,&plan);
  if(status!=PORTABLE_REALTIME_OK) {
    editor_message=status==PORTABLE_REALTIME_GAP?"DST gap - time does not exist":
      status==PORTABLE_REALTIME_FOLD?"Choose first or second occurrence":"Invalid date or native 2038 limit";
    return false;
  }
  status=portable_set_time_open(&snt_writer,rt,snt_guard,NULL);
  if(snt_halted(status))return false;
  if(status!=PORTABLE_REALTIME_OK){editor_message="Native time unavailable - not saved";return false;}
  status=portable_set_time_apply_confirmed(&snt_writer,&request,&snt_last_result);
  snt_describe_result();
  if(snt_halted(status))return false;
  int closed=portable_set_time_close(&snt_writer);
  if(snt_halted(closed))return false;
  if(status!=PORTABLE_REALTIME_OK || closed!=PORTABLE_REALTIME_OK)return false;
  snt_basis=plan.basis;snt_basis.reference_epoch=(uint32_t)snt_last_result.verified_utc_epoch;
  snt_basis_status=PORTABLE_RTC_BASIS_LOADED;return true;
}

#ifdef PORTABLE_QUICK_ACTIONS
static bool settings_native_clock(uint8_t *hour,uint8_t *minute) {
  twatch_rtc_time_v1 local;
  if(!snt_read_local(&local))return false;
  *hour=local.hour;*minute=local.minute;return true;
}
#endif
