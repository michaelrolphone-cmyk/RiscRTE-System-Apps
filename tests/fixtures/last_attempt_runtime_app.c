#include <stdbool.h>
#include <string.h>
#include "RiscRuntimeV1.h"
#include "PortableLastAttempt.h"
static bool failed,native_custody_retained;
static const risc_runtime_api_v1 *native_custody_runtime;
#include "last_attempt.inc"
extern int last_attempt_test_stage(unsigned);
__attribute__((visibility("default"))) int app_module_init(void){
 native_custody_runtime=risc_runtime_get_api(1);last_attempt_initialize();return 0;
}
__attribute__((visibility("default"))) void app_main(void){
 PORTABLE_LAST_ATTEMPT("fixture.operation");
 /* Model a false provider result, leaving the production diagnostic latch to
  * stop itself; neither failure nor retention flag is set yet. */
 (void)portable_last_attempt_bool(false);
 PORTABLE_LAST_ATTEMPT("must-not-overwrite-false");
 if(last_attempt_test_stage(0)){
  if(!native_custody_runtime->retain_invocation())__builtin_trap();
  native_custody_retained=true;
 }
 PORTABLE_LAST_ATTEMPT("must-not-overwrite-return");
 (void)last_attempt_test_stage(1);
}
__attribute__((visibility("default"))) void app_module_fini(void){PORTABLE_LAST_ATTEMPT("must-not-overwrite-fini");}
