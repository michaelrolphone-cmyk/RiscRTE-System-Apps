/* Actual production main loop; deterministic scene/update/Runtime endpoints.
 * Each scenario is a fresh process. No renderer or network transport claim. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../Apps/app_store_scene.c"
static risc_runtime_api_v1 test_rt,test_other;
static bool replaced;
static unsigned mode,yields,opens,closes,updates,nexts,steps,cancels,activations,released,retained,polls,policies,selected_events,modal_cancels,review_events;
static unsigned source_events;
static software_update_status_v1 test_state;
static risc_scene_components_api_v1 scene;
static void io(void){assert(!replaced&&!retained);}
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t v){assert(v==1);return replaced?&test_other:&test_rt;}
static bool ustatus(void*c,software_update_status_v1*out){(void)c;io();*out=test_state;return true;}
static bool uget(void*c,uint32_t i,software_update_row_v1*out){(void)c;io();assert(!i);*out=(software_update_row_v1){.struct_size=sizeof(*out),.availability=SOFTWARE_UPDATE_AVAILABLE,.size=100};strcpy(out->id,"package");strcpy(out->version,"2");strcpy(out->installed,"1");strcpy(out->reason,"new release");return true;}
static bool ustep(void*c){(void)c;io();steps++;if(test_state.state==SOFTWARE_UPDATE_VERIFYING)test_state.state=SOFTWARE_UPDATE_READY;return true;}
static bool ucancel(void*c){(void)c;io();cancels++;test_state.state=SOFTWARE_UPDATE_LIST;test_state.resources_open=false;return true;}
static bool uactivate(void*c){(void)c;io();activations++;return true;}
static bool urefresh(void*c,uint64_t utc){(void)c;(void)utc;io();assert(mode!=10);return true;}
static bool ubegin(void*c,uint32_t i,uint64_t utc){(void)c;(void)i;(void)utc;assert(!"unexpected install");return false;}
static bool urestart(void*c){(void)c;assert(!"unexpected restart");return false;}
static const software_update_v1 service={1,sizeof(service),NULL,urefresh,ustep,ustatus,uget,ubegin,ucancel,uactivate,urestart};
static int catalog_ok(void*c){(void)c;io();return 0;}
static uint32_t catalog_count(void*c){(void)c;io();return 0;}
static uint32_t catalog_gen(void*c){(void)c;io();return 1;}
static bool catalog_get(void*c,uint32_t i,installed_app_v1*out){(void)c;(void)i;(void)out;assert(!"empty catalog read");return false;}
static int feed_refresh(void*c,uint64_t utc){(void)c;(void)utc;assert(!"source switch must not refresh feed");return -1;}
static int feed_install(void*c,uint32_t i,uint32_t g){(void)c;(void)i;(void)g;assert(!"source switch must not install");return -1;}
static int provision(void*c,const char*j){(void)c;(void)j;assert(!"source switch must not provision");return -1;}
static const installed_apps_feed_v1 feed={.base={.base={.api_version=1,.struct_size=sizeof(feed),.refresh=catalog_ok,.generation=catalog_gen,.count=catalog_count,.get=catalog_get,.cancel=catalog_ok},.provision_tag=INSTALLED_APPS_PROVISION_TAG,.provision_version=1,.install_descriptor=provision},.feed_tag=INSTALLED_APPS_FEED_TAG,.feed_version=1,.refresh=feed_refresh,.step=catalog_ok,.cancel=catalog_ok,.generation=catalog_gen,.count=catalog_count,.get=catalog_get,.install=feed_install};
static int32_t sopen(void*c,const risc_components_document_v1*d,const risc_scene_navigation_v1*n,uint64_t*t){(void)c;io();assert(d->root==1&&n->depth==1);opens++;*t=1;return 0;}
static int32_t supdate(void*c,uint64_t token,const risc_components_document_v1*d){(void)c;(void)d;io();assert(token==1);if(mode==1)assert(yields>=2);updates++;return 0;}
static int32_t sclose(void*c,uint64_t token){(void)c;io();assert(token==1);closes++;return 0;}
static int32_t configure(void*c,uint64_t token,uint32_t flags){(void)c;(void)flags;io();assert(token==1);return 0;}
static int32_t snapshot(void*c,uint64_t token,risc_scene_navigation_v1*n,uint32_t*f){(void)c;io();assert(token==1);*n=(risc_scene_navigation_v1){.api_version=1,.struct_size=sizeof(*n),.depth=1,.routes={1}};*f=mode==1&&yields<2?RISC_SCENE_INPUT_BUSY:0;
 if(mode==2){n->depth=0;}if(mode==3){return -99;}if(mode==6){replaced=true;}if(mode==7){n->routes[1]=99;}if(mode==8){*f=UINT32_MAX;}return 0;}
static int event(risc_scene_event_v1*e,unsigned action,bool zero){*e=(risc_scene_event_v1){.struct_size=sizeof(*e),.kind=RISC_SCENE_ACTION_EVENT,.sequence=nexts,.document_revision=as_document.revision,.action=action};if(!zero){for(unsigned i=0;i<as_document.node_count;i++)if(as_document.nodes[i].action==action){e->node=as_document.nodes[i].id;return 0;}assert(!"action missing from actual document");}return 0;}
static int32_t snext(void*c,uint64_t token,risc_scene_event_v1*e){(void)c;io();assert(token==1);nexts++;assert(nexts<200);
 if(mode==10&&source_events<2&&asp_presented.feed_enabled==(source_events==1)){assert(as_feed_available&&!asc.radio_owned&&!asc.connecting&&!asc.status.resources_open);source_events++;return event(e,ASP_SOURCE,false);}
 if(mode==0){if(nexts==1)return event(e,999,true);if(nexts==2)return event(e,ASP_CANCEL,false);}
 if(mode==1){if(yields<2){asc.dirty=true;assert(!steps&&!activations);return RISC_SCENE_IDLE;}if(!cancels)return event(e,ASP_CANCEL,false);}
 if(mode==4||mode==5){if(!selected_events){selected_events++;return event(e,ASP_SELECT_BASE,false);}if(mode==5&&!review_events){for(unsigned i=0;i<as_document.node_count;i++)if(as_document.nodes[i].action==ASP_REVIEW){review_events++;return event(e,ASP_REVIEW,false);}}if(mode==5&&as_document.cancel_action&&!modal_cancels){modal_cancels++;return event(e,as_document.cancel_action,true);}}
 if(mode==9&&nexts<=33)return event(e,999,true);
 if(mode==9&&nexts==34)return event(e,ASP_CANCEL,false);
 return RISC_SCENE_IDLE;}
static bool acquire_(const char*name,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){io();assert(v==1);if(!strcmp(name,RISC_SCENE_CAPABILITY)){assert(!id);g->api=&scene;}else if(!strcmp(name,SOFTWARE_UPDATE_APPS_CAPABILITY)){assert(!id);g->api=&service;}else if(mode==10&&!strcmp(name,INSTALLED_APPS_CAPABILITY)){g->api=&feed;}else {if(mode==10)assert(strcmp(name,"net.wifi"));return false;}g->slot=1;g->generation=1;return true;}
static bool release_(risc_runtime_capability_v1*g){io();released++;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static bool health_(risc_runtime_health_v1*h){io();h->struct_size=sizeof(*h);h->uptime_ms=yields*8;return true;}
static bool mono_(uint32_t*t){io();*t=yields*8;return true;}
static void yield_(uint32_t n){(void)n;io();yields++;if(yields==9)fprintf(stderr,"stuck mode%u confirm%d dirty%d updates%u cancelaction%u selected%u modal%u\n",mode,asc.confirming,asc.dirty,updates,as_document.cancel_action,selected_events,modal_cancels);assert(yields<10);}
static bool exit_(void){io();if(mode==10)return source_events==2&&!as_feed_enabled;if(mode==4){assert(asc.confirming&&!cancels&&!policies);return yields>=2;}if(mode==5)return modal_cancels>0;return yields>=3;}
static bool retain_(void){assert(!replaced);retained++;return true;}
static int32_t checkpoint(uint64_t token,const risc_resident_request_v1*q,risc_resident_reply_v1*r){io();assert(token==42);if(q->reason==RISC_RESIDENT_CHECKPOINT_POLL){polls++;if(asc.confirming)assert((q->flags&(RISC_RESIDENT_POLL_INHIBIT_IDLE|RISC_RESIDENT_POLL_INHIBIT_POLICY))==(RISC_RESIDENT_POLL_INHIBIT_IDLE|RISC_RESIDENT_POLL_INHIBIT_POLICY));r->flags=RISC_RESIDENT_REPLY_POLICY_REQUEST;return 0;}policies++;return 0;}
static bool resident_(risc_resident_client_v1*c){io();if(mode!=4)return false;*c=(risc_resident_client_v1){.api_version=1,.struct_size=sizeof(*c),.invocation=42,.role=RISC_RESIDENT_ROLE_FOREGROUND,.checkpoint=checkpoint};return true;}
int main(int argc,char**argv){assert(argc==2);mode=(unsigned)atoi(argv[1]);test_rt=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(test_rt),.acquire=acquire_,.release=release_,.health=health_,.monotonic_ms=mono_,.yield_ms=yield_,.exit_requested=exit_,.retain_invocation=retain_,.resident_shell=resident_};test_other=test_rt;
 scene=(risc_scene_components_api_v1){.lifecycle={.base={.api_version=1,.struct_size=sizeof(scene),.next=snext,.snapshot=snapshot,.close=sclose},.configure=configure},.tag=RISC_COMPONENTS_TAG,.version=1,.open=sopen,.update=supdate};
 test_state=(software_update_status_v1){.struct_size=sizeof(test_state),.state=(mode==0||mode==1||mode==9)?SOFTWARE_UPDATE_VERIFYING:SOFTWARE_UPDATE_LIST,.count=1,.done=50,.total=100};app_main();
 fprintf(stderr,"mode%u opens%u close%u release%u retain%u cancel%u steps%u next%u terminal%d\n",mode,opens,closes,released,retained,cancels,steps,nexts,as_terminal);assert(!activations);
 if(mode==2||mode==3||mode==7||mode==8){assert(retained==1&&!nexts&&!steps&&!closes&&!released);}
 else if(mode==6){assert(replaced&&!retained&&!nexts&&!steps&&!closes&&!released);}
 else {assert(cancels&&closes&&released==(mode==10?3u:2u)&&!retained);if(mode==10)assert(source_events==2&&!as_feed_enabled);if(mode==4)assert(polls&&!policies);if(mode==5)assert(modal_cancels==1);if(mode==9)assert(nexts>=34);}
 printf("main-loop scenario %u passed\n",mode);return 0;}
