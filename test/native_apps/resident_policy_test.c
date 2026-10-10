/* Synthetic peripherals and test observations. Runtime, Graph, host policy,
 * foreground adapter, Quick preferences and radio policy are production code. */
#ifdef POLICY_PROVIDER
#include <RiscProviderV2.h>
extern const void *policy_fixture_api(unsigned);
extern void policy_fixture_provider_event(unsigned,unsigned);
static bool start(const risc_provider_dependency_v1 *deps,size_t count){(void)deps;policy_fixture_provider_event(POLICY_PROVIDER,1);return count==1;}
static bool quiesce(void){policy_fixture_provider_event(POLICY_PROVIDER,2);return true;}
static void stop(void){policy_fixture_provider_event(POLICY_PROVIDER,3);}
static risc_driver_v2 driver={2,sizeof(driver),POLICY_CAPABILITY,POLICY_CAPABILITY,POLICY_API,NULL,start,stop,quiesce};
__attribute__((visibility("default"))) const risc_driver_v2 *t5_driver_get(uint32_t abi){driver.capability=policy_fixture_api(POLICY_PROVIDER);return abi==2?&driver:NULL;}
#else
#include "PortableApps.h"
#include "RiscRuntimeV1.h"
#include "RiscResidentShellV1.h"
#include "RiscDisplayOutputSnapshotV1.h"
#include "RiscTouchV1.h"
#include "RiscInputNavigationV1.h"
#include "RiscBatteryGaugeV1.h"
#include "PortableRtcClock.h"
#include "PortableBluetoothControl.h"
#include "PortableAppSleep.h"
#include "PortableLowBattery.h"
#include "WifiApi.h"
#include <AlarmServiceV2.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern unsigned policy_runtime_role(void);
extern bool policy_runtime_reset_safe(void);
static const char *mode;
static unsigned ms,lifecycle_role,focus,frame,token,step,subs[3],opens[3],closes[3],focus_changes;
static unsigned maps[3],unmaps[3],entries[3],polls,light_calls,explicit_calls,battery_reads,edge_writes,guard_calls,audio_pauses;
static unsigned provider_calls,after_terminal,policy_opens,policy_closes,last_focus,activity_retained,activity_consumed;
static unsigned baseline_reads,baseline_focus,baseline_closes,baseline_effects,host_result,policy_effects;
static bool terminal,capture,touch_down,child_started,child_returned;
static void (*busy_hook)(bool);
static uint8_t pixels[48000],completed[48000];
static struct {char key[40];uint8_t bytes[128];uint32_t size;} values[16];
static unsigned value_count;
static unsigned role(void){unsigned current=policy_runtime_role();return current?current:lifecycle_role;}
static bool is(const char *name){return !strcmp(mode,name);}
static void live(void){if(terminal){++after_terminal;assert(!"Provider operation after retention");}++provider_calls;}
static void policy_boundary(void){
 if(child_started&&!child_returned&&role()==1){assert(!frame&&!focus&&!subs[1]&&!subs[2]&&!capture&&audio_pauses);++policy_effects;}
}
const char *policy_fixture_mode(void){return mode;}
bool policy_fixture_safe(void){return !capture;}
bool policy_fixture_terminal(void){return terminal;}
bool policy_fixture_health(risc_runtime_health_v1 *out){live();out->uptime_ms=ms;return true;}
void policy_fixture_delay(uint32_t value){live();ms+=value;}
bool policy_fixture_log(const char *text){if(!terminal)live();(void)text;return true;}
void policy_fixture_busy(bool value){assert(busy_hook);busy_hook(value);}
void policy_fixture_capture(bool value){
 capture=value;
 if(value)last_focus=focus_changes;
 else assert(focus_changes==last_focus&&!policy_effects);
}
void policy_fixture_set_busy_hook(void (*hook)(bool)){busy_hook=hook;}
bool policy_fixture_guard(void){++guard_calls;return !is("dirty-edit");}
static int32_t kv_get(void*c,const char *key,void *out,uint32_t cap,uint32_t *used){
 (void)c;live();assert(!frame);policy_boundary();
 for(unsigned i=0;i<value_count;++i)if(!strcmp(values[i].key,key)){
  *used=values[i].size;if(cap<*used)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(out,values[i].bytes,*used);return RISC_KEY_VALUE_OK;
 }
 *used=0;return RISC_KEY_VALUE_NOT_FOUND;
}
static int32_t kv_put(void*c,const char *key,const void *bytes,uint32_t size){
 (void)c;live();assert(!frame&&size<=128);policy_boundary();
 unsigned i=0;for(;i<value_count;++i)if(!strcmp(values[i].key,key))break;
 if(i==value_count){assert(i<16);++value_count;snprintf(values[i].key,sizeof(values[i].key),"%s",key);}
 memcpy(values[i].bytes,bytes,size);values[i].size=size;
 if(!strcmp(key,PORTABLE_LOW_BATTERY_KEY))++edge_writes;
 return RISC_KEY_VALUE_OK;
}
int32_t policy_fixture_kv_get(uint32_t ns,const char *key,void *out,uint32_t cap,uint32_t *used){assert(ns==1);return kv_get(NULL,key,out,cap,used);}
int32_t policy_fixture_kv_put(uint32_t ns,const char *key,const void *bytes,uint32_t size){assert(ns==1);return kv_put(NULL,key,bytes,size);}
static const risc_key_value_v1 preferences={1,sizeof(preferences),NULL,kv_get,kv_put};
static bool display_info(void*c,risc_display_info_v1*out){live();(void)c;*out=(risc_display_info_v1){.width=480,.height=800,.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_MONO1),.flags=RISC_DISPLAY_INFO_RETAINS_IMAGE|RISC_DISPLAY_INFO_CLEAN_PRESENT|RISC_DISPLAY_INFO_PARTIAL_DAMAGE|RISC_DISPLAY_INFO_BRIGHTNESS,.damage_x_alignment=8,.damage_width_alignment=8};return true;}
static bool acquire_frame(void*c,uint32_t format,risc_display_surface_v1*out){live();(void)c;assert(!frame&&format==RISC_DISPLAY_FORMAT_MONO1);frame=role();memset(pixels,0xcd,sizeof(pixels));*out=(risc_display_surface_v1){.frame=1,.pixels=pixels,.width=480,.height=800,.stride_bytes=60,.size_bytes=sizeof(pixels),.pixel_format=format};return true;}
static void release_frame(void*c,risc_display_frame_v1 value){live();(void)c;assert(value==1&&frame==role());frame=0;}
static bool submit_frame(void*c,risc_display_frame_v1 value,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*out){live();(void)c;(void)r;(void)n;(void)o;assert(value==1&&frame==role());frame=0;*out=++token;memcpy(completed,pixels,sizeof(pixels));return true;}
static bool brightness(void*c,uint16_t value,uint16_t maximum){live();(void)c;assert(!frame&&value<=100&&maximum==100);policy_boundary();return true;}
static bool present_status(void*c,risc_display_present_token_v1 value,risc_display_present_status_v1*out){live();(void)c;assert(value==token);out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static bool history(void*c,risc_display_frame_v1 value){(void)c;(void)value;return false;}
static int32_t power(void*c,uint32_t budget){live();(void)c;(void)budget;return 0;}
static bool metrics(void*c,risc_display_present_metrics_v1*out){(void)c;(void)out;return false;}
static bool snapshot(void*c,uint32_t format,void*out,size_t size,uint32_t stride){live();(void)c;assert(!frame&&format==RISC_DISPLAY_FORMAT_MONO1&&size==sizeof(completed)&&stride==60);memcpy(out,completed,size);return true;}
static const risc_display_output_api_v1_snapshot display={
 .metrics={.power={.history={.base={.api_version=1,.struct_size=sizeof(display),.get_info=display_info,.acquire=acquire_frame,.release=release_frame,.submit=submit_frame,.present_status=present_status,.set_brightness=brightness},.extension_tag=RISC_DISPLAY_HISTORY_TAG,.extension_version=1,.seed_previous=history},.power_tag=RISC_DISPLAY_POWER_TAG,.power_version=1,.prepare=power,.resume=power},.metrics_tag=RISC_DISPLAY_METRICS_TAG,.metrics_version=1,.snapshot=metrics},.snapshot_tag=RISC_DISPLAY_SNAPSHOT_TAG,.snapshot_version=1,.copy_completed=snapshot};
static uint64_t subscribe(void*c){live();(void)c;unsigned r=role();assert(!subs[r]);subs[r]=1;++opens[r];if(child_started&&r==2)++policy_opens;return r;}
static bool unsubscribe(void*c,uint64_t id){live();(void)c;assert(id==role()&&subs[id]);subs[id]=0;++closes[id];if(child_started&&id==2)++policy_closes;return true;}
static bool poll_touch(void*c,size_t count){live();(void)c;assert(count==1);return true;}
static int32_t next_touch(void*c,uint64_t id,risc_touch_event_v1*out){live();(void)c;(void)out;assert(id==role());return 0;}
static bool touch_snapshot(void*c,risc_touch_snapshot_v1*out){live();(void)c;*out=(risc_touch_snapshot_v1){.width=480,.height=800};if(role()==2&&touch_down){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=230,.y=330};}return true;}
static const risc_touch_api_v1 touch={1,sizeof(touch),NULL,subscribe,unsubscribe,poll_touch,next_touch,touch_snapshot};
static bool nav_poll(void*c,risc_input_navigation_frame_v1*out){live();(void)c;assert(focus==role());*out=(risc_input_navigation_frame_v1){0};if(is("explicit-sleep")&&role()==2&&step==2)out->pressed=RISC_NAV_HOME;return true;}
static bool nav_focus(void*c,const risc_input_foreground_v1*claims,size_t count){live();(void)c;assert(count<=1);if(count){assert(claims&&!strcmp(claims[0].capability,"input.touch.raw")&&(!focus||focus==role()));focus=role();}else{assert(!focus||focus==role());focus=0;}++focus_changes;return true;}
static bool nav_reset(void*c){live();(void)c;return true;}
static const risc_input_navigation_api_v1 navigation={1,sizeof(navigation),NULL,nav_poll,nav_focus,nav_reset};
static bool battery_read(void*c,risc_battery_sample_v1*out){live();(void)c;++battery_reads;policy_boundary();*out=(risc_battery_sample_v1){.percent=(is("low-battery")&&step>=2)?8:84};return true;}
static const risc_battery_gauge_api_v1 battery={1,sizeof(battery),NULL,battery_read};
static bool rtc_read(void*c,twatch_rtc_time_v1*out){live();(void)c;*out=(twatch_rtc_time_v1){2026,10,9,5,9,41,0};return true;}
static const twatch_rtc_api_v1 rtc={.api_version=2,.struct_size=sizeof(rtc),.read=rtc_read};
static int32_t alarm_status(void*c,alarm_status_v1*out){live();(void)c;*out=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*out),.state=ALARM_STATE_READY};return ALARM_OK;}
static int32_t alarm_step(void*c){live();(void)c;return ALARM_OK;}
static int32_t alarm_ack(void*c,const alarm_token_v1*t){live();(void)c;(void)t;return ALARM_OK;}
static int32_t alarm_prepare(void*c,alarm_sleep_v1*t){live();(void)c;(void)t;return ALARM_INVALID;}
static const alarm_service_descriptor_v2 alarm={.base={2,sizeof(alarm),NULL,alarm_status,alarm_step,alarm_step,alarm_ack,alarm_prepare,alarm_step},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1,.output_modes=ALARM_MODE_VISUAL};
static wifi_link_t wifi_status(void*c){live();(void)c;return WIFI_LINK_DOWN;}
static bool wifi_stop(void*c){live();(void)c;policy_boundary();return true;}
static const wifi_api_v1 wifi={.api_version=1,.struct_size=sizeof(wifi),.status=wifi_status,.disconnect_checked=wifi_stop};
static bool ble_set(void*c,bool enabled){live();(void)c;assert(!enabled);policy_boundary();return true;}
static bool ble_status(void*c,uint8_t*out){live();(void)c;*out=PORTABLE_BLUETOOTH_OFF;return true;}
static const portable_bluetooth_control_v1 ble={.api_version=1,.struct_size=sizeof(ble),.set_enabled=ble_set,.status=ble_status};
const void *policy_fixture_api(unsigned index){const void *tables[]={NULL,&display,&touch,&navigation,&battery,&rtc,&alarm,&wifi,&ble};assert(index<9);return tables[index];}
void policy_fixture_provider_event(unsigned index,unsigned event){(void)index;(void)event;live();}
bool portable_audio_services_safe(void){return true;}
bool portable_audio_capture_active(void){return capture;}
void portable_audio_capture_resume(void){}
bool portable_audio_suspend(void){live();assert(!capture);++audio_pauses;return true;}
int portable_app_idle_sleep(const risc_runtime_api_v1*rt,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*g,const alarm_service_v1*a){
 (void)rt;assert(d&&g&&alarm_service_descriptor(a));live();policy_boundary();assert(role()==1&&!policy_runtime_reset_safe());++light_calls;
 if(is("retained")){terminal=true;return -2;}return is("refused")?0:1;
}
int portable_app_sleep(const risc_runtime_api_v1*rt,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*g){(void)rt;(void)d;(void)g;assert(false);return -1;}
int portable_app_alarm_sleep(const risc_runtime_api_v1*rt,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*g,const alarm_service_v1*a){
 (void)rt;(void)d;(void)g;(void)a;live();assert(child_returned&&unmaps[2]==1&&policy_runtime_reset_safe());++explicit_calls;return 1;
}
void policy_fixture_step(unsigned value){
 step=value;
 if(is("activity")){
  static const unsigned times[]={0,4400,5500,6600,7700,8800,9900,11000,12100,13200,14300,15400};
  ms=times[value];touch_down=value>=1&&value<=4;
 }else if(is("capture")){
  capture=value>=1&&value<=4;ms=value*2000;touch_down=false;
 }else if(is("poll")||is("busy")||is("explicit-sleep"))ms=value*40;
 else ms=value*1100;
 if(is("busy"))policy_fixture_busy(value<3);
 if(value==1){baseline_reads=battery_reads;baseline_focus=focus_changes;baseline_closes=closes[2];baseline_effects=policy_effects;}
 if(is("capture")&&value==1)last_focus=focus_changes;
}
void policy_fixture_event(unsigned event,unsigned value){
 if(event==10){lifecycle_role=value;++maps[value];return;}
 if(event==11){++unmaps[value];if(value==2)lifecycle_role=1;return;}
 if(event==12){lifecycle_role=value;++entries[value];if(value==2)child_started=true;return;}
 if(event==1){host_result=value;return;}
 if(event==5){assert(value==1&&unmaps[2]==1);return;}
 if(event==4){assert(!guard_calls);return;}
 if(event==3){assert(value==71);capture=false;touch_down=false;child_returned=true;return;}
 assert(event==2);++polls;
 if(is("poll")&&step>=1){assert(battery_reads==baseline_reads&&focus_changes==baseline_focus&&closes[2]==baseline_closes&&policy_effects==baseline_effects&&!light_calls);}
 if(is("busy")&&step<3){assert(value&1u);++activity_retained;}
 if(is("busy")&&step>=3){assert(!(value&1u));++activity_consumed;}
 if(is("activity")&&step<=8)assert(!light_calls);
 if(is("capture")&&step>=1&&step<=4){assert(value&1u);assert(focus_changes==last_focus&&battery_reads==baseline_reads&&!light_calls);}
 if(is("capture")&&step==5){assert(!(value&1u));assert(!light_calls);}
}
void policy_fixture_setup(const char*name){
 mode=name;lifecycle_role=1;
 assert(portable_sleep_timer_save(&preferences,false,5000));
 assert(pqa_preference_save(&preferences,PQA_BRIGHTNESS_KEY,80,0));
}
void policy_fixture_verify(bool retained){
 assert(retained==is("retained"));
 if(retained){assert(terminal&&!after_terminal&&!unmaps[1]&&!unmaps[2]&&light_calls==1&&host_result==0);}
 else{
  assert(!terminal&&!frame&&!subs[1]&&!subs[2]&&!focus&&maps[1]==unmaps[1]&&maps[2]==unmaps[2]&&host_result==2);
  if(is("poll"))assert(polls==12&&battery_reads==1&&!light_calls&&policy_opens==1);
  if(is("busy"))assert(activity_retained==3&&activity_consumed==9&&battery_reads==1);
  if(is("policy-busy"))assert(battery_reads==1&&policy_opens==2);
  if(is("capture-pending"))assert(battery_reads==1&&policy_opens==1);
  if(is("idle")||is("refused")||is("dirty-edit"))assert(light_calls>=1);
  if(is("dirty-edit"))assert(guard_calls==2);
  if(is("low-battery"))assert(edge_writes==1&&battery_reads>=2);
  if(is("activity")||is("capture"))assert(light_calls>=1);
  if(is("explicit-sleep"))assert(explicit_calls==1&&!light_calls&&guard_calls>=1);
 }
 assert(!explicit_calls||is("explicit-sleep"));
 printf("System host/client + production Runtime/Graph: %s polls=%u Light=%u battery=%u touch=%u/%u focus=%u retained=%u PASS\n",mode,polls,light_calls,battery_reads,policy_opens,policy_closes,focus_changes,retained);
}
#endif
