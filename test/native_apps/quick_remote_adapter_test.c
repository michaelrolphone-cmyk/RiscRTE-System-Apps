/* Includes the actual resident adapter and renderer. Only fake capability
 * callbacks are linked: this binary cannot perform RF or persistent writes. */
#include "../../lib/PortableApps/src/adapter.c"
#include <assert.h>
#include <stdio.h>

/* Empty foreground catalogue is never used by this focused adapter harness. */
const t5_app_manifest_t portable_catalog[1]={{.compatible=false}};
const unsigned portable_catalog_count=0;

static struct {
    bool close_ok, release_ok, scan_active, restore_failed;
    unsigned acquires, opens, polls, statuses, devices, closes, releases;
    unsigned ordinary_calls, retains, clocks;
    uint32_t count;
    risc_ble_sensor_device_v1 rows[PQB_MAX_CANDIDATES];
} fixture;
static risc_bluetooth_sensors_v1 sensor_api;
static risc_runtime_api_v1 fake_runtime;

static bool mock_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *grant) {
    assert(!fixture.retains);
    if(strcmp(name,RISC_BLUETOOTH_SENSORS_CAPABILITY)) {
        fixture.ordinary_calls++;return false;
    }
    assert(version==1 && instance==0 && grant->struct_size==sizeof(*grant));
    fixture.acquires++;
    *grant=(risc_runtime_capability_v1){.struct_size=sizeof(*grant),.slot=17,.generation=29,.api=&sensor_api};
    return true;
}
static bool mock_release(risc_runtime_capability_v1 *grant) {
    assert(!fixture.retains && grant->api==&sensor_api);
    assert(grant->slot==17 && grant->generation==29);
    fixture.releases++;
    if(!fixture.release_ok)return false;
    memset(grant,0,sizeof(*grant));return true;
}
static bool mock_open(void *context,uint64_t *token) {
    assert(context==&fixture && !fixture.retains && !*token);
    fixture.opens++;fixture.scan_active=true;*token=UINT64_C(0x123456789abcdef0);return true;
}
static bool mock_poll(void *context,uint64_t token,uint32_t maximum) {
    assert(context==&fixture && !fixture.retains && token==UINT64_C(0x123456789abcdef0));
    assert(maximum==PQB_POLL_EVENTS);fixture.polls++;return true;
}
static bool mock_status(void *context,risc_ble_sensor_status_v1 *status) {
    assert(context==&fixture && !fixture.retains);fixture.statuses++;
    assert(status->struct_size==sizeof(*status));
    *status=(risc_ble_sensor_status_v1){.struct_size=sizeof(*status),
        .state=fixture.scan_active?RISC_BLE_SENSOR_SCANNING:RISC_BLE_SENSOR_COMPLETE,
        .count=fixture.count,.cleanup_pending=fixture.scan_active,.restore_failed=fixture.restore_failed};
    return true;
}
static bool mock_device(void *context,uint32_t index,risc_ble_sensor_device_v1 *device) {
    assert(context==&fixture && !fixture.retains && index<fixture.count);
    fixture.devices++;*device=fixture.rows[index];return true;
}
static bool mock_close(void *context,uint64_t token) {
    assert(context==&fixture && !fixture.retains && token==UINT64_C(0x123456789abcdef0));
    fixture.closes++;if(fixture.close_ok)fixture.scan_active=false;return fixture.close_ok;
}
static bool mock_retain(void) {assert(!fixture.retains);fixture.retains++;return true;}
static bool mock_health(risc_runtime_health_v1 *health) {
    assert(!fixture.retains);fixture.clocks++;
    *health=(risc_runtime_health_v1){.struct_size=sizeof(*health),.uptime_ms=0};return true;
}
static bool mock_monotonic(uint32_t *now) {
    assert(!fixture.retains);fixture.clocks++;*now=0;return true;
}
static unsigned provider_calls(void) {
    return fixture.acquires+fixture.opens+fixture.polls+fixture.statuses+
        fixture.devices+fixture.closes+fixture.releases+fixture.ordinary_calls;
}
static void reset_fixture(void) {
    memset(&fixture,0,sizeof(fixture));fixture.close_ok=fixture.release_ok=true;
    sensor_api=(risc_bluetooth_sensors_v1){1,sizeof(sensor_api),&fixture,
        mock_open,mock_poll,mock_status,mock_device,mock_close};
    fake_runtime=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(fake_runtime),
        .acquire=mock_acquire,.release=mock_release,.retain_invocation=mock_retain,
        .health=mock_health,.monotonic_ms=mock_monotonic};
    rt=native_custody_runtime=&fake_runtime;
    failed=native_custody_retained=native_sleep_retained=alarm_modal=false;
    quick_modal=true;home_pending=crown_pending=handoff_requested=false;navigation_pending=0;
    surface=(risc_display_surface_v1){0};
    memset(&quick,0,sizeof(quick));memset(&quick_remote,0,sizeof(quick_remote));
    memset(&quick_remote_shown,0,sizeof(quick_remote_shown));quick_remote_restore_reported=false;
#ifdef PORTABLE_QUICK_ATOMIC_PREFERENCES
    quick_atomic_grant=(risc_runtime_capability_v1){0};quick_atomic_edit=(portable_preference_atomic){0};
    memset(quick_atomic_key,0,sizeof(quick_atomic_key));memset(quick_atomic_queue,0,sizeof(quick_atomic_queue));
    quick_atomic_running=quick_atomic_attempted=false;quick_atomic_revision=0;
    quick_atomic_contact=quick_atomic_captured_contact=quick_atomic_delivered_contact=0;
    quick_atomic_discard();
#endif
    assert(pqb_init(&quick_remote));pqa_init(&quick.ui);
    quick.ui.paper=quick.ui.remote_battery_controls=true;
    quick.ui.radio_controls=quick.ui.radios_valid=quick.ui.bluetooth_enabled=true;
    quick.ui.brightness_valid=quick.ui.applied_brightness_valid=true;
    quick.ui.brightness=quick.ui.applied_brightness=40;
    quick.ui.position_q8=quick.ui.target_q8=pqa_open_q8(&quick.ui);
    quick.ui.remote_battery_epoch=19;
    memset(&quick_radios,0,sizeof(quick_radios));
    quick_radios.valid=quick_radios.available=true;
    quick_radios.flags=PORTABLE_RADIO_BLUETOOTH;
}
static void measurement(unsigned index,unsigned id,unsigned type,unsigned percent) {
    assert(index<PQB_MAX_CANDIDATES && percent<=100);
    risc_ble_sensor_device_v1 *row=&fixture.rows[index];memset(row,0,sizeof(*row));
    row->address[0]=(uint8_t)id;row->address[1]=0x12;row->address[2]=0x34;
    row->address[3]=0x56;row->address[4]=0x78;row->address[5]=0x90;
    row->address_type=(uint8_t)type;memcpy(row->name,"Same watch name",16);
    row->has_battery=true;row->battery=(uint8_t)percent;row->reading_count=1;
    row->readings[0]=(risc_ble_reading_v1){RISC_TELEMETRY_BATTERY_PERCENT,(int32_t)percent};
    row->reports=1;row->measurement_age_ms=0;
    if(fixture.count<=index)fixture.count=index+1;
}
static uint32_t tap(int x,int y) {
    assert(pqa_input(&quick.ui,10,true,1,7,x,y,true));
    assert(!pqa_take_action(&quick.ui));
    assert(pqa_input(&quick.ui,20,true,0,7,0,0,true));
    return pqa_take_action(&quick.ui);
}
static void click(int x,int y,uint32_t expected) {
    uint32_t action=tap(x,y);assert(action==expected);assert(quick_apply(action));
}
static void open_page(void) {click(340,770,PQA_REMOTE_OPEN);assert(quick.ui.remote_battery_page);}
static void poll_rows(void) {
    assert(pqb_poll(&quick_remote));quick_remote_sync();
    assert(quick.ui.remote_battery_count==quick_remote.count);
}
static void test_open_and_scan_policy(void) {
    for(unsigned flags=0;flags<8;flags++) {
        reset_fixture();quick.ui.radios_valid=!!(flags&1);
        quick.ui.bluetooth_enabled=!!(flags&2);quick.ui.airplane=!!(flags&4);
        open_page();assert(!provider_calls() && !quick_remote.active && !quick_remote.selected);
        for(unsigned i=0;i<50;i++){assert(quick_apply(0));assert(!provider_calls());}
        click(240,110,PQA_REMOTE_SCAN);
        bool allowed=flags==3;
        assert(quick_remote.active==allowed && fixture.acquires==(unsigned)allowed);
        assert(fixture.opens==(unsigned)allowed && !quick_remote.selected && !quick_remote.current);
        if(!allowed)assert(quick_remote.status==PQB_DISABLED && !provider_calls());
        assert(quick_remote_close());
    }
}
static void test_closed_or_alarm_admission(void) {
    for(unsigned gate=0;gate<8;gate++) {
        reset_fixture();open_page();
        uint32_t action=tap(240,110);assert(action==PQA_REMOTE_SCAN);
        if(gate==0)quick_modal=false;
        if(gate==1)quick.ui.position_q8=quick.ui.target_q8=0;
        if(gate==2)quick.ui.target_q8=0;
        if(gate==3)alarm_modal=true;
        if(gate==4)quick.ui.remote_battery_epoch++;
        if(gate==5)home_pending=true;
        if(gate==6)crown_pending=true;
        if(gate==7)handoff_requested=true;
        assert(quick_apply(action));
        assert(!provider_calls() && !quick_remote.active);
    }
    /* The actual queued intent order that exposed the teardown race. This is
     * the real admission helper, not the optional atomic queue implementation. */
    reset_fixture();open_page();click(240,110,PQA_REMOTE_SCAN);
    uint32_t stop=tap(384,110),scan=tap(240,110);
    assert(stop==PQA_REMOTE_STOP && scan==PQA_REMOTE_SCAN);
    quick.ui.target_q8=0;
    assert(quick_remote_close());unsigned calls=provider_calls();
    assert(quick_apply(stop) && quick_apply(scan));
    assert(provider_calls()==calls && fixture.opens==1 && !quick_remote.active);
}
static void test_explicit_selection_and_paging(void) {
    reset_fixture();for(unsigned i=0;i<13;i++)measurement(i,i+1,i%2,20+i);
    open_page();click(240,110,PQA_REMOTE_SCAN);poll_rows();
    assert(!quick_remote.selected && !quick_remote.last_valid && !quick_remote.current);
    uint32_t epoch=quick.ui.remote_battery_epoch;
    click(360,688,PQA_REMOTE_NEXT);assert(quick.ui.remote_battery_offset==6);
    assert(quick.ui.remote_battery_epoch==epoch+1);
    unsigned calls=provider_calls();click(240,340,PQA_REMOTE_SELECT);
    assert(provider_calls()==calls && quick.ui.action_remote_index==7);
    assert(quick_remote.selected && quick_remote.current && quick_remote.last_percent==27);
    assert(quick_remote.selected_address[0]==8 && quick_remote.selected_address_type==1);
    click(360,688,PQA_REMOTE_NEXT);assert(quick.ui.remote_battery_offset==12);
    click(360,688,PQA_REMOTE_NEXT);assert(quick.ui.remote_battery_offset==12);
    click(120,688,PQA_REMOTE_PREVIOUS);assert(quick.ui.remote_battery_offset==6);
    click(120,688,PQA_REMOTE_PREVIOUS);assert(!quick.ui.remote_battery_offset);
    click(120,688,PQA_REMOTE_PREVIOUS);assert(!quick.ui.remote_battery_offset);
    fixture.rows[0].measurement_invalid=true;poll_rows();
    click(240,276,PQA_REMOTE_SELECT);assert(quick_remote.selected_address[0]==8);
    assert(quick_remote.last_percent==27 && quick_remote.current);
    assert(quick_remote_close());
}
static void test_stale_queued_selection(void) {
    reset_fixture();measurement(0,1,0,45);measurement(1,2,1,77);
    open_page();click(240,110,PQA_REMOTE_SCAN);poll_rows();
    uint32_t action=tap(240,340);assert(action==PQA_REMOTE_SELECT);
    uint32_t epoch=quick.ui.action_remote_epoch;
    fixture.rows[1].address[0]=3;poll_rows();
    assert(quick.ui.remote_battery_epoch!=epoch);
    assert(quick_apply(action));assert(!quick_remote.selected && !quick_remote.last_valid);
    assert(quick.ui.remote_battery_count==3); /* Original identities remain in this scan. */
    click(240,404,PQA_REMOTE_SELECT);assert(quick_remote.selected_address[0]==3 && quick_remote.last_percent==77);
    assert(quick_remote_close());
}
static void test_stop_and_back_keep_value(void) {
    for(unsigned back=0;back<2;back++) {
        reset_fixture();measurement(0,1,0,63);open_page();click(240,110,PQA_REMOTE_SCAN);
        poll_rows();click(240,276,PQA_REMOTE_SELECT);assert(quick_remote.current);
        click(back?96:384,110,back?PQA_REMOTE_BACK:PQA_REMOTE_STOP);
        assert(!quick_remote.active && !quick_remote.current && quick_remote.last_valid);
        assert(quick_remote.last_percent==63 && quick_remote.selected);
        assert(quick.ui.remote_battery_page==!back && fixture.closes==1 && fixture.releases==1);
        assert(strstr(quick_remote_status_text(),"NOT CURRENT"));
    }
}
static void test_navigation_back(void) {
    for(unsigned reason=0;reason<4;reason++) {
        reset_fixture();measurement(0,1,0,63);open_page();click(240,110,PQA_REMOTE_SCAN);
        poll_rows();click(240,276,PQA_REMOTE_SELECT);
        uint32_t queued=tap(240,110);assert(queued==PQA_REMOTE_SCAN);
        home_pending=reason==1;crown_pending=reason==2;handoff_requested=reason==3;
        assert(quick_remote_back());
        assert(!quick_remote.active && !quick_remote.current && quick_remote.last_valid);
        assert(quick_remote.last_percent==63 && fixture.closes==1 && fixture.releases==1);
        assert(home_pending==(reason==1) && crown_pending==(reason==2) && handoff_requested==(reason==3));
        if(!reason)assert(!quick.ui.remote_battery_page && quick.ui.target_q8==PQA_OPEN_Q8);
        else assert(quick.ui.remote_battery_page && !quick.ui.target_q8);
        unsigned calls=provider_calls();assert(quick_apply(queued));assert(provider_calls()==calls);
    }
    reset_fixture();assert(quick_remote_back());assert(!quick.ui.target_q8 && !provider_calls());
}
static void test_cleanup_failure_fence(void) {
    for(unsigned mode=0;mode<3;mode++) {
        reset_fixture();measurement(0,1,0,63);open_page();click(240,110,PQA_REMOTE_SCAN);
        poll_rows();click(240,276,PQA_REMOTE_SELECT);
        uint64_t token=quick_remote.token;risc_runtime_capability_v1 grant=quick_remote.grant;
        if(mode==2)fixture.release_ok=false;else fixture.close_ok=false;
        uint32_t action=mode==1?PQA_BLUETOOTH:tap(384,110);
        assert(!quick_apply(action));
        assert(fixture.retains==1 && native_custody_retained && native_sleep_retained && failed);
        assert(quick_remote.retained && !quick_remote.current && quick_remote.last_valid);
        assert(quick_remote.last_percent==63 && quick_remote.grant_owned);
        assert(!memcmp(&quick_remote.grant,&grant,sizeof(grant)));
        assert(quick_remote.token==(mode==2?0:token));
        assert(fixture.closes==1 && fixture.releases==(mode==2?1u:0u));
        unsigned calls=provider_calls(),clocks=fixture.clocks;
        for(unsigned i=0;i<20;i++) {
            assert(!quick_apply(PQA_BLUETOOTH|PQA_BRIGHTNESS_COMMIT));
            assert(!quick_apply(PQA_REMOTE_SCAN));assert(!quick_apply(0));
        }
        assert(provider_calls()==calls && fixture.clocks==clocks && !fixture.ordinary_calls && fixture.retains==1);
    }
}
#ifdef PORTABLE_QUICK_ATOMIC_PREFERENCES
static void queue_tap(int x,int y) {
    quick_atomic_contact++;
    assert(pqa_input(&quick.ui,10,true,1,7,x,y,true));
    assert(!pqa_take_action(&quick.ui));
    assert(pqa_input(&quick.ui,20,true,0,7,0,0,true));
    quick_atomic_enqueue();
}
static void test_atomic_queue(void) {
    reset_fixture();for(unsigned i=0;i<13;i++)measurement(i,i+1,i%2,20+i);
    open_page();click(240,110,PQA_REMOTE_SCAN);poll_rows();click(360,688,PQA_REMOTE_NEXT);
    uint32_t epoch=quick.ui.remote_battery_epoch;
    queue_tap(240,340);queue_tap(240,404);assert(quick_atomic_queue_count==2);
    quick.ui.action_remote_index=30;quick.ui.action_remote_epoch=epoch+1;
    unsigned calls=provider_calls();uint32_t action=quick_atomic_take_action();
    assert(action==PQA_REMOTE_SELECT && quick.ui.action_remote_index==7 && quick.ui.action_remote_epoch==epoch);
    assert(quick_apply(action) && quick_remote.selected_address[0]==8 && quick_remote.last_percent==27);
    action=quick_atomic_take_action();
    assert(action==PQA_REMOTE_SELECT && quick.ui.action_remote_index==8 && quick.ui.action_remote_epoch==epoch);
    assert(quick_apply(action) && quick_remote.selected_address[0]==9 && quick_remote.last_percent==28);
    assert(provider_calls()==calls && !quick_atomic_queue_count && !quick_atomic_take_action());
    queue_tap(240,276);fixture.rows[6].name[0]='N';poll_rows();
    assert(quick.ui.remote_battery_epoch!=epoch);
    action=quick_atomic_take_action();assert(action==PQA_REMOTE_SELECT && quick.ui.action_remote_epoch==epoch);
    assert(quick_apply(action) && quick_remote.selected_address[0]==9);
    assert(quick_remote_close());
    for(unsigned order=0;order<2;order++) {
        reset_fixture();open_page();click(240,110,PQA_REMOTE_SCAN);
        queue_tap(384,110);queue_tap(240,110);assert(quick_atomic_queue_count==2);
        if(order==1) {
            action=quick_atomic_take_action();assert(action==PQA_REMOTE_STOP && quick_apply(action));
        }
        pqa_close(&quick.ui);assert(quick_remote_close());calls=provider_calls();
        while(quick_atomic_queue_count) {
            action=quick_atomic_take_action();
            assert(action==(quick_atomic_queue_count?PQA_REMOTE_STOP:PQA_REMOTE_SCAN));
            assert(quick_apply(action));
        }
        assert(provider_calls()==calls && fixture.opens==1 && fixture.closes==1 && !quick_remote.active);
    }
}
#endif
static void test_restoration_failure(void) {
    reset_fixture();measurement(0,1,0,63);open_page();click(240,110,PQA_REMOTE_SCAN);
    poll_rows();click(240,276,PQA_REMOTE_SELECT);
    fixture.restore_failed=true;
    click(384,110,PQA_REMOTE_STOP);
    assert(quick_remote.restoration_failed && quick_remote.status==PQB_ERROR);
    assert(!quick_remote.active && !quick_remote.current && quick_remote.last_valid);
    assert(quick_remote.last_percent==63 && fixture.closes==1 && fixture.releases==1);
    assert(!fixture.retains && !native_custody_retained && !native_sleep_retained && !failed);
    assert(!quick.ui.radios_valid && !quick.ui.bluetooth_enabled && (quick.ui.error_flags&PQA_ERROR_RADIO));
    assert(strstr(quick_remote_status_text(),"RESTORATION UNCONFIRMED"));
    unsigned calls=provider_calls();
    for(unsigned i=0;i<20;i++) {quick_remote_sync();assert(quick_apply(0));}
    assert(provider_calls()==calls && !fixture.ordinary_calls);
    click(240,110,PQA_REMOTE_SCAN);
    assert(provider_calls()==calls && fixture.opens==1 && !quick_remote.active);
    assert(quick_remote.last_percent==63 && quick_remote.last_valid && !quick_remote.current);
}
static uint32_t region_hash(const uint8_t *pixels,unsigned stride,unsigned left,unsigned top,unsigned right,unsigned bottom) {
    uint32_t hash=UINT32_C(2166136261);
    for(unsigned y=top;y<bottom;y++)for(unsigned x=left;x<right;x++) {
        hash^=!!(pixels[y*stride+x/8]&(128u>>(x%8)));hash*=UINT32_C(16777619);
    }
    return hash;
}
static void render_capture(const char *directory,const char *name) {
    enum { W=480,H=800,STRIDE=63,GUARD=32 };
    uint8_t pixels[STRIDE*H+GUARD*2];memset(pixels,0xa5,sizeof(pixels));
    for(unsigned y=0;y<H;y++)memset(pixels+GUARD+y*STRIDE,0,W/8);
    info=(risc_display_info_v1){.struct_size=sizeof(info),.width=W,.height=H};
    surface=(risc_display_surface_v1){.frame=1,.pixels=pixels+GUARD,.width=W,.height=H,
        .stride_bytes=STRIDE,.size_bytes=STRIDE*H,.pixel_format=RISC_DISPLAY_FORMAT_MONO1};
    surface_format=RISC_DISPLAY_FORMAT_MONO1;paper_rotated=false;qp_offset=0;
    unsigned before=provider_calls();quick_paper_paint("12:34",true,74);
    assert(!failed && provider_calls()==before);
    for(unsigned i=0;i<GUARD;i++)assert(pixels[i]==0xa5 && pixels[GUARD+STRIDE*H+i]==0xa5);
    unsigned ink=0;
    for(unsigned y=0;y<H;y++) {
        for(unsigned x=W/8;x<STRIDE;x++)assert(pixels[GUARD+y*STRIDE+x]==0xa5);
        for(unsigned x=0;x<W/8;x++)ink+=pixels[GUARD+y*STRIDE+x]!=0;
    }
    assert(ink>500);
    if(!quick.ui.remote_battery_page) {
        uint32_t original=region_hash(pixels+GUARD,STRIDE,32,744,238,796);
        for(unsigned y=0;y<H;y++)memset(pixels+GUARD+y*STRIDE,0,W/8);
        quick_paper_paint("12:34",true,25);
        assert(original!=region_hash(pixels+GUARD,STRIDE,32,744,238,796));
        for(unsigned y=0;y<H;y++)memset(pixels+GUARD+y*STRIDE,0,W/8);
        quick_paper_paint("12:34",true,74);
    }
    if(directory) {
        char path[1024];int n=snprintf(path,sizeof(path),"%s/%s.pbm",directory,name);
        assert(n>0 && (size_t)n<sizeof(path));FILE *out=fopen(path,"wb");assert(out);
        assert(fprintf(out,"P4\n%d %d\n",W,H)>0);
        for(unsigned y=0;y<H;y++)assert(fwrite(pixels+GUARD+y*STRIDE,1,W/8,out)==W/8);
        assert(!fclose(out));
    }
    surface=(risc_display_surface_v1){0};
}
static void test_renderer(const char *directory) {
    const char *labels="WATCH BATTERY0123456789%";
    for(unsigned i=0;labels[i];i++)if(labels[i]!=' ') {
        const quick_ref_glyph *g=&QUICK_REF_TILE.glyphs[(unsigned)labels[i]-32];
        assert(g->bits && g->width && g->height && g->advance_q6);
    }
    reset_fixture();render_capture(directory,"quick-summary-unselected");
    open_page();render_capture(directory,"remote-idle");
    for(unsigned i=0;i<8;i++)measurement(i,i+1,i%2,20+i);
    click(240,110,PQA_REMOTE_SCAN);poll_rows();render_capture(directory,"remote-candidates");
    click(240,340,PQA_REMOTE_SELECT);render_capture(directory,"remote-current");
    click(360,688,PQA_REMOTE_NEXT);render_capture(directory,"remote-page-two");
    click(384,110,PQA_REMOTE_STOP);render_capture(directory,"remote-last-not-current");
    click(96,110,PQA_REMOTE_BACK);render_capture(directory,"quick-summary-last-not-current");
}
int main(int argc,char **argv) {
    assert(argc<=2);setvbuf(stdout,NULL,_IONBF,0);
    test_open_and_scan_policy();puts("actual adapter: open without acquisition and 8 radio-policy combinations PASS");
    test_closed_or_alarm_admission();puts("actual adapter: closing/invisible/non-modal/alarm/stale scans and queued teardown PASS");
    test_explicit_selection_and_paging();puts("actual adapter: explicit identity selection, invalid rows, next/previous PASS");
    test_stale_queued_selection();puts("actual adapter: changed candidate epoch discards queued selection PASS");
    test_stop_and_back_keep_value();puts("actual adapter: Stop/Back preserve last value and mark not-current PASS");
    test_navigation_back();puts("actual adapter: Back returns page; Home/crown/handoff preserve terminal navigation and close sheet PASS");
    test_cleanup_failure_fence();puts("actual adapter: close/release failure retains invocation before other provider calls PASS");
    #ifdef PORTABLE_QUICK_ATOMIC_PREFERENCES
    test_atomic_queue();puts("actual atomic queue: captured indices/epochs, stale discard, STOP then SCAN then dismissal PASS");
#endif
    test_restoration_failure();puts("actual adapter: RF-safe close with restoration failure invalidates radio state without enabling or retention PASS");
    test_renderer(argc==2?argv[1]:NULL);puts("actual renderer: 7 paper states, bounds/padding, no sensor I/O PASS");
    return 0;
}
