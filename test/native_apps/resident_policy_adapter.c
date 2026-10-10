/* Compile the actual adapter once per ELF. These test-only exports observe
 * private state and inject a clean host refusal without replacing Runtime. */
#include "../../lib/PortableApps/src/adapter.c"
unsigned policy_test_state(void) {
#ifdef PORTABLE_RESIDENT_SHELL_CLIENT
 return (resident_activity_pending?1u:0u)|(resident_policy_pending?2u:0u);
#else
 return 0;
#endif
}
int policy_test_checkpoint(unsigned reason) {
#ifdef PORTABLE_RESIDENT_SHELL_CLIENT
 return resident_checkpoint(reason);
#else
 (void)reason;return RISC_RESIDENT_DENIED;
#endif
}
void policy_test_host_busy(bool busy) {
#ifdef PORTABLE_RESIDENT_SHELL_HOST
 quick_modal=busy;
#else
 (void)busy;
#endif
}
