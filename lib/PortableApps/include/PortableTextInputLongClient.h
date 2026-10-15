#pragma once
#include "PortableTextInputClient.h"
/* Opt-in long printable-ASCII client. Shares the original lease, presenter, liveness
 * and terminal cleanup contract without changing any old consumer's layout. */
static inline int portable_text_long_clear(void *memory,size_t size,int result) {
    volatile unsigned char *p=memory;for(size_t i=0;i<size;i++)p[i]=0;return result;
}
static inline int portable_text_client_begin_long(portable_text_client *c,const risc_runtime_api_v1 *runtime,
                                                      const char *label,const char *initial,uint32_t capacity) {
    if(!c||!portable_text_client_live(c))return RISC_TEXT_ENTRY_RETAINED;
    if(c->active||c->acquired||c->suspended)return RISC_TEXT_ENTRY_BUSY;
    if(!runtime||!runtime->acquire||!runtime->release||!label||!initial||!capacity||capacity>RISC_TEXT_ENTRY_LONG_BYTES)return RISC_TEXT_ENTRY_INVALID;
    risc_text_entry_request_v1_long request={.api_version=1,.struct_size=sizeof(request),.capacity=capacity};
    unsigned n=0;for(;n<RISC_TEXT_ENTRY_LABEL&&label[n];n++){if((unsigned char)label[n]<32||(unsigned char)label[n]>126)return portable_text_long_clear(&request,sizeof(request),RISC_TEXT_ENTRY_INVALID);request.label[n]=label[n];}
    if(n==RISC_TEXT_ENTRY_LABEL)return portable_text_long_clear(&request,sizeof(request),RISC_TEXT_ENTRY_INVALID);
    for(n=0;n<capacity&&initial[n];n++){if((unsigned char)initial[n]<32||(unsigned char)initial[n]>126)return portable_text_long_clear(&request,sizeof(request),RISC_TEXT_ENTRY_INVALID);request.text[n]=initial[n];}
    if(n==capacity)return portable_text_long_clear(&request,sizeof(request),RISC_TEXT_ENTRY_INVALID);
    c->runtime=runtime;c->capacity=capacity;c->grant=(risc_runtime_capability_v1){.struct_size=sizeof(c->grant)};
    bool acquired=runtime->acquire(RISC_TEXT_ENTRY_CAPABILITY,1,0,&c->grant);
    if(!portable_text_client_live(c))return portable_text_long_clear(&request,sizeof(request),RISC_TEXT_ENTRY_RETAINED);
    if(!acquired){if(c->grant.struct_size!=sizeof(c->grant)||c->grant.api||c->grant.slot||c->grant.generation)return portable_text_long_clear(&request,sizeof(request),portable_text_client_retain(c));return portable_text_long_clear(&request,sizeof(request),RISC_TEXT_ENTRY_UNAVAILABLE);}
    if(c->grant.struct_size!=sizeof(c->grant)||!c->grant.api||!c->grant.slot||!c->grant.generation)return portable_text_long_clear(&request,sizeof(request),portable_text_client_retain(c));
    c->acquired=true;c->api=c->grant.api;
    const risc_text_entry_api_v1_long *ext=risc_text_entry_long(c->api);
    if(!ext||!c->api->open||!c->api->poll||!c->api->close){int closed=portable_text_client_release(c);return portable_text_long_clear(&request,sizeof(request),closed==RISC_TEXT_ENTRY_OK?RISC_TEXT_ENTRY_UNAVAILABLE:closed);}
    c->home_reason=true;request.reserved=RISC_TEXT_ENTRY_REQUEST_HOME_REASON;
    int rc=portable_text_adapter_suspend();
    if(rc!=RISC_TEXT_ENTRY_OK){if(rc==RISC_TEXT_ENTRY_RETAINED)return portable_text_long_clear(&request,sizeof(request),portable_text_client_retain(c));int closed=portable_text_client_release(c);return portable_text_long_clear(&request,sizeof(request),closed==RISC_TEXT_ENTRY_OK?rc:closed);}
    c->suspended=true;c->session=0;
    rc=ext->open_long(c->api->context,&request,&c->session);
    if(rc==RISC_TEXT_ENTRY_RETAINED)return portable_text_long_clear(&request,sizeof(request),portable_text_client_retain(c));
    if(!portable_text_client_live(c))return portable_text_long_clear(&request,sizeof(request),RISC_TEXT_ENTRY_RETAINED);
    if(rc==RISC_TEXT_ENTRY_OK&&c->session){c->active=true;return portable_text_long_clear(&request,sizeof(request),RISC_TEXT_ENTRY_OK);}
    if(c->session||(rc!=RISC_TEXT_ENTRY_UNAVAILABLE&&rc!=RISC_TEXT_ENTRY_BUSY&&rc!=RISC_TEXT_ENTRY_INVALID))return portable_text_long_clear(&request,sizeof(request),portable_text_client_retain(c));
    int closed=portable_text_client_release(c);return portable_text_long_clear(&request,sizeof(request),closed==RISC_TEXT_ENTRY_OK?rc:closed);
}
static inline int portable_text_client_poll_long(portable_text_client *c,risc_text_entry_state_v1_long *out) {
    if(!c||!out)return RISC_TEXT_ENTRY_INVALID;
    if(!portable_text_client_live(c))return RISC_TEXT_ENTRY_RETAINED;
    if(!c->active)return RISC_TEXT_ENTRY_STALE;
    if(!c->capacity||c->capacity>RISC_TEXT_ENTRY_LONG_BYTES)return portable_text_client_retain(c);
    if(c->closing)return RISC_TEXT_ENTRY_AGAIN;
    const risc_text_entry_api_v1_long *ext=risc_text_entry_long(c->api);
    if(!ext)return portable_text_client_retain(c);
    risc_text_entry_state_v1_long state={.struct_size=sizeof(state)};
    int rc=ext->poll_long(c->api->context,c->session,&state);
    if(rc==RISC_TEXT_ENTRY_RETAINED)return portable_text_long_clear(&state,sizeof(state),portable_text_client_retain(c));
    if(!portable_text_client_live(c))return portable_text_long_clear(&state,sizeof(state),RISC_TEXT_ENTRY_RETAINED);
    if(rc==RISC_TEXT_ENTRY_AGAIN)return portable_text_long_clear(&state,sizeof(state),rc);
    if(rc!=RISC_TEXT_ENTRY_OK||state.struct_size<sizeof(state)||!state.revision||
       (state.flags&~(RISC_TEXT_ENTRY_HARDWARE|RISC_TEXT_ENTRY_PRESENTING|RISC_TEXT_ENTRY_HOME_CANCEL))||state.state>RISC_TEXT_ENTRY_CANCELLED||
       ((state.flags&RISC_TEXT_ENTRY_HOME_CANCEL)&&state.state!=RISC_TEXT_ENTRY_CANCELLED)||
       !memchr(state.text,0,c->capacity))return portable_text_long_clear(&state,sizeof(state),portable_text_client_retain(c));
    for(unsigned i=0;state.text[i];i++)if((unsigned char)state.text[i]<32||(unsigned char)state.text[i]>126)return portable_text_long_clear(&state,sizeof(state),portable_text_client_retain(c));
    int attention=portable_text_adapter_attention();
    if(attention<0)return portable_text_long_clear(&state,sizeof(state),portable_text_client_retain(c));
    if(attention)state.state=RISC_TEXT_ENTRY_CANCELLED;
    *out=state;return portable_text_long_clear(&state,sizeof(state),RISC_TEXT_ENTRY_OK);
}
