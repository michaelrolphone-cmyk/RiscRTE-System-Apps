#include "PortableTimeSync.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
static bool terminal,dirty;static unsigned service_depth,begin_count,end_count,commit_count;
static int link_state;enum {WIFI_LINK_UP=2};
static const risc_runtime_api_v1 *wr;
#define PORTABLE_APP_RETAINED() terminal
#define PORTABLE_APP_RETAIN() (terminal=true)
static bool portable_wifi_services_begin(void){assert(!terminal);service_depth++;begin_count++;return true;}
static bool portable_wifi_services_end(void){assert(!terminal&&service_depth==1);service_depth--;end_count++;return true;}
static int outcome;static bool close_retained,close_pending;
int portable_time_sync_start(portable_time_sync*c,const risc_runtime_api_v1*r,portable_time_sync_guard g,void*x,const char*n,uint64_t i,const char*s,uint32_t t){(void)r;(void)g;(void)x;assert(service_depth==1&&!strcmp(n,"net.time-sample")&&!i&&!strcmp(s,"pool.ntp.org")&&t==5000);c->self=c;c->state=1;return 1;}
int portable_time_sync_step(portable_time_sync*c){assert(service_depth==1);c->state=outcome;return outcome;}
int portable_time_sync_cancel(portable_time_sync*c){assert(!terminal);c->state=close_retained?4:close_pending?5:0;return c->state;}
int portable_wifi_time_commit(portable_time_sync*c,const char**m){assert(service_depth==1&&c->state==2);commit_count++;*m="verified";return 1;}
#include "../../Apps/wifi_time_sync.inc"
static void reset(void){terminal=dirty=false;service_depth=begin_count=end_count=commit_count=0;link_state=0;wifi_time=(portable_time_sync){0};wifi_time_attempted=wifi_time_ip=wifi_time_lease=false;outcome=1;close_retained=close_pending=wifi_time_cancelling=false;}
int main(void){reset();wifi_time_update();assert(!begin_count);link_state=WIFI_LINK_UP;wifi_time_update();assert(!begin_count);wifi_time_ip=true;wifi_time_update();assert(begin_count==1&&service_depth==1);wifi_time_update();assert(begin_count==1);outcome=2;wifi_time_update();assert(commit_count==1&&end_count==1&&!service_depth);wifi_time_update();assert(commit_count==1);wifi_time_attempted=false;outcome=3;wifi_time_update();assert(begin_count==2&&end_count==2&&commit_count==1);reset();link_state=WIFI_LINK_UP;wifi_time_ip=true;wifi_time_update();assert(wifi_time_cancel()&&!service_depth&&end_count==1);wifi_time_update();assert(begin_count==1);reset();link_state=WIFI_LINK_UP;wifi_time_ip=true;wifi_time_update();close_retained=true;assert(!wifi_time_cancel()&&terminal&&service_depth==1&&!end_count);reset();link_state=WIFI_LINK_UP;wifi_time_ip=true;wifi_time_update();close_pending=true;assert(!wifi_time_cancel()&&!terminal&&service_depth==1&&!end_count);wifi_time_update();assert(!end_count);close_pending=false;wifi_time_update();assert(end_count==1&&!service_depth);puts("PASS Wi-Fi sync valid-IP/once/retry/cancel/lease/terminal policy");return 0;}
