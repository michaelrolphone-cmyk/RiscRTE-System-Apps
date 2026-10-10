/* Production GT911 + shared consumer. Physical I/O comes from product fixture. */
#define main x4_gt911_provider_suite_main
#include X4_GT911_FIXTURE
#undef main
#define PORTABLE_NATIVE_CUSTODY_FENCE 1
#define PORTABLE_STAGE_LOGS 1
#include <PortableTouch.h>
static unsigned retained_calls,issues;
static const char *last_operation;
static int last_status,last_terminal;
void portable_adapter_retain(void) { ++retained_calls; }
void portable_adapter_touch_issue(const char *operation,int result,int terminal) {
    if(!operation)return;
    ++issues;last_operation=operation;last_status=result;last_terminal=terminal;
}
static int32_t provider_fault(void*c,uint64_t sub,risc_touch_event_v1*out){(void)c;(void)sub;(void)out;return -2;}
static bool snapshot_fault(void*c,risc_touch_snapshot_v1*out){(void)c;(void)out;return false;}
static bool refuse_release(risc_runtime_capability_v1 *grant){(void)grant;return false;}
int main(int argc,char **argv) {
    assert(argc==2);const char *scenario=argv[1];
    driver=t5_driver_get(2);assert(driver);api=driver->capability;assert(start());
    portable_touch t={.api=api,.subscription=api->subscribe(NULL)};
    uint64_t other=api->subscribe(NULL);assert(t.subscription&&other);
    portable_touch_sample s;
    packet(0,0,0,false);portable_touch_read(&t,&s);assert(s.valid&&t.neutral);
    packet(1,120,300,false);portable_touch_read(&t,&s);assert(s.began&&s.tap_eligible);
    bool terminal=false;
    risc_touch_api_v1 injected=*api;
    if(!strcmp(scenario,"provider-fault")){injected.next=provider_fault;t.api=&injected;terminal=true;}
    else if(!strcmp(scenario,"snapshot-failed")){injected.snapshot=snapshot_fault;t.api=&injected;terminal=true;}
    else if(!strcmp(scenario,"two-contact"))packet(2,121,301,true);
    else if(!strcmp(scenario,"out-of-range"))packet(1,480,301,true);
    else if(!strcmp(scenario,"queue-gap"))for(unsigned i=0;i<RISC_TOUCH_QUEUE_LENGTH+1;i++){packet(1,121+i,301,true);assert(api->poll(NULL,1));}
    else if(!strcmp(scenario,"transient-read")){packet(1,121,301,true);read_ok=false;}
    else if(!strcmp(scenario,"ambiguous-ack")){packet(1,121,301,true);ack_ok=false;}
    else if(!strcmp(scenario,"unlock-retained")){packet(1,121,301,true);unlock_ok=false;terminal=true;}
    else if(!strcmp(scenario,"close-refused")){
        risc_runtime_api_v1 rt={.release=refuse_release};t.grant.api=api;
        assert(!portable_touch_close(&t,&rt)&&t.subscription==0&&t.grant.api==api);
        assert(last_terminal&& !strcmp(last_operation,"release"));
        assert(api->unsubscribe(NULL,other));assert(driver->quiesce());
        puts("close-refused: ownership remains visible; first-cause diagnostic preserved");return 0;
    }else assert(!"unknown scenario");
    portable_touch_read(&t,&s);read_ok=ack_ok=unlock_ok=true;
    assert(s.cancelled&&!s.home_pressed&&!s.released&&!s.tap_eligible);
    assert(issues&&last_terminal==terminal);
    if(terminal){assert(retained_calls==1);assert((!strcmp(last_operation,"next")&&last_status==-2)||(!strcmp(last_operation,"snapshot")&&last_status==0));puts("unlock-retained: genuine provider retention preserved");return 0;}
    assert(!retained_calls&&!t.neutral&&!t.down);
    risc_touch_snapshot_v1 healthy={0};assert(api->snapshot(NULL,&healthy));
    /* Inherited held contact/Home cannot become a tap or a Home edge. */
    for(unsigned i=0;i<3;i++){packet(1,125,310,true);portable_touch_read(&t,&s);assert(!s.home_pressed&&!s.tap_eligible&&!s.released);}
    packet(0,0,0,false);portable_touch_read(&t,&s);assert(!s.tap_eligible&&!s.home_pressed);
    packet(0,0,0,false);portable_touch_read(&t,&s);assert(t.neutral&&t.home_neutral);
    packet(1,125,310,true);portable_touch_read(&t,&s);assert(s.began&&s.tap_eligible&&s.home_pressed);
    packet(0,0,0,false);portable_touch_read(&t,&s);assert(s.released&&s.tap_eligible&&!s.moved&&!s.home_pressed);
    assert(!retained_calls);assert(api->unsubscribe(NULL,other));done(t.subscription);
    printf("%s: report-loss cancellation, neutral rearm, fresh touch/Home, clean ownership PASS\n",scenario);
}
