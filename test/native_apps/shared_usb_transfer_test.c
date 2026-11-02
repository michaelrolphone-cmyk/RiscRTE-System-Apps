#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "RiscRuntimeV1.h"
#include "RiscSceneComponentsV1.h"
#include "RiscUsbDeviceMscV1.h"
static const char *fault="";static bool fault_hit;
#define REVOKE(name,value) do { if(!strcmp(fault,name)){assert(live);fault_hit=true;live=false;return value;} } while(0)
static int mode,ticks,begins,ends,steps,polls,releases,launches,retains;
static unsigned model_state=RISC_USB_MSC_IDLE,pending;
static uint32_t revision;static uint64_t sequence;
static bool live=true,removed=false,configured=false;
static risc_scene_components_api_v1 presenter;
static risc_usb_device_msc_api_v1_prepare msc;
#include "AlarmServiceV2.h"
static alarm_service_descriptor_v2 alarm_fixture;
static bool alarm_dismissed;
static int alarm_steps,controls;
static risc_runtime_api_v1 fake_runtime;
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){return version==1&&live?&fake_runtime:NULL;}
#define PORTABLE_USB_TRANSFER_SHARED
#include "../../Apps/usb_sd_transfer.c"
static bool acquire(const char *name,uint32_t api,uint64_t instance,risc_runtime_capability_v1*out){
 assert(live&&instance==0);
 REVOKE("acquire",false);
 if(!strcmp(name,"alarm.service")){assert(api==2);if(mode!=7&&mode!=8&&mode!=14&&mode!=15&&mode!=16)return false;out->api=&alarm_fixture;out->slot=3;out->generation=1;return true;}
 assert(api==1);
 if(!strcmp(name,"ui.scene")){if(mode==14)return false;out->api=&presenter;}
 else {assert(!strcmp(name,"usb.device.msc"));if(mode==2)return false;out->api=&msc;}
 out->slot=!strcmp(name,"ui.scene")?1:2;out->generation=1;return true;
}
static bool release(risc_runtime_capability_v1*g){
 assert(live);REVOKE("release",true);if(g->slot==2&&mode==5&&begins)return false;
 ++releases;memset(g,0,sizeof(*g));return true;
}
static bool launch(const char*p){assert(live&&!token&&!scene_session);assert(!strcmp(p,"default.elf")||!strcmp(p,"springboard.elf"));++launches;return true;}
static bool retain(void){++retains;live=false;return true;}
static bool diagnostic(const char*p){assert(live&&p);REVOKE("diagnostic",true);return true;}
static void yield(uint32_t ms){assert(live&&(ms==1||ms==2));assert(++ticks<100);}
static int32_t begin(void*c,uint64_t*t){
 (void)c;assert(live);REVOKE("begin",RISC_USB_MSC_OK);++begins;*t=mode==6?0:7;
 if(mode==9)live=false;
 model_state=RISC_USB_MSC_PREPARING;
 return mode==6?RISC_USB_MSC_RETAINED:RISC_USB_MSC_OK;
}
static int32_t poll(void*c,uint64_t t,risc_usb_device_msc_status_v1*s){
 (void)c;assert(live&&t==7);REVOKE("poll",RISC_USB_MSC_OK);++polls;
 if((mode==0||mode==12||((mode==7)&&alarm_dismissed&&ticks>=20))&&model_state==RISC_USB_MSC_WAITING&&ticks>=9)model_state=RISC_USB_MSC_EJECTED;
 if(mode==3&&model_state==RISC_USB_MSC_WAITING)model_state=RISC_USB_MSC_CONNECTED;
 s->state=model_state;s->local_media_ready=model_state==RISC_USB_MSC_EJECTED;
 return 0;
}
static int32_t end(void*c,uint64_t t,uint32_t reason){
 (void)c;assert(live&&t==7);REVOKE("end",RISC_USB_MSC_OK);++ends;
 if(mode==3&&reason!=RISC_USB_MSC_END_CABLE_REMOVED)return RISC_USB_MSC_REFUSED;
 if(reason==RISC_USB_MSC_END_CABLE_REMOVED)removed=true;
 model_state=RISC_USB_MSC_DISCONNECTED;return 0;
}
static bool last_error(void*c,char*out,size_t n){(void)c;assert(live);REVOKE("last_error",true);snprintf(out,n,"fixture reason");return true;}
static int32_t prepare(void*c,uint64_t t){
 (void)c;assert(live&&t==7&&!pending&&preparing_presented);REVOKE("prepare",RISC_USB_MSC_OK);
 assert(!dirty);++steps;if(steps==2)model_state=RISC_USB_MSC_WAITING;return 0;
}
static int32_t open_scene(void*c,const risc_components_document_v1*d,const risc_scene_navigation_v1*n,uint64_t*t){
 (void)c;assert(live&&d->node_count&&n->depth==1);REVOKE("open",RISC_SCENE_OK);
 if(mode==16)return RISC_SCENE_UNAVAILABLE;if(mode==2){assert(state==RISC_USB_MSC_MEDIA_UNAVAILABLE);bool unavailable=false;
 for(unsigned i=0;i<d->node_count;i++)if(d->nodes[i].action==USB_START_STOP)unavailable=(d->nodes[i].flags&RISC_SCENE_DISABLED)!=0;
 assert(unavailable&&strstr(detail,"unavailable"));}
 revision=d->revision;*t=9;pending=2;return 0;
}
static int32_t update_scene(void*c,uint64_t t,const risc_components_document_v1*d){
 (void)c;assert(live&&t==9&&d->revision>revision);REVOKE("update",RISC_SCENE_OK);revision=d->revision;pending=2;return 0;
}
static int32_t configure(void*c,uint64_t t,uint32_t f){(void)c;assert(live&&t==9&&f==(resident.invocation?RISC_SCENE_FEATURE_SHARED_CONTROLS:0));REVOKE("configure",RISC_SCENE_OK);configured=true;return 0;}
static int32_t snapshot_scene(void*c,uint64_t t,risc_scene_navigation_v1*n,uint32_t*f){
 (void)c;(void)n;assert(live&&t==9&&configured);REVOKE("snapshot",RISC_SCENE_OK);
 *f=pending?RISC_SCENE_PRESENTING:0;
 if(pending)--pending;
 if(mode==4&&ticks==7)return RISC_SCENE_RETAINED;
 return 0;
}
static int32_t next_scene(void*c,uint64_t t,risc_scene_event_v1*e){
 (void)c;assert(live&&t==9);REVOKE("next",RISC_SCENE_IDLE);
 unsigned action=0;
 if(ticks==3)action=(mode==10||mode==13)?USB_CONTROLS:USB_START_STOP;
 if(mode==7&&ticks==14)action=USB_ALARM_DISMISS;
 if((mode==10||mode==13)&&ticks==10)action=USB_BACK;
 if(mode==11&&ticks==3)action=USB_BACK;
 if(mode==11&&ticks==9)action=USB_BACK;
 if(mode==1&&ticks==4)action=USB_START_STOP;
 if(mode==1&&ticks==9)action=USB_BACK;
 if(mode==2&&ticks==9)action=USB_BACK;
 if(mode==3&&ticks==10)action=USB_START_STOP;
 if(mode==3&&ticks==14)action=USB_KEEP;
 if(mode==3&&ticks==18)action=USB_START_STOP;
 if(mode==3&&ticks==22)action=USB_REMOVED;
 if(mode==3&&ticks==26)action=USB_BACK;
 if(mode==5&&ticks==9)action=USB_START_STOP;
 if(mode==6&&ticks==9)return RISC_SCENE_RETAINED;
 if(!action)return RISC_SCENE_IDLE;
 *e=(risc_scene_event_v1){.struct_size=sizeof(*e),.kind=RISC_SCENE_ACTION_EVENT,
 .document_revision=revision,.node=(mode==11&&ticks==3)?999:action==USB_BACK||action==USB_KEEP?21:action==USB_CONTROLS?23:20,.action=action,.sequence=++sequence};
 if(mode==13&&ticks==3){e->kind=RISC_SCENE_CONTROLS_EVENT;e->node=e->action=0;}
 return 0;
}
static int32_t close_scene(void*c,uint64_t t){(void)c;assert(live&&t==9&&!token);REVOKE("close",RISC_SCENE_OK);return 0;}

static int32_t alarm_step(void*c){(void)c;assert(live);REVOKE("alarm_step",ALARM_OK);++alarm_steps;return 0;}
static int32_t alarm_get(void*c,alarm_status_v1*s){
 (void)c;assert(live);REVOKE("alarm_status",ALARM_OK);s->api_version=1;s->struct_size=sizeof(*s);
 s->state=ticks>=8&&!alarm_dismissed?ALARM_STATE_ALERT:ALARM_STATE_READY;
 s->snapshot=s->state;s->output_uncertain=mode==8&&ticks>=8;
 if(s->state==ALARM_STATE_ALERT)s->occurrence.generation=1;
 snprintf(s->label,sizeof(s->label),"fixture alarm");return 0;
}
static int32_t alarm_ack(void*c,const alarm_token_v1*t){
 (void)c;assert(live&&t->generation==1);REVOKE("alarm_ack",ALARM_OK);alarm_dismissed=true;return 0;
}
static int32_t alarm_sleep(void*c,alarm_sleep_v1*t){(void)c;(void)t;assert(0);return ALARM_INVALID;}
static int32_t checkpoint(uint64_t id,const risc_resident_request_v1*q,risc_resident_reply_v1*r){
 assert(live&&id==77&&!scene_session&&!token);
 assert(q->reason==RISC_RESIDENT_CHECKPOINT_CONTROLS);r->flags=0;++controls;return 0;
}
static bool resident_get(struct risc_resident_client_v1*r){
 if(mode!=10&&mode!=12&&mode!=13)return false;
 *r=(risc_resident_client_v1){.api_version=1,.struct_size=sizeof(*r),.invocation=77,
 .role=RISC_RESIDENT_ROLE_FOREGROUND,.checkpoint=checkpoint};return true;
}

int main(int argc,char**argv){
 assert(argc==2||argc==3);mode=atoi(argv[1]);if(argc==3)fault=argv[2];
 fake_runtime=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(fake_runtime),
 .yield_ms=yield,.diagnostic=diagnostic,.request_launch=launch,.acquire=acquire,
 .release=release,.retain_invocation=retain,.resident_shell=resident_get};
 alarm_fixture=(alarm_service_descriptor_v2){.base={.api_version=2,.struct_size=sizeof(alarm_fixture),.status=alarm_get,.step=alarm_step,.refresh=alarm_step,.acknowledge=alarm_ack,.prepare_sleep=alarm_sleep,.stop_only=alarm_step},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1};
 msc=(risc_usb_device_msc_api_v1_prepare){.base={.api_version=1,.struct_size=sizeof(msc),
 .begin=begin,.poll=poll,.end=end,.last_error=last_error},
 .prepare_tag=RISC_USB_MSC_PREPARE_TAG,.prepare_version=1,.prepare_step=prepare};
 presenter=(risc_scene_components_api_v1){.lifecycle={.base={.api_version=1,
 .struct_size=sizeof(presenter),.next=next_scene,.snapshot=snapshot_scene,.close=close_scene},
 .configure=configure},.tag=RISC_COMPONENTS_TAG,.version=1,.open=open_scene,.update=update_scene};
 if(mode==15)presenter.tag=0;
 assert(app_module_init()==0);app_main();app_module_fini();
 if(*fault){assert(fault_hit&&!live&&!launches&&!retains);printf("shared USB revoked callback %s PASS\n",fault);return 0;}
 if(mode==0){assert(begins==1&&steps==2&&ends==1&&launches==1&&!retains);}
 if(mode==1){assert(begins==1&&steps==0&&ends==1&&launches==1&&!retains);}
 if(mode==2){assert(!begins&&!steps&&launches==1&&!retains);}
 if(mode==3){assert(removed&&ends==3&&launches==1&&!retains);}
 if(mode==4||mode==5||mode==6){assert(retains==1&&!launches);}
 if(mode==7){assert(alarm_dismissed&&alarm_steps&&ends==1&&launches==1&&!retains);}
 if(mode==8){assert(retains==1&&!launches);}
 if(mode==9){assert(!live&&begins==1&&!polls&&!steps&&!ends&&!launches&&!retains);}
 if(mode==10||mode==13){assert(controls==1&&!begins&&launches==1&&!retains);}
 if(mode==11){assert(!begins&&launches==1&&ticks>=9&&!retains);}
 if(mode>=14&&mode<=16){assert(!begins&&!alarm_steps&&!retains&&!launches&&releases==(mode==14?2:3));}
 if(mode==12){assert(begins==1&&ends==1&&!launches&&!retains);}
 printf("shared USB scenario %d PASS ticks=%d polls=%d steps=%d\n",mode,ticks,polls,steps);
 return 0;
}
