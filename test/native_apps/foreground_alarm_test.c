/* Production bridge + real tagged client. Only service behavior is modeled. */
#define PORTABLE_ALARM_CLIENT
#define ALARM_SERVICE_TAGGED_V2
#define PORTABLE_NATIVE_CUSTODY_FENCE
#include "PortableAlarmClient.h"
#include <assert.h>
#include <stdio.h>
static bool native_custody_retained,native_sleep_retained;
void portable_adapter_retain(void){native_custody_retained=true;}
void portable_adapter_retain_silent(void){native_custody_retained=true;}
bool portable_adapter_retained(void){return native_custody_retained;}
static portable_alarm_client alarms;
#ifdef PORTABLE_WIFI_SERVICE_LEASE
static unsigned lease_begins,lease_ends;
static int lease_begin_mode,lease_end_mode;
static bool portable_wifi_services_begin(void){assert(!native_custody_retained);lease_begins++;if(lease_begin_mode==2)native_custody_retained=true;return lease_begin_mode==0;}
static bool portable_wifi_services_end(void){assert(!native_custody_retained);lease_ends++;if(lease_end_mode==2)native_custody_retained=true;return lease_end_mode==0;}
#endif
#include "../../lib/PortableApps/src/foreground_alarm.inc"
enum {PREP,STEP,STATUS,RESUME,NOPS};
static unsigned calls[NOPS],pending_count;
static int results[NOPS],retain_op;
static alarm_sleep_v1 issued,resumed;
static alarm_status_v1 copied;
static bool forbidden;
static int call(unsigned op){assert(!forbidden&&!native_custody_retained);calls[op]++;if(retain_op==(int)op)native_custody_retained=true;if(results[op]==99||results[op]==-99||results[op]==ALARM_RETAINED||results[op]==ALARM_OUTPUT)forbidden=true;return results[op];}
static int prepare(void*c,alarm_sleep_v1*t){assert(c==&issued&&t->struct_size==sizeof(*t));int r=call(PREP);if(r)return r;if(pending_count){pending_count--;return ALARM_PENDING;}*t=issued;return ALARM_OK;}
static int step(void*c){assert(c==&issued);return call(STEP);}
static int status(void*c,alarm_status_v1*s){assert(c==&issued&&s->struct_size==sizeof(*s));int r=call(STATUS);*s=copied;return r;}
static int resume(void*c,const alarm_sleep_v1*t){assert(c==&issued);resumed=*t;return call(RESUME);}
static int unused(void*c){(void)c;assert(0);return 0;}
static int unused_ack(void*c,const alarm_token_v1*t){(void)t;return unused(c);}
static alarm_service_descriptor_v2 descriptor;
static void setup(void){
#ifdef PORTABLE_WIFI_SERVICE_LEASE
 lease_begins=lease_ends=0;lease_begin_mode=lease_end_mode=0;
#endif
 native_custody_retained=native_sleep_retained=forbidden=false;retain_op=-1;pending_count=1;
 memset(calls,0,sizeof(calls));memset(results,0,sizeof(results));memset(&resumed,0,sizeof(resumed));
 issued=(alarm_sleep_v1){sizeof(issued),7,1800000000u,1800000010u};
 copied=(alarm_status_v1){.api_version=1,.struct_size=sizeof(copied),.state=ALARM_STATE_READY,.snapshot=7,.rtc_seconds=1800000000u};
 descriptor=(alarm_service_descriptor_v2){.base={2,sizeof(descriptor),&issued,status,step,unused,unused_ack,prepare,unused},.tag=ALARM_SERVICE_DESCRIPTOR_TAG,.descriptor_version=1,.output_modes=ALARM_MODE_VISUAL,.features=ALARM_DESCRIPTOR_RESUME_SLEEP,.resume_sleep=resume};
 memset(&alarms,0,sizeof(alarms));alarms.api=&descriptor.base;alarms.status=copied;alarms.status.snapshot=6;foreground_alarm_ticket_valid=false;
}
static void terminal_tail(void){unsigned before=0;for(unsigned i=0;i<NOPS;i++)before+=calls[i];uint32_t d=0;assert(foreground_alarm_prepare(NULL,&d)==-2);assert(foreground_alarm_resume(NULL)==-2);unsigned after=0;for(unsigned i=0;i<NOPS;i++)after+=calls[i];assert(before==after&&!foreground_alarm_ticket_valid);}
static void ready(void){uint32_t duration=999;assert(foreground_alarm_prepare(NULL,&duration)==1);assert(duration==9000&&foreground_alarm_ticket_valid);}
static void plans(void){
 const uint32_t seconds[]={0,1,2,86400,86401,UINT32_MAX-1800000000u};
 const uint32_t expected[]={0,1,1000,86399000,86400000,86400000};
 for(unsigned i=0;i<6;i++){setup();issued.deadline=seconds[i]?issued.rtc_seconds+seconds[i]:0;uint32_t d=999;assert(foreground_alarm_prepare(NULL,&d)==1&&d==expected[i]);assert(!memcmp(&foreground_alarm_ticket,&issued,sizeof(issued)));assert(foreground_alarm_resume(NULL)==1);assert(calls[RESUME]==1&&!memcmp(&resumed,&issued,sizeof(issued)));assert(foreground_alarm_resume(NULL)==-2);terminal_tail();}
 setup();pending_count=0;alarms.status=copied;uint32_t cached_duration=0;assert(foreground_alarm_prepare(NULL,&cached_duration)==0&&!foreground_alarm_ticket_valid);
 setup();pending_count=3;ready();assert(calls[PREP]==4&&calls[STEP]==3&&calls[STATUS]==3);
 setup();pending_count=1000;uint32_t d=999;assert(foreground_alarm_prepare(NULL,&d)==0&&!d&&!foreground_alarm_ticket_valid);assert(calls[PREP]==64&&calls[STEP]==64&&calls[STATUS]==64&&!native_custody_retained);
 for(unsigned i=0;i<6;i++){setup();if(i==4)issued.snapshot++;if(i==5)issued.rtc_seconds++;if(i==0)issued.struct_size--;if(i==1)issued.snapshot=0;if(i==2)issued.deadline=issued.rtc_seconds;if(i==3)issued.deadline=issued.rtc_seconds-1;assert(foreground_alarm_prepare(NULL,&d)==0&&!foreground_alarm_ticket_valid&&!calls[RESUME]);}
 setup();ready();results[PREP]=ALARM_STALE;assert(foreground_alarm_prepare(NULL,&d)==0&&!foreground_alarm_ticket_valid);assert(foreground_alarm_resume(NULL)==-2&&!calls[RESUME]);
}
static void errors(void){
 const int terminal[]={99,-99,ALARM_RETAINED,ALARM_OUTPUT};
 for(unsigned op=0;op<NOPS;op++)for(unsigned i=0;i<4;i++){
  setup();if(op==RESUME)ready();else if(op!=PREP)pending_count=1;results[op]=terminal[i];uint32_t d=999;
  assert((op==RESUME?foreground_alarm_resume(NULL):foreground_alarm_prepare(NULL,&d))==-2);assert(native_custody_retained&&calls[op]==1);terminal_tail();
 }
 for(unsigned op=0;op<NOPS;op++){
  setup();if(op==RESUME)ready();else if(op!=PREP)pending_count=1;retain_op=(int)op;uint32_t d=0;
  assert((op==RESUME?foreground_alarm_resume(NULL):foreground_alarm_prepare(NULL,&d))==-2);terminal_tail();
 }
 const int clean[]={ALARM_BUSY,ALARM_INVALID,ALARM_STALE,ALARM_STORAGE,ALARM_RTC,ALARM_EXHAUSTED,ALARM_FOREGROUND};
 for(unsigned op=0;op<NOPS;op++)for(unsigned i=0;i<7;i++){
  setup();if(op==RESUME)ready();else if(op!=PREP)pending_count=1;results[op]=clean[i];uint32_t d=0;
  assert((op==RESUME?foreground_alarm_resume(NULL):foreground_alarm_prepare(NULL,&d))==0);assert(!native_custody_retained&&!foreground_alarm_ticket_valid);
 }
 for(unsigned i=0;i<4;i++){setup();pending_count=1;copied.error=terminal[i];uint32_t d=0;assert(foreground_alarm_prepare(NULL,&d)==-2);terminal_tail();}
 setup();native_sleep_retained=true;uint32_t d=0;assert(foreground_alarm_prepare(NULL,&d)==-2&&!calls[PREP]);terminal_tail();
 setup();assert(foreground_alarm_prepare(NULL,NULL)==-2&&!calls[PREP]);terminal_tail();
}
static void status_and_descriptor(void){
 for(unsigned i=0;i<8;i++){setup();pending_count=1;switch(i){case 0:copied.output_uncertain=1;break;case 1:copied.state=ALARM_STATE_BLOCKED;break;case 2:copied.state=ALARM_STATE_CUE;break;case 3:copied.state=ALARM_STATE_ALERT;break;case 4:copied.state=ALARM_STATE_DISMISSING;break;case 5:copied.occurrence.generation=1;break;case 6:copied.api_version=0;break;case 7:copied.struct_size=0;break;}uint32_t d=0;assert(foreground_alarm_prepare(NULL,&d)==(i==0?-2:0));assert(!foreground_alarm_ticket_valid&&!calls[RESUME]);}
 for(unsigned i=0;i<3;i++){setup();if(i==0)descriptor.tag=0;if(i==1){descriptor.resume_sleep=NULL;descriptor.features=0;}if(i==2)alarms.api=NULL;uint32_t d=0;assert(foreground_alarm_prepare(NULL,&d)==-2&&!calls[PREP]);terminal_tail();}
}
static void leases(void){
#ifdef PORTABLE_WIFI_SERVICE_LEASE
 for(int mode=1;mode<=2;mode++){
  setup();lease_begin_mode=mode;uint32_t d=0;assert(foreground_alarm_prepare(NULL,&d)==(mode==1?0:-2));assert(!calls[PREP]&&!lease_ends);
  setup();lease_end_mode=mode;assert(foreground_alarm_prepare(NULL,&d)==-2&&!foreground_alarm_ticket_valid);terminal_tail();
  setup();ready();lease_begin_mode=mode;assert(foreground_alarm_resume(NULL)==-2);assert(!calls[RESUME]);
  setup();ready();lease_end_mode=mode;assert(foreground_alarm_resume(NULL)==-2&&calls[RESUME]==1);terminal_tail();
 }
 setup();pending_count=1;results[STEP]=99;uint32_t d=0;assert(foreground_alarm_prepare(NULL,&d)==-2&&!lease_ends);terminal_tail();
#endif
}
int main(void){plans();errors();status_and_descriptor();leases();puts("PASS foreground alarm bridge: bounded fresh reconciliation, conservative durations, exact once ticket/resume, clean refusal, malformed/due/stale plans, all callback unknown/retained fences");}
