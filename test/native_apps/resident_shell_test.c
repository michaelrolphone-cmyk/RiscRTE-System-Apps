#include "PortableApps.h"
#include "RiscRuntimeV1.h"
#include "RiscKeyValueV1.h"
#include "RiscResidentShellV1.h"
#include "RiscDisplayOutputSnapshotV1.h"
#include "RiscTouchV1.h"
#include "RiscInputNavigationV1.h"
#include "RiscBatteryGaugeV1.h"
#include "PortableRtcClock.h"
#include "AlarmServiceV1.h"
#include <assert.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static unsigned role=1,phase,ms,focus=1,frame,token,grants,subs,host_frames,child_frames;
static unsigned entries,finis,checkpoints,overlays,requests,exit_requests,client_polls,snapshots;
static unsigned mode,iteration,host_result,after_terminal,sleep_seen,focus_changes,guard_calls;
static bool terminal,in_dispatch,child_exit,host_registered,host_polling;
static unsigned host_steps;
#ifdef TEST_RESIDENT_LEGACY_HANDOFF
static bool legacy_unwind,finalizing;
static unsigned continuation_queries;
#endif
static char queued[64];
static uint8_t pixels[48000],completed[48000],child_image[48000];
static risc_resident_callbacks_v1 callbacks;
static const char *fixture_dir;
static void live(void){
#ifdef TEST_RESIDENT_LEGACY_HANDOFF
 assert(!legacy_unwind || finalizing);
#endif
 if(terminal){after_terminal++;assert(!"Provider called after terminal retention");}}
static bool health(risc_runtime_health_v1 *out){live();assert(ms<20000);out->uptime_ms=ms;return true;}
static void wait_ms(uint32_t n){live();ms+=n;}
static bool diagnostic(const char *s){live();(void)s;return true;}
static bool retain(void){terminal=true;return true;}
static bool request_launch(const char *path){live();assert(role==2 && !in_dispatch);requests++;snprintf(queued,sizeof(queued),"%s",path);return true;}
static bool display_info(void*c,risc_display_info_v1*out){live();(void)c;*out=(risc_display_info_v1){.width=480,.height=800,.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_MONO1),.flags=RISC_DISPLAY_INFO_RETAINS_IMAGE|RISC_DISPLAY_INFO_CLEAN_PRESENT|RISC_DISPLAY_INFO_PARTIAL_DAMAGE|RISC_DISPLAY_INFO_BRIGHTNESS,.damage_x_alignment=8,.damage_width_alignment=8};return true;}
static bool acquire_frame(void*c,uint32_t f,risc_display_surface_v1*out){live();(void)c;assert(!frame && f==RISC_DISPLAY_FORMAT_MONO1);frame=role;memset(pixels,0xcd,sizeof(pixels));*out=(risc_display_surface_v1){.frame=1,.pixels=pixels,.width=480,.height=800,.stride_bytes=60,.size_bytes=sizeof(pixels),.pixel_format=f};return true;}
static void release_frame(void*c,risc_display_frame_v1 f){live();(void)c;assert(f==1&&frame==role);frame=0;}
static bool submit_frame(void*c,risc_display_frame_v1 f,const risc_display_rect_v1*r,size_t n,const risc_display_present_options_v1*o,risc_display_present_token_v1*t){live();(void)c;(void)r;(void)n;(void)o;assert(f==1&&frame==role);frame=0;*t=++token;memcpy(completed,pixels,sizeof(pixels));if(role==1){host_frames++;if(in_dispatch && phase==0 && ms-iteration<=640) {
 char path[1024];snprintf(path,sizeof(path),"%s/quick-%u.pbm",fixture_dir,mode);FILE *file=fopen(path,"wb");assert(file);fprintf(file,"P4\n480 800\n");assert(fwrite(pixels,1,sizeof(pixels),file)==sizeof(pixels));fclose(file);
 }}else{child_frames++;memcpy(child_image,pixels,sizeof(pixels));}return true;}
static bool brightness(void*c,uint16_t value,uint16_t maximum){live();(void)c;assert(!frame&&value<=100&&maximum==100);return true;}
static bool status_frame(void*c,risc_display_present_token_v1 t,risc_display_present_status_v1*s){live();(void)c;assert(t==token);s->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;}
static bool history(void*c,risc_display_frame_v1 f){(void)c;(void)f;return false;}
static int32_t power(void*c,uint32_t budget){(void)c;(void)budget;return 0;}
static bool metrics(void*c,risc_display_present_metrics_v1*out){(void)c;(void)out;return false;}
static bool snapshot_frame(void*c,uint32_t f,void*p,size_t n,uint32_t stride){live();(void)c;assert(role==1&&in_dispatch&&!frame&&f==RISC_DISPLAY_FORMAT_MONO1&&n==sizeof(pixels)&&stride==60);snapshots++;if(mode==2)return false;memcpy(p,completed,n);return true;}
static const risc_display_output_api_v1_snapshot display={
 .metrics={.power={.history={.base={.api_version=1,.struct_size=sizeof(display),.get_info=display_info,.acquire=acquire_frame,.release=release_frame,.submit=submit_frame,.present_status=status_frame,.set_brightness=brightness},.extension_tag=RISC_DISPLAY_HISTORY_TAG,.extension_version=1,.seed_previous=history},.power_tag=RISC_DISPLAY_POWER_TAG,.power_version=1,.prepare=power,.resume=power},.metrics_tag=RISC_DISPLAY_METRICS_TAG,.metrics_version=1,.snapshot=metrics},.snapshot_tag=RISC_DISPLAY_SNAPSHOT_TAG,.snapshot_version=1,.copy_completed=snapshot_frame};
static uint64_t subscribe(void*c){live();(void)c;subs++;return role;}
static bool unsubscribe(void*c,uint64_t id){live();(void)c;assert(id==role && subs);subs--;return true;}
static bool poll_touch(void*c,size_t count){live();(void)c;assert(count==1);if(role==1 && host_polling)host_steps++;return true;}
static int32_t next_touch(void*c,uint64_t id,risc_touch_event_v1*out){live();(void)c;(void)out;assert(id==role);return 0;}
static bool snapshot_touch(void*c,risc_touch_snapshot_v1*out){live();(void)c;*out=(risc_touch_snapshot_v1){.width=480,.height=800};
 unsigned since=ms-iteration;
 if(role==2) {
  if(phase==0 && since>=40 && since<80){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=3,.x=240,.y=20};}
  if(phase==0 && since>=80){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=3,.x=240,.y=160};}
  if(phase==2 && since>=40 && since<80){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=4,.x=240,.y=20};}
  if(phase==2 && since>=80){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=4,.x=240,.y=180};}
  if(phase==4)out->buttons=since>60?RISC_TOUCH_BUTTON_PRIMARY:0;
 } else if(mode==12 && host_polling){out->buttons=(host_steps==2 || host_steps==3)?RISC_TOUCH_BUTTON_PRIMARY:0;
 } else if(in_dispatch) {
  if(since<140){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=phase==0?3:4,.x=240,.y=since<80?160:460};}
  if(since>=650 && since<690){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=5,.x=240,.y=mode==1?550:728};}
 }
 return true;
}
static const risc_touch_api_v1 touch={1,sizeof(touch),NULL,subscribe,unsubscribe,poll_touch,next_touch,snapshot_touch};
static bool nav_poll(void*c,risc_input_navigation_frame_v1*out){live();(void)c;assert(focus==role);*out=(risc_input_navigation_frame_v1){0};if((mode==6 && role==2 && ms-iteration>=40)||(mode==20 && role==1 && in_dispatch && ms-iteration>=650))out->pressed=RISC_NAV_HOME;return true;}
static bool nav_focus(void*c,const risc_input_foreground_v1*claims,size_t count){live();(void)c;assert(count<=1);if(count){if(mode==5 && in_dispatch)return false;assert(claims&&!strcmp(claims[0].capability,"input.touch.raw"));assert(!focus||focus==role);focus=role;}else {assert(!focus||focus==role);focus=0;}focus_changes++;return true;}
static bool nav_reset(void*c){live();(void)c;return true;}
static const risc_input_navigation_api_v1 nav={1,sizeof(nav),NULL,nav_poll,nav_focus,nav_reset};
static bool battery_read(void*c,risc_battery_sample_v1*out){live();(void)c;*out=(risc_battery_sample_v1){.percent=84};return true;}
static const risc_battery_gauge_api_v1 battery={1,sizeof(battery),NULL,battery_read};
static bool rtc_read(void*c,twatch_rtc_time_v1*out){live();(void)c;*out=(twatch_rtc_time_v1){2026,10,9,5,9,41,0};return true;}
static const twatch_rtc_api_v1 rtc={.api_version=2,.struct_size=sizeof(rtc),.read=rtc_read};
static int32_t kv_get(void*c,const char*key,void*p,uint32_t n,uint32_t*used){live();(void)c;(void)key;(void)p;(void)n;assert(!frame);*used=0;return RISC_KEY_VALUE_NOT_FOUND;}
static int32_t kv_put(void*c,const char*key,const void*p,uint32_t n){live();(void)c;(void)key;(void)p;(void)n;assert(!frame);return RISC_KEY_VALUE_OK;}
static const risc_key_value_v1 kv={1,sizeof(kv),NULL,kv_get,kv_put};
static int32_t alarm_status(void*c,alarm_status_v1*out){live();(void)c;*out=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*out),.state=ALARM_STATE_READY};return ALARM_OK;}
static int32_t alarm_step(void*c){live();(void)c;return ALARM_OK;}
static int32_t alarm_ack(void*c,const alarm_token_v1*t){(void)c;(void)t;return ALARM_OK;}
static int32_t alarm_prepare(void*c,alarm_sleep_v1*t){(void)c;(void)t;return ALARM_INVALID;}
static alarm_service_outputs_v1 alarm={.service={1,sizeof(alarm),NULL,alarm_status,alarm_step,alarm_step,alarm_ack,alarm_prepare,alarm_step},.output_modes=ALARM_MODE_VISUAL};
static bool acquire(const char*name,uint32_t version,uint64_t id,risc_runtime_capability_v1*out){live();(void)version;(void)id;assert(!frame);const void *api=NULL;
 if(!strcmp(name,"display.output"))api=&display;else if(!strcmp(name,"input.touch.raw"))api=&touch;else if(!strcmp(name,"input.navigation"))api=&nav;
 else if(!strcmp(name,"board.battery"))api=&battery;else if(!strcmp(name,"storage.key-value"))api=&kv;else if(!strcmp(name,"rtc.clock"))api=&rtc;else if(!strcmp(name,ALARM_SERVICE_CAPABILITY))api=&alarm;
 if(!api)return false;
 out->api=api;out->slot=++grants;return true;}
static bool release(risc_runtime_capability_v1*out){live();assert(out->api&&grants);grants--;*out=(risc_runtime_capability_v1){.struct_size=sizeof(*out)};return true;}
static int32_t register_shell(uint64_t invocation,const risc_resident_callbacks_v1 *value){live();assert(role==1&&invocation==1&&!host_registered);callbacks=*value;host_registered=true;return RISC_RESIDENT_OK;}
static int32_t request_exit(uint64_t invocation){live();assert(role==1&&invocation==1&&in_dispatch);child_exit=true;exit_requests++;return RISC_RESIDENT_OK;}
static int32_t checkpoint(uint64_t invocation,const risc_resident_request_v1*request,risc_resident_reply_v1*reply){live();assert(role==2&&invocation==2&&!frame&&!in_dispatch);checkpoints++;
 if(request->reason==RISC_RESIDENT_CHECKPOINT_CONTROLS) {
  assert(!focus);if(mode==3){terminal=true;return RISC_RESIDENT_RETAINED;}overlays++;
 }
 role=1;in_dispatch=true;int result=callbacks.dispatch(callbacks.context,request,reply);in_dispatch=false;role=2;
 if(request->reason==RISC_RESIDENT_CHECKPOINT_CONTROLS && result==RISC_RESIDENT_OK) {
  assert(!focus&&!memcmp(completed,child_image,sizeof(completed)));phase+=2;iteration=ms;
 }
 if(child_exit)return RISC_RESIDENT_EXIT;
 return result;}
static void load_app(const char *file,unsigned wanted) {
 char path[1024];snprintf(path,sizeof(path),"%s/%s",fixture_dir,file);void *lib=dlopen(path,RTLD_NOW|RTLD_LOCAL);if(!lib){fprintf(stderr,"%s\n",dlerror());abort();}
 const risc_resident_app_descriptor_v1_t *descriptor=dlsym(lib,RISC_RESIDENT_DESCRIPTOR_SYMBOL);assert(descriptor&&descriptor->role==wanted&&descriptor->struct_size==sizeof(*descriptor));
 int (*init)(void)=dlsym(lib,"app_module_init");void(*entry)(void)=dlsym(lib,"app_main"),(*fini)(void)=dlsym(lib,"app_module_fini");assert(init&&entry&&fini);
 int initialized=init();if(mode==10){assert(initialized!=0);dlclose(lib);return;}assert(initialized==0);entries++;entry();if(!terminal){
#ifdef TEST_RESIDENT_LEGACY_HANDOFF
 finalizing=true;
#endif
 fini();finis++;dlclose(lib);
#ifdef TEST_RESIDENT_LEGACY_HANDOFF
 finalizing=false;
#endif
 }
}
static int32_t run_child(uint64_t invocation,const char*path,risc_resident_result_v1*out){live();assert(role==1&&invocation==1&&host_registered&&!in_dispatch&&!focus&&!subs&&!frame);child_exit=false;
#ifdef TEST_RESIDENT_LEGACY_HANDOFF
 if(!path) {
  ++continuation_queries;
  if(mode==16){legacy_unwind=true;out->status=RISC_RESIDENT_HANDOFF;return out->status;}
  if(mode==17){terminal=true;out->status=RISC_RESIDENT_RETAINED;return out->status;}
  if(mode==18){out->status=RISC_RESIDENT_FAILED;return out->status;}
  if(mode!=14){out->status=RISC_RESIDENT_NO_PENDING;return out->status;}
  path="client.elf";
 }
 if(mode==13){legacy_unwind=true;out->status=RISC_RESIDENT_HANDOFF;return out->status;}
#endif
 if(!strcmp(path,"usb_sd_transfer.elf")){assert(mode==1);exit_requests++;out->status=RISC_RESIDENT_OK;return RISC_RESIDENT_OK;}
 assert(!strcmp(path,"client.elf"));if(mode==4){out->status=RISC_RESIDENT_DENIED;return RISC_RESIDENT_DENIED;}
 role=2;iteration=ms;load_app("client.elf",2);role=1;
#ifdef TEST_RESIDENT_LEGACY_HANDOFF
 if(mode==19){legacy_unwind=true;out->status=RISC_RESIDENT_HANDOFF;return out->status;}
#endif
 out->status=terminal?RISC_RESIDENT_RETAINED:RISC_RESIDENT_OK;return out->status;}
static bool resident_get(risc_resident_client_v1*out){live();if(out->struct_size<sizeof(*out))return false;*out=(risc_resident_client_v1){.api_version=1,.struct_size=sizeof(*out),.invocation=role,.role=role,.register_shell=register_shell,.run_foreground=run_child,.checkpoint=checkpoint,.request_foreground_exit=request_exit};if(mode==8 && role==1)out->struct_size=8;if(mode==9 && role==2)out->api_version=2;return true;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.health=health,.yield_ms=wait_ms,.diagnostic=diagnostic,.request_launch=request_launch,.acquire=acquire,.release=release,.retain_invocation=retain,.resident_shell=resident_get};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t version){static risc_runtime_api_v1 short_runtime;short_runtime=runtime;if(mode==10)short_runtime.struct_size=RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE;return version==1?&short_runtime:NULL;}
bool resident_fixture_guard(void){++guard_calls;if(mode==11 && guard_calls==1){phase=4;iteration=ms;return false;}return true;}
void resident_fixture_event(unsigned event,int value){
 if(event==1){host_result=(unsigned)(value+1);return;}
 if(event==5){sleep_seen=(unsigned)value;return;}
 if(event==6){host_polling=value!=0;return;}
 if(event==3){assert(value==71);return;}
 assert(event==2&&role==2);client_polls++;
 if(mode==2 && client_polls>10){phase=4;iteration=0;}
}
int main(int argc,char**argv){assert(argc==3);fixture_dir=argv[1];mode=(unsigned)atoi(argv[2]);if(mode==7)alarm.output_modes=ALARM_MODE_SOUND;load_app("host.elf",1);
#ifdef TEST_RESIDENT_LEGACY_HANDOFF
 if(mode>=13 && mode<20) {
  assert(continuation_queries==1);
  if(mode==17){assert(terminal&&!after_terminal&&host_result==0&&!finis&&!host_frames);}
  else {
   assert(!terminal&&!frame&&!grants&&!subs&&focus==0);
   if(mode==13)assert(host_result==3&&host_frames==1&&entries==1&&finis==1);
   if(mode==14)assert(host_result==2&&host_frames>0&&entries==2&&finis==2);
   if(mode==15)assert(host_result==2&&overlays==2&&entries==2&&finis==2);
   if(mode==16)assert(host_result==3&&!host_frames&&entries==1&&finis==1);
   if(mode==18)assert(host_result==1&&!host_frames&&entries==1&&finis==1);
   if(mode==19)assert(host_result==3&&host_frames>0&&entries==2&&finis==2);
  }
  printf("Resident legacy mode=%u startup_queries=%u host_frames=%u retained=%u PASS\n",mode,continuation_queries,host_frames,terminal);return 0;
 }
#endif
 if(mode==3||mode==5||mode==9){assert(terminal&&!after_terminal&&host_result==0&&finis==0);}
 else if(mode==10){assert(!terminal&&!grants&&!subs&&!frame&&!entries&&!finis);}
 else {assert(!terminal&&!frame&&!grants&&!subs&&focus==0&&host_result==((mode==4||mode==8)?1u:2u));
  if(mode==20)assert(overlays==1&&exit_requests==1&&!sleep_seen);
  if(mode==0||mode==7||mode==12)assert(overlays==2&&snapshots==2&&host_frames>5&&child_frames==1&&requests==0&&entries==2&&finis==2);
  if(mode==1)assert(overlays==1&&exit_requests==2&&requests==0);
  if(mode==2)assert(host_frames==1&&snapshots>0&&requests==0);
  if(mode==6)assert(sleep_seen==1&&exit_requests==1&&!overlays&&!in_dispatch);
  if(mode==11)assert(!overlays&&host_frames==1&&guard_calls>=2);
  if(mode==4||mode==8)assert(entries==1&&finis==1&&!overlays);
 }
 printf("Resident ELF mode=%u overlays=%u checkpoints=%u host_frames=%u child_frames=%u focus_changes=%u retained=%u PASS\n",mode,overlays,checkpoints,host_frames,child_frames,focus_changes,terminal);return 0;}
