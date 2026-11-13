/* Real published scene host + native navigation endpoint, no semantic injection. */
#define main published_host_fixture_main
#include "../scene/host_test.c"
#undef main
#include "RiscSceneComponentsV1.h"
static uint32_t focused(void){risc_scene_navigation_v1 n={.struct_size=sizeof(n)};uint32_t f=0;assert(api->snapshot(NULL,session,&n,&f)==RISC_SCENE_OK);return n.focus[0];}
static int press(uint32_t key,risc_scene_event_v1*e){
 nav_buttons=key;nav_pressed=key;int rc=tick(e);
 nav_buttons=0;nav_released=key;
 risc_scene_event_v1 release_event;assert(tick(&release_event)==RISC_SCENE_IDLE);
 settle();return rc;
}
int main(int argc,char**argv){
 assert(argc==2);const char*profile=argv[1];bool paper=!strcmp(profile,"paper");
 if(paper){w=800;h=480;format=1;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,0xffff,0,270,0,0};}
 else if(!strcmp(profile,"wave")){w=410;h=502;}
 else if(!strcmp(profile,"square")){w=466;h=466;}
 else assert(!strcmp(profile,"watch"));
 startup();assert(api->close(NULL,session)==RISC_SCENE_OK);
 const risc_scene_components_api_v1*c=risc_scene_components_get_v1(api);assert(c);
 risc_components_document_v1 d={.api_version=1,.struct_size=sizeof(d),.revision=1,.root=1,.route_count=1,.node_count=26,.screen_key=1,.flags=RISC_COMPONENTS_ROOT_HEADER};
 d.routes[0]=(risc_scene_route_v1){.id=1,.back_action=999,.title="HEADER NAVIGATION"};
 for(unsigned i=0;i<26;i++){
  d.nodes[i]=(risc_scene_node_v1){.id=i+1,.route=1,.kind=i<2?RISC_COMPONENT_HEADER_ACTION:RISC_SCENE_ACTION,.action=100+i};
  snprintf(d.nodes[i].label,sizeof(d.nodes[i].label),i<2?"HEADER %u":"LOWER ROW %u",i+1);
 }
 expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;
 assert(c->open(NULL,&d,NULL,&session)==RISC_SCENE_OK);settle();
 risc_scene_event_v1 e;
 // Navigate to the final lower action: the scroll must advance to reveal it.
 for(unsigned i=0;i<26;i++){assert(press(RISC_NAV_DOWN,&e)==RISC_SCENE_IDLE);assert(focused()==i+1);}
 assert(press(RISC_NAV_CONFIRM,&e)==RISC_SCENE_OK&&e.node==26&&e.action==125);
 // Wrap to each header from the lower viewport; Confirm must execute that
 // committed header, not silently move the focus to another hidden target.
 assert(press(RISC_NAV_DOWN,&e)==RISC_SCENE_IDLE&&focused()==1);
 int rc=press(RISC_NAV_CONFIRM,&e);
 fprintf(stderr,"profile=%s wrapped_header=1 result=%d emitted_node=%u action=%u focus=%u\n",profile,rc,e.node,e.action,focused());
 assert(rc==RISC_SCENE_OK&&e.kind==RISC_SCENE_ACTION_EVENT&&e.node==1&&e.action==100);
 assert(press(RISC_NAV_DOWN,&e)==RISC_SCENE_IDLE&&focused()==2);
 assert(press(RISC_NAV_CONFIRM,&e)==RISC_SCENE_OK&&e.node==2&&e.action==101);
 // Reverse traversal also returns header2 after lower-content scrolling.
 assert(press(RISC_NAV_UP,&e)==RISC_SCENE_IDLE&&focused()==1);
 assert(press(RISC_NAV_UP,&e)==RISC_SCENE_IDLE&&focused()==26);
 for(unsigned i=26;i>2;i--)assert(press(RISC_NAV_UP,&e)==RISC_SCENE_IDLE);
 assert(focused()==2);
 assert(press(RISC_NAV_CONFIRM,&e)==RISC_SCENE_OK&&e.node==2&&e.action==101);
 finish();printf("scene header focus %s PASS\n",profile);return 0;
}
