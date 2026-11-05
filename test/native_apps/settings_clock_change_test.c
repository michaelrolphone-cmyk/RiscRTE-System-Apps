/* Real Settings RTC save helper, with the explicit alarm boundary. */
#define PORTABLE_ALARM_CLIENT
#define PORTABLE_SETTINGS_CLOCK_CHANGE_CONTROL
#define main original_settings_main
#include "portable_settings_test.c"
#undef main
static unsigned clock_changes;
static uint32_t clock_seconds;
static int32_t change_result=ALARM_PENDING;
static int32_t changed(void *c,uint32_t seconds) {
    (void)c;assert(writes==1&&reads_after_write>=1);clock_changes++;clock_seconds=seconds;return change_result;
}
int main(void) {
    alarm_clock_change_v1 control={.sleep.base={.api_version=1,.struct_size=sizeof(control)},
        .tag=ALARM_CLOCK_CHANGE_TAG,.version=ALARM_CLOCK_CHANGE_VERSION,.clock_changed=changed};
    rt=&runtime_api;rtc=&rtc_api;alarms.api=&control.sleep.base;
    twatch_rtc_time_v1 value={2026,10,12,1,12,30,0};
    assert(editor_save_raw(&value));assert(writes==1&&clock_changes==1&&clock_seconds==calendar_seconds(&value));
    writes=reads_after_write=clock_changes=0;control.tag^=1;
    assert(!editor_save_raw(&value)&&!writes&&!clock_changes);
    assert(!strcmp(editor_message,"Alarm service update required"));control.tag^=1;
    scenario=3;assert(!editor_save_raw(&value)&&writes==1&&!clock_changes);
    writes=reads_after_write=0;scenario=11;assert(!editor_save_raw(&value)&&writes==1&&!clock_changes);
    writes=reads_after_write=0;scenario=0;change_result=ALARM_BUSY;
    assert(!editor_save_raw(&value)&&writes==1&&clock_changes==1);
    assert(!strcmp(editor_message,"RTC saved; alarm sync failed"));
    puts("Settings real save: exactly one write, verified clock-change call, incompatible service blocks before write, RTC failures and alarm-boundary failure remain visible PASS");
    return 0;
}
