#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../lib/PortableApps/src/PortableHomeSleep.c"
static const char *scenario;
static bool context_lost;
static bool terminal,timer_only,held,input_hold,power_prepared,panel_prepared,ticket_ready,staged;
static unsigned calls,entries,resumes,clears,radio_acquires,ui_restores;
static uint32_t now=100,entered_ms;
static alarm_sleep_v1 ticket;
static uint8_t staged_bytes[PORTABLE_DESK_CLOCK_RECORD_BYTES];
static bool is(const char*s){return !strcmp(scenario,s);}
static void safe(void){assert(!terminal&&!context_lost);calls++;}
void portable_adapter_retain_silent(void){terminal=true;}
bool portable_adapter_retained(void){return terminal;}
bool portable_desk_adapter_timer_only(void){return timer_only;}
bool portable_desk_clock_lock_requested(void){return true;}
void portable_desk_clock_radios_off(void){safe();assert(!timer_only);}
void portable_desk_clock_refused(void){safe();}
int portable_desk_clock_run(const risc_runtime_api_v1*r,const portable_desk_sleep_ops*o){(void)r;(void)o;assert(0);return 0;}
bool portable_home_canvas_sleep_available(void){return true;}
int portable_home_canvas_prepare_sleep(void){safe();assert(ticket_ready&&!held);input_hold=true;if(is("input-retained")){terminal=true;return -2;}return is("input-refused")?0:1;}
int portable_home_canvas_resume_sleep(void){safe();assert(!held&&!power_prepared&&!panel_prepared);input_hold=false;return is("input-restore")?-2:1;}
static bool prepare_panel(void*p){(void)p;safe();assert(!held&&!ticket_ready);panel_prepared=true;if(is("panel-context"))context_lost=true;return !is("panel-refused");}
static bool prepare_power(void*p){(void)p;safe();assert(panel_prepared&&!held);power_prepared=true;if(is("power-context"))context_lost=true;return !is("power-refused");}
static bool restore_power(void*p){(void)p;safe();power_prepared=false;return !is("power-restore");}
static int32_t restore_panel(void*p){(void)p;safe();assert(!power_prepared);held=false;panel_prepared=false;return is("panel-restore")?-1:0;}
static bool wake_pending(void*p,bool*out){(void)p;safe();*out=is("cancelled");if(is("wake-retained")){terminal=true;return false;}return true;}
static int32_t hold_panel(void*p){(void)p;safe();assert(ticket_ready&&staged&&input_hold);if(is("hold-retained")){terminal=true;return RISC_DEEP_SLEEP_RETAINED;}if(is("hold-refused"))return RISC_DEEP_SLEEP_BUSY;held=true;if(is("hold-context"))context_lost=true;return 0;}
static int32_t deep(void*p,uint32_t ms){(void)p;safe();assert(held&&staged&&input_hold&&ticket_ready);entries++;entered_ms=ms;if(is("entry-retained")){terminal=true;return RISC_DEEP_SLEEP_RETAINED;}if(is("entry-context"))context_lost=true;return is("entry-zero")?0:RISC_DEEP_SLEEP_BUSY;}
static int32_t light(void*p,uint32_t ms,risc_light_sleep_result_v1*out){(void)p;safe();assert(ticket_ready&&input_hold&&!held);entries++;entered_ms=ms;out->struct_size=sizeof(*out);if(is("light-retained")){terminal=true;return RISC_LIGHT_SLEEP_RETAINED;}if(is("light-context"))context_lost=true;return is("light-refused")?RISC_LIGHT_SLEEP_BUSY:RISC_LIGHT_SLEEP_OK;}
static risc_display_power_api_v1 panel={.api_version=1,.struct_size=sizeof(panel),.prepare_sleep=prepare_panel,.prepare_deep_sleep=hold_panel,.resume_status=restore_panel};
static risc_power_sleep_api_v1 power={.api_version=1,.struct_size=sizeof(power),.prepare_sleep=prepare_power,.resume=restore_power,.sleep_wake_pending=wake_pending,.deep_sleep_for=deep,.light_sleep_for=light};
static int32_t read_record(void*p,uint32_t t,uint32_t s,risc_retained_wake_record_v1*out,uint32_t*c){(void)p;(void)t;(void)s;(void)out;safe();*c=RISC_BOOT_RESET;return RISC_RETAINED_WAKE_ABSENT;}
static int32_t stage_record(void*p,const risc_retained_wake_record_v1*r){(void)p;(void)r;assert(0);return -1;}
static int32_t clear_record(void*p){(void)p;safe();assert(!held&&!input_hold&&!power_prepared&&!panel_prepared);clears++;staged=false;return is("clear-retained")?RISC_RETAINED_WAKE_CONTEXT:0;}
static int32_t read_bytes(void*p,uint32_t t,uint32_t s,void*out,uint32_t cap,uint32_t*size,uint32_t*c){(void)p;(void)t;(void)s;(void)out;(void)cap;(void)size;safe();*c=RISC_BOOT_RESET;return RISC_RETAINED_WAKE_ABSENT;}
static int32_t stage_bytes(void*p,uint32_t type,uint32_t schema,const void*bytes,uint32_t size){(void)p;safe();assert(!held&&!input_hold&&!panel_prepared&&!power_prepared&&type==PORTABLE_DESK_CLOCK_RECORD_TYPE&&schema==3&&size==408);memcpy(staged_bytes,bytes,size);staged=true;if(is("stage-retained")){terminal=true;return RISC_RETAINED_WAKE_CONTEXT;}return is("stage-refused")?RISC_RETAINED_WAKE_INVALID:0;}
static risc_retained_wake_api_v1_extended checkpoint={{1,sizeof(checkpoint),NULL,read_record,stage_record,clear_record},RISC_RETAINED_WAKE_EXTENDED_TAG,1,512,0,read_bytes,stage_bytes};
static bool radio_disconnect(void*p){(void)p;safe();assert(!timer_only&&!ticket_ready&&!held);if(is("light-radio-retained")){terminal=true;return false;}if(is("light-radio-context"))context_lost=true;return true;}
static wifi_link_t radio_status(void*p){(void)p;safe();return WIFI_LINK_DOWN;}
static bool bluetooth_off(void*p,bool enabled){(void)p;safe();assert(!enabled&&!timer_only&&!ticket_ready&&!held);return true;}
static bool bluetooth_status(void*p,uint8_t*out){(void)p;safe();*out=PORTABLE_BLUETOOTH_OFF;return true;}
static const wifi_api_v1 wifi={.api_version=1,.struct_size=sizeof(wifi),.disconnect_checked=radio_disconnect,.status=radio_status};
static const portable_bluetooth_control_v1 bluetooth={.api_version=1,.struct_size=sizeof(bluetooth),.set_enabled=bluetooth_off,.status=bluetooth_status};
static bool acquire(const char*name,uint32_t version,uint64_t instance,risc_runtime_capability_v1*g){safe();assert(version==1&&instance==0&&!held);if(!strcmp(name,"net.wifi")||!strcmp(name,"bluetooth.hci")){assert(!timer_only);radio_acquires++;
 bool has_wifi=is("light-radio-context")||is("light-wifi-only")||is("light-radio-retained")||is("light-radio-release-failed");
 bool has_ble=is("light-ble-only");
 if((!strcmp(name,"net.wifi")&&has_wifi)||(!strcmp(name,"bluetooth.hci")&&has_ble)){
  g->api=!strcmp(name,"net.wifi")?(const void*)&wifi:(const void*)&bluetooth;g->slot=4;g->generation=1;return true;
 }
 return false;}if((is("absent")&&!strcmp(name,"power.sleep"))||(!strncmp(scenario,"boot-",5)&&!strcmp(name,RISC_RETAINED_WAKE_CAPABILITY))){
 if(is("boot-context"))context_lost=true;
 if(is("boot-dirty"))g->slot=1;
 return false;
 }g->api=is("null-table")?NULL:!strcmp(name,"power.sleep")?(void*)&power:!strcmp(name,"display.power")?(void*)&panel:(void*)&checkpoint;g->slot=1;g->generation=1;return true;}
static unsigned releases;
static bool release(risc_runtime_capability_v1*g){safe();assert(!held&&!input_hold);releases++;if(is("release-failed")||(is("light-radio-release-failed")&&g->slot==4))return false;memset(g,0,sizeof(*g));return true;}
static bool health(risc_runtime_health_v1*out){safe();out->uptime_ms=now;return true;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release,.health=health};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){assert(version==1);return context_lost?NULL:&runtime;}
int32_t portable_home_boot_read(const risc_retained_wake_api_v1*w,uint32_t t,uint32_t s,risc_retained_wake_record_v1*r,uint32_t*c){(void)w;(void)t;(void)s;(void)r;(void)c;assert(0);return -1;}
int32_t portable_home_boot_read_bytes(const risc_retained_wake_api_v1_extended*w,uint32_t t,uint32_t s,void*p,uint32_t cap,uint32_t*n,uint32_t*c){(void)w;(void)t;(void)s;(void)p;(void)cap;(void)n;(void)c;assert(0);return -1;}
static int32_t alarm_status(void*p,alarm_status_v1*out){(void)p;safe();assert(!held);out->error=0;out->output_uncertain=0;return 0;}
static int32_t alarm_simple(void*p){(void)p;safe();assert(!held);return 0;}
static int32_t alarm_ack(void*p,const alarm_token_v1*t){(void)t;return alarm_simple(p);}
static int32_t alarm_prepare(void*p,alarm_sleep_v1*out){(void)p;safe();assert(!held&&!input_hold&&power_prepared);if(is("alarm-retained")||is("alarm-unknown")){terminal=true;return is("alarm-unknown")?-99:ALARM_RETAINED;}
 if(is("alarm-context"))context_lost=true;ticket=(alarm_sleep_v1){sizeof(ticket),7,1000,1010};*out=ticket;ticket_ready=true;return 0;}
static int32_t alarm_resume(void*p,const alarm_sleep_v1*t){(void)p;safe();assert(!held&&!input_hold&&!panel_prepared&&!power_prepared&&!memcmp(t,&ticket,sizeof(ticket)));resumes++;return 0;}
static const alarm_service_descriptor_v2 alarm={{2,sizeof(alarm),NULL,alarm_status,alarm_simple,alarm_simple,alarm_ack,alarm_prepare,alarm_simple},ALARM_SERVICE_DESCRIPTOR_TAG,1,0,ALARM_DESCRIPTOR_RESUME_SLEEP,alarm_resume};
static portable_idle_ui_result begin_ui(void*p){(void)p;safe();assert(ticket_ready&&!held);return is("ui-refused")?PORTABLE_IDLE_UI_REFUSED:PORTABLE_IDLE_UI_READY;}
static portable_idle_ui_result restore_ui(void*p){(void)p;safe();assert(!held&&!input_hold&&!power_prepared&&!panel_prepared);ui_restores++;return PORTABLE_IDLE_UI_READY;}
int main(int argc,char**argv){
 assert(argc==2);scenario=argv[1];timer_only=strncmp(scenario,"light",5)&&strncmp(scenario,"ui",2)&&!is("absent");
 if(!strncmp(scenario,"boot-",5)){
  portable_desk_record record={0};
  assert(portable_desk_clock_boot_read(&runtime,&record)==(is("boot-absent")?0:-2));
  assert(!releases&&!entries);return 0;
 }
 if(is("wake-clear-missing"))checkpoint.base.clear=NULL;
 if(is("null-table")||is("release-failed")||is("wake-clear-missing")){
  assert(portable_home_sleep_admit(&runtime)==1);
  if(is("null-table"))assert(!portable_home_sleep_modes());
  if(is("wake-clear-missing"))assert(portable_home_sleep_modes()==PORTABLE_HOME_MODE_LIGHT);
  assert(portable_home_sleep_close()==(is("release-failed")?-2:1));
  assert(releases==(is("release-failed")?1u:3u));
 }else if(is("alarm-no-resume")){
  alarm_service_descriptor_v2 copy=alarm;copy.features=0;copy.resume_sleep=NULL;
  assert(portable_home_light_sleep_with_ui(&runtime,NULL,NULL,&copy.base,NULL)==0&&!calls&&!entries);
 }else if(!strncmp(scenario,"light",5)||is("ui-refused")||is("absent")){
  const portable_idle_sleep_ui ui={NULL,begin_ui,restore_ui};
  int result=portable_home_light_sleep_with_ui(&runtime,NULL,NULL,&alarm.base,&ui);
  if(is("light-context")||is("light-radio-context")||is("light-retained")||is("light-radio-retained")||is("light-radio-release-failed"))assert(result==-2&&terminal&&!resumes&&!ui_restores);
  else{bool slept=is("light")||is("light-wifi-only")||is("light-ble-only");assert(!terminal);assert(result==(slept?1:0));assert(resumes==(slept?1u:0u));assert(portable_home_sleep_close()==1);}
 }else{
  assert(portable_home_sleep_admit(&runtime)==1);
  portable_desk_config config={.face=0,.time_format=1};strcpy(config.time_zone,"UTC");
  portable_desk_record record={.config=config,.displayed_minute=INT64_C(1704067200),.refresh_modulo=1,.has_image=true};
  home_desk_sleep state={.rt=&runtime,.alarms=&alarm.base,.wake=&checkpoint.base,.timer_only=true,.physical={.power=&power,.display=&panel,.call_safe=home_context_safe}};
  int result=desk_stage(&state,&record);
  if(result==1)result=desk_prepare(&state);
  if(result==1){
   if(is("expired"))now+=10000;
   if(is("rewound"))now=1;
   if(is("record-changed"))record.refresh_modulo++;
   result=desk_stage_enter(&state,&record,60000,100);
  }
  if(result==-2){assert(terminal);unsigned before=calls;assert(desk_resume(&state)==-2&&calls==before);}
  else{assert(desk_resume(&state)==1||terminal);if(!terminal){assert(!held&&!input_hold&&!staged);assert(portable_home_sleep_close()==1);}}
  assert(!radio_acquires);
  if(is("normal"))assert(entries==1&&entered_ms==9000&&clears==1);
  if(is("expired")||is("rewound")||is("record-changed"))assert(!entries);
 }
 printf("Common Home sleep %s: exact408 schema, deadline, alarm ticket and custody PASS\n",scenario);
 return 0;
}
