/* Selected app-side modal handoff. The host has its own presenter and input
 * grants; nothing here is linked into, or exported to, its provider module. */
#include "PortableTextInputHandoff.h"
#include "RiscTextEntryV1.h"
bool portable_text_adapter_retained(void) {
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
    if(native_custody_retained)return true;
#endif
#ifdef PORTABLE_ALARM_CLIENT
    if(native_sleep_retained)return true;
#endif
    return text_input_retained;
}
void portable_text_adapter_retain(void) {
    if(text_input_retained)return;
    text_input_retained=true;
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
    /* Runtime or the text host may already have retained custody. The stage
     * logger calls health/diagnostic, so terminal handoff must latch silently. */
    portable_adapter_retain_silent();
#else
    portable_adapter_retain();
#endif
#else
    /* Legacy Watch adapters still use Runtime's invocation fence. Retention
     * is terminal: do not diagnose, release, stop outputs or touch providers. */
    failed=true;
#ifdef PORTABLE_ALARM_CLIENT
    native_sleep_retained=true;
#endif
    bool (*retain)(void)=NULL;
    const size_t offset=RISC_RUNTIME_CAPABILITIES_V1_SIZE+sizeof(bool (*)(void));
    if(rt&&rt->struct_size>=offset+sizeof(retain))memcpy(&retain,(const unsigned char *)rt+offset,sizeof(retain));
    if(!retain||!retain())for(;;){}
#endif
}
static int text_input_failure(void) {portable_text_adapter_retain();return RISC_TEXT_ENTRY_RETAINED;}
static bool text_input_alive(void) {
    if(portable_text_adapter_retained()||failed)return false;
    if(!risc_runtime_get_api(1)){portable_text_adapter_retain();return false;}
    return true;
}
/* These handoff-only operations check native liveness at each boundary,
 * including providers that report success while retaining the invocation. */
static bool text_input_touch_close(void) {
    if(touch.subscription) {
        bool ok=touch.api->unsubscribe(touch.api->context,touch.subscription);
        if(!text_input_alive()||!ok)return false;
        touch.subscription=0;
    }
    if(touch.grant.api) {
        risc_runtime_capability_v1 released=touch.grant;
        bool ok=rt->release(&released);
        if(!text_input_alive()||!ok||released.api||released.slot||released.generation)return false;
    }
    touch=(portable_touch){0};return true;
}
static bool text_input_touch_open(void) {
    touch=(portable_touch){.grant={.struct_size=sizeof(touch.grant)}};
    bool ok=rt->acquire("input.touch.raw",1,0,&touch.grant);
    if(!text_input_alive()||!ok||!touch.grant.api||!touch.grant.slot||!touch.grant.generation)return false;
    touch.api=touch.grant.api;
    if(touch.api->api_version!=1||touch.api->struct_size<sizeof(*touch.api)||!touch.api->subscribe||
       !touch.api->unsubscribe||!touch.api->poll||!touch.api->next||!touch.api->snapshot)return false;
    touch.subscription=touch.api->subscribe(touch.api->context);
    return text_input_alive()&&touch.subscription;
}
#ifdef PORTABLE_INPUT_NAVIGATION
static bool text_input_navigation(const risc_input_foreground_v1 *claims,size_t count) {
    if(!navigation_ready)return true;
    bool ok=navigation->foreground(navigation->context,claims,count);
    if(!text_input_alive()||!ok)return false;
    ok=navigation->reset(navigation->context);
    return text_input_alive()&&ok;
}
#endif

static void text_input_neutral(void) {
    input_sample=(portable_touch_sample){0};input_pending=false;navigation_pending=0;
    home_pending=crown_pending=false;clear_contact_snapshots();
#ifdef PORTABLE_INPUT_NAVIGATION
    navigation_neutral=false;
#endif
#ifdef PORTABLE_QUICK_ACTIONS
    pqa_cancel_input(&quick.ui);quick_replay_pending=quick_replay_delivery=false;
#endif
#if defined(PORTABLE_NOVA_UI) && !defined(PORTABLE_APP_OWNS_TOUCH_CHROME)
    nu_gesture=false;
#endif
}
int portable_text_adapter_suspend(void) {
    if(!text_input_alive())return text_input_failure();
    if(text_input_suspended)return RISC_TEXT_ENTRY_BUSY;
#ifdef PORTABLE_ALARM_CLIENT
    if(alarm_modal)return RISC_TEXT_ENTRY_BUSY;
#endif
#ifdef PORTABLE_QUICK_ACTIONS
    if(quick_modal||pqa_visible(&quick.ui)||pqa_capture(&quick.ui))return RISC_TEXT_ENTRY_BUSY;
#endif
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
    if(!portable_paper_frame_drain())return text_input_failure();
#endif
#ifdef PORTABLE_PAPER_CROSSFADE
    portable_paper_transition_cancel();
#endif
#ifdef PORTABLE_RETAINED_RGB565_HANDOFF
    handoff_finish();
#endif
    if(surface.frame){display->release(display->context,surface.frame);surface=(risc_display_surface_v1){0};}
    if(!text_input_alive())return text_input_failure();
#ifdef PORTABLE_ALARM_CLIENT
    if(!display_settled)return text_input_failure();
#endif
    if(!text_input_touch_close())return text_input_failure();
#ifdef PORTABLE_INPUT_NAVIGATION
    if(!text_input_navigation(NULL,0))return text_input_failure();
#endif
    text_input_neutral();text_input_suspended=true;return RISC_TEXT_ENTRY_OK;
}
int portable_text_adapter_attention(void) {
    if(!text_input_alive())return text_input_failure();
    if(!text_input_suspended)return RISC_TEXT_ENTRY_INVALID;
#ifdef PORTABLE_ALARM_CLIENT
    /* Caller invokes only after a settled host frame. Never paint the app or
     * alarm over a host lease; a pending alarm instead cancels the editor. */
    if(!alarm_audio_pump()||alarms.status.output_uncertain)return text_input_failure();
    if(!text_input_alive())return text_input_failure();
    return portable_alarm_owned(&alarms)||alarms.status.state==ALARM_STATE_BLOCKED||alarms.status.state==ALARM_STATE_CUE;
#else
    return 0;
#endif
}
int portable_text_adapter_resume(void) {
    if(!text_input_alive())return text_input_failure();
    if(!text_input_suspended)return RISC_TEXT_ENTRY_INVALID;
    if(!text_input_touch_open())return text_input_failure();
#ifdef PORTABLE_INPUT_NAVIGATION
    const risc_input_foreground_v1 claims[]={{"input.touch.raw",1}};
    if(!text_input_navigation(claims,1))return text_input_failure();
#endif
    text_input_neutral();previous_valid=paper_previous_valid=false;
#ifdef PORTABLE_ALARM_CLIENT
    alarm_pixels_valid=false;
#endif
    last_poll_at=input_sampled_at=millis_now();
#ifdef PORTABLE_APP_SLEEP_LOCAL
    last_activity=last_poll_at;
#endif
    if(!text_input_alive())return text_input_failure();
    text_input_suspended=false;return RISC_TEXT_ENTRY_OK;
}
