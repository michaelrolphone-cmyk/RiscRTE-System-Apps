#include "PortableApps.h"
#include "PortableResidentShell.h"
#include "RiscRuntimeV1.h"
#include "RiscResidentShellV1.h"
#include "RiscKeyValueV1.h"
#include "../../Apps/PaperPresentation.h"
#include <assert.h>
#include <string.h>
extern bool failure_mode(const char *);
extern void failure_event(unsigned,int);
#ifdef PORTABLE_RESIDENT_SHELL_HOST
const t5_app_manifest_t portable_catalog[]={{.compatible=false}};
const unsigned portable_catalog_count=0;
void app_main(void) {
 const t5_app_api_v1 *app=t5_app_get_api(1);const paper_presentation *paper=paper_presentation_get();assert(app&&paper);
 app->set_back_exits_app(false);
 int initial=portable_resident_run_foreground(NULL);
 failure_event(1,initial);
 if(initial<0)return;
 assert(initial==PORTABLE_RESIDENT_NO_PENDING);
 paper->begin();app->fill_rect(60,160,150,140,true);app->present(true);
 int status=portable_resident_run_foreground("client.elf");failure_event(2,status);
 if(status<0)return;
 if(failure_mode("repeat")) {status=portable_resident_run_foreground("client.elf");failure_event(2,status);}
 if(status<0)return;
 /* Acknowledgement must not become Home, sleep or a drawer in the caller. */
 for(unsigned i=0;i<3;++i){t5_app_input_t input={0};assert(app->poll(&input,20)&&!input.exit_requested);}
 assert(!portable_resident_take_sleep() && !portable_resident_take_launch());
 failure_event(3,1);
}
#else
static const risc_runtime_api_v1 *api;
static risc_runtime_capability_v1 grant;
#ifndef FAILURE_BAD_ABI
__attribute__((visibility("default"))) const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_FOREGROUND,0};
#else
__attribute__((visibility("default"))) const risc_resident_app_descriptor_v1_t risc_resident_app_descriptor_v1={1,sizeof(risc_resident_app_descriptor_v1_t),RISC_RESIDENT_ROLE_HOST,0};
#endif
int app_module_init(void) {
 failure_event(4,1);api=risc_runtime_get_api(1);assert(api);
 grant.struct_size=sizeof(grant);assert(api->acquire("storage.key-value",1,1,&grant));
 if(failure_mode("init") || failure_mode("repeat") || failure_mode("touch") || failure_mode("held") || failure_mode("acquire") || failure_mode("surface") || failure_mode("submit") || failure_mode("status") || failure_mode("timeout") || failure_mode("navigation") || failure_mode("touch-failure"))return -1;
 return 0;
}
void app_main(void){failure_event(5,1);if(failure_mode("retained")){assert(api->retain_invocation());return;}}
void app_module_fini(void){failure_event(6,1);if(grant.api)assert(api->release(&grant));}
__attribute__((destructor)) static void unmapped(void){failure_event(7,1);}
#endif
