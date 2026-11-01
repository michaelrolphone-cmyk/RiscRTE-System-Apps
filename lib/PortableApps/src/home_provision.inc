/* Explicit sparse-Home foreground provisioning; no timer-only entry. */
#ifdef PORTABLE_HOME_PROVISION
#if !defined(PORTABLE_DESK_CLOCK_SPARSE_START) || !defined(PORTABLE_DESK_DEFER_RADIOS) || !defined(PORTABLE_INSTALLED_CATALOG_HOST) || !defined(PORTABLE_CONTEXTS_CLOCK_RF_ONLY) || !defined(PORTABLE_BLE_BROADCAST)
#error "Home provisioning requires the selected sparse resident Home profile"
#endif
#include "PortableAutomaticProvision.h"
#include "PortableRealtimeClient.h"
static portable_automatic_provision home_provision;
static bool home_provision_safe(void*context){(void)context;return !failed&&!native_custody_retained&&!native_sleep_retained;}
static bool home_provision_network(void*context){(void)context;return desk_radios_loaded&&quick_radios.valid&&(quick_radios.flags&PORTABLE_RADIO_WIFI)&&!(quick_radios.flags&PORTABLE_RADIO_AIRPLANE);}
static bool home_provision_prepare(void*context){(void)context;
 if(!portable_contexts_stop()||!portable_broadcast_stop())return false;
 home_provision_busy=true;return true;
}
static int home_provision_time_guard(void*context){return home_provision_safe(context)?PORTABLE_REALTIME_GUARD_SAFE:PORTABLE_REALTIME_GUARD_RETAINED;}
static bool home_provision_time_halted(int rc){return rc==PORTABLE_REALTIME_CONTEXT||rc==PORTABLE_REALTIME_UNCERTAIN||rc==PORTABLE_REALTIME_RETAINED;}
static bool home_provision_utc(void*context,uint64_t*out){
 /* Read the already-authorized native control prefix without invoking seed or
  * RTC reconciliation. Strict UNSET is a private first-fetch token only. */
 portable_realtime_client client={0};risc_realtime_snapshot_v1 sample;
 int rc=portable_realtime_open(&client,rt,PORTABLE_REALTIME_CONTROL,
     PORTABLE_REALTIME_NORMAL_START,home_provision_time_guard,context);
 if(home_provision_time_halted(rc)){portable_adapter_retain();return false;}
 if(rc!=PORTABLE_REALTIME_OK)return false;
 rc=portable_realtime_read(&client,&sample);
 if(home_provision_time_halted(rc)){portable_adapter_retain();return false;}
 if(portable_realtime_close(&client)!=PORTABLE_REALTIME_OK){portable_adapter_retain();return false;}
 if(rc==PORTABLE_REALTIME_UNSET){*out=0;return true;}
 if(rc!=PORTABLE_REALTIME_OK||!sample.epoch_seconds)return false;
 *out=(uint64_t)sample.epoch_seconds;return true;
}
static void home_provision_retain(void*context){(void)context;portable_adapter_retain();}
static void home_provision_init(void){
 portable_automatic_hooks hooks={.runtime=rt,.network_allowed=home_provision_network,.safe=home_provision_safe,
  .prepare=home_provision_prepare,.utc=home_provision_utc,.retain=home_provision_retain,.profile="core",
  .wifi_instance=15,.settings_namespace=6,.volume_instance=0,.sd_enabled=true};
 portable_automatic_init(&home_provision,&hooks);installed_home_started=true;
}
static int home_provision_close(void){
 if(!installed_home_started)return INSTALLED_APPS_OK;
 int rc=portable_automatic_close(&home_provision);if(rc==INSTALLED_APPS_OK)home_provision_busy=false;
 return rc;
}
static void home_provision_progress(void){
 if(desk_phase!=DESK_FOREGROUND)return;
 if(installed_home_started&&home_provision.closing){(void)home_provision_close();return;}
 if(!desk_home_submitted||!desk_radios_loaded||!display_settled||!portable_paper_frame_idle()||
    alarm_modal||home_provision_alarm_busy()||quick_modal||quick_open_requested||
    pqa_visible(&quick.ui)||pqa_capture(&quick.ui)||home_pending||crown_pending||navigation_pending||handoff_requested)return;
 if(!installed_home_started)home_provision_init();
 int rc=portable_automatic_step(&home_provision,millis_now());
 if(!home_provision.phase&&!home_provision.closing)home_provision_busy=false;
 if(rc==INSTALLED_APPS_RETAINED&&!native_custody_retained)portable_adapter_retain();
}
#endif
