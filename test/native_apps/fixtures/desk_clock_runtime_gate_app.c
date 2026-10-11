/* Focused policy/Runtime boundary fixture. The full Clock rendering/lifecycle
 * fixture is paper_desk_clock_test.c; this one must not replace Runtime APIs. */
#include "desk_clock_runtime_gate.h"
#include <PortableDeskClock.h>
#include <RiscDeepSleepV1.h>
#include <RiscRuntimeV1.h>
#include <assert.h>
#include <string.h>
static risc_retained_wake_record_v1 encode(const portable_desk_record *proposal) {
    risc_retained_wake_record_v1 record={.struct_size=sizeof(record),
        .type=PORTABLE_DESK_CLOCK_RECORD_TYPE,.schema_version=PORTABLE_DESK_CLOCK_RECORD_SCHEMA,
        .size=PORTABLE_DESK_CLOCK_RECORD_BYTES};
    assert(portable_desk_encode(proposal,record.payload,record.size));return record;
}
static void blocked(const risc_retained_wake_api_v1 *retained,
                    const risc_retained_wake_record_v1 *original) {
    risc_retained_wake_record_v1 replacement=*original;
    replacement.payload[6]=PORTABLE_DESK_DECO;
    test_desk_gate_safe(false);
    assert(retained->stage(retained->context,&replacement)==RISC_RETAINED_WAKE_CONTEXT);
    assert(retained->clear(retained->context)==RISC_RETAINED_WAKE_CONTEXT);
    test_desk_gate_pending(original); /* Neither rejected call changes staging. */
}
__attribute__((visibility("default"))) void app_main(void) {
    const risc_runtime_api_v1 *runtime=risc_runtime_get_api(1);assert(runtime);
    risc_runtime_capability_v1 retained_grant={.struct_size=sizeof(retained_grant)};
    risc_runtime_capability_v1 provider_grant={.struct_size=sizeof(provider_grant)};
    assert(runtime->acquire(RISC_RETAINED_WAKE_CAPABILITY,1,0,&retained_grant));
    assert(runtime->acquire(DESK_GATE_CAPABILITY,1,7,&provider_grant));
    const risc_retained_wake_api_v1 *retained=retained_grant.api;
    const desk_gate_api *provider=provider_grant.api;
    portable_desk_config config={.face=PORTABLE_DESK_SEGMENTS,.time_format=1};
    strcpy(config.time_zone,"RTC wall time");
    portable_desk_cycle cycle;portable_desk_frame frame;portable_desk_record proposal;
    uint32_t milliseconds=0;
    assert(portable_desk_begin(&cycle,&config,0,false));
    assert(portable_desk_plan_frame(&cycle,120,&frame));
    /* No checkpoint proposal before checked image completion. */
    portable_desk_cycle incomplete=cycle;
    assert(portable_desk_prepare_record(&incomplete,121,0,&proposal,&milliseconds)==PORTABLE_DESK_STOP);
    assert(portable_desk_presented(&cycle,true));
    assert(portable_desk_prepare_record(&cycle,121,0,&proposal,&milliseconds)==PORTABLE_DESK_READY);
    risc_retained_wake_record_v1 record=encode(&proposal);
    test_desk_gate_safe(true);
    assert(retained->stage(retained->context,&record)==RISC_RETAINED_WAKE_OK);
    test_desk_gate_pending(&record);

    /* Each real CPU resource independently fences retained-wake mutations. */
    assert(provider->hold(true));blocked(retained,&record);
    assert(provider->hold(false));test_desk_gate_safe(true);
    assert(provider->lock(true));blocked(retained,&record);
    assert(provider->lock(false));test_desk_gate_safe(true);
    assert(provider->hold(true));assert(provider->lock(true));blocked(retained,&record);

    if(test_desk_gate_mode()==1) {
        /* Preparation crosses the minute. The previous checked image remains
         * staged until both holds and provider lock are restored. */
        assert(portable_desk_prepare_record(&cycle,180,0,&proposal,&milliseconds)==PORTABLE_DESK_REPAINT);
        assert(provider->hold(false));blocked(retained,&record); /* Lock alone. */
        assert(provider->lock(false));test_desk_gate_safe(true);
        assert(retained->clear(retained->context)==RISC_RETAINED_WAKE_OK);
        test_desk_gate_pending(0);
        cycle.full=true;
        assert(portable_desk_plan_frame(&cycle,180,&frame) && frame.full);
        assert(portable_desk_presented(&cycle,true));
        assert(portable_desk_prepare_record(&cycle,181,0,&proposal,&milliseconds)==PORTABLE_DESK_READY);
        record=encode(&proposal);
        assert(retained->stage(retained->context,&record)==RISC_RETAINED_WAKE_OK);
        test_desk_gate_pending(&record);
        assert(provider->hold(true));assert(provider->lock(true));blocked(retained,&record);
    }

    /* Entry does not restage: storage is correctly fenced during preparation.
     * Mode 2 commits at the native terminal boundary and never returns. */
    assert(provider->enter(milliseconds)==RISC_DEEP_SLEEP_ACTIVE_WAKE);
    assert(test_desk_gate_mode()!=2);
    blocked(retained,&record); /* Ordinary refusal does not restore ownership. */
    assert(provider->lock(false));blocked(retained,&record); /* GPIO hold alone. */
    assert(provider->hold(false));test_desk_gate_safe(true);
    assert(retained->clear(retained->context)==RISC_RETAINED_WAKE_OK);
    test_desk_gate_pending(0);
    assert(runtime->release(&provider_grant));
    assert(runtime->release(&retained_grant));
}
