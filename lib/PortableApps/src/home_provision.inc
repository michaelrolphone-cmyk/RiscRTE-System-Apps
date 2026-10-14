/* Explicit sparse-Home foreground provisioning; no timer-only entry. */
#ifdef PORTABLE_HOME_PROVISION
#include "PortableAutomaticProvision.h"
static portable_automatic_provision home_provision;
extern bool portable_desk_clock_utc(uint64_t*);
static bool home_provision_safe(void*context){(void)context;return !failed&&!native_custody_retained&&!native_sleep_retained;}
static bool home_provision_network(void*context){(void)context;return desk_radios_loaded&&quick_radios.valid&&(quick_radios.flags&PORTABLE_RADIO_WIFI)&&!(quick_radios.flags&PORTABLE_RADIO_AIRPLANE);}
static bool home_provision_prepare(void*context){(void)context;
 if(!portable_contexts_stop()||!portable_broadcast_stop())return false;
 home_provision_busy=true;return true;
}
static bool home_provision_utc(void*context,uint64_t*out){(void)context;return portable_desk_clock_utc(out);}
static void home_provision_retain(void*context){(void)context;portable_adapter_retain();}
static void home_provision_init(void){
 portable_automatic_hooks hooks={.runtime=rt,.network_allowed=home_provision_network,.safe=home_provision_safe,
  .prepare=home_provision_prepare,.utc=home_provision_utc,.retain=home_provision_retain,.profile="core",
  .wifi_instance=15,.settings_namespace=6,.volume_instance=PORTABLE_INSTALLED_VOLUME_INSTANCE,.sd_enabled=true};
 portable_automatic_init(&home_provision,&hooks);installed_home_started=true;
}
static int home_provision_close(void){
 if(!installed_home_started)return INSTALLED_APPS_OK;
 int rc=portable_automatic_close(&home_provision);if(rc==INSTALLED_APPS_OK)home_provision_busy=false;
 return rc;
}
static void home_provision_progress(void){
 if(!installed_home_started)home_provision_init();
 if(home_provision.closing){(void)home_provision_close();return;}
 if(desk_phase!=DESK_FOREGROUND||!desk_home_submitted||!desk_radios_loaded||!display_settled||!portable_paper_frame_idle()||
    alarm_modal||quick_modal||pqa_visible(&quick.ui)||pqa_capture(&quick.ui)||home_pending||crown_pending||handoff_requested)return;
 int rc=portable_automatic_step(&home_provision,millis_now());
 if(!home_provision.phase&&!home_provision.closing)home_provision_busy=false;
 if(rc==INSTALLED_APPS_RETAINED&&!native_custody_retained)portable_adapter_retain();
}
#endif
