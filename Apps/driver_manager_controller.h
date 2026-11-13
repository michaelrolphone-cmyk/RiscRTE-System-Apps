#pragma once
#include "driver_manager_presenter.h"
#include "driver_manager/DriverManagerSelection.h"
/* Pure copied-model controller. Provider/Runtime call ordering lives in the
 * scene owner; no stored provider pointer or authority is hidden here. */
typedef struct {
 dm_view view;
 driver_manager_snapshot_v1 snapshot;
 driver_manager_row_v1 rows[DM_ROWS_MAX];
 driver_manager_selection_v1 selection;
 bool selected, terminal;
} dm_controller;
static inline void dm_controller_init(dm_controller*c){
 memset(c,0,sizeof(*c));c->view.selected=-1;
 snprintf(c->view.status,sizeof(c->view.status),"Driver package provider unavailable.");
}
static inline bool dm_controller_copy(dm_controller*c,const driver_manager_snapshot_v1*s,
 const driver_manager_row_v1*rows){
 if(c->terminal||!driver_manager_snapshot_valid(s)||(!rows&&s->count))return false;
 for(unsigned i=0;i<s->count;i++)if(!driver_manager_row_valid(&rows[i]))return false;
 bool keep=c->selected&&c->selection.index<s->count&&
 driver_manager_selection_matches(s,&c->selection,&rows[c->selection.index]);
 c->snapshot=*s;c->view.source=s->source;c->view.generation=s->generation;
 c->view.count=s->count;c->view.available_sources=s->available_sources;
 c->view.busy=s->result==DRIVER_MANAGER_WORKING||s->result==DRIVER_MANAGER_NEEDS_NETWORK||s->custody!=DRIVER_MANAGER_CUSTODY_NONE;
 c->terminal=s->result==DRIVER_MANAGER_RETAINED||s->custody==DRIVER_MANAGER_CUSTODY_RETAINED;
 memcpy(c->view.status,s->status,sizeof(c->view.status));
 for(unsigned i=0;i<s->count;i++){
  c->rows[i]=rows[i];dm_row*r=&c->view.rows[i];memset(r,0,sizeof(*r));
  memcpy(r->id,rows[i].id,sizeof(r->id));memcpy(r->version,rows[i].version,sizeof(r->version));
  memcpy(r->installed_version,rows[i].installed_version,sizeof(r->installed_version));
  memcpy(r->label,rows[i].label,sizeof(r->label));memcpy(r->detail,rows[i].detail,sizeof(r->detail));
  uint32_t f=rows[i].flags;
  r->flags=((f&DRIVER_MANAGER_VALID)?DM_VALID:0)|
   ((f&(DRIVER_MANAGER_CAN_INSTALL|DRIVER_MANAGER_CAN_UPDATE))?DM_INSTALLABLE:0)|
   (rows[i].installed_version[0]?DM_INSTALLED:0)|((f&DRIVER_MANAGER_CAN_UPDATE)?DM_UPDATE:0)|
   ((f&DRIVER_MANAGER_CAN_UNINSTALL)?DM_REMOVABLE:0)|
   ((f&DRIVER_MANAGER_CAN_RETRY)?DM_RETRYABLE:0)|((f&DRIVER_MANAGER_CAN_DISCARD)?DM_DISCARDABLE:0);
 }
 if(!keep){c->selected=false;c->view.selected=-1;c->view.confirming=0;}
 return true;
}
static inline uint32_t dm_provider_operation(unsigned action){
 switch(action){case DM_INSTALL:return DRIVER_MANAGER_INSTALL;case DM_UNINSTALL:return DRIVER_MANAGER_UNINSTALL;
 case DM_RETRY:return DRIVER_MANAGER_RETRY;case DM_DISCARD:return DRIVER_MANAGER_DISCARD;default:return 0;}
}
static inline bool dm_controller_select(dm_controller*c,unsigned index){
 if(c->terminal||index>=c->snapshot.count||!driver_manager_select(&c->snapshot,index,&c->rows[index],&c->selection))return false;
 c->selected=true;c->view.selected=(int32_t)index;c->view.confirming=0;return true;
}
static inline bool dm_controller_prepare(dm_controller*c,unsigned action){
 if(!c->selected||c->terminal||!dm_operation_allowed(&c->view,action)||
 !driver_manager_selection_matches(&c->snapshot,&c->selection,&c->rows[c->selection.index]))return false;
 c->view.confirming=action;return true;
}
/* Re-read snapshot and selected row immediately before start. Never replace a
 * stale confirmation with a newer selection carrying the same display text. */
static inline bool dm_controller_confirm(dm_controller*c,const driver_manager_snapshot_v1*fresh,
 const driver_manager_row_v1*row,uint32_t*operation,driver_manager_selection_v1*out){
 uint32_t op=dm_provider_operation(c->view.confirming);
 if(!operation||!out||!c->selected||c->terminal||!op||
 !driver_manager_selection_matches(fresh,&c->selection,row)||
 !driver_manager_action_allowed(fresh->source,op,row,true))return false;
 *operation=op;*out=c->selection;c->view.confirming=0;c->selected=false;c->view.selected=-1;
 return true;
}
static inline bool dm_controller_clean(const dm_controller*c){
 return !c->terminal&&c->snapshot.result!=DRIVER_MANAGER_WORKING&&
 c->snapshot.result!=DRIVER_MANAGER_NEEDS_NETWORK&&c->snapshot.custody==DRIVER_MANAGER_CUSTODY_NONE;
}
