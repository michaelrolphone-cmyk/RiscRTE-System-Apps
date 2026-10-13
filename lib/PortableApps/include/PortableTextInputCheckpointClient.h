#pragma once
#include "PortableTextInputClient.h"
/* Optional modal checkpoint owner. Its lifetime is nested in an ordinary text
 * client session; base close also safely closes a paused/pause-pending host. */
typedef struct {
    const risc_text_entry_api_v1_checkpoint *api;
    bool enabled,paused,pausing,focus_change;
} portable_text_checkpoint_client;
static inline int portable_text_checkpoint_enable(portable_text_client *c,portable_text_checkpoint_client *p){
    if(!c||!p)return RISC_TEXT_ENTRY_INVALID;
    if(!portable_text_client_live(c))return RISC_TEXT_ENTRY_RETAINED;
    if(!c->active||c->closing||p->enabled)return RISC_TEXT_ENTRY_BUSY;
    p->api=risc_text_entry_checkpoint(c->api);
    if(!p->api)return RISC_TEXT_ENTRY_UNAVAILABLE;
    int rc=p->api->checkpoint_enable(c->api->context,c->session);
    if(!portable_text_client_live(c))return RISC_TEXT_ENTRY_RETAINED;
    if(rc==RISC_TEXT_ENTRY_OK){p->enabled=true;return rc;}
    if(rc==RISC_TEXT_ENTRY_UNAVAILABLE||rc==RISC_TEXT_ENTRY_BUSY)return rc;
    return portable_text_client_retain(c);
}
static inline int portable_text_checkpoint_snapshot(portable_text_client *c,portable_text_checkpoint_client *p,risc_text_entry_checkpoint_state_v1 *out){
    if(!c||!p||!out)return RISC_TEXT_ENTRY_INVALID;
    if(!portable_text_client_live(c))return RISC_TEXT_ENTRY_RETAINED;
    if(!c->active||c->closing||!p->enabled)return RISC_TEXT_ENTRY_BUSY;
    risc_text_entry_checkpoint_state_v1 state={.struct_size=sizeof(state)};
    int rc=p->api->checkpoint_snapshot(c->api->context,c->session,&state);
    if(!portable_text_client_live(c))return RISC_TEXT_ENTRY_RETAINED;
    if(rc==RISC_TEXT_ENTRY_BUSY)return rc;
    if(rc!=RISC_TEXT_ENTRY_OK||state.struct_size!=sizeof(state)||
       (state.flags&~(RISC_TEXT_ENTRY_CHECKPOINT_PRESENTING|RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY|RISC_TEXT_ENTRY_CHECKPOINT_ACTIVITY|RISC_TEXT_ENTRY_CHECKPOINT_PAUSED))||
       (!!(state.flags&RISC_TEXT_ENTRY_CHECKPOINT_PAUSED)!=p->paused)||
       ((state.flags&RISC_TEXT_ENTRY_CHECKPOINT_PAUSED)&&state.flags!=RISC_TEXT_ENTRY_CHECKPOINT_PAUSED))return portable_text_client_retain(c);
    *out=state;return RISC_TEXT_ENTRY_OK;
}
static inline int portable_text_checkpoint_pause_mode(portable_text_client *c,portable_text_checkpoint_client *p,bool continuity){
    if(!c||!p)return RISC_TEXT_ENTRY_INVALID;
    if(!portable_text_client_live(c))return RISC_TEXT_ENTRY_RETAINED;
    if(!c->active||c->closing||!p->enabled)return RISC_TEXT_ENTRY_BUSY;
    if((p->paused||p->pausing)&&p->focus_change==continuity)return portable_text_client_retain(c);
    if(p->paused)return RISC_TEXT_ENTRY_OK;
    int rc=continuity?p->api->pause(c->api->context,c->session):p->api->pause_for_focus_change(c->api->context,c->session);
    if(!portable_text_client_live(c))return RISC_TEXT_ENTRY_RETAINED;
    if(rc==RISC_TEXT_ENTRY_OK){p->paused=true;p->pausing=false;p->focus_change=!continuity;return rc;}
    if(rc==RISC_TEXT_ENTRY_AGAIN){p->pausing=true;p->focus_change=!continuity;return rc;}
    if(rc==RISC_TEXT_ENTRY_BUSY&&!p->pausing)return rc;
    return portable_text_client_retain(c);
}
static inline int portable_text_checkpoint_pause(portable_text_client *c,portable_text_checkpoint_client *p){return portable_text_checkpoint_pause_mode(c,p,true);}
static inline int portable_text_checkpoint_resume(portable_text_client *c,portable_text_checkpoint_client *p,uint32_t flags){
    if(!c||!p)return RISC_TEXT_ENTRY_INVALID;
    if(!portable_text_client_live(c))return RISC_TEXT_ENTRY_RETAINED;
    if(!c->active||c->closing||!p->enabled||!p->paused||p->pausing)return RISC_TEXT_ENTRY_BUSY;
    int rc=p->api->resume(c->api->context,c->session,flags);
    if(!portable_text_client_live(c))return RISC_TEXT_ENTRY_RETAINED;
    if(rc!=RISC_TEXT_ENTRY_OK)return portable_text_client_retain(c);
    p->paused=false;return RISC_TEXT_ENTRY_OK;
}
