/* Actual selected GT911 driver, using the product's strict physical-I/O fixture. */
#define main x4_gt911_provider_suite_main
#include X4_GT911_FIXTURE
#undef main
void hid_renderer_watch_report(risc_touch_snapshot_v1 *sample);
uint64_t hid_renderer_watch_millis(void);
const char *hid_recovery_scenario(void);
unsigned hid_recovery_poll(void);
static bool injecting_queue;
static bool hid_transact(void*c,uint64_t id,const uint8_t*w,size_t wn,uint8_t*r,size_t rn,uint32_t timeout) {
    if(wn==2&&w[0]==0x81&&w[1]==0x4e&&!injecting_queue){
        risc_touch_snapshot_v1 s={0};hid_renderer_watch_report(&s);
        const char *scenario=hid_recovery_scenario();unsigned poll=hid_recovery_poll();
        bool fault=poll>=30&&poll<=34;
        read_ok=!(fault&&!strcmp(scenario,"transient-read"));
        ack_ok=!(fault&&!strcmp(scenario,"ambiguous-ack"));
        if(fault&&(!strcmp(scenario,"two-contact")||strstr(scenario,"close-refused")))s.contact_count=2;
        if(fault&&!strcmp(scenario,"out-of-range")){s.contact_count=1;s.contacts[0].x=480;}
        packet(s.contact_count,s.contacts[0].x,s.contacts[0].y,!!(s.buttons&RISC_TOUCH_BUTTON_PRIMARY));
    }
    return transact(c,id,w,wn,r,rn,timeout);
}
void hid_recovery_unlock_fault(void){unlock_ok=false;(void)api->poll(NULL,1);}
void hid_recovery_queue_loss(void){
    injecting_queue=true;
    for(unsigned i=0;i<RISC_TOUCH_QUEUE_LENGTH+1;i++){packet(1,150+i,350,true);assert(api->poll(NULL,1));}
    injecting_queue=false;
}
static uint64_t hid_now(void*c){(void)c;return hid_renderer_watch_millis();}
const risc_touch_api_v1 *hid_watch_touch_start(void){
    driver=t5_driver_get(2);assert(driver);api=driver->capability;power=risc_touch_power(api);assert(power);
    bus.base.transact=hid_transact;clock_api.monotonic_ms=hid_now;assert(start());return api;
}
void hid_watch_touch_stop(void){done(0);}
