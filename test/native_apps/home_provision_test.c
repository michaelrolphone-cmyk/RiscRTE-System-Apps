/* Actual Home adapter, controller, native-time client and network session.
 * Only Runtime/provider/service boundaries are doubled. */
#define DEFERRED_FIXTURE_MAIN unused_provision_deferred_main
#include "deferred_home_radios_test.c"
#include "PortableAutomaticProvision.h"
extern void provision_test_neutral(void);
extern void provision_test_progress(void),provision_test_network(unsigned),provision_test_gate(unsigned);
extern bool provision_test_busy(void),provision_test_utc(uint64_t*),provision_test_background(void);
extern bool provision_test_quick_pending(void),provision_test_alarm_pending(void);
extern unsigned provision_test_phase(void);
extern int provision_test_close(void);
static const char*provision_case;
static unsigned service_acquires,volume_acquires,wifi_session_acquires,settings_acquires;
static unsigned refreshed,scanned,normal_started,time_started,service_steps,cancels,cancel_waits;
static unsigned time_reads,time_releases,service_releases,volume_releases,lease_starts,lease_ends;
static bool lease_live,service_live,volume_live,no_volume,release_uncertain;
static installed_apps_automatic_time_v1 automatic;
static risc_realtime_control_api_v1 selected_time;
static bool pc(const char*n){return !strcmp(provision_case,n);}
static void provisioning_boundary(void){safe();assert(promoted&&!portable_desk_adapter_timer_only());}
static int32_t cache_start(void*c){(void)c;provisioning_boundary();assert(provision_test_busy());refreshed++;return INSTALLED_APPS_OK;}
static int32_t local_scan(void*c,const risc_storage_volume_api_v1*v){(void)c;provisioning_boundary();assert(no_volume?!v:v==&sd.terminal.power.volume.base);scanned++;return INSTALLED_APPS_OK;}
static int32_t auto_cancel(void*c){(void)c;provisioning_boundary();cancels++;if(cancel_waits){cancel_waits--;return INSTALLED_APPS_WORKING;}return INSTALLED_APPS_OK;}
static int32_t auto_begin(void*c,const char*profile,uint64_t utc){(void)c;provisioning_boundary();assert(lease_live&&!strcmp(profile,"core")&&utc==1800000000);normal_started++;return 0;}
static int32_t time_begin(void*c,const char*profile){(void)c;provisioning_boundary();assert(lease_live&&!strcmp(profile,"core"));time_started++;return 0;}
static int32_t auto_step(void*c,const risc_storage_volume_api_v1*v){(void)c;provisioning_boundary();assert(lease_live&&(no_volume?!v:v==&sd.terminal.power.volume.base));service_steps++;return INSTALLED_APPS_WORKING;}
static int32_t profile_supply(void*c,const char*s,const char*w){(void)c;(void)s;(void)w;assert(0);return -1;}
static int32_t feed_start(void*c,uint64_t t){(void)c;(void)t;assert(0);return -1;}
static int32_t feed_step(void*c){(void)c;assert(0);return -1;}
static int32_t install_index(void*c,uint32_t i,uint32_t generation){(void)c;(void)i;(void)generation;assert(0);return -1;}
static int32_t install_descriptor(void*c,const char*n){(void)c;(void)n;assert(0);return -1;}
static int32_t provision_read_time(void*c,risc_realtime_snapshot_v1*out){(void)c;provisioning_boundary();time_reads++;
 if(pc("time-context"))return RISC_REALTIME_CONTEXT;
 if(pc("time-io"))return RISC_REALTIME_IO;
 bool unset=pc("unset")||pc("clock-unset")||!strncmp(provision_case,"time-unset-",11);
 *out=(risc_realtime_snapshot_v1){.struct_size=sizeof(*out),.validity=unset?RISC_REALTIME_UNSET:RISC_REALTIME_VALID,
  .epoch_seconds=unset?0:1800000000,.monotonic_before_us=1000,.monotonic_after_us=1001};
 if(pc("time-size"))out->struct_size--;
 if(pc("time-reserved"))out->reserved=1;
 if(pc("time-bracket"))out->monotonic_after_us=999;
 if(pc("time-nano"))out->nanoseconds=1000000000;
 if(pc("time-fraction"))out->nanoseconds=1;
 if(pc("time-validity"))out->validity=2;
 if(pc("time-range"))out->epoch_seconds=INT64_C(2147483648);
 if(pc("time-unset-epoch"))out->epoch_seconds=1;
 if(pc("time-unset-nano"))out->nanoseconds=1000;
 if(pc("time-zero"))out->epoch_seconds=0;
 return RISC_REALTIME_OK;
}
static int32_t forbidden_seed(void*c,int64_t t,uint32_t n){(void)c;(void)t;(void)n;assert(!"Provisioning must never seed native time");return -1;}
static wifi_link_t session_status(void*c){(void)c;provisioning_boundary();return WIFI_LINK_UP;}
static int8_t session_rssi(void*c){(void)c;provisioning_boundary();return -42;}
static bool session_addresses(void*c,wifi_ipv4_v1*sta,wifi_ipv4_v1*ap){(void)c;provisioning_boundary();*sta=(wifi_ipv4_v1){{192,0,2,1},{0},{0}};*ap=(wifi_ipv4_v1){{0},{0},{0}};return true;}
static int32_t session_begin(void*c,const risc_radio_request_v1*q,uint32_t*id){(void)c;(void)q;(void)id;assert(0);return -1;}
static int32_t session_poll(void*c,uint32_t id,risc_radio_progress_v1*p){(void)c;(void)id;(void)p;assert(0);return -1;}
static int32_t session_cancel(void*c,uint32_t id){(void)c;(void)id;assert(0);return -1;}
static int32_t lease_begin(void*c,uint32_t*out){(void)c;provisioning_boundary();assert(!lease_live);*out=1;lease_live=true;lease_starts++;return RISC_RADIO_ACCEPTED;}
static int32_t lease_end(void*c,uint32_t id){(void)c;provisioning_boundary();assert(lease_live&&id==1&&!cancel_waits);lease_live=false;lease_ends++;return RISC_RADIO_ACCEPTED;}
static const wifi_async_v1 session_wifi={.base={.api_version=1,.struct_size=sizeof(session_wifi),.status=session_status,.rssi=session_rssi,.addresses=session_addresses},
 .async_tag=RISC_RADIO_ASYNC_TAG,.async_version=RISC_RADIO_ASYNC_VERSION,.begin=session_begin,.poll=session_poll,.cancel=session_cancel,.service_begin=lease_begin,.service_end=lease_end};
static int32_t provision_preferences(void*c,const char*k,void*out,uint32_t cap,uint32_t*size){
 int rc=deferred_preferences(c,k,out,cap,size);
 if(clock_run&&!strcmp(k,"quick_radio")&&rc==RISC_KEY_VALUE_OK){uint8_t*b=out;b[2]=PORTABLE_RADIO_WIFI;b[3]=b[2]^0xa5u;}
 return rc;
}
static bool cold_unset_rtc(void*c,twatch_rtc_time_v1*out){(void)c;safe();assert(rtc_live);rtc_reads++;*out=(twatch_rtc_time_v1){0};return true;}
static bool provision_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){
 const void*api=NULL;
 if(!strcmp(n,INSTALLED_APPS_CAPABILITY)){provisioning_boundary();assert(v==1&&!id&&!service_live);service_acquires++;service_live=true;api=&automatic;if(clock_run)assert(home_markers&&home_target&&completed_frame>=home_target);}
 else if(!strcmp(n,"storage.volume")&&!id){provisioning_boundary();assert(v==1&&service_live);volume_acquires++;
  if(pc("volume-partial")){g->slot=99;return false;}if(no_volume)return false;volume_live=true;api=&sd;
 }else if(!strcmp(n,RISC_REALTIME_CONTROL_CAPABILITY)){provisioning_boundary();assert(v==1&&!id);api=&selected_time;}
 else if(!strcmp(n,"net.wifi")&&provision_test_busy()){provisioning_boundary();assert(v==1&&id==15&&scanned);wifi_session_acquires++;api=&session_wifi;}
 else if(!strcmp(n,RISC_KEY_VALUE_CAPABILITY)&&id==6){provisioning_boundary();assert(v==1);settings_acquires++;api=&deferred_kv;}
 else if(clock_run&&!strcmp(n,"rtc.clock")){
  if(!deferred_acquire(n,v,id,g))return false;
  static twatch_rtc_api_v1 unset_rtc;unset_rtc=*(const twatch_rtc_api_v1*)g->api;unset_rtc.read=cold_unset_rtc;g->api=&unset_rtc;return true;
 }else return deferred_acquire(n,v,id,g);
 g->api=api;g->slot=++grants;g->generation=1;return true;
}
static bool provision_drop(risc_runtime_capability_v1*g){
 if(g->api==&selected_time){provisioning_boundary();if(pc("time-release"))return false;time_releases++;}
 if(g->api==&automatic){provisioning_boundary();assert(!lease_live&&!volume_live);if(release_uncertain)return false;service_live=false;service_releases++;}
 if(g->api==&sd){provisioning_boundary();assert(!lease_live);volume_live=false;volume_releases++;}
 if(g->api==&session_wifi)assert(!lease_live);
 return deferred_drop(g);
}
static void controller_setup(void){
 clock_run=pc("clock-valid")||pc("clock-unset");raw_async=clock_run;
 deferred_case=clock_run?"clock-async":"provision";force_allowed=!clock_run;configure();
 deferred_kv.get=provision_preferences;
 automatic=(installed_apps_automatic_time_v1){
  .base={.base={.base={.base={.base={.api_version=1,.struct_size=sizeof(automatic),.refresh=cache_start,.generation=installed_zero,.count=installed_zero,.get=installed_get,.step=local_scan,.cancel=auto_cancel},
     .provision_tag=INSTALLED_APPS_PROVISION_TAG,.provision_version=1,.install_descriptor=install_descriptor},
    .feed_tag=INSTALLED_APPS_FEED_TAG,.feed_version=1,.refresh=feed_start,.step=feed_step,.cancel=auto_cancel,.generation=installed_zero,.count=installed_zero,.get=installed_get,.install=install_index},
   .automatic_tag=INSTALLED_APPS_AUTOMATIC_TAG,.automatic_version=1,.begin=auto_begin,.step=auto_step,.cancel=auto_cancel},
  .boot_profile_tag=INSTALLED_APPS_BOOT_PROFILE_TAG,.boot_profile_version=1,.supply_wifi=profile_supply},
  .automatic_time_tag=INSTALLED_APPS_AUTOMATIC_TIME_TAG,.automatic_time_version=1,.begin_with_time=time_begin};
 selected_time=(risc_realtime_control_api_v1){1,sizeof(selected_time),&native_context,provision_read_time,forbidden_seed};
 runtime.acquire=provision_acquire;runtime.release=provision_drop;
 if(clock_run)return;
 promoted=promotion_attempted=true;
 assert(portable_desk_adapter_start(pc("timer")?PORTABLE_DESK_START_TIMER:PORTABLE_DESK_START_FOREGROUND)==1);
 if(!pc("timer"))provision_test_neutral();
}
static void start_work(void){
 portable_desk_adapter_home_submitted();assert(deferred_test_ready(false));
 provision_test_network(pc("airplane")?PORTABLE_RADIO_AIRPLANE:pc("wifi-off")?0:PORTABLE_RADIO_WIFI);
}
static void retained_check(void){
 assert(terminal&&barriers==1&&portable_app_sleep_retained());unsigned held=grants,called=cancels;
 assert(provision_test_close()==(service_acquires?INSTALLED_APPS_RETAINED:INSTALLED_APPS_OK));app_module_fini();
 assert(grants==held&&cancels==called&&barriers==1);
}
int main(int argc,char**argv){assert(argc==2);provision_case=argv[1];no_volume=pc("no-volume");controller_setup();
 if(clock_run){
  int escaped=setjmp(entry);if(!escaped){app_main();assert(!"Clock must reach settled Home provisioning");}
  assert(escaped==2&&after_load_polls==25&&service_acquires==1&&scanned==1&&service_steps&&lease_live);
  assert(pc("clock-unset")?(time_started==1&&!normal_started):(normal_started==1&&!time_started));
  assert(!seed_calls&&rtc_reads&&home_markers&&completed_frame>=home_target);
  assert(provision_test_close()==INSTALLED_APPS_OK);app_module_fini();
  assert(!grants&&!subs&&!frames&&!native_live&&!terminal);
  printf("Home %s: real cold Clock, completed frame, unchanged native/RTC time and provisioning PASS\n",provision_case);return 0;
 }
 if(pc("timer")){
  for(unsigned i=0;i<20;i++){portable_desk_adapter_home_submitted();provision_test_progress();}
  assert(!service_acquires&&!volume_acquires&&!wifi_session_acquires&&!time_reads&&!provision_test_busy());
 }else{
  if(pc("gates")){
   for(unsigned bit=1;bit<=1024;bit<<=1){provision_test_gate(bit);provision_test_progress();assert(!service_acquires);}
   provision_test_gate(0); /* Fixture state only; restore before real radio load. */
  }
  start_work();
  if(!strncmp(provision_case,"time-",5)){
   uint64_t value=123;assert(!provision_test_utc(&value)&&value==123);
   assert(time_reads==1&&!seed_calls&&!rtc_reads&&!service_acquires&&!wifi_session_acquires);
   if(pc("time-context")||pc("time-release")){retained_check();puts("Home native-time uncertainty pins all custody PASS");return 0;}
   assert(time_releases==1);
  }else{
   for(unsigned i=0;i<12&&!terminal;i++)provision_test_progress();
   if(pc("volume-partial")){assert(!wifi_session_acquires&&!scanned);retained_check();puts("Home partial optional volume retains all custody PASS");return 0;}
   assert(refreshed==1&&scanned==1&&service_acquires==1&&volume_acquires==1);
   if(pc("airplane")||pc("wifi-off"))assert(!wifi_session_acquires&&!time_reads&&!service_live&&!provision_test_busy());
   else{
    assert(provision_test_phase()==4&&provision_test_busy()&&lease_live&&service_steps);
    assert(pc("unset")?(time_started==1&&!normal_started):(normal_started==1&&!time_started));
    assert(time_reads==2&&time_releases==2&&!seed_calls&&!rtc_reads);
    unsigned before=broadcast_acquires;assert(provision_test_background());assert(broadcast_acquires==before);
    if(pc("cancel-pending")||pc("quick-pending")||pc("alarm-pending")||pc("fini-pending")){
     cancel_waits=1000;
     if(pc("fini-pending")){app_module_fini();assert(!grants&&!service_live&&!lease_live&&!subs&&!frames);puts("Home finalization drains 1000 pending cancels before teardown PASS");return 0;}
     if(pc("alarm-pending"))alarm_state.state=ALARM_STATE_CUE;
     for(unsigned i=0;i<1000;i++){
      if(pc("quick-pending"))assert(provision_test_quick_pending());
      else if(pc("alarm-pending"))assert(provision_test_alarm_pending());
      else assert(provision_test_close()==INSTALLED_APPS_WORKING);
      assert(service_live&&lease_live&&!service_releases&&!volume_releases);
     }
     alarm_state.state=ALARM_STATE_READY;
    }
    release_uncertain=pc("retain-release");
    int closed=provision_test_close();
    if(release_uncertain){assert(closed==INSTALLED_APPS_RETAINED);retained_check();puts("Home uncertain release retains dependencies without retry PASS");return 0;}
    assert(closed==INSTALLED_APPS_OK&&!provision_test_busy()&&!lease_live&&!service_live);
    assert(lease_starts==1&&lease_ends==1&&service_releases==1&&volume_releases==(no_volume?0u:1u));
    assert(provision_test_background());assert(broadcast_acquires>before);
   }
  }
 }
 app_module_fini();assert(!grants&&!subs&&!frames&&!native_live&&!terminal);
 printf("Home %s: actual foreground gates, native time, network and cleanup PASS\n",provision_case);return 0;
}
