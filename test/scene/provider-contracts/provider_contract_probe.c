/* Reuse exact existing fake devices/assertions; execute real scene provider. */
#define main historical_fixture_main
#include HOST_FIXTURE
#undef main
#include "RiscScenePageV1.h"
int main(int argc,char **argv){
 assert(argc==4);char *end;unsigned long wi=strtoul(argv[1],&end,10);assert(*argv[1]&&!*end);unsigned long he=strtoul(argv[2],&end,10);assert(*argv[2]&&!*end);assert(wi>=160&&he>=160&&wi*he<=800u*480u);w=(unsigned)wi;h=(unsigned)he;
 const char *mode=argv[3];driver=t5_driver_get(2);assert(driver);api=driver->capability;
 risc_provider_dependency_v1 deps[]={{"display.output",1,&disp},{"input.touch.raw",1,&raw},{"platform.clock",1,&clk},{"ui.presentation-profile",1,&prof}};
 if(!strncmp(mode,"missing-",8)){
  unsigned omit=(unsigned)strtoul(mode+8,&end,10);assert(!*end&&omit<4);for(unsigned i=omit;i<3;i++)deps[i]=deps[i+1];unsigned before=calls;assert(!driver->start(deps,3));assert(calls==before);printf("required dependency %u absent: clean refusal\n",omit);return 0;
 }
 assert(driver->start(deps,4));assert(!calls);make_document();int opened=api->open(NULL,&doc,NULL,&session);
 if(!strcmp(mode,"compact-reject")){assert(opened==RISC_SCENE_UNAVAILABLE&&!session&&!subscribed&&!held);assert(driver->quiesce());driver->stop();puts("explicit compact provider rejects incompatible geometry");return 0;}
 assert(opened==RISC_SCENE_OK&&session);const risc_scene_page_api_v1 *page_api=risc_scene_page_get_v1(api);assert(page_api);risc_scene_page_geometry_v1 geometry={.struct_size=sizeof geometry};assert(page_api->geometry(NULL,&geometry)==0&&geometry.width>0&&geometry.width<=w&&geometry.height>0&&geometry.height<=h);
 if(!strcmp(mode,"absent-navigation")){settle();unsigned top=prof.font_scale*7+prof.padding*2+8;touch_tap(w/2,top+prof.row_height+10);risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_IDLE);risc_scene_navigation_v1 state={.struct_size=sizeof state};uint32_t flags;assert(api->snapshot(NULL,session,&state,&flags)==0&&state.depth==2);settle();finish();}
 else if(!strcmp(mode,"busy-close")){allow_complete=false;begin_frame();assert(api->close(NULL,session)==RISC_SCENE_AGAIN&&!driver->quiesce());allow_complete=true;finish();}
 else if(!strcmp(mode,"gap-recovery")){settle();queue_gap=true;held_input=true;risc_scene_event_v1 e;assert(tick(&e)==RISC_SCENE_IDLE);
  unsigned top=prof.font_scale*7+prof.padding*2+8;touch_tap(w/2,top+prof.row_height+10);assert(tick(&e)==RISC_SCENE_IDLE);
  risc_scene_navigation_v1 state={.struct_size=sizeof state};uint32_t flags;assert(api->snapshot(NULL,session,&state,&flags)==0&&state.depth==1);
  held_input=false;settle();touch_tap(w/2,top+prof.row_height+10);assert(tick(&e)==RISC_SCENE_IDLE);settle();
  assert(api->snapshot(NULL,session,&state,&flags)==0&&state.depth==2);finish();}
 else assert(0);
 /* submit transfers custody; release counts only cancelled unsubmitted surfaces. */
 assert(!held&&!presenting&&!subscribed);printf("%s %ux%u optional navigation absent PASS\n",mode,w,h);return 0;
}
