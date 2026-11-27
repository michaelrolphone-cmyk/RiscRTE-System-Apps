/* Actual presenter and input path; synthetic display/touch only. */
#define main baseline_main
#include "host_test.c"
#undef main
#include "RiscSceneConnectivityV1.h"
int telemetry_test_target(unsigned,int,int*,int*);
void connectivity_test_dimensions(unsigned,unsigned);
#ifndef CONNECTIVITY_COMPACT
#define CONNECTIVITY_COMPACT 0
#endif
_Static_assert((RISC_COMPONENTS_CONNECTIVITY&RISC_COMPONENTS_LANDSCAPE)==0,
               "Connectivity must not rotate landscape sessions");
int main(int argc,char **argv){
 assert(argc==5);w=(unsigned)strtoul(argv[1],NULL,10);h=(unsigned)strtoul(argv[2],NULL,10);
 format=(unsigned)strtoul(argv[3],NULL,10);unsigned rotation=(unsigned)strtoul(argv[4],NULL,10);
 prof=(risc_scene_profile_v1){1,sizeof(prof),format,format==5?2:3,format==5?42:88,
    format==5?8:20,format==5?0xffff:0,format==5?0x0841:0xffff,0x05ff,rotation,0,0};
 startup();assert(!api->close(NULL,session));
 const risc_scene_connectivity_api_v1 *cn=risc_scene_connectivity_get_v1(api);assert(cn);
 const risc_scene_components_api_v1 *cmp=risc_scene_components_get_v1(api);assert(cmp);
 assert((risc_scene_telemetry_get_v1(api)==NULL)==(CONNECTIVITY_COMPACT!=0));
 risc_scene_connectivity_api_v1 old=*cn;
 ((risc_scene_api_v1*)&old)->struct_size=sizeof(risc_scene_telemetry_api_v1);
 assert(!risc_scene_connectivity_get_v1((void*)&old));old=*cn;old.connectivity_tag=0;
 assert(!risc_scene_connectivity_get_v1((void*)&old));old=*cn;old.connectivity_version++;
 assert(!risc_scene_connectivity_get_v1((void*)&old));
 risc_components_document_v1 d={.api_version=1,.struct_size=sizeof(d),.revision=1,.root=1,
   .route_count=1,.node_count=3,.screen_key=1,.flags=RISC_COMPONENTS_CONNECTIVITY|RISC_COMPONENTS_ROOT_HEADER};
 d.routes[0]=(risc_scene_route_v1){1,99,"WI-FI"};
 d.nodes[0]=(risc_scene_node_v1){.id=1,.route=1,.kind=RISC_COMPONENT_NETWORK_ROW,
   .action=10,.value=4,.label="Test network",.text="WPA2"};
 d.details[0].symbol=RISC_SYMBOL_LOCK;
 d.nodes[1]=(risc_scene_node_v1){.id=2,.route=1,.kind=RISC_COMPONENT_TAB_BAR,
   .action=20,.maximum=1,.step=1};strcpy(d.details[1].choices,"WI-FI|SAVED");d.details[1].symbol=RISC_SYMBOL_WIFI;
 d.nodes[2]=(risc_scene_node_v1){.id=3,.route=1,.kind=RISC_COMPONENT_HEADER_ACTION,.action=30,.label="RESCAN"};
 d.details[2].symbol=RISC_SYMBOL_REFRESH;
 expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;
 assert(!cmp->open(NULL,&d,NULL,&session));settle();
 connectivity_test_dimensions(rotation==90||rotation==270?h:w,rotation==90||rotation==270?w:h);
 risc_scene_event_v1 e;int x,y;
 assert(telemetry_test_target(10,0,&x,&y)==1);touch_tap((unsigned)x,(unsigned)y);
 assert(tick(&e)==RISC_SCENE_OK&&e.action==10);settle();
 for(unsigned i=0;i<2;i++){assert(telemetry_test_target(20,(int)i,&x,&y)==1);
   touch_tap((unsigned)x,(unsigned)y);assert(tick(&e)==RISC_SCENE_OK&&e.action==20&&e.value==(int)i);settle();}
 unsigned before=calls;risc_components_document_v1 bad=d;bad.revision++;
 bad.nodes[0].value=5;assert(cmp->update(NULL,session,&bad)==RISC_SCENE_INVALID);
 bad=d;bad.revision++;bad.flags|=128u;assert(cmp->update(NULL,session,&bad)==RISC_SCENE_INVALID);
 bad=d;bad.revision++;bad.nodes[1].maximum=6;assert(cmp->update(NULL,session,&bad)==RISC_SCENE_INVALID);
 assert(calls==before);
 if(CONNECTIVITY_COMPACT){bad=d;bad.revision++;bad.flags|=RISC_COMPONENTS_TELEMETRY;
   assert(cmp->update(NULL,session,&bad)==RISC_SCENE_INVALID&&calls==before);}
 /* Every accepted tab count uses the same generic layout and hit mapping. */
 for(unsigned count=2;count<=6;count++){
   d.revision++;d.nodes[1].maximum=(int)count-1;d.details[1].choices[0]=0;
   for(unsigned i=0;i<count;i++)strcat(d.details[1].choices,i?"|T":"T");
   assert(!cmp->update(NULL,session,&d));settle();
   for(unsigned i=0;i<count;i++){assert(telemetry_test_target(20,(int)i,&x,&y)==1);
     touch_tap((unsigned)x,(unsigned)y);assert(tick(&e)==RISC_SCENE_OK&&e.action==20&&e.value==(int)i);settle();}
 }
 finish();printf("Connectivity %ux%u format=%u rotation=%u compact=%u PASS\n",w,h,format,rotation,CONNECTIVITY_COMPACT);return 0;
}
