#define main baseline_main
#include "host_test.c"
#undef main
#include "RiscSceneTelemetryV1.h"
int telemetry_test_target(unsigned,int,int*,int*);
uint32_t telemetry_test_digest(void);
static risc_components_document_v1 rich,bad;
static risc_scene_telemetry_v1 traces,saved;
int main(void){
 w=480;h=800;format=1;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,0xffff,0,0,0,0};startup();assert(!api->close(NULL,session));
 const risc_scene_components_api_v1*cmp=risc_scene_components_get_v1(api);const risc_scene_telemetry_api_v1*tm=risc_scene_telemetry_get_v1(api);const risc_scene_checkpoint_api_v1*ck=risc_scene_checkpoint_get_v1(api);assert(cmp&&tm&&ck);
 risc_scene_telemetry_api_v1 old=*tm;((risc_scene_api_v1*)&old)->struct_size=sizeof(risc_scene_reading_links_api_v1);assert(!risc_scene_telemetry_get_v1((void*)&old));
 old=*tm;old.telemetry_tag=0;assert(!risc_scene_telemetry_get_v1((void*)&old));old=*tm;old.telemetry_version++;assert(!risc_scene_telemetry_get_v1((void*)&old));old=*tm;old.present=NULL;assert(!risc_scene_telemetry_get_v1((void*)&old));
 rich=(risc_components_document_v1){.api_version=1,.struct_size=sizeof(rich),.revision=1,.root=1,.route_count=1,.node_count=7,.screen_key=1,.flags=RISC_COMPONENTS_DOCUMENT|RISC_COMPONENTS_TELEMETRY|RISC_COMPONENTS_ROOT_HEADER};rich.routes[0]=(risc_scene_route_v1){1,99,"LIVE"};strcpy(rich.subtitle,"BLE|CAN 11/500");
 rich.nodes[0]=(risc_scene_node_v1){.id=1,.route=1,.kind=RISC_COMPONENT_INSTRUMENT,.label="088",.text="KM/H"};
 rich.nodes[1]=(risc_scene_node_v1){.id=2,.route=1,.kind=RISC_COMPONENT_BAR_GAUGE,.label="ENGINE SPEED",.text="2400 RPM",.value=2400,.maximum=8000};
 for(unsigned i=2;i<6;i++)rich.nodes[i]=(risc_scene_node_v1){.id=i+1,.route=1,.kind=RISC_COMPONENT_METRIC_TILE,.action=20+i,.label="SENSOR",.text="90"};
 rich.nodes[6]=(risc_scene_node_v1){.id=7,.route=1,.kind=RISC_COMPONENT_TAB_BAR,.action=50,.maximum=3,.step=1};strcpy(rich.details[6].choices,"LIVE|GRAPH|FAULTS|INFO");
 expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;assert(!cmp->open(NULL,&rich,NULL,&session));rich.revision++;
 traces.struct_size=sizeof(traces);traces.count=1;traces.curves[0].node=3;traces.curves[0].count=151;for(unsigned j=0;j<151;j++)traces.curves[0].points[j]=(uint16_t)(j*66);traces.curves[0].points[80]=UINT16_MAX;saved=traces;
 assert(!tm->present(NULL,session,&rich,&traces));uint32_t digest=telemetry_test_digest();memset(&traces,0xa5,sizeof(traces));assert(telemetry_test_digest()==digest);settle();assert(telemetry_test_digest()==digest);traces=saved;
 int x,y;assert(telemetry_test_target(22,0,&x,&y)==1&&x<240&&y>300&&y<728);touch_tap((unsigned)x,(unsigned)y);risc_scene_event_v1 e;assert(tick(&e)==0&&e.action==22&&e.document_revision==rich.revision);
 settle();for(unsigned j=0;j<4;j++){assert(telemetry_test_target(50,(int)j,&x,&y)==1&&y>=728);touch_tap((unsigned)x,(unsigned)y);assert(tick(&e)==0&&e.action==50&&e.value==(int)j);}
 unsigned before=calls;bad=rich;bad.revision++;
 traces.count=12;assert(tm->present(NULL,session,&bad,&traces)==RISC_SCENE_INVALID);traces=saved;traces.curves[0].node=1;assert(tm->present(NULL,session,&bad,&traces)==RISC_SCENE_INVALID);traces=saved;traces.curves[0].count=152;assert(tm->present(NULL,session,&bad,&traces)==RISC_SCENE_INVALID);traces=saved;traces.curves[0].points[0]=10001;assert(tm->present(NULL,session,&bad,&traces)==RISC_SCENE_INVALID);traces=saved;traces.curves[0].style=3;assert(tm->present(NULL,session,&bad,&traces)==RISC_SCENE_INVALID);
 traces=saved;traces.count=2;traces.curves[1]=traces.curves[0];assert(tm->present(NULL,session,&bad,&traces)==RISC_SCENE_INVALID);traces=saved;strcpy(bad.details[6].choices,"LIVE||FAULTS|INFO");assert(tm->present(NULL,session,&bad,&traces)==RISC_SCENE_INVALID);
 assert(calls==before&&telemetry_test_digest()==digest);bad=rich;bad.revision++;bad.nodes[1].maximum=0;assert(tm->present(NULL,session,&bad,&traces)==RISC_SCENE_INVALID);
 assert(tm->present(NULL,session+100,&rich,&traces)==RISC_SCENE_STALE);assert(tm->present(NULL,session,&rich,&traces)==RISC_SCENE_STALE);
 settle();unsigned paints=submits;int rc;while((rc=ck->pause(NULL,session))==RISC_SCENE_AGAIN){risc_scene_event_v1 out;assert(tick(&out)==RISC_SCENE_IDLE);}assert(rc==0);assert(!ck->resume(NULL,session,0));settle();assert(submits==paints&&telemetry_test_digest()==digest);
 /* Three distinguishable plot traces accepted; a fourth is rejected. */
 rich.revision++;rich.screen_key++;rich.nodes[0].kind=RISC_COMPONENT_PLOT;rich.nodes[0].value=30;rich.nodes[0].label[0]=rich.nodes[0].text[0]=0;traces.count=3;
 for(unsigned i=0;i<3;i++){traces.curves[i]=saved.curves[0];traces.curves[i].node=1;traces.curves[i].style=i;}
 assert(!tm->present(NULL,session,&rich,&traces));settle();digest=telemetry_test_digest();bad=rich;bad.revision++;traces.count=4;traces.curves[3]=traces.curves[0];assert(tm->present(NULL,session,&bad,&traces)==RISC_SCENE_INVALID&&telemetry_test_digest()==digest);
 finish();puts("Telemetry: optional ABI, atomic copies, curve bounds/gaps/styles, real tile/tab taps, invalid/repeated revisions, retained checkpoint no repaint PASS");return 0;
}
