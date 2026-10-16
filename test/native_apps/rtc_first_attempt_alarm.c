/* Actual alarm service with only storage/monotonic/output dependencies modeled. */
#define t5_driver_get rtc_first_alarm_driver_get
#include ALARM_PRODUCTION_SOURCE
#include <assert.h>
#include <string.h>
#include <stdio.h>
static uint64_t model_ms;
static unsigned model_outputs,model_puts;
static uint8_t empty_points[POINTS_RECORD_SIZE];
static int32_t model_get(void *c,const char *k,void *b,uint32_t n,uint32_t *size) {
    (void)c;*size=0;
    if(!strcmp(k,POINTS_CONFIG_KEY)) {
        assert(n>=sizeof(empty_points));memcpy(b,empty_points,sizeof(empty_points));
        *size=sizeof(empty_points);return RISC_BOUND_KEY_VALUE_OK;
    }
    return RISC_BOUND_KEY_VALUE_NOT_FOUND;
}
static int32_t model_put(void *c,const char *k,const void *b,uint32_t n) {
    (void)c;(void)k;(void)b;(void)n;model_puts++;return ALARM_OK;
}
static uint64_t model_mono(void *c) {(void)c;return model_ms;}
static bool model_effect(void *c,uint8_t e) {(void)c;(void)e;model_outputs++;return true;}
static bool model_success(void *c) {(void)c;return true;}
static bool model_audio_open(void *c,uint32_t r,uint8_t n) {(void)c;(void)r;(void)n;model_outputs++;return true;}
static bool model_audio_write(void *c,const int16_t *p,size_t n) {(void)c;(void)p;(void)n;model_outputs++;return true;}
static bool model_audio_gain(void *c,uint16_t g,uint16_t m) {(void)c;(void)g;(void)m;return true;}
static const risc_bound_key_value_v1 model_storage={1,sizeof(model_storage),NULL,model_get,model_put};
static const risc_platform_clock_api_v1 model_clock={1,sizeof(model_clock),NULL,model_mono,NULL};
static const twatch_haptic_api_v1 model_haptic={1,sizeof(model_haptic),NULL,model_effect,model_success};
static const twatch_audio_out_api_v1 model_audio={1,sizeof(model_audio),NULL,model_audio_open,model_audio_write,model_audio_gain,model_success,model_success};
static const alarm_service_v1 *model_client;
static alarm_status_v1 model_status(void) {
    alarm_status_v1 status={.struct_size=sizeof(status)};
    assert(model_client->status(NULL,&status)==ALARM_OK);return status;
}
void rtc_first_alarm_pump(void) {
    for(unsigned i=0;i<200;i++){model_client->step(NULL);model_ms++;}
}
const alarm_service_v1 *rtc_first_alarm_open(const twatch_rtc_api_v1 *provider,unsigned invalid) {
    model_ms=0;model_outputs=model_puts=0;
    points_config no_points={.revision=1};points_config_encode(&no_points,empty_points);
    risc_provider_dependency_v1 deps[]={
        {"storage.key-value.bound",1,&model_storage},{"platform.clock",1,&model_clock},
        {"rtc.clock",2,provider},{"haptic.effect",1,&model_haptic},{"audio.output",1,&model_audio}};
    const risc_driver_v2 *descriptor=rtc_first_alarm_driver_get(2);
    assert(descriptor && descriptor->start(deps,5));model_client=descriptor->capability;
    rtc_first_alarm_pump();
    fprintf(stderr,"alarm open invalid=%u state=%u error=%d phase=%u\n",invalid,model_status().state,model_status().error,phase);
    assert(model_status().state==(invalid?ALARM_STATE_BLOCKED:ALARM_STATE_READY));
    assert(!invalid || model_status().error==ALARM_RTC);
    assert(!model_outputs&&!model_puts);return model_client;
}
void rtc_first_alarm_expect(unsigned ready,uint32_t expected) {
    rtc_first_alarm_pump();
    alarm_status_v1 status=model_status();
    assert(status.state==(ready?ALARM_STATE_READY:ALARM_STATE_BLOCKED));
    if(ready)assert(status.rtc_seconds==expected && !status.error);
    else assert(status.error==ALARM_RTC);
    assert(!model_outputs&&!model_puts);
}
void rtc_first_alarm_unannounced(void) {model_ms+=1001;rtc_first_alarm_expect(0,0);}
void rtc_first_alarm_close(void) {assert(rtc_first_alarm_driver_get(2)->quiesce());}
