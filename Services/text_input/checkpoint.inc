/* Optional resident-policy boundary. Existing clients never call these methods
 * and keep the original poll/open/close behavior and copied ABI layouts. */
static int32_t checkpoint_enable(void *c,uint64_t s){
    (void)c;int32_t r=check(s);if(r)return r;
    if(closing||checkpoint_paused||checkpoint_pausing||state.state!=RISC_TEXT_ENTRY_EDITING)return RISC_TEXT_ENTRY_BUSY;
    if(checkpoint_enabled)return RISC_TEXT_ENTRY_OK;
    const risc_scene_lifecycle_api_v1 *lifecycle=risc_scene_lifecycle_get_v1(scene);
    const risc_scene_checkpoint_api_v1 *cp=risc_scene_checkpoint_get_v1(scene);
    if(!lifecycle||!cp)return RISC_TEXT_ENTRY_UNAVAILABLE;
    r=lifecycle->configure(scene->context,scene_session,RISC_SCENE_FEATURE_POLICY_INPUT_CONTINUITY);
    if(!live())return RISC_TEXT_ENTRY_RETAINED;
    if(r==RISC_SCENE_INVALID)return RISC_TEXT_ENTRY_UNAVAILABLE;
    if(r!=RISC_SCENE_OK)return retain();
    checkpoint_scene=cp;checkpoint_enabled=true;checkpoint_activity=false;
    if(keyboard){bool held=false;if(!snapshot(false,&held))return retain();checkpoint_usb_held=held;}
    return RISC_TEXT_ENTRY_OK;
}
static int32_t checkpoint_snapshot(void *c,uint64_t s,risc_text_entry_checkpoint_state_v1 *out){
    (void)c;int32_t r=check(s);if(r)return r;
    if(!out||out->struct_size!=sizeof(*out))return RISC_TEXT_ENTRY_INVALID;
    if(!checkpoint_enabled)return RISC_TEXT_ENTRY_UNAVAILABLE;
    if(closing||checkpoint_pausing)return RISC_TEXT_ENTRY_BUSY;
    if(checkpoint_paused){*out=(risc_text_entry_checkpoint_state_v1){sizeof(*out),RISC_TEXT_ENTRY_CHECKPOINT_PAUSED};return RISC_TEXT_ENTRY_OK;}
    risc_scene_navigation_v1 navigation={.struct_size=sizeof(navigation)};uint32_t flags=0;
    r=scene->snapshot(scene->context,scene_session,&navigation,&flags);
    if(!live())return RISC_TEXT_ENTRY_RETAINED;
    if(r!=RISC_SCENE_OK)return retain();
    if(flags&RISC_SCENE_ACTIVITY)checkpoint_activity=true;
    uint32_t report=(flags&RISC_SCENE_PRESENTING)?RISC_TEXT_ENTRY_CHECKPOINT_PRESENTING:0;
    if((flags&(RISC_SCENE_INPUT_BUSY|RISC_SCENE_CLOSING))||checkpoint_usb_held||draining||keyboard_pending||(keyboard&&!neutral))report|=RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY;
    if(checkpoint_activity)report|=RISC_TEXT_ENTRY_CHECKPOINT_ACTIVITY;
    *out=(risc_text_entry_checkpoint_state_v1){sizeof(*out),report};checkpoint_activity=false;
    return RISC_TEXT_ENTRY_OK;
}
static int32_t checkpoint_pause_mode(void *c,uint64_t s,bool full){
    (void)c;int32_t r=check(s);if(r)return r;
    if(!checkpoint_enabled)return RISC_TEXT_ENTRY_UNAVAILABLE;
    if(closing)return RISC_TEXT_ENTRY_BUSY;
    if(checkpoint_paused)return checkpoint_full_pause==full?RISC_TEXT_ENTRY_OK:RISC_TEXT_ENTRY_BUSY;
    if(checkpoint_pausing&&checkpoint_full_pause!=full)return RISC_TEXT_ENTRY_BUSY;
    if(!checkpoint_pausing){
        /* Recheck real input before detaching. Never drop an accepted/cancelled
         * outcome or a key that arrived after the caller's settled snapshot. */
        uint32_t revision=state.revision,presentation=0;r=poll_checked(&presentation);if(r)return r;
        if(state.state!=RISC_TEXT_ENTRY_EDITING||state.revision!=revision)return RISC_TEXT_ENTRY_BUSY;
        risc_text_entry_checkpoint_state_v1 snapshot_state={.struct_size=sizeof(snapshot_state)};
        r=checkpoint_snapshot(NULL,s,&snapshot_state);if(r)return r;
        if(snapshot_state.flags&RISC_TEXT_ENTRY_CHECKPOINT_ACTIVITY)checkpoint_activity=true;
        if(snapshot_state.flags&(RISC_TEXT_ENTRY_CHECKPOINT_INPUT_BUSY|RISC_TEXT_ENTRY_CHECKPOINT_PRESENTING|RISC_TEXT_ENTRY_CHECKPOINT_ACTIVITY))return RISC_TEXT_ENTRY_BUSY;
        const risc_scene_lifecycle_api_v1 *lifecycle=risc_scene_lifecycle_get_v1(scene);
        r=lifecycle->configure(scene->context,scene_session,full?0:RISC_SCENE_FEATURE_POLICY_INPUT_CONTINUITY);
        if(!live())return RISC_TEXT_ENTRY_RETAINED;
        if(r!=RISC_SCENE_OK)return retain();
        checkpoint_full_pause=full;checkpoint_pausing=true;
    }
    /* Retain the passive subscription and its bounded queue. Releasing it
     * here would create an unrecoverable event gap on every policy sample. */
    if(full&&subscription){
        bool ok=keyboard->unsubscribe(keyboard->context,subscription);
        if(!live())return RISC_TEXT_ENTRY_RETAINED;
        if(!ok)return retain();
        subscription=0;
    }
    r=checkpoint_scene->pause(scene->context,scene_session);
    if(!live())return RISC_TEXT_ENTRY_RETAINED;
    if(r==RISC_SCENE_AGAIN)return RISC_TEXT_ENTRY_AGAIN;
    if(r!=RISC_SCENE_OK)return retain();
    checkpoint_pausing=false;checkpoint_paused=true;return RISC_TEXT_ENTRY_OK;
}
static int32_t checkpoint_pause(void *c,uint64_t s){return checkpoint_pause_mode(c,s,false);}
static int32_t checkpoint_pause_for_focus_change(void *c,uint64_t s){return checkpoint_pause_mode(c,s,true);}
static int32_t checkpoint_resume(void *c,uint64_t s,uint32_t flags){
    (void)c;int32_t r=check(s);if(r)return r;
    if(flags&~(RISC_TEXT_ENTRY_RESUME_REDRAW|RISC_TEXT_ENTRY_RESUME_FOCUS_CHANGED))return RISC_TEXT_ENTRY_INVALID;
    if(!checkpoint_enabled)return RISC_TEXT_ENTRY_UNAVAILABLE;
    if(closing||checkpoint_pausing||!checkpoint_paused)return RISC_TEXT_ENTRY_BUSY;
    if(checkpoint_full_pause){
        flags|=RISC_TEXT_ENTRY_RESUME_FOCUS_CHANGED;
        if(keyboard){
            subscription=keyboard->subscribe(keyboard->context,0);
            if(!live())return RISC_TEXT_ENTRY_RETAINED;
            if(!subscription)return retain();
            bool held=false;if(!snapshot(false,&held))return retain();
        }
    }
    if(keyboard&&(flags&RISC_TEXT_ENTRY_RESUME_FOCUS_CHANGED)){
        /* Wake/cancel input belongs to the intervening focus, not this draft.
         * Drain bounded stale history and wait for neutral, preserving caps
         * unless the hardware attachment itself changed. */
        neutral=false;draining=true;checkpoint_neutralizing=true;
        r=keyboard_step();if(r)return r;
    }
    r=checkpoint_scene->resume(scene->context,scene_session,((flags&RISC_TEXT_ENTRY_RESUME_REDRAW)?RISC_SCENE_RESUME_REDRAW:0)|((flags&RISC_TEXT_ENTRY_RESUME_FOCUS_CHANGED)?RISC_SCENE_RESUME_FOCUS_CHANGED:0));
    if(!live())return RISC_TEXT_ENTRY_RETAINED;
    if(r!=RISC_SCENE_OK)return retain();
    if(checkpoint_full_pause){
        const risc_scene_lifecycle_api_v1 *lifecycle=risc_scene_lifecycle_get_v1(scene);
        r=lifecycle->configure(scene->context,scene_session,RISC_SCENE_FEATURE_POLICY_INPUT_CONTINUITY);
        if(!live())return RISC_TEXT_ENTRY_RETAINED;
        if(r!=RISC_SCENE_OK)return retain();
    }
    checkpoint_paused=false;checkpoint_full_pause=false;
    /* A changed hardware attachment while sleeping changes only its display
     * ownership hint; the copied draft and keyboard layer remain untouched. */
    if(changed){r=update_scene();if(r)return r;}
    return RISC_TEXT_ENTRY_OK;
}
