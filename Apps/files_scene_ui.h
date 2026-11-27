#pragma once
/* Checked semantic-scene custody. Application state and intent mapping stay
 * outside this client; providers retain all geometry and physical ownership. */
#include "RiscRuntimeV1.h"
#include "RiscSceneComponentsV1.h"
#include "RiscSceneLifecycleV1.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
typedef struct files_scene_ui {
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
} files_scene_ui;
static bool files_ui_live(files_scene_ui *u){
 if(u->terminal)return false;
 if(!u->alive||!u->alive(u->owner)){u->terminal=true;return false;}
 return true;
}
static bool files_ui_fail(files_scene_ui *u){
 if(!u->terminal){u->terminal=true;if(u->retain)u->retain(u->owner);}
 return false;
}
static bool files_ui_enter(files_scene_ui *u){
 if(!files_ui_live(u))return false;
 if(u->busy)return files_ui_fail(u);
 u->busy=true;return true;
}
static bool files_ui_leave(files_scene_ui *u){
 u->busy=false;return files_ui_live(u);
}
static bool files_ui_empty(const risc_runtime_capability_v1 *g){
 return !g->api&&!g->slot&&!g->generation&&
  (!g->struct_size||g->struct_size==sizeof(*g));
}
static void files_ui_init(files_scene_ui *u,const risc_runtime_api_v1 *rt,
 bool (*alive)(void *),void (*retain)(void *),void *owner){
 memset(u,0,sizeof(*u));u->runtime=rt;u->alive=alive;u->retain=retain;u->owner=owner;
}
/* 1 available, 0 clean optional absence, -1 retained. */
static int files_ui_bind(files_scene_ui *u){
 if(!files_ui_enter(u))return -1;
 if(!u->runtime||!u->runtime->acquire||!u->runtime->release){files_ui_fail(u);u->busy=false;return -1;}
 if(!files_ui_empty(&u->grant)){files_ui_fail(u);u->busy=false;return -1;}
 risc_runtime_capability_v1 g={.struct_size=sizeof(g)};
 bool ok=u->runtime->acquire(RISC_SCENE_CAPABILITY,1,0,&g);
 if(!files_ui_leave(u))return -1;
 if(g.struct_size!=sizeof(g)||(ok?(!g.api||!g.slot||!g.generation):!files_ui_empty(&g))){
  files_ui_fail(u);return -1;
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
static bool files_ui_snapshot(files_scene_ui *u){
 if(!files_ui_live(u))return false;
 if(!u->token){u->flags=0;return true;}
 if(!files_ui_enter(u))return false;
 risc_scene_navigation_v1 n={.api_version=1,.struct_size=sizeof(n)};
 uint32_t flags=0;int rc=u->api->snapshot(u->api->context,u->token,&n,&flags);
 if(!files_ui_leave(u))return false;
 if(rc!=RISC_SCENE_OK||n.api_version!=1||n.struct_size!=sizeof(n)||
 n.reserved||n.depth!=1||
 (flags&~(RISC_SCENE_PRESENTING|RISC_SCENE_CLOSING|RISC_SCENE_INPUT_BUSY|RISC_SCENE_ACTIVITY)))
 return files_ui_fail(u);
 for(uint32_t i=0;i<n.depth;i++)if(n.routes[i]!=u->committed.routes[i].id)return files_ui_fail(u);
 u->flags=flags;return true;
}
/* 1 committed; 0 transient/no commit; -1 terminal. Caller commits its copied
 * intent tuple only after 1, never from a later mutable domain snapshot. */
static int files_ui_publish(files_scene_ui *u,const risc_components_document_v1 *d,
 bool shared_controls){
 if(!files_ui_live(u))return -1;
 if(u->closing)return 0;
 if(u->token&&(!files_ui_snapshot(u)))return -1;
 if(u->flags&(RISC_SCENE_PRESENTING|RISC_SCENE_CLOSING|RISC_SCENE_INPUT_BUSY))return 0;
 if(!d||d->api_version!=1||d->struct_size!=sizeof(*d)||d->route_count!=1||!d->revision||
 (u->token&&d->revision<=u->committed.revision)){files_ui_fail(u);return -1;}
 if(!files_ui_enter(u))return -1;
 uint64_t opened=0;
 int rc=u->token?u->components->update(u->api->context,u->token,d):
 u->components->open(u->api->context,d,NULL,&opened);
 if(!files_ui_leave(u))return -1;
 if(rc==RISC_SCENE_AGAIN||rc==RISC_SCENE_BUSY){
  if(opened){files_ui_fail(u);return -1;}return 0;
 }
 if(rc!=RISC_SCENE_OK||(!u->token&&!opened)){files_ui_fail(u);return -1;}
 if(!u->token){u->token=opened;u->sequence=0;}
 u->committed=*d;
 uint32_t features=shared_controls?RISC_SCENE_FEATURE_SHARED_CONTROLS:0;
 if(u->lifecycle&&(!u->configured||features!=u->features)){
  if(!files_ui_enter(u))return -1;
  rc=u->lifecycle->configure(u->api->context,u->token,
   features);
  if(!files_ui_leave(u))return -1;
  if(rc!=RISC_SCENE_OK){files_ui_fail(u);return -1;}
  u->features=features;u->configured=true;
 }
 return 1;
}
/* 1 event, 0 no event, -1 terminal. Stale revisions are returned for the
 * application to discard, but their sequence is still consumed once. */
static int files_ui_next(files_scene_ui *u,risc_scene_event_v1 *out){
 if(!files_ui_live(u))return -1;
 if(!u->token||u->closing)return 0;
 if(!files_ui_enter(u))return -1;
 risc_scene_event_v1 e={.struct_size=sizeof(e)};
 int rc=u->api->next(u->api->context,u->token,&e);
 if(!files_ui_leave(u))return -1;
 if(rc==RISC_SCENE_IDLE){
  risc_scene_event_v1 empty={.struct_size=sizeof(empty)};
  if(memcmp(&e,&empty,sizeof(e))){files_ui_fail(u);return -1;}return 0;
 }
 if(rc!=RISC_SCENE_OK||e.struct_size!=sizeof(e)||!e.sequence||
 e.sequence<=u->sequence){files_ui_fail(u);return -1;}
 u->sequence=e.sequence;*out=e;return 1;
}
static bool files_ui_close(files_scene_ui *u){
 if(!files_ui_live(u))return false;
 if(!u->token){u->closing=false;return true;}
 u->closing=true;
 if(!files_ui_enter(u))return false;
 int rc=u->api->close(u->api->context,u->token);
 if(!files_ui_leave(u))return false;
 if(rc==RISC_SCENE_AGAIN)return false;
 if(rc!=RISC_SCENE_OK)return files_ui_fail(u);
 u->token=u->sequence=0;u->flags=u->features=0;u->closing=u->configured=false;
 memset(&u->committed,0,sizeof(u->committed));return true;
}
static bool files_ui_release(files_scene_ui *u){
 if(!files_ui_live(u)||!files_ui_close(u))return false;
 if(files_ui_empty(&u->grant))return true;
 if(!u->grant.api||!u->grant.slot||!u->grant.generation||
 u->grant.struct_size!=sizeof(u->grant))return files_ui_fail(u);
 if(!files_ui_enter(u))return false;
 risc_runtime_capability_v1 g=u->grant;
 bool ok=u->runtime->release(&g);
 if(!files_ui_leave(u))return false;
 if(!ok||g.struct_size!=sizeof(g)||!files_ui_empty(&g))return files_ui_fail(u);
 u->grant=g;u->api=NULL;u->components=NULL;u->lifecycle=NULL;return true;
}
