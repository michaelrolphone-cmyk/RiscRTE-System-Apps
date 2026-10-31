/* Copied semantic App Store presentation. Geometry, touch and rendering belong
 * exclusively to ui.scene. Main owns provider calls and irreversible intents. */
#include "RiscSceneComponentsV1.h"
enum { ASP_CHECK=1,ASP_PREVIOUS,ASP_NEXT,ASP_CANCEL,ASP_CONFIRM,ASP_RESTART,ASP_BACK,ASP_OPEN,ASP_RETRY_NETWORK,ASP_REVIEW,ASP_SOURCE,ASP_SELECT_BASE=256 };
#define ASP_PAGE_ROWS 8u
typedef struct {unsigned mode,page,count,selected;software_update_row_v1 row;bool can_launch,retry_network,modal,feed_available,feed_enabled;bool available[ASP_PAGE_ROWS];} app_store_presenter_view;
static app_store_presenter_view asp_candidate,asp_presented;
static bool asp_confirmation;
static risc_scene_node_v1 *asp_node(risc_components_document_v1*d,unsigned kind,const char*label,const char*text,unsigned action){
 if(d->node_count>=RISC_COMPONENTS_MAX_NODES)return NULL;
 unsigned i=d->node_count++;risc_scene_node_v1*n=&d->nodes[i];
 n->id=i+1;n->route=1;n->kind=kind;n->action=action;
 snprintf(n->label,sizeof(n->label),"%s",label?label:"");snprintf(n->text,sizeof(n->text),"%s",text?text:"");return n;
}
static void asp_text(risc_components_document_v1*d,const char*label,const char*text){
 if(!text||!*text){(void)asp_node(d,RISC_SCENE_TEXT_NODE,label,"Not available",0);return;}
 unsigned offset=0;do {char part[RISC_SCENE_TEXT];unsigned n=0;while(text[offset]&&n+1<sizeof(part))part[n++]=text[offset++];part[n]=0;(void)asp_node(d,RISC_SCENE_TEXT_NODE,offset==n?label:"",part,0);}while(text[offset]);
}
static void asp_button(risc_components_document_v1*d,const char*label,unsigned action,bool header){(void)asp_node(d,header?RISC_COMPONENT_HEADER_ACTION:RISC_SCENE_ACTION,label,"",action);}
static unsigned asp_mode(void){
 if(asc.terminal)return 5;
 if(asc.restart_pending)return 4;
 if(asc.cleanup_pending)return 3;
 if(asc.confirming)return 2;
 if(asc.connecting||asc.status.state==SOFTWARE_UPDATE_CATALOG||(asc.status.state>=SOFTWARE_UPDATE_PREPARING&&asc.status.state<=SOFTWARE_UPDATE_READY))return 1;
 return 0;
}
static const char *asp_state_name(void){
 if(asc.connecting)return "Connecting";
 switch(asc.status.state){case SOFTWARE_UPDATE_CATALOG:return "Checking catalog";case SOFTWARE_UPDATE_PREPARING:return "Preparing";case SOFTWARE_UPDATE_DOWNLOAD:return "Downloading";case SOFTWARE_UPDATE_VERIFYING:return "Verifying";case SOFTWARE_UPDATE_READY:return "Verified, not yet installed";default:return "Working";}
}
static const char *asp_confirm_label(unsigned availability){return availability==4u?"Remove app":availability==5u?"Install app":"Install and restart";}
static void asp_build(risc_components_document_v1*d){
 *d=(risc_components_document_v1){.api_version=1,.struct_size=sizeof(*d),.root=1,.route_count=1};
 d->routes[0].id=1;snprintf(d->routes[0].title,sizeof(d->routes[0].title),"App Store");
 unsigned mode=asp_mode();if(mode!=2||asc.selected!=asp_presented.selected||!asc_same_row(&asc.selected_row,&asp_presented.row))asp_confirmation=false;asp_candidate=(app_store_presenter_view){.mode=mode,.page=asc.page,.count=asc.status.count,.selected=asc.selected,.row=asc.selected_row,.modal=asp_confirmation,.feed_available=as_feed_available,.feed_enabled=as_feed_enabled,.can_launch=as_can_launch,.retry_network=asc.network_wait&&!asc.connecting&&!asc.radio_owned};
 d->screen_key=1u+(asp_confirmation?65536u:0u)+mode*256u+(mode==0?asc.page:mode==2?asc.selected:0u);
 if(mode==5){asp_text(d,"Session retained","Restart required; no further operations are safe");return;}
 if(asc.message[0]&&!(mode==2&&asp_confirmation))asp_text(d,"Status",asc.message);
 if(mode==4){asp_text(d,"Activation",asc.status.state==SOFTWARE_UPDATE_ACTIVATION_UNKNOWN?"Activation uncertain; restart required":"Activation confirmed; restart required");asp_button(d,"Restart device",ASP_RESTART,false);return;}
 if(mode==3){asp_text(d,"Cleanup","Previous operation must finish closing before another action");asp_button(d,"Retry cleanup",ASP_CANCEL,false);return;}
 if(mode==2){
  if(asp_confirmation){
   d->flags=RISC_COMPONENTS_CONFIRM;d->cancel_action=ASP_CANCEL;
   (void)asp_node(d,RISC_SCENE_TEXT_NODE,"Confirm selected app",as_selected_identity[0]?as_selected_identity:asc.selected_row.id,0);
   asp_button(d,"Cancel",ASP_CANCEL,false);asp_button(d,asp_confirm_label(asc.selected_row.availability),ASP_CONFIRM,false);return;
  }
  snprintf(d->routes[0].title,sizeof(d->routes[0].title),"Confirm app action");
  asp_text(d,"App",as_selected_identity[0]?as_selected_identity:asc.selected_row.id);asp_text(d,"Version",as_selected_version[0]?as_selected_version:asc.selected_row.version);if(as_selected_route[0])asp_text(d,"Route",as_selected_route);asp_text(d,"Installed",asc.selected_row.installed);asp_text(d,"Details",asc.selected_row.reason);
  char bytes[32];snprintf(bytes,sizeof(bytes),"%u bytes",(unsigned)asc.selected_row.size);asp_text(d,"Size",bytes);
  asp_button(d,"Cancel",ASP_CANCEL,false);
  if(as_can_launch)asp_button(d,"Open app",ASP_OPEN,false);
  if(asc.selected_row.availability==SOFTWARE_UPDATE_AVAILABLE||asc.selected_row.availability==4u||asc.selected_row.availability==5u)asp_button(d,"Review confirmation",ASP_REVIEW,false);
  else asp_text(d,"Action unavailable","This row cannot be changed by this service");
  return;
 }
 if(mode==1){
  risc_scene_node_v1*n=asp_node(d,RISC_COMPONENT_PROGRESS,asp_state_name(),"",0);
  if(n){if(asc.status.total){uint32_t unit=asc.status.total/1000u+(asc.status.total%1000u!=0);n->maximum=(int32_t)(asc.status.total/unit+(asc.status.total%unit!=0));n->value=asc.status.done==asc.status.total?n->maximum:(int32_t)(asc.status.done/unit);}else {n->maximum=0;n->value=0;}}
  char progress[64];snprintf(progress,sizeof(progress),"%u / %u bytes",(unsigned)asc.status.done,(unsigned)asc.status.total);asp_text(d,"Progress",progress);asp_button(d,"Cancel operation",ASP_CANCEL,false);if(asp_candidate.retry_network)asp_button(d,"Retry network",ASP_RETRY_NETWORK,false);return;
 }
 d->flags=RISC_COMPONENTS_ROOT_HEADER;
 asp_button(d,"Check",ASP_CHECK,true);asp_button(d,"Back",ASP_BACK,true);
 if(as_feed_available)asp_button(d,as_feed_enabled?"Release updates":"Verified package feed",ASP_SOURCE,false);
 if(!asc.service){asp_text(d,"Unavailable","App update and installed inventory service unavailable");return;}
 unsigned pages=asc.status.count?(asc.status.count+ASP_PAGE_ROWS-1u)/ASP_PAGE_ROWS:1u;
 if(asc.page>=pages)asc.page=pages-1u;
 asp_candidate.page=asc.page;d->screen_key=1u+asc.page;
 char page[48];snprintf(page,sizeof(page),"Page %u of %u; %u apps",asc.page+1u,pages,(unsigned)asc.status.count);asp_text(d,"Catalog",page);
 if(!asc.status.count){(void)asp_node(d,RISC_COMPONENT_EMPTY,"No apps listed","Check to refresh the catalog",0);return;}
 unsigned first=asc.page*ASP_PAGE_ROWS;
 for(unsigned slot=0;slot<ASP_PAGE_ROWS&&first+slot<asc.status.count;slot++){
  software_update_row_v1 row;
  if(!asc_row(&asc,first+slot,&row)){if(!asc_live(&asc))return;asp_text(d,"App unavailable","Catalog row could not be read; Check to retry");continue;}
  asp_candidate.available[slot]=true;
  char secondary[RISC_SCENE_TEXT];
  if(strlen(row.id)>=RISC_SCENE_LABEL)snprintf(secondary,sizeof(secondary),"%s",row.id);
  else snprintf(secondary,sizeof(secondary),"%s / %s",row.version,row.installed[0]?row.installed:"none");
  risc_scene_node_v1*n=asp_node(d,RISC_COMPONENT_ROW,row.id,secondary,ASP_SELECT_BASE+first+slot);
  if(n){unsigned i=d->node_count-1;const char*badge=row.availability==SOFTWARE_UPDATE_AVAILABLE?"Available":row.availability==SOFTWARE_UPDATE_CURRENT?"Current":row.availability==4u?"Installed":row.availability==5u?"Install":"Unavailable";snprintf(d->details[i].badge,sizeof(d->details[i].badge),"%s",badge);}
 }
 if(asc.page)asp_button(d,"Previous page",ASP_PREVIOUS,false);
 if(asc.page+1u<pages)asp_button(d,"Next page",ASP_NEXT,false);
}
/* Main commits only after open/update succeeds; unpublished candidates cannot
 * authorize an input. Main also checks revision/sequence and settled rendering. */
static void asp_commit(void){asp_presented=asp_candidate;}
static bool asp_action(uint32_t action){
 if(!asc_live(&asc)||asp_mode()!=asp_presented.mode)return false;
 unsigned mode=asp_presented.mode;
 if(action==ASP_BACK)return mode<=3;
 if(action==ASP_CANCEL)return mode==1||mode==2||mode==3;
 if(action==ASP_RESTART)return mode==4;
 if(action==ASP_RETRY_NETWORK)return mode==1&&asp_presented.retry_network&&asc.network_wait&&!asc.connecting&&!asc.radio_owned;
 if(action==ASP_OPEN)return mode==2&&asp_presented.can_launch&&as_can_launch&&asc.selected==asp_presented.selected&&asc_same_row(&asc.selected_row,&asp_presented.row);
 if(action==ASP_REVIEW&&mode==2&&!asp_presented.modal&&asc.selected==asp_presented.selected&&asc_same_row(&asc.selected_row,&asp_presented.row)&&(asc.selected_row.availability==0||asc.selected_row.availability==4||asc.selected_row.availability==5)){asp_confirmation=true;asc.dirty=true;return true;}
 if(mode==2)return asp_presented.modal&&action==ASP_CONFIRM&&(asc.selected_row.availability==SOFTWARE_UPDATE_AVAILABLE||asc.selected_row.availability==4u||asc.selected_row.availability==5u)&&asc.selected==asp_presented.selected&&asc_same_row(&asc.selected_row,&asp_presented.row);
 if(mode!=0)return false;
 if(action==ASP_SOURCE)return !asc.status.resources_open&&asp_presented.feed_available&&as_feed_available&&asp_presented.feed_enabled==as_feed_enabled&&!asc.connecting&&!asc.radio_owned&&!asc.network_wait;
 if(action==ASP_CHECK||action==ASP_BACK)return true;
 if(asc.page!=asp_presented.page||asc.status.count!=asp_presented.count)return false;
 if(action==ASP_PREVIOUS&&asc.page){--asc.page;asc.dirty=true;return true;}
 if(action==ASP_NEXT&&(asc.page+1u)*ASP_PAGE_ROWS<asc.status.count){++asc.page;asc.dirty=true;return true;}
 if(action>=ASP_SELECT_BASE&&action<ASP_SELECT_BASE+SOFTWARE_UPDATE_ROWS_MAX){asp_confirmation=false;unsigned index=action-ASP_SELECT_BASE,first=asp_presented.page*ASP_PAGE_ROWS;return index>=first&&index<first+ASP_PAGE_ROWS&&index<asp_presented.count&&asp_presented.available[index-first];}
 return false;
}
