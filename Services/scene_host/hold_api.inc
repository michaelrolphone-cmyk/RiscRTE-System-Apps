static int32_t hold_open_impl(void *c,const risc_components_document_v1 *d,const risc_scene_navigation_v1 *p,uint64_t *out){
    if(!alive()||hold_fault_delivered)return fail_retained();
    if(active||hold_fault)return RISC_SCENE_BUSY;
    hold_enabled=true;hold_active=hold_pending=hold_fault_delivered=false;hold_last=0;hold_nav_buttons=0;
    int32_t r=components_open(c,d,p,out);
    if(r!=RISC_SCENE_OK&&!active&&!retained)hold_enabled=false;
    return r;
}
static int32_t hold_update_impl(void *c,uint64_t s,const risc_components_document_v1 *d){
    if(!hold_enabled)return RISC_SCENE_INVALID;
    hold_enter=true;return components_update(c,s,d);
}
static int32_t hold_deliver(risc_scene_hold_event_v1 *out){
    if(!alive())return RISC_SCENE_RETAINED;
    *out=hold_event;hold_pending=false;
    if(hold_fault)hold_fault_delivered=true;
    return RISC_SCENE_OK;
}
static int32_t hold_next_impl(void *c,uint64_t s,risc_scene_hold_event_v1 *out){
    if(!out||out->struct_size!=sizeof(*out))return RISC_SCENE_INVALID;
    hold_dispatch=true;hold_enter=true;int32_t r=check(s);hold_dispatch=false;
    if(r)return r;
    if(!hold_enabled)return RISC_SCENE_INVALID;
    if(hold_pending)return hold_deliver(out);
    hold_now=clock_api->monotonic_ms(clock_api->context);
    if(!alive())return RISC_SCENE_RETAINED;
    if(hold_active){
        if(hold_now<hold_last||hold_now-hold_last>100)hold_cancel(RISC_HOLD_REASON_SERVICE_GAP);
        else if(hold_confirm&&(hold_now<hold_started||hold_now-hold_started>800))hold_cancel(RISC_HOLD_REASON_TIMEOUT);
    }
    hold_last=hold_now;
    if(hold_pending)return hold_deliver(out);
    risc_scene_event_v1 e={.struct_size=sizeof(e)};
    hold_dispatch=true;hold_enter=true;r=scene_next(c,s,&e);hold_dispatch=false;
    if(retained)return RISC_SCENE_RETAINED;
    if(hold_pending)return hold_deliver(out);
    if(r==RISC_SCENE_OK)*out=(risc_scene_hold_event_v1){.struct_size=sizeof(*out),.event=e};
    return r;
}

static int32_t hold_open(void *c,const risc_components_document_v1 *d,const risc_scene_navigation_v1 *p,uint64_t *out){
    if(hold_busy)return fail_retained();
    hold_busy=true;int32_t r=hold_open_impl(c,d,p,out);hold_busy=hold_enter=false;return r;
}
static int32_t hold_update(void *c,uint64_t s,const risc_components_document_v1 *d){
    if(hold_busy)return fail_retained();
    hold_busy=true;int32_t r=hold_update_impl(c,s,d);hold_busy=hold_enter=false;return r;
}
static int32_t hold_next(void *c,uint64_t s,risc_scene_hold_event_v1 *out){
    if(hold_busy)return fail_retained();
    hold_busy=true;int32_t r=hold_next_impl(c,s,out);hold_busy=hold_enter=false;return r;
}
