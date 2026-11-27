/* Full production Files root; only module lifecycle is observed. */
#include "../../Apps/files_scene.c"
#include <assert.h>
extern void scene_test_lifecycle(unsigned);
__attribute__((visibility("default"))) int app_module_init(void){scene_test_lifecycle(1);return 0;}
__attribute__((visibility("default"))) void app_module_fini(void){assert(files_clean&&!files_retained&&!files_ui.token);scene_test_lifecycle(2);}
__attribute__((destructor)) static void unloaded(void){scene_test_lifecycle(3);}
/* Failure-only trace: diagnostic observation, no application state change. */
#include <execinfo.h>
__attribute__((no_instrument_function)) void __cyg_profile_func_enter(void*function,void*caller){(void)caller;if(function==(void*)files_retain){void*frames[32];int n=backtrace(frames,32);backtrace_symbols_fd(frames,n,2);}}
__attribute__((no_instrument_function)) void __cyg_profile_func_exit(void*function,void*caller){(void)function;(void)caller;}
