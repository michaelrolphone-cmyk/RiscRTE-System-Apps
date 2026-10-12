/* Explicit opt-in for modal clients; the ordinary app input poll remains
 * suspended. The shared host is the only touch, navigation and USB consumer. */
#include "PortableTextInputPolicy.h"
#include "PortableResidentPolicyInput.h"
#ifdef PORTABLE_RESIDENT_SHELL_CLIENT
static uint32_t text_policy_poll_at;
#endif
int portable_text_adapter_policy_capture(void){
    if(!text_input_alive()||!text_input_suspended)return text_input_failure();
#ifdef PORTABLE_CONTEXTS_CLIENT
    if(!contexts_capture_checkpoint()||!text_input_alive())return text_input_failure();
#endif
    return 0;
}
int portable_text_adapter_policy_prepare(uint32_t flags,bool *continuity) {
    if(!text_input_alive()||!text_input_suspended||!continuity)return text_input_failure();
    const uint32_t now=millis_now();
    if(!text_input_alive())return text_input_failure();
    if(flags&(RISC_TEXT_ENTRY_CHECKPOINT_ACTIVITY|RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY)) {
#ifdef PORTABLE_APP_SLEEP_LOCAL
        last_activity=now;
#endif
#ifdef PORTABLE_RESIDENT_POLICY
        resident_activity_pending=true;
#endif
    }
    *continuity=false;
    /* A settled host has no mutable raster custody. Preserve the predecessor
     * service progression without consuming any of its input subscriptions. */
    if(flags&RISC_TEXT_ENTRY_CHECKPOINT_PRESENTING)return 0;
#ifdef PORTABLE_BLE_BROADCAST
    if(!broadcast_tick()||!text_input_alive())return text_input_failure();
#endif
#ifdef PORTABLE_CONTEXTS_CLIENT
    if(!contexts_capture_checkpoint()||!text_input_alive())return text_input_failure();
    if(!contexts_tick()||!text_input_alive())return text_input_failure();
#endif
    if(flags&RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY)return 0;
#ifdef PORTABLE_CONTEXTS_CLIENT
    /* A workflow message borrows foreground focus, never passive-input
     * continuity. Preserve the keyboard draft across its ordinary dismissal. */
    if(context_message_open)return 1;
#endif
#ifdef PORTABLE_RESIDENT_SHELL_CLIENT
#ifdef PORTABLE_RESIDENT_POLICY
    if((uint32_t)(now-text_policy_poll_at)<500u)return 0;
    text_policy_poll_at=now;
    if(resident_capture_active()){resident_activity_pending=true;return 0;}
    if(!resident_bind())return text_input_failure();
    risc_resident_request_v1 request={sizeof(request),RISC_RESIDENT_CHECKPOINT_POLL,resident_policy_flags(),0};
    risc_resident_reply_v1 reply={sizeof(reply),0};
    int status=resident.checkpoint(resident.invocation,&request,&reply);
    if(!text_input_alive()||status==RISC_RESIDENT_RETAINED)return text_input_failure();
    if(status==RISC_RESIDENT_EXIT){handoff_requested=true;return 0;}
    if(status==RISC_RESIDENT_BUSY)return 0;
    if(status!=RISC_RESIDENT_OK||reply.struct_size!=sizeof(reply))return text_input_failure();
    resident_activity_pending=false;
    resident_policy_pending=!(request.flags&RISC_RESIDENT_POLL_INHIBIT_POLICY)&&
        (reply.flags&RISC_RESIDENT_REPLY_POLICY_REQUEST);
    if(!resident_policy_pending)return 0;
    *continuity=reply.flags==(RISC_RESIDENT_REPLY_POLICY_REQUEST|PORTABLE_RESIDENT_REPLY_INPUT_CONTINUITY);
    return 1;
#else
    return 0;
#endif
#else
    bool sleep_due=false,battery_due=false;
#if defined(PORTABLE_APP_SLEEP_LOCAL) && !defined(PORTABLE_SLEEP_MANUAL_ONLY)
    sleep_due=(uint32_t)(now-last_activity)>=portable_idle_ms()&&!idle_capture_active();
#endif
#ifdef PORTABLE_LOW_BATTERY
    battery_due=!low_battery_sampled||(uint32_t)(now-low_battery_sampled_at)>=5000u;
#endif
    if(!sleep_due&&!battery_due)return 0;
    *continuity=!sleep_due;return 1;
#endif
}
int portable_text_adapter_policy_apply(bool continuity,uint32_t *resume_flags) {
    if(!text_input_alive()||!text_input_suspended||!resume_flags)return text_input_failure();
    *resume_flags=RISC_TEXT_ENTRY_RESUME_REDRAW|(continuity?0:RISC_TEXT_ENTRY_RESUME_FOCUS_CHANGED);
#ifdef PORTABLE_CONTEXTS_CLIENT
    if(context_message_open){
        if(continuity)return text_input_failure();
        /* The host is fully paused and detached. Borrow app-local input only
         * for the existing workflow message, then drain/release it before
         * resuming the same shared editor and neutralizing dismissal input. */
        if(!text_input_touch_open())return text_input_failure();
#ifdef PORTABLE_INPUT_NAVIGATION
        const risc_input_foreground_v1 claims[]={{"input.touch.raw",1}};
        if(!text_input_navigation(claims,1))return text_input_failure();
#endif
        text_input_neutral();text_input_suspended=false;
        if(!contexts_message_foreground()||!text_input_alive())return text_input_failure();
        if(portable_text_adapter_suspend()!=RISC_TEXT_ENTRY_OK)return text_input_failure();
    }
#endif
#ifdef PORTABLE_RESIDENT_SHELL_CLIENT
#ifdef PORTABLE_RESIDENT_POLICY
    if(!resident_ready||!resident_policy_pending)return text_input_failure();
#ifdef PORTABLE_CONTEXTS_CLIENT
    if(!contexts_suspend()||!text_input_alive())return text_input_failure();
#endif
#ifdef PORTABLE_BLE_BROADCAST
    if(!portable_broadcast_stop()||!text_input_alive())return text_input_failure();
#endif
    risc_resident_request_v1 request={sizeof(request),RISC_RESIDENT_CHECKPOINT_POLICY,0,0};
    risc_resident_reply_v1 reply={sizeof(reply),0};
    int status=resident.checkpoint(resident.invocation,&request,&reply);
    if(!text_input_alive()||status==RISC_RESIDENT_RETAINED)return text_input_failure();
    resident_policy_pending=false;
    if(status==RISC_RESIDENT_EXIT){handoff_requested=true;return 1;}
    if(status!=RISC_RESIDENT_OK&&status!=RISC_RESIDENT_BUSY)return text_input_failure();
    if(status==RISC_RESIDENT_OK&&(reply.struct_size!=sizeof(reply)||
       (continuity&&(reply.flags&RISC_RESIDENT_REPLY_REDRAW))))return text_input_failure();
#endif
#else
#ifdef PORTABLE_LOW_BATTERY
    if(!low_battery_poll()||!text_input_alive())return text_input_failure();
#endif
#if defined(PORTABLE_APP_SLEEP_LOCAL) && !defined(PORTABLE_SLEEP_MANUAL_ONLY)
    /* A routine battery sample cannot turn into sleep if time or preferences
     * changed since prepare. A fresh detached boundary handles it next time. */
    if(!continuity&&(uint32_t)(millis_now()-last_activity)>=portable_idle_ms()&&!idle_capture_active()) {
        if(!idle_sleep()||!text_input_alive())return text_input_failure();
    }
#endif
#endif
    previous_valid=paper_previous_valid=false;
    return text_input_alive()?0:text_input_failure();
}
