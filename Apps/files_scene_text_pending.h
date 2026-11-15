#pragma once
/* Queued Files text entry. No provider work until the owner drains its scene.
 * snapshot is copied-state only; drain/open own the existing checked clients.
 * On every mount teardown/replacement call cancel(), even if a pointer/index
 * could later be reused. No published dedicated mount epoch is assumed. */
#include "RiscRuntimeV1.h"
#include "RiscStorageVolumeV1.h"
#include "RiscTextEntryV1.h"
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#define FILES_TEXT_PATH_CAP 512u
#define FILES_TEXT_DRAFT_CAP 128u
typedef struct files_text_identity {
 unsigned mode,choice,storage;
 uint64_t mount_epoch;
 const risc_storage_volume_api_v1 *volume;
 uint32_t grant_slot,grant_generation;
 risc_storage_dirent_v1 selected;
 char path[FILES_TEXT_PATH_CAP],file_path[FILES_TEXT_PATH_CAP];
 bool mounted,new_directory;
 const void *peer;
 uint32_t pairing_generation,pairing_number,operation_generation,wait_generation;
} files_text_identity;
typedef struct files_text_intent {
 files_text_identity identity;
 unsigned requested_mode;
 uint32_t capacity;
 char label[RISC_TEXT_ENTRY_LABEL],draft[FILES_TEXT_DRAFT_CAP];
} files_text_intent;
typedef struct files_text_pending {
 void *context;
 bool (*alive)(void *);
 void (*retain)(void *);
 bool (*snapshot)(void *,files_text_identity *);
 /* drain: 1 closed, 0 checked AGAIN, -1 retained; no other result valid.
  * open: 1 opened, 0 clean original begin failure, -1 retained. Called ONCE.
  * open must use captured return mode/choice and draft, not mutable globals. */
 int (*drain)(void *);
 int (*open)(void *,const files_text_intent *);
 files_text_intent intent;
 bool pending,cancelled,busy,terminal;
} files_text_pending;
enum {FILES_TEXT_WAIT=0,FILES_TEXT_OPENED=1,FILES_TEXT_CANCELLED=2,
 FILES_TEXT_UNAVAILABLE=3,FILES_TEXT_RETAINED=-1};
static bool files_text_pending_live(files_text_pending *p){
 if(p->terminal)return false;
 if(!p->alive||!p->alive(p->context)){p->terminal=true;return false;}
 return true;
}
static bool files_text_pending_fail(files_text_pending *p){
 if(!p->terminal){p->terminal=true;if(p->retain)p->retain(p->context);}
 return false;
}
static bool files_text_pending_enter(files_text_pending *p){
 if(!files_text_pending_live(p))return false;
 if(p->busy)return files_text_pending_fail(p);
 p->busy=true;return true;
}
/* Fresh invocation only. Never use init to discard pending/native custody. */
static void files_text_pending_init(files_text_pending *p,void *context,
 bool(*alive)(void *),void(*retain)(void *),
 bool(*snapshot)(void *,files_text_identity *),int(*drain)(void *),
 int(*open)(void *,const files_text_intent *)){
 memset(p,0,sizeof(*p));p->context=context;p->alive=alive;p->retain=retain;
 p->snapshot=snapshot;p->drain=drain;p->open=open;
}
static bool files_text_copy(char *out,size_t n,const char *in){
 if(!in||!n)return false;
 size_t i=0;for(;i<n;i++){out[i]=in[i];if(!in[i])return true;}
 return false;
}
static bool files_text_identity_valid(const files_text_identity *i){
 return memchr(i->path,0,sizeof(i->path))&&memchr(i->file_path,0,sizeof(i->file_path))&&
 memchr(i->selected.name,0,sizeof(i->selected.name))&&i->selected.is_directory<=1;
}
static bool files_text_identity_equal(const files_text_identity *a,const files_text_identity *b){
 return a->mount_epoch==b->mount_epoch&&a->mode==b->mode&&a->choice==b->choice&&a->storage==b->storage&&
 a->volume==b->volume&&a->grant_slot==b->grant_slot&&a->grant_generation==b->grant_generation&&
 a->selected.is_directory==b->selected.is_directory&&a->selected.size==b->selected.size&&
 !strcmp(a->selected.name,b->selected.name)&&!strcmp(a->path,b->path)&&!strcmp(a->file_path,b->file_path)&&
 a->mounted==b->mounted&&a->new_directory==b->new_directory&&a->peer==b->peer&&
 a->pairing_generation==b->pairing_generation&&a->pairing_number==b->pairing_number&&
 a->operation_generation==b->operation_generation&&a->wait_generation==b->wait_generation;
}
/* true means QUEUED, not yet opened. This preserves new-folder caller intent.
 * A second request never overwrites the first, including while drain is AGAIN. */
static bool files_text_pending_request(files_text_pending *p,unsigned mode,
 const char *label,const char *draft,uint32_t capacity){
 if(!files_text_pending_enter(p))return false;
 if(p->pending){p->busy=false;return false;}
 if(!p->snapshot||!p->drain||!p->open||!capacity||capacity>FILES_TEXT_DRAFT_CAP){
  p->busy=false;return false;
 }
 files_text_intent next={0};next.requested_mode=mode;next.capacity=capacity;
 if(!files_text_copy(next.label,sizeof(next.label),label)||
 !files_text_copy(next.draft,capacity,draft)){p->busy=false;return false;}
 bool ok=p->snapshot(p->context,&next.identity);
 if(!files_text_pending_live(p)){p->busy=false;return false;}
 if(!ok||!files_text_identity_valid(&next.identity)){p->busy=false;return false;}
 p->intent=next;p->pending=true;p->cancelled=false;p->busy=false;return true;
}
static void files_text_pending_cancel(files_text_pending *p){
 if(!files_text_pending_live(p))return;
 /* Local cancellation is legal from a mount invalidation inside drain. */
 if(p->pending)p->cancelled=true;
}
static bool files_text_pending_current(files_text_pending *p){
 files_text_identity now={0};
 bool ok=p->snapshot(p->context,&now);
 if(!files_text_pending_live(p))return false;
 return ok&&files_text_identity_valid(&now)&&files_text_identity_equal(&now,&p->intent.identity);
}
static int files_text_pending_step(files_text_pending *p){
 if(!files_text_pending_enter(p))return FILES_TEXT_RETAINED;
 if(!p->pending){p->busy=false;return FILES_TEXT_WAIT;}
 if(!p->cancelled&&!files_text_pending_current(p))p->cancelled=true;
 if(!files_text_pending_live(p)){p->busy=false;return FILES_TEXT_RETAINED;}
 int rc=p->drain(p->context);
 if(!files_text_pending_live(p)){p->busy=false;return FILES_TEXT_RETAINED;}
 if(rc==0){p->busy=false;return FILES_TEXT_WAIT;}
 if(rc!=1){files_text_pending_fail(p);p->busy=false;return FILES_TEXT_RETAINED;}
 /* A drain callback may progress/cancel a remote mount. Recheck after it. */
 if(!p->cancelled&&!files_text_pending_current(p))p->cancelled=true;
 if(!files_text_pending_live(p)){p->busy=false;return FILES_TEXT_RETAINED;}
 if(p->cancelled){
  p->pending=false;memset(&p->intent,0,sizeof(p->intent));
  p->busy=false;return FILES_TEXT_CANCELLED;
 }
 rc=p->open(p->context,&p->intent);
 if(!files_text_pending_live(p)){p->busy=false;return FILES_TEXT_RETAINED;}
 if(rc!=0&&rc!=1){files_text_pending_fail(p);p->busy=false;return FILES_TEXT_RETAINED;}
 p->pending=false;memset(&p->intent,0,sizeof(p->intent));p->busy=false;
 return rc==1?FILES_TEXT_OPENED:FILES_TEXT_UNAVAILABLE;
}
