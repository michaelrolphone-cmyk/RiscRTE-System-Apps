#include "PortableMouseConsumer.h"
#include <string.h>

static int result(portable_mouse_consumer *c, int code) {
 if(c)c->result=code;
 return code;
}
static void frame_begin(const portable_mouse_consumer *c, portable_mouse_frame *out) {
 memset(out,0,sizeof(*out));
 if(c && c->self==c && c->initialized) {
  out->previous_count=out->current_count=c->count;
  memcpy(out->previous,c->states,sizeof(c->states));
  memcpy(out->current,c->states,sizeof(c->states));
 }
}
static int clear(portable_mouse_consumer *c, portable_mouse_frame *out, int code) {
 memset(c->states,0,sizeof(c->states));c->count=0;c->needs_snapshot=true;
 memset(out->current,0,sizeof(out->current));out->current_count=0;
 return result(c,code);
}
static int retain(portable_mouse_consumer *c, portable_mouse_frame *out, bool owner) {
 c->cleanup_required=true;c->focused=false;
 clear(c,out,PMC_RETAINED);
 if(owner)c->fence_accepted=c->runtime->retain_invocation();
 return PMC_RETAINED;
}
static bool tag(const risc_runtime_api_v1 *rt, uint64_t *context) {
 risc_stream_client_v1 now={.struct_size=sizeof(now)};
 if(!rt->stream_client(&now) || now.api_version!=RISC_STREAM_CLIENT_API_V1 ||
    now.struct_size<sizeof(now) || !now.context)return false;
 *context=now.context;return true;
}
static bool current(portable_mouse_consumer *c, portable_mouse_frame *out) {
 if(!out){result(c,PMC_INVALID);return false;}
 frame_begin(c,out);
 if(!c || c->self!=c || !c->initialized) {
  result(c,PMC_INVALID);return false;
 }
 if(c->cleanup_required){result(c,PMC_RETAINED);return false;}
 uint64_t now=0;
 if(!tag(c->runtime,&now) || now!=c->invocation) {
  retain(c,out,false);return false;
 }
 return true;
}
static bool grant_empty(const risc_runtime_capability_v1 *g) {
 return g->struct_size==sizeof(*g) && !g->slot && !g->generation && !g->api;
}
static bool release(portable_mouse_consumer *c, portable_mouse_frame *out) {
 risc_runtime_capability_v1 copy=c->grant;
 if(!c->runtime->release(&copy) || !grant_empty(&copy)) {
  retain(c,out,true);return false;
 }
 c->grant=copy;c->mouse=NULL;return true;
}
static bool state_valid(const portable_mouse_consumer *c, const risc_usb_mouse_state_v1 *s) {
 return s->device && s->session && !s->reserved && s->connected==1 &&
        (!c->filter || s->device==c->filter);
}
static bool same(const risc_usb_mouse_state_v1 *a, const risc_usb_mouse_state_v1 *b) {
 return a->device==b->device && a->session==b->session;
}
static bool interface_same(const risc_usb_mouse_state_v1 *a, const risc_usb_mouse_state_v1 *b) {
 return a->device==b->device && a->interface_number==b->interface_number && a->alternate==b->alternate;
}
static int baseline(portable_mouse_consumer *c, portable_mouse_frame *out) {
 risc_usb_mouse_state_v1 states[RISC_USB_MOUSE_MAX_DEVICES]={0};
 size_t n=RISC_USB_MOUSE_MAX_DEVICES;
 if(!c->mouse->snapshot(c->mouse->context,c->subscription,states,&n))
  return clear(c,out,PMC_GAP); /* No retry, no partial state, GAP remains. */
 if(n>RISC_USB_MOUSE_MAX_DEVICES)return clear(c,out,PMC_MALFORMED);
 for(size_t i=0;i<n;i++) {
  if(!state_valid(c,&states[i]))return clear(c,out,PMC_MALFORMED);
  for(size_t j=0;j<i;j++)
   if(states[i].session==states[j].session || interface_same(&states[i],&states[j]))
    return clear(c,out,PMC_MALFORMED);
 }
 memcpy(c->states,states,sizeof(states));c->count=n;c->needs_snapshot=false;
 memcpy(out->current,states,sizeof(states));out->current_count=n;
 return result(c,PMC_BASELINE);
}
static int phase(portable_mouse_consumer *c, portable_mouse_frame *out) {
 uint32_t state=c->mouse->status(c->mouse->context);
 if(state==RISC_USB_MOUSE_RETAINED)return retain(c,out,true);
 if(state==RISC_USB_MOUSE_RUNNING)return PMC_EMPTY;
 if(state==RISC_USB_MOUSE_CLOSING || state==RISC_USB_MOUSE_STOPPED)
  return clear(c,out,PMC_UNAVAILABLE);
 return retain(c,out,true);
}
bool portable_mouse_init(portable_mouse_consumer *c, const risc_runtime_api_v1 *rt,
                         uint64_t instance, uint64_t filter) {
 if(!c)return false;
 memset(c,0,sizeof(*c));c->self=c;c->instance=instance;c->filter=filter;
 c->grant.struct_size=sizeof(c->grant);
#ifndef PORTABLE_USB_MOUSE
 (void)rt;result(c,PMC_UNSUPPORTED);return false;
#else
 if(!rt || rt->api_version!=RISC_RUNTIME_API_V1 ||
    rt->struct_size<RISC_RUNTIME_STREAM_CLIENT_V1_SIZE || !rt->acquire ||
    !rt->release || !rt->retain_invocation || !rt->stream_client || !tag(rt,&c->invocation)) {
  result(c,PMC_UNSUPPORTED);return false;
 }
 c->runtime=rt;c->initialized=true;return true;
#endif
}
int portable_mouse_open(portable_mouse_consumer *c, portable_mouse_frame *out) {
 if(!current(c,out))return c?c->result:PMC_INVALID;
 if(c->subscription || c->grant.api)return result(c,PMC_INVALID);
 risc_runtime_capability_v1 grant={.struct_size=sizeof(grant)};
 bool ok=c->runtime->acquire("usb.hid.mouse",RISC_USB_MOUSE_API_V1,c->instance,&grant);
 c->grant=grant;
 /* Check both successful and failed acquisition before borrowed access or a
  * retention fence: neither operation may affect a newer invocation. */
 if(!current(c,out))return c->result;
 if(!ok) {
  if(!grant_empty(&grant))return retain(c,out,true);
  return result(c,PMC_DENIED);
 }
 if(grant.struct_size!=sizeof(grant) || !grant.slot || !grant.generation || !grant.api)
  return retain(c,out,true);
 const risc_usb_mouse_api_v1 *api=grant.api;
 uint32_t header[2];memcpy(header,api,sizeof(header));
 if(header[0]!=RISC_USB_MOUSE_API_V1 || header[1]<sizeof(*api) ||
    !api->subscribe || !api->unsubscribe || !api->poll || !api->next || !api->snapshot || !api->status) {
  if(!release(c,out))return c->result;
  return result(c,PMC_UNSUPPORTED);
 }
 c->mouse=api;
 if(phase(c,out)!=PMC_EMPTY)return c->result;
 c->subscription=api->subscribe(api->context,c->filter);
 if(!c->subscription) {
  if(!release(c,out))return c->result;
  return result(c,PMC_DENIED);
 }
 c->focused=true;c->needs_snapshot=true;c->sequence=0;
 return baseline(c,out);
}
int portable_mouse_next(portable_mouse_consumer *c, portable_mouse_frame *out) {
 if(!current(c,out))return c?c->result:PMC_INVALID;
 if(!c->subscription)return result(c,PMC_CLOSED);
 if(!c->focused)return result(c,PMC_PAUSED);
 if(phase(c,out)!=PMC_EMPTY)return c->result;
 if(c->needs_snapshot)return baseline(c,out);
 risc_usb_mouse_event_v1 event={0};
 int32_t rc=c->mouse->next(c->mouse->context,c->subscription,&event);
 if(rc==0)return result(c,PMC_EMPTY);
 if(rc==-2)return clear(c,out,PMC_GAP); /* Snapshot on the next bounded step. */
 if(rc!=1)return retain(c,out,true);
 const risc_usb_mouse_state_v1 *s=&event.state;
 bool malformed=!event.sequence || event.sequence<=c->sequence || !s->device ||
   !s->session || s->reserved || (c->filter && s->device!=c->filter);
 for(size_t i=0;i<sizeof(event.reserved);i++)malformed|=event.reserved[i]!=0;
 size_t index=0;while(index<c->count && !same(s,&c->states[index]))++index;
 if(event.kind==RISC_USB_MOUSE_CONNECTED) {
  malformed|=index!=c->count || c->count==RISC_USB_MOUSE_MAX_DEVICES ||
    !state_valid(c,s) || s->buttons || event.pressed || event.released ||
    event.x || event.y || event.wheel || event.pan;
  for(size_t i=0;i<c->count;i++)
   malformed|=s->session==c->states[i].session || interface_same(s,&c->states[i]);
 } else if(event.kind==RISC_USB_MOUSE_REPORT || event.kind==RISC_USB_MOUSE_DISCONNECTED) {
  if(index==c->count)malformed=true;
  else {
   uint32_t old=c->states[index].buttons;
   malformed|=!interface_same(s,&c->states[index]) ||
     event.pressed!=(s->buttons & ~old) || event.released!=(old & ~s->buttons);
  }
  if(event.kind==RISC_USB_MOUSE_REPORT)malformed|=!state_valid(c,s);
  else malformed|=s->connected || s->buttons || event.pressed ||
      event.x || event.y || event.wheel || event.pan;
 } else malformed=true;
 if(malformed)return clear(c,out,PMC_MALFORMED);
 if(event.kind==RISC_USB_MOUSE_DISCONNECTED) {
  for(size_t i=index+1;i<c->count;i++)c->states[i-1]=c->states[i];
  c->states[--c->count]=(risc_usb_mouse_state_v1){0};
 } else {
  c->states[index]=*s;if(index==c->count)++c->count;
 }
 c->sequence=event.sequence;out->event=event;
 memcpy(out->current,c->states,sizeof(c->states));out->current_count=c->count;
 return result(c,PMC_EVENT);
}
int portable_mouse_focus(portable_mouse_consumer *c, bool focused, portable_mouse_frame *out) {
 if(!current(c,out))return c?c->result:PMC_INVALID;
 if(!c->subscription)return result(c,PMC_CLOSED);
 if(c->focused==focused)return result(c,focused?PMC_EMPTY:PMC_PAUSED);
 c->focused=focused;
 return clear(c,out,PMC_RESET);
}
int portable_mouse_close(portable_mouse_consumer *c, portable_mouse_frame *out) {
 if(!current(c,out))return c?c->result:PMC_INVALID;
 c->focused=false;clear(c,out,PMC_RESET);
 if(!c->grant.api)return result(c,PMC_CLOSED);
 if(c->mouse) {
  uint32_t state=c->mouse->status(c->mouse->context);
  if(state==RISC_USB_MOUSE_RETAINED || state>RISC_USB_MOUSE_RETAINED)
   return retain(c,out,true);
 }
 if(c->subscription) {
  if(!c->mouse->unsubscribe(c->mouse->context,c->subscription))return retain(c,out,true);
  c->subscription=0;
 }
 if(!release(c,out))return c->result;
 return result(c,PMC_CLOSED);
}
