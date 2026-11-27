/* Shared alarm foreground controller. All geometry belongs to ui.scene;
 * scheduler persistence, RTC and output policy remain with alarm.service. */
#include "RiscAlarmForegroundV1.h"
#include <stddef.h>
#include "AlarmServiceV2.h"
#include "RiscRuntimeV1.h"
#include "RiscProviderV2.h"
#include "RiscSceneModalV1.h"
#include <stdbool.h>
#include <string.h>
#include <limits.h>
static const risc_scene_api_v1 *scene;
static const risc_scene_components_api_v1 *components;
static const risc_scene_modal_api_v1 *modal;
static const alarm_service_v1 *scheduler;
static alarm_status_v1 state,published;
static alarm_token_v1 ack_token;
static risc_components_document_v1 document;
static uint64_t session,sequence;
static uint32_t revision,seen_snapshot;
static int32_t seen_error;
static bool started,retained,in_call,closing,ack_pending,refresh_pending,error_seen;
enum { DISMISS=1,RETRY=2 };
static int32_t retain(void){retained=true;return RISC_ALARM_FOREGROUND_RETAINED;}
static bool live(void){if(retained)return false;if(!risc_runtime_get_api(1)){retain();return false;}return true;}
static bool result(int rc){
 if(!live())return false;
 if(rc==ALARM_OUTPUT||rc==ALARM_RETAINED||rc<ALARM_FOREGROUND||rc>ALARM_PENDING){retain();return false;}
 return true;
}
static bool owned(void){return state.occurrence.generation||state.output_uncertain||
 state.state==ALARM_STATE_ALERT||state.state==ALARM_STATE_DISMISSING;}
static bool wants_focus(void){
 return state.state!=ALARM_STATE_CUE&&state.state!=ALARM_STATE_LOADING&&(owned()||(state.state==ALARM_STATE_BLOCKED&&
 (!error_seen||seen_error!=state.error||seen_snapshot!=state.snapshot)));
}
static bool read_status(void){
 alarm_status_v1 next={.struct_size=sizeof(next)};
 int rc=scheduler->status(scheduler->context,&next);
 if(!result(rc)||rc!=ALARM_OK||!result(next.error)||next.api_version!=1||
 next.struct_size<sizeof(next)||next.state>ALARM_STATE_CUE||next.output_uncertain>1)return false;
 if(next.output_uncertain){retain();return false;}
 state=next;return true;
}
static int32_t presentation_failure(void){
 /* A failed presenter allows only one bounded stop-only attempt sequence.
  * Do not ACK, restart output, release dependencies or retry normal work. */
 if(!live())return retain();
 for(unsigned n=0;n<3;n++){
  int rc=scheduler->stop_only(scheduler->context);
  if(!result(rc)||rc!=ALARM_PENDING)break;
 }
 return retain();
}
static void text(char *to,size_t n,const char *s){
 size_t z=0;while(z+1<n&&s[z])z++;memcpy(to,s,z);to[z]=0;
}
static const char *title(void){
 for(unsigned i=0;i<sizeof(state.label);i++){
  unsigned ch=(unsigned char)state.label[i];
  if(!ch)return i?state.label:(state.occurrence.kind==ALARM_KIND_COUNTDOWN?"COUNTDOWN FINISHED":"ALARM");
  if(ch<32||ch>126)break;
 }
 return state.occurrence.kind==ALARM_KIND_COUNTDOWN?"COUNTDOWN FINISHED":"ALARM";
}
static void node(unsigned kind,unsigned action,const char*label,const char*body){
 unsigned i=document.node_count++;
 document.nodes[i]=(risc_scene_node_v1){.id=i+1,.route=1,.kind=kind,.action=action,.step=1};
 text(document.nodes[i].label,sizeof(document.nodes[i].label),label);
 text(document.nodes[i].text,sizeof(document.nodes[i].text),body);
}
static bool changed(void){
 return state.state!=published.state||state.error!=published.error||
 memcmp(&state.occurrence,&published.occurrence,sizeof(state.occurrence))||
 memcmp(state.label,published.label,sizeof(state.label));
}
static bool declare(void){
 if(revision==UINT32_MAX){retain();return false;}
 document=(risc_components_document_v1){.api_version=1,.struct_size=sizeof(document),
 .revision=++revision,.root=1,.route_count=1,.screen_key=1,
 .flags=RISC_COMPONENTS_ALERT,.cancel_action=DISMISS};
 document.routes[0]=(risc_scene_route_v1){.id=1,.back_action=DISMISS};
 text(document.routes[0].title,sizeof(document.routes[0].title),title());
 node(RISC_SCENE_TEXT_NODE,0,title(),state.state==ALARM_STATE_BLOCKED?
 (state.error==ALARM_RTC?"RTC error":"Service error"):
 state.state==ALARM_STATE_DISMISSING?"Stopping / saving":"Time is up");
 node(RISC_SCENE_ACTION,DISMISS,state.occurrence.generation?"DISMISS":"BACK","");
 if(state.state==ALARM_STATE_BLOCKED)node(RISC_SCENE_ACTION,RETRY,"RETRY","");
 return true;
}
static int32_t snapshot_reply(risc_alarm_foreground_status_v1*out,int rc){
 uint32_t flags=(wants_focus()?RISC_ALARM_FOREGROUND_NEEDS_FOCUS:0)|
 (session?RISC_ALARM_FOREGROUND_HAS_FOCUS:0)|
 (state.state==ALARM_STATE_CUE?RISC_ALARM_FOREGROUND_CUE:0)|
 (state.state==ALARM_STATE_LOADING?RISC_ALARM_FOREGROUND_LOADING:0);
 *out=(risc_alarm_foreground_status_v1){.struct_size=sizeof(*out),.flags=flags,.alarm=state};
 return rc;
}
static int32_t close_modal(risc_alarm_foreground_status_v1*out){
 closing=true;int rc=modal->close(scene->context,session);
 if(!live())return retain();
 if(rc==RISC_SCENE_AGAIN||rc==RISC_SCENE_BUSY)return snapshot_reply(out,RISC_ALARM_FOREGROUND_AGAIN);
 if(rc==RISC_SCENE_RETAINED)return retain();
 if(rc!=RISC_SCENE_OK)return presentation_failure();
 session=0;closing=false;sequence=0;return snapshot_reply(out,RISC_ALARM_FOREGROUND_OK);
}
static int32_t advance_impl(uint32_t flags,risc_alarm_foreground_status_v1*out){
 if(!live())return retain();
 if(!started||!out||out->struct_size!=sizeof(*out)||flags&~RISC_ALARM_FOREGROUND_FOCUS_AVAILABLE)return RISC_ALARM_FOREGROUND_INVALID;
 if(session&&!(flags&RISC_ALARM_FOREGROUND_FOCUS_AVAILABLE))return RISC_ALARM_FOREGROUND_BUSY;
 if(closing)return close_modal(out);
 if(session){
  risc_scene_event_v1 e={.struct_size=sizeof(e)};
  int rc=scene->next(scene->context,session,&e);
  if(!live())return retain();
  if(rc==RISC_SCENE_RETAINED)return retain();
  if(rc!=RISC_SCENE_OK&&rc!=RISC_SCENE_IDLE&&rc!=RISC_SCENE_AGAIN)return presentation_failure();
  if(rc==RISC_SCENE_OK&&e.document_revision==revision&&e.sequence>sequence&&
   (!e.node||(e.node<=document.node_count&&document.nodes[e.node-1].action==e.action))){
   sequence=e.sequence;
   if(e.kind==RISC_SCENE_ACTION_EVENT&&e.action==DISMISS){
    if(published.occurrence.generation){if(!ack_pending){ack_token=published.occurrence;ack_pending=true;}}
    else if(published.state==ALARM_STATE_BLOCKED){
     error_seen=true;seen_error=state.error;seen_snapshot=state.snapshot;
     return close_modal(out);
    }
   }else if(e.kind==RISC_SCENE_ACTION_EVENT&&e.action==RETRY&&published.state==ALARM_STATE_BLOCKED)refresh_pending=true;
  }
  risc_scene_navigation_v1 nav={.struct_size=sizeof(nav)};uint32_t sf=0;
  rc=scene->snapshot(scene->context,session,&nav,&sf);
  if(!live())return retain();if(rc!=RISC_SCENE_OK)return presentation_failure();
  if(sf&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY))return snapshot_reply(out,RISC_ALARM_FOREGROUND_AGAIN);
 }
 if(refresh_pending){
  int rc=scheduler->refresh(scheduler->context);if(!result(rc))return retain();
  if(rc==ALARM_BUSY)return snapshot_reply(out,RISC_ALARM_FOREGROUND_AGAIN);
  refresh_pending=false;
 }
 if(ack_pending){
  int rc=scheduler->acknowledge(scheduler->context,&ack_token);if(!result(rc))return retain();
  if(rc==ALARM_BUSY)return snapshot_reply(out,RISC_ALARM_FOREGROUND_AGAIN);
  ack_pending=false;memset(&ack_token,0,sizeof(ack_token));
 }
 int rc=scheduler->step(scheduler->context);
 if(!result(rc)||!read_status())return retain();
 if(session&&!wants_focus()&&state.state!=ALARM_STATE_LOADING&&state.state!=ALARM_STATE_CUE)return close_modal(out);
 if(wants_focus()&&(flags&RISC_ALARM_FOREGROUND_FOCUS_AVAILABLE)&&!session){
  if(!declare())return retain();
  uint64_t token=0;rc=modal->open(scene->context,&document,&token);
  if(!live())return retain();
  if((rc==RISC_SCENE_BUSY||rc==RISC_SCENE_AGAIN)&&!token)return snapshot_reply(out,RISC_ALARM_FOREGROUND_AGAIN);
  if(rc==RISC_SCENE_RETAINED||(token&&rc!=RISC_SCENE_OK))return retain();
  if(rc!=RISC_SCENE_OK||!token)return presentation_failure();
  session=token;
  rc=components->lifecycle.configure(scene->context,session,0);
  if(!live()||rc==RISC_SCENE_RETAINED)return retain();
  if(rc!=RISC_SCENE_OK)return presentation_failure();
  published=state;
 }else if(session&&changed()){
  if(!declare())return retain();
  rc=components->update(scene->context,session,&document);
  if(!live())return retain();if(rc!=RISC_SCENE_OK)return presentation_failure();published=state;
 }
 return snapshot_reply(out,RISC_ALARM_FOREGROUND_OK);
}
static int32_t advance(void*c,uint32_t flags,risc_alarm_foreground_status_v1*out){
 (void)c;if(retained)return RISC_ALARM_FOREGROUND_RETAINED;
 if(in_call)return RISC_ALARM_FOREGROUND_BUSY;
 in_call=true;int rc=advance_impl(flags,out);in_call=false;return rc;
}
static bool start(const risc_provider_dependency_v1*deps,size_t count){
 if(started||retained||!deps||count!=2)return false;
 scene=NULL;scheduler=NULL;
 for(size_t i=0;i<count;i++){
  if(!deps[i].capability_id||!deps[i].api)return false;
  if(!strcmp(deps[i].capability_id,"ui.scene")&&deps[i].api_version==1&&!scene)scene=deps[i].api;
  else if(!strcmp(deps[i].capability_id,ALARM_SERVICE_CAPABILITY)&&!scheduler){
   const alarm_service_v1*s=deps[i].api;
   if(deps[i].api_version==2){if(!alarm_service_descriptor(s))return false;}
   else if(deps[i].api_version!=1||s->api_version!=1||s->struct_size<sizeof(*s)||
    !s->status||!s->step||!s->refresh||!s->acknowledge||!s->prepare_sleep||!s->stop_only)return false;
   scheduler=s;
  }else return false;
 }
 if(!scene||!scheduler)return false;
 components=risc_scene_components_get_v1(scene);modal=risc_scene_modal_get_v1(scene);
 if(!components||!components->lifecycle.configure||!modal||!scene->next||!scene->snapshot)return false;
 memset(&state,0,sizeof(state));memset(&published,0,sizeof(published));
 state.struct_size=sizeof(state);state.api_version=1;state.state=ALARM_STATE_LOADING;
 session=sequence=0;revision=0;closing=ack_pending=refresh_pending=error_seen=false;
 started=true;return true;
}
static bool quiesce(void){return !retained&&!in_call&&!session&&!owned()&&
 (state.state==ALARM_STATE_READY||state.state==ALARM_STATE_BLOCKED);}
static void stop(void){if(quiesce()){started=false;scene=NULL;components=NULL;modal=NULL;scheduler=NULL;}}
static const risc_alarm_foreground_v1 api={1,sizeof(api),NULL,advance};
static const risc_driver_v2 driver={2,sizeof(driver),"alarm-foreground",RISC_ALARM_FOREGROUND_CAPABILITY,1,&api,start,stop,quiesce};
__attribute__((visibility("default"))) const risc_driver_v2*t5_driver_get(uint32_t abi){return abi==2?&driver:NULL;}
