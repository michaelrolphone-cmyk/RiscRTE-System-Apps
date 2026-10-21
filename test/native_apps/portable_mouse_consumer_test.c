#include "PortableMouseConsumer.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint64_t context, selected_instance, selected_filter;
static unsigned getters, acquires, releases, subscriptions, unsubscriptions, snapshots, reads, statuses, fences;
static int acquire_mode, release_mode, snapshot_mode, next_result;
static bool unsubscribe_ok, subscribe_ok, getter_ok, fence_ok, change_context;
static uint32_t provider_status;
static risc_usb_mouse_state_v1 states[4];
static size_t state_count;
static risc_usb_mouse_event_v1 event;
static uint64_t sub(void *p,uint64_t filter){(void)p;subscriptions++;selected_filter=filter;return subscribe_ok?57:0;}
static bool unsub(void *p,uint64_t s){(void)p;assert(s==57);unsubscriptions++;return unsubscribe_ok;}
static bool poll_forbidden(void *p,size_t n){(void)p;(void)n;assert(!"consumer polled provider");return false;}
static int32_t next(void *p,uint64_t s,risc_usb_mouse_event_v1 *out){(void)p;assert(s==57);reads++;*out=event;int rc=next_result;next_result=0;return rc;}
static bool snapshot(void *p,uint64_t s,risc_usb_mouse_state_v1 *out,size_t *n){
 (void)p;assert(s==57 && *n==4);snapshots++;
 if(snapshot_mode==1){*n=5;return false;}
 if(snapshot_mode==2){*n=5;return true;}
 memcpy(out,states,sizeof(states));*n=state_count;return true;
}
static uint32_t status(void *p){(void)p;statuses++;return provider_status;}
static risc_usb_mouse_api_v1 api;
static bool acquire(const char *cap,uint32_t v,uint64_t instance,risc_runtime_capability_v1 *g){
 assert(!strcmp(cap,"usb.hid.mouse") && v==1);acquires++;selected_instance=instance;
 if(change_context)context++;
 if(acquire_mode==1)return false;
 *g=(risc_runtime_capability_v1){sizeof(*g),2,9,&api};
 if(acquire_mode==2)return false;
 if(acquire_mode==3)g->slot=0;
 return true;
}
static bool release(risc_runtime_capability_v1 *g){
 releases++;assert(g->slot==2 && g->generation==9 && g->api==&api);
 assert(!subscriptions || unsubscriptions || !subscribe_ok);
 if(release_mode==1){g->api=NULL;return false;} /* Local copy may be mangled. */
 if(release_mode==2)return true;
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;
}
static bool get(risc_stream_client_v1 *out){
 getters++;if(!getter_ok)return false;
 *out=(risc_stream_client_v1){.api_version=1,.struct_size=sizeof(*out),.context=context};return true;
}
static bool fence(void){fences++;return fence_ok;}
static risc_runtime_api_v1 runtime;
static portable_mouse_consumer client;
static portable_mouse_frame frame;
static unsigned external(void){return getters+acquires+releases+subscriptions+unsubscriptions+snapshots+reads+statuses+fences;}
static void fixture(void){
 getters=acquires=releases=subscriptions=unsubscriptions=snapshots=reads=statuses=fences=0;
 acquire_mode=release_mode=snapshot_mode=next_result=0;
 subscribe_ok=unsubscribe_ok=getter_ok=fence_ok=true;change_context=false;
 context=77;provider_status=RISC_USB_MOUSE_RUNNING;state_count=0;
 memset(states,0,sizeof(states));memset(&event,0,sizeof(event));
 api=(risc_usb_mouse_api_v1){1,sizeof(api),NULL,sub,unsub,poll_forbidden,next,snapshot,status};
 runtime=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release,.retain_invocation=fence,.stream_client=get};
 assert(portable_mouse_init(&client,&runtime,81,0));
}
static risc_usb_mouse_state_v1 state(uint64_t device,uint64_t session,uint32_t buttons,uint8_t iface){
 return (risc_usb_mouse_state_v1){device,session,buttons,iface,0,1,0};
}
static void opened(void){assert(portable_mouse_open(&client,&frame)==PMC_BASELINE);assert(acquires==1 && subscriptions==1 && snapshots==1 && selected_instance==81);}
static void emit(uint8_t kind,uint64_t sequence,risc_usb_mouse_state_v1 s,uint32_t pressed,uint32_t released){
 event=(risc_usb_mouse_event_v1){.sequence=sequence,.state=s,.pressed=pressed,.released=released,.kind=kind};next_result=1;
}
static void terminal(void){
 assert(client.cleanup_required && client.result==PMC_RETAINED && !client.count);
 unsigned calls=external();
 assert(portable_mouse_next(&client,&frame)==PMC_RETAINED);
 assert(portable_mouse_close(&client,&frame)==PMC_RETAINED);
 assert(portable_mouse_open(&client,&frame)==PMC_RETAINED);
 assert(portable_mouse_focus(&client,true,&frame)==PMC_RETAINED);
 assert(calls==external());
}
static void normal(void){
 fixture();states[0]=state(1,10,1,0);states[1]=state(1,11,2,1);state_count=2;opened();
 assert(frame.current_count==2 && !frame.previous_count && !frame.event.kind);
 emit(RISC_USB_MOUSE_REPORT,9,state(1,10,3,0),2,0);
 event.x=-32768;event.y=32767;event.wheel=-2;event.pan=3;
 assert(portable_mouse_next(&client,&frame)==PMC_EVENT);
 assert(frame.event.x==-32768 && frame.event.y==32767 && frame.event.wheel==-2 && frame.event.pan==3);
 assert(frame.previous[0].buttons==1 && frame.current[0].buttons==3 && frame.current[1].buttons==2);
 assert(portable_mouse_next(&client,&frame)==PMC_EMPTY && !frame.event.kind && !frame.event.x);
 emit(RISC_USB_MOUSE_REPORT,100,state(1,11,0,1),0,2); /* Global sequence may skip. */
 assert(portable_mouse_next(&client,&frame)==PMC_EVENT && frame.current[0].buttons==3);
 emit(RISC_USB_MOUSE_DISCONNECTED,101,state(1,10,0,0),0,3);event.state.connected=0;
 assert(portable_mouse_next(&client,&frame)==PMC_EVENT && frame.current_count==1 && frame.event.released==3);
 assert(frame.current[0].session==11);
 emit(RISC_USB_MOUSE_CONNECTED,102,state(1,12,0,0),0,0);
 assert(portable_mouse_next(&client,&frame)==PMC_EVENT && frame.current_count==2);
 assert(portable_mouse_close(&client,&frame)==PMC_CLOSED && frame.previous_count==2 && !frame.current_count);
 assert(unsubscriptions==1 && releases==1 && !client.grant.api);
 assert(portable_mouse_close(&client,&frame)==PMC_CLOSED && releases==1);
 assert(portable_mouse_open(&client,&frame)==PMC_BASELINE && acquires==2);
}
static void gaps_and_focus(void){
 fixture();states[0]=state(3,7,4,0);state_count=1;snapshot_mode=1;
 assert(portable_mouse_open(&client,&frame)==PMC_GAP && client.subscription && client.needs_snapshot && !client.count);
 assert(snapshots==1 && reads==0);
 snapshot_mode=0;assert(portable_mouse_next(&client,&frame)==PMC_BASELINE && frame.current[0].buttons==4);
 next_result=-2;assert(portable_mouse_next(&client,&frame)==PMC_GAP && frame.previous[0].buttons==4 && !frame.current_count);
 assert(snapshots==2); /* No inline retry/snapshot after next. */
 states[0]=state(3,8,2,0);assert(portable_mouse_next(&client,&frame)==PMC_BASELINE && frame.current[0].session==8 && !frame.event.kind);
 unsigned before=snapshots+reads+statuses;
 assert(portable_mouse_focus(&client,false,&frame)==PMC_RESET && frame.previous[0].buttons==2 && !frame.current_count);
 assert(portable_mouse_next(&client,&frame)==PMC_PAUSED);
 assert(portable_mouse_focus(&client,false,&frame)==PMC_PAUSED);
 assert(portable_mouse_focus(&client,true,&frame)==PMC_RESET);
 assert(before==snapshots+reads+statuses);
 state_count=0;assert(portable_mouse_next(&client,&frame)==PMC_BASELINE && !frame.current_count);
 assert(portable_mouse_focus(&client,true,&frame)==PMC_EMPTY);
 assert(portable_mouse_next(&client,&frame)==PMC_EMPTY);
 provider_status=RISC_USB_MOUSE_CLOSING;
 assert(portable_mouse_next(&client,&frame)==PMC_UNAVAILABLE && client.needs_snapshot);
 provider_status=RISC_USB_MOUSE_RUNNING;
 assert(portable_mouse_next(&client,&frame)==PMC_BASELINE);
}
static void malformed(void){
 for(unsigned mode=0;mode<14;mode++){
  fixture();states[0]=state(1,5,1,0);state_count=1;opened();
  emit(RISC_USB_MOUSE_REPORT,3,state(1,5,2,0),2,1);
  switch(mode){
   case 0:event.sequence=0;break;
   case 1:event.state.session=9;break;
   case 2:event.state.device=0;break;
   case 3:event.state.reserved=1;break;
   case 4:event.reserved[6]=1;break;
   case 5:event.state.connected=2;break;
   case 6:event.state.interface_number=1;break;
   case 7:event.pressed=0;break;
   case 8:event.released=0;break;
   case 9:event.kind=4;break;
   case 10:event.kind=RISC_USB_MOUSE_DISCONNECTED;break;
   case 11:event.kind=RISC_USB_MOUSE_CONNECTED;break;
   case 12:client.filter=2;break; /* Simulate a bad filtered record. */
   case 13:client.sequence=3;break;
  }
  assert(portable_mouse_next(&client,&frame)==PMC_MALFORMED && !frame.event.kind && !frame.current_count);
  assert(frame.previous[0].buttons==1 && client.needs_snapshot);
 }
 for(unsigned mode=0;mode<6;mode++){
  fixture();states[0]=state(1,5,1,0);state_count=1;
  if(mode==0)snapshot_mode=2;
  if(mode==1)states[0].connected=0;
  if(mode==2)states[0].session=0;
  if(mode==3)states[0].reserved=1;
  if(mode==4){states[1]=state(2,5,0,0);state_count=2;}
  if(mode==5){states[1]=state(1,6,0,0);state_count=2;}
  assert(portable_mouse_open(&client,&frame)==PMC_MALFORMED && !client.count && client.needs_snapshot);
 }
 fixture();opened();emit(RISC_USB_MOUSE_CONNECTED,1,state(1,1,0,0),0,0);
 assert(portable_mouse_next(&client,&frame)==PMC_EVENT);
 emit(RISC_USB_MOUSE_REPORT,2,state(1,1,0,0),0,0);event.x=7;
 assert(portable_mouse_next(&client,&frame)==PMC_EVENT && frame.event.x==7);
 next_result=1;assert(portable_mouse_next(&client,&frame)==PMC_MALFORMED && !frame.event.x);
 assert(client.sequence==2);states[0]=state(1,1,0,0);state_count=1;
 assert(portable_mouse_next(&client,&frame)==PMC_BASELINE);
 next_result=1;assert(portable_mouse_next(&client,&frame)==PMC_MALFORMED); /* Replay across snapshot rejected. */
}
static void failures(void){
 fixture();acquire_mode=1;assert(portable_mouse_open(&client,&frame)==PMC_DENIED && !client.cleanup_required && !releases);
 fixture();acquire_mode=2;assert(portable_mouse_open(&client,&frame)==PMC_RETAINED && fences==1);terminal();
 fixture();acquire_mode=3;assert(portable_mouse_open(&client,&frame)==PMC_RETAINED);terminal();
 fixture();change_context=true;assert(portable_mouse_open(&client,&frame)==PMC_RETAINED && !fences && !subscriptions);terminal();
 fixture();acquire_mode=1;change_context=true;assert(portable_mouse_open(&client,&frame)==PMC_RETAINED && !fences);terminal();
 fixture();api.struct_size=8;assert(portable_mouse_open(&client,&frame)==PMC_UNSUPPORTED && releases==1 && !subscriptions);
 fixture();api.snapshot=NULL;assert(portable_mouse_open(&client,&frame)==PMC_UNSUPPORTED && releases==1);
 fixture();subscribe_ok=false;assert(portable_mouse_open(&client,&frame)==PMC_DENIED && releases==1 && !unsubscriptions);
 fixture();subscribe_ok=false;release_mode=1;assert(portable_mouse_open(&client,&frame)==PMC_RETAINED);terminal();
 fixture();opened();unsubscribe_ok=false;assert(portable_mouse_close(&client,&frame)==PMC_RETAINED);
 assert(client.subscription==57 && client.grant.api==&api && !releases && fences==1);terminal();
 for(int mode=1;mode<=2;mode++){
  fixture();opened();release_mode=mode;assert(portable_mouse_close(&client,&frame)==PMC_RETAINED);
  assert(!client.subscription && client.grant.api==&api && client.grant.slot==2 && unsubscriptions==1 && releases==1 && fences==1);terminal();
 }
 fixture();opened();context++;assert(portable_mouse_next(&client,&frame)==PMC_RETAINED && !fences && !reads);terminal();
 fixture();opened();getter_ok=false;assert(portable_mouse_close(&client,&frame)==PMC_RETAINED && !unsubscriptions && !releases);terminal();
 fixture();opened();next_result=-1;assert(portable_mouse_next(&client,&frame)==PMC_RETAINED && !unsubscriptions);terminal();
 fixture();opened();provider_status=RISC_USB_MOUSE_RETAINED;assert(portable_mouse_next(&client,&frame)==PMC_RETAINED);terminal();
 fixture();opened();provider_status=99;assert(portable_mouse_close(&client,&frame)==PMC_RETAINED && !unsubscriptions);terminal();
 fixture();opened();portable_mouse_consumer copy=client;unsigned before=external();
 assert(portable_mouse_next(&copy,&frame)==PMC_INVALID && before==external());
 assert(portable_mouse_next(&client,NULL)==PMC_INVALID && before==external());
 assert(portable_mouse_next(NULL,&frame)==PMC_INVALID);
 fixture();runtime.struct_size=RISC_RUNTIME_CAPABILITIES_V1_SIZE;
 assert(!portable_mouse_init(&client,&runtime,0,0) && client.result==PMC_UNSUPPORTED);
}
int main(void){normal();gaps_and_focus();malformed();failures();puts("mouse consumer: copied events, recovery, focus and custody PASS");}
