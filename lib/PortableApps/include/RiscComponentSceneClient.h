#pragma once
/* Checked semantic-scene custody. Application state and intent mapping stay
 * outside this client; providers retain all geometry and physical ownership. */
#include "RiscRuntimeV1.h"
#include "RiscSceneComponentsV1.h"
#include "RiscSceneLifecycleV1.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
typedef struct risc_component_scene_client {
 const risc_runtime_api_v1 *runtime;
 bool (*alive)(void *);
 void (*retain)(void *);
 void *owner;
 risc_runtime_capability_v1 grant;
 const risc_scene_api_v1 *api;
 const risc_scene_components_api_v1 *components;
 const risc_scene_lifecycle_api_v1 *lifecycle;
 uint64_t token,sequence;
 uint32_t flags,features;
 risc_components_document_v1 committed;
 bool closing,busy,terminal,configured;
 /* Snapshot consumes provider ACTIVITY. Caller clears this sticky latch only
  * after transferring it to its lifecycle/activity owner. */
 bool activity;
} risc_component_scene_client;
static inline bool risc_component_scene_live(risc_component_scene_client *u){
 if(u->terminal)return false;
 if(!u->alive||!u->alive(u->owner)){u->terminal=true;return false;}
 return true;
}
static inline bool risc_component_scene_fail(risc_component_scene_client *u){
 if(!u->terminal){u->terminal=true;if(u->retain)u->retain(u->owner);}
 return false;
}
static inline bool risc_component_scene_enter(risc_component_scene_client *u){
 if(!risc_component_scene_live(u))return false;
 if(u->busy)return risc_component_scene_fail(u);
 u->busy=true;return true;
}
static inline bool risc_component_scene_leave(risc_component_scene_client *u){
 u->busy=false;return risc_component_scene_live(u);
}
static inline bool risc_component_scene_empty(const risc_runtime_capability_v1 *g){
 return !g->api&&!g->slot&&!g->generation&&
  (!g->struct_size||g->struct_size==sizeof(*g));
}
static inline void risc_component_scene_init(risc_component_scene_client *u,const risc_runtime_api_v1 *rt,
 bool (*alive)(void *),void (*retain)(void *),void *owner){
 memset(u,0,sizeof(*u));u->runtime=rt;u->alive=alive;u->retain=retain;u->owner=owner;
}
/* 1 available, 0 clean optional absence, -1 retained. */
static inline int risc_component_scene_bind(risc_component_scene_client *u){
 if(!risc_component_scene_enter(u))return -1;
 if(!u->runtime||!u->runtime->acquire||!u->runtime->release){risc_component_scene_fail(u);u->busy=false;return -1;}
 if(!risc_component_scene_empty(&u->grant)){risc_component_scene_fail(u);u->busy=false;return -1;}
 risc_runtime_capability_v1 g={.struct_size=sizeof(g)};
 bool ok=u->runtime->acquire(RISC_SCENE_CAPABILITY,1,0,&g);
 if(!risc_component_scene_leave(u))return -1;
 if(g.struct_size!=sizeof(g)||(ok?(!g.api||!g.slot||!g.generation):!risc_component_scene_empty(&g))){
  risc_component_scene_fail(u);return -1;
 }
 if(!ok)return 0;
 u->grant=g;u->api=g.api;u->components=risc_scene_components_get_v1(u->api);
 u->lifecycle=risc_scene_lifecycle_get_v1(u->api);
 if(!u->components||!u->api->snapshot||!u->api->next||!u->api->close){
  /* A live grant is retained until the root explicitly releases it. */
  return 0;
 }
 return 1;
}
static inline bool risc_component_scene_snapshot(risc_component_scene_client *u){
 if(!risc_component_scene_live(u))return false;
 if(!u->token){u->flags=0;return true;}
 if(!risc_component_scene_enter(u))return false;
 risc_scene_navigation_v1 n={.api_version=1,.struct_size=sizeof(n)};
 uint32_t flags=0;int rc=u->api->snapshot(u->api->context,u->token,&n,&flags);
 if(!risc_component_scene_leave(u))return false;
 if(rc!=RISC_SCENE_OK||n.api_version!=1||n.struct_size!=sizeof(n)||
 n.reserved||n.depth!=1||
 (flags&~(RISC_SCENE_PRESENTING|RISC_SCENE_CLOSING|RISC_SCENE_INPUT_BUSY|RISC_SCENE_ACTIVITY)))
 return risc_component_scene_fail(u);
 for(uint32_t i=0;i<n.depth;i++)if(n.routes[i]!=u->committed.routes[i].id)return risc_component_scene_fail(u);
 if(flags&RISC_SCENE_ACTIVITY)u->activity=true;
 u->flags=flags;return true;
}
/* A successful update copies a document; it does not make it visible. A
 * mutating controller must wait for this exact revision to settle, after
 * draining semantic events that could cancel the pending explicit action.
 * 1 ready, 0 not yet ready or obsolete revision, -1 terminal custody. */
static inline int risc_component_scene_presented(risc_component_scene_client *u,uint32_t revision){
 if(!risc_component_scene_live(u))return -1;
 if(!revision||!u->token||u->closing||u->committed.revision!=revision)return 0;
 if(!risc_component_scene_snapshot(u))return -1;
 return !(u->flags&(RISC_SCENE_PRESENTING|RISC_SCENE_CLOSING|RISC_SCENE_INPUT_BUSY));
}
/* 1 committed; 0 transient/no commit; -1 terminal. Caller commits its copied
 * intent tuple only after 1, never from a later mutable domain snapshot. */
static inline int risc_component_scene_publish(risc_component_scene_client *u,const risc_components_document_v1 *d,
 bool shared_controls){
 if(!risc_component_scene_live(u))return -1;
 if(u->closing)return 0;
 if(u->token&&(!risc_component_scene_snapshot(u)))return -1;
 if(u->flags&(RISC_SCENE_PRESENTING|RISC_SCENE_CLOSING|RISC_SCENE_INPUT_BUSY))return 0;
 if(!d||d->api_version!=1||d->struct_size!=sizeof(*d)||d->route_count!=1||!d->revision||
 (u->token&&d->revision<=u->committed.revision)){risc_component_scene_fail(u);return -1;}
 if(!risc_component_scene_enter(u))return -1;
 uint64_t opened=0;
 int rc=u->token?u->components->update(u->api->context,u->token,d):
 u->components->open(u->api->context,d,NULL,&opened);
 if(!risc_component_scene_leave(u))return -1;
 if(rc==RISC_SCENE_AGAIN||rc==RISC_SCENE_BUSY){
  if(opened){risc_component_scene_fail(u);return -1;}return 0;
 }
 if(rc!=RISC_SCENE_OK||(!u->token&&!opened)){risc_component_scene_fail(u);return -1;}
 if(!u->token){u->token=opened;u->sequence=0;}
 u->committed=*d;
 uint32_t features=shared_controls?RISC_SCENE_FEATURE_SHARED_CONTROLS:0;
 if(u->lifecycle&&(!u->configured||features!=u->features)){
  if(!risc_component_scene_enter(u))return -1;
  rc=u->lifecycle->configure(u->api->context,u->token,
   features);
  if(!risc_component_scene_leave(u))return -1;
  if(rc!=RISC_SCENE_OK){risc_component_scene_fail(u);return -1;}
  u->features=features;u->configured=true;
 }
 return 1;
}
/* 1 event, 0 no event, -1 terminal. Stale revisions are returned for the
 * application to discard, but their sequence is still consumed once. */
static inline int risc_component_scene_next(risc_component_scene_client *u,risc_scene_event_v1 *out){
 if(!risc_component_scene_live(u))return -1;
 if(!u->token||u->closing)return 0;
 if(!risc_component_scene_enter(u))return -1;
 risc_scene_event_v1 e={.struct_size=sizeof(e)};
 int rc=u->api->next(u->api->context,u->token,&e);
 if(!risc_component_scene_leave(u))return -1;
 if(rc==RISC_SCENE_IDLE){
  risc_scene_event_v1 empty={.struct_size=sizeof(empty)};
  if(memcmp(&e,&empty,sizeof(e))){risc_component_scene_fail(u);return -1;}return 0;
 }
 if(rc!=RISC_SCENE_OK||e.struct_size!=sizeof(e)||!e.sequence||
 e.sequence<=u->sequence){risc_component_scene_fail(u);return -1;}
 u->sequence=e.sequence;*out=e;return 1;
}
static inline bool risc_component_scene_close(risc_component_scene_client *u){
 if(!risc_component_scene_live(u))return false;
 if(!u->token){u->closing=false;return true;}
 u->closing=true;
 if(!risc_component_scene_enter(u))return false;
 int rc=u->api->close(u->api->context,u->token);
 if(!risc_component_scene_leave(u))return false;
 if(rc==RISC_SCENE_AGAIN)return false;
 if(rc!=RISC_SCENE_OK)return risc_component_scene_fail(u);
 u->token=u->sequence=0;u->flags=u->features=0;u->closing=u->configured=false;
 memset(&u->committed,0,sizeof(u->committed));return true;
}
static inline bool risc_component_scene_release(risc_component_scene_client *u){
 if(!risc_component_scene_live(u)||!risc_component_scene_close(u))return false;
 if(risc_component_scene_empty(&u->grant))return true;
 if(!u->grant.api||!u->grant.slot||!u->grant.generation||
 u->grant.struct_size!=sizeof(u->grant))return risc_component_scene_fail(u);
 if(!risc_component_scene_enter(u))return false;
 risc_runtime_capability_v1 g=u->grant;
 bool ok=u->runtime->release(&g);
 if(!risc_component_scene_leave(u))return false;
 if(!ok||g.struct_size!=sizeof(g)||!risc_component_scene_empty(&g))return risc_component_scene_fail(u);
 u->grant=g;u->api=NULL;u->components=NULL;u->lifecycle=NULL;return true;
}
