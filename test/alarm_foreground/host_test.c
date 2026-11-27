/* Actual shared modal presenter with a deterministic scheduler. */
#define main baseline_main
#include "../scene/host_test.c"
#undef main
#include "RiscSceneModalV1.h"
#include "RiscAlarmForegroundV1.h"
#include "AlarmServiceV2.h"
extern const risc_driver_v2 *alarm_driver_get(uint32_t);
static alarm_status_v1 alarm_view={.api_version=1,.struct_size=sizeof(alarm_view),.state=ALARM_STATE_READY};
static unsigned alarm_steps,alarm_statuses,alarm_acks,alarm_stops,alarm_refreshes,ack_delay;
static int alarm_result;
static bool alarm_loss;
static alarm_token_v1 acknowledged;
static int32_t alarm_status(void*c,alarm_status_v1*out){
 (void)c;assert(native_alive);alarm_statuses++;*out=alarm_view;return ALARM_OK;
}
static int32_t alarm_step(void*c){
 (void)c;assert(native_alive);alarm_steps++;
 if(alarm_loss){native_alive=false;return ALARM_OK;}
 if(alarm_view.state==ALARM_STATE_DISMISSING&&ack_delay&&!--ack_delay){
  alarm_view.state=ALARM_STATE_READY;memset(&alarm_view.occurrence,0,sizeof(alarm_view.occurrence));
 }
 return alarm_result;
}
static int32_t alarm_refresh(void*c){(void)c;assert(native_alive);alarm_refreshes++;return ALARM_OK;}
static int32_t alarm_ack(void*c,const alarm_token_v1*t){
 (void)c;assert(native_alive);alarm_acks++;acknowledged=*t;
 if(memcmp(t,&alarm_view.occurrence,sizeof(*t)))return ALARM_STALE;
 alarm_view.state=ALARM_STATE_DISMISSING;ack_delay=3;return ALARM_PENDING;
}
static int32_t alarm_sleep(void*c,alarm_sleep_v1*t){(void)c;(void)t;return ALARM_OK;}
static int32_t alarm_stop(void*c){(void)c;assert(native_alive);alarm_stops++;return ALARM_OK;}
static const alarm_service_v1 scheduler={1,sizeof(scheduler),NULL,alarm_status,alarm_step,alarm_refresh,alarm_ack,alarm_sleep,alarm_stop};
static const risc_alarm_foreground_v1 *alarm_ui;
static risc_alarm_foreground_status_v1 report;
static int advance_alarm(unsigned flags){
 report.struct_size=sizeof(report);ms+=20;
 return alarm_ui->advance(NULL,flags,&report);
}
int main(int argc,char**argv){
 assert(argc==2);const char*mode=argv[1];
 w=480;h=800;format=1;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,0xffff,0,0,0,0};
 startup();settle();const risc_scene_checkpoint_api_v1*cp=risc_scene_checkpoint_get_v1(api);assert(cp);
 const risc_driver_v2*ad=alarm_driver_get(2);assert(ad);alarm_ui=ad->capability;
 risc_provider_dependency_v1 deps[]={{"ui.scene",1,api},{"alarm.service",1,&scheduler}};
 alarm_service_descriptor_v2 tagged={.base=scheduler,.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1};
 if(!strcmp(mode,"tagged")||!strcmp(mode,"malformed-tagged")){
  tagged.base.api_version=2;tagged.base.struct_size=sizeof(tagged);
  if(!strcmp(mode,"malformed-tagged"))tagged.tag=0;
  deps[1].api_version=2;deps[1].api=&tagged;
 }
 risc_scene_api_v1 old=*api;
 if(!strcmp(mode,"old-scene")){old.struct_size=sizeof(old);deps[0].api=&old;}
 if(!strcmp(mode,"malformed-tagged")||!strcmp(mode,"old-scene")){
  assert(!ad->start(deps,2));finish();puts("Alarm clean incompatible dependency refusal PASS");return 0;
 }
 assert(ad->start(deps,2));assert(advance_alarm(0)==RISC_ALARM_FOREGROUND_OK&&!report.flags);
 if(!strcmp(mode,"bad-status")||!strcmp(mode,"uncertain-output")){
  if(!strcmp(mode,"bad-status"))alarm_view.api_version=0;else alarm_view.output_uncertain=1;
  assert(advance_alarm(0)==RISC_ALARM_FOREGROUND_RETAINED&&!ad->quiesce());
  unsigned before=alarm_statuses;assert(advance_alarm(0)==RISC_ALARM_FOREGROUND_RETAINED&&alarm_statuses==before);
  puts("Alarm malformed/uncertain copied status retention PASS");return 0;
 }
 if(!strcmp(mode,"cue")){
  alarm_view.state=ALARM_STATE_CUE;alarm_view.occurrence.generation=9;
  assert(advance_alarm(0)==0&&(report.flags&RISC_ALARM_FOREGROUND_CUE)&&!(report.flags&RISC_ALARM_FOREGROUND_NEEDS_FOCUS));
  assert(!ad->quiesce());alarm_view.state=ALARM_STATE_READY;alarm_view.occurrence.generation=0;
  assert(advance_alarm(0)==0&&!report.flags&&ad->quiesce());ad->stop();finish();
  puts("Alarm cue progresses without modal focus PASS");return 0;
 }
 if(!strcmp(mode,"loss")||!strcmp(mode,"output")){
  unsigned before=alarm_statuses;alarm_loss=!strcmp(mode,"loss");alarm_result=alarm_loss?0:ALARM_OUTPUT;
  assert(advance_alarm(0)==RISC_ALARM_FOREGROUND_RETAINED&&alarm_statuses==before);
  before=alarm_steps;assert(advance_alarm(0)==RISC_ALARM_FOREGROUND_RETAINED&&alarm_steps==before);
  assert(!ad->quiesce()&&!alarm_stops);puts("Alarm scheduler uncertainty retains without subsequent I/O PASS");return 0;
 }
 alarm_view.state=ALARM_STATE_ALERT;
 alarm_view.occurrence=(alarm_token_v1){ALARM_KIND_ALARM,7,1234,9};strcpy(alarm_view.label,"Test alarm");
 if(!strcmp(mode,"blocked")){alarm_view.state=ALARM_STATE_BLOCKED;alarm_view.error=ALARM_STORAGE;alarm_view.snapshot=42;memset(&alarm_view.occurrence,0,sizeof(alarm_view.occurrence));}
 assert(!advance_alarm(0)&&(report.flags&RISC_ALARM_FOREGROUND_NEEDS_FOCUS)&&!(report.flags&RISC_ALARM_FOREGROUND_HAS_FOCUS));
 assert(!cp->pause(NULL,session));assert(!held&&!presenting&&!subscribed);
 expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;
 assert(!advance_alarm(RISC_ALARM_FOREGROUND_FOCUS_AVAILABLE)&&(report.flags&RISC_ALARM_FOREGROUND_HAS_FOCUS));
 risc_scene_event_v1 e={.struct_size=sizeof(e)};assert(api->next(NULL,session,&e)==RISC_SCENE_BUSY);
 if(!strcmp(mode,"pending"))allow_complete=false;
 for(unsigned n=0;!presenting;n++){assert(n<2000);assert(advance_alarm(1)>=0);}
 if(!strcmp(mode,"pending")){
  unsigned before=alarm_steps;
  for(unsigned n=0;n<20;n++)assert(advance_alarm(1)==RISC_ALARM_FOREGROUND_AGAIN);
  assert(alarm_steps==before);allow_complete=true;
 }
 for(unsigned n=0;held||presenting;n++){assert(n<2000);assert(advance_alarm(1)>=0);}
 /* Back belongs to the modal, and ACK forwards its exact displayed token. */
 nav_pressed=RISC_NAV_BACK;
 if(strcmp(mode,"blocked")){
  for(unsigned n=0;!alarm_acks;n++){assert(n<2000);assert(advance_alarm(1)>=0);}
  assert(acknowledged.kind==ALARM_KIND_ALARM&&acknowledged.revision==7&&acknowledged.deadline==1234&&acknowledged.generation==9);
 }
 for(unsigned n=0;report.flags&(RISC_ALARM_FOREGROUND_NEEDS_FOCUS|RISC_ALARM_FOREGROUND_HAS_FOCUS);n++){
  assert(n<2000);assert(advance_alarm(1)>=0);
 }
 expected_intent=0; /* The restored parent is the legacy fixture document. */
 assert(!cp->resume(NULL,session,RISC_SCENE_RESUME_REDRAW|RISC_SCENE_RESUME_FOCUS_CHANGED));settle();
 assert(tick(&e)==RISC_SCENE_IDLE);assert(ad->quiesce());ad->stop();finish();
 printf("Shared alarm %s exact ACK, durable wait and parent focus restoration PASS\n",mode);return 0;
}
