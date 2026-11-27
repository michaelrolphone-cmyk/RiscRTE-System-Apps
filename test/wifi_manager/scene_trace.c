/* Test-only observation of the real provider; no production hooks. */
#define t5_driver_get wifi_raw_scene_driver_get
#include "../../Services/scene_host/host.c"
#undef t5_driver_get
static bool wifi_trace_enabled;
static unsigned wifi_trace_calls;
void wifi_scene_trace_enable(void){wifi_trace_enabled=true;wifi_trace_calls=0;}
static int32_t wifi_trace_next(void *c,uint64_t s,risc_scene_event_v1 *e){
    bool current=active&&view_current(),barrier=key_barrier,waiting=keyboard_waiting,handoff=handoff_waiting;
    unsigned keys=key_count;uint32_t revision=document.revision;
    int32_t r=scene_next(c,s,e);
    if(wifi_trace_enabled&&(wifi_trace_calls<8||(r==RISC_SCENE_OK&&wifi_trace_calls<64))){
        fprintf(stderr,"WIFI_SCENE_TRACE call=%u token=%llu result=%d before_revision=%u current=%u barrier=%u waiting=%u handoff=%u keys=%u after_current=%u after_barrier=%u after_waiting=%u paused=%u nav_claimed=%u event_kind=%u event_revision=%u node=%u action=%u value=%d\n",
            wifi_trace_calls,(unsigned long long)s,r,revision,current,barrier,waiting,handoff,keys,
            active&&view_current(),key_barrier,keyboard_waiting,paused,nav_claimed,
            r==RISC_SCENE_OK?e->kind:0,r==RISC_SCENE_OK?e->document_revision:0,r==RISC_SCENE_OK?e->node:0,
            r==RISC_SCENE_OK?e->action:0,r==RISC_SCENE_OK?e->value:0);
    }
    if(wifi_trace_enabled)++wifi_trace_calls;
    return r;
}
static const risc_input_navigation_api_v1 *wifi_trace_navigation;
static risc_input_navigation_api_v1 wifi_traced_navigation;
static bool wifi_trace_nav_poll(void *c,risc_input_navigation_frame_v1 *f){
    bool ok=wifi_trace_navigation->poll(c,f);
    if(wifi_trace_enabled&&(wifi_trace_calls<8||(ok&&f->pressed)))
        fprintf(stderr,"WIFI_NAV_TRACE call=%u ok=%u pressed=%u released=%u buttons=%u current=%u barrier=%u waiting=%u handoff=%u\n",
            wifi_trace_calls,ok,ok?f->pressed:0,ok?f->released:0,ok?f->buttons:0,
            active&&view_current(),key_barrier,keyboard_waiting,handoff_waiting);
    return ok;
}
static bool wifi_trace_start(const risc_provider_dependency_v1 *deps,size_t count){
    if(!deps||count>5)return start(deps,count);
    risc_provider_dependency_v1 copy[5];memcpy(copy,deps,count*sizeof(*deps));
    for(size_t i=0;i<count;i++)if(copy[i].capability_id&&!strcmp(copy[i].capability_id,"input.navigation")){
        wifi_trace_navigation=copy[i].api;if(!wifi_trace_navigation)return start(deps,count);
        wifi_traced_navigation=*wifi_trace_navigation;wifi_traced_navigation.poll=wifi_trace_nav_poll;
        copy[i].api=&wifi_traced_navigation;
    }
    return start(copy,count);
}
const risc_driver_v2 *t5_driver_get(uint32_t abi){
    static risc_scene_modal_api_v1 traced_api;static risc_driver_v2 traced_driver;
    const risc_driver_v2 *base=wifi_raw_scene_driver_get(abi);if(!base)return NULL;
    traced_api=api;((risc_scene_api_v1*)&traced_api)->next=wifi_trace_next;
    traced_driver=*base;traced_driver.start=wifi_trace_start;traced_driver.capability=&traced_api;return &traced_driver;
}
