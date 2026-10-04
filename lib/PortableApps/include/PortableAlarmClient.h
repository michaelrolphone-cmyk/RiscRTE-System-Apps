#pragma once
/* Optional Utilities service consumer. No schedule, RTC or output policy here.
 * Every normal call belongs to the foreground serialized settled-frame point.
 * A failed/pending presentation permits only failure_stop, never pump. */
#include "AlarmServiceV1.h"
#include "RiscRuntimeV1.h"
#include <string.h>
typedef struct {
    risc_runtime_capability_v1 grant;
    const alarm_service_v1 *api;
    alarm_status_v1 status;
} portable_alarm_client;
static inline bool portable_alarm_open(portable_alarm_client *c,const risc_runtime_api_v1 *r) {
    memset(c,0,sizeof(*c));c->grant.struct_size=sizeof(c->grant);
    if(!r->acquire(ALARM_SERVICE_CAPABILITY,1,0,&c->grant))return false;
    const alarm_service_v1 *a=c->grant.api;
    if(!a || a->api_version!=1 || a->struct_size<sizeof(*a) || !a->status || !a->step ||
       !a->refresh || !a->acknowledge || !a->prepare_sleep || !a->stop_only)return false;
    c->api=a;return true;
}
static inline bool portable_alarm_status(portable_alarm_client *c) {
    c->status=(alarm_status_v1){.struct_size=sizeof(c->status)};
    return c->api && c->api->status(c->api->context,&c->status)==ALARM_OK &&
        c->status.api_version==1 && c->status.struct_size>=sizeof(c->status) &&
        c->status.state<=ALARM_STATE_BLOCKED;
}
static inline bool portable_alarm_pump(portable_alarm_client *c) {
    (void)c->api->step(c->api->context);return portable_alarm_status(c);
}
static inline bool portable_alarm_owned(const portable_alarm_client *c) {
    return c->status.occurrence.generation || c->status.output_uncertain ||
        c->status.state==ALARM_STATE_ALERT || c->status.state==ALARM_STATE_DISMISSING;
}
static inline bool portable_alarm_failure_stop(portable_alarm_client *c) {
    if(!c->api)return true;
    int32_t result=ALARM_PENDING;
    /* Exactly three independent operations, never normal I/O or output start.
       Do not automatically repeat after an uncertain result. */
    for(unsigned i=0;i<3 && result==ALARM_PENDING;i++)result=c->api->stop_only(c->api->context);
    return result==ALARM_OK;
}
static inline bool portable_alarm_close(portable_alarm_client *c,const risc_runtime_api_v1 *r) {
    if(c->grant.api && !r->release(&c->grant))return false;
    memset(c,0,sizeof(*c));return true;
}
