/* Actual shared-scene app with copied namespace fixtures. These qualify app
 * control flow, not physical durability or provider NativeReplicaCore internals. */
static void storage_test_draft(void){
    invoke(ADD_NETWORK,0);
    copy_text(wm.draft.ssid,sizeof(wm.draft.ssid),"Fixture network");
    copy_text(wm.draft.password,sizeof(wm.draft.password),"testpass123");
    copy_text(wm.selected.ssid,sizeof(wm.selected.ssid),wm.draft.ssid);
    wm.selected.auth=2;wm.draft_dirty=true;
}
static void storage_test_record(void){
    portable_wifi_credentials got;
    assert(!portable_wifi_profile_load(&kv,0,&got));
    assert(!strcmp(got.ssid,"Fixture network")&&!strcmp(got.password,"testpass123"));
    wipe(&got,sizeof(got));
}
static void storage_test_submitted(void){
    fixture_profile.collect_delay=1000;
    wifi_manager_test_action(SAVE_DRAFT,0);
    for(unsigned n=0;!fixture_profile.token;n++){assert(n<100);pump(1);}
    assert(storage_pending(&wm)&&!kv_writes);
}
static void storage_test_run(const char *mode){
    storage_test_draft();
    if(!strcmp(mode,"storage-save")){
        invoke(SAVE_DRAFT,0);pump(100);
        assert(fixture_profile.submissions==1&&fixture_profile.collections==1);
        assert(!storage_pending(&wm)&&!wm.draft_dirty);storage_test_record();
    }else if(!strcmp(mode,"storage-cancel")||!strcmp(mode,"storage-commit-race")){
        storage_test_submitted();
        fixture_profile.commit_wins=!strcmp(mode,"storage-commit-race");
        wifi_manager_test_action(BACK,0);assert(fixture_profile.cancellations==1);
        assert(storage_pending(&wm));fixture_profile.collect_delay=0;pump(100);
        assert(!storage_pending(&wm)&&fixture_profile.collections==1);
        if(fixture_profile.commit_wins){assert(!wm.draft_dirty);storage_test_record();}
        else assert(!kv_writes&&wm.draft_dirty&&!strcmp(wm.draft.password,"testpass123"));
    }else if(!strcmp(mode,"storage-stale-join")){
        wifi_manager_test_action(CONNECT,0);
        for(unsigned n=0;!wm.atomic_ready;n++){assert(n<100);pump(1);}
        assert(!fixture_profile.submissions);++fixture_profile.revision;
        pump(100);
        assert(joins==1&&wm.link_up&&!fixture_profile.submissions&&!kv_writes);
        assert(wm.draft_dirty&&!strcmp(wm.draft.password,"testpass123"));
        assert(strstr(wm.feedback,"changed"));
    }else if(!strcmp(mode,"storage-generation")){
        ++fixture_profile.availability_generation;fixture_profile.availability=RISC_KEY_VALUE_CONFLICT;
        invoke(SAVE_DRAFT,0);assert(!fixture_profile.submissions&&wm.draft_dirty);
        assert(!strcmp(wm.draft.password,"testpass123"));
        ++fixture_profile.availability_generation;fixture_profile.availability=RISC_KEY_VALUE_AVAILABLE;
        pump(100);assert(!fixture_profile.submissions&&!kv_writes&&!store_cached_ready(&wm.network_storage));
        invoke(STORAGE_RELOAD,0);invoke(KEEP_DRAFT,0);
        assert(wm.draft_dirty&&!strcmp(wm.draft.password,"testpass123"));
        invoke(STORAGE_RELOAD,0);invoke(DISCARD_RELOAD,0);pump(100);
        assert(!wm.draft_dirty&&store_cached_ready(&wm.network_storage)&&!kv_writes);
    }else if(!strcmp(mode,"storage-generation-inflight")){
        storage_test_submitted();
        ++fixture_profile.availability_generation;fixture_profile.availability=RISC_KEY_VALUE_CONFLICT;
        fixture_profile.collect_delay=0;pump(100);
        assert(fixture_profile.collections==1&&!storage_pending(&wm));storage_test_record();
        assert(!store_cached_ready(&wm.network_storage));
    }else if(!strcmp(mode,"storage-cancel-before-submit")){
        wifi_manager_test_action(SAVE_DRAFT,0);
        assert(wm.db_job==DB_SAVE&&!fixture_profile.submissions);
        wifi_manager_test_action(BACK,0);
        assert(!wm.db_job&&!storage_pending(&wm)&&wm.draft_dirty&&!kv_writes);
    }else if(!strcmp(mode,"storage-policy-stale")){
        wifi_manager_test_action(TOGGLE,0);assert(wm.policy_dirty);
        ++fixture_policy.revision;pump(100);
        assert(wm.enabled&&!wm.policy_dirty&&!fixture_policy.submissions&&!kv_writes);
    }else if(!strcmp(mode,"storage-malformed")){
        storage_test_submitted();unsigned grants=active_grants;
        fixture_profile.malformed_receipt=true;fixture_profile.collect_delay=0;pump(100);
        assert(wm.retained&&!native_alive&&active_grants==grants);
        assert(wm.atomic_edit.token&&fixture_profile.collections==1);
        puts("Shared Wi-Fi malformed atomic receipt retains custody PASS");return;
    }else if(!strcmp(mode,"storage-policy")){
        invoke(TOGGLE,0);pump(100);
        uint8_t flags=0;assert(portable_radio_load(&pkv,&flags)&&!(flags&PORTABLE_RADIO_WIFI));
        assert(!wm.enabled&&fixture_policy.submissions==1&&fixture_policy.collections==1);
        invoke(TOGGLE,0);pump(100);
        assert(portable_radio_load(&pkv,&flags)&&(flags&PORTABLE_RADIO_WIFI));
        assert(wm.enabled&&fixture_policy.submissions==2&&fixture_policy.collections==2);
    }else if(!strcmp(mode,"storage-exit-draft")){
        wm.exit_requested=true;pump(100);
        assert(!wm.finished&&wm.screen==STORAGE&&wm.draft_dirty);
        invoke(KEEP_DRAFT,0);assert(wm.screen==ADD&&!strcmp(wm.draft.password,"testpass123"));
    }else assert(!"unknown shared storage scenario");
    exit_app();printf("Shared Wi-Fi %s PASS\n",mode);
}
