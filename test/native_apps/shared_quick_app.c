#include "shared_quick_reference.h"
#include "PortableApps.h"
#include "PortableNativeCustody.h"
#include <assert.h>
#include <string.h>
const t5_app_manifest_t portable_catalog[]={{.compatible=false}};
const unsigned portable_catalog_count=0;
#define app_main reference_controller_main
#ifdef PORTABLE_RESIDENT_SHELL_HOST
/* Sparse Home/real sleep is qualified separately; this Runtime harness owns
 * foreground routing and must never request terminal sleep. */
bool portable_desk_adapter_sleep(void){assert(!"Foreground routing fixture must not sleep");return false;}
#include "../../Apps/paper_clock.c"
#define KIND 0
extern void reference_observe_ui(reference_ui*);
extern bool reference_ui_point(unsigned,int*,int*);
extern void reference_finish_home(void);
#elif defined(PORTABLE_FILE_BROWSER_APP)
#include "../../Apps/file_browser.c"
#define KIND 1
#elif defined(PORTABLE_USB_TRANSFER_APP)
#include "../../Apps/usb_sd_transfer.c"
#define KIND 2
#endif
#undef app_main
__attribute__((constructor)) static void mapped(void){reference_event(10,KIND);}
__attribute__((destructor)) static void unmapped(void){reference_event(11,KIND);}
void app_main(void){
#ifdef PORTABLE_RESIDENT_SHELL_HOST
 reference_hooks(reference_observe_ui,reference_ui_point,reference_finish_home);
#endif
 reference_event(12,KIND);
#ifdef PORTABLE_FILE_BROWSER_APP
 assert(fb_open());assert(fb_count>=4);fb_choice=3;fbs_selected=fbs_directory_selected=3;strcpy(fb_editor,"preserved draft");
#endif
 reference_controller_main();if(portable_adapter_retained()){reference_event(14,KIND);return;}
#ifdef PORTABLE_FILE_BROWSER_APP
 assert(fb_choice==3&&fbs_selected==3&&!strcmp(fb_editor,"preserved draft"));
#endif
 reference_event(13,KIND);
}
