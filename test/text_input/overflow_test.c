/* White-box only at the unreachable-by-short-run revision boundary. Including
 * the real implementation avoids adding any production testing hooks. */
#include "../../Services/text_input/host.c"
#include <assert.h>
#include <stdio.h>

const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){static const risc_runtime_api_v1 r={.api_version=1,.struct_size=sizeof(r)};return v==1?&r:NULL;}
static unsigned io_calls,snapshots,event_kind;
static int32_t fake_open(void *c,const risc_scene_document_v1 *d,const risc_scene_navigation_v1 *p,uint64_t *out){
    (void)c;(void)d;(void)p;assert(!retained);++io_calls;*out=19;return RISC_SCENE_OK;
}
static int32_t fake_update(void *c,uint64_t s,const risc_scene_document_v1 *d){
    (void)c;(void)s;(void)d;assert(!retained);++io_calls;return RISC_SCENE_OK;
}
static int32_t fake_next(void *c,uint64_t s,risc_scene_event_v1 *out){
    (void)c;assert(!retained&&s==19);++io_calls;
    *out=(risc_scene_event_v1){.struct_size=sizeof(*out),.kind=event_kind,
        .document_revision=UINT32_MAX,.node=1,.action=event_kind==RISC_SCENE_ACTION_EVENT?2:1,
        .value=RISC_SCENE_KEY_CANCEL,.sequence=1};return RISC_SCENE_OK;
}
static int32_t fake_snapshot(void *c,uint64_t s,risc_scene_navigation_v1 *out,uint32_t *flags){
    (void)c;(void)s;(void)out;(void)flags;assert(!retained);++io_calls;++snapshots;return RISC_SCENE_OK;
}
static int32_t fake_close(void *c,uint64_t s){(void)c;(void)s;assert(!retained);++io_calls;return RISC_SCENE_OK;}
static const risc_scene_api_v1 fake_scene={1,sizeof(fake_scene),NULL,fake_open,fake_update,fake_next,NULL,fake_snapshot,fake_close};
int main(int argc,char **argv){
    assert(argc==2);
    event_kind=!strcmp(argv[1],"suspend")?RISC_SCENE_SUSPEND_EVENT:
        !strcmp(argv[1],"action")?RISC_SCENE_ACTION_EVENT:RISC_SCENE_VALUE_EVENT;
    const risc_driver_v2 *driver=t5_driver_get(2);assert(driver);
    risc_provider_dependency_v1 dependency={RISC_SCENE_CAPABILITY,1,&fake_scene};
    assert(driver->start(&dependency,1));
    risc_text_entry_request_v1 request={.api_version=1,.struct_size=sizeof(request),.capacity=16,.label="Text"};
    uint64_t token=0;assert(api.base.base.open(NULL,&request,&token)==RISC_TEXT_ENTRY_OK&&token);
    state.revision=document.revision=UINT32_MAX;
    unsigned before=io_calls;risc_text_entry_state_v1 result={.struct_size=sizeof(result)};
    assert(api.base.base.poll(NULL,token,&result)==RISC_TEXT_ENTRY_RETAINED);
    assert(retained&&io_calls==before+1&&!snapshots);
    before=io_calls;
    assert(api.base.base.poll(NULL,token,&result)==RISC_TEXT_ENTRY_RETAINED);
    assert(api.base.base.close(NULL,token)==RISC_TEXT_ENTRY_RETAINED&&!driver->quiesce());
    driver->stop();assert(io_calls==before);
    printf("text host revision overflow %s PASS (no I/O after retention)\n",argv[1]);return 0;
}
