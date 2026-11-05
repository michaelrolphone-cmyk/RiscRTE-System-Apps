#pragma once
#include "RiscRuntimeV1.h"
#include "PortableBroadcastClient.h"
/* Reusable foreground bridge: only borrowed synchronous provider calls. The
 * caller owns storage availability/revisions and passes a copied policy.
 * No hardware, surface, input, or product selection belongs here.
 * false is terminal custody uncertainty: retain the invocation and all grants.
 * Clean provider refusal is observable as unavailable, never claimed as parity. */
typedef struct {
 const risc_runtime_api_v1 *runtime;
 risc_runtime_capability_v1 grant;
 const telemetry_broadcast_v1 *api;
 bool (*safe)(void *);
 void *context;
 bool paused,unavailable;
} portable_foreground_broadcast;
static inline bool portable_foreground_broadcast_safe(portable_foreground_broadcast*c){
 return c&&c->runtime&&c->safe&&c->safe(c->context);
}
static inline void portable_foreground_broadcast_init(portable_foreground_broadcast*c,
 const risc_runtime_api_v1*rt,bool(*safe)(void*),void*context){
 memset(c,0,sizeof(*c));c->runtime=rt;c->safe=safe;c->context=context;
}
static inline bool portable_foreground_broadcast_open(portable_foreground_broadcast*c){
 if(!portable_foreground_broadcast_safe(c))return false;
 if(c->api||c->unavailable)return true;
 c->grant=(risc_runtime_capability_v1){.struct_size=sizeof(c->grant)};
 risc_runtime_capability_v1 empty=c->grant;
 bool ok=c->runtime->acquire(TELEMETRY_BROADCAST_CAPABILITY,1,0,&c->grant);
 if(!portable_foreground_broadcast_safe(c))return false;
 if(!ok){
  if(memcmp(&empty,&c->grant,sizeof(empty)))return false;
  c->unavailable=true;return true;
 }
 const telemetry_broadcast_v1*p=c->grant.api;
 if(c->grant.struct_size!=sizeof(c->grant)||!c->grant.slot||!c->grant.generation||
 !p||p->api_version!=1||p->struct_size<sizeof(*p)||!p->step||!p->pause||
 !p->status||!p->enumerate||!p->read)return false;
 c->api=p;return true;
}
static inline bool portable_foreground_broadcast_release(portable_foreground_broadcast*c){
 if(!portable_foreground_broadcast_safe(c))return false;
 if(c->grant.api){
  bool ok=c->runtime->release(&c->grant);
  if(!portable_foreground_broadcast_safe(c)||!ok||
   c->grant.api||c->grant.slot||c->grant.generation)return false;
 }
 c->api=NULL;return true;
}
static inline bool portable_foreground_broadcast_pause(portable_foreground_broadcast*c,bool owned){
 if(!portable_foreground_broadcast_safe(c))return false;
 /* Even a stop HCI operation is forbidden during native async ownership. */
 if(owned)return c->paused||c->unavailable;
 if(c->paused)return true;
 if(!portable_foreground_broadcast_open(c))return false;
 if(c->unavailable)return true;
 bool ok=c->api->pause(c->api->context);
 if(!portable_foreground_broadcast_safe(c)||!ok)return false;
 c->paused=true;return portable_foreground_broadcast_release(c);
}
static inline bool portable_foreground_broadcast_step(portable_foreground_broadcast*c,
 bool owned,bool allow,const telemetry_broadcast_policy_v1*policy){
 if(!portable_foreground_broadcast_safe(c))return false;
 if(owned)return c->paused||c->unavailable;
 if(!policy||policy->struct_size!=sizeof(*policy)||!portable_foreground_broadcast_open(c))return false;
 if(c->unavailable)return true;
 bool ok=c->api->step(c->api->context,allow,policy);
 if(!portable_foreground_broadcast_safe(c)||!ok)return false;
 c->paused=!allow;return portable_foreground_broadcast_release(c);
}
/* Called after a resident handoff: the other foreground client may have
 * restarted advertising while our last local pause receipt stayed true. */
static inline void portable_foreground_broadcast_invalidate(portable_foreground_broadcast*c){
 c->paused=false;c->unavailable=false;
}
