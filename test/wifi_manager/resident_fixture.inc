/* Real resident checkpoint boundary for the actual Wi-Fi app fixture.
 * Include after edge()/radio fixture globals and before the runtime API.
 * Default is the legacy candidate; the integrated fixture should define
 * WIFI_MANAGER_STORAGE_PENDING() as storage_pending(&wm). */
#ifndef WIFI_MANAGER_STORAGE_PENDING
#define WIFI_MANAGER_STORAGE_PENDING() false
#endif
static bool resident_fixture;
static unsigned resident_polls,resident_controls;
static void pump(unsigned);
static void invoke(unsigned,int);
static void exit_app(void);
static int32_t resident_checkpoint_fixture(uint64_t invocation,const risc_resident_request_v1 *request,risc_resident_reply_v1 *reply){
    edge("resident-checkpoint");assert(invocation==1&&request&&reply);
    /* Runtime crosses its native exit barrier even for a POLL. Inhibition
     * flags do not permit crossing while a native async job is owned. */
    assert(!radio_id&&!wm.operation&&!wm.service&&!WIFI_MANAGER_STORAGE_PENDING());
    assert(!held&&!presenting);
    reply->flags=0;
    if(request->reason==RISC_RESIDENT_CHECKPOINT_POLL){resident_polls++;return RISC_RESIDENT_OK;}
    assert(request->reason==RISC_RESIDENT_CHECKPOINT_CONTROLS);
    assert(!wm.session&&!wm.editor_session&&!subscribed);
    resident_controls++;reply->flags=RISC_RESIDENT_REPLY_CONFIGURATION_CHANGED;
    return RISC_RESIDENT_OK;
}
static bool rt_resident_fixture(risc_resident_client_v1 *out){
    if(!resident_fixture)return false;
    *out=(risc_resident_client_v1){.api_version=1,.struct_size=sizeof(*out),
        .invocation=1,.role=RISC_RESIDENT_ROLE_FOREGROUND,.checkpoint=resident_checkpoint_fixture};
    return true;
}
static void resident_fixture_run(void){
    assert(wm.resident.enabled);
    /* Wait for the real presenter safe point, not a fixed startup frame count. */
    for(unsigned n=0;!resident_polls;n++){assert(n<1000);pump(1);}
    unsigned before=resident_polls;
    invoke(SCAN,0);pump(100);assert(radio_id&&wm.operation&&resident_polls==before);
    stop_delay=8;wm.shared_reason=RISC_RESIDENT_CHECKPOINT_CONTROLS;pump(100);
    assert(!radio_id&&!wm.operation&&resident_controls==1&&wm.session);
    assert(resident_polls>before);
    before=resident_polls;invoke(SCAN,0);pump(100);assert(resident_polls==before);
    invoke(CANCEL,0);pump(100);assert(!radio_id&&resident_polls>before);
    exit_app();puts("resident checkpoints defer owned radio and drain controls handoff PASS");
}
