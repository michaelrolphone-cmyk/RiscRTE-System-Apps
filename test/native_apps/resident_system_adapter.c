/* Test-only observations of the production adapter compiled in each ELF. */
#include "../../lib/PortableApps/src/adapter.c"
int system_checkpoint(unsigned reason) {
#ifdef PORTABLE_RESIDENT_SHELL_CLIENT
 return resident_checkpoint(reason);
#else
 (void)reason;return RISC_RESIDENT_DENIED;
#endif
}
bool system_drain(void){return portable_paper_frame_drain();}
unsigned system_client_pending(void){
#ifdef PORTABLE_RESIDENT_SHELL_CLIENT
 return (resident_activity_pending?1u:0u)|(resident_policy_pending?2u:0u);
#else
 return 0;
#endif
}
void system_host_busy(bool busy){
#ifdef PORTABLE_RESIDENT_SHELL_HOST
 quick_modal=busy;
#else
 (void)busy;
#endif
}
