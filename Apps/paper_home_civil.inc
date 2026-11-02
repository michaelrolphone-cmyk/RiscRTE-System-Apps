/* Foreground-only provider policy. No binding is reachable from pure TIMER. */
#include "RiscCivilClockV1.h"
typedef struct {risc_runtime_capability_v1 grant;const risc_civil_clock_policy_v1*api;bool owned;} desk_civil_client;
static int desk_civil_fence(void){desk_fence();return PORTABLE_REALTIME_RETAINED;}
static int desk_civil_result(int rc){
 if(desk_phase_guard(NULL)!=PORTABLE_REALTIME_GUARD_SAFE|| (rc!=0&&rc!=-1))return desk_civil_fence();
 return rc?PORTABLE_REALTIME_UNUSABLE:PORTABLE_REALTIME_OK;
}
static int desk_civil_close(desk_civil_client*c){
 if(desk_phase_guard(NULL)!=PORTABLE_REALTIME_GUARD_SAFE)return desk_civil_fence();
 if(c->owned){
  if(!rt->release(&c->grant)||c->grant.api||c->grant.slot||c->grant.generation)return desk_civil_fence();
  c->owned=false;c->api=NULL;
 }
 return PORTABLE_REALTIME_OK;
}
static int desk_civil_open(desk_civil_client*c){
 *c=(desk_civil_client){.grant={.struct_size=sizeof(c->grant)}};
 if(!desk_promoted||desk_phase_guard(NULL)!=PORTABLE_REALTIME_GUARD_SAFE)return PORTABLE_REALTIME_DENIED;
 if(!rt->acquire(RISC_CIVIL_CLOCK_CAPABILITY,1,0,&c->grant)){
  if(portable_app_sleep_retained()||c->grant.api||c->grant.slot||c->grant.generation)return desk_civil_fence();
  return PORTABLE_REALTIME_UNAVAILABLE;
 }
 c->owned=true;
 if(desk_phase_guard(NULL)!=PORTABLE_REALTIME_GUARD_SAFE)return desk_civil_fence();
 if(c->grant.api){
  const risc_civil_clock_api_v1*base=c->grant.api;
  c->api=risc_civil_clock_get_policy(base,base->struct_size);
 }
 if(!c->api){
  if(desk_civil_close(c)!=PORTABLE_REALTIME_OK)return desk_civil_fence();
  return PORTABLE_REALTIME_UNAVAILABLE;
 }
 return PORTABLE_REALTIME_OK;
}
static int desk_civil_interpret(void*p,const portable_timezone_civil*raw,int64_t*out){
 desk_civil_client*c=p;
 risc_civil_calendar_v1 value={.struct_size=sizeof(value),.year=raw->year,.month=raw->month,
  .day=raw->day,.hour=raw->hour,.minute=raw->minute,.second=raw->second,.weekday=raw->weekday};
 int rc=desk_civil_result(c->api->interpret_rtc(c->api->context,&value,out));
 return desk_retained?PORTABLE_REALTIME_RETAINED:rc;
}
static int desk_civil_project(int64_t epoch,uint32_t ns,risc_civil_calendar_v1*out){
 desk_civil_client client;int rc=desk_civil_open(&client);
 if(rc!=PORTABLE_REALTIME_OK)return rc;
 risc_civil_calendar_v1 value={.struct_size=sizeof(value)};
 rc=desk_civil_result(client.api->project(client.api->context,epoch,ns,&value));
 if(desk_retained)return PORTABLE_REALTIME_RETAINED;
 if(!rc){
  portable_timezone_civil checked={value.year,value.month,value.day,value.hour,value.minute,value.second,value.weekday};
  int64_t ignored;
  if(value.struct_size!=sizeof(value)||value.reserved||value.weekday>6||value.nanoseconds!=ns||
     !memchr(value.zone,0,sizeof(value.zone))||!value.zone[0]||
     portable_timezone_civil_to_epoch(&checked,&ignored)!=PORTABLE_TIMEZONE_OK)rc=PORTABLE_REALTIME_MALFORMED;
 }
 if(desk_civil_close(&client)!=PORTABLE_REALTIME_OK)return PORTABLE_REALTIME_RETAINED;
 if(!rc)*out=value;
 return rc;
}
static int desk_civil_reconcile(portable_realtime_client*native,portable_realtime_recovery_result*out){
 *out=(portable_realtime_recovery_result){0};
 desk_civil_client client;int rc=desk_civil_open(&client);
 if(rc!=PORTABLE_REALTIME_OK)return rc;
 rc=portable_realtime_reconcile_policy(native,0,desk_civil_interpret,&client,&desk_rtc_check,out);
 if(desk_halted(rc)||desk_retained)return desk_civil_fence();
 if(desk_civil_close(&client)!=PORTABLE_REALTIME_OK)return desk_civil_fence();
 return rc;
}

static int desk_civil_cached_rule(char out[40]){
 desk_civil_client client;int rc=desk_civil_open(&client);
 if(rc!=PORTABLE_REALTIME_OK)return rc;
 const risc_civil_clock_retained_v1*retained=risc_civil_clock_get_retained(client.api,client.api->struct_size);
 char token[40]={0};
 if(!retained)rc=PORTABLE_REALTIME_UNAVAILABLE;
 else rc=desk_civil_result(retained->cached_rule(client.api->context,token,sizeof(token)));
 if(desk_retained)return PORTABLE_REALTIME_RETAINED;
 if(!rc){
  bool ended=false,encoding=true;
  for(unsigned i=0;i<sizeof(token);i++){
   unsigned char ch=(unsigned char)token[i];
   if((ended&&ch)||(ch&& (ch<32||ch>126)))encoding=false;
   if(!ch)ended=true;
  }
  portable_timezone_rule checked;
  if(!encoding||!ended||!memchr(token,0,sizeof(token))||portable_timezone_resolve(token,sizeof(token),&checked)!=PORTABLE_TIMEZONE_OK)
   rc=PORTABLE_REALTIME_MALFORMED;
 }
 if(desk_civil_close(&client)!=PORTABLE_REALTIME_OK)return PORTABLE_REALTIME_RETAINED;
 if(!rc)memcpy(out,token,sizeof(token));
 return rc;
}
