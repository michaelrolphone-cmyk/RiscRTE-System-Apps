#pragma once
/* Driver Manager owns intent and selection; ui.scene owns dimensions,
 * typography, pagination, pixels and input. No product identifiers occur here. */
#include "RiscSceneComponentsV1.h"
#include <stdio.h>
#include <string.h>
#define DM_ROWS_MAX 65u
enum { DM_ONLINE, DM_INBOX, DM_RECOVERY };
enum { DM_BACK=1, DM_REFRESH, DM_CANCEL, DM_CONFIRM, DM_INSTALL,
       DM_UNINSTALL, DM_RETRY, DM_DISCARD, DM_ONLINE_TAB=10, DM_INBOX_TAB,
       DM_RECOVERY_TAB, DM_SELECT=100 };
enum { DM_VALID=1u, DM_INSTALLABLE=2u, DM_INSTALLED=4u, DM_UPDATE=8u,
       DM_REMOVABLE=16u, DM_RETRYABLE=32u, DM_DISCARDABLE=64u };
typedef struct {
 char id[64],version[32],installed_version[32],label[96],detail[160];
 uint32_t flags;
} dm_row;
typedef struct {
 uint32_t source,generation,count,available_sources;
 dm_row rows[DM_ROWS_MAX];
 int32_t selected;
 uint32_t confirming;
 bool busy;
 char status[160];
} dm_view;
static inline const char *dm_source_name(unsigned source){
 return source==DM_ONLINE?"Release catalog":source==DM_INBOX?"SD Inbox":"Recovery";
}
static inline bool dm_source_available(const dm_view *v){
 return v->source<=DM_RECOVERY && (v->available_sources&(1u<<v->source));
}
static inline const char *dm_operation_name(unsigned action,const dm_row *row){
 switch(action){
 case DM_INSTALL:return row&&(row->flags&DM_UPDATE)?"Update driver":"Install driver";
 case DM_UNINSTALL:return "Uninstall driver";
 case DM_RETRY:return "Retry recovery";
 case DM_DISCARD:return "Discard retained stage";
 default:return "";
 }
}
static inline bool dm_operation_allowed(const dm_view *v,unsigned action){
 if(v->busy||!dm_source_available(v)||v->selected<0||((uint32_t)v->selected>=v->count||(uint32_t)v->selected>=DM_ROWS_MAX))return false;
 uint32_t f=v->rows[v->selected].flags;
 switch(action){
 case DM_INSTALL:return v->source!=DM_RECOVERY&&(f&DM_VALID)&&(f&DM_INSTALLABLE);
 case DM_UNINSTALL:return v->source==DM_INBOX&&(f&DM_REMOVABLE);
 case DM_RETRY:return v->source==DM_RECOVERY&&(f&DM_RETRYABLE);
 case DM_DISCARD:return v->source==DM_RECOVERY&&(f&DM_DISCARDABLE);
 default:return false;
 }
}
static inline void dm_node(risc_components_document_v1 *d,unsigned action,unsigned kind,
 const char *label,const char *text,unsigned flags,unsigned symbol){
 if(d->node_count>=RISC_COMPONENTS_MAX_NODES)return;
 unsigned i=d->node_count++;
 risc_scene_node_v1 *n=&d->nodes[i];
 n->id=i+1;n->route=1;n->kind=kind;n->action=action;n->flags=flags;
 snprintf(n->label,sizeof(n->label),"%.39s",label?label:"");
 snprintf(n->text,sizeof(n->text),"%.71s",text?text:"");
 d->details[i].symbol=symbol;
}
static inline void dm_text_sections(risc_components_document_v1*d,const char*title,const char*text){
 size_t remaining=strlen(text);
 if(!remaining){dm_node(d,0,RISC_SCENE_TEXT_NODE,title,"",0,0);return;}
 while(remaining){
  size_t n=remaining<71?remaining:71;
  while(n<remaining&&n&&((unsigned char)text[n]&0xc0u)==0x80u)--n;
  if(!n)return;
  char part[72];memcpy(part,text,n);part[n]=0;
  dm_node(d,0,RISC_SCENE_TEXT_NODE,title,part,0,0);title="";
  text+=n;remaining-=n;
 }
}
static inline void dm_document(const dm_view *v,uint32_t revision,risc_components_document_v1 *d){
 memset(d,0,sizeof(*d));d->api_version=1;d->struct_size=sizeof(*d);
 d->revision=revision;d->root=1;d->route_count=1;d->routes[0].id=1;
 d->routes[0].back_action=DM_BACK;
 snprintf(d->routes[0].title,sizeof(d->routes[0].title),"Driver Manager");
 snprintf(d->subtitle,sizeof(d->subtitle),"%s",dm_source_name(v->source));
 d->screen_key=1+v->source+(v->selected>=0?10u:0u)+(v->confirming?100u:0u);
 const dm_row *r=v->selected>=0&&(uint32_t)v->selected<v->count&&(uint32_t)v->selected<DM_ROWS_MAX?&v->rows[v->selected]:NULL;
 if(v->confirming&&r){
  d->flags=RISC_COMPONENTS_CONFIRM;d->cancel_action=DM_CANCEL;
  d->routes[0].back_action=DM_CANCEL;
  dm_node(d,0,RISC_SCENE_TEXT_NODE,dm_operation_name(v->confirming,r),r->id[0]?r->id:r->label,0,RISC_SYMBOL_GEAR);
  dm_node(d,0,RISC_SCENE_TEXT_NODE,"Selected version",r->version,0,0);
  dm_node(d,0,RISC_SCENE_TEXT_NODE,"Existing data",
   v->confirming==DM_DISCARD?"Only the explicitly selected retained stage may be discarded.":"Provider preserves saved data; no driver activation is requested.",0,0);
  dm_node(d,DM_CANCEL,RISC_SCENE_ACTION,"Cancel","Keep existing files",0,RISC_SYMBOL_BACK);
  dm_node(d,DM_CONFIRM,RISC_SCENE_ACTION,dm_operation_name(v->confirming,r),"Confirm selected operation",
   (v->confirming==DM_UNINSTALL||v->confirming==DM_DISCARD?RISC_SCENE_DESTRUCTIVE:0)|
   (dm_operation_allowed(v,v->confirming)?0:RISC_SCENE_DISABLED),RISC_SYMBOL_CHECK);
  return;
 }
 dm_node(d,0,RISC_SCENE_TEXT_NODE,v->busy?"Working":"Status",v->status,0,RISC_SYMBOL_INFO);
 if(v->busy){
  dm_node(d,0,RISC_COMPONENT_PROGRESS,"Package operation","Provider owns verification and transaction",0,RISC_SYMBOL_ACTIVITY);
  dm_node(d,DM_CANCEL,RISC_SCENE_ACTION,"Cancel / finish cleanup","Pending custody is drained before returning",0,RISC_SYMBOL_CLOSE);
  return;
 }
 if(r){
  dm_node(d,0,RISC_COMPONENT_SECTION,"Selected driver",r->id,0,RISC_SYMBOL_GEAR);
  dm_node(d,0,RISC_COMPONENT_STAT_ROW,"Candidate version",r->version,0,0);
  dm_node(d,0,RISC_COMPONENT_STAT_ROW,"Installed version",r->installed_version[0]?r->installed_version:"Not installed",0,0);
  dm_text_sections(d,"Package state",r->detail);
  if(v->source==DM_RECOVERY){
   dm_node(d,DM_RETRY,RISC_SCENE_ACTION,"Retry recovery","Reverify retained files offline",
    dm_operation_allowed(v,DM_RETRY)?0:RISC_SCENE_DISABLED,RISC_SYMBOL_REFRESH);
   dm_node(d,DM_DISCARD,RISC_SCENE_ACTION,"Discard retained stage","Separate confirmation required",
    RISC_SCENE_DESTRUCTIVE|(dm_operation_allowed(v,DM_DISCARD)?0:RISC_SCENE_DISABLED),RISC_SYMBOL_DELETE);
  }else{
   dm_node(d,DM_INSTALL,RISC_SCENE_ACTION,dm_operation_name(DM_INSTALL,r),"Verified installation; not activation",
    dm_operation_allowed(v,DM_INSTALL)?0:RISC_SCENE_DISABLED,RISC_SYMBOL_CHECK);
   if(v->source==DM_INBOX)dm_node(d,DM_UNINSTALL,RISC_SCENE_ACTION,"Uninstall driver","Separate confirmation; saved data retained",
    RISC_SCENE_DESTRUCTIVE|(dm_operation_allowed(v,DM_UNINSTALL)?0:RISC_SCENE_DISABLED),RISC_SYMBOL_DELETE);
  }
  dm_node(d,DM_BACK,RISC_SCENE_ACTION,"Back","Return to driver list",0,RISC_SYMBOL_BACK);
  return;
 }
 dm_node(d,DM_ONLINE_TAB,RISC_COMPONENT_CHIP,"Online","",0,RISC_SYMBOL_LIBRARY);
 dm_node(d,DM_INBOX_TAB,RISC_COMPONENT_CHIP,"SD Inbox","",0,RISC_SYMBOL_DOCUMENT_LIST);
 dm_node(d,DM_RECOVERY_TAB,RISC_COMPONENT_CHIP,"Recovery","",0,RISC_SYMBOL_REFRESH);
 if(!dm_source_available(v)){
  dm_node(d,0,RISC_COMPONENT_EMPTY,"Unavailable",
   v->source==DM_ONLINE?"Online driver packages unavailable from authorized providers.":
   v->source==DM_INBOX?"SD driver package access unavailable; existing files retained.":
   "Driver recovery unavailable; retained files remain unchanged.",0,RISC_SYMBOL_INFO);
 }else if(!v->count){
  dm_node(d,0,RISC_COMPONENT_EMPTY,v->source==DM_RECOVERY?"No retained stages":"No drivers",
   v->source==DM_INBOX?"Place driver ZIPs or package directories in /Packages/Inbox.":
   v->source==DM_ONLINE?"No driver package in the selected catalog.":"No recovery candidate reported by provider.",0,RISC_SYMBOL_LIBRARY);
 }else{
  unsigned count=v->count<DM_ROWS_MAX?v->count:DM_ROWS_MAX;
  for(unsigned i=0;i<count;i++){
   const dm_row *item=&v->rows[i];
   dm_node(d,DM_SELECT+i,RISC_COMPONENT_ROW,item->id[0]?item->id:item->label,item->detail,0,
    item->flags&DM_UPDATE?RISC_SYMBOL_REFRESH:item->flags&DM_INSTALLED?RISC_SYMBOL_CHECK:RISC_SYMBOL_GEAR);
   snprintf(d->details[d->node_count-1].badge,sizeof(d->details[0].badge),"%.15s",item->version);
  }
 }
 dm_node(d,DM_REFRESH,RISC_SCENE_ACTION,"Refresh",dm_source_name(v->source),
  dm_source_available(v)?0:RISC_SCENE_DISABLED,RISC_SYMBOL_REFRESH);
 dm_node(d,DM_BACK,RISC_SCENE_ACTION,"Back to Home","",0,RISC_SYMBOL_BACK);
}
static inline bool dm_event_allowed(const risc_components_document_v1 *d,const risc_scene_event_v1 *e){
 if(e->struct_size!=sizeof(*e)||e->kind!=RISC_SCENE_ACTION_EVENT||e->document_revision!=d->revision)return false;
 if(!e->node)return e->action==d->routes[0].back_action||(d->cancel_action&&e->action==d->cancel_action);
 for(unsigned i=0;i<d->node_count;i++){
  const risc_scene_node_v1*n=&d->nodes[i];
  if(n->id==e->node&&n->action&&n->action==e->action&&!(n->flags&(RISC_SCENE_DISABLED|RISC_SCENE_HIDDEN)))return true;
 }
 return false;
}
