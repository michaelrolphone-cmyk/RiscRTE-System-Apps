/* Foreground-only modal, compiled solely for deployments selecting alarm.service.
 * Blocking here retains the caller's actual stack, model and private grants.
 * Its saved frame is app-owned; no lease or callback crosses service calls. */
#include "PortableAlarmClient.h"
static portable_alarm_client alarms;
#ifdef PORTABLE_APP_SLEEP_LOCAL
static const alarm_service_v1 *alarm_sleep_api(void) { return alarms.api; }
#endif
static bool alarm_error_seen,alarm_failed_cleaned;
static int32_t alarm_seen_error;
static uint32_t alarm_seen_snapshot;
static void alarm_cancel_input(void) {
    input_pending=false;input_sample=(portable_touch_sample){0};navigation_pending=0;
    touch.neutral=touch.down=false;
#ifdef PORTABLE_INPUT_NAVIGATION
    input_navigation_reset();
#endif
}
static bool alarm_failure(void) {
    failed=true;
    if(native_sleep_retained)return false;
    if(alarm_failed_cleaned)return false;
    alarm_failed_cleaned=true;
    if(!portable_alarm_failure_stop(&alarms)) {
        rt->diagnostic("ALARM foreground-failed output-stop-unconfirmed; invocation retained");
        /* No normal I/O, grant release, graph handoff, or repeated cleanup. */
        for(;;)rt->yield_ms(50);
    }
    return false;
}
static void alarm_draw(void) {
    const alarm_status_v1 *s=&alarms.status;
    clear();
    label(8,38,width()-16,s->occurrence.kind==ALARM_KIND_COUNTDOWN?"COUNTDOWN FINISHED":"ALARM");
    label(8,78,width()-16,s->state==ALARM_STATE_BLOCKED?
        (s->error==ALARM_RTC?"RTC ERROR":"SERVICE ERROR"):
        s->state==ALARM_STATE_DISMISSING?"STOPPING / SAVING":"TIME IS UP");
    label(8,108,width()-16,s->output_uncertain?"OUTPUT STOP UNCONFIRMED":"");
    label(8,height()-67,width()-16,s->occurrence.generation?"DISMISS":"BACK");
    if(s->state==ALARM_STATE_BLOCKED)label(8,height()-27,width()-16,"RETRY");
    present(false);
}
static bool alarm_save_frame(uint16_t **saved) {
    *saved=NULL;
    /* Preserve the settled foreground image. Unlike retained handoff this
       reads our own completed-frame copy, never a provider-retained promise. */
    if(!alarm_pixels || !alarm_pixels_valid)return false;
    *saved=alarm_pixels;alarm_modal=true;return true;
}
static bool alarm_restore(const uint16_t *saved) {
    clear();if(failed)return false;
    for(unsigned y=0;y<info.height;y++)memcpy((uint8_t *)surface.pixels+(size_t)y*surface.stride_bytes,
        saved+(size_t)y*info.width,info.width*2);
    present(false);return !failed && display_settled;
}
/* Returns false only on foreground failure; consumed prevents initiating Back
 * or touch from leaking into the suspended app after a modal closes. */
static bool alarm_foreground(bool *consumed) {
    *consumed=false;
    if(failed || !display_settled || surface.frame)return alarm_failure();
    if(!portable_alarm_pump(&alarms)){failed=true;return alarm_failure();}
    /* Reconciliation is bounded by service phases, not provider poll. Input
       waits until the snapshot says READY or gives an alert/error. */
    for(unsigned n=0;alarms.status.state==ALARM_STATE_LOADING;n++) {
        if(n>=64){failed=true;return alarm_failure();}
        rt->yield_ms(1);
        if(!portable_alarm_pump(&alarms)){failed=true;return alarm_failure();}
    }
    bool error=alarms.status.state==ALARM_STATE_BLOCKED;
    if(!portable_alarm_owned(&alarms) && (!error ||
        (alarm_error_seen && alarm_seen_error==alarms.status.error && alarm_seen_snapshot==alarms.status.snapshot)))return true;
    uint16_t *saved;
    if(!alarm_save_frame(&saved)){failed=true;return alarm_failure();}
    *consumed=true;alarm_cancel_input();
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
    handoff_finish();
#endif
    alarm_token_v1 shown=alarms.status.occurrence;
    bool redraw=true;
    for(;;) {
        if(redraw){alarm_draw();redraw=false;}
        if(failed || !display_settled){alarm_modal=false;return alarm_failure();}
        rt->yield_ms(20);input_service();
        if(failed){alarm_modal=false;return alarm_failure();}
        portable_touch_sample sample;input_take(&sample);
        uint32_t buttons=navigation_pending;navigation_pending=0;
        bool tap=sample.valid && sample.released && sample.tap_eligible && !sample.moved && !sample.cancelled;
        bool retry=(buttons&T5_APP_BUTTON_CONFIRM) || (tap && sample.y>=info.height-42);
        bool dismiss=(buttons&T5_APP_BUTTON_BACK) || (tap && sample.y>=info.height-88 && sample.y<info.height-42);
        if(alarms.status.state==ALARM_STATE_BLOCKED && retry) {
            (void)alarms.api->refresh(alarms.api->context);alarm_cancel_input();redraw=true;
        } else if(dismiss) {
            if(alarms.status.occurrence.generation) {
                /* Copy exact current token; service rejects stale identities. */
                alarm_token_v1 token=alarms.status.occurrence;
                (void)alarms.api->acknowledge(alarms.api->context,&token);
                alarm_cancel_input();redraw=true;
            } else if(alarms.status.state==ALARM_STATE_BLOCKED && !alarms.status.output_uncertain) {
                alarm_error_seen=true;alarm_seen_error=alarms.status.error;alarm_seen_snapshot=alarms.status.snapshot;break;
            }
        }
        if(failed || !display_settled || surface.frame){alarm_modal=false;return alarm_failure();}
        uint32_t old_state=alarms.status.state;int32_t old_error=alarms.status.error;
        if(!portable_alarm_pump(&alarms)){failed=true;alarm_modal=false;return alarm_failure();}
        if(memcmp(&shown,&alarms.status.occurrence,sizeof(shown))) {
            shown=alarms.status.occurrence;alarm_cancel_input();redraw=true;
        }
        if(old_state!=alarms.status.state || old_error!=alarms.status.error)redraw=true;
        /* Only READY after durable ACK/expiry and safe output close restores
           the caller. A new simultaneous due identity stays in the modal. */
        if(alarms.status.state==ALARM_STATE_READY && !portable_alarm_owned(&alarms))break;
    }
    alarm_cancel_input();
    bool ok=alarm_restore(saved);alarm_modal=false;
    if(!ok)return alarm_failure();
    alarm_cancel_input(); /* restore presentation may have sampled contact */
    return failed?alarm_failure():true;
}
