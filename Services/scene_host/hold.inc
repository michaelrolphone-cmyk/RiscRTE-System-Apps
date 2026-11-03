/* Held input is copied before logical acknowledgement. This state never owns
 * a caller pointer, domain capability, or application geometry. */
static bool hold_boundary(uint32_t reason);
static void hold_queue(uint32_t phase,uint32_t reason,uint64_t now){
    if(!hold_active||(hold_pending&&!(phase==RISC_HOLD_CANCEL&&hold_event.phase==RISC_HOLD_PRESS)))return;
    uint64_t duration=now>=hold_started?now-hold_started:0;
    hold_event=(risc_scene_hold_event_v1){.struct_size=sizeof(hold_event),
        .phase=phase,.reason=reason,.event=hold_identity,
        .generation=hold_generation,.held_ms=duration};
    hold_pending=true;
    if(phase!=RISC_HOLD_PRESS){hold_active=false;contact=false;top_pending=false;neutral=false;}
}
static void hold_cancel(uint32_t reason){
    hold_queue(RISC_HOLD_CANCEL,reason,hold_now);
}
static bool hold_boundary(uint32_t reason){
    if(pointer_boundary())return true;
    if(!hold_enabled)return false;
    hold_cancel(reason);
    return hold_pending;
}
static int32_t hold_local_fault(void){
    if(pointer_enabled&&(pointer_active||pointer_pending)&&alive()){pointer_reset(true);hold_fault=true;return RISC_SCENE_BUSY;}
    if(!hold_enabled||!hold_active||hold_pending||!alive())return fail_retained();
    hold_cancel(RISC_HOLD_REASON_LOCAL_FAULT);
    hold_event.flags=RISC_HOLD_LOCAL_FAULT_PENDING;hold_fault=true;
    return RISC_SCENE_BUSY;
}
static const risc_scene_node_v1 *hold_node(uint32_t id){
    int i=node_index(&document,id);
    if(i<0)return NULL;
    const risc_scene_node_v1 *n=&document.nodes[i];
    return n->kind==RISC_COMPONENT_HOLD_KEY&&!(n->flags&(RISC_SCENE_DISABLED|RISC_SCENE_HIDDEN))?n:NULL;
}
static bool hold_begin(uint32_t id,uint64_t at,bool nav){
    const risc_scene_node_v1 *n=hold_node(id);
    if(!hold_enabled||!n)return false;
    if(hold_active||hold_pending||!view_current()||!neutral)return true;
    if(nav?(contact||touch_state.contact_count||touch_state.buttons||dispatched_count):hold_nav_buttons)return true;
    if(hold_generation==UINT64_MAX||event_serial==UINT64_MAX){(void)fail_retained();return true;}
    ++hold_generation;hold_started=at;hold_nav=nav;hold_confirm=n->step==RISC_HOLD_CONFIRM;
    hold_identity=(risc_scene_event_v1){.struct_size=sizeof(hold_identity),.kind=RISC_SCENE_ACTION_EVENT,
        .document_revision=document.revision,.node=n->id,.action=n->action,.sequence=++event_serial};
    hold_active=true;hold_queue(RISC_HOLD_PRESS,RISC_HOLD_REASON_NONE,at);
    top_pending=false;return true;
}
static bool hold_touch_down(int x,int y,uint64_t at){
    if(!hold_enabled)return false;
    for(unsigned i=0;i<visible.count;i++){
        const hit *h=&visible.items[i];
        if(x>=h->x&&y>=h->y&&x<h->x+h->w&&y<h->y+h->h&&hold_begin(h->node,at,false))return true;
    }
    return false;
}
static bool hold_touch_end(int x,int y,uint64_t at,bool release){
    if(!hold_active||hold_nav)return false;
    bool dragged_out=x-down_x>8||down_x-x>8||y-down_y>8||down_y-y>8;
    if(dragged_out)hold_queue(RISC_HOLD_CANCEL,RISC_HOLD_REASON_MOTION,at);
    else if(!view_current()||hold_identity.document_revision!=document.revision)
        hold_queue(RISC_HOLD_CANCEL,RISC_HOLD_REASON_TOPOLOGY,at);
    else if(at<hold_started||(hold_confirm&&at-hold_started>800))
        hold_queue(RISC_HOLD_CANCEL,RISC_HOLD_REASON_TIMEOUT,at);
    else if(release)hold_queue(RISC_HOLD_RELEASE,RISC_HOLD_REASON_NONE,at);
    return true;
}
static bool hold_navigation(const risc_input_navigation_frame_v1 *f){
    if(!hold_enabled)return false;
    if(hold_active){
        if(!hold_nav){if(f->pressed||f->buttons)hold_cancel(RISC_HOLD_REASON_AMBIGUOUS);return hold_pending;}
        if((f->buttons|f->pressed)&~RISC_NAV_CONFIRM)hold_cancel(RISC_HOLD_REASON_AMBIGUOUS);
        else if(f->released&RISC_NAV_CONFIRM){
            if(f->buttons&RISC_NAV_CONFIRM)hold_cancel(RISC_HOLD_REASON_AMBIGUOUS);
            else hold_queue(RISC_HOLD_RELEASE,RISC_HOLD_REASON_NONE,hold_now);
        }else if(!(f->buttons&RISC_NAV_CONFIRM))hold_cancel(RISC_HOLD_REASON_AMBIGUOUS);
        return true;
    }
    if(f->pressed&RISC_NAV_CONFIRM){
        const risc_scene_node_v1 *n=hold_node(path.focus[path.depth-1]);
        if(n){
            if(f->buttons==RISC_NAV_CONFIRM&&f->pressed==RISC_NAV_CONFIRM&&!(f->released&RISC_NAV_CONFIRM))
                (void)hold_begin(n->id,hold_now,true);
            return true;
        }
    }
    return false;
}
