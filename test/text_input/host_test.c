#include "RiscTextEntryV1.h"
#include "RiscRuntimeV1.h"
#include "RiscSceneV1.h"
#include "SceneKeyboardV1.h"
#include "RiscProviderV2.h"
#include "RiscUsbHidV1.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool native_live=true;static const char *loss_site;
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){static const risc_runtime_api_v1 r={.api_version=1,.struct_size=sizeof(r)};return v==1&&native_live?&r:NULL;}
static void boundary(const char *site){assert(native_live);if(loss_site&&!strcmp(site,loss_site))native_live=false;}
static unsigned calls,scene_opens,scene_closes,unsubs;
static bool scene_active,kb_active,attached,held,bad_unsubscribe,bad_snapshot,bad_scene,bad_poll,pending;
static uint64_t sequence;
static risc_scene_document_v1 doc;
static risc_scene_event_v1 event;
static risc_usb_keyboard_event_v1 keys[40];static unsigned key_count,key_at;
static int32_t scene_open(void*c,const risc_scene_document_v1*d,const risc_scene_navigation_v1*p,uint64_t*out){boundary("scene-open");(void)c;(void)p;++calls;assert(!scene_active);scene_active=true;doc=*d;++scene_opens;*out=12;return 0;}
static int32_t scene_update(void*c,uint64_t s,const risc_scene_document_v1*d){boundary("scene-update");(void)c;++calls;assert(s==12&&scene_active&&d->revision>doc.revision);doc=*d;event=(risc_scene_event_v1){0};return 0;}
static int32_t scene_next(void*c,uint64_t s,risc_scene_event_v1*out){boundary("scene-next");(void)c;++calls;assert(s==12&&scene_active);if(bad_scene)return RISC_SCENE_RETAINED;if(!event.kind)return RISC_SCENE_IDLE;*out=event;event.kind=0;return 0;}
static int32_t scene_snapshot(void*c,uint64_t s,risc_scene_navigation_v1*out,uint32_t*f){boundary("scene-snapshot");(void)c;++calls;assert(s==12&&scene_active);*out=(risc_scene_navigation_v1){.struct_size=sizeof(*out)};*f=pending?RISC_SCENE_PRESENTING:0;return 0;}
static int32_t scene_close(void*c,uint64_t s){boundary("scene-close");(void)c;++calls;assert(s==12&&scene_active);if(pending)return RISC_SCENE_AGAIN;scene_active=false;++scene_closes;return 0;}
static const risc_scene_api_v1 scenes={1,sizeof(scenes),NULL,scene_open,scene_update,scene_next,NULL,scene_snapshot,scene_close};
static uint64_t subscribe(void*c,uint64_t device){boundary("subscribe");(void)c;++calls;assert(!kb_active&&!device);kb_active=true;return 55;}
static bool unsubscribe(void*c,uint64_t token){boundary("unsubscribe");(void)c;++calls;assert(token==55&&kb_active);if(bad_unsubscribe)return false;kb_active=false;++unsubs;return true;}
static bool keyboard_poll(void*c,size_t reports){boundary("poll");(void)c;++calls;assert(kb_active&&reports<=8);return !bad_poll;}
static int32_t keyboard_next(void*c,uint64_t token,risc_usb_keyboard_event_v1*out){boundary("next");(void)c;++calls;assert(token==55&&kb_active);if(key_at==key_count)return 0;*out=keys[key_at++];return out->kind==99?-2:out->kind==98?-1:1;}
static bool keyboard_snapshot(void*c,risc_usb_keyboard_state_v1*out,size_t*count){boundary("snapshot");(void)c;++calls;if(bad_snapshot)return false;assert(*count>=1);*count=attached?1:0;if(attached)*out=(risc_usb_keyboard_state_v1){.device=7,.keys={held?4:0},.connected=1};return true;}
static const risc_usb_keyboard_api_v1 keyboard={1,sizeof(keyboard),NULL,subscribe,unsubscribe,keyboard_poll,keyboard_next,keyboard_snapshot};
static const risc_text_entry_api_v1 *api;static const risc_driver_v2 *driver;static uint64_t session;
static risc_text_entry_state_v1 tick(void){risc_text_entry_state_v1 s={.struct_size=sizeof(s)};assert(api->poll(NULL,session,&s)==0);return s;}
static void key(unsigned usage,unsigned mods){keys[key_count++]=(risc_usb_keyboard_event_v1){++sequence,7,3,(uint8_t)usage,(uint8_t)mods,0};}
static void flush(void){key_count=key_at=0;}
static void scene_key(unsigned key){event=(risc_scene_event_v1){sizeof(event),RISC_SCENE_VALUE_EVENT,doc.revision,1,1,(int32_t)key,1};}
static void open_text(const char*text,unsigned cap){
 risc_text_entry_request_v1 r={.api_version=1,.struct_size=sizeof(r),.capacity=cap,.label="Name"};strcpy(r.text,text);
 assert(api->open(NULL,&r,&session)==0&&session);memset(&r,0xa5,sizeof(r));
 assert(!driver->quiesce());assert(!strcmp(tick().text,text));
}
static void finish(void){assert(api->close(NULL,session)==0);assert(driver->quiesce());assert(!scene_active&&!kb_active);driver->stop();}
int main(int argc,char**argv){
 assert(argc==2);const char*mode=argv[1];driver=t5_driver_get(2);assert(driver&&driver->abi_version==2);api=driver->capability;
 bool usb=strcmp(mode,"plain")&&strcmp(mode,"invalid")&&strcmp(mode,"scene-fault");
 risc_provider_dependency_v1 deps[]={{RISC_SCENE_CAPABILITY,1,&scenes},{"usb.hid.keyboard",1,&keyboard}};
 assert(driver->start(deps,usb?2:1));
 if(!strncmp(mode,"native-",7)){
  const char *site=mode+7;
  if(strcmp(site,"subscribe")&&strcmp(site,"scene-open"))open_text("",8);
  loss_site=site;int rc;
  if(!strcmp(site,"subscribe")||!strcmp(site,"scene-open")){risc_text_entry_request_v1 q={.api_version=1,.struct_size=sizeof(q),.capacity=8};rc=api->open(NULL,&q,&session);}
  else if(!strcmp(site,"unsubscribe")||!strcmp(site,"scene-close"))rc=api->close(NULL,session);
  else {if(!strcmp(site,"scene-update"))scene_key('a');risc_text_entry_state_v1 v={.struct_size=sizeof(v)};rc=api->poll(NULL,session,&v);}
  assert(!native_live&&rc==RISC_TEXT_ENTRY_RETAINED);unsigned before=calls;risc_text_entry_state_v1 v={.struct_size=sizeof(v)};assert(api->poll(NULL,session,&v)==RISC_TEXT_ENTRY_RETAINED&&api->close(NULL,session)==RISC_TEXT_ENTRY_RETAINED&&!driver->quiesce());driver->stop();assert(calls==before);
 }else if(!strcmp(mode,"invalid")){
  risc_text_entry_request_v1 r={.api_version=1,.struct_size=sizeof(r),.capacity=8};uint64_t s=0;
  r.text[0]='\n';assert(api->open(NULL,&r,&s)==RISC_TEXT_ENTRY_INVALID);r.text[0]=0;r.capacity=73;assert(api->open(NULL,&r,&s)==RISC_TEXT_ENTRY_INVALID);
  r.capacity=8;r.reserved=1;assert(api->open(NULL,&r,&s)==RISC_TEXT_ENTRY_INVALID);r.reserved=0;memset(r.label,'x',sizeof(r.label));assert(api->open(NULL,&r,&s)==RISC_TEXT_ENTRY_INVALID);assert(!s&&!calls);driver->stop();
 }else if(!strcmp(mode,"plain")){
  open_text("old",8);scene_key('a');assert(!strcmp(tick().text,"olda"));scene_key(129);assert(!strcmp(tick().text,"old"));scene_key(128);tick();assert(doc.nodes[0].value==1);
  scene_key('!');assert(!strcmp(tick().text,"old!"));scene_key(132);assert(tick().state==RISC_TEXT_ENTRY_CANCELLED);assert(tick().state==RISC_TEXT_ENTRY_CANCELLED);
  uint64_t stale=session;finish();assert(driver->start(deps,1));open_text("new",8);risc_text_entry_state_v1 s={.struct_size=sizeof(s)};assert(session!=stale&&api->poll(NULL,stale,&s)==RISC_TEXT_ENTRY_STALE);finish();
 }else if(!strcmp(mode,"pending")){
  open_text("",8);pending=true;assert(tick().flags&RISC_TEXT_ENTRY_PRESENTING);assert(api->close(NULL,session)==RISC_TEXT_ENTRY_AGAIN);assert(unsubs==1);unsigned before=calls;risc_text_entry_state_v1 s={.struct_size=sizeof(s)};assert(api->poll(NULL,session,&s)==RISC_TEXT_ENTRY_BUSY&&calls==before);pending=false;finish();assert(unsubs==1);
 }else if(!strcmp(mode,"scene-fault")||!strcmp(mode,"close-fault")||!strcmp(mode,"snapshot-fault")||!strcmp(mode,"keyboard-fault")){
  open_text("",8);int32_t rc;
  if(!strcmp(mode,"close-fault")){bad_unsubscribe=true;rc=api->close(NULL,session);}
  else{bad_scene=!strcmp(mode,"scene-fault");bad_snapshot=!strcmp(mode,"snapshot-fault");if(!strcmp(mode,"keyboard-fault")){keys[key_count++]=(risc_usb_keyboard_event_v1){.kind=99};}risc_text_entry_state_v1 s={.struct_size=sizeof(s)};rc=api->poll(NULL,session,&s);}
  assert(rc==RISC_TEXT_ENTRY_RETAINED);unsigned before=calls;risc_text_entry_state_v1 s={.struct_size=sizeof(s)};assert(api->poll(NULL,session,&s)==RISC_TEXT_ENTRY_RETAINED&&api->close(NULL,session)==RISC_TEXT_ENTRY_RETAINED&&!driver->quiesce());driver->stop();assert(calls==before);
 }else{
  if(!strcmp(mode,"held")){attached=held=true;}open_text("",5);
  if(!strcmp(mode,"held")){key(4,0);assert(!tick().text[0]);flush();held=false;tick();key(5,0);assert(!strcmp(tick().text,"b"));finish();}
  else{
   attached=true;keys[key_count++]=(risc_usb_keyboard_event_v1){++sequence,7,1,0,0,0};key(4,0);assert((tick().flags&RISC_TEXT_ENTRY_HARDWARE)&&!tick().text[0]);flush();assert(doc.nodes[0].flags&RISC_SCENE_DISABLED);
   if(!strcmp(mode,"attach-detach")){key(4,0);assert(!strcmp(tick().text,"a"));flush();scene_key('Q');assert(!strcmp(tick().text,"a"));attached=false;keys[key_count++]=(risc_usb_keyboard_event_v1){++sequence,7,2,0,0,0};key(5,0);assert(!(tick().flags&RISC_TEXT_ENTRY_HARDWARE));flush();scene_key('Q');assert(!strcmp(tick().text,"aQ"));}
   else if(!strcmp(mode,"gap")||!strcmp(mode,"transient")){key(4,0);if(!strcmp(mode,"gap"))keys[key_count++]=(risc_usb_keyboard_event_v1){.kind=98};else bad_poll=true;assert(!tick().text[0]);flush();bad_poll=false;key(5,0);assert(!strcmp(tick().text,"b"));}
   else if(!strcmp(mode,"drain")){for(unsigned i=0;i<34;i++)key(4,0);assert(!tick().text[0]);assert(!tick().text[0]);flush();key(5,0);assert(!strcmp(tick().text,"b"));}
   else if(!strcmp(mode,"ascii")){key(4,0);key(5,2);key(30,2);key(31,0);key(32,0);assert(!strcmp(tick().text,"aB!2"));flush();key(0x2a,0);assert(!strcmp(tick().text,"aB!"));flush();key(0x39,0);key(6,0);assert(!strcmp(tick().text,"aB!C"));flush();key(0x28,0);assert(tick().state==RISC_TEXT_ENTRY_ACCEPTED);}
   else if(!strcmp(mode,"shortcuts")){key(4,1);key(0x28,1);assert(!tick().text[0]&&tick().state==RISC_TEXT_ENTRY_EDITING);flush();key(0x29,0);assert(tick().state==RISC_TEXT_ENTRY_CANCELLED);}
   else assert(0);
   finish();
  }
 }
 printf("text host %s PASS (calls=%u opens=%u closes=%u)\n",mode,calls,scene_opens,scene_closes);return 0;
}
