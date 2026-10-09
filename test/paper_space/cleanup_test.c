/* Focused fault injection for production cleanup; complements actual-runtime tests. */
#define PAPER_SPACE_NAVIGATION
#define PAPER_SPACE_TOUCH
#include "../../Apps/paper_space/main.c"
#include <stdio.h>
#include <assert.h>
const paper_space_item paper_space_items[]={{"One","one.elf"},{"Two","two.elf"},{"Three","three.elf"}};
const unsigned paper_space_item_count=3;
static bool claim_failure, clear_failure, suppressed, navigation_live;
static unsigned clears, after_release;
static bool health(risc_runtime_health_v1 *v){v->uptime_ms=0;return true;}
static void delay(uint32_t n){(void)n;}
static bool log_message(const char *s){puts(s);return true;}
static bool launch(const char *s){(void)s;return false;}
static bool info_get(void *c,risc_display_info_v1 *v){(void)c;*v=(risc_display_info_v1){.api_version=1,.struct_size=sizeof(*v),.width=480,.height=800,.supported_formats=1};return true;}
static bool frame_get(void *c,uint32_t f,risc_display_surface_v1 *v){(void)c;(void)f;(void)v;return false;}
static void frame_free(void *c,risc_display_frame_v1 f){(void)c;(void)f;}
static bool frame_submit(void *c,risc_display_frame_v1 f,const risc_display_rect_v1 *r,size_t n,const risc_display_present_options_v1 *o,risc_display_present_token_v1 *t){(void)c;(void)f;(void)r;(void)n;(void)o;(void)t;return false;}
static bool status(void *c,risc_display_present_token_v1 t,risc_display_present_status_v1 *s){(void)c;(void)t;(void)s;return false;}
static bool nav_poll(void *c,risc_input_navigation_frame_v1 *f){(void)c;(void)f;return false;}
static bool reset(void *c){(void)c;return true;}
static bool foreground(void *c,const risc_input_foreground_v1 *f,size_t n){(void)c;(void)f;if(!navigation_live)++after_release;if(n){suppressed=true;return !claim_failure;}++clears;if(clear_failure){clear_failure=false;return false;}suppressed=false;return true;}
static uint64_t subscribe(void *c){(void)c;return 1;}
static bool unsubscribe(void *c,uint64_t n){(void)c;(void)n;return true;}
static bool touch_poll(void *c,size_t n){(void)c;(void)n;return true;}
static int32_t touch_next(void *c,uint64_t n,risc_touch_event_v1 *e){(void)c;(void)n;(void)e;return 0;}
static bool snapshot(void *c,risc_touch_snapshot_v1 *s){(void)c;*s=(risc_touch_snapshot_v1){.width=480,.height=800};return true;}
static const risc_display_output_api_v1 disp={1,sizeof(disp),0,info_get,frame_get,frame_free,frame_submit,status,0,0};
static const risc_input_navigation_api_v1 nav={1,sizeof(nav),0,nav_poll,foreground,reset};
static const risc_touch_api_v1 tch={1,sizeof(tch),0,subscribe,unsubscribe,touch_poll,touch_next,snapshot};
static bool cap_get(const char *s,uint32_t v,uint64_t i,risc_runtime_capability_v1 *g){(void)v;(void)i;if(!strcmp(s,"display.output"))g->api=&disp;else if(!strcmp(s,"input.navigation")){g->api=&nav;navigation_live=true;}else g->api=&tch;return true;}
static bool cap_free(risc_runtime_capability_v1 *g){if(g->api==&nav)navigation_live=false;g->api=0;return true;}
static const risc_runtime_api_v1 rt={1,sizeof(rt),health,delay,log_message,launch,cap_get,cap_free};
const risc_runtime_api_v1 *risc_runtime_get_api(uint32_t v){(void)v;return &rt;}
int main(int argc,char **argv) {
    (void)argv;
    claim_failure = argc == 1;
    int init = app_module_init();
    if (claim_failure) {
        assert(init == -1 && clears == 1 && !suppressed && !navigation_live && !after_release);
        puts("Failed foreground claim rolls back suppression before releasing grant PASS");
    } else {
        assert(init == 0);
        clear_failure = true;
        assert(!cleanup());
        assert(navigation_live && foreground_claimed && suppressed);
        app_module_fini();
        assert(clears == 2 && !after_release && !navigation_live && !suppressed);
        puts("Failed foreground clear retains grant for fini retry; no call after revocation PASS");
    }
    return 0;
}
