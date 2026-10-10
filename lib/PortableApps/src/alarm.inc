/* Foreground-only modal, compiled solely for deployments selecting alarm.service.
 * Blocking here retains the caller's actual stack, model and private grants.
 * Its saved frame is app-owned; no lease or callback crosses service calls. */
#include "PortableAlarmClient.h"
static portable_alarm_client alarms;
#if defined(PORTABLE_WIFI_SETTINGS_APP) && (defined(ALARM_SERVICE_TAGGED_V2) || defined(PORTABLE_ALARM_TERMINAL_RETENTION))
#define PORTABLE_WIFI_SERVICE_LEASE
static bool alarm_refresh_pending,alarm_ack_pending,alarm_service_deferred;
static alarm_token_v1 alarm_ack_token;
static bool alarm_commit_pending(void);
#endif
#if (defined(PORTABLE_APP_SLEEP_LOCAL) && !defined(PORTABLE_RESIDENT_SHELL_CLIENT)) || (defined(PORTABLE_QUICK_ACTIONS) && !defined(ALARM_SERVICE_TAGGED_V2))
static const alarm_service_v1 *alarm_sleep_api(void) { return alarms.api; }
#endif
#if (defined(PORTABLE_QUICK_ACTIONS) || defined(PORTABLE_CONTEXTS_CLIENT)) && defined(ALARM_SERVICE_TAGGED_V2)
static uint32_t alarm_output_modes(void) { return portable_alarm_output_modes(&alarms); }
#endif
#if (defined(PORTABLE_QUICK_ACTIONS) && (defined(ALARM_SERVICE_TAGGED_V2) || defined(PORTABLE_ALARM_TERMINAL_RETENTION))) || defined(PORTABLE_WIFI_SERVICE_LEASE)
#ifdef PORTABLE_WIFI_SERVICE_LEASE
static bool alarm_refresh(void) { alarm_refresh_pending=true;return alarm_commit_pending(); }
#else
static bool alarm_refresh(void) { return portable_alarm_refresh(&alarms); }
#endif
#endif
static bool alarm_error_seen,alarm_failed_cleaned;
static int32_t alarm_seen_error;
static uint32_t alarm_seen_snapshot;
static void alarm_cancel_input(void) {
#ifdef PORTABLE_QUICK_ACTIONS
    if(quick_modal)pqa_cancel_input(&quick.ui);
#endif
    input_pending=false;input_sample=(portable_touch_sample){0};navigation_pending=0;
    input_reset();
#ifdef PORTABLE_INPUT_NAVIGATION
    input_navigation_reset();
#endif
}
static bool alarm_failure(void) {
    failed=true;
    if(native_sleep_retained)return false;
#if defined(PORTABLE_BLE_BROADCAST) || defined(PORTABLE_CONTEXTS_CLIENT)
    if(!portable_background_stop())return false;
#endif
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
    portable_adapter_retain();return false;
#endif
#ifdef PORTABLE_RADIO_SESSION
    if(!portable_radio_suspend()) {
        rt->diagnostic("RADIO foreground-failed cleanup-unconfirmed; invocation retained");
        for(;;)rt->yield_ms(50);
    }
#endif
#ifdef PORTABLE_AUDIO_SESSION
    /* Runtime checks app exit safety before fini. Never defer live/uncertain
     * audio cleanup until fini or let alarm stop close another owner's stream. */
    if(!portable_audio_suspend()) {
        rt->diagnostic("AUDIO foreground-failed cleanup-unconfirmed; invocation retained");
        for(;;)rt->yield_ms(50);
    }
#endif
#if defined(PORTABLE_WIFI_SETTINGS_APP) || defined(PORTABLE_UPDATE_APP)
    /* Even healthy RF must drain before app_main returns: Runtime checks its
     * exit barrier before invoking fini, so deferring cleanup there deadlocks
     * an ordinary display/input failure behind our own live radio session. */
    bool wifi_stopped=portable_wifi_suspend();
#ifdef PORTABLE_WIFI_SETTINGS_APP
    /* A healthy pending stop is not a terminal cleanup failure. Keep the
     * owner alive and poll copied completion between bounded cancel attempts;
     * no service lease is held over this yield. Never release on a deadline. */
    while(!wifi_stopped && portable_wifi_stop_pending()) {
        rt->yield_ms(1);
        if(native_sleep_retained)return false;
        if(!portable_wifi_services_safe())break;
        wifi_stopped=portable_wifi_suspend();
    }
#endif
    if(!wifi_stopped) {
        rt->diagnostic("WIFI alarm-failure cleanup-unconfirmed; invocation retained");
        for(;;)rt->yield_ms(50);
    }
#endif
    if(alarm_failed_cleaned)return false;
    alarm_failed_cleaned=true;
    if(!portable_alarm_failure_stop(&alarms)) {
        rt->diagnostic("ALARM foreground-failed output-stop-unconfirmed; invocation retained");
        /* No normal I/O, grant release, graph handoff, or repeated cleanup. */
        for(;;)rt->yield_ms(50);
    }
    return false;
}
#if defined(PORTABLE_AUDIO_SESSION) || defined(PORTABLE_RADIO_SESSION) || defined(PORTABLE_CONTEXTS_CLIENT)
static bool alarm_sessions_safe(void) {
#ifdef PORTABLE_CONTEXTS_CLIENT
    if(native_custody_retained)return false;
    if(contexts_client.api) {
        contexts_status_v1 status={.struct_size=sizeof(status)};
        if(!contexts_client.api->status(contexts_client.api->context,&status)||status.cleanup_pending)return false;
    }
#endif
#ifdef PORTABLE_RADIO_SESSION
    if(!portable_radio_services_safe())return false;
#endif
#ifdef PORTABLE_AUDIO_SESSION
    if(!portable_audio_services_safe())return false;
#endif
    return true;
}
static bool alarm_sessions_suspend(void) {
#ifdef PORTABLE_CONTEXTS_CLIENT
    if(!portable_contexts_stop())return false;
#endif
#ifdef PORTABLE_RADIO_SESSION
    if(!portable_radio_suspend())return false;
#endif
#ifdef PORTABLE_AUDIO_SESSION
    if(!portable_audio_suspend())return false;
#endif
    return true;
}
static bool alarm_audio_pump_unleased(void) {
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
    if(portable_adapter_retained())return false;
#endif
    /* Check copied status before storage-backed reconciliation, then again
     * before a newly exposed occurrence can own output in the next phase. */
    if(!alarm_sessions_safe() || !portable_alarm_status(&alarms))return false;
    if((portable_alarm_owned(&alarms) || alarms.status.state==ALARM_STATE_BLOCKED) &&
       !alarm_sessions_suspend())return false;
    if(!portable_alarm_pump(&alarms))return false;
    return !(portable_alarm_owned(&alarms) || alarms.status.state==ALARM_STATE_BLOCKED) ||
        alarm_sessions_suspend();
}
#elif defined(PORTABLE_NATIVE_CUSTODY_FENCE)
static bool alarm_audio_pump_unleased(void) {
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
    if(portable_adapter_retained())return false;
#endif
    if(native_sleep_retained)return false;
#if defined(ALARM_SERVICE_TAGGED_V2) || defined(PORTABLE_ALARM_TERMINAL_RETENTION)
    if(!portable_alarm_pump(&alarms) || alarms.status.output_uncertain) {
#ifdef PORTABLE_WIFI_SERVICE_LEASE
        return false; /* End the lease before the caller's failure handling. */
#else
        return alarm_failure();
#endif
    }
#else
    int32_t result=alarms.api->step(alarms.api->context);
    if(result!=ALARM_OK && result!=ALARM_PENDING)return alarm_failure();
    if(!portable_alarm_status(&alarms) || alarms.status.output_uncertain)return alarm_failure();
#endif
    return true;
}
#else
#define alarm_audio_pump_unleased() portable_alarm_pump(&alarms)
#endif
#ifdef PORTABLE_WIFI_SERVICE_LEASE
/* A real native resource lease covers the whole non-yielding service call.
 * BUSY is not an alarm error; explicit refresh/ACK intents retain copied state.
 * No lease or provider pointer is carried through the modal's yield. */
static bool alarm_pending_unleased(void) {
    if(alarm_refresh_pending) {
        if(!portable_alarm_refresh(&alarms))return false;
        alarm_refresh_pending=false;
    }
    if(alarm_ack_pending) {
        if(!portable_alarm_acknowledge(&alarms,&alarm_ack_token))return false;
        alarm_ack_pending=false;
    }
    return true;
}
static bool alarm_service_call(bool pump) {
    alarm_service_deferred=false;
    if(failed || native_sleep_retained)return false;
    if(!portable_wifi_services_begin()) {
        alarm_service_deferred=true;return !failed && !native_sleep_retained;
    }
    bool ok=alarm_pending_unleased();
    if(ok && pump)ok=alarm_audio_pump_unleased();
    if(native_sleep_retained)return false;
    bool ended=portable_wifi_services_end();
    return ok && ended;
}
static bool alarm_commit_pending(void) { return alarm_service_call(false); }
static bool alarm_audio_pump(void) { return alarm_service_call(true); }
#else
#define alarm_audio_pump() alarm_audio_pump_unleased()
#endif
/* Service labels are copied status, never retained storage pointers. Accept
 * bounded printable titles; a malformed provider title keeps the old fallback. */
static const char *alarm_title(const alarm_status_v1 *s,char out[24]) {
    for(unsigned i=0;i<24;i++) {
        unsigned ch=(unsigned char)s->label[i];
        if(!ch) { if(i) {out[i]=0;return out;} break; }
        if(ch<32||ch>126)break;
        out[i]=(char)ch;
    }
    return s->occurrence.kind==ALARM_KIND_COUNTDOWN?"COUNTDOWN FINISHED":"ALARM";
}
static void alarm_draw(void) {
    const alarm_status_v1 *s=&alarms.status;
    char title[24];
    if(pp_enabled()) {
        pp_begin();
        pp_text(32,40,width()-64,alarm_title(s,title),2,true,true);
        fill(32,94,width()-64,3,0);
        pp_text(32,140,width()-64,s->state==ALARM_STATE_BLOCKED?
            (s->error==ALARM_RTC?"RTC ERROR":"SERVICE ERROR"):
            s->state==ALARM_STATE_DISMISSING?"STOPPING / SAVING":"TIME IS UP",1,false,true);
        pp_text(32,190,width()-64,s->output_uncertain?"OUTPUT STOP UNCONFIRMED":"",1,false,true);
        fill(32,height()-188,width()-64,88,0);
        pp_text(60,height()-162,width()-120,s->occurrence.generation?"DISMISS":"BACK",2,true,false);
        if(s->state==ALARM_STATE_BLOCKED) {
            fill(32,height()-92,width()-64,3,0);
            pp_text(60,height()-64,width()-120,"RETRY",2,true,true);
        }
        present(false);return;
    }
#ifdef PORTABLE_NOVA_UI
    portable_nova_begin();
    portable_nova_center(0,16,34,208,alarm_title(s,title),NOVA_CYAN);
    portable_nova_rule(20,65,200);
    portable_nova_center(1,16,88,208,s->state==ALARM_STATE_BLOCKED?
        (s->error==ALARM_RTC?"RTC error":"Service error"):
        s->state==ALARM_STATE_DISMISSING?"Stopping / saving":"Time is up",NOVA_TEXT);
    portable_nova_center(2,16,117,208,s->output_uncertain?"Output stop unconfirmed":"",NOVA_CAP);
    portable_nova_button(24,144,192,44,s->occurrence.generation?"Dismiss":"Back",false);
    if(s->state==ALARM_STATE_BLOCKED)portable_nova_button(24,192,192,44,"Retry",false);
#else
    clear();
    label(8,38,width()-16,alarm_title(s,title));
    label(8,78,width()-16,s->state==ALARM_STATE_BLOCKED?
        (s->error==ALARM_RTC?"RTC ERROR":"SERVICE ERROR"):
        s->state==ALARM_STATE_DISMISSING?"STOPPING / SAVING":"TIME IS UP");
    label(8,108,width()-16,s->output_uncertain?"OUTPUT STOP UNCONFIRMED":"");
    label(8,height()-67,width()-16,s->occurrence.generation?"DISMISS":"BACK");
    if(s->state==ALARM_STATE_BLOCKED)label(8,height()-27,width()-16,"RETRY");
    #endif
    present(false);
}
static bool alarm_save_frame(uint16_t **saved) {
    *saved=NULL;
    /* App-owned immutable pixels may still be presenting. Modal logic is
     * independent; drawing and final restoration retain token custody. */
    if(!alarm_pixels || (!alarm_pixels_valid && !paper_token
#ifdef PORTABLE_RASTER_SNAPSHOT
        &&!raster_sealed
#endif
    ))return false;
    *saved=alarm_pixels;alarm_modal=true;return true;
}
static bool alarm_restore(const uint16_t *saved) {
    if(!portable_paper_frame_drain())return false;
#ifdef PORTABLE_RASTER_SNAPSHOT
    raster_begin_allowed=false; /* Direct restore owns an immediate pixel lease. */
#endif
    clear();if(failed)return false;
    unsigned bytes=surface_format==RISC_DISPLAY_FORMAT_MONO1?(info.width+7)/8:info.width*2;
    for(unsigned y=0;y<info.height;y++)memcpy((uint8_t *)surface.pixels+(size_t)y*surface.stride_bytes,
        (const uint8_t*)saved+(size_t)y*bytes,bytes);
    present(false);return portable_paper_frame_drain() && !failed && display_settled;
}
#if defined(PORTABLE_WIFI_SETTINGS_APP) || defined(PORTABLE_UPDATE_APP)
/* A failed radio cleanup forbids storage-backed service calls. Output-only
 * stop does not acknowledge or change the persisted occurrence. Restore the
 * editor so cleanup can be retried; normal reconciliation resumes afterward. */
static bool alarm_wifi_pause(const uint16_t *saved) {
    if(native_sleep_retained)return false;
    if(!portable_alarm_failure_stop(&alarms)) {
        /* The bounded stop-only sequence is exhausted: never repeat it through
         * alarm_failure, release grants, or resume normal provider work. */
        alarm_modal=false;failed=true;alarm_failed_cleaned=true;
        rt->diagnostic("ALARM Wi-Fi pause stop-unconfirmed; invocation retained");
        for(;;)rt->yield_ms(50);
    }
    alarm_cancel_input();bool ok=alarm_restore(saved);alarm_modal=false;
    if(!ok)return alarm_failure();
    alarm_cancel_input();return !failed;
}
#endif
/* Returns false only on foreground failure; consumed prevents initiating Back
 * or touch from leaking into the suspended app after a modal closes. */
static bool alarm_foreground(bool *consumed) {
    *consumed=false;
#ifdef PORTABLE_FILE_BROWSER_APP
    if(!portable_file_browser_safe())return !failed;
#endif
    if(failed || raster_lease_mutable() || (!display_settled && !paper_token
#ifdef PORTABLE_RASTER_SNAPSHOT
      &&!raster_sealed
#endif
    ))return alarm_failure();
#if defined(PORTABLE_WIFI_SETTINGS_APP) || defined(PORTABLE_UPDATE_APP)
    if(!portable_wifi_services_safe())return !failed;
#endif
    if(!alarm_audio_pump()){failed=true;return alarm_failure();}
#ifdef PORTABLE_WIFI_SERVICE_LEASE
    if(alarm_service_deferred)return true;
#endif
    /* Reconciliation is bounded by service phases, not provider poll. Input
       waits until the snapshot says READY or gives an alert/error. */
    bool cue_seen=false;
    for(unsigned n=0;alarms.status.state==ALARM_STATE_LOADING || portable_alarm_cue(&alarms);n++) {
        if(n>=64){failed=true;return alarm_failure();}
        if(portable_alarm_cue(&alarms)){
#if defined(PORTABLE_BLE_BROADCAST) || defined(PORTABLE_CONTEXTS_CLIENT)
            if(!portable_background_stop())return false;
#endif
            cue_seen=true;*consumed=true;
        }
        input_service();
        if(!paper_present_progress())return alarm_failure();
        rt->yield_ms(portable_alarm_cue(&alarms)?4:1);
#if defined(PORTABLE_WIFI_SETTINGS_APP) || defined(PORTABLE_UPDATE_APP)
        if(!portable_wifi_services_safe())return !failed;
#endif
        if(!alarm_audio_pump()){failed=true;return alarm_failure();}
#ifdef PORTABLE_WIFI_SERVICE_LEASE
        if(alarm_service_deferred)return true;
#endif
        /* Drain one short cue with the original frame untouched. Finish once
         * its outputs and durable ACK are settled; subsequent simultaneous
         * cues reconcile on the next foreground call rather than exhausting
         * one loading-loop budget. Consumed input cannot leak into an exit. */
        if(cue_seen && !portable_alarm_cue(&alarms)) {
            alarm_cancel_input();
            if(alarms.status.state!=ALARM_STATE_BLOCKED)return true;
            break;
        }
    }
    bool error=alarms.status.state==ALARM_STATE_BLOCKED;
    if(!portable_alarm_owned(&alarms) && (!error ||
        (alarm_error_seen && alarm_seen_error==alarms.status.error && alarm_seen_snapshot==alarms.status.snapshot)))return true;
#if defined(PORTABLE_BLE_BROADCAST) || defined(PORTABLE_CONTEXTS_CLIENT)
    if(!portable_background_stop())return false;
#endif
#ifdef PORTABLE_QUICK_ACTIONS
    if(!quick_interrupt())return alarm_failure();
#endif
    uint16_t *saved;
    if(!alarm_save_frame(&saved)){failed=true;return alarm_failure();}
    *consumed=true;alarm_cancel_input();
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
    handoff_finish();
#endif
    alarm_token_v1 shown=alarms.status.occurrence;
    bool redraw=true;
    for(;;) {
        if(redraw && portable_paper_frame_ready()){alarm_draw();redraw=false;}
        if(failed){alarm_modal=false;return alarm_failure();}
        input_service();input_dispatch();
        if(!input_progressed && !navigation_pending) {
            input_pending=false;rt->yield_ms(1);input_service();input_dispatch();
        }
        if(failed){alarm_modal=false;return alarm_failure();}
#if defined(PORTABLE_WIFI_SETTINGS_APP) || defined(PORTABLE_UPDATE_APP)
        if(!portable_wifi_services_safe())return alarm_wifi_pause(saved);
#endif
        portable_touch_sample sample;input_take(&sample);
        uint32_t buttons=navigation_pending;navigation_pending=0;
        bool tap=sample.valid && sample.released && sample.tap_eligible && !sample.moved && !sample.cancelled;
        bool paper=pp_enabled();
#ifdef PORTABLE_NOVA_UI
        bool retry=(buttons&T5_APP_BUTTON_CONFIRM) || (tap && portable_nova_hit(sample.x,sample.y,24,192,192,44));
        bool dismiss=(buttons&T5_APP_BUTTON_BACK) || (tap && portable_nova_hit(sample.x,sample.y,24,144,192,44));
#else
        bool retry=(buttons&T5_APP_BUTTON_CONFIRM) || (tap && sample.y>=info.height-42);
        bool dismiss=(buttons&T5_APP_BUTTON_BACK) || (tap && sample.y>=info.height-88 && sample.y<info.height-42);
#endif
        if(paper) {
            retry=(buttons&T5_APP_BUTTON_CONFIRM) || (tap && sample.y>=height()-92);
            dismiss=(buttons&T5_APP_BUTTON_BACK) || (tap && sample.y>=height()-188 && sample.y<height()-100);
        }
        if(alarms.status.state==ALARM_STATE_BLOCKED && retry) {
#if defined(ALARM_SERVICE_TAGGED_V2) || defined(PORTABLE_ALARM_TERMINAL_RETENTION)
#ifdef PORTABLE_WIFI_SERVICE_LEASE
            if(!alarm_refresh())return alarm_failure();
#else
            if(!portable_alarm_refresh(&alarms))return alarm_failure();
#endif
#elif defined(PORTABLE_NATIVE_CUSTODY_FENCE)
            int32_t result=alarms.api->refresh(alarms.api->context);
            if(result!=ALARM_OK && result!=ALARM_PENDING)return alarm_failure();
#else
            (void)alarms.api->refresh(alarms.api->context);
#endif
            alarm_cancel_input();redraw=true;
        } else if(dismiss) {
            if(alarms.status.occurrence.generation) {
                /* Copy exact current token; service rejects stale identities. */
                alarm_token_v1 token=alarms.status.occurrence;
#if defined(ALARM_SERVICE_TAGGED_V2) || defined(PORTABLE_ALARM_TERMINAL_RETENTION)
#ifdef PORTABLE_WIFI_SERVICE_LEASE
                if(!alarm_ack_pending){alarm_ack_token=token;alarm_ack_pending=true;}
                if(!alarm_commit_pending())return alarm_failure();
#else
                if(!portable_alarm_acknowledge(&alarms,&token))return alarm_failure();
#endif
#elif defined(PORTABLE_NATIVE_CUSTODY_FENCE)
                int32_t result=alarms.api->acknowledge(alarms.api->context,&token);
                if(result!=ALARM_OK && result!=ALARM_PENDING)return alarm_failure();
#else
                (void)alarms.api->acknowledge(alarms.api->context,&token);
#endif
                alarm_cancel_input();redraw=true;
            } else if(alarms.status.state==ALARM_STATE_BLOCKED && !alarms.status.output_uncertain) {
                alarm_error_seen=true;alarm_seen_error=alarms.status.error;alarm_seen_snapshot=alarms.status.snapshot;break;
            }
        }
        if(failed || raster_lease_mutable() || !paper_present_progress()){alarm_modal=false;return alarm_failure();}
#if defined(PORTABLE_WIFI_SETTINGS_APP) || defined(PORTABLE_UPDATE_APP)
        if(!portable_wifi_services_safe())return alarm_wifi_pause(saved);
#endif
        uint32_t old_state=alarms.status.state;int32_t old_error=alarms.status.error;
        if(!alarm_audio_pump()){failed=true;alarm_modal=false;return alarm_failure();}
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
