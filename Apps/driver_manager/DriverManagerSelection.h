#pragma once
#include "DriverManagerV1.h"
#include <string.h>
static inline bool driver_manager_text(const char*s,size_t capacity){
 return s && memchr(s,0,capacity)!=NULL;
}
static inline bool driver_manager_api_ready(const driver_manager_v1*a){
 return a&&a->api_version==DRIVER_MANAGER_API_VERSION&&a->struct_size>=DRIVER_MANAGER_V1_SIZE&&
 a->snapshot&&a->row_get&&a->refresh&&a->start&&a->step&&a->cancel&&a->network_ready;
}
static inline bool driver_manager_row_valid(const driver_manager_row_v1*r){
 return r&&r->token&&driver_manager_text(r->id,sizeof(r->id))&&
 driver_manager_text(r->version,sizeof(r->version))&&driver_manager_text(r->installed_version,sizeof(r->installed_version))&&
 driver_manager_text(r->label,sizeof(r->label))&&driver_manager_text(r->detail,sizeof(r->detail))&&
 !(r->flags & ~(uint32_t)(DRIVER_MANAGER_VALID|DRIVER_MANAGER_CAN_INSTALL|DRIVER_MANAGER_CAN_UPDATE|
 DRIVER_MANAGER_CAN_UNINSTALL|DRIVER_MANAGER_CAN_RETRY|DRIVER_MANAGER_CAN_DISCARD));
}
static inline bool driver_manager_snapshot_valid(const driver_manager_snapshot_v1*s){
 return s&&s->generation&&s->count<=DRIVER_MANAGER_ROWS_MAX&&s->source<=DRIVER_MANAGER_RECOVERY&&
 !(s->available_sources & ~7u)&&s->operation<=DRIVER_MANAGER_DISCARD&&
 (s->result==DRIVER_MANAGER_OK||s->result==DRIVER_MANAGER_WORKING||s->result==DRIVER_MANAGER_NEEDS_NETWORK||s->result==DRIVER_MANAGER_UNAVAILABLE||s->result==DRIVER_MANAGER_INVALID||s->result==DRIVER_MANAGER_RETAINED||s->result==DRIVER_MANAGER_STALE)&&s->custody<=DRIVER_MANAGER_CUSTODY_RETAINED&&driver_manager_text(s->status,sizeof(s->status));
}
/* A copied UI selection is an optimistic concurrency key, never authorization. */
static inline bool driver_manager_select(const driver_manager_snapshot_v1*s,uint32_t i,
 const driver_manager_row_v1*r,driver_manager_selection_v1*out){
 if(!out||!driver_manager_snapshot_valid(s)||!driver_manager_row_valid(r)||i>=s->count||
 !(s->available_sources&(1u<<s->source))||s->custody!=DRIVER_MANAGER_CUSTODY_NONE||(s->result==DRIVER_MANAGER_WORKING||s->result==DRIVER_MANAGER_RETAINED||s->result==DRIVER_MANAGER_NEEDS_NETWORK))return false;
 memset(out,0,sizeof(*out));out->generation=s->generation;out->source=s->source;out->index=i;out->token=r->token;
 memcpy(out->id,r->id,sizeof(out->id));memcpy(out->version,r->version,sizeof(out->version));return true;
}
static inline bool driver_manager_selection_matches(const driver_manager_snapshot_v1*s,
 const driver_manager_selection_v1*k,const driver_manager_row_v1*r){
 return k&&driver_manager_snapshot_valid(s)&&driver_manager_row_valid(r)&&
 driver_manager_text(k->id,sizeof(k->id))&&driver_manager_text(k->version,sizeof(k->version))&&
 k->generation==s->generation&&k->source==s->source&&k->index<s->count&&k->token==r->token&&
 !strcmp(k->id,r->id)&&!strcmp(k->version,r->version)&&
 (s->available_sources&(1u<<s->source))&&s->custody==DRIVER_MANAGER_CUSTODY_NONE&&s->result!=DRIVER_MANAGER_WORKING&&s->result!=DRIVER_MANAGER_RETAINED&&s->result!=DRIVER_MANAGER_NEEDS_NETWORK;
}
static inline bool driver_manager_action_allowed(uint32_t source,uint32_t op,const driver_manager_row_v1*r,bool confirmed){
 if(source>DRIVER_MANAGER_RECOVERY||!driver_manager_row_valid(r))return false;
 switch(op){
 case DRIVER_MANAGER_INSTALL:return source!=DRIVER_MANAGER_RECOVERY&&r->id[0]&&
 (r->flags&DRIVER_MANAGER_VALID)&&(r->flags&(DRIVER_MANAGER_CAN_INSTALL|DRIVER_MANAGER_CAN_UPDATE));
 case DRIVER_MANAGER_UNINSTALL:return source!=DRIVER_MANAGER_RECOVERY&&confirmed&&r->id[0]&&r->installed_version[0]&&
 (r->flags&DRIVER_MANAGER_CAN_UNINSTALL);
 case DRIVER_MANAGER_RETRY:return source==DRIVER_MANAGER_RECOVERY&&(r->flags&DRIVER_MANAGER_CAN_RETRY);
 case DRIVER_MANAGER_DISCARD:return source==DRIVER_MANAGER_RECOVERY&&confirmed&&(r->flags&DRIVER_MANAGER_CAN_DISCARD);
 default:return false;
 }
}
