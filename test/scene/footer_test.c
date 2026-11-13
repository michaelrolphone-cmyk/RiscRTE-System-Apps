#define main baseline_main
#include "host_test.c"
#undef main
#include "RiscSceneComponentsV1.h"
bool scene_footer_target(uint32_t,int*,int*);
int main(int argc,char**argv){
 assert(argc==3);w=(unsigned)atoi(argv[1]);h=(unsigned)atoi(argv[2]);
 startup();assert(!api->close(NULL,session));expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;
 const risc_scene_components_api_v1*c=risc_scene_components_get_v1(api);assert(c);
 risc_components_document_v1 d={.api_version=1,.struct_size=sizeof(d),.revision=1,.root=1,.route_count=1,.node_count=2,.screen_key=1,
 .routes={{.id=1,.back_action=99,.title="READY"}},
 .nodes={{.id=1,.route=1,.kind=RISC_COMPONENT_EMPTY,.label="LEARN THIS PLACE",.text="Carry on as normal."},
 {.id=2,.route=1,.kind=RISC_SCENE_ACTION,.action=17,.label="START"}}};
 strcpy(d.subtitle,"READY WHEN YOU ARE");assert(!c->open(NULL,&d,NULL,&session));settle();
 nav_pressed=RISC_NAV_DOWN;risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_IDLE);settle();
 int x,y;assert(scene_footer_target(17,&x,&y));
 touch_tap((unsigned)x,(unsigned)y);assert(tick(&e)==RISC_SCENE_OK&&e.action==17);
 finish();puts("fixed footer body action remains wholly exposed PASS");return 0;
}
