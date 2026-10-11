/* Production adapter lifecycle checks. Pixel math has a separate independent
 * oracle; this fixture checks ownership, elapsed time, full frames and input. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "PortableApps.h"
#include "RiscDisplayOutputV1.h"
#include "RiscTouchV1.h"

static unsigned mock_allocations,mock_fail_allocation,mock_live;
static void *mock_owned[4];
static void *mock_malloc(size_t bytes) {
    if(++mock_allocations==mock_fail_allocation)return NULL;
    void *p=malloc(bytes);assert(p);
    unsigned i;for(i=0;i<4 && mock_owned[i];i++);assert(i<4);
    mock_owned[i]=p;mock_live++;return p;
}
static void mock_free(void *p) {
    if(!p)return;
    unsigned i;for(i=0;i<4 && mock_owned[i]!=p;i++);assert(i<4);
    mock_owned[i]=NULL;mock_live--;free(p);
}
#define malloc mock_malloc
#define free mock_free
#include "../../lib/PortableApps/src/adapter.c"
#undef malloc
#undef free

const t5_app_manifest_t portable_catalog[]={{.display_name="Clock",.file_name="default.elf",.icon="solid:f017",.compatible=true}};
const unsigned portable_catalog_count=1;
static uint16_t mock_pixels[240*243],mock_old[240*240];
static unsigned mock_ms,mock_grants,mock_subscriptions,mock_frames,mock_presents,mock_polls;
static unsigned mock_latency,mock_submit_ms,mock_last_touch,mock_max_gap,mock_flags,mock_hz=60000;
static bool mock_reject_submit,mock_fail_status,mock_down;
static uint16_t mock_x=120,mock_y=120;
static unsigned mock_launches;
static bool mock_health(risc_runtime_health_v1 *h){h->uptime_ms=mock_ms;return true;}
static void mock_yield(uint32_t n){mock_ms+=n;}
static bool mock_diagnostic(const char *s){(void)s;return true;}
static bool mock_launch(const char *s){
#ifdef PORTABLE_RETURN_APP
 assert(!strcmp(s,"default.elf") || !strcmp(s,PORTABLE_RETURN_APP));
#else
 assert(!strcmp(s,"default.elf"));
#endif
 mock_launches++;return true;}

static bool mock_info(void *c,risc_display_info_v1 *out){
    (void)c;*out=(risc_display_info_v1){.width=240,.height=240,
        .supported_formats=RISC_DISPLAY_FORMAT_BIT(RISC_DISPLAY_FORMAT_RGB565),
        .flags=mock_flags,.nominal_refresh_millihz=mock_hz,.typical_present_latency_us=16000};return true;
}
static bool mock_acquire_frame(void *c,uint32_t format,risc_display_surface_v1 *out){
    (void)c;assert(!mock_frames);mock_frames=1;
    *out=(risc_display_surface_v1){.frame=1,.pixels=mock_pixels,.width=240,.height=240,
        .stride_bytes=486,.size_bytes=sizeof(mock_pixels),.pixel_format=format};return true;
}
static void mock_release_frame(void *c,risc_display_frame_v1 f){(void)c;assert(f==1 && mock_frames);mock_frames=0;}
static bool mock_submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,
                        const risc_display_present_options_v1 *o,risc_display_present_token_v1 *token){
    (void)c;(void)o;assert(f==1 && mock_frames);assert(!r && !n);
    if(mock_reject_submit)return false;
    mock_frames=0;mock_submit_ms=mock_ms;*token=++mock_presents;return true;
}
static bool mock_status(void *c,risc_display_present_token_v1 t,risc_display_present_status_v1 *out){
    (void)c;assert(t==mock_presents);
    out->state=mock_fail_status?RISC_DISPLAY_PRESENT_FAILED:
        mock_ms-mock_submit_ms<mock_latency?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;return true;
}
static uint64_t mock_subscribe(void *c){(void)c;assert(!mock_subscriptions);mock_subscriptions=1;return 1;}
static bool mock_unsubscribe(void *c,uint64_t s){(void)c;assert(s==1 && mock_subscriptions);mock_subscriptions=0;return true;}
static bool mock_poll_touch(void *c,size_t n){
    (void)c;assert(n==1);mock_polls++;
    unsigned gap=mock_ms-mock_last_touch;if(mock_polls>1 && gap>mock_max_gap)mock_max_gap=gap;
    mock_last_touch=mock_ms;return true;
}
static int32_t mock_next(void *c,uint64_t s,risc_touch_event_v1 *e){(void)c;(void)e;assert(s==1);return 0;}
static bool mock_snapshot(void *c,risc_touch_snapshot_v1 *s){
    (void)c;*s=(risc_touch_snapshot_v1){.width=240,.height=240,.contact_count=mock_down?1:0};
    s->contacts[0].x=mock_x;s->contacts[0].y=mock_y;return true;
}
static const risc_display_output_api_v1 mock_display={.api_version=1,.struct_size=sizeof(mock_display),
    .get_info=mock_info,.acquire=mock_acquire_frame,.release=mock_release_frame,.submit=mock_submit,.present_status=mock_status};
static const risc_touch_api_v1 mock_touch={1,sizeof(mock_touch),NULL,mock_subscribe,mock_unsubscribe,mock_poll_touch,mock_next,mock_snapshot};
static bool mock_acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *g){
    assert(version==1 && !instance);g->struct_size=sizeof(*g);
    if(!strcmp(name,"display.output"))g->api=&mock_display;
    else if(!strcmp(name,"input.touch.raw"))g->api=&mock_touch;
    else return false;
    mock_grants++;return true;
}
static bool mock_release(risc_runtime_capability_v1 *g){assert(mock_grants && g->api);mock_grants--;g->api=NULL;return true;}
static const risc_runtime_api_v1 mock_rt={.api_version=1,.struct_size=sizeof(mock_rt),.health=mock_health,.yield_ms=mock_yield,.diagnostic=mock_diagnostic,.request_launch=mock_launch,.acquire=mock_acquire,.release=mock_release};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t version){return version==1?&mock_rt:NULL;}

static void begin_test(void){
    assert(!mock_grants && !mock_subscriptions && !mock_frames && !mock_live);
    mock_ms=mock_presents=mock_polls=mock_latency=mock_max_gap=mock_last_touch=mock_allocations=mock_fail_allocation=mock_launches=0;
    mock_reject_submit=mock_fail_status=mock_down=false;mock_flags=RISC_DISPLAY_INFO_PARTIAL_DAMAGE;mock_hz=60000;
    for(unsigned y=0;y<240;y++)for(unsigned x=0;x<243;x++){
        mock_pixels[y*243+x]=(uint16_t)(x<240?(x*127u+y*393u):0xa55a);
        if(x<240)mock_old[y*240+x]=mock_pixels[y*243+x];
    }
}
static void end_test(void){
    app_module_fini();assert(!mock_grants && !mock_subscriptions && !mock_frames && !mock_live);
    assert(!handoff_old && !handoff_scratch && !springboard_transition_active());
    for(unsigned y=0;y<240;y++)for(unsigned x=240;x<243;x++)assert(mock_pixels[y*243+x]==0xa55a);
}
static void draw_fresh(void){clear();fill(0,0,240,240,0x07e0);}
static void assert_fresh(void){for(unsigned y=0;y<240;y++)for(unsigned x=0;x<240;x++)assert(mock_pixels[y*243+x]==0x07e0);}
static void assert_old(void){for(unsigned y=0;y<240;y++)assert(!memcmp(mock_pixels+y*243,mock_old+y*240,480));}
static void lifecycle(void){
    begin_test();assert(app_module_init()==0);assert(!previous_pixels);
    assert(springboard_presentation_get());assert(handoff_pending && mock_polls==1);
    draw_fresh();assert(mock_live==2 && handoff_active);mock_ms=700;present(false);assert_old();
    assert(handoff_started==700 && mock_presents==1 && !launch(0));
    mock_ms=790;draw_fresh();present(false);assert(handoff_active && mock_live==2);
    assert(memcmp(mock_pixels,mock_old,480));
    mock_ms=880;draw_fresh();present(false);assert_fresh();
    assert(!handoff_active && mock_live==0 && mock_presents==3 && launch(0));
    mock_ms=920;draw_fresh();present(false);assert_fresh();assert(mock_presents==4);end_test();
}
static void allocation_fallback(void){
    for(unsigned fail=1;fail<=2;fail++){
        begin_test();mock_fail_allocation=fail;assert(app_module_init()==0);assert(springboard_presentation_get());
        draw_fresh();assert(!handoff_active && !mock_live);present(false);assert_fresh();end_test();
    }
}
static void failed_and_interrupted(void){
    for(unsigned path=0;path<3;path++){
        begin_test();assert(app_module_init()==0);assert(springboard_presentation_get());draw_fresh();
        if(path==0)mock_reject_submit=true;
        if(path==1)mock_fail_status=true;
        if(path<2){present(false);assert(failed && !launch(0));}
        /* path2 exits while still holding the acquired outgoing snapshot. */
        end_test();
    }
}
static void delayed_held_contact_and_wrap(void){
    begin_test();mock_ms=UINT32_MAX-40u;mock_down=true;
    assert(app_module_init()==0);assert(springboard_presentation_get());
    springboard_contact c;np_contact(&c);assert(c.down && !c.tap_eligible);
    draw_fresh();mock_latency=120;present(false);assert_old();assert(mock_polls>=8 && mock_max_gap<=16);
    mock_x=150;mock_y=135;mock_ms+=20;input_service();t5_app_input_t in={0};np_poll(&in);np_contact(&c);
    assert(c.down && c.x==150 && c.y==135 && !c.tap_eligible);
    mock_down=false;mock_ms+=20;input_service();np_poll(&in);np_contact(&c);assert(c.released && !c.tap_eligible);
    mock_ms=handoff_started+180u;draw_fresh();mock_latency=0;present(false);assert_fresh();assert(!handoff_active);end_test();
}
static void unsupported_display(void){
    begin_test();mock_hz=0;assert(app_module_init()==0);assert(!springboard_presentation_get());
    draw_fresh();present(false);assert_fresh();assert(!mock_allocations);end_test();
}
int main(void){
    lifecycle();allocation_fallback();failed_and_interrupted();delayed_held_contact_and_wrap();unsupported_display();
    puts("Retained handoff: exact first/final frame, held touch, delayed full-frame submit, rollover, allocation failures and interrupted cleanup passed");
    return 0;
}
