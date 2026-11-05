#define main baseline_main
#include "host_test.c"
#undef main
#include "RiscSceneTelemetryV1.h"
void telemetry_test_orientation(unsigned,unsigned,unsigned);
int main(void){
 for(unsigned rotation=0;rotation<360;rotation+=90){
  w=480;h=800;format=1;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,0xffff,0,rotation,(360-rotation)%360,0};
  startup();assert(!api->close(NULL,session));
  const risc_scene_components_api_v1*cmp=risc_scene_components_get_v1(api);
  const risc_scene_telemetry_api_v1*tm=risc_scene_telemetry_get_v1(api);assert(tm&&tm->telemetry_version==2);
  risc_components_document_v1 d={.api_version=1,.struct_size=sizeof(d),.revision=1,.root=1,.route_count=1,.node_count=1,.screen_key=1,.flags=RISC_COMPONENTS_TELEMETRY|RISC_COMPONENTS_LANDSCAPE};
  d.routes[0]=(risc_scene_route_v1){1,99,"LANDSCAPE"};d.nodes[0]=(risc_scene_node_v1){.id=1,.route=1,.kind=RISC_COMPONENT_ROW,.action=42,.label="TEST"};
  expected_intent=RISC_DISPLAY_PRESENT_LOW_LATENCY;assert(!cmp->open(NULL,&d,NULL,&session));settle();
  unsigned expected=rotation%180?rotation:(rotation+90)%360;
  telemetry_test_orientation(800,480,expected);
  d.revision++;d.flags^=RISC_COMPONENTS_LANDSCAPE;assert(cmp->update(NULL,session,&d)==RISC_SCENE_INVALID);
  finish();check_guards();
 }
 puts("Landscape: four baseline rotations, inverse touch coordinates, raster guards, immutable active orientation and session cleanup PASS");return 0;
}
