#include "observer.h"
#define app_main store_production_main
#include "../../Apps/app_store_scene.c"
#undef app_main
extern void store_test_mapping(unsigned,store_test_observer);
extern unsigned store_test_entry(void);
static void observe(store_test_view*out){*out=(store_test_view){.document=as_document,.navigation=as_nav,.flags=as_flags,.session=as_session};}
__attribute__((constructor)) static void mapped(void){store_test_mapping(1,observe);}
__attribute__((destructor)) static void unmapped(void){store_test_mapping(2,observe);}
__attribute__((visibility("default"))) int app_main(void){unsigned entry=store_test_entry();int result=store_production_main();const risc_runtime_api_v1*rt=risc_runtime_get_api(1);if(entry==1&&rt&&rt->request_launch)(void)rt->request_launch("default.elf");return result;}
