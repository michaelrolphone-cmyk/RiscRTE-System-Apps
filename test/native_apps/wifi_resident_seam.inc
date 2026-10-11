/* Production resident checkpoint with a synthetic dispatch boundary. */
#define PORTABLE_RESIDENT_SHELL_CLIENT
#define PORTABLE_RESIDENT_POLICY
#include "RiscResidentShellV1.h"
static bool resident_activity_pending,native_custody_retained;
static risc_display_surface_v1 native_custody_surface;
static void portable_adapter_retain(void){native_custody_retained=true;}
#include "../../lib/PortableApps/src/resident_adapter.inc"
static unsigned checkpoints;
static int32_t checkpoint(uint64_t token,const risc_resident_request_v1 *q,risc_resident_reply_v1 *out){
 (void)token;(void)q;(void)out;++checkpoints;
 assert(!portable_wifi_async_owned());
 return RISC_RESIDENT_OK;
}
static void resident_fixture_start(void){resident_ready=true;resident.checkpoint=checkpoint;}
static void resident_fixture_busy(void){
 assert(resident_checkpoint(RISC_RESIDENT_CHECKPOINT_POLL)==RISC_RESIDENT_BUSY);
 assert(!native_custody_retained&&resident_activity_pending);
}
