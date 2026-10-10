/* Compile the actual controller with selectors from the target build record.
 * UI callbacks are inert: this fixture tests acquisition and controller state,
 * while the separate raster/raw-touch fixtures cover the portable adapter. */
#include <assert.h>
#define app_main production_file_browser_main
#include "../../../Apps/file_browser.c"
#undef app_main

extern const char *file_browser_runtime_mode(void);
extern void file_browser_runtime_event(const char *name);
extern void file_browser_runtime_owner(bool owned);
extern void file_browser_runtime_recover(void);
extern unsigned file_browser_runtime_visit(void);
static unsigned visit, polls;
static bool mode(const char *name) {return !strcmp(file_browser_runtime_mode(),name);}
static bool rejected(void) {return mode("wrong-instance") || mode("wrong-capability");}
static bool nested(void) {return mode("nested-no-handler") || mode("nested-handoff");}
static bool handoff(void) {return mode("handoff") || mode("nested-handoff");}
static int32_t dimension(void) {return 240;}
static uint32_t milliseconds(void) {return polls*25;}
static void present(bool full) {(void)full;}
static bool nested_input(t5_app_input_t *input) {
    const unsigned step=polls++;assert(step<=18);
    if(step==0) {
        assert(fb_loaded && fb_count==1 && !strcmp(fb_path,"/") && !strcmp(fb_rows[0].name,"Books") && fb_rows[0].is_directory);
        if(visit==2) {
            assert(mode("nested-handoff") && !strcmp(fb_status,"File handler returned") && !fb_query[0]);
            file_browser_runtime_event("fresh-caller-return");return false;
        }
    } else if(step==2) {
        assert(fb_loaded && fb_count==1 && !strcmp(fb_path,"/Books") && !strcmp(fb_rows[0].name,"read.txt") && !fb_rows[0].is_directory);
    } else if(step==4) {
        assert(fb_mode==FB_ACTIONS && fb_choice==0 && !strcmp(fb_file_path,"/Books/read.txt"));
    } else if(step==10) {
        assert(fb_mode==FB_PREVIEW && fb_preview_count==5 && !memcmp(fb_preview,"hello",5));
        assert(!fb_retained_file && portable_file_browser_safe());file_browser_runtime_event("nested-preview");
    } else if(step==12)assert(fb_mode==FB_ACTIONS && fb_choice==2);
    else if(step==16)assert(fb_mode==FB_ACTIONS && fb_choice==0);
    else if(step==18) {
        assert(mode("nested-no-handler") && fb_mode==FB_NOTICE && strstr(fb_status,"No declared application"));
        assert(!fb_terminal && !strcmp(fb_handler_path,"/sd/Books/read.txt"));
        file_browser_runtime_event("no-handler");return false;
    }
    /* Release between every press, through the real controller input loop:
     * open Books, select read.txt, Preview, Back, then Open. */
    if(step==1 || step==3 || step==9 || step==17)input->buttons=T5_APP_BUTTON_CONFIRM;
    else if(step==5 || step==7)input->buttons=T5_APP_BUTTON_DOWN;
    else if(step==11)input->buttons=T5_APP_BUTTON_BACK;
    else if(step==13 || step==15)input->buttons=T5_APP_BUTTON_UP;
    return true;
}
static bool poll_input(t5_app_input_t *input,uint32_t wait) {
    (void)wait;memset(input,0,sizeof(*input));if(nested())return nested_input(input);
    assert(polls<6);
    if(!polls++) {
        if(rejected()) {assert(!fb_loaded && !fb_volume && strstr(fb_status,"unavailable"));return false;}
        if(mode("absent") || mode("refresh-error") || mode("listing-error")) {
            assert(!fb_loaded && !fb_count && fb_volume && fb_grant.api);
            assert(strstr(fb_status,mode("absent")?"absent":mode("refresh-error")?"refresh error":"read failed"));return false;
        }
        assert(fb_loaded && fb_count==1 && !strcmp(fb_rows[0].name,"read.txt") && fb_manage);
        if(mode("handoff") && visit==2) {assert(!strcmp(fb_status,"File handler returned"));file_browser_runtime_event("fresh-caller-return");return false;}
        return true;
    }
    if(polls==2 || polls==4) {input->buttons=T5_APP_BUTTON_CONFIRM;return true;}
    if(polls==3) {assert(fb_mode==FB_ACTIONS && !strcmp(fb_file_path,"/read.txt"));return true;}
    assert(mode("no-handler") && fb_mode==FB_NOTICE && strstr(fb_status,"No declared application"));
    assert(!fb_terminal);file_browser_runtime_event("no-handler");return false;
}
static const t5_app_api_v1 app_api={.abi_version=1,.struct_size=sizeof(app_api),
    .screen_width=dimension,.screen_height=dimension,.present=present,.poll=poll_input,.millis=milliseconds};
const t5_app_api_v1 *t5_app_get_api(uint32_t version) {return version==1?&app_api:NULL;}
const paper_presentation *paper_presentation_get(void) {return NULL;}
void portable_nova_begin(void) {}
void portable_nova_header(const char *title) {(void)title;}
void portable_nova_text(unsigned face,int x,int y,int width,const char *text,uint32_t color) {(void)face;(void)x;(void)y;(void)width;(void)text;(void)color;}
void portable_nova_fill(int x,int y,int width,int height,uint32_t color) {(void)x;(void)y;(void)width;(void)height;(void)color;}
void portable_nova_button(int x,int y,int width,int height,const char *text,bool selected) {(void)x;(void)y;(void)width;(void)height;(void)text;(void)selected;}
void portable_nova_row(int x,int y,int width,int height,const char *label,const char *value,bool selected) {(void)x;(void)y;(void)width;(void)height;(void)label;(void)value;(void)selected;}
bool portable_nova_wrap(unsigned face,int x,int y,int width,int line_height,unsigned max_lines,const char *text,uint32_t color) {(void)face;(void)x;(void)y;(void)width;(void)line_height;(void)max_lines;(void)text;(void)color;return true;}

__attribute__((visibility("default"))) int app_module_init(void) {file_browser_runtime_event("app-init");return 0;}
__attribute__((visibility("default"))) void app_module_fini(void) {
    assert(portable_file_browser_safe() && !fb_grant.api && !fb_handler_grant.api);
    file_browser_runtime_event("app-fini");
}
__attribute__((visibility("default"))) void app_main(void) {
    visit=file_browser_runtime_visit();
    const risc_runtime_api_v1 *runtime=risc_runtime_get_api(1);assert(runtime);
    risc_runtime_capability_v1 denied={.struct_size=sizeof(denied)};
    assert(!runtime->acquire("storage.volume",1,8,&denied) && !denied.api);
    assert(!runtime->acquire("storage.installed-files",1,9,&denied) && !denied.api);
    assert(!runtime->acquire("storage.volume",2,9,&denied) && !denied.api);
    file_browser_runtime_owner(false);
    assert(!risc_runtime_get_api(1) && !runtime->acquire("storage.volume",1,9,&denied));
    file_browser_runtime_owner(true);
    if(mode("dir-close") || mode("file-close")) {
        assert(fb_open() && fb_grant.api && fb_handler_grant.api);
        if(mode("dir-close"))assert(!fb_loaded && !portable_file_browser_safe());
        else {
            assert(fb_loaded);fb_activate_file(0);
            assert(!fb_preview_read() && fb_retained_file && !portable_file_browser_safe());
        }
        const risc_runtime_capability_v1 before=fb_grant;
        assert(!portable_file_browser_close() && fb_grant.api==before.api && fb_grant.generation==before.generation);
        file_browser_runtime_recover();assert(portable_file_browser_close());
        risc_runtime_capability_v1 stale=before;assert(!runtime->release(&stale));
        assert(fb_load(0) && fb_loaded);assert(portable_file_browser_close());
        file_browser_runtime_event("cleanup-recovered");
    } else production_file_browser_main();
    assert(portable_file_browser_safe() && !fb_grant.api && !fb_handler_grant.api);
    if(handoff() && visit==1) {
        assert(fb_terminal);
        assert(!strcmp(fb_handler_path,nested()?"/sd/Books/read.txt":"/sd/read.txt"));
    }
    file_browser_runtime_event("app-return");
}
