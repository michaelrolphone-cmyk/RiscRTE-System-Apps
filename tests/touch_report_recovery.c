/* Production PortableTouch consumer with a scripted touch API. No hardware. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "PortableTouch.h"
static risc_touch_snapshot_v1 sample;
static bool poll_ok = true, snapshot_ok = true;
static int next_code;
static unsigned retained, diagnostics, snapshots, reads;
static char last_diagnostic[96];
static uint64_t subscribe(void *ctx) { (void)ctx; return 1; }
static bool unsubscribe(void *ctx, uint64_t token) { (void)ctx; return token == 1; }
static bool poll(void *ctx, size_t limit) { (void)ctx; (void)limit; assert(!retained); return poll_ok; }
static int32_t next(void *ctx, uint64_t token, risc_touch_event_v1 *out) {
    (void)ctx; assert(token == 1 && !retained); ++reads;
    if (next_code == 1) { memset(out, 0, sizeof(*out)); out->kind=RISC_TOUCH_EVENT_MOVE; out->id=1; out->x=120; out->y=300; }
    return next_code;
}
static bool snapshot(void *ctx, risc_touch_snapshot_v1 *out) {
    (void)ctx; assert(!retained); ++snapshots; *out=sample; return snapshot_ok;
}
static const risc_touch_api_v1 api = {
    .api_version=1, .struct_size=sizeof(api), .subscribe=subscribe,
    .unsubscribe=unsubscribe, .poll=poll, .next=next, .snapshot=snapshot
};
static bool diagnostic(const char *text) {
    assert(!retained); ++diagnostics;
    snprintf(last_diagnostic,sizeof(last_diagnostic),"%s",text); return true;
}
static bool acquire(const char *cap, uint32_t version, uint64_t instance,
                    risc_runtime_capability_v1 *out) {
    assert(!strcmp(cap,"input.touch.raw") && version==1 && instance==0);
    out->api=&api; return true;
}
static bool release(risc_runtime_capability_v1 *grant) { return grant->api==&api; }
static const risc_runtime_api_v1 runtime = {.api_version=1, .struct_size=sizeof(runtime),
    .acquire=acquire, .release=release, .diagnostic=diagnostic};
void portable_adapter_retain(void) {
    /* The first cause must be recorded while runtime diagnostics still work. */
    assert(diagnostics==1); ++retained;
}
static void contacts(unsigned count) {
    sample.contact_count=(uint8_t)count;
    for (unsigned i=0;i<RISC_TOUCH_MAX_CONTACTS;i++) {
        sample.contacts[i].id=(uint8_t)(i+1);
        sample.contacts[i].x=120; sample.contacts[i].y=300;
    }
}
int main(int argc, char **argv) {
    assert(argc==2); portable_touch t; portable_touch_sample out;
    sample.width=480; sample.height=800;
    assert(portable_touch_open(&t,&runtime));
    portable_touch_read(&t,&out); assert(out.valid && t.neutral);
    contacts(1); portable_touch_read(&t,&out); assert(out.valid && out.began);
    unsigned before=snapshots; bool fatal=false;
    if (!strcmp(argv[1],"poll-error")) poll_ok=false;
    else if (!strcmp(argv[1],"queue-gap")) next_code=-1;
    else if (!strcmp(argv[1],"poll-gap")) { poll_ok=false; next_code=-1; }
    else if (!strcmp(argv[1],"queue-budget")) next_code=1;
    else if (!strcmp(argv[1],"multi-contact")) contacts(2);
    else if (!strcmp(argv[1],"bad-coordinate")) sample.contacts[0].x=480;
    else if (!strcmp(argv[1],"provider-fault")) { next_code=-2; fatal=true; }
    else if (!strcmp(argv[1],"unknown-fault")) { next_code=-99; fatal=true; }
    else if (!strcmp(argv[1],"snapshot-fault")) { snapshot_ok=false; fatal=true; }
    else if (!strcmp(argv[1],"poll-snapshot-fault")) { poll_ok=false; snapshot_ok=false; fatal=true; }
    else assert(!"unknown scenario");
    portable_touch_read(&t,&out);
    assert(out.cancelled && !out.valid && !out.home_pressed && !out.released);
#ifdef PORTABLE_NATIVE_CUSTODY_FENCE
    if (fatal) {
        assert(retained==1 && diagnostics==1 && strstr(last_diagnostic,"action=retain"));
        if (next_code < -1) assert(snapshots==before);
        else assert(snapshots==before+1);
        printf("%s: terminal fault diagnosed before retention\n",argv[1]); return 0;
    }
#else
    (void)fatal;
#endif
    assert(!retained && !diagnostics && snapshots==before+1);
    assert(!t.down && !t.neutral && !t.home_down && !t.home_neutral);
    /* A held contact or Home after loss is never promoted to an activation. */
    poll_ok=snapshot_ok=true; next_code=0; contacts(1);
    sample.buttons=RISC_TOUCH_BUTTON_PRIMARY;
    portable_touch_read(&t,&out); assert(out.valid && !out.tap_eligible && !out.home_pressed);
    contacts(0); sample.buttons=0;
    portable_touch_read(&t,&out); assert(!out.tap_eligible && !out.home_pressed);
    contacts(1); portable_touch_read(&t,&out); assert(out.valid && out.began && out.tap_eligible);
    contacts(0); portable_touch_read(&t,&out); assert(out.released && out.tap_eligible);
    assert(portable_touch_close(&t,&runtime));
    printf("%s: cancelled then recovered after neutral\n",argv[1]);
    return 0;
}
