#pragma once
/* Optional Utilities service consumer. No schedule, RTC or output policy here.
 * Every normal call belongs to the foreground serialized settled-frame point.
 * A failed/pending presentation permits only failure_stop, never pump. */
#include "AlarmServiceV1.h"
#include "RiscRuntimeV1.h"
#include <string.h>
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
#include "PortableNativeCustody.h"
/* Additive service result; API1 prefix and copied status layout are unchanged. */
#ifndef ALARM_RETAINED
#define ALARM_RETAINED (-9)
#endif
#endif
typedef struct {
    risc_runtime_capability_v1 grant;
    const alarm_service_v1 *api;
    alarm_status_v1 status;
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
    bool retained;
    alarm_service_sleep_v1 guarded;
#endif
} portable_alarm_client;
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
static inline bool portable_alarm_terminal(const portable_alarm_client *c) {
    return c->retained || portable_adapter_retained();
}
static inline int32_t portable_alarm_result(portable_alarm_client *c,int32_t result) {
    if(result==ALARM_RETAINED) {
        c->retained=true;
        portable_adapter_retain();
    }
    return portable_alarm_terminal(c)?ALARM_RETAINED:result;
}
static inline bool portable_alarm_callable(const portable_alarm_client *c) {
    return !portable_alarm_terminal(c) && c->api;
}
/* The deployment sleep owner receives this guarded, append-only API1 view.
 * Every callback fences its entry and result, including memory-only status. */
static inline int32_t portable_alarm_service_status(void *context,alarm_status_v1 *out) {
    portable_alarm_client *c=context;
    if(!portable_alarm_callable(c))return portable_alarm_terminal(c)?ALARM_RETAINED:ALARM_INVALID;
    int32_t result=portable_alarm_result(c,c->api->status(c->api->context,out));
    if(result==ALARM_OK && out) {
        if(portable_alarm_result(c,out->error)==ALARM_RETAINED)return ALARM_RETAINED;
    }
    return result;
}
#define PORTABLE_ALARM_GUARD0(name) \
static inline int32_t portable_alarm_service_##name(void *context) { \
    portable_alarm_client *c=context; \
    if(!portable_alarm_callable(c))return portable_alarm_terminal(c)?ALARM_RETAINED:ALARM_INVALID; \
    return portable_alarm_result(c,c->api->name(c->api->context)); \
}
PORTABLE_ALARM_GUARD0(step)
PORTABLE_ALARM_GUARD0(refresh)
PORTABLE_ALARM_GUARD0(stop_only)
#undef PORTABLE_ALARM_GUARD0
static inline int32_t portable_alarm_service_acknowledge(void *context,const alarm_token_v1 *token) {
    portable_alarm_client *c=context;
    if(!portable_alarm_callable(c))return portable_alarm_terminal(c)?ALARM_RETAINED:ALARM_INVALID;
    return portable_alarm_result(c,c->api->acknowledge(c->api->context,token));
}
static inline int32_t portable_alarm_service_prepare_sleep(void *context,alarm_sleep_v1 *decision) {
    portable_alarm_client *c=context;
    if(!portable_alarm_callable(c))return portable_alarm_terminal(c)?ALARM_RETAINED:ALARM_INVALID;
    return portable_alarm_result(c,c->api->prepare_sleep(c->api->context,decision));
}
static inline int32_t portable_alarm_service_resume_sleep(void *context,const alarm_sleep_v1 *decision) {
    portable_alarm_client *c=context;
    if(!portable_alarm_callable(c))return portable_alarm_terminal(c)?ALARM_RETAINED:ALARM_INVALID;
    const alarm_service_sleep_v1 *extended=(const alarm_service_sleep_v1 *)c->api;
    if(c->api->struct_size<ALARM_SERVICE_SLEEP_V1_SIZE || !extended->resume_sleep)return ALARM_INVALID;
    return portable_alarm_result(c,extended->resume_sleep(c->api->context,decision));
}
static inline bool portable_alarm_refresh(portable_alarm_client *c) {
    return portable_alarm_callable(c) && portable_alarm_service_refresh(c)!=ALARM_RETAINED;
}
static inline bool portable_alarm_acknowledge(portable_alarm_client *c,const alarm_token_v1 *token) {
    return portable_alarm_callable(c) && portable_alarm_service_acknowledge(c,token)!=ALARM_RETAINED;
}
#endif
static inline bool portable_alarm_open(portable_alarm_client *c,const risc_runtime_api_v1 *r) {
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
    if(portable_adapter_retained())return false;
#endif
    memset(c,0,sizeof(*c));c->grant.struct_size=sizeof(c->grant);
    if(!r->acquire(ALARM_SERVICE_CAPABILITY,1,0,&c->grant)) {
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
        /* Activation may retain Runtime before a grant can be returned. */
        if(!risc_runtime_get_api(1)){c->retained=true;portable_adapter_retain();}
#endif
        return false;
    }
    const alarm_service_v1 *a=c->grant.api;
    if(!a || a->api_version!=1 || a->struct_size<sizeof(*a) || !a->status || !a->step ||
       !a->refresh || !a->acknowledge || !a->prepare_sleep || !a->stop_only)return false;
    c->api=a;
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
    c->guarded.base=(alarm_service_v1){1,sizeof(alarm_service_v1),c,
        portable_alarm_service_status,portable_alarm_service_step,portable_alarm_service_refresh,
        portable_alarm_service_acknowledge,portable_alarm_service_prepare_sleep,portable_alarm_service_stop_only};
    if(a->struct_size>=ALARM_SERVICE_SLEEP_V1_SIZE && ((const alarm_service_sleep_v1 *)a)->resume_sleep) {
        c->guarded.base.struct_size=sizeof(c->guarded);
        c->guarded.resume_sleep=portable_alarm_service_resume_sleep;
    }
#endif
    return true;
}
static inline bool portable_alarm_status(portable_alarm_client *c) {
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
    if(!portable_alarm_callable(c))return false;
#endif
    c->status=(alarm_status_v1){.struct_size=sizeof(c->status)};
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
    return portable_alarm_service_status(c,&c->status)==ALARM_OK &&
#else
    return c->api && c->api->status(c->api->context,&c->status)==ALARM_OK &&
#endif
        c->status.api_version==1 && c->status.struct_size>=sizeof(c->status) &&
        c->status.state<=ALARM_STATE_CUE;
}
static inline bool portable_alarm_pump(portable_alarm_client *c) {
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
    if(portable_alarm_service_step(c)==ALARM_RETAINED)return false;
#else
    (void)c->api->step(c->api->context);
#endif
    return portable_alarm_status(c);
}
static inline bool portable_alarm_owned(const portable_alarm_client *c) {
    return c->status.state==ALARM_STATE_CUE || c->status.occurrence.generation || c->status.output_uncertain ||
        c->status.state==ALARM_STATE_ALERT || c->status.state==ALARM_STATE_DISMISSING;
}
/* A cue reserves outputs but is not a foreground modal occurrence. */
static inline bool portable_alarm_cue(const portable_alarm_client *c) {
    return c->status.state==ALARM_STATE_CUE;
}
static inline bool portable_alarm_failure_stop(portable_alarm_client *c) {
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
    if(portable_alarm_terminal(c))return false;
#endif
    if(!c->api)return true;
    int32_t result=ALARM_PENDING;
    /* Exactly three independent operations, never normal I/O or output start.
       Do not automatically repeat after an uncertain result. */
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
    for(unsigned i=0;i<3 && result==ALARM_PENDING;i++)result=portable_alarm_service_stop_only(c);
#else
    for(unsigned i=0;i<3 && result==ALARM_PENDING;i++)result=c->api->stop_only(c->api->context);
#endif
    return result==ALARM_OK;
}
static inline bool portable_alarm_close(portable_alarm_client *c,const risc_runtime_api_v1 *r) {
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
    if(portable_alarm_terminal(c))return false;
#endif
    if(c->grant.api && !r->release(&c->grant)) {
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
        if(!risc_runtime_get_api(1)){c->retained=true;portable_adapter_retain();}
#endif
        return false;
    }
#ifdef PORTABLE_ALARM_TERMINAL_RETENTION
    if(portable_alarm_terminal(c))return false;
#endif
    memset(c,0,sizeof(*c));return true;
}
