/* A real sub-500ms foreground CUE must never become a calibration tap. */
#define PORTABLE_TAP_SETTINGS
#define PORTABLE_ALARM_FIXTURE_MAIN original_alarm_fixture_main
#include "portable_alarm_test.c"
static bool tap_live,injected;
static unsigned observe_calls,calls_at_cue,restore_calls;
static uint32_t cue_at;
static bool long_health(risc_runtime_health_v1*h){h->uptime_ms=ticks;return polls<10000;}
static bool motion_info(void*c,twatch_tap_info_v1*out){(void)c;*out=(twatch_tap_info_v1){sizeof(*out),1,0,7,3};return true;}
static bool motion_configure(void*c,uint16_t value){(void)c;(void)value;return true;}
static bool motion_begin(void*c,uint16_t value){(void)c;assert(value==7&&!tap_live);tap_live=true;return true;}
static bool motion_resume(void*c){(void)c;assert(tap_live);tap_live=false;restore_calls++;return true;}
static bool motion_observe(void*c,twatch_tap_observation_v1*out){
 (void)c;assert(tap_live);observe_calls++;
 /* If the controller incorrectly reads after arbitration, the cue's physical
  * disturbance would look like a measured double tap. It must not read it. */
 *out=(twatch_tap_observation_v1){.struct_size=sizeof(*out),.double_tap=injected,.sample_ready=true,.z_mg=1000};return true;
}
static const twatch_motion_api_v1 motion_api={.api_version=1,.struct_size=sizeof(motion_api),.resume_wake=motion_resume,
 .tap_info=motion_info,.tap_configure=motion_configure,.tap_observe_begin=motion_begin,.tap_observe=motion_observe};
static bool acquire_tap(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){
 if(strcmp(n,TWATCH_MOTION_CAPABILITY))return acquire_alarm(n,v,id,g);
 assert(v==1&&!id);g->api=&motion_api;g->slot=200;grants++;return true;
}
static bool release_tap(risc_runtime_capability_v1*g){if(g->api==&motion_api)assert(!tap_live);return test_release(g);}
static void cue_yield(uint32_t ms){
 test_yield(ms);
 if(settings_motion_active && tap_calibration.phase==TAP_CAL_PAIR && !injected){
  injected=true;cue_at=ticks;calls_at_cue=observe_calls;alarm_fake.state=ALARM_STATE_CUE;alarm_fake.output_uncertain=1;
 }
}
int main(void){
 scenario=100;alarm_scenario=13;
 risc_runtime_api_v1 r=runtime_api;r.health=long_health;r.acquire=acquire_tap;r.release=release_tap;r.yield_ms=cue_yield;
 rt=&r;dg.struct_size=sizeof(dg);bg.struct_size=sizeof(bg);
 assert(acquire_alarm("display.output",1,0,&dg));display=dg.api;assert(display_info(NULL,&info));
 assert(portable_touch_open(&touch,&r));failed=false;display_settled=true;
 alarm_pixels=malloc(sizeof(framebuffer));assert(alarm_pixels);assert(portable_alarm_open(&alarms,&r));alarm_fake.state=ALARM_STATE_READY;
 assert(settings_open());clear();fill(0,0,240,240,0x1234);present(false);assert(alarm_pixels_valid);sv_active=true;
 assert(!settings_tap_measure());
 assert(injected && ticks-cue_at<500 && settings_motion_interrupted && !alarm_modal && !failed);
 assert(observe_calls==calls_at_cue && !tap_calibration.pairs && tap_calibration.phase==TAP_CAL_ERROR);
 assert(restore_calls==1 && !tap_live && !tap_grant.api && !settings_motion_active && !tap_cleanup_pending);
 assert(!strcmp(tap_message,"Calibration interrupted") && !format_writes && !native_sleep_calls && !acks);
 app_module_fini();assert(!grants);
 puts("Actual short alarm CUE arbitration cancels calibration before reading a hardware tap, restores once and writes nothing PASS");
}
