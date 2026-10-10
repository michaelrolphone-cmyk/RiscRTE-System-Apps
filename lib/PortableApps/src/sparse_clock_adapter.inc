/* Opt-in app-local lifecycle. The default adapter never preprocesses this file. */
#ifdef PORTABLE_DESK_POINTS_SNAPSHOT
#include "home_points_catalog_adapter.inc"
int portable_desk_points_catalog_refresh(portable_points_catalog_view *out) {
 if(portable_adapter_retained())return -2;
 if(!out || out->struct_size<sizeof(*out) || desk_phase!=DESK_TIMER || failed ||
    surface.frame || paper_token || !display_settled || !alarms.api)return -1;
 if(!portable_alarm_status(&alarms))return portable_adapter_retained()?-2:-1;
 if(alarms.status.output_uncertain){alarms.retained=true;portable_adapter_retain_silent();return -2;}
 if(portable_alarm_owned(&alarms) || alarms.status.error ||
    alarms.status.state==ALARM_STATE_BLOCKED)return -1;
 uint32_t before=alarms.status.snapshot;
 if(!portable_alarm_refresh(&alarms))return portable_adapter_retained()?-2:-1;
 /* Like the deployment's sleep preparation, bound cooperative phases. A
  * copied projection may stay available during LOAD; only a new evaluated
  * snapshot is fresh. Stop before subsequent output phases are advanced. */
 for(unsigned i=0;i<64;i++) {
  if(!portable_alarm_pump(&alarms))return portable_adapter_retained()?-2:-1;
  if(alarms.status.output_uncertain){alarms.retained=true;portable_adapter_retain_silent();return -2;}
  if(alarms.status.error || portable_alarm_owned(&alarms) ||
     alarms.status.state==ALARM_STATE_BLOCKED)return -1;
  if(alarms.status.state==ALARM_STATE_READY && alarms.status.snapshot!=before) {
   int copied=portable_home_points_catalog(out);
   if(copied==1 && out->snapshot!=alarms.status.snapshot)return -1;
   return copied;
  }
  rt->yield_ms(1);
  if(portable_adapter_retained())return -2;
 }
 return -1;
}
#endif
static bool desk_guard_health(risc_runtime_health_v1 *out) {
 if(native_sleep_retained)return false;
 if(!desk_runtime->health(out)){portable_desk_adapter_retain();return false;}
 return true;
}
static void desk_guard_yield(uint32_t ms) {
 if(!native_sleep_retained)desk_runtime->yield_ms(ms);
}
static bool desk_guard_diagnostic(const char *line) {
 return !native_sleep_retained && desk_runtime->diagnostic(line);
}
static bool desk_guard_launch(const char *path) {
#ifdef PORTABLE_CONTEXTS_CLIENT
 if(native_sleep_retained || !portable_paper_frame_drain())return false;
 if(!contexts_suspend())return false;
 return desk_runtime->request_launch(path);
#else
 return !native_sleep_retained && portable_paper_frame_drain() && desk_runtime->request_launch(path);
#endif
}
#ifdef PORTABLE_QUICK_RADIOS
/* QuickActions can acquire several transient grants before applying policy.
 * Guard its two copied control tables as well as Runtime calls: a later failed
 * acquisition may retain the graph even when earlier tables remain readable. */
static const wifi_api_v1 *desk_wifi;
static const portable_bluetooth_control_v1 *desk_ble;
static wifi_api_v1 desk_wifi_control;
static portable_bluetooth_control_v1 desk_ble_control;
static bool desk_ble_wanted;
static bool desk_wifi_off(void *context) {
 if(native_sleep_retained)return false;
 if(!desk_wifi->disconnect_checked(context)){portable_desk_adapter_retain();return false;}
 return true;
}
static wifi_link_t desk_wifi_status(void *context) {
 if(native_sleep_retained)return WIFI_LINK_JOINING;
 wifi_link_t state=desk_wifi->status(context);
 if(state!=WIFI_LINK_DOWN)portable_desk_adapter_retain();
 return state;
}
static bool desk_ble_enabled(void *context,bool enabled) {
 if(native_sleep_retained)return false;
 if(!desk_ble->set_enabled(context,enabled)){portable_desk_adapter_retain();return false;}
 desk_ble_wanted=enabled;return true;
}
static bool desk_ble_status(void *context,uint8_t *out) {
 if(native_sleep_retained)return false;
 if(!desk_ble->status(context,out) || *out!=(desk_ble_wanted?PORTABLE_BLUETOOTH_ON:PORTABLE_BLUETOOTH_OFF)){portable_desk_adapter_retain();return false;}
 return true;
}
#endif
#ifdef PORTABLE_X4_IDLE_POLICY
/* Transient settings clients must stop at a lost native KV context as well as
 * at failed grant acquisition. Later callbacks observe the same fence. */
typedef struct {bool live;const risc_key_value_v1 *source;risc_key_value_v1 api;} desk_kv_slot;
static desk_kv_slot desk_kv_slots[16];
static void desk_kv_reset(void){memset(desk_kv_slots,0,sizeof(desk_kv_slots));}
static int32_t desk_kv_result(int32_t rc) {
 if(rc!=RISC_KEY_VALUE_OK && rc!=RISC_KEY_VALUE_NOT_FOUND && rc!=RISC_KEY_VALUE_BUFFER_SMALL &&
    rc!=RISC_KEY_VALUE_INVALID && rc!=RISC_KEY_VALUE_IO)portable_desk_adapter_retain();
 return rc;
}
static int32_t desk_kv_get(void *context,const char *key,void *data,uint32_t size,uint32_t *used) {
 desk_kv_slot *s=context;if(native_sleep_retained || !s->live)return RISC_KEY_VALUE_CONTEXT;
#ifdef PORTABLE_CONTEXTS_CLIENT
 if(!contexts_before_storage())return RISC_KEY_VALUE_CONTEXT;
#endif
#ifdef PORTABLE_BLE_BROADCAST
 if(!broadcast_before_storage())return RISC_KEY_VALUE_CONTEXT;
#endif
 return desk_kv_result(s->source->get(s->source->context,key,data,size,used));
}
static int32_t desk_kv_put(void *context,const char *key,const void *data,uint32_t size) {
 desk_kv_slot *s=context;if(native_sleep_retained || !s->live)return RISC_KEY_VALUE_CONTEXT;
#ifdef PORTABLE_CONTEXTS_CLIENT
 if(!contexts_before_storage())return RISC_KEY_VALUE_CONTEXT;
#endif
#ifdef PORTABLE_BLE_BROADCAST
 if(!broadcast_before_storage())return RISC_KEY_VALUE_CONTEXT;
#endif
 return desk_kv_result(s->source->put(s->source->context,key,data,size));
}
#endif
static bool desk_acquire(const char *name,uint32_t version,uint64_t instance,
                         risc_runtime_capability_v1 *grant) {
 if(native_sleep_retained || desk_phase==DESK_FINALIZED)return false;
 if(!desk_runtime->acquire(name,version,instance,grant)) {
#ifdef PORTABLE_CONTEXTS_CLIENT
  if(!strcmp(name,CONTEXTS_SERVICE_CAPABILITY) && version==1 && instance==0 &&
     grant->struct_size==sizeof(*grant) && !grant->api && !grant->slot && !grant->generation)return false;
#endif
  /* False, including an empty output, does not prove failed-start rollback.
   * Do not release even an apparently valid partial grant after this fence. */
  portable_desk_adapter_retain();grant->api=NULL;return false;
 }
#ifdef PORTABLE_X4_IDLE_POLICY
 if(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)) {
  const risc_key_value_v1 *kv=grant->api;
  if(!portable_sleep_api_valid(kv)){portable_desk_adapter_retain();return false;}
  desk_kv_slot *slot=NULL;
  for(unsigned i=0;i<16;++i)if(!desk_kv_slots[i].live){slot=&desk_kv_slots[i];break;}
  if(!slot){portable_desk_adapter_retain();return false;}
  slot->source=kv;slot->live=true;slot->api=*kv;slot->api.context=slot;
  slot->api.get=desk_kv_get;slot->api.put=desk_kv_put;grant->api=&slot->api;
 }
#endif
 return true;
}
#ifdef PORTABLE_QUICK_RADIOS
static bool desk_quick_acquire(const char *name,uint32_t version,uint64_t instance,
                               risc_runtime_capability_v1 *grant) {
 if(!desk_acquire(name,version,instance,grant))return false;
 if(!strcmp(name,"net.wifi") && grant->api) {
  const wifi_api_v1 *w=grant->api;
  if(w->api_version==1 && w->struct_size>=sizeof(*w) && w->status && w->disconnect_checked) {
   desk_wifi=w;desk_wifi_control=*w;desk_wifi_control.struct_size=sizeof(desk_wifi_control);
   desk_wifi_control.status=desk_wifi_status;desk_wifi_control.disconnect_checked=desk_wifi_off;
   grant->api=&desk_wifi_control;
  }
 }
 if(!strcmp(name,"bluetooth.hci") && grant->api) {
  const portable_bluetooth_control_v1 *b=grant->api;
  if(b->api_version==1 && b->struct_size>=sizeof(*b) && b->set_enabled && b->status) {
   desk_ble=b;desk_ble_control=*b;desk_ble_control.struct_size=sizeof(desk_ble_control);
   desk_ble_control.set_enabled=desk_ble_enabled;desk_ble_control.status=desk_ble_status;
   grant->api=&desk_ble_control;
  }
 }
 return true;
}
/* Apply guarded prefixes only to these known QuickActions consumers. The
 * general Runtime table keeps the complete canonical provider suffixes for
 * the deployment-local sleep hook and all other app-local clients. */
#undef pqa_radios_load
#undef pqa_radios_apply
#undef pqa_radios_suspend
#undef pqa_radios_resume
static bool desk_radios_load(pqa_radios *state,pqa_state *ui,const risc_runtime_api_v1 *runtime) {
 (void)runtime;
 if(native_sleep_retained)return false;
 if(!pqa_radios_load(state,ui,&desk_quick_runtime)){portable_desk_adapter_retain();return false;}
 return !native_sleep_retained;
}
static bool desk_radios_apply(pqa_radios *state,pqa_state *ui,const risc_runtime_api_v1 *runtime,uint32_t actions) {
 (void)runtime;
 if(native_sleep_retained)return false;
 if(!pqa_radios_apply(state,ui,&desk_quick_runtime,actions)){portable_desk_adapter_retain();return false;}
 return !native_sleep_retained;
}
static bool desk_radios_suspend(const risc_runtime_api_v1 *runtime) {
 (void)runtime;
 if(native_sleep_retained)return false;
 if(!pqa_radios_suspend(&desk_quick_runtime)){portable_desk_adapter_retain();return false;}
 return !native_sleep_retained;
}
static bool desk_radios_resume(pqa_radios *state,pqa_state *ui,const risc_runtime_api_v1 *runtime) {
 (void)runtime;
 if(native_sleep_retained)return false;
 if(!pqa_radios_resume(state,ui,&desk_quick_runtime)){portable_desk_adapter_retain();return false;}
 return !native_sleep_retained;
}
#endif
static bool desk_release(risc_runtime_capability_v1 *grant) {
 if(native_sleep_retained)return false;
#ifdef PORTABLE_QUICK_RADIOS
 if(grant->api==&desk_wifi_control)grant->api=desk_wifi;
 if(grant->api==&desk_ble_control)grant->api=desk_ble;
#endif
#ifdef PORTABLE_X4_IDLE_POLICY
 desk_kv_slot *slot=NULL;
 for(unsigned i=0;i<16;++i)if(grant->api==&desk_kv_slots[i].api){slot=&desk_kv_slots[i];grant->api=slot->source;break;}
#endif
 if(!desk_runtime->release(grant)){portable_desk_adapter_retain();return false;}
#ifdef PORTABLE_CONTEXTS_CLIENT
 if(grant->struct_size!=sizeof(*grant) || grant->api || grant->slot || grant->generation) {
  portable_desk_adapter_retain();return false;
 }
#endif
#ifdef PORTABLE_X4_IDLE_POLICY
 if(slot)slot->live=false;
#endif
 return true;
}
bool portable_desk_adapter_timer_only(void) {
 return desk_phase==DESK_TIMER && !failed;
}
static int desk_start_error(void) {
 failed=true;desk_phase=DESK_FAILED;
 return native_sleep_retained?-2:0;
}
static int desk_open_foreground(void) {
#ifdef PORTABLE_BLE_BROADCAST
 if(!portable_broadcast_stop())return desk_start_error();
#endif
 /* A failure after a subscription/claim may conceal retained native state.
  * No cleanup is attempted on these ambiguous outcomes. */
 if(!portable_touch_open(&touch,rt)){portable_desk_adapter_retain();return desk_start_error();}
#ifdef PORTABLE_INPUT_NAVIGATION
 if(!input_navigation_open()){portable_desk_adapter_retain();return desk_start_error();}
#endif
 if(!rt->acquire("board.battery",1,0,&bg))return desk_start_error();
 gauge=bg.api;
 if(!gauge || gauge->api_version!=1 || gauge->struct_size<sizeof(*gauge) || !gauge->read)
  return desk_start_error();
#ifdef PORTABLE_QUICK_ACTIONS
 quick.ui.paper=pp_enabled();
 if(!pqa_session_load(&quick,rt) || native_sleep_retained)return desk_start_error();
#ifdef PORTABLE_DESK_WAKE_LIGHT
 /* The selected deployment cached this native wake classification before
  * opening providers. Restore the persisted on/off and exact saved level only
  * for an interactive GPIO wake, never a minute timer or cold boot. */
 if(portable_desk_clock_boot_is_interactive() &&
    !pqa_session_sync_brightness(&quick,display)) {
  portable_desk_adapter_retain();return desk_start_error();
 }
#endif
 quick_paper_capabilities();
#ifdef PORTABLE_FRONTLIGHT_TONE
 /* Configure tone without changing cold-boot/off brightness. Sparse timer
  * execution never enters this foreground-only capability path. */
 if(quick.ui.paper && !pqa_session_sync_tone(&quick,display)) {
  portable_desk_adapter_retain();return desk_start_error();
 }
#endif
#ifdef PORTABLE_QUICK_RADIOS
 if(!desk_radios_load(&quick_radios,&quick.ui,rt) || native_sleep_retained) {
  portable_desk_adapter_retain();return desk_start_error();
 }
 desk_radios_loaded=true;
#endif
 pqa_cancel_input(&quick.ui);
 quick_replay_pending=quick_replay_delivery=quick_modal=quick_launch_pending=false;
#endif
 /* Do not invalidate/reacquire display or damage history during upgrade. */
 input_sample=(portable_touch_sample){0};input_pending=false;navigation_pending=0;
 home_pending=crown_pending=false;clear_contact_snapshots();desk_touch_neutral=false;
 input_reset();
#ifdef PORTABLE_INPUT_NAVIGATION
 navigation_neutral=false;
#endif
#if defined(PORTABLE_NOVA_UI) && !defined(PORTABLE_APP_OWNS_TOUCH_CHROME)
 nu_gesture=false;
#endif
 last_activity=last_poll_at=input_sampled_at=millis_now();
 if(failed)return desk_start_error();
 desk_phase=DESK_FOREGROUND;return 1;
}
int portable_desk_adapter_start(unsigned mode) {
 if(native_sleep_retained)return -2;
 if(mode!=PORTABLE_DESK_START_TIMER && mode!=PORTABLE_DESK_START_FOREGROUND)return 0;
 unsigned wanted=mode==PORTABLE_DESK_START_TIMER?DESK_TIMER:DESK_FOREGROUND;
 if(desk_phase==wanted && !failed)return 1;
 if(desk_phase!=DESK_INITIALIZED || failed)return 0;
 desk_phase=DESK_STARTING;
 if(!rt->acquire("display.output",1,0,&dg))return desk_start_error();
 display=dg.api;
 if(!display || display->api_version!=1 || display->struct_size<sizeof(*display) ||
    !display->get_info || !display->acquire || !display->release ||
    !display->submit || !display->present_status)return desk_start_error();
 if(!display->get_info(display->context,&info) || info.width<160 || info.height<160 ||
    info.width>1024 || info.height>1024 ||
    !(info.supported_formats&(RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565)|
                              RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_MONO1))))return desk_start_error();
 surface_format=(info.supported_formats&RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565))?
     RISC_DISPLAY_FORMAT_RGB565:RISC_DISPLAY_FORMAT_MONO1;
 paper_rotated=PORTABLE_DISPLAY_ROTATION==90 && surface_format==RISC_DISPLAY_FORMAT_MONO1 &&
     (info.flags&RISC_DISPLAY_INFO_RETAINS_IMAGE) && info.width>info.height;
 if(surface_format==RISC_DISPLAY_FORMAT_MONO1 && !pp_enabled())return desk_start_error();
#ifdef PORTABLE_NOVA_UI
 if(!pp_enabled() && (info.width!=240 || info.height!=240))return desk_start_error();
#endif
 alarm_pixels=malloc((size_t)(surface_format==RISC_DISPLAY_FORMAT_MONO1?(info.width+7)/8:info.width*2)*info.height);
 if(!alarm_pixels || !portable_alarm_open(&alarms,rt))return desk_start_error();
#ifndef PORTABLE_FORCE_FULL_FRAMES
 if(surface_format==RISC_DISPLAY_FORMAT_MONO1 && (info.flags&RISC_DISPLAY_INFO_PARTIAL_DAMAGE))
  paper_previous=malloc((size_t)((info.width+7)/8)*info.height);
 if(surface_format==RISC_DISPLAY_FORMAT_RGB565 && (info.flags&RISC_DISPLAY_INFO_PARTIAL_DAMAGE) && info.width<=320 && info.height<=320)
  previous_pixels=malloc((size_t)info.width*info.height*2);
#endif
 if(wanted==DESK_TIMER){desk_phase=DESK_TIMER;return 1;}
 return desk_open_foreground();
}
int portable_desk_adapter_upgrade(int32_t promotion_status) {
 if(native_sleep_retained)return -2;
 /* These are Runtime0.1.50's result values, not evidence supplied by hardware.
  * The caller owns the live promotion grant and invokes promotion beforehand. */
 if(promotion_status==-4){portable_desk_adapter_retain();return -2;}
 if(promotion_status!=0 && promotion_status!=1)return 0;
 if(desk_phase==DESK_FOREGROUND && !failed)return 1;
 if(desk_phase!=DESK_TIMER || failed || surface.frame || !display_settled)return 0;
 desk_phase=DESK_STARTING;
 return desk_open_foreground();
}
static void desk_finalize(void) {
 if(!rt || native_sleep_retained || desk_phase==DESK_COLD || desk_phase==DESK_FINALIZED)return;
 if(desk_phase==DESK_INITIALIZED){desk_phase=DESK_FINALIZED;return;}
 if(paper_token && !portable_paper_frame_drain())return;
#ifdef PORTABLE_CONTEXTS_CLIENT
 if(!contexts_suspend())return;
#endif
#ifdef PORTABLE_BLE_BROADCAST
 if(broadcast_client.runtime && !portable_broadcast_stop())return;
#endif
 if(alarms.api && !alarm_failed_cleaned) {
  bool stop=failed || !display_settled;
  if(!stop) {
   if(!portable_alarm_status(&alarms) || alarms.status.output_uncertain){portable_desk_adapter_retain();return;}
   stop=portable_alarm_owned(&alarms);
  }
  if(stop && !portable_alarm_failure_stop(&alarms)){portable_desk_adapter_retain();return;}
 }
 if(!portable_alarm_close(&alarms,rt))return;
#ifdef PORTABLE_QUICK_ACTIONS
 if(quick.ui.torch && !pqa_session_restore(&quick,display)){portable_desk_adapter_retain();return;}
#endif
#ifdef PORTABLE_RASTER_SNAPSHOT
 raster_discard();
#endif
 if(surface.frame){display->release(display->context,surface.frame);surface.frame=0;}
#ifdef PORTABLE_INPUT_NAVIGATION
 if(navigation_ready) {
  if(!navigation->foreground(navigation->context,NULL,0) || !navigation->reset(navigation->context)) {
   portable_desk_adapter_retain();return;
  }
  navigation_ready=false;
 }
 if(navigation_grant.api && !rt->release(&navigation_grant))return;
 navigation=NULL;
#endif
 if(!portable_touch_close(&touch,rt)){portable_desk_adapter_retain();return;}
 np_close();if(native_sleep_retained)return;
 if(bg.api && !rt->release(&bg))return;
 if(dg.api && !rt->release(&dg))return;
 free(alarm_pixels);alarm_pixels=NULL;alarm_pixels_valid=false;
#ifdef PORTABLE_QUICK_ACTIONS
 free(quick_background);quick_background=NULL;
#endif
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
 handoff_finish();
#endif
#ifdef PORTABLE_PAPER_CROSSFADE
 paper_handoff_finish();
#endif
 free(previous_pixels);previous_pixels=NULL;previous_valid=false;
 free(paper_previous);paper_previous=NULL;paper_previous_valid=false;
 gauge=NULL;display=NULL;desk_phase=DESK_FINALIZED;
}
