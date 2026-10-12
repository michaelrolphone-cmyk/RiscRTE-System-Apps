#pragma once
#include "PortableTextInputCheckpointClient.h"
#include "PortableResidentPolicyInput.h"
#include "RiscSceneResidentV1.h"
/* Separate from frozen predecessor scene helpers. Only the exact reason-bound
 * POLL promise permits input continuity. Old owners return no promise and use
 * full detach, so selected touch-power subscriber guards remain authoritative. */
static inline int32_t portable_text_resident_poll(risc_scene_resident_v1 *s,uint32_t scene_flags,uint32_t work_flags,bool *continuity){
    if(scene_flags&RISC_SCENE_ACTIVITY)s->activity=true;
    if(!s->enabled||scene_flags&(RISC_SCENE_PRESENTING|RISC_SCENE_INPUT_BUSY|RISC_SCENE_CLOSING))return RISC_RESIDENT_BUSY;
    risc_resident_request_v1 request={sizeof(request),RISC_RESIDENT_CHECKPOINT_POLL,
        (s->activity?RISC_RESIDENT_POLL_ACTIVITY:0)|(work_flags&(RISC_RESIDENT_POLL_INHIBIT_IDLE|RISC_RESIDENT_POLL_INHIBIT_POLICY)),0};
    risc_resident_reply_v1 reply={sizeof(reply),0};
    int32_t rc=s->client.checkpoint(s->client.invocation,&request,&reply);
    if(!risc_runtime_get_api(1)||rc==RISC_RESIDENT_RETAINED)return RISC_RESIDENT_RETAINED;
    if(rc==RISC_RESIDENT_OK){
        if(reply.struct_size!=sizeof(reply))return RISC_RESIDENT_RETAINED;
        s->activity=false;s->policy=!(work_flags&RISC_RESIDENT_POLL_INHIBIT_POLICY)&&(reply.flags&RISC_RESIDENT_REPLY_POLICY_REQUEST);
        if(continuity)*continuity=s->policy&&reply.flags==(RISC_RESIDENT_REPLY_POLICY_REQUEST|PORTABLE_RESIDENT_REPLY_INPUT_CONTINUITY);
    }
    return rc;
}
static inline int32_t portable_text_resident_policy(risc_scene_resident_v1 *s,bool continuity,uint32_t *resume_flags){
    if(resume_flags)*resume_flags=RISC_TEXT_ENTRY_RESUME_REDRAW|(continuity?0:RISC_TEXT_ENTRY_RESUME_FOCUS_CHANGED);
    if(!s||!s->enabled)return RISC_RESIDENT_DENIED;
    risc_resident_request_v1 request={sizeof(request),RISC_RESIDENT_CHECKPOINT_POLICY,0,0};
    risc_resident_reply_v1 reply={sizeof(reply),0};
    int32_t rc=s->client.checkpoint(s->client.invocation,&request,&reply);
    if(!risc_runtime_get_api(1)||rc==RISC_RESIDENT_RETAINED)return RISC_RESIDENT_RETAINED;
    if(rc==RISC_RESIDENT_OK){
        if(reply.struct_size!=sizeof(reply)||(continuity&&(reply.flags&RISC_RESIDENT_REPLY_REDRAW)))return RISC_RESIDENT_RETAINED;
        s->policy=false;
    }
    return rc;
}
