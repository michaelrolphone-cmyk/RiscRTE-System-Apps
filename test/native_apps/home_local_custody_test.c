#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include "RiscRuntimeV1.h"
#define PORTABLE_SHARED_HOME_CANVAS 1
#define PORTABLE_ALARM_TERMINAL_RETENTION 1
#define PORTABLE_NATIVE_CUSTODY_FENCE 1
#define PORTABLE_ALARM_CLIENT 1
typedef struct {unsigned frame;} risc_display_surface_v1;
static bool native_custody_retained,failed,native_sleep_retained;
static const void *gauge;
static const risc_runtime_api_v1 *native_custody_runtime;
static risc_display_surface_v1 surface;
static volatile risc_display_surface_v1 native_custody_surface;
static unsigned logs,retains;static bool lost;
static bool retain(void){assert(!lost);++retains;return true;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.retain_invocation=retain};
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t version){assert(version==1);return lost?NULL:&runtime;}
static void portable_stage_log(const risc_runtime_api_v1*r,const char*a,const char*b){(void)r;(void)a;(void)b;assert(!lost);++logs;}
#include "../../lib/PortableApps/src/native_custody_adapter.inc"
int main(void){
 native_custody_runtime=&runtime;surface.frame=7;lost=true;
 portable_adapter_retain();
 assert(native_custody_retained&&native_sleep_retained&&failed&&!logs&&!retains&&!surface.frame&&native_custody_surface.frame==7);
 native_custody_retained=native_sleep_retained=failed=lost=false;surface.frame=9;
 portable_adapter_retain_silent();
 assert(native_custody_retained&&native_sleep_retained&&failed&&!logs&&retains==1&&portable_adapter_retained());
 puts("Home custody: lost native identity performs only local halt; live context retains exactly once PASS");
}
