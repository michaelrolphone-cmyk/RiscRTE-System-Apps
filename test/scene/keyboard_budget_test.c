/* Real presenter clock/custody boundaries for timed keyboard raster batches. */
#define SCENE_EMBED_KEYBOARD_TEST
#include "keyboard_test.c"
#undef SCENE_EMBED_KEYBOARD_TEST
static unsigned clock_mode,clock_calls;
static uint64_t budget_clock(void *context){
    (void)context;io("budget-clock");++clock_calls;
    if(clock_mode==1)return UINT64_MAX;
    if(clock_mode==2)return clock_calls==1?ms+10:ms;
    return ms;
}
int main(int argc,char **argv){
    assert(argc==2);clock_mode=(unsigned)atoi(argv[1]);assert(clock_mode>=1&&clock_mode<=3);
    w=800;h=480;format=1;prof=(risc_scene_profile_v1){1,sizeof(prof),1,3,88,20,0,65535,0,270,0,0};
    risc_platform_clock_api_v1 clock=clk;clock.monotonic_ms=budget_clock;
    driver=t5_driver_get(2);assert(driver);api=driver->capability;
    risc_provider_dependency_v1 deps[]={{"display.output",1,&disp},{"input.touch.raw",1,&raw},{"input.navigation",1,&nav},{"platform.clock",1,&clock},{"ui.presentation-profile",1,&prof}};
    assert(driver->start(deps,5));make_document();assert(!api->open(NULL,&doc,NULL,&session));keyboard_document();
    if(clock_mode==3)loss_callback="budget-clock";
    risc_scene_event_v1 event;assert(tick(&event)==RISC_SCENE_RETAINED);
    assert(held&&!presenting&&!submits&&clock_calls==(clock_mode==2?2u:1u));
    unsigned before=calls;native_alive=true;
    assert(tick(&event)==RISC_SCENE_RETAINED&&api->close(NULL,session)==RISC_SCENE_RETAINED);
    assert(!driver->quiesce());driver->stop();assert(calls==before);
    puts("keyboard raster clock/custody failure stays retained PASS");return 0;
}
