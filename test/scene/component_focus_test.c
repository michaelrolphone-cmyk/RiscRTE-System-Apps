#define main baseline_main
#include "host_test.c"
#undef main
#include "RiscSceneComponentsV1.h"
int main(int argc,char **argv){
 assert(argc==3);w=(unsigned)atoi(argv[1]);h=(unsigned)atoi(argv[2]);
 assert(w>=160&&h>=160&&w*h<=800*480);startup();assert(!api->close(NULL,session));
 const risc_scene_components_api_v1 *components=risc_scene_components_get_v1(api);assert(components);
 risc_components_document_v1 rich={.api_version=1,.struct_size=sizeof(rich),.revision=1,.root=1,.route_count=1,.node_count=2,.screen_key=1,
 .routes={{.id=1,.back_action=99,.title="LEARN THIS PLACE"}},.subtitle="READY WHEN YOU ARE",
 .nodes={{.id=1,.route=1,.kind=RISC_COMPONENT_EMPTY,.label="LEARN THIS PLACE",.text="Stand in the room and carry on as normal."},
 {.id=2,.route=1,.kind=RISC_SCENE_ACTION,.action=29,.flags=RISC_SCENE_PRIMARY,.label="START"}}};
 expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;assert(!components->open(NULL,&rich,NULL,&session));settle();
 risc_scene_event_v1 e;nav_pressed=RISC_NAV_DOWN;assert(tick(&e)==RISC_SCENE_IDLE);settle();
 /* The centered focused action must be above the fixed Back overlay. */
 unsigned scale=w>=400?2:1;touch_tap(w/2,h-65*scale);
 assert(tick(&e)==RISC_SCENE_OK&&e.action==29&&e.node==2);
 rich.revision++;assert(!components->update(NULL,session,&rich));settle();
 touch_tap(w/2,h-27*scale);assert(tick(&e)==RISC_SCENE_OK&&e.action==99);
 finish();printf("Focused action and fixed Back remain independently touchable at %ux%u PASS\n",w,h);return 0;
}
