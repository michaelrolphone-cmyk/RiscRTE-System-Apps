/* Production Files/controller caller for the Runtime source-broker fixture.
 * Runtime supplies a declared storage.test-volume platform fixture. Rendering
 * is inert; the separate source controller tests exercise RGB/paper controls. */
#define PORTABLE_FILE_BROWSER_APP
#define PORTABLE_FILE_BROWSER_RGB_ONLY
#define PORTABLE_FILE_BROWSER_HANDLERS
#define PORTABLE_FILE_BROWSER_SOURCE_HANDOFF
#define PORTABLE_FILE_BROWSER_CAPABILITY "storage.test-volume"
#define PORTABLE_FILE_BROWSER_INSTANCE 0u
#define app_main source_production_browser_main
#include "../../../Apps/file_browser.c"
#undef app_main
#include <assert.h>
extern unsigned source_browser_visit(void);
extern void source_browser_event(const char*);
static unsigned polls;
static int32_t dimension(void){return 240;}
static uint32_t milliseconds(void){return polls*10;}
static void present(bool full){(void)full;}
static bool poll_input(t5_app_input_t*input,uint32_t wait){
 (void)wait;memset(input,0,sizeof(*input));assert(++polls<20000);
 if(source_browser_visit()>1){assert(fsource_state==FSOURCE_NONE&&!strcmp(fb_status,"File handler returned"));source_browser_event("result");return false;}
 if(polls==1){assert(fb_loaded&&fb_count==1&&!strcmp(fb_rows[0].name,"Original.OBJ")&&fb_manage);return true;}
 if(polls==2||polls==4){input->buttons=T5_APP_BUTTON_CONFIRM;return true;}
 if(polls==3){assert(fb_mode==FB_ACTIONS);return true;}
 if(fb_mode==FB_HANDLERS){assert(!strcmp(fb_handlers[0].app_id,"viewer"));if(polls%2==0)input->buttons=T5_APP_BUTTON_CONFIRM;}
 else assert(fsource_busy()&&fb_mode==FB_NOTICE);
 return true;
}
static const t5_app_api_v1 app_api={.abi_version=1,.struct_size=sizeof(app_api),.screen_width=dimension,.screen_height=dimension,.present=present,.poll=poll_input,.millis=milliseconds};
const t5_app_api_v1*t5_app_get_api(uint32_t version){return version==1?&app_api:NULL;}
const paper_presentation*paper_presentation_get(void){return NULL;}
void portable_nova_begin(void){}
void portable_nova_header(const char*t){(void)t;}
void portable_nova_text(unsigned f,int x,int y,int w,const char*t,uint32_t c){(void)f;(void)x;(void)y;(void)w;(void)t;(void)c;}
void portable_nova_fill(int x,int y,int w,int h,uint32_t c){(void)x;(void)y;(void)w;(void)h;(void)c;}
void portable_nova_button(int x,int y,int w,int h,const char*t,bool selected){(void)x;(void)y;(void)w;(void)h;(void)t;(void)selected;}
void portable_nova_row(int x,int y,int w,int h,const char*l,const char*v,bool selected){(void)x;(void)y;(void)w;(void)h;(void)l;(void)v;(void)selected;}
bool portable_nova_wrap(unsigned f,int x,int y,int w,int h,unsigned n,const char*t,uint32_t c){(void)f;(void)x;(void)y;(void)w;(void)h;(void)n;(void)t;(void)c;return true;}
__attribute__((visibility("default"))) int app_module_init(void){source_browser_event("init");return 0;}
__attribute__((visibility("default"))) void app_module_fini(void){assert(portable_file_browser_safe()&&!fb_grant.api&&!fb_handler_grant.api&&!fsource_grant.api);source_browser_event("fini");}
__attribute__((visibility("default"))) void app_main(void){source_browser_event("main");source_production_browser_main();assert(portable_file_browser_safe()&&!fb_grant.api&&!fsource_grant.api);if(source_browser_visit()==1)assert(fb_terminal);source_browser_event("return");}
