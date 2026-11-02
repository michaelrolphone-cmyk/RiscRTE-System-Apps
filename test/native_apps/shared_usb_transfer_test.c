#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "RiscRuntimeV1.h"
#include "RiscSceneComponentsV1.h"
#include "RiscUsbDeviceMscV1.h"
static int mode,ticks,begins,ends,steps,polls,releases,launches,retains;
static unsigned model_state=RISC_USB_MSC_IDLE,pending;
static uint32_t revision;static uint64_t sequence;
static bool live=true,removed=false,configured=false;
static risc_scene_components_api_v1 presenter;
static risc_usb_device_msc_api_v1_prepare msc;
static risc_runtime_api_v1 fake_runtime;
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){return version==1&&live?&fake_runtime:NULL;}
#define PORTABLE_USB_TRANSFER_SHARED
#include "../../Apps/usb_sd_transfer.c"
static bool acquire(const char *name,uint32_t api,uint64_t instance,risc_runtime_capability_v1*out){
 assert(live&&api==1&&instance==0);
 if(!strcmp(name,"ui.scene"))out->api=&presenter;
 else {assert(!strcmp(name,"usb.device.msc"));if(mode==2)return false;out->api=&msc;}
 out->slot=!strcmp(name,"ui.scene")?1:2;out->generation=1;return true;
}
static bool release(risc_runtime_capability_v1*g){
 assert(live);if(g->slot==2&&mode==5)return false;
 ++releases;memset(g,0,sizeof(*g));return true;
}
static bool launch(const char*p){assert(live&&!token&&!scene_session);assert(!strcmp(p,"default.elf")||!strcmp(p,"springboard.elf"));++launches;return true;}
static bool retain(void){++retains;live=false;return true;}
static bool diagnostic(const char*p){assert(live&&p);return true;}
static void yield(uint32_t ms){assert(live&&(ms==1||ms==2));assert(++ticks<100);}
static int32_t begin(void*c,uint64_t*t){
 (void)c;++begins;*t=mode==6?0:7;model_state=RISC_USB_MSC_PREPARING;
 return mode==6?RISC_USB_MSC_RETAINED:RISC_USB_MSC_OK;
}
static int32_t poll(void*c,uint64_t t,risc_usb_device_msc_status_v1*s){
 (void)c;assert(live&&t==7);++polls;
 if(mode==0&&model_state==RISC_USB_MSC_WAITING&&ticks>=9)model_state=RISC_USB_MSC_EJECTED;
 if(mode==3&&model_state==RISC_USB_MSC_WAITING)model_state=RISC_USB_MSC_CONNECTED;
 s->state=model_state;s->local_media_ready=model_state==RISC_USB_MSC_EJECTED;
 return 0;
}
static int32_t end(void*c,uint64_t t,uint32_t reason){
 (void)c;assert(live&&t==7);++ends;
 if(mode==3&&reason!=RISC_USB_MSC_END_CABLE_REMOVED)return RISC_USB_MSC_REFUSED;
 if(reason==RISC_USB_MSC_END_CABLE_REMOVED)removed=true;
 model_state=RISC_USB_MSC_DISCONNECTED;return 0;
}
static bool last_error(void*c,char*out,size_t n){(void)c;snprintf(out,n,"fixture reason");return true;}
static int32_t prepare(void*c,uint64_t t){
 (void)c;assert(live&&t==7&&!pending&&preparing_presented);
 assert(!dirty);++steps;if(steps==2)model_state=RISC_USB_MSC_WAITING;return 0;
}
static int32_t open_scene(void*c,const risc_components_document_v1*d,const risc_scene_navigation_v1*n,uint64_t*t){
 (void)c;assert(d->node_count&&n->depth==1);revision=d->revision;*t=9;pending=2;return 0;
}
static int32_t update_scene(void*c,uint64_t t,const risc_components_document_v1*d){
 (void)c;assert(live&&t==9&&d->revision>revision);revision=d->revision;pending=2;return 0;
}
static int32_t configure(void*c,uint64_t t,uint32_t f){(void)c;assert(t==9&&!f);configured=true;return 0;}
static int32_t snapshot_scene(void*c,uint64_t t,risc_scene_navigation_v1*n,uint32_t*f){
 (void)c;(void)n;assert(live&&t==9&&configured);
 *f=pending?RISC_SCENE_PRESENTING:0;
 if(pending)--pending;
 if(mode==4&&ticks==7)return RISC_SCENE_RETAINED;
 return 0;
}
static int32_t next_scene(void*c,uint64_t t,risc_scene_event_v1*e){
 (void)c;assert(live&&t==9);
 unsigned action=0;
 if(ticks==3)action=USB_START_STOP;
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
 .document_revision=revision,.node=20,.action=action,.sequence=++sequence};
 return 0;
}
static int32_t close_scene(void*c,uint64_t t){(void)c;assert(live&&t==9&&!token);return 0;}
int main(int argc,char**argv){
 assert(argc==2);mode=atoi(argv[1]);
 fake_runtime=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(fake_runtime),
 .yield_ms=yield,.diagnostic=diagnostic,.request_launch=launch,.acquire=acquire,
 .release=release,.retain_invocation=retain};
 msc=(risc_usb_device_msc_api_v1_prepare){.base={.api_version=1,.struct_size=sizeof(msc),
 .begin=begin,.poll=poll,.end=end,.last_error=last_error},
 .prepare_tag=RISC_USB_MSC_PREPARE_TAG,.prepare_version=1,.prepare_step=prepare};
 presenter=(risc_scene_components_api_v1){.lifecycle={.base={.api_version=1,
 .struct_size=sizeof(presenter),.next=next_scene,.snapshot=snapshot_scene,.close=close_scene},
 .configure=configure},.tag=RISC_COMPONENTS_TAG,.version=1,.open=open_scene,.update=update_scene};
 assert(app_module_init()==0);app_main();app_module_fini();
 if(mode==0){assert(begins==1&&steps==2&&ends==1&&launches==1&&!retains);}
 if(mode==1){assert(begins==1&&steps==0&&ends==1&&launches==1&&!retains);}
 if(mode==2){assert(!begins&&!steps&&launches==1&&!retains);}
 if(mode==3){assert(removed&&ends==3&&launches==1&&!retains);}
 if(mode==4||mode==5||mode==6){assert(retains==1&&!launches);}
 printf("shared USB scenario %d PASS ticks=%d polls=%d steps=%d\n",mode,ticks,polls,steps);
 return 0;
}
