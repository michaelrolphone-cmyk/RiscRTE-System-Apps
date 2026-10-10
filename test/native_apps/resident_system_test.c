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
static unsigned submitted_at,pending_present;
static unsigned file_requests,file_receives,file_returns;
static unsigned ms,lifecycle_role,focus,frame,token,subs[3],opens[3],closes[3],focus_changes;
static unsigned maps[3],unmaps[3],entries[3],battery_reads,audio_pauses;
static unsigned provider_calls,after_terminal,policy_opens,policy_closes,last_focus,capture_calls;
static unsigned host_result,policy_effects,model_checks,overlay_copies,host_nav,child_nav;
static bool home_requested;
static bool terminal,capture,touch_down,child_started,child_returned;
static void (*busy_hook)(bool);
static uint8_t pixels[48000],completed[48000];
static struct {char key[40];uint8_t bytes[128];uint32_t size;} values[16];
static unsigned value_count;
static unsigned role(void){unsigned current=policy_runtime_role();return current?current:lifecycle_role;}
static bool is(const char *name){return !strcmp(mode,name);}
static void live(void){if(terminal){++after_terminal;assert(!"Provider operation after retention");}++provider_calls;}
static void policy_boundary(void){if(child_started&&!child_returned&&role()==1){assert(!frame&&!capture);++policy_effects;}}
bool policy_fixture_safe(void){return !capture;}
bool policy_fixture_health(risc_runtime_health_v1 *out){live();out->uptime_ms=ms;return true;}
void policy_fixture_delay(uint32_t value){live();ms+=value;}
bool policy_fixture_log(const char *text){if(!terminal)live();(void)text;return true;}
void policy_fixture_busy(bool value){assert(busy_hook);busy_hook(value);}
void policy_fixture_capture(bool value){
 capture=value;
 if(value){last_focus=focus_changes;capture_calls=provider_calls;}
 else assert(focus_changes==last_focus&&provider_calls==capture_calls);
}
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
 return RISC_KEY_VALUE_OK;
}
int32_t policy_fixture_kv_get(uint32_t ns,const char *key,void *out,uint32_t cap,uint32_t *used){assert(ns==1||ns==6);return kv_get(NULL,key,out,cap,used);}
int32_t policy_fixture_kv_put(uint32_t ns,const char *key,const void *bytes,uint32_t size){assert(ns==1||ns==6);return kv_put(NULL,key,bytes,size);}
static const risc_key_value_v1 preferences={1,sizeof(preferences),NULL,kv_get,kv_put};
static bool display_info(void*c,risc_display_info_v1*out){live();(void)c;*out=(risc_display_info_v1){.width=480,.height=800,.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_MONO1),.flags=RISC_DISPLAY_INFO_ASYNC_PRESENT|RISC_DISPLAY_INFO_RETAINS_IMAGE|RISC_DISPLAY_INFO_CLEAN_PRESENT|RISC_DISPLAY_INFO_PARTIAL_DAMAGE|RISC_DISPLAY_INFO_BRIGHTNESS,.damage_x_alignment=8,.damage_width_alignment=8};return true;}
static bool acquire_frame(void*c,uint32_t format,risc_display_surface_v1*out){live();(void)c;assert(!frame&&!pending_present&&format==RISC_DISPLAY_FORMAT_MONO1);frame=role();memset(pixels,0xcd,sizeof(pixels));*out=(risc_display_surface_v1){.frame=1,.pixels=pixels,.width=480,.height=800,.stride_bytes=60,.size_bytes=sizeof(pixels),.pixel_format=format};return true;}
static void release_frame(void*c,risc_display_frame_v1 value){live();(void)c;assert(value==1&&frame==role());frame=0;}
static bool submit_frame(void*c,risc_display_frame_v1 value,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*out){live();(void)c;(void)r;(void)n;(void)o;assert(value==1&&frame==role());frame=0;*out=++token;pending_present=token;submitted_at=ms;return true;}
static bool brightness(void*c,uint16_t value,uint16_t maximum){live();(void)c;assert(!frame&&value<=100&&maximum==100);policy_boundary();return true;}
static bool present_status(void*c,risc_display_present_token_v1 value,risc_display_present_status_v1*out){live();(void)c;assert(value==token&&pending_present==token);if(ms-submitted_at<3){out->state=RISC_DISPLAY_PRESENT_ACTIVE;return true;}pending_present=0;memcpy(completed,pixels,sizeof(pixels));out->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static bool history(void*c,risc_display_frame_v1 value){(void)c;(void)value;return false;}
static int32_t power(void*c,uint32_t budget){live();(void)c;(void)budget;return 0;}
static bool metrics(void*c,risc_display_present_metrics_v1*out){(void)c;(void)out;return false;}
static bool snapshot(void*c,uint32_t format,void*out,size_t size,uint32_t stride){live();(void)c;assert(!frame&&!pending_present&&format==RISC_DISPLAY_FORMAT_MONO1&&size==sizeof(completed)&&stride==60);memcpy(out,completed,size);++overlay_copies;return true;}
static const risc_display_output_api_v1_snapshot display={
 .metrics={.power={.history={.base={.api_version=1,.struct_size=sizeof(display),.get_info=display_info,.acquire=acquire_frame,.release=release_frame,.submit=submit_frame,.present_status=present_status,.set_brightness=brightness},.extension_tag=RISC_DISPLAY_HISTORY_TAG,.extension_version=1,.seed_previous=history},.power_tag=RISC_DISPLAY_POWER_TAG,.power_version=1,.prepare=power,.resume=power},.metrics_tag=RISC_DISPLAY_METRICS_TAG,.metrics_version=1,.snapshot=metrics},.snapshot_tag=RISC_DISPLAY_SNAPSHOT_TAG,.snapshot_version=1,.copy_completed=snapshot};
static uint64_t subscribe(void*c){live();(void)c;unsigned r=role();assert(!subs[r]);subs[r]=1;++opens[r];if(child_started&&r==2)++policy_opens;return r;}
static bool unsubscribe(void*c,uint64_t id){live();(void)c;assert(id==role()&&subs[id]);subs[id]=0;++closes[id];if(child_started&&id==2)++policy_closes;return true;}
static bool poll_touch(void*c,size_t count){live();(void)c;assert(count==1);return true;}
static int32_t next_touch(void*c,uint64_t id,risc_touch_event_v1*out){live();(void)c;(void)out;assert(id==role());return 0;}
static bool touch_snapshot(void*c,risc_touch_snapshot_v1*out){live();(void)c;*out=(risc_touch_snapshot_v1){.width=480,.height=800};if(role()==2&&touch_down){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=230,.y=330};}return true;}
static const risc_touch_api_v1 touch={1,sizeof(touch),NULL,subscribe,unsubscribe,poll_touch,next_touch,touch_snapshot};
static bool nav_poll(void*c,risc_input_navigation_frame_v1*out){
 live();(void)c;assert(focus==role());*out=(risc_input_navigation_frame_v1){0};
 if(child_started&&role()==1){++host_nav;if(host_nav%3==0)out->pressed=RISC_NAV_BACK;}
 if(home_requested&&role()==2){++child_nav;if(child_nav%3==0)out->pressed=RISC_NAV_HOME;}
 assert(ms<10000);return true;
}
static bool nav_focus(void*c,const risc_input_foreground_v1*claims,size_t count){
 live();(void)c;assert(count<=1);
 if(is("terminal")&&child_started&&role()==2&&!count){terminal=true;return false;}
 if(count){assert(claims&&!strcmp(claims[0].capability,"input.touch.raw")&&(!focus||focus==role()));focus=role();}
 else{assert(!focus||focus==role());focus=0;}
 ++focus_changes;return true;
}
static bool nav_reset(void*c){live();(void)c;return true;}
static const risc_input_navigation_api_v1 navigation={1,sizeof(navigation),NULL,nav_poll,nav_focus,nav_reset};
static bool battery_read(void*c,risc_battery_sample_v1*out){live();(void)c;++battery_reads;policy_boundary();*out=(risc_battery_sample_v1){.percent=84};return true;}
static const risc_battery_gauge_api_v1 battery={1,sizeof(battery),NULL,battery_read};
static bool rtc_read(void*c,twatch_rtc_time_v1*out){live();(void)c;*out=(twatch_rtc_time_v1){2026,10,9,5,9,41,0};return true;}
static const twatch_rtc_api_v1 rtc={.api_version=2,.struct_size=sizeof(rtc),.read=rtc_read};
static int32_t alarm_status(void*c,alarm_status_v1*out){live();(void)c;*out=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*out),.state=ALARM_STATE_READY};return ALARM_OK;}
static int32_t alarm_step(void*c){live();(void)c;return ALARM_OK;}
static int32_t alarm_ack(void*c,const alarm_token_v1*t){live();(void)c;(void)t;return ALARM_OK;}
static int32_t alarm_prepare(void*c,alarm_sleep_v1*t){live();(void)c;(void)t;return ALARM_INVALID;}
static const alarm_service_descriptor_v2 alarm={.base={2,sizeof(alarm),NULL,alarm_status,alarm_step,alarm_step,alarm_ack,alarm_prepare,alarm_step},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1,.output_modes=ALARM_MODE_VISUAL};
static wifi_link_t wifi_status(void*c){live();(void)c;return WIFI_LINK_DOWN;}
static bool wifi_stop(void*c){live();(void)c;policy_boundary();if(is("cleanup-terminal")&&role()==2){terminal=true;return false;}return true;}

static bool ble_set(void*c,bool enabled){live();(void)c;assert(!enabled);policy_boundary();return true;}
static bool ble_status(void*c,uint8_t*out){live();(void)c;*out=PORTABLE_BLUETOOTH_OFF;return true;}
static const portable_bluetooth_control_v1 ble={.api_version=1,.struct_size=sizeof(ble),.set_enabled=ble_set,.status=ble_status};
void policy_fixture_provider_event(unsigned index,unsigned event){(void)index;(void)event;live();}
bool portable_audio_services_safe(void){return true;}
bool portable_audio_capture_active(void){return capture;}
void portable_audio_capture_resume(void){}
bool portable_audio_suspend(void){live();assert(!capture);++audio_pauses;return true;}
int portable_app_idle_sleep(const risc_runtime_api_v1*rt,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*g,const alarm_service_v1*a){(void)rt;(void)d;(void)g;(void)a;assert(!"Short resident fixtures must not sleep");return -1;}
int portable_app_sleep(const risc_runtime_api_v1*rt,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*g){(void)rt;(void)d;(void)g;assert(!"Short resident fixtures must not sleep");return -1;}
int portable_app_alarm_sleep(const risc_runtime_api_v1*rt,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*g,const alarm_service_v1*a){(void)rt;(void)d;(void)g;(void)a;assert(!"Short resident fixtures must not sleep");return -1;}
#include "RiscStorageVolumeV1.h"
#include "SoftwareUpdateV1.h"
#include "RiscUsbDeviceMscV1.h"
static unsigned directory,dir_index,usb_owned,usb_begins,usb_ends;
static bool yes(void*c){(void)c;live();return true;}
static bool label(void*c,char*out,size_t size){(void)c;live();snprintf(out,size,"Fixture volume");return true;}
static bool stat_file(void*c,const char*p,uint64_t*size,bool*dir){(void)c;(void)p;live();*size=100;*dir=false;return true;}
static risc_storage_dir_t dir_open(void*c,const char*p){(void)c;live();assert(!directory&&!strcmp(p,"/"));directory=1;dir_index=0;return 1;}
static bool dir_next(void*c,risc_storage_dir_t h,risc_storage_dirent_v1*out){(void)c;live();assert(h==1&&directory);if(dir_index==24)return false;*out=(risc_storage_dirent_v1){.size=100};snprintf(out->name,sizeof(out->name),"File %03u.txt",dir_index++);return true;}
static void dir_close(void*c,risc_storage_dir_t h){(void)c;live();assert(h==1&&directory);directory=0;}
static risc_storage_file_t file_open(void*c,const char*p,uint64_t*size){(void)c;(void)p;live();*size=0;return 0;}
static size_t file_read(void*c,risc_storage_file_t h,void*out,size_t size){(void)c;(void)h;(void)out;(void)size;live();return 0;}
static bool file_close(void*c,risc_storage_file_t h,bool commit){(void)c;(void)h;(void)commit;live();if(is("cleanup-terminal")){terminal=true;return false;}return true;}
static bool error_text(void*c,char*out,size_t size){(void)c;live();assert(size);*out=0;return true;}
static const risc_storage_volume_api_v1 volume={1,sizeof(volume),NULL,yes,yes,label,stat_file,dir_open,dir_next,dir_close,file_open,file_read,NULL,NULL,file_close,NULL,error_text};
static bool wifi_connect(void*c,const char*s,const char*p){(void)c;(void)s;(void)p;assert(!"No network connection is authorized by fixture");return false;}
static int8_t wifi_rssi(void*c){(void)c;live();return -30;}
static bool wifi_addresses(void*c,wifi_ipv4_v1*s,wifi_ipv4_v1*a){(void)c;live();*s=(wifi_ipv4_v1){0};*a=(wifi_ipv4_v1){0};return true;}
static bool wifi_scan(void*c,garden_radio_scan_result_v1*out){(void)c;live();*out=(garden_radio_scan_result_v1){0};return true;}
static const wifi_api_v1 wifi={.api_version=1,.struct_size=sizeof(wifi),.connect=wifi_connect,.status=wifi_status,.rssi=wifi_rssi,.addresses=wifi_addresses,.scan_start=yes,.scan_poll=wifi_scan,.scan_cancel=yes,.disconnect_checked=wifi_stop};
static bool update_refresh(void*c,uint64_t now){(void)c;(void)now;live();return true;}
static bool update_status(void*c,software_update_status_v1*out){(void)c;live();*out=(software_update_status_v1){.struct_size=sizeof(*out),.state=SOFTWARE_UPDATE_LIST,.count=24};return true;}
static bool update_get(void*c,uint32_t i,software_update_row_v1*out){(void)c;live();assert(i<24);*out=(software_update_row_v1){.struct_size=sizeof(*out),.availability=SOFTWARE_UPDATE_AVAILABLE};snprintf(out->id,sizeof(out->id),"fixture-%02u",i);strcpy(out->version,"1.2.3");return true;}
static bool update_begin(void*c,uint32_t i,uint64_t now){(void)c;(void)i;(void)now;assert(!"Fixture must not install");return false;}
static bool update_cancel(void*c){(void)c;live();if(is("cleanup-terminal")){terminal=true;return false;}return true;}
static const software_update_v1 updates={1,sizeof(updates),NULL,update_refresh,yes,update_status,update_get,update_begin,update_cancel,yes,yes};
static int32_t usb_begin(void*c,uint64_t*out){(void)c;live();assert(!usb_owned);usb_owned=1;++usb_begins;*out=42;return RISC_USB_MSC_OK;}
static int32_t usb_poll(void*c,uint64_t key,risc_usb_device_msc_status_v1*out){(void)c;live();assert(usb_owned&&key==42);*out=(risc_usb_device_msc_status_v1){.struct_size=sizeof(*out),.state=RISC_USB_MSC_WAITING};return RISC_USB_MSC_OK;}
static int32_t usb_end(void*c,uint64_t key,uint32_t reason){(void)c;(void)reason;live();assert(usb_owned&&key==42);usb_owned=0;++usb_ends;return RISC_USB_MSC_OK;}
static int32_t usb_prepare(void*c,uint64_t key){(void)c;(void)key;assert(!"No media preparation needed for stopped-owner gate");return RISC_USB_MSC_OK;}
static const risc_usb_device_msc_api_v1_prepare usb={.base={1,sizeof(usb),NULL,usb_begin,usb_poll,usb_end,error_text},.prepare_tag=RISC_USB_MSC_PREPARE_TAG,.prepare_version=1,.prepare_step=usb_prepare};
const void *policy_fixture_api(unsigned index){const void *tables[]={NULL,&display,&touch,&navigation,&battery,&rtc,&alarm,&wifi,&ble,&volume,&updates,&updates,&usb};assert(index<13);return tables[index];}
const char *system_mode(void){return mode;}
void system_busy(bool value){policy_fixture_busy(value);}
void system_capture(bool value){policy_fixture_capture(value);}
void system_home(void){home_requested=true;}
unsigned system_client_visit(void){return entries[2];}
void system_set_busy_hook(void (*hook)(bool)){busy_hook=hook;}
void system_event(unsigned event,unsigned value){
 if(event==10){lifecycle_role=value;++maps[value];return;}
 if(event==11){++unmaps[value];if(value==2)lifecycle_role=1;return;}
 if(event==12){lifecycle_role=value;++entries[value];if(value==2)child_started=true;return;}
 if(event==1){host_result=value;return;}
 if(event==3){assert(value==1);child_returned=true;return;}
 if(event==4){assert(value==1);++model_checks;return;}
 if(event==13){assert(unmaps[2]==1&&entries[2]==1);++file_receives;return;}
 if(event==14){assert(file_requests==1&&file_receives==1);++file_returns;return;}
 if(event==15){assert(!file_requests);++file_requests;return;}
 assert(!"Unknown fixture event");
}
void policy_fixture_setup(const char*name){mode=name;lifecycle_role=1;assert(portable_sleep_timer_save(&preferences,false,5000));}
void policy_fixture_verify(bool was_retained){
 assert(was_retained==(is("terminal")||is("cleanup-terminal")));
 if(was_retained){assert(terminal&&!after_terminal&&!unmaps[1]&&!unmaps[2]&&host_result==0);}
 else{
  assert(!terminal&&!frame&&!pending_present&&!directory&&!usb_owned&&!subs[1]&&!subs[2]&&!focus);
  assert(maps[1]==1&&maps[2]==(is("file-open")?2u:1u)&&unmaps[1]==1&&unmaps[2]==maps[2]&&entries[1]==1&&entries[2]==maps[2]&&host_result==2&&child_returned);
  assert(model_checks&&child_nav>=3&&opens[1]==closes[1]&&opens[2]==closes[2]);
  if(is("poll")||is("capture")||is("policy-busy"))assert(!overlay_copies);
  else assert(overlay_copies==(is("file-open")?2u:1u)&&host_nav>=3);
  if(is("file-open"))assert(file_requests==1&&file_receives==1&&file_returns==1);
  assert(usb_begins==usb_ends);
 }
 printf("Actual System controller + adapter + Runtime/Graph ELFs: %s models=%u overlays=%u home-polls=%u usb=%u/%u retained=%u PASS\n",mode,model_checks,overlay_copies,child_nav,usb_begins,usb_ends,was_retained);
}
#endif
