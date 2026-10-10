#include "PortableRealtimeClient.h"
#include "PortableRtcClock.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct {
 int phase,after_phase;unsigned event_count,after_event;
 unsigned acquires,reads,seeds,rtc_acquires,rtc_reads,releases,rtc_releases;
 unsigned generation;bool native_live,rtc_live,fail_native_acquire,fail_rtc_acquire;
 int acquire_shape,release_mode,native_status,seed_status,post_seed_status;
 bool malformed_after_seed;
 bool rtc_read_ok,seed_unset,hidden_retained,retention_on_rtc_failure;
 int fail_release_number;
 const void *native_table,*rtc_table;
 void *context;
 uint64_t instance;
 risc_realtime_snapshot_v1 snapshot;
 twatch_rtc_time_v1 calendar;
 int64_t seeded_epoch;uint32_t seeded_ns;
 char events[128];
} f;
static risc_realtime_api_v1 reader;
static risc_realtime_control_api_v1 control;
static twatch_rtc_api_v1 rtc;
static unsigned cases;
static int phase(void *context){assert(context==&f);return f.phase;}
static void event(char code) {
 assert(f.event_count+1<sizeof(f.events));f.events[f.event_count++]=code;
 if(f.after_event==f.event_count)f.phase=f.after_phase;
}
static int32_t read_native(void *context,risc_realtime_snapshot_v1 *out) {
 assert(out->struct_size==sizeof(*out));++f.reads;event('R');
 if(f.hidden_retained || !f.native_live || context!=f.context)return RISC_REALTIME_CONTEXT;
 if(f.native_status)return f.native_status;
 *out=f.snapshot;return RISC_REALTIME_OK;
}
static int32_t seed_native(void *context,int64_t epoch,uint32_t ns) {
 assert(!f.rtc_live);assert(f.native_live && context==f.context);
 assert(epoch>=0 && epoch<=PORTABLE_REALTIME_MAX_EPOCH && ns==0);
 ++f.seeds;f.seeded_epoch=epoch;f.seeded_ns=ns;event('S');
 if(f.seed_status==RISC_REALTIME_IO){f.snapshot.validity=RISC_REALTIME_UNSET;f.snapshot.epoch_seconds=0;f.snapshot.nanoseconds=0;}
 if(f.seed_status)return f.seed_status;
 if(!f.seed_unset){f.snapshot.validity=RISC_REALTIME_VALID;f.snapshot.epoch_seconds=epoch;f.snapshot.nanoseconds=ns;}
 f.native_status=f.post_seed_status;
 if(f.malformed_after_seed)f.snapshot.reserved=1;
 return RISC_REALTIME_OK;
}
static bool read_rtc(void *context,twatch_rtc_time_v1 *out) {
 assert(f.rtc_live && context==&f);++f.rtc_reads;event('C');
 *out=f.calendar;
 if(!f.rtc_read_ok && f.retention_on_rtc_failure)f.hidden_retained=true;
 return f.rtc_read_ok;
}
static bool acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *g) {
 assert(g->struct_size==sizeof(*g) && !g->slot && !g->generation && !g->api);
 bool ext=!strcmp(name,TWATCH_RTC_CAPABILITY);bool failed;
 if(ext) {
  assert(version==2 && instance==f.instance);assert(!f.rtc_live);
  ++f.rtc_acquires;event('T');failed=f.fail_rtc_acquire;
 } else {
  assert(!strcmp(name,RISC_REALTIME_CAPABILITY)||!strcmp(name,RISC_REALTIME_CONTROL_CAPABILITY));
  assert(version==1 && instance==0 && !f.native_live);++f.acquires;event('A');failed=f.fail_native_acquire;
 }
 if(failed && !f.acquire_shape) {
  /* Production Runtime::acquire clears out first; malformed provider table +
   * failed graph release then sets internal retained_ and returns false.
   * There is no public retained query in the app capability prefix. */
  if(ext && f.retention_on_rtc_failure)f.hidden_retained=true;
  return false;
 }
 g->slot=ext?2:1;g->generation=++f.generation;
 if(ext){g->api=f.rtc_table?f.rtc_table:&rtc;f.rtc_live=true;}
 else {
  f.context=(void *)(uintptr_t)(f.generation+100u);reader.context=control.context=f.context;
  g->api=f.native_table?f.native_table:!strcmp(name,RISC_REALTIME_CONTROL_CAPABILITY)?(void *)&control:(void *)&reader;
  f.native_live=true;
 }
 if(f.acquire_shape==1)g->slot=0;
 if(f.acquire_shape==2)g->generation=0;
 if(f.acquire_shape==3)g->api=0;
 if(f.acquire_shape==4)g->struct_size=0;
 return !failed;
}
static bool release(risc_runtime_capability_v1 *g) {
 assert(g->struct_size==sizeof(*g) && g->generation && g->api);
 assert(!f.hidden_retained); /* No normal release after unreported retention. */
 ++f.releases;
 if(g->slot==2){assert(f.rtc_live);++f.rtc_releases;event('t');}
 else {assert(g->slot==1 && f.native_live && !f.rtc_live);event('a');}
 bool fail=f.fail_release_number==(int)f.releases;
 if(fail) {
  if(f.release_mode==1)*g=(risc_runtime_capability_v1){0};
  return false;
 }
 if(f.release_mode==2)return true; /* Lying/malformed release: not cleared. */
 if(g->slot==2)f.rtc_live=false;else f.native_live=false;
 g->slot=g->generation=0;g->api=0;return true;
}
static risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release};
static void reset(void) {
 memset(&f,0,sizeof(f));f.phase=PORTABLE_REALTIME_GUARD_SAFE;f.after_phase=PORTABLE_REALTIME_GUARD_RETAINED;
 f.rtc_read_ok=true;f.instance=UINT64_C(0xAABB11223344);
 f.snapshot=(risc_realtime_snapshot_v1){sizeof(f.snapshot),RISC_REALTIME_VALID,1767225600,123456000,0,100,150};
 f.calendar=(twatch_rtc_time_v1){2026,1,1,4,0,0,0};
 reader=(risc_realtime_api_v1){1,sizeof(reader),0,read_native};
 control=(risc_realtime_control_api_v1){1,sizeof(control),0,read_native,seed_native};
 rtc=(twatch_rtc_api_v1){.api_version=2,.struct_size=sizeof(rtc),.context=&f,.read=read_rtc};
 ++cases;
}
static void unset(void){f.snapshot.validity=RISC_REALTIME_UNSET;f.snapshot.epoch_seconds=0;f.snapshot.nanoseconds=0;}
static portable_realtime_recovery_policy policy(const char *zone,bool utc) {
 portable_realtime_recovery_policy p={.rtc_instance=f.instance,.basis={utc,0},.fold_choice=-1};
 assert(portable_timezone_parse(zone,strlen(zone)+1,&p.rule)==0);return p;
}
static int open_client(portable_realtime_client *c,int access,int startup) {
 return portable_realtime_open(c,&runtime,access,startup,phase,&f);
}
static void assert_frozen(portable_realtime_client *c,int expected) {
 unsigned before=f.event_count;risc_realtime_snapshot_v1 s;
 assert(portable_realtime_read(c,&s)==expected);
 assert(portable_realtime_close(c)==expected);
 assert(portable_realtime_close(c)==expected);
 f.phase=PORTABLE_REALTIME_GUARD_SAFE;
 assert(open_client(c,1,1)==expected);
 assert(f.event_count==before);
}
static void basic(void) {
 for(int access=0;access<2;++access)for(int startup=0;startup<2;++startup) {
  reset();portable_realtime_client c={0};risc_realtime_snapshot_v1 s;
  assert(open_client(&c,access,startup)==0);assert(f.acquires==1 && !f.reads);
  assert(open_client(&c,access,startup)==PORTABLE_REALTIME_CONTEXT);
  assert(portable_realtime_read(&c,&s)==0 && s.epoch_seconds==f.snapshot.epoch_seconds);
  portable_realtime_recovery_result result;
  assert(portable_realtime_recover_rtc(&c,0,&result)==0);
  assert(!f.rtc_acquires && !f.rtc_reads && !f.seeds);
  assert(portable_realtime_close(&c)==0 && portable_realtime_close(&c)==0 && f.releases==1);
 }
 for(int access=0;access<2;++access)for(int startup=0;startup<2;++startup) {
  reset();unset();portable_realtime_client c={0};assert(open_client(&c,access,startup)==0);
  risc_realtime_snapshot_v1 s;assert(portable_realtime_read(&c,&s)==PORTABLE_REALTIME_UNSET);
  assert(!s.epoch_seconds && !s.nanoseconds);
  portable_realtime_recovery_result r;
  assert(portable_realtime_recover_rtc(&c,0,&r)==(access&&startup?PORTABLE_REALTIME_INVALID:PORTABLE_REALTIME_DENIED));
  assert(!f.rtc_acquires && !f.seeds);assert(portable_realtime_close(&c)==0);
 }
 reset();portable_realtime_client c={0};assert(portable_realtime_close(&c)==0 && !f.event_count);
 assert(open_client(&c,2,0)==PORTABLE_REALTIME_INVALID && !f.event_count);
 assert(open_client(&c,0,2)==PORTABLE_REALTIME_INVALID && !f.event_count);
 assert(portable_realtime_open(&c,&runtime,0,0,0,&f)==PORTABLE_REALTIME_INVALID);
 assert(portable_realtime_read(&c,0)==PORTABLE_REALTIME_INVALID);
 assert(portable_realtime_close(0)==PORTABLE_REALTIME_INVALID);
 for(int mode=0;mode<5;++mode) {
  reset();portable_realtime_client bad={0};risc_runtime_api_v1 rt=runtime;
  if(mode==1)rt.api_version=2;
  if(mode==2)rt.struct_size=8;
  if(mode==3)rt.acquire=0;
  if(mode==4)rt.release=0;
  assert(portable_realtime_open(&bad,mode?&rt:0,0,0,phase,&f)==PORTABLE_REALTIME_UNAVAILABLE);
  assert(!f.event_count && portable_realtime_close(&bad)==0);
 }
}
static void malformed_native(void) {
 for(int access=0;access<2;++access)for(int mode=0;mode<5;++mode) {
  reset();portable_realtime_client c={0};
  uint32_t short_table[2]={1,8};
  if(mode==0){reader.api_version=control.api_version=2;}
  if(mode==1)f.native_table=short_table;
  if(mode==2){reader.read=0;control.read=0;}
  if(mode==3){control.seed=0;if(!access)reader.struct_size=sizeof(reader)-1;}
  if(mode==4){ /* A copied table retains a NULL opaque native context. */
   risc_realtime_control_api_v1 bad=control;risc_realtime_api_v1 bad_read=reader;
   f.native_table=access?(void *)&bad:(void *)&bad_read;
   assert(open_client(&c,access,0)==PORTABLE_REALTIME_MALFORMED);
  } else assert(open_client(&c,access,0)==PORTABLE_REALTIME_MALFORMED);
  assert(f.acquires==1 && f.releases==1 && !f.reads && portable_realtime_close(&c)==0);
 }
 for(int mode=1;mode<=4;++mode)for(int fail=0;fail<2;++fail) {
  reset();portable_realtime_client c={0};f.acquire_shape=mode;f.fail_native_acquire=fail!=0;
  assert(open_client(&c,1,0)==PORTABLE_REALTIME_UNCERTAIN);assert(!f.releases && !f.reads);
  assert_frozen(&c,PORTABLE_REALTIME_UNCERTAIN);
 }
 reset();portable_realtime_client absent={0};f.fail_native_acquire=true;
 assert(open_client(&absent,1,0)==PORTABLE_REALTIME_UNAVAILABLE && !f.releases);
 f.fail_native_acquire=false;assert(open_client(&absent,1,0)==0);assert(portable_realtime_close(&absent)==0);
 for(int mode=0;mode<3;++mode) {
  reset();portable_realtime_client c={0};control.read=0;f.fail_release_number=mode<2?1:0;f.release_mode=mode;
  assert(open_client(&c,1,0)==PORTABLE_REALTIME_UNCERTAIN);assert(f.releases==1);
  assert_frozen(&c,PORTABLE_REALTIME_UNCERTAIN);
 }
}
static void snapshots(void) {
 for(int mode=0;mode<10;++mode) {
  reset();portable_realtime_client c={0};assert(open_client(&c,1,0)==0);
  switch(mode) {
   case 0:f.snapshot.struct_size=0;break;case 1:f.snapshot.validity=2;break;
   case 2:f.snapshot.reserved=1;break;case 3:f.snapshot.monotonic_before_us=151;break;
   case 4:f.snapshot.nanoseconds=1000000000u;break;case 5:f.snapshot.nanoseconds=1;break;
   case 6:f.snapshot.validity=RISC_REALTIME_UNSET;break;
   case 7:unset();f.snapshot.nanoseconds=1000;break;
   case 8:f.snapshot.epoch_seconds=-1;break;case 9:f.snapshot.epoch_seconds=INT64_MAX;break;
  }
  risc_realtime_snapshot_v1 out;memset(&out,0x33,sizeof(out));risc_realtime_snapshot_v1 saved=out;
  assert(portable_realtime_read(&c,&out)==(mode>=8?PORTABLE_REALTIME_RANGE:PORTABLE_REALTIME_MALFORMED));
  assert(!memcmp(&out,&saved,sizeof(out)));assert(portable_realtime_close(&c)==0);
 }
 const int statuses[]={RISC_REALTIME_INVALID,RISC_REALTIME_IO,RISC_REALTIME_CONTEXT,17,-42};
 const int want[]={PORTABLE_REALTIME_INVALID,PORTABLE_REALTIME_IO,PORTABLE_REALTIME_CONTEXT,PORTABLE_REALTIME_UNCERTAIN,PORTABLE_REALTIME_UNCERTAIN};
 for(unsigned i=0;i<sizeof(statuses)/sizeof(statuses[0]);++i) {
  reset();portable_realtime_client c={0};assert(open_client(&c,0,0)==0);f.native_status=statuses[i];
  risc_realtime_snapshot_v1 out;memset(&out,0x22,sizeof(out));risc_realtime_snapshot_v1 saved=out;
  assert(portable_realtime_read(&c,&out)==want[i] && !memcmp(&out,&saved,sizeof(out)));
  if(i<2)assert(portable_realtime_close(&c)==0);else assert_frozen(&c,want[i]);
 }
 reset();portable_realtime_client stale={0};assert(open_client(&stale,0,0)==0);f.context=(void *)(uintptr_t)999;
 risc_realtime_snapshot_v1 s;assert(portable_realtime_read(&stale,&s)==PORTABLE_REALTIME_CONTEXT);
 assert_frozen(&stale,PORTABLE_REALTIME_CONTEXT);
}
static void recovery(void) {
 const char *ny="EST5EDT,M3.2.0,M11.1.0";
 reset();unset();f.instance=0;portable_realtime_client unique={0};
 portable_realtime_recovery_policy unique_policy=policy("UTC0",true);portable_realtime_recovery_result unique_result;
 assert(open_client(&unique,1,1)==0 && portable_realtime_recover_rtc(&unique,&unique_policy,&unique_result)==PORTABLE_REALTIME_RECOVERED);
 assert(portable_realtime_close(&unique)==0);
 for(int utc=0;utc<2;++utc) {
  reset();unset();portable_realtime_client c={0};portable_realtime_recovery_policy p=policy(ny,utc!=0);
  portable_realtime_recovery_result r;assert(open_client(&c,1,1)==0);
  assert(portable_realtime_recover_rtc(&c,&p,&r)==PORTABLE_REALTIME_RECOVERED);
  assert(r.seeded && r.interpreted && r.cleanup==0 && f.seeded_epoch==INT64_C(1767225600)+(utc?0:18000));
  assert(!strcmp(f.events,"ARTCtSR"));assert(portable_realtime_close(&c)==0 && !f.rtc_live);
 }
 for(int mode=0;mode<7;++mode) {
  reset();unset();portable_realtime_client c={0};portable_realtime_recovery_policy p=policy("UTC0",true);
  if(mode==0)p.fold_choice=-2;
  if(mode==1)p.fold_choice=2;
  if(mode==2)p.basis.reference_epoch=1;
  if(mode==3)p.rule.has_daylight=2;
  if(mode==4)p.rule.standard_offset=86401;
  if(mode==5)p.rule.has_daylight=1;
  assert(open_client(&c,1,1)==0);portable_realtime_recovery_result r;
  assert(portable_realtime_recover_rtc(&c,mode==6?0:&p,&r)==PORTABLE_REALTIME_INVALID);
  assert(!f.rtc_acquires && !f.seeds);assert(portable_realtime_close(&c)==0);
 }
 for(int mode=0;mode<12;++mode) {
  reset();unset();portable_realtime_client c={0};portable_realtime_recovery_policy p=policy("UTC0",true);
  uint32_t short_table[2]={2,8};int expected=PORTABLE_REALTIME_MALFORMED;
  if(mode==0)rtc.api_version=1;
  if(mode==1)f.rtc_table=short_table;
  if(mode==2)rtc.read=0;
  if(mode==3){f.rtc_read_ok=false;expected=PORTABLE_REALTIME_UNCERTAIN;}
  if(mode==4)f.calendar.month=13;
  if(mode==5)f.calendar.second=60;
  if(mode==6)f.calendar.day=32;
  if(mode==7){f.calendar=(twatch_rtc_time_v1){2038,1,19,2,3,14,8};expected=PORTABLE_REALTIME_RANGE;}
  if(mode==8){f.calendar=(twatch_rtc_time_v1){1999,1,1,0,0,0,0};expected=PORTABLE_REALTIME_UNUSABLE;}
  if(mode==9){f.calendar=(twatch_rtc_time_v1){1969,1,1,0,0,0,0};expected=PORTABLE_REALTIME_RANGE;}
  if(mode==10){f.calendar.year=10000;expected=PORTABLE_REALTIME_RANGE;}
  if(mode==11){f.calendar=(twatch_rtc_time_v1){2026,2,29,0,0,0,0};}
  assert(open_client(&c,1,1)==0);portable_realtime_recovery_result r;
  int got=portable_realtime_recover_rtc(&c,&p,&r);
  if(got!=expected)fprintf(stderr,"RTC mode %d: got %d expected %d\n",mode,got,expected);
  assert(got==expected && !f.seeds);
  if(mode==3) {
   assert(!f.rtc_releases && r.reason==PORTABLE_REALTIME_IO && r.cleanup==PORTABLE_REALTIME_UNCERTAIN);
   assert_frozen(&c,PORTABLE_REALTIME_UNCERTAIN);
  } else {
   assert(f.rtc_releases==1 && r.reason==expected && r.cleanup==0);
   assert(portable_realtime_close(&c)==0);
  }
 }
 reset();unset();portable_realtime_client max={0};portable_realtime_recovery_policy p=policy("UTC0",true);
 f.calendar=(twatch_rtc_time_v1){2038,1,19,255,3,14,7};portable_realtime_recovery_result r;
 assert(open_client(&max,1,1)==0 && portable_realtime_recover_rtc(&max,&p,&r)==PORTABLE_REALTIME_RECOVERED);
 assert(f.seeded_epoch==PORTABLE_REALTIME_MAX_EPOCH && portable_realtime_close(&max)==0);
 for(int choice=-1;choice<=1;++choice) {
  reset();unset();portable_realtime_client c={0};p=policy(ny,false);p.fold_choice=choice;
  f.calendar=(twatch_rtc_time_v1){2026,11,1,0,1,30,0};assert(open_client(&c,1,1)==0);
  assert(portable_realtime_recover_rtc(&c,&p,&r)==(choice<0?PORTABLE_REALTIME_FOLD:PORTABLE_REALTIME_RECOVERED));
  if(choice>=0)assert(f.seeded_epoch==INT64_C(1793511000)+choice*3600);
  else assert(!f.seeds && r.interpretation.local_status==PORTABLE_TIMEZONE_FOLD);
  assert(portable_realtime_close(&c)==0);
 }
 reset();unset();portable_realtime_client gap={0};p=policy(ny,false);f.calendar=(twatch_rtc_time_v1){2026,3,8,0,2,30,0};
 assert(open_client(&gap,1,1)==0 && portable_realtime_recover_rtc(&gap,&p,&r)==PORTABLE_REALTIME_GAP && !f.seeds);
 assert(portable_realtime_close(&gap)==0);
 for(int allow=0;allow<2;++allow) {
  reset();unset();portable_realtime_client c={0};p=policy(ny,true);p.basis.reference_epoch=1767243600u;p.allow_basis_change=allow!=0;
  assert(open_client(&c,1,1)==0);
  assert(portable_realtime_recover_rtc(&c,&p,&r)==(allow?PORTABLE_REALTIME_RECOVERED:PORTABLE_REALTIME_BASIS_CHOICE));
  assert(r.interpreted && r.interpretation.mode_changed && !r.interpretation.stores_utc);
  assert(f.seeds==(unsigned)allow && portable_realtime_close(&c)==0);
 }
}
static void recovery_failures(void) {
 for(int mode=0;mode<6;++mode) {
  reset();unset();portable_realtime_client c={0};portable_realtime_recovery_policy p=policy("UTC0",true);
  portable_realtime_recovery_result r;assert(open_client(&c,1,1)==0);
  f.fail_rtc_acquire=true;f.acquire_shape=mode;f.retention_on_rtc_failure=mode==0;
  assert(portable_realtime_recover_rtc(&c,&p,&r)==PORTABLE_REALTIME_UNCERTAIN);
  assert(r.reason==(mode?PORTABLE_REALTIME_UNCERTAIN:PORTABLE_REALTIME_UNAVAILABLE));
  assert(r.cleanup==PORTABLE_REALTIME_UNCERTAIN && !f.rtc_reads && !f.seeds && !f.releases);
  if(!mode)assert(f.hidden_retained && f.phase==PORTABLE_REALTIME_GUARD_SAFE && !c.rtc_grant.api);
  assert_frozen(&c,PORTABLE_REALTIME_UNCERTAIN);
 }
 for(int mode=0;mode<3;++mode) {
  reset();unset();portable_realtime_client c={0};portable_realtime_recovery_policy p=policy("UTC0",true);
  portable_realtime_recovery_result r;assert(open_client(&c,1,1)==0);
  f.fail_release_number=mode<2?1:0;f.release_mode=mode;
  assert(portable_realtime_recover_rtc(&c,&p,&r)==PORTABLE_REALTIME_UNCERTAIN);
  assert(!f.seeds && r.cleanup==PORTABLE_REALTIME_UNCERTAIN && f.releases==1);
  assert_frozen(&c,PORTABLE_REALTIME_UNCERTAIN);
 }
 const int status[]={RISC_REALTIME_INVALID,RISC_REALTIME_IO,RISC_REALTIME_CONTEXT,42};
 const int expected[]={PORTABLE_REALTIME_INVALID,PORTABLE_REALTIME_IO,PORTABLE_REALTIME_CONTEXT,PORTABLE_REALTIME_UNCERTAIN};
 for(unsigned i=0;i<4;++i) {
  reset();unset();portable_realtime_client c={0};portable_realtime_recovery_policy p=policy("UTC0",true);
  f.seed_status=status[i];portable_realtime_recovery_result r;assert(open_client(&c,1,1)==0);
  assert(portable_realtime_recover_rtc(&c,&p,&r)==expected[i] && !r.seeded && f.rtc_releases==1 && f.reads==1);
  if(i<2)assert(portable_realtime_close(&c)==0);else assert_frozen(&c,expected[i]);
 }
 for(int mode=0;mode<3;++mode) {
  reset();unset();portable_realtime_client c={0};portable_realtime_recovery_policy p=policy("UTC0",true);
  f.post_seed_status=mode==0?RISC_REALTIME_IO:mode==1?RISC_REALTIME_CONTEXT:0;f.malformed_after_seed=mode==2;
  portable_realtime_recovery_result r;assert(open_client(&c,1,1)==0);
  int expected=mode==0?PORTABLE_REALTIME_IO:mode==1?PORTABLE_REALTIME_CONTEXT:PORTABLE_REALTIME_MALFORMED;
  assert(portable_realtime_recover_rtc(&c,&p,&r)==expected && r.seeded && f.reads==2 && f.rtc_releases==1);
  if(mode==1)assert_frozen(&c,expected);else assert(portable_realtime_close(&c)==0);
 }
 reset();unset();f.seed_unset=true;portable_realtime_client c={0};portable_realtime_recovery_policy p=policy("UTC0",true);
 portable_realtime_recovery_result r;assert(open_client(&c,1,1)==0);
 assert(portable_realtime_recover_rtc(&c,&p,&r)==PORTABLE_REALTIME_IO && r.seeded && f.reads==2);
 assert(portable_realtime_close(&c)==0);
}
static void rtc_read_custody(void) {
 for(int hidden=0;hidden<2;++hidden) {
  reset();unset();portable_realtime_client c={0};portable_realtime_recovery_policy p=policy("UTC0",true);
  portable_realtime_recovery_result r;f.rtc_read_ok=false;f.retention_on_rtc_failure=hidden!=0;
  assert(open_client(&c,1,1)==0);
  assert(portable_realtime_recover_rtc(&c,&p,&r)==PORTABLE_REALTIME_UNCERTAIN);
  assert(r.reason==PORTABLE_REALTIME_IO && r.cleanup==PORTABLE_REALTIME_UNCERTAIN);
  assert(!f.seeds && !f.releases && f.rtc_reads==1 && !strcmp(f.events,"ARTC"));
  assert(f.hidden_retained==(hidden!=0) && f.phase==PORTABLE_REALTIME_GUARD_SAFE);
  assert(c.rtc_grant.api && c.rtc_grant.generation && c.realtime_grant.api);
  unsigned count=f.event_count;
  assert(portable_realtime_recover_rtc(&c,&p,&r)==PORTABLE_REALTIME_UNCERTAIN);
  assert_frozen(&c,PORTABLE_REALTIME_UNCERTAIN);assert(f.event_count==count);
 }
}
static void guard_boundaries(void) {
 for(int retained=0;retained<2;++retained) {
  int reason=retained?PORTABLE_REALTIME_RETAINED:PORTABLE_REALTIME_CONTEXT;
  for(unsigned stop=0;stop<=8;++stop) {
   reset();unset();portable_realtime_client c={0};portable_realtime_recovery_policy p=policy("UTC0",true);
   f.after_phase=retained?PORTABLE_REALTIME_GUARD_RETAINED:PORTABLE_REALTIME_GUARD_UNCONFIRMED;
   if(!stop)f.phase=f.after_phase;else f.after_event=stop;
   int rc=open_client(&c,1,1);portable_realtime_recovery_result r;
   if(!rc)rc=portable_realtime_recover_rtc(&c,&p,&r);
   if(rc==PORTABLE_REALTIME_RECOVERED)rc=portable_realtime_close(&c);
   assert(rc==reason && f.event_count==stop);assert_frozen(&c,reason);
  }
  reset();portable_realtime_client c={0};assert(open_client(&c,1,0)==0);
  portable_realtime_stop(&c,retained!=0);assert_frozen(&c,reason);
 }
 reset();portable_realtime_client original={0};assert(open_client(&original,1,0)==0);
 portable_realtime_client copy=original;risc_realtime_snapshot_v1 s;unsigned events=f.event_count;
 assert(portable_realtime_read(&copy,&s)==PORTABLE_REALTIME_CONTEXT);
 assert(portable_realtime_close(&copy)==PORTABLE_REALTIME_CONTEXT && f.event_count==events);
 assert(portable_realtime_close(&original)==0);
 reset();portable_realtime_client c={0};assert(open_client(&c,1,0)==0);
 f.phase=99;assert(portable_realtime_close(&c)==PORTABLE_REALTIME_CONTEXT);assert_frozen(&c,PORTABLE_REALTIME_CONTEXT);
}
static void projection(void) {
 reset();portable_realtime_client c={0};risc_realtime_snapshot_v1 sample;
 assert(open_client(&c,1,0)==0 && portable_realtime_read(&c,&sample)==0);
 void *old_context=c.native_context;assert(portable_realtime_close(&c)==0);
 unsigned events=f.event_count;f.phase=PORTABLE_REALTIME_GUARD_UNCONFIRMED; /* Peripheral preparation. */
 portable_realtime_estimate estimate;
 assert(portable_realtime_project(&sample,900000,1000000,&estimate)==0);
 assert(estimate.epoch_seconds==sample.epoch_seconds+1 && estimate.nanoseconds==23456000);
 assert(f.event_count==events && !f.rtc_acquires && !f.seeds);
 assert(portable_realtime_close(&c)==0 && f.event_count==events); /* Already closed is pure. */
 f.phase=PORTABLE_REALTIME_GUARD_SAFE; /* Ordinary refusal restored all holds. */
 assert(open_client(&c,1,0)==0 && old_context!=c.native_context);
 assert(portable_realtime_read(&c,&sample)==0 && portable_realtime_close(&c)==0);
 estimate=(portable_realtime_estimate){123,456};
 assert(portable_realtime_project(&sample,1001,1000,&estimate)==PORTABLE_REALTIME_RANGE && estimate.epoch_seconds==123);
 sample.epoch_seconds=PORTABLE_REALTIME_MAX_EPOCH;sample.nanoseconds=999999000;
 assert(portable_realtime_project(&sample,0,0,&estimate)==0);
 assert(portable_realtime_project(&sample,1,1,&estimate)==PORTABLE_REALTIME_RANGE);
 sample.epoch_seconds=0;sample.nanoseconds=0;
 assert(portable_realtime_project(&sample,UINT32_MAX,UINT32_MAX,&estimate)==0 && estimate.epoch_seconds==4294 && estimate.nanoseconds==967295000);
 sample.validity=RISC_REALTIME_UNSET;
 assert(portable_realtime_project(&sample,1,1,&estimate)==PORTABLE_REALTIME_UNSET);
 assert(portable_realtime_project(0,0,0,&estimate)==PORTABLE_REALTIME_MALFORMED);
 assert(portable_realtime_project(&sample,0,0,0)==PORTABLE_REALTIME_INVALID);
}
int main(void) {
 basic();malformed_native();snapshots();recovery();recovery_failures();rtc_read_custody();guard_boundaries();projection();
 printf("Portable realtime production client: %u scenarios PASS (timer isolation, recovery, ambiguity, bounds, grants, no-I/O stops and projection)\n",cases);
 return 0;
}
