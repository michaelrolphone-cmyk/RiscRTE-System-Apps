/* Production host + production idle helper, deterministic native providers.
 * The separate product matrix uses the real tagged alarm service as well. */
#include <AlarmServiceV2.h>
/* Legacy shell fixture aggregate is used only by its unused baseline main. */
typedef struct { alarm_service_v1 service; uint32_t output_modes; } alarm_service_outputs_v1;
#define main original_resident_shell_main
#define risc_runtime_get_api original_get_api
#include "resident_shell_test.c"
#undef risc_runtime_get_api
#undef main
#include "PortableAppSleep.h"
#include "PortableBluetoothControl.h"
#include "WifiApi.h"
#include "X4PowerV1.h"
#include <RiscTouchPowerV1.h>
#include <RiscStorageVolumeV1.h>
#include <AlarmServiceV2.h>
static const char *idle_case;
static bool touch_prepared,panel_prepared,storage_prepared,idle_active,overlay_visible;
static unsigned status_calls,overlay_frames,restored_frames,sleep_calls,alarm_prepares,alarm_resumes,post_prepare_display;
static int result_seen;
static void (*known_fail)(void),(*known_retain)(void);
static bool (*known_flipped)(void);
void idle_fixture_bind(void (*fail)(void),void (*retained)(void),bool (*flipped)(void)){known_fail=fail;known_retain=retained;known_flipped=flipped;}
static risc_display_output_api_v1_snapshot idle_display;
#ifdef PORTABLE_FRONTLIGHT_TONE
#include "RiscDisplayOutputFrontlightV1.h"
static risc_display_output_api_v1_frontlight idle_tone_display;
static unsigned idle_tone=50,idle_level,idle_tone_sets,idle_tone_gets;
static bool tone_case(void){return !strncmp(idle_case,"tone-",5);}
#endif
static risc_touch_power_api_v1 idle_touch;
static bool is(const char *s){return !strcmp(idle_case,s);}
static void idle_safe(void);
static int32_t idle_preferences(void*c,const char*key,void*p,uint32_t n,uint32_t*used){
#ifdef PORTABLE_FRONTLIGHT_TONE
 if(tone_case()&&(!strcmp(key,"brightness")||!strcmp(key,"frontlight_tone"))){
  idle_safe();assert(n>=1);*(uint8_t*)p=!strcmp(key,"brightness")?0:80;*used=1;return RISC_KEY_VALUE_OK;
 }
#endif
 if((is("flip-on")||is("flip-off"))&&!strcmp(key,"reader_flip_ui")) {
  idle_safe();assert(n>=4);
  unsigned flipped=is("flip-on")?idle_active:!idle_active;
  const uint8_t value[]={0x52,1,(uint8_t)flipped,(uint8_t)(flipped^0xa5u)};
  memcpy(p,value,sizeof(value));*used=sizeof(value);return RISC_KEY_VALUE_OK;
 }
 return kv_get(c,key,p,n,used);
}
void idle_result(int result){result_seen=result;}
static void idle_safe(void){live();assert(!touch_prepared&&!panel_prepared&&!storage_prepared);}
static bool idle_health(risc_runtime_health_v1 *out){live();out->uptime_ms=ms;return true;}
static bool idle_key(void*c,bool*down){(void)c;live();*down=is("cancel-prepared")&&panel_prepared;return true;}
static int32_t idle_light(void*c,uint32_t n,risc_light_sleep_result_v1*out){
 (void)c;(void)out;live();assert(n&&touch_prepared&&panel_prepared&&storage_prepared&&overlay_visible);++sleep_calls;
 if(is("native-retained")){terminal=true;return RISC_LIGHT_SLEEP_RETAINED;}
 if(is("native-refused"))return RISC_LIGHT_SLEEP_BUSY;
#ifdef PORTABLE_FRONTLIGHT_TONE
 if(tone_case()){assert(idle_tone==80&&idle_level==0);idle_tone=50;}
#endif
 ms+=10000;return RISC_LIGHT_SLEEP_OK;
}
static const x4_power_v1 idle_power={1,sizeof(idle_power),NULL,idle_key,idle_light};
static int32_t idle_panel_prepare(void*c,uint32_t n){
 (void)c;live();assert(n&&touch_prepared&&overlay_visible&&!frame);panel_prepared=true;
 if(is("panel-retained")){terminal=true;return RISC_DISPLAY_POWER_RETAINED;}
 return is("panel-refused")?RISC_DISPLAY_POWER_BUSY:RISC_DISPLAY_POWER_OK;
}
static int32_t idle_panel_resume(void*c,uint32_t n){(void)c;live();assert(n&&panel_prepared&&!storage_prepared);panel_prepared=false;return RISC_DISPLAY_POWER_OK;}
static int32_t idle_touch_prepare(void*c,uint32_t n){(void)c;idle_safe();assert(n);touch_prepared=true;return RISC_TOUCH_POWER_OK;}
static int32_t idle_touch_resume(void*c,uint32_t n){(void)c;live();assert(n&&touch_prepared&&!panel_prepared&&!storage_prepared);touch_prepared=false;return RISC_TOUCH_POWER_OK;}
static bool idle_sd_prepare(void*c){(void)c;live();assert(panel_prepared&&touch_prepared);storage_prepared=true;return true;}
static bool idle_sd_commit(void*c){(void)c;live();assert(storage_prepared);return true;}
static int32_t idle_sd_resume(void*c){(void)c;live();assert(storage_prepared);storage_prepared=false;return RISC_STORAGE_SLEEP_READY;}
static risc_storage_volume_api_v1_sleep idle_storage;
static wifi_link_t idle_wifi_status(void*c){(void)c;idle_safe();return WIFI_LINK_DOWN;}
static bool idle_wifi_off(void*c){(void)c;idle_safe();if(idle_active&&is("wifi-retained")){terminal=true;return false;}return true;}
static const wifi_api_v1 idle_wifi={.api_version=1,.struct_size=sizeof(idle_wifi),.status=idle_wifi_status,.disconnect_checked=idle_wifi_off};
static bool idle_ble_set(void*c,bool on){(void)c;idle_safe();assert(!on);return true;}
static bool idle_ble_status(void*c,uint8_t*out){(void)c;idle_safe();*out=PORTABLE_BLUETOOTH_OFF;return true;}
static const portable_bluetooth_control_v1 idle_ble={.api_version=1,.struct_size=sizeof(idle_ble),.set_enabled=idle_ble_set,.status=idle_ble_status};
static int32_t idle_alarm_prepare(void*c,alarm_sleep_v1*out){
 (void)c;idle_safe();++alarm_prepares;
 if(is("alarm-retained")){terminal=true;return ALARM_RETAINED;}
 if(is("alarm-cancel"))return ALARM_FOREGROUND;
 *out=(alarm_sleep_v1){.struct_size=sizeof(*out),.rtc_seconds=1700000000};return ALARM_OK;
}
static int32_t idle_alarm_resume(void*c,const alarm_sleep_v1*in){(void)c;idle_safe();assert(in->rtc_seconds==1700000000);++alarm_resumes;return ALARM_OK;}
static const alarm_service_descriptor_v2 idle_alarm={
 .base={2,sizeof(idle_alarm),NULL,alarm_status,alarm_step,alarm_step,alarm_ack,idle_alarm_prepare,alarm_step},
 .tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1,.output_modes=ALARM_MODE_VISUAL,
 .features=ALARM_DESCRIPTOR_RESUME_SLEEP,.resume_sleep=idle_alarm_resume};
static bool idle_frame(void*c,uint32_t format,risc_display_surface_v1*out){
 idle_safe();if(idle_active&&(is("frame-retained")||(is("restore-frame-retained")&&overlay_frames))){terminal=true;return false;}return acquire_frame(c,format,out);
}
static bool idle_submit(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){
 idle_safe();if(idle_active){
  assert(!focus&&!subs&&o->intent==RISC_DISPLAY_PRESENT_LOW_LATENCY);
  if(is("submit-retained")||(is("restore-submit-retained")&&overlay_visible)){terminal=true;return false;}
  if(!overlay_visible){
   ++overlay_frames;overlay_visible=true;
   /* Foreground pixels outside the centered 240 x 96 card are preserved. */
   for(unsigned y=0;y<800;++y)for(unsigned x=0;x<60;++x)
    if(y<352||y>=448||x<15||x>=45)assert(pixels[y*60+x]==child_image[y*60+x]);
   assert(memcmp(pixels,child_image,sizeof(pixels)));
   char path[1024];snprintf(path,sizeof(path),"%s/sleeping.pbm",fixture_dir);
   FILE *file=fopen(path,"wb");assert(file);fprintf(file,"P4\n480 800\n");assert(fwrite(pixels,1,sizeof(pixels),file)==sizeof(pixels));fclose(file);
  }else{assert(!memcmp(pixels,child_image,sizeof(pixels)));++restored_frames;overlay_visible=false;}
 }
 return submit_frame(c,f,r,n,o,t);
}
static bool idle_present_status(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*out){
 idle_safe();if(idle_active&&is("slow")&&++status_calls%4){out->state=RISC_DISPLAY_PRESENT_ACTIVE;return true;}if(idle_active&&(is("present-retained")||(is("restore-present-retained")&&restored_frames))){terminal=true;return false;}return status_frame(c,t,out);
}
static bool idle_snapshot(void*c,uint32_t f,void*p,size_t n,uint32_t stride){
 idle_safe();if(is("snapshot-refused"))return false;return snapshot_frame(c,f,p,n,stride);
}
static bool idle_brightness(void*c,uint16_t value,uint16_t scale){idle_safe();
#ifdef PORTABLE_FRONTLIGHT_TONE
 if(tone_case()){assert(value==0);idle_level=value;}
#endif
 return brightness(c,value,scale);}
#ifdef PORTABLE_FRONTLIGHT_TONE
static bool idle_set_tone(void*c,uint16_t w,uint16_t m){
 (void)c;idle_safe();assert(tone_case()&&w==80&&m==100&&idle_level==0);++idle_tone_sets;
 if(sleep_calls&&is("tone-set-retained")){terminal=true;return false;}
 idle_tone=w;return true;
}
static int32_t idle_get_tone(void*c,uint16_t*w,uint16_t*m){
 (void)c;idle_safe();assert(tone_case()&&idle_level==0);++idle_tone_gets;
 if(sleep_calls&&is("tone-get-retained")){terminal=true;return RISC_DISPLAY_TONE_FAILED;}
 *w=(uint16_t)idle_tone;*m=100;return RISC_DISPLAY_TONE_OK;
}
#endif
static bool idle_obtain(const char*name,uint32_t version,uint64_t id,risc_runtime_capability_v1*out){
 idle_safe();const void *api=NULL;
 if(idle_active&&is("acquire-retained")&&!strcmp(name,X4_POWER_CAPABILITY)){terminal=true;return false;}
 if(!strcmp(name,X4_POWER_CAPABILITY))api=&idle_power;
 else if(!strcmp(name,"storage.volume"))api=&idle_storage;
 else if(!strcmp(name,"net.wifi"))api=&idle_wifi;
 else if(!strcmp(name,"bluetooth.hci"))api=&idle_ble;
 if(api){out->api=api;out->slot=++grants;return true;}
 if(!acquire(name,version,id,out))return false;
 if(!strcmp(name,"storage.key-value")){
  static risc_key_value_v1 preferences;preferences=kv;preferences.get=idle_preferences;out->api=&preferences;
 }
 if(!strcmp(name,"display.output")){
  out->api=&idle_display;
#ifdef PORTABLE_FRONTLIGHT_TONE
  if(tone_case())out->api=&idle_tone_display;
#endif
 }
 if(!strcmp(name,"input.touch.raw"))out->api=&idle_touch;
 if(!strcmp(name,ALARM_SERVICE_CAPABILITY))out->api=&idle_alarm;
 return true;
}
static int32_t idle_run(uint64_t id,const char*path,risc_resident_result_v1*out){
 live();assert(id==1&&!strcmp(path,"child.elf")&&!focus&&!subs&&!frame);
 if(is("flip-on")||is("flip-off"))assert(known_flipped&&known_flipped()==is("flip-off"));
 memset(child_image,0x55,sizeof(child_image));memcpy(completed,child_image,sizeof(completed));
 ms+=60001;idle_active=true;in_dispatch=true;
 risc_resident_request_v1 request={.struct_size=sizeof(request),.reason=RISC_RESIDENT_CHECKPOINT_POLL};
 risc_resident_reply_v1 reply={.struct_size=sizeof(reply)};
 if(is("known-failed")||is("known-retained")){
  if(is("known-failed"))known_fail();else known_retain();
  int status=callbacks.dispatch(NULL,&request,&reply);
  assert(status==(is("known-failed")?RISC_RESIDENT_BUSY:RISC_RESIDENT_RETAINED));
  in_dispatch=idle_active=false;out->status=status;return status;
 }
 int status=RISC_RESIDENT_OK;
 for(unsigned cycle=0;cycle<((is("repeat")||is("tone-repeat"))?2u:1u);++cycle) {
  if(cycle)ms+=60001;
  request.reason=RISC_RESIDENT_CHECKPOINT_POLL;
  assert(callbacks.dispatch(NULL,&request,&reply)==RISC_RESIDENT_OK);
  assert(reply.flags&RISC_RESIDENT_REPLY_POLICY_REQUEST);
  request.reason=RISC_RESIDENT_CHECKPOINT_POLICY;
  status=callbacks.dispatch(NULL,&request,&reply);
  if(is("flip-on")||is("flip-off"))assert(known_flipped&&known_flipped()==is("flip-on"));
#ifdef PORTABLE_FRONTLIGHT_TONE
  if(tone_case()&&status==RISC_RESIDENT_OK)assert(idle_tone==80&&idle_level==0);
#endif
  if(status==RISC_RESIDENT_OK){assert(!overlay_visible&&!memcmp(completed,child_image,sizeof(completed)));assert(reply.flags&RISC_RESIDENT_REPLY_REDRAW);}
  else break;
 }
 in_dispatch=idle_active=false;out->status=status;return status;
}
static bool idle_resident(risc_resident_client_v1*out){if(!resident_get(out))return false;out->run_foreground=idle_run;return true;}
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t version){
 static risc_runtime_api_v1 api;api=runtime;api.acquire=idle_obtain;api.health=idle_health;api.resident_shell=idle_resident;return version==1?&api:NULL;
}
int portable_app_alarm_sleep(const risc_runtime_api_v1*r,const risc_display_output_api_v1*d,const risc_battery_gauge_api_v1*g,const alarm_service_v1*a){(void)r;(void)d;(void)g;(void)a;assert(false);return -2;}
int main(int argc,char**argv){
 assert(argc==3);fixture_dir=argv[1];idle_case=argv[2];mode=100;
 idle_display=display;idle_display.copy_completed=idle_snapshot;
 risc_display_output_api_v1 *base=&idle_display.metrics.power.history.base;
 base->acquire=idle_frame;base->submit=idle_submit;base->present_status=idle_present_status;base->set_brightness=idle_brightness;
 idle_display.metrics.power.prepare=idle_panel_prepare;idle_display.metrics.power.resume=idle_panel_resume;
#ifdef PORTABLE_FRONTLIGHT_TONE
 idle_tone_display.snapshot=idle_display;idle_tone_display.snapshot.metrics.power.history.base.struct_size=sizeof(idle_tone_display);
 idle_tone_display.frontlight_tag=RISC_DISPLAY_FRONTLIGHT_TAG;idle_tone_display.frontlight_version=1;idle_tone_display.set_tone=idle_set_tone;idle_tone_display.get_tone=idle_get_tone;
#endif
 idle_touch.base=touch;idle_touch.base.struct_size=sizeof(idle_touch);idle_touch.power_tag=RISC_TOUCH_POWER_TAG;idle_touch.power_version=1;idle_touch.prepare=idle_touch_prepare;idle_touch.resume=idle_touch_resume;
 idle_storage.terminal.power.volume.base=(risc_storage_volume_api_v1){.api_version=1,.struct_size=sizeof(idle_storage)};
 idle_storage.terminal.extension_tag=RISC_STORAGE_POWER_COMMIT_TAG;idle_storage.terminal.extension_version=1;idle_storage.terminal.commit_power_down=idle_sd_commit;
 idle_storage.sleep_tag=RISC_STORAGE_SLEEP_TAG;idle_storage.sleep_version=1;idle_storage.prepare_sleep=idle_sd_prepare;idle_storage.commit_sleep=idle_sd_commit;idle_storage.resume_sleep=idle_sd_resume;
 load_app("host.elf",1);
 assert(!after_terminal&&!post_prepare_display);
 if(terminal){assert(result_seen==-1&&!finis);if(is("panel-retained")||is("native-retained"))assert(overlay_visible&&overlay_frames==1&&!restored_frames);}
 else{assert(result_seen==(is("known-failed")?-1:1)&&!grants&&!subs&&!frame&&!focus&&!touch_prepared&&!panel_prepared&&!storage_prepared);assert(overlay_frames==restored_frames);}
 if(is("known-failed")||is("known-retained")||is("acquire-retained")||is("wifi-retained")||is("alarm-retained")||is("alarm-cancel")||is("snapshot-refused"))assert(!overlay_frames&&!sleep_calls);
 if(is("ok"))assert(sleep_calls==1&&alarm_resumes==1&&restored_frames==1);
 if(is("repeat")||is("tone-repeat"))assert(sleep_calls==2&&alarm_resumes==2&&overlay_frames==2&&restored_frames==2);
#ifdef PORTABLE_FRONTLIGHT_TONE
 if(tone_case()){
  unsigned cycles=is("tone-repeat")?2u:1u;assert(sleep_calls==cycles&&idle_level==0);
  assert(idle_tone_gets==1+cycles&&idle_tone_sets==1+cycles-(unsigned)is("tone-get-retained"));
  assert(terminal==(is("tone-get-retained")||is("tone-set-retained")));
  assert(idle_tone==(terminal?50u:80u));
 }
#endif
 printf("Actual host + idle helper %s: overlays=%u restored=%u sleep=%u retained=%u PASS\n",idle_case,overlay_frames,restored_frames,sleep_calls,terminal);return 0;
}
