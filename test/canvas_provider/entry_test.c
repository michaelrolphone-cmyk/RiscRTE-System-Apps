#define main provider_fixture_main
#include "provider_test.c"
#undef main
#include "../../Services/canvas_host/host.c"
static bool invocation_live=true;
static void yield_ms(uint32_t ms){(void)ms;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.yield_ms=yield_ms};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t version){return version==1&&invocation_live?&runtime:NULL;}
int main(void){
 const risc_driver_v2*driver=t5_driver_get(2);assert(driver&&!t5_driver_get(1));
 risc_display_output_api_v1 d={.api_version=1,.struct_size=sizeof(d),.get_info=info,.acquire=acquire,.release=release,.submit=submit,.present_status=status};
 risc_scene_profile_v1 profile={.api_version=1,.struct_size=sizeof(profile),.preferred_format=1};
 risc_provider_dependency_v1 deps[]={{"display.output",1,&d},{"ui.presentation-profile",1,&profile}};
 assert(!driver->start(deps,1));risc_provider_dependency_v1 duplicate[]={deps[0],deps[0],deps[1]};assert(!driver->start(duplicate,3));
 for(unsigned display_rotation=0;display_rotation<4;display_rotation++)for(unsigned touch_rotation=0;touch_rotation<4;touch_rotation++){
  profile.display_rotation=display_rotation*90;profile.touch_rotation=touch_rotation*90;invocation_live=false;assert(driver->start(deps,2));invocation_live=true;
  const risc_canvas_api_v1*a=risc_canvas_get_v1(driver->capability,sizeof(risc_canvas_api_v1));assert(a);
  risc_canvas_open_v1 request={sizeof(request),1,3,0,0};uint64_t session=0;assert(!a->open(a->context,&request,&session));assert(!driver->quiesce());
  int32_t x,y;assert(canvas_provider_touch_map(&provider,0,0,8,8,&x,&y));
  const int32_t corners[4][2]={{0,0},{7,0},{7,7},{0,7}};assert(x==corners[touch_rotation][0]&&y==corners[touch_rotation][1]);
  driver->stop();assert(provider.frames.session==session);assert(!a->close(a->context,session));assert(driver->quiesce());driver->stop();
 }
 assert(driver->start(deps,2));const risc_canvas_api_v1*a=driver->capability;risc_canvas_open_v1 request={sizeof(request),1,3,0,0};uint64_t session=0;assert(!a->open(a->context,&request,&session));
 invocation_live=false;risc_canvas_geometry_v1 geometry={.struct_size=sizeof(geometry)};assert(a->geometry(a->context,session,&geometry)==RISC_CANVAS_RETAINED);assert(!driver->quiesce());driver->stop();assert(provider.frames.retained);
 puts("Sixteen native entry profile combinations, dependency refusal and Runtime authority fence passed");
}
