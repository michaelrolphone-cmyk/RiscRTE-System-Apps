/* Exact production GT911 with the original strict scoped transport fixture.
 * The controller model is independently timed and latched until STATUS ACK. */
#define main original_gt911_fixture_main
#define t5_driver_get scene_timing_gt911_get
#include "gt911_test.c"
#undef t5_driver_get
#undef main
extern uint64_t scene_timing_us(void);
extern void scene_timing_advance(unsigned);
extern bool scene_timing_report(uint8_t *,uint16_t *,uint16_t *);
extern void scene_timing_ack(void);
static bool timing_transact(void*c,uint64_t id,const uint8_t*w,size_t wn,uint8_t*r,size_t rn,uint32_t timeout){
    if(wn==2&&w[0]==0x81&&w[1]==0x4e){
        uint8_t n;uint16_t x,y;
        if(scene_timing_report(&n,&x,&y)){status=(uint8_t)(0x80u|n);memset(raw,0,sizeof(raw));wire_point(0,0,x,y);}
        else status=0;
    }
    bool ok=transact(c,id,w,wn,r,rn,timeout);
    if(ok&&wn==3&&w[0]==0x81&&w[1]==0x4e)scene_timing_ack();
    return ok;
}
static uint64_t timing_now(void*c){(void)c;require_locked();return scene_timing_us()/1000;}
static void timing_sleep(void*c,uint32_t ms){(void)c;require_locked();scene_timing_advance(ms*1000);}
const risc_touch_api_v1 *scene_timing_touch_api(void){
    driver=scene_timing_gt911_get(2);assert(driver);return driver->capability;
}
const risc_touch_api_v1 *scene_timing_touch_start(void){
    driver=scene_timing_gt911_get(2);assert(driver);api=driver->capability;power=risc_touch_power(api);assert(power);
    bus.base.transact=timing_transact;clock_api.monotonic_ms=timing_now;clock_api.sleep_ms=timing_sleep;
    assert(start());return api;
}
bool scene_timing_touch_quiesce(void){return driver->quiesce();}
void scene_timing_touch_stop(void){clean();driver->stop();}
