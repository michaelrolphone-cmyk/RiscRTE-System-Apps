#include <stdint.h>
#include <stdbool.h>
static uint64_t bound_namespace=UINT64_C(0x43525348),bound_volume=9;
static bool binding_retained;
uint64_t portable_home_crash_namespace(void){return bound_namespace;}
uint64_t portable_home_catalog_volume(void){return bound_volume;}
bool portable_home_bindings_retained(void){return binding_retained;}
#define PORTABLE_SHARED_HOME_CANVAS
#define PORTABLE_CRASH_REPORT_NAMESPACE portable_home_crash_namespace()
#define main original_archive_tests
#include "failure_archive_lifecycle_test.c"
#undef main
int main(void){
 original_archive_tests();
 reset();bound_namespace=0;
 assert(capture(31)&&!failure_archive_committed&&!f.counts[ACQUIRE_DATA]);
 finish_presentation();assert(!f.acks);
 assert(failure_archive_checkpoint()&&!f.counts[ACQUIRE_DATA]&&!f.counts[ACQUIRE_SD]&&!f.acks);
 reset();bound_namespace=UINT64_C(0x43525348);binding_retained=true;
 assert(!capture(32)&&native_custody_retained&&!f.counts[ACQUIRE_DATA]&&!f.acks);
 binding_retained=false;reset();bound_volume=0;
 assert(capture(33)&&failure_archive_committed);finish_presentation();
 assert(failure_archive_checkpoint()&&!f.counts[ACQUIRE_SD]);
 puts("Shared overview crash bindings: original lifecycle, namespace0 no acquire/ACK, retained lookup fence and unavailable SD preserved PASS");
}
