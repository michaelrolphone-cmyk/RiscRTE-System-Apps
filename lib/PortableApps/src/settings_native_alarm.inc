/* Manual Settings and network time use the same alarm-aware UTC transaction. */
#if !defined(PORTABLE_SETTINGS_NATIVE_ONLY) || !defined(ALARM_SERVICE_CLOCK_EDIT_V2) || !defined(PORTABLE_ALARM_CLIENT)
#error Native clock editor requires native-only Settings and selected tagged alarm client
#endif
#include "PortableClockCommit.h"
static portable_clock_commit snt_commit;
static void snt_retain(void *context){(void)context;portable_adapter_retain();}
static bool snt_clock_commit(int64_t epoch){
 snt_commit.alarms=&alarms;snt_commit.runtime=rt;snt_commit.guard=snt_guard;
 snt_commit.retain=snt_retain;snt_commit.context=NULL;
 bool result=portable_clock_commit_apply(&snt_commit,epoch);
 editor_message=snt_commit.message;
 snt_last_result.native=snt_commit.seeded;
 return result;
}
