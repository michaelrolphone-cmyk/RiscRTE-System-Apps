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
    if(!strcmp(mode,"storage-pref-stale-read")||!strcmp(mode,"storage-pref-unavailable")){
        capture(NULL,NULL);
        const uint8_t radios[]={0x51,1,3,3^0xa5},enabled[]={0x62,1,1,1^0xa5};
        assert(!kv_put((void*)1,PORTABLE_RADIO_KEY,radios,sizeof(radios)));
        assert(!kv_put((void*)1,TELEMETRY_BROADCAST_KEY,enabled,sizeof(enabled)));
        for(unsigned n=0;!broadcast_live;n++){assert(n<1000);pump(1);}
        fixture_policy.stale_radio_read=!strcmp(mode,"storage-pref-stale-read");
        fixture_policy.unavailable=!strcmp(mode,"storage-pref-unavailable");
        capture(NULL,NULL);wm.now=wm.broadcast_at+1000;
        assert(background_step(&wm,0)&&!broadcast_live&&!wm.retained);
        unsigned result=policy_load(&wm);
        assert(result==(fixture_policy.unavailable?PORTABLE_RADIO_UNAVAILABLE:PORTABLE_RADIO_BUSY));
        unsigned submitted=fixture_policy.submissions;
        wifi_manager_test_action(TOGGLE,0);pump(100);
        assert(fixture_policy.submissions==submitted&&!wm.policy_dirty&&!broadcast_live);
        assert(!wm.radio_storage.availability.api&&wm.radio_storage.raw==&fixture_policy_atomic);
        fixture_policy.stale_radio_read=false;fixture_policy.unavailable=false;
    }else if(!strcmp(mode,"storage-save")){
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
    }else if(!strcmp(mode,"storage-policy-uncertain")||!strcmp(mode,"storage-policy-cancel")||!strcmp(mode,"storage-policy-commit-race")){
        fixture_policy.collect_delay=1000;
        fixture_policy.uncertain=!strcmp(mode,"storage-policy-uncertain");
        fixture_policy.commit_wins=!strcmp(mode,"storage-policy-commit-race");
        wifi_manager_test_action(TOGGLE,0);pump(1);
        assert(wm.policy_edit.token&&fixture_policy.token&&wm.enabled);
        assert(!fixture_profile.submissions&&!kv_writes);
        if(!fixture_policy.uncertain){
            wifi_manager_test_action(BACK,0);
            assert(fixture_policy.cancellations==1&&wm.policy_edit.token);
        }
        fixture_policy.collect_delay=0;pump(100);
        assert(!wm.policy_edit.token&&fixture_policy.collections==1&&!wm.policy_dirty);
        assert(wm.enabled==!fixture_policy.commit_wins);
        if(fixture_policy.uncertain){
            assert(strstr(wm.message,"uncertain"));unsigned submitted=fixture_policy.submissions;
            pump(100);assert(fixture_policy.submissions==submitted);
        }
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
