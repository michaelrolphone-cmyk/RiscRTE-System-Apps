/* Selected shared policy: one attempt per owned IP connection, plus explicit
 * retry. Never scans/connects/loads credentials or changes timezone itself. */
#include "PortableTimeSync.h"
#ifdef PORTABLE_FOREGROUND_IDLE
#define WIFI_TIME_RETAIN() portable_adapter_retain_silent()
#else
#define WIFI_TIME_RETAIN() PORTABLE_APP_RETAIN()
#endif
#ifndef PORTABLE_TIME_SYNC_SERVER
#define PORTABLE_TIME_SYNC_SERVER "pool.ntp.org"
#endif
static portable_time_sync wifi_time;
static bool wifi_time_attempted,wifi_time_ip,wifi_time_lease,wifi_time_cancelling,wifi_time_disconnect_pending,wifi_time_radio_stop;
static unsigned wifi_time_row;
static const char *wifi_time_notice="Connect to sync time";
extern int portable_wifi_time_commit(portable_time_sync *,const char **);
static bool wifi_time_guard(void *context){(void)context;return !PORTABLE_APP_RETAINED();}
static bool wifi_time_cancel(void){
 if(PORTABLE_APP_RETAINED())return false;
 if(wifi_time.self){
  int state=portable_time_sync_cancel(&wifi_time);
  if(state==PORTABLE_TIME_SYNC_RETAINED){WIFI_TIME_RETAIN();return false;}
  if(state==PORTABLE_TIME_SYNC_CLOSING){wifi_time_cancelling=true;wifi_time_notice="Stopping time sync...";dirty=true;return false;}
 }
 wifi_time_cancelling=false;
 if(wifi_time_lease){if(!portable_wifi_services_end())return false;wifi_time_lease=false;}
 return true;
}
static void wifi_time_update(void){
 if(PORTABLE_APP_RETAINED())return;
 if(wifi_time_cancelling){if(wifi_time_cancel()){wifi_time_notice="Time sync cancelled";dirty=true;}return;}
 if(link_state!=WIFI_LINK_UP || !wifi_time_ip){
  if(wifi_time_lease && !wifi_time_cancel())return;
  wifi_time_attempted=false;return;
 }
 if(!wifi_time_attempted){
  if(!portable_wifi_services_begin())return;
  wifi_time_lease=true;wifi_time_attempted=true;
  (void)portable_time_sync_start(&wifi_time,wr,wifi_time_guard,NULL,"net.time-sample",0,
   PORTABLE_TIME_SYNC_SERVER,5000);
  wifi_time_notice="Syncing; unauthenticated SNTP";dirty=true;
 }
 if(!wifi_time_lease)return;
 int state=portable_time_sync_step(&wifi_time);
 if(state==PORTABLE_TIME_SYNC_RETAINED){WIFI_TIME_RETAIN();return;}
 if(state==PORTABLE_TIME_SYNC_PENDING || state==PORTABLE_TIME_SYNC_CLOSING)return;
 if(state==PORTABLE_TIME_SYNC_SAMPLE){
  int committed=portable_wifi_time_commit(&wifi_time,&wifi_time_notice);
  if(committed==-2){WIFI_TIME_RETAIN();return;}
  if(!committed)return;
 }else wifi_time_notice=wifi_time.result==RISC_TIME_SAMPLE_TIMEOUT?"Time server timed out; retry":
   wifi_time.result==RISC_TIME_SAMPLE_PROTOCOL?"Invalid time response; retry":"Time sync failed; retry";
 if(!portable_wifi_services_end())return;
 wifi_time_lease=false;dirty=true;
}
