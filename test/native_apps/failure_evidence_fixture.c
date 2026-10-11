/* Only peripherals and physical input are synthetic. The test runner loads
 * the production Runtime/Graph and real System host as separate ELF modules. */
#ifdef FAILURE_PROVIDER
#include <RiscProviderV2.h>
extern const void *failure_provider_api(unsigned);
extern void failure_provider_event(unsigned);
static bool start(const risc_provider_dependency_v1 *deps,size_t count){(void)deps;failure_provider_event(1);return count==1;}
static bool quiesce(void){failure_provider_event(2);return true;}
static void stop(void){failure_provider_event(3);}
static risc_driver_v2 driver={2,sizeof(driver),FAILURE_CAPABILITY,FAILURE_CAPABILITY,FAILURE_VERSION,NULL,start,stop,quiesce};
__attribute__((visibility("default"))) const risc_driver_v2 *t5_driver_get(uint32_t abi){driver.capability=failure_provider_api(FAILURE_PROVIDER);return abi==2?&driver:NULL;}
#else
#include "RiscRuntimeV1.h"
#include "RiscResidentShellV1.h"
#include "RiscFailureEvidenceV1.h"
#include "PortableResidentShell.h"
#include "RiscDisplayOutputSnapshotV1.h"
#include "RiscTouchV1.h"
#include "RiscInputNavigationV1.h"
#include "RiscBatteryGaugeV1.h"
#include "PortableRtcClock.h"
#include "RiscKeyValueV1.h"
#include "AlarmServiceV1.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
extern bool failure_runtime_retained(void);
static const char *mode,*directory;
static unsigned ms,frame,token,pending,submitted_at,screens,normal_frames,touch_polls,nav_polls,focus,subscribed,provider_calls;
static unsigned returns,dismissals,alarms;
static unsigned hosts,reads,acks,stale_acks;
static risc_failure_evidence_v1 evidence;
static int boot_result=99;
static bool painting_failure,faulted;
static uint8_t pixels[48000],completed[48000];
bool failure_mode(const char *name){return !strcmp(mode,name);}
static bool is(const char *name){return failure_mode(name);}
static unsigned panel_width(void){return is("landscape")?800:480;}
static unsigned panel_height(void){return is("landscape")?480:800;}
unsigned failure_hosts(void){return hosts;}
const risc_runtime_api_v1 *failure_runtime_get_api(uint32_t version) {
 const risc_runtime_api_v1 *api=risc_runtime_get_api(version);
 if(!api || !is("old-runtime"))return api;
 static void *prefix;
 if(!prefix){prefix=malloc(RISC_RUNTIME_RESIDENT_SHELL_V1_SIZE);assert(prefix);memcpy(prefix,api,RISC_RUNTIME_RESIDENT_SHELL_V1_SIZE);uint32_t size=RISC_RUNTIME_RESIDENT_SHELL_V1_SIZE;memcpy((unsigned char*)prefix+sizeof(uint32_t),&size,sizeof(size));}
 return prefix;
}
static void live(void){assert(!failure_runtime_retained());++provider_calls;}
void failure_event(unsigned event,int value){
 if(event==20){++hosts;return;}
 if(event==1)boot_result=value;else if(event==3)returns+=(unsigned)value;else assert(false);
}
void failure_provider_event(unsigned event){(void)event;live();}
bool failure_health(risc_runtime_health_v1 *out){live();assert(ms<25000);out->uptime_ms=ms;return true;}
void failure_delay(uint32_t delay){live();ms+=delay;}
bool failure_log(const char *line){live();(void)line;return true;}
int32_t failure_kv_get(uint32_t ns,const char *key,void *out,uint32_t capacity,uint32_t *used){
 live();assert(ns==1);(void)key;(void)out;(void)capacity;*used=0;return RISC_KEY_VALUE_NOT_FOUND;
}
int32_t failure_kv_put(uint32_t ns,const char *key,const void *data,uint32_t size){live();(void)ns;(void)key;(void)data;(void)size;assert(false);return RISC_KEY_VALUE_INVALID;}
bool failure_prior(risc_resident_failure_v1 *out){(void)out;return false;}
int32_t failure_evidence_read(risc_failure_evidence_v1 *out) {
 assert(!failure_runtime_retained());++reads;assert(out->struct_size>=sizeof(*out));
 if(is("none"))return RISC_FAILURE_EVIDENCE_NONE;
 *out=evidence;return RISC_FAILURE_EVIDENCE_OK;
}
int32_t failure_evidence_ack(uint32_t boot,uint32_t sequence) {
 assert(!failure_runtime_retained());assert(screens && !frame && !pending);
 assert(dismissals==screens);++acks;
 if(is("ack-denied"))return RISC_FAILURE_EVIDENCE_DENIED;
 if(is("stale") && !stale_acks++){++evidence.record_sequence;return RISC_FAILURE_EVIDENCE_STALE;}
 assert(boot==evidence.record_boot && sequence==evidence.record_sequence);
 evidence.flags&=~RISC_FAILURE_PENDING;return RISC_FAILURE_EVIDENCE_OK;
}
static bool display_info(void *c,risc_display_info_v1 *out){live();(void)c;*out=(risc_display_info_v1){.width=panel_width(),.height=panel_height(),.supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_MONO1),.flags=RISC_DISPLAY_INFO_ASYNC_PRESENT|RISC_DISPLAY_INFO_RETAINS_IMAGE|RISC_DISPLAY_INFO_CLEAN_PRESENT|RISC_DISPLAY_INFO_PARTIAL_DAMAGE|RISC_DISPLAY_INFO_BRIGHTNESS,.damage_x_alignment=8,.damage_width_alignment=8};return true;}
static bool acquire_frame(void *c,uint32_t format,risc_display_surface_v1 *out){
 live();(void)c;assert(!frame&&!pending&&format==RISC_DISPLAY_FORMAT_MONO1);
 painting_failure=(evidence.flags&RISC_FAILURE_PENDING) && !is("none") && !is("old-runtime") && !is("old-system");
 if(painting_failure && is("acquire")){faulted=true;return false;}
 frame=1;memset(pixels,0xcd,sizeof(pixels));*out=(risc_display_surface_v1){.frame=1,.pixels=pixels,.width=panel_width(),.height=panel_height(),.stride_bytes=panel_width()/8,.size_bytes=sizeof(pixels),.pixel_format=format};
 if(painting_failure && is("surface")){out->width=479;faulted=true;}
 return true;
}
static void release_frame(void *c,risc_display_frame_v1 value){live();(void)c;assert(value==1&&frame);frame=0;}
static bool submit_frame(void *c,risc_display_frame_v1 value,const risc_display_rect_v1 *damage,size_t count,const risc_display_present_options_v1 *options,risc_display_present_token_v1 *out){
 live();(void)c;(void)damage;assert(value==1&&frame);
 if(painting_failure) {
  assert(options->intent==RISC_DISPLAY_PRESENT_CLEAN && count==0 && !pending);
  if(is("submit")){faulted=true;return false;}
 }else ++normal_frames;
 frame=0;*out=++token;pending=token;submitted_at=ms;return true;
}
static bool status_frame(void *c,risc_display_present_token_v1 value,risc_display_present_status_v1 *out){
 live();(void)c;assert(value==token&&pending==token);
 if(painting_failure && is("status")){faulted=true;return false;}
 if(painting_failure && (is("failed") || is("superseded"))){faulted=true;out->state=is("failed")?RISC_DISPLAY_PRESENT_FAILED:RISC_DISPLAY_PRESENT_SUPERSEDED;return true;}
 if((painting_failure&&is("timeout"))||ms-submitted_at<3){out->state=RISC_DISPLAY_PRESENT_ACTIVE;return true;}
 pending=0;memcpy(completed,pixels,sizeof(pixels));out->state=RISC_DISPLAY_PRESENT_COMPLETE;
 if(painting_failure) {
  ++screens;touch_polls=nav_polls=0;
  char path[1024];snprintf(path,sizeof(path),"%s/failure-%s-%u.pbm",directory,mode,screens);FILE *file=fopen(path,"wb");assert(file);fprintf(file,"P4\n%u %u\n",panel_width(),panel_height());assert(fwrite(completed,1,sizeof(completed),file)==sizeof(completed));assert(!fclose(file));
  /* The entire stale frame was replaced, with white margins and real text. */
  unsigned stride=panel_width()/8;
  for(unsigned y=0;y<panel_height();++y){assert(!completed[y*stride]&&!completed[y*stride+stride-1]);}
  unsigned ink=0;for(unsigned i=0;i<sizeof(completed);++i)ink+=completed[i]!=0;assert(ink>500&&ink<20000);

 }
 return true;
}
static bool brightness(void *c,uint16_t value,uint16_t max){live();(void)c;assert(value<=max);return true;}
static const risc_display_output_api_v1 display={.api_version=1,.struct_size=sizeof(display),.get_info=display_info,.acquire=acquire_frame,.release=release_frame,.submit=submit_frame,.present_status=status_frame,.set_brightness=brightness};
static uint64_t subscribe(void *c){live();(void)c;assert(!subscribed);subscribed=1;return 1;}
static bool unsubscribe(void *c,uint64_t id){live();(void)c;assert(id==1&&subscribed);subscribed=0;return true;}
static bool poll_touch(void *c,size_t count){live();(void)c;assert(count==1);if(screens>dismissals)++touch_polls;return true;}
static int32_t next_touch(void *c,uint64_t id,risc_touch_event_v1 *out){live();(void)c;(void)out;assert(id==1);return 0;}
static bool snapshot_touch(void *c,risc_touch_snapshot_v1 *out){
 live();(void)c;*out=(risc_touch_snapshot_v1){.width=480,.height=800};
 if(screens>dismissals && is("touch-failure")){faulted=true;return false;}
 if(screens>dismissals && is("touch")) {
  /* Inherited held touch, release, an out-of-button tap, then Continue. */
  if(touch_polls==1 || touch_polls==2 || touch_polls==4 || touch_polls==6){out->contact_count=1;out->contacts[0]=(risc_touch_contact_v1){.id=1,.x=240,.y=touch_polls==4?300:750};}
  if(touch_polls==7)++dismissals;
 }
 return true;
}
static const risc_touch_api_v1 touch={1,sizeof(touch),NULL,subscribe,unsubscribe,poll_touch,next_touch,snapshot_touch};
static bool nav_poll(void *c,risc_input_navigation_frame_v1 *out){
 live();(void)c;assert(focus);*out=(risc_input_navigation_frame_v1){0};
 if(screens<=dismissals || is("touch"))return true;
 if(is("navigation")){faulted=true;return false;}
 ++nav_polls;
 if(is("held") && nav_polls<=2){out->buttons=out->pressed=RISC_NAV_HOME;return true;}
 if(is("home") && nav_polls==2){out->pressed=RISC_NAV_HOME;return true;}
 if(is("right") && nav_polls==2){out->pressed=RISC_NAV_RIGHT;if(screens==1)++dismissals;return true;}
 if(nav_polls==((is("held")||is("home")||is("right"))?4u:2u)){out->pressed=is("back") && screens==2?RISC_NAV_BACK:RISC_NAV_CONFIRM;++dismissals;}
 return true;
}
static bool nav_focus(void *c,const risc_input_foreground_v1 *claims,size_t count){live();(void)c;if(count){assert(count==1&&claims);focus=1;}else focus=0;return true;}
static bool nav_reset(void *c){live();(void)c;return true;}
static const risc_input_navigation_api_v1 navigation={1,sizeof(navigation),NULL,nav_poll,nav_focus,nav_reset};
static bool battery_read(void *c,risc_battery_sample_v1 *out){live();(void)c;*out=(risc_battery_sample_v1){.percent=84};return true;}
static const risc_battery_gauge_api_v1 battery={1,sizeof(battery),NULL,battery_read};
static bool rtc_read(void *c,twatch_rtc_time_v1 *out){live();(void)c;*out=(twatch_rtc_time_v1){2026,10,9,5,9,41,0};return true;}
static const twatch_rtc_api_v1 rtc={.api_version=2,.struct_size=sizeof(rtc),.read=rtc_read};
static int32_t alarm_status(void *c,alarm_status_v1 *out){live();(void)c;*out=(alarm_status_v1){.api_version=1,.struct_size=sizeof(*out),.state=ALARM_STATE_READY};return ALARM_OK;}
static int32_t alarm_step(void *c){live();(void)c;++alarms;return ALARM_OK;}
static int32_t alarm_ack(void *c,const alarm_token_v1 *t){live();(void)c;(void)t;return ALARM_OK;}
static int32_t alarm_prepare(void *c,alarm_sleep_v1 *t){live();(void)c;(void)t;return ALARM_INVALID;}
static const alarm_service_outputs_v1 alarm={.service={1,sizeof(alarm),NULL,alarm_status,alarm_step,alarm_step,alarm_ack,alarm_prepare,alarm_step},.output_modes=ALARM_MODE_VISUAL};
const void *failure_provider_api(unsigned index){const void *tables[]={NULL,&display,&touch,&navigation,&battery,&rtc,&alarm};assert(index<7);return tables[index];}
void failure_setup(const char *name,const char *root){
 mode=name;directory=root;
 evidence=(risc_failure_evidence_v1){.api_version=1,.struct_size=sizeof(evidence),.record_boot=7,.record_sequence=81,
  .current_reset_reason=6,.kind=RISC_FAILURE_NATIVE_PANIC,.flags=RISC_FAILURE_PENDING|RISC_FAILURE_CONTEXT|RISC_FAILURE_REGISTERS,
  .pc=0x42012345,.sp=0x3fca0000,.phase=RISC_FAILURE_PHASE_MAIN,.invocation=42,.frame_count=8,.history_count=8,.stack_status=RISC_FAILURE_STACK_LIMIT};
 strcpy(evidence.application,"apps/example.elf");strcpy(evidence.detail,"Exception captured before restart");
 for(unsigned i=0;i<8;++i){evidence.frames[i]=(risc_failure_frame_v1){0x42010000+i*0x20,0x3fca0000+i*0x40};evidence.history[i]=(risc_failure_step_v1){70+i,1+i,40+i};}
 if(is("empty")){evidence.flags=RISC_FAILURE_PENDING;evidence.kind=RISC_FAILURE_RESET_ONLY;evidence.application[0]=evidence.detail[0]=0;evidence.frame_count=evidence.history_count=0;evidence.stack_status=RISC_FAILURE_STACK_UNAVAILABLE;}
 if(is("unsupported")){evidence.stack_status=RISC_FAILURE_STACK_UNSUPPORTED;evidence.frame_count=0;evidence.flags&=~RISC_FAILURE_REGISTERS;}
 if(is("invalid-stack")){evidence.stack_status=RISC_FAILURE_STACK_INVALID;evidence.frame_count=3;}
 if(is("retention")){evidence.kind=RISC_FAILURE_RETENTION;evidence.status=-7;strcpy(evidence.detail,"Provider cleanup unconfirmed");}
 if(is("long")){memset(evidence.application,'W',sizeof(evidence.application));memset(evidence.detail,'W',sizeof(evidence.detail));evidence.frame_count=evidence.history_count=100;}
 if(is("acknowledged"))evidence.flags&=~RISC_FAILURE_PENDING;
}
void failure_verify(bool retained){
 bool unsafe=is("acquire")||is("surface")||is("submit")||is("status")||is("failed")||is("superseded")||is("timeout")||is("navigation")||is("touch-failure");
 assert(retained==unsafe);
 if(unsafe){assert(evidence.flags&RISC_FAILURE_PENDING);assert(!acks&&!normal_frames&&!returns);if(!is("timeout"))assert(faulted);}
 else if(is("ack-denied")){assert((evidence.flags&RISC_FAILURE_PENDING)&&acks==1&&!normal_frames&&!returns&&boot_result<0&&!frame&&!pending&&!focus&&!subscribed);}
 else {
  assert(!frame&&!pending&&!focus&&!subscribed&&returns==1&&boot_result==PORTABLE_RESIDENT_NO_PENDING);
  if(is("none")||is("acknowledged")||is("old-runtime")||is("old-system")){assert(!screens&&!acks);if(!is("none")&&!is("acknowledged"))assert(!reads);}
  else {assert(screens&&dismissals==screens&&!(evidence.flags&RISC_FAILURE_PENDING));assert(acks==(is("stale")?2u:1u));}
  if(is("repeat")){assert(hosts==3 && reads==4);}
  else assert(hosts==1);
 }
 printf("Failure evidence %s: screens=%u reads=%u acknowledgements=%u hosts=%u retained=%u PASS\n",mode,screens,reads,acks,hosts,retained);
}
#endif
