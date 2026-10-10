/* Actual System controllers with test-only setup and model observations.
 * The concluding Home path runs the normal controller loop. All checkpoints,
 * host dispatch, grants, focus and ELF lifetime use production Runtime. */
#include "PortableApps.h"
#include "PortableResidentShell.h"
#include "PortableNativeCustody.h"
#include "RiscResidentShellV1.h"
#include "../../Apps/PaperPresentation.h"
#include <assert.h>
#include <string.h>
extern const char *system_mode(void);
extern void system_event(unsigned,unsigned);
extern void system_busy(bool);
extern void system_capture(bool);
extern void system_home(void);
extern unsigned system_client_visit(void);
extern void system_set_busy_hook(void (*)(bool));
extern int system_checkpoint(unsigned);
extern bool system_drain(void);
extern unsigned system_client_pending(void);
extern void system_host_busy(bool);
static bool mode(const char *s){return !strcmp(system_mode(),s);}
const t5_app_manifest_t portable_catalog[]={{.compatible=false}};
const unsigned portable_catalog_count=0;
#ifdef PORTABLE_RESIDENT_SHELL_HOST
#define ROLE 1
#else
#define ROLE 2
#define app_main system_controller_main
#if defined(PORTABLE_FILE_BROWSER_APP)
#include "../../Apps/file_browser.c"
#elif defined(PORTABLE_WIFI_SETTINGS_APP)
#include "../../Apps/wifi_settings.c"
#elif defined(PORTABLE_UPDATE_APP)
#include "../../Apps/update_portable.inc"
#elif defined(PORTABLE_USB_TRANSFER_APP)
#include "../../Apps/usb_sd_transfer.c"
#endif
#undef app_main
static void prepare_model(void){
#ifdef PORTABLE_FILE_BROWSER_APP
 assert(fb_open());if(mode("file-open")&&system_client_visit()==2){assert(!strcmp(fb_status,"File handler returned"));system_event(14,1);}assert(fb_count>=4);fb_choice=3;fbs_selected=fbs_directory_selected=3;
 strcpy(fb_editor,"draft-name.txt");fb_draw();
#elif defined(PORTABLE_WIFI_SETTINGS_APP)
 assert(wifi_open());page=WP_SSID;choice=4;strcpy(editor,"uncommitted-network");strcpy(credentials.ssid,"Saved Network");draft_dirty=true;
 wifi_make_view();portable_wifi_render(&view);
#elif defined(PORTABLE_UPDATE_APP)
 assert(open_update());assert(us->status(us->context,&ust));selected=3;assert(us->get(us->context,2,&selected_row));render_update();
#elif defined(PORTABLE_USB_TRANSFER_APP)
 app=t5_app_get_api(1);paper=paper_presentation_get();runtime=risc_runtime_get_api(1);assert(app&&paper&&runtime);state=RISC_USB_MSC_IDLE;dirty=true;app->set_back_exits_app(false);render();
#endif
 assert(system_drain());
}
static void verify_model(void){
#ifdef PORTABLE_FILE_BROWSER_APP
 assert(fb_choice==3&&fbs_selected==3&&!strcmp(fb_path,"/")&&!strcmp(fb_editor,"draft-name.txt"));
#elif defined(PORTABLE_WIFI_SETTINGS_APP)
 assert(page==WP_SSID&&choice==4&&!strcmp(editor,"uncommitted-network")&&!strcmp(credentials.ssid,"Saved Network")&&draft_dirty);
#elif defined(PORTABLE_UPDATE_APP)
 assert(selected==3&&!strcmp(selected_row.id,"fixture-02")&&!strcmp(selected_row.version,"1.2.3"));
#elif defined(PORTABLE_USB_TRANSFER_APP)
 assert(!portable_usb_transfer_owned());
#endif
 system_event(4,1);
}
static void cleanup_block(bool blocked){
#ifdef PORTABLE_FILE_BROWSER_APP
 fb_retained_file=blocked?99:0;
#elif defined(PORTABLE_WIFI_SETTINGS_APP) || defined(PORTABLE_UPDATE_APP)
 cleanup_pending=blocked;
#elif defined(PORTABLE_USB_TRANSFER_APP)
 if(blocked){start();assert(portable_usb_transfer_owned());}
 else {assert(end_session(RISC_USB_MSC_END_CANCEL_WAITING,false));assert(!portable_usb_transfer_owned());}
#endif
}
static void policy_block(bool blocked){
#ifdef PORTABLE_WIFI_SETTINGS_APP
 joining=blocked;
#elif defined(PORTABLE_UPDATE_APP)
 confirming=blocked;
#else
 (void)blocked;
#endif
}
#endif
__attribute__((constructor)) static void mapped(void){system_event(10,ROLE);}
__attribute__((destructor)) static void unmapped(void){system_event(11,ROLE);}
void app_main(void){
 system_event(12,ROLE);
#ifdef PORTABLE_RESIDENT_SHELL_HOST
 const t5_app_api_v1 *a=t5_app_get_api(1);const paper_presentation *p=paper_presentation_get();assert(a&&p);a->set_back_exits_app(false);
 p->begin();a->fill_rect(90,180,110,140,true);a->present(true);system_set_busy_hook(system_host_busy);
 int status=portable_resident_run_foreground("client.elf");system_event(1,(unsigned)(status+1));
#else
 prepare_model();
 const t5_app_api_v1 *saved_api=t5_app_get_api(1);const paper_presentation *saved_paper=paper_presentation_get();assert(saved_api&&saved_paper);
 if(mode("cleanup-terminal")) {
#ifdef PORTABLE_FILE_BROWSER_APP
  fb_retained_file=99;assert(!portable_file_browser_close());
#elif defined(PORTABLE_WIFI_SETTINGS_APP)
  assert(!portable_wifi_suspend());
#elif defined(PORTABLE_UPDATE_APP)
  assert(!portable_update_suspend());
#endif
  assert(portable_adapter_retained());t5_app_input_t in={0};assert(!saved_api->poll(&in,20));saved_paper->begin();saved_api->present(false);assert(!system_drain());system_event(3,1);return;
 }
 if(mode("busy")) {
  const paper_presentation *p=paper_presentation_get();p->begin();
  assert(system_checkpoint(RISC_RESIDENT_CHECKPOINT_CONTROLS)==RISC_RESIDENT_BUSY);
  t5_app_get_api(1)->present(false);assert(system_drain());
  system_busy(true);assert(system_checkpoint(RISC_RESIDENT_CHECKPOINT_CONTROLS)==RISC_RESIDENT_BUSY);system_busy(false);verify_model();
 }
 if(mode("cleanup")) {cleanup_block(true);assert(system_checkpoint(RISC_RESIDENT_CHECKPOINT_CONTROLS)==RISC_RESIDENT_BUSY);cleanup_block(false);verify_model();}
 if(mode("capture")) {system_capture(true);assert(system_checkpoint(RISC_RESIDENT_CHECKPOINT_POLL)==RISC_RESIDENT_BUSY);assert(system_client_pending()&1u);system_capture(false);}
 if(mode("policy-busy")) {
  policy_block(true);assert(system_checkpoint(RISC_RESIDENT_CHECKPOINT_POLL)==RISC_RESIDENT_OK);assert(!(system_client_pending()&2u));
  assert(system_checkpoint(RISC_RESIDENT_CHECKPOINT_POLICY)==RISC_RESIDENT_BUSY);policy_block(false);
 }
 if(mode("poll")||mode("capture")||mode("policy-busy")) {
#ifdef PORTABLE_USB_TRANSFER_APP
  assert(system_checkpoint(RISC_RESIDENT_CHECKPOINT_POLL)==RISC_RESIDENT_BUSY);assert(system_client_pending()&1u);
#else
  assert(system_checkpoint(RISC_RESIDENT_CHECKPOINT_POLL)==RISC_RESIDENT_OK);assert(system_client_pending()&2u);
  assert(system_checkpoint(RISC_RESIDENT_CHECKPOINT_POLICY)==RISC_RESIDENT_OK);assert(!system_client_pending());
#endif
  verify_model();
 } else {
  int status=system_checkpoint(RISC_RESIDENT_CHECKPOINT_CONTROLS);
  if(mode("terminal")){assert(status==RISC_RESIDENT_RETAINED&&portable_adapter_retained());t5_app_input_t in={0};assert(!saved_api->poll(&in,20));saved_paper->begin();saved_api->present(false);assert(!system_drain());system_event(3,1);return;}
  assert(status==RISC_RESIDENT_OK);verify_model();
 }

#ifdef PORTABLE_FILE_BROWSER_APP
 if(mode("file-open")&&system_client_visit()==1){fb_selected=fb_rows[3];strcpy(fb_file_path,"/File 003.txt");fbx_open(false);assert(fb_terminal&&!fb_volume&&!fb_handler_api);system_event(15,1);return;}
#endif
 system_home();system_controller_main();assert(!portable_adapter_retained());system_event(3,1);
#endif
}
