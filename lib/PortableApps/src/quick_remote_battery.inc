#include <stdio.h>
#include "PortableRemoteBatteryCache.h"
static pqb_cache quick_remote_cache;
static bool quick_remote_clock_valid;
static uint32_t quick_remote_clock_raw;
static uint64_t quick_remote_clock_total;
static bool quick_storage_safe(void);
static bool quick_remote_alive(void *unused) {
 (void)unused;
 if(failed||native_custody_retained||native_sleep_retained)return false;
 if(!native_custody_runtime||risc_runtime_get_api(1)!=native_custody_runtime){
  native_custody_retained=native_sleep_retained=failed=true;return false;
 }
 return true;
}
static bool quick_remote_tick(void) {
 if(!quick_remote_alive(NULL))return false;
 if(native_custody_runtime->struct_size<RISC_RUNTIME_MONOTONIC_V1_SIZE||!native_custody_runtime->monotonic_ms)return false;
 uint32_t now=UINT32_MAX;bool ok=native_custody_runtime->monotonic_ms(&now);
 if(!quick_remote_alive(NULL))return false;
 if(!ok){portable_adapter_retain_silent();
 return false;}
 uint32_t delta=now-quick_remote_clock_raw;
 if(quick_remote_clock_valid&&delta>INT32_MAX){portable_adapter_retain_silent();
 return false;}
 if(!quick_remote_clock_valid){quick_remote_clock_total=now;quick_remote_clock_valid=true;}
 else quick_remote_clock_total+=delta;
 quick_remote_clock_raw=now;
 if(!pqb_note_time(&quick_remote,quick_remote_clock_total)){portable_adapter_retain_silent();
 return false;}
 return true;
}
static void quick_remote_bind(void) {
 quick_remote.alive=quick_remote_alive;quick_remote.alive_context=NULL;
 quick_remote_cache.alive=quick_remote_alive;quick_remote_cache.context=NULL;
}
static bool quick_remote_storage(bool load) {
 quick_remote_bind();
 if(!quick_remote_alive(NULL))return false;
 if(quick_remote.active||quick_remote.token||quick_remote.grant_owned)return false;
 if(!quick_storage_safe()||raster_lease_mutable())return false;
#if defined(PORTABLE_BLE_BROADCAST) || defined(PORTABLE_CONTEXTS_CLIENT)
 if(!portable_background_stop())return false;
#endif
 bool ok=load?pqb_cache_load(&quick_remote_cache,rt,PQA_STORE_INSTANCE):
  pqb_cache_flush(&quick_remote_cache,rt,PQA_STORE_INSTANCE);
 if(!ok){
  if(!native_custody_retained)portable_adapter_retain_silent();
  return false;
 }
 if(load&&quick_remote_cache.loaded&&quick_remote_cache.valid&&!quick_remote.selected){
  quick_remote.last_valid=true;quick_remote.last_percent=quick_remote_cache.percent;quick_remote.current=false;
 }
 return true;
}
/* Local, resident-owned opt-in. Opening this view never starts the radio.
 * Selected addresses identify one observed advertiser session, not an owner. */
#define QUICK_REMOTE_ACTIONS (PQA_REMOTE_OPEN|PQA_REMOTE_BACK|PQA_REMOTE_SCAN|PQA_REMOTE_STOP|PQA_REMOTE_SELECT|PQA_REMOTE_PREVIOUS|PQA_REMOTE_NEXT)
typedef struct {
 pqb_status status;
 bool active,selected,last_valid,current;
 uint8_t address[6],type,percent;
 uint32_t count,link_state,cache_status;
 pqb_candidate rows[PQB_MAX_CANDIDATES];
} quick_remote_view;
static quick_remote_view quick_remote_shown;
static bool quick_remote_restore_reported;
static void quick_remote_sync(void) {
 quick_remote_bind();
 if(quick_remote.selected&&quick_remote.last_valid)pqb_cache_observe(&quick_remote_cache,quick_remote.last_percent);
 if(quick_remote.restoration_failed && !quick_remote_restore_reported) {
  quick.ui.radios_valid=false;quick.ui.bluetooth_enabled=false;quick.ui.error_flags|=PQA_ERROR_RADIO;
  quick_remote_restore_reported=true;
 } else if(!quick_remote.restoration_failed)quick_remote_restore_reported=false;
 quick_remote_view v;memset(&v,0,sizeof(v));
 v.link_state=quick_remote.link_state;v.cache_status=quick_remote_cache.status;
 v.status=quick_remote.status;v.active=quick_remote.active;v.selected=quick_remote.selected;
 v.last_valid=quick_remote.last_valid;v.current=pqb_recent(&quick_remote);v.percent=quick_remote.last_percent;
 memcpy(v.address,quick_remote.selected_address,6);v.type=quick_remote.selected_address_type;
 v.count=pqb_candidates(&quick_remote,v.rows,PQB_MAX_CANDIDATES);
 if(memcmp(&v,&quick_remote_shown,sizeof(v))) {
  /* A changed row list invalidates captured/queued coordinate selections. */
  if(v.count!=quick_remote_shown.count || memcmp(v.rows,quick_remote_shown.rows,sizeof(v.rows)))
   quick.ui.remote_battery_epoch++;
  quick_remote_shown=v;quick.ui.remote_battery_revision++;
 }
 quick.ui.remote_battery_count=(uint8_t)v.count;
 if(quick.ui.remote_battery_offset>=v.count)quick.ui.remote_battery_offset=0;
}
static bool quick_remote_safe(void) {
 if(!quick_remote.retained&&!quick_remote_cache.retained)return quick_remote_alive(NULL);
 /* Do not release or use another provider once cleanup has retained custody. */
 if(!native_custody_retained)portable_adapter_retain_silent();
 return false;
}
static bool quick_remote_close(void) {
 quick_remote_bind();
 if(!pqb_close(&quick_remote)){(void)quick_remote_safe();return false;}
 quick_remote_sync();
 if(quick_remote_cache.dirty&&!quick_remote_storage(false))return false;
 quick_remote_sync();return true;
}
static bool quick_remote_actions(uint32_t *actions) {
 uint32_t selected=*actions&QUICK_REMOTE_ACTIONS;*actions&=~(uint32_t)QUICK_REMOTE_ACTIONS;
 quick_remote_bind();
 if(!quick_remote_safe())return false;
 if(*actions && !quick_remote_close())return false;
 if(!selected || !quick_modal || !pqa_visible(&quick.ui) || quick.ui.target_q8!=PQA_OPEN_Q8 || alarm_modal || home_pending || crown_pending || handoff_requested ||
    quick.ui.action_remote_epoch!=quick.ui.remote_battery_epoch)return true;
 if(selected&PQA_REMOTE_OPEN) {
  quick.ui.remote_battery_page=true;quick.ui.remote_battery_offset=0;quick.ui.remote_battery_epoch++;
 } else if(quick.ui.remote_battery_page) {
  if(selected&PQA_REMOTE_BACK) {
   if(!quick_remote_close())return false;
   quick.ui.remote_battery_page=false;quick.ui.remote_battery_epoch++;
  } else if(selected&PQA_REMOTE_STOP) {if(!quick_remote_close())return false;}
  else if(selected&PQA_REMOTE_SCAN) {
   if(!quick_remote_close())return false;
   (void)pqb_start(&quick_remote,rt,quick.ui.radios_valid&&quick.ui.bluetooth_enabled&&!quick.ui.airplane);
  } else if(selected&PQA_REMOTE_SELECT) {
   uint32_t index=quick.ui.action_remote_index;
   if(index<quick_remote_shown.count) {
    const pqb_candidate *row=&quick_remote_shown.rows[index];
    if(row->battery_valid)(void)pqb_select(&quick_remote,row->address,row->address_type);
   }
  } else if(selected&PQA_REMOTE_PREVIOUS) {
   if(quick.ui.remote_battery_offset>=6){quick.ui.remote_battery_offset-=6;quick.ui.remote_battery_epoch++;}
  } else if(selected&PQA_REMOTE_NEXT) {
   if((uint32_t)quick.ui.remote_battery_offset+6<quick_remote_shown.count){quick.ui.remote_battery_offset+=6;quick.ui.remote_battery_epoch++;}
  }
 }
 quick_remote_sync();return quick_remote_safe();
}
static bool quick_remote_back(void) {
 if(!quick_remote_close())return false;
 if(quick.ui.remote_battery_page && !home_pending && !crown_pending && !handoff_requested) {
  quick.ui.remote_battery_page=false;quick.ui.remote_battery_epoch++;
 } else pqa_close(&quick.ui);
 return true;
}
static const char *quick_remote_status_text(void) {
 if(quick_remote.restoration_failed)return "RADIO RESTORATION UNCONFIRMED";
 if(pqb_recent(&quick_remote))return "RECENT BATTERY READING";
 if(quick_remote_cache.status==PQB_CACHE_UNCONFIRMED)return "OLD - SAVE UNCONFIRMED";
 if(quick_remote.last_valid&&!quick_remote.selected)return "OLD READING FROM PRIOR SESSION";
 switch(quick_remote.status) {
 case PQB_DISABLED:return "BLUETOOTH OFF / AIRPLANE";
 case PQB_UNAVAILABLE:return "SENSOR CAPABILITY UNAVAILABLE";
 case PQB_ERROR:return "SCAN FAILED - LAST VALUE KEPT";
 case PQB_RETAINED:return "SCAN CLEANUP UNCONFIRMED";
 default:return quick_remote.active?"SCANNING - READING OLD":"OLD - MANUAL REFRESH";
 }
}
static void quick_remote_identity(char out[32],const uint8_t address[6],uint8_t type) {
 snprintf(out,32,"%02X:%02X:%02X:%02X:%02X:%02X / %u",address[0],address[1],address[2],address[3],address[4],address[5],type);
}

static const char *quick_remote_link_text(void) {
 return pqb_link_text(quick_remote.link_state);
}
static const char *quick_remote_link_short(void) {
 return quick_remote.link_state==RISC_BLE_LINK_CONNECTED?"LINKED":
  quick_remote.link_state==RISC_BLE_LINK_NOT_CONNECTED?"NO LINK":"LINK ?";
}
