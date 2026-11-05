/* Production Settings save helper and controller composed with the board RTC.
 * No physical bus, oscillator timing, target instructions or flash are tested. */
#ifdef TEST_FIRST_ALARM
#define TEST_SETTINGS_CUSTOM_RUNTIME
#define PORTABLE_ALARM_CLIENT
#define PORTABLE_SETTINGS_CLOCK_CHANGE_CONTROL
#endif
#define main settings_fixture_main
#include "portable_settings_test.c"
#undef main
extern const twatch_rtc_api_v1 *rtc_first_open(const twatch_rtc_time_v1 *,unsigned);
extern void rtc_first_fault(unsigned,bool);
extern unsigned rtc_first_writes(void),rtc_first_reads(void);
extern void rtc_first_close(void);
extern void rtc_first_fault_after(unsigned,unsigned);
#ifdef TEST_FIRST_ALARM
extern const alarm_service_v1 *rtc_first_alarm_open(const twatch_rtc_api_v1 *,unsigned);
extern void rtc_first_alarm_expect(unsigned,uint32_t),rtc_first_alarm_unannounced(void),rtc_first_alarm_close(void);
#endif
static const twatch_rtc_api_v1 *real_rtc;
static unsigned initial_invalid;

static bool real_read(void *context,twatch_rtc_time_v1 *out) {
    (void)context;
    if(!real_rtc)real_rtc=rtc_first_open(&stored,initial_invalid);
    if(writes)reads_after_write++;
    return real_rtc->read(real_rtc->context,out);
}
static bool real_write(void *context,const twatch_rtc_time_v1 *in) {
    (void)context;assert(real_rtc);
    writes++;written=*in;
    return real_rtc->write(real_rtc->context,in);
}
#ifdef TEST_FIRST_ALARM
static bool alarm_test_acquire(const char *name,uint32_t version,uint64_t id,risc_runtime_capability_v1 *grant) {
    if(!strcmp(name,ALARM_SERVICE_CAPABILITY)) {
        assert(version==1 && !id);
        if(!real_rtc)real_rtc=rtc_first_open(&stored,initial_invalid);
        grant->api=rtc_first_alarm_open(real_rtc,initial_invalid);
        if(initial_invalid) {
            /* A cold invalid clock has an existing RTC-error modal. Dismiss it
             * with Back, never Retry, before the first valid editor Save. */
            for(size_t i=0;i<input_count;i++)input_script[i].poll+=6;
            tap(3,100,170);
        }
        grants++;return true;
    }
    return test_acquire(name,version,id,grant);
}
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version) {
    static risc_runtime_api_v1 composed;
    composed=runtime_api;composed.acquire=alarm_test_acquire;
    return version==1?&composed:NULL;
}
#endif
static void verify_calendar(const twatch_rtc_time_v1 *expected) {
    twatch_rtc_time_v1 got={0};
    assert(real_rtc->read(real_rtc->context,&got));
    assert(!memcmp(&got,expected,sizeof(got)));
}
int main(int argc,char **argv) {
    assert(argc==3);
    unsigned invalid=(unsigned)strtoul(argv[2],NULL,10);
    assert(invalid<=3);
    if(!strcmp(argv[1],"ui")) {
        initial_invalid=invalid;
        rtc_api.read=real_read;rtc_api.write=real_write;
        char *args[]={argv[0],invalid?"2":"0",NULL};
        assert(settings_fixture_main(2,args)==0);
        assert(writes==1 && rtc_first_writes()==1 && reads_after_write>=1);
        assert(!strcmp(settings_message,"Saved to RTC"));
        verify_calendar(&written);
#ifdef TEST_FIRST_ALARM
        rtc_first_alarm_expect(1,calendar_seconds(&written));rtc_first_alarm_close();
#endif
        rtc_first_close();
        printf("first RTC Save: real UI, one write, checked readback, initial=%u PASS\n",invalid);
        return 0;
    }
    const twatch_rtc_time_v1 initial={2024,2,29,4,23,59,59};
    const twatch_rtc_time_v1 desired={2026,10,16,5,20,47,31};
    real_rtc=rtc_first_open(&initial,invalid);rtc=real_rtc;rt=&runtime_api;
    twatch_rtc_time_v1 got={0};
    assert(real_rtc->read(NULL,&got)==(invalid==0));
#ifdef TEST_FIRST_ALARM
    alarms.api=rtc_first_alarm_open(real_rtc,invalid);
    if(!strcmp(argv[1],"unannounced")) {
        assert(real_rtc->write(NULL,&desired));
        rtc_first_alarm_unannounced();
    }
#endif
    unsigned writes_before=rtc_first_writes();
    unsigned failures=0;bool fail_write=false,fail_alarm=false;
    if(!strcmp(argv[1],"retry-read"))failures=1;
    else if(!strcmp(argv[1],"fail-read"))failures=4;
    else if(!strcmp(argv[1],"fail-write"))fail_write=true;
    else if(!strcmp(argv[1],"fail-alarm-read"))fail_alarm=true;
    else assert(!strcmp(argv[1],"save") || !strcmp(argv[1],"unannounced"));
    rtc_first_fault(failures,fail_write);
    if(fail_alarm)rtc_first_fault_after(1,1);
    bool saved=editor_save_raw(&desired);
    assert(saved==(!fail_write && failures<4 && !fail_alarm));
    assert(rtc_first_writes()==writes_before+1); /* No hidden second RTC write or Save. */
    if(saved)verify_calendar(&desired);
    else if(fail_alarm)assert(!strcmp(editor_message,"RTC saved; alarm sync failed"));
    else if(fail_write)assert(!strcmp(editor_message,"Write failed - check RTC"));
    else assert(!strcmp(editor_message,"Verify failed - check RTC"));
    assert(ticks<=60);
#ifdef TEST_FIRST_ALARM
    if(saved)rtc_first_alarm_expect(1,calendar_seconds(&desired));
    rtc_first_alarm_close();
#endif
    rtc_first_close();
    printf("first RTC Save: %s initial=%u writes=1 bounded-readback PASS\n",argv[1],invalid);
    return 0;
}
