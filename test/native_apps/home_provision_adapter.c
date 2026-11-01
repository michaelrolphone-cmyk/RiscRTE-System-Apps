#include "../../lib/PortableApps/src/adapter.c"
/* Test visibility only: every action delegates to the production implementation. */
bool deferred_test_ready(bool force) { return desk_radio_policy_ready(force); }
bool deferred_test_loaded(void) { return desk_radios_loaded; }
bool deferred_test_home(void) { return desk_home_submitted; }
unsigned deferred_test_pending(void) {
 return (paper_token?1u:0u) | (surface.frame?2u:0u)
#ifdef PORTABLE_RASTER_SNAPSHOT
  | (raster_sealed?4u:0u) | (raster_recording?8u:0u)
#endif
 ;
}
void deferred_test_busy(unsigned flags) {
 display_settled=!(flags&1u);paper_token=(flags&2u)?1:0;
 surface.frame=(flags&4u)?1:0;
#ifdef PORTABLE_RASTER_SNAPSHOT
 raster_sealed=!!(flags&8u);raster_recording=!!(flags&16u);
#endif
}
bool deferred_test_mutable(void) { return raster_lease_mutable(); }
bool deferred_test_low_battery(void) { return low_battery_poll(); }
bool deferred_test_queue_token(void) {
 for(unsigned i=0;i<1000&&!paper_token;i++)if(!paper_present_progress())return false;
 return paper_token!=0;
}
bool deferred_test_submit(void) {
 if(!portable_paper_frame_ready())return false;
 portable_desk_adapter_begin();present(false);return !failed;
}
bool deferred_test_quick(void) { bool consumed=false;return quick_foreground(&consumed); }
bool deferred_test_alarm(void) { bool consumed=false;return alarm_foreground(&consumed); }
bool deferred_test_launch(void) { return desk_guard_launch("settings.elf"); }

void provision_test_progress(void){installed_progress();}
bool provision_test_busy(void){return home_provision_busy;}
unsigned provision_test_phase(void){return home_provision.phase;}
int provision_test_close(void){return portable_installed_prepare_launch();}
bool provision_test_utc(uint64_t*out){return home_provision_utc(NULL,out);}
void provision_test_network(unsigned flags){quick_radios.valid=true;quick_radios.flags=flags;}
void provision_test_gate(unsigned bits){
 desk_home_submitted=!(bits&1);desk_radios_loaded=!(bits&2);display_settled=!(bits&4);
 quick_modal=!!(bits&8);alarm_modal=!!(bits&16);home_pending=!!(bits&32);
 crown_pending=!!(bits&64);handoff_requested=!!(bits&128);quick_open_requested=!!(bits&256);
 navigation_pending=bits&512;surface.frame=bits&1024?1:0;
}
bool provision_test_background(void){return contexts_tick()&&broadcast_tick();}
bool provision_test_quick_pending(void){bool consumed=false;quick_open_requested=true;bool ok=quick_foreground(&consumed);quick_open_requested=false;return ok&&consumed;}
bool provision_test_alarm_pending(void){bool consumed=false;alarms.status.state=ALARM_STATE_CUE;bool ok=alarm_foreground(&consumed);return ok&&consumed;}
void provision_test_neutral(void){(void)pqa_input(&quick.ui,millis_now(),true,0,0,0,0,true);}
