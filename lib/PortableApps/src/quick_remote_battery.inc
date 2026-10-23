#include <stdio.h>
/* Local, resident-owned opt-in. Opening this view never starts the radio.
 * Selected addresses identify one observed advertiser session, not an owner. */
#define QUICK_REMOTE_ACTIONS (PQA_REMOTE_OPEN|PQA_REMOTE_BACK|PQA_REMOTE_SCAN|PQA_REMOTE_STOP|PQA_REMOTE_SELECT|PQA_REMOTE_PREVIOUS|PQA_REMOTE_NEXT)
typedef struct {
 pqb_status status;
 bool active,selected,last_valid,current;
 uint8_t address[6],type,percent;
 uint32_t count;
 pqb_candidate rows[PQB_MAX_CANDIDATES];
} quick_remote_view;
static quick_remote_view quick_remote_shown;
static bool quick_remote_restore_reported;
static void quick_remote_sync(void) {
 if(quick_remote.restoration_failed && !quick_remote_restore_reported) {
  quick.ui.radios_valid=false;quick.ui.bluetooth_enabled=false;quick.ui.error_flags|=PQA_ERROR_RADIO;
  quick_remote_restore_reported=true;
 } else if(!quick_remote.restoration_failed)quick_remote_restore_reported=false;
 quick_remote_view v;memset(&v,0,sizeof(v));
 v.status=quick_remote.status;v.active=quick_remote.active;v.selected=quick_remote.selected;
 v.last_valid=quick_remote.last_valid;v.current=quick_remote.current;v.percent=quick_remote.last_percent;
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
 if(!quick_remote.retained)return true;
 /* Do not release or use another provider once cleanup has retained custody. */
 portable_adapter_retain_silent();return false;
}
static bool quick_remote_close(void) {
 if(!pqb_close(&quick_remote)){(void)quick_remote_safe();return false;}
 quick_remote_sync();return true;
}
static bool quick_remote_actions(uint32_t *actions) {
 uint32_t selected=*actions&QUICK_REMOTE_ACTIONS;*actions&=~(uint32_t)QUICK_REMOTE_ACTIONS;
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
 if(quick_remote.current)return "CURRENT: FRESH SCAN READING";
 switch(quick_remote.status) {
 case PQB_DISABLED:return "BLUETOOTH OFF / AIRPLANE";
 case PQB_UNAVAILABLE:return "SENSOR CAPABILITY UNAVAILABLE";
 case PQB_ERROR:return "SCAN FAILED - LAST VALUE KEPT";
 case PQB_RETAINED:return "SCAN CLEANUP UNCONFIRMED";
 default:return quick_remote.active?"SCANNING - NOT CURRENT":"NOT CURRENT - MANUAL REFRESH";
 }
}
static void quick_remote_identity(char out[32],const uint8_t address[6],uint8_t type) {
 snprintf(out,32,"%02X:%02X:%02X:%02X:%02X:%02X / %u",address[0],address[1],address[2],address[3],address[4],address[5],type);
}
