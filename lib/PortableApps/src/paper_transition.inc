/* Shared, opt-in, capability-selected completed-image handoff. */
#include "PortablePaperTransition.h"
#include "RiscDisplayOutputSnapshotV1.h"
static uint8_t *paper_handoff_old;
static uint32_t paper_handoff_started;
static bool paper_handoff_clock;
static void paper_handoff_finish(void) {
    free(paper_handoff_old);paper_handoff_old=NULL;paper_handoff_clock=false;
}
bool portable_paper_transition_active(void) {return paper_handoff_old && !failed;}
void portable_paper_transition_cancel(void) {
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
    if(native_custody_retained)return;
#endif
    paper_handoff_finish();
}
void portable_paper_transition_begin(void) {
    if(failed)return;
    paper_handoff_finish();
    if(surface.frame || surface_format!=RISC_DISPLAY_FORMAT_MONO1 ||
       !(info.flags&RISC_DISPLAY_INFO_RETAINS_IMAGE) || info.width>800 || info.height>800)return;
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
    if(paper_token)return;
#endif
    const risc_display_output_api_v1_snapshot *ext=risc_display_output_snapshot(display);
    if(!ext)return;
    size_t stride=(info.width+7u)/8u,bytes=stride*info.height;
    if(!bytes || bytes>65536)return;
    paper_handoff_old=malloc(bytes);
    if(paper_handoff_old && !ext->copy_completed(display->context,RISC_DISPLAY_FORMAT_MONO1,
        paper_handoff_old,bytes,(uint32_t)stride))paper_handoff_finish();
    /* This history is physically completed, so the very first transition
     * frame can use the ordinary damage calculation. An acquired surface's
     * contents are deliberately never used as the outgoing image. */
    if(paper_handoff_old && paper_previous) {
        memcpy(paper_previous,paper_handoff_old,bytes);paper_previous_valid=true;
    }
}
static bool paper_handoff_blend(void) {
    if(!paper_handoff_old)return false;
#ifdef PORTABLE_QUICK_ACTIONS
    if(quick_modal)return false;
#endif
#ifdef PORTABLE_ALARM_CLIENT
    if(alarm_modal)return false;
#endif
    uint32_t now=millis_now();
    if(failed)return false;
    if(!paper_handoff_clock){paper_handoff_started=now;paper_handoff_clock=true;}
    uint32_t elapsed=now-paper_handoff_started;
    /* A 400 ms app-side envelope gives a ~100 ms capable panel intermediate
     * frames. It is not a guarantee of physical completion time. The old image
     * is already visible; start with incoming coverage instead of resending it. */
    unsigned coverage=elapsed>=400u?16u:1u+elapsed*15u/400u;
    if(coverage==16u)paper_handoff_finish();
    else if(!portable_paper_blend(surface.pixels,surface.stride_bytes,paper_handoff_old,
        (info.width+7u)/8u,info.width,info.height,coverage))paper_handoff_finish();
    return true;
}
