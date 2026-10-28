#if !defined(PORTABLE_ALARM_CLIENT) || !defined(ALARM_SERVICE_CLOCK_EDIT_V2)
#error Time sync requires the shared tagged alarm clock editor
#endif
#include "PortableClockCommit.h"
#include "PortableTimeSync.h"
static portable_clock_commit wifi_clock_commit;
static int wifi_clock_guard(void *context){
 portable_time_sync *sample=context;
 if(sample && sample->state==PORTABLE_TIME_SYNC_RETAINED)return PORTABLE_REALTIME_GUARD_RETAINED;
 return native_custody_retained || native_sleep_retained?PORTABLE_REALTIME_GUARD_RETAINED:
  failed?PORTABLE_REALTIME_GUARD_UNCONFIRMED:PORTABLE_REALTIME_GUARD_SAFE;
}
static void wifi_clock_retain(void *context){
 (void)context;
#ifdef PORTABLE_FOREGROUND_IDLE
 portable_adapter_retain_silent();
#else
 portable_adapter_retain();
#endif
}
static bool wifi_clock_fresh(void *context,int64_t *epoch){return portable_time_sync_epoch(context,epoch);}
/* Called only by the foreground Wi-Fi controller, with its network service
 * lease held. A pending frame/modal defers without opening an edit intent. */
int portable_wifi_time_commit(portable_time_sync *sample,const char **message){
 if(wifi_clock_guard(NULL)!=PORTABLE_REALTIME_GUARD_SAFE)return -2;
 if(!display_settled || surface.frame || alarm_modal)return 0;
 int64_t epoch;
 if(!portable_time_sync_epoch(sample,&epoch)){
  if(sample->state==PORTABLE_TIME_SYNC_RETAINED){wifi_clock_retain(NULL);return -2;}
  *message="Time sample expired; retry";return -1;
 }
 wifi_clock_commit.alarms=&alarms;wifi_clock_commit.runtime=rt;
 wifi_clock_commit.guard=wifi_clock_guard;wifi_clock_commit.retain=wifi_clock_retain;
 wifi_clock_commit.context=sample;wifi_clock_commit.prepare_seed=wifi_clock_fresh;
 bool ok=portable_clock_commit_apply(&wifi_clock_commit,epoch);
 if(sample->state==PORTABLE_TIME_SYNC_RETAINED || wifi_clock_guard(NULL)!=PORTABLE_REALTIME_GUARD_SAFE){wifi_clock_retain(NULL);return -2;}
 *message=ok?"Verified; unauthenticated SNTP":wifi_clock_commit.message;
 return ok?1:-1;
}
