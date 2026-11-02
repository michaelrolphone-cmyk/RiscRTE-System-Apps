#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../Apps/driver_manager_presenter.h"
static risc_components_document_v1 document;
static dm_view view;
static const risc_scene_node_v1 *action(unsigned id){
 for(unsigned i=0;i<document.node_count;i++)if(document.nodes[i].action==id)return &document.nodes[i];
 return NULL;
}
static bool enabled(unsigned id){
 const risc_scene_node_v1*n=action(id);return n&&!(n->flags&RISC_SCENE_DISABLED);
}
static void build(void){dm_document(&view,7,&document);assert(document.node_count<=RISC_COMPONENTS_MAX_NODES);assert(document.revision==7);for(unsigned i=0;i<document.node_count;i++){assert(document.nodes[i].id==i+1);assert(document.nodes[i].route==1);}}
int main(void){
 memset(&view,0,sizeof(view));view.selected=-1;
 for(unsigned source=0;source<3;source++){
  view.source=source;build();assert(!enabled(DM_REFRESH));assert(enabled(DM_BACK));
  view.available_sources=1u<<source;build();assert(enabled(DM_REFRESH));view.available_sources=0;
 }
 view.available_sources=7;view.count=DM_ROWS_MAX;
 for(unsigned i=0;i<DM_ROWS_MAX;i++){snprintf(view.rows[i].id,sizeof(view.rows[i].id),"driver-%u",i);strcpy(view.rows[i].version,"1.0.8");strcpy(view.rows[i].detail,"Verified provider metadata");view.rows[i].flags=DM_VALID|DM_INSTALLABLE|DM_INSTALLED|DM_UPDATE|DM_REMOVABLE|DM_RETRYABLE|DM_DISCARDABLE;}
 view.source=DM_ONLINE;build();assert(enabled(DM_SELECT+64));assert(document.node_count==71);
 risc_scene_event_v1 event={.struct_size=sizeof(event),.kind=RISC_SCENE_ACTION_EVENT,.document_revision=7,.node=action(DM_SELECT+64)->id,.action=DM_SELECT+64};
 assert(dm_event_allowed(&document,&event));event.document_revision=6;assert(!dm_event_allowed(&document,&event));event.document_revision=7;event.action=DM_DISCARD;assert(!dm_event_allowed(&document,&event));
 view.selected=0;build();assert(enabled(DM_INSTALL));assert(!action(DM_UNINSTALL));assert(!action(DM_DISCARD));
 view.source=DM_INBOX;build();assert(enabled(DM_UNINSTALL));
 view.source=DM_RECOVERY;build();assert(enabled(DM_RETRY)&&enabled(DM_DISCARD));assert(!action(DM_INSTALL));
 for(unsigned source=0;source<3;source++)for(unsigned flags=0;flags<128;flags++){
  view.source=source;view.rows[0].flags=flags;view.confirming=0;build();
  assert(dm_operation_allowed(&view,DM_INSTALL)==(source!=DM_RECOVERY&&(flags&DM_VALID)&&(flags&DM_INSTALLABLE)));
  assert(dm_operation_allowed(&view,DM_UNINSTALL)==(source==DM_INBOX&&!!(flags&DM_REMOVABLE)));
  assert(dm_operation_allowed(&view,DM_RETRY)==(source==DM_RECOVERY&&!!(flags&DM_RETRYABLE)));
  assert(dm_operation_allowed(&view,DM_DISCARD)==(source==DM_RECOVERY&&!!(flags&DM_DISCARDABLE)));
 }
 view.source=DM_RECOVERY;view.rows[0].flags=DM_DISCARDABLE;view.confirming=DM_DISCARD;build();
 assert(document.flags&RISC_COMPONENTS_CONFIRM);assert(document.cancel_action==DM_CANCEL);assert(document.routes[0].back_action==DM_CANCEL);
 assert(action(DM_CANCEL)->id<action(DM_CONFIRM)->id);assert(action(DM_CONFIRM)->flags&RISC_SCENE_DESTRUCTIVE);assert(enabled(DM_CONFIRM));
 view.rows[0].flags=0;build();assert(!enabled(DM_CONFIRM));
 event=(risc_scene_event_v1){.struct_size=sizeof(event),.kind=RISC_SCENE_ACTION_EVENT,.document_revision=7,.node=action(DM_CONFIRM)->id,.action=DM_CONFIRM};assert(!dm_event_allowed(&document,&event));
 view.confirming=0;view.busy=true;build();assert(enabled(DM_CANCEL));assert(!dm_operation_allowed(&view,DM_RETRY));
 view.busy=false;view.count=1000;view.selected=999;build();assert(!dm_operation_allowed(&view,DM_INSTALL));assert(document.node_count<=RISC_COMPONENTS_MAX_NODES);
 puts("Driver Manager semantic presentation: capability/row/source/confirmation/stale-input matrix PASS");return 0;
}
