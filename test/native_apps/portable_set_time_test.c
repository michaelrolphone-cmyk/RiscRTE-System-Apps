#include "PortableSetTime.h"
#include "PortableRtcClock.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct {
 int phase,after_phase,native_status,seed_status,post_seed_status,put_status,get_status;
 unsigned count,stop_event,guards,stop_guard,generation;
 unsigned acquires,rtc_reads,rtc_writes,seeds,native_reads,puts,gets,releases;
 bool live[4],hidden,write_ok,read_ok,commit,seed_unset,native_malformed;
 bool write_mismatch,read_malformed,get_mismatch;
 unsigned get_size,fail_acquire,acquire_shape,fail_release,release_shape,rtc_tick,fail_read_number;
 uint64_t rtc_window_us;bool backward_monotonic,rtc_before_sample;
 uint64_t instance;int64_t native_delta_us;
 const void *rtc_table,*kv_table;
 void *native_context;
 twatch_rtc_time_v1 calendar,written;
 risc_realtime_snapshot_v1 snapshot;
 uint8_t bytes[12];char events[128];
} f;
static unsigned cases,vectors;
static risc_realtime_control_api_v1 native;
static twatch_rtc_api_v1 rtc;
static risc_key_value_v1 kv;
static int phase(void *context) {
 assert(context==&f);++f.guards;
 if(f.stop_guard==f.guards)f.phase=f.after_phase;
 return f.phase;
}
static void event(char code) {
 assert(!f.hidden && f.phase==PORTABLE_REALTIME_GUARD_SAFE);
 assert(f.count+1<sizeof(f.events));f.events[f.count++]=code;
 if(f.stop_event==f.count)f.phase=f.after_phase;
}
static int32_t read_native(void *ctx,risc_realtime_snapshot_v1 *out) {
 ++f.native_reads;event('R');
 if(!f.live[1] || ctx!=f.native_context)return RISC_REALTIME_CONTEXT;
 if(f.native_status && (!f.fail_read_number || f.fail_read_number==f.native_reads))return f.native_status;
 assert(out->struct_size==sizeof(*out));*out=f.snapshot;
 if(f.live[2]) {
  out->monotonic_before_us=f.rtc_before_sample?100:100+f.rtc_window_us;
  out->monotonic_after_us=out->monotonic_before_us;
  if(f.backward_monotonic && !f.rtc_before_sample)out->monotonic_before_us=out->monotonic_after_us=99;
  f.rtc_before_sample=false;
 }
 return 0;
}
static int32_t seed_native(void *ctx,int64_t epoch,uint32_t ns) {
 assert(!f.live[2] && !f.live[3]);++f.seeds;event('S');
 if(!f.live[1] || ctx!=f.native_context)return RISC_REALTIME_CONTEXT;
 assert(!ns && epoch>=0 && epoch<=PORTABLE_REALTIME_MAX_EPOCH);
 if(f.seed_status==RISC_REALTIME_IO)f.snapshot=(risc_realtime_snapshot_v1){.struct_size=sizeof(f.snapshot)};
 if(f.seed_status)return f.seed_status;
 f.snapshot=(risc_realtime_snapshot_v1){.struct_size=sizeof(f.snapshot),.validity=RISC_REALTIME_VALID,
   .epoch_seconds=epoch+f.native_delta_us/1000000,.nanoseconds=(uint32_t)(f.native_delta_us%1000000)*1000u,
   .monotonic_before_us=100,.monotonic_after_us=110};
 if(f.seed_unset){f.snapshot.validity=RISC_REALTIME_UNSET;f.snapshot.epoch_seconds=0;f.snapshot.nanoseconds=0;}
 if(f.native_malformed)f.snapshot.reserved=1;
 f.native_status=f.post_seed_status;return 0;
}
static bool write_rtc(void *ctx,const twatch_rtc_time_v1 *in) {
 assert(ctx==&f && f.live[2]);++f.rtc_writes;event('W');f.written=*in;
 if(f.write_ok || f.commit)f.calendar=*in;
 if(f.rtc_tick) {
  portable_timezone_civil civil={f.calendar.year,f.calendar.month,f.calendar.day,f.calendar.hour,
    f.calendar.minute,f.calendar.second,f.calendar.weekday};int64_t epoch;
  assert(!portable_timezone_civil_to_epoch(&civil,&epoch));assert(!portable_timezone_epoch_to_civil(epoch+f.rtc_tick,&civil));
  f.calendar=(twatch_rtc_time_v1){(uint16_t)civil.year,civil.month,civil.day,civil.weekday,civil.hour,civil.minute,civil.second};
 }
 if(f.write_mismatch)f.calendar.second+=2;
 if(!f.write_ok)f.hidden=true;
 return f.write_ok;
}
static bool read_rtc(void *ctx,twatch_rtc_time_v1 *out) {
 assert(ctx==&f && f.live[2]);++f.rtc_reads;event('C');*out=f.calendar;
 if(f.read_malformed)out->month=13;
 if(!f.read_ok)f.hidden=true;
 return f.read_ok;
}
static int32_t put(void *ctx,const char *key,const void *data,uint32_t size) {
 assert(ctx==&f && f.live[3] && !f.live[2]);assert(!strcmp(key,PORTABLE_RTC_BASIS_KEY) && size==12);
 ++f.puts;event('P');if(!f.put_status || f.commit)memcpy(f.bytes,data,size);
 if(f.put_status==RISC_KEY_VALUE_CONTEXT || f.put_status<-5 || f.put_status>0)f.hidden=true;
 return f.put_status;
}
static int32_t get(void *ctx,const char *key,void *data,uint32_t cap,uint32_t *size) {
 assert(ctx==&f && f.live[3] && !f.live[2]);assert(!strcmp(key,PORTABLE_RTC_BASIS_KEY) && cap==12);
 ++f.gets;event('G');
 if(f.get_status){if(f.get_status==RISC_KEY_VALUE_CONTEXT || f.get_status<-5 || f.get_status>0)f.hidden=true;return f.get_status;}
 memcpy(data,f.bytes,12);*size=f.get_size;
 if(f.get_mismatch)((uint8_t *)data)[11]^=1;
 return 0;
}
static bool acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *g) {
 unsigned slot=(!strcmp(name,RISC_REALTIME_CONTROL_CAPABILITY)||!strcmp(name,RISC_REALTIME_CAPABILITY))?1u:!strcmp(name,TWATCH_RTC_CAPABILITY)?2u:3u;
 assert(!f.live[slot] && g->struct_size==sizeof(*g) && !g->slot && !g->generation && !g->api);
 if(slot==1)assert(version==1 && !instance);
 if(slot==2)assert(version==2 && instance==f.instance);
 if(slot==3)assert(!strcmp(name,RISC_KEY_VALUE_CAPABILITY) && version==1 && instance==1);
 ++f.acquires;event(slot==1?'A':slot==2?'T':'K');
 if(slot==f.fail_acquire && !f.acquire_shape){if(slot!=1)f.hidden=true;return false;}
 f.live[slot]=true;g->slot=slot;g->generation=++f.generation;
 if(slot==1){f.native_context=(void *)(uintptr_t)(100+f.generation);native.context=f.native_context;g->api=&native;}
 if(slot==2){g->api=f.rtc_table?f.rtc_table:&rtc;f.rtc_before_sample=true;}
 if(slot==3)g->api=f.kv_table?f.kv_table:&kv;
 if(f.acquire_shape && (slot==f.fail_acquire || !f.fail_acquire)) {
  if(f.acquire_shape==1)g->slot=0;
  if(f.acquire_shape==2)g->generation=0;
  if(f.acquire_shape==3)g->api=0;
  if(f.acquire_shape==4)g->struct_size=0;
 }
 if(slot==f.fail_acquire){if(slot!=1)f.hidden=true;return false;}
 return true;
}
static bool release(risc_runtime_capability_v1 *g) {
 assert(g->slot>0 && g->slot<4 && f.live[g->slot]);++f.releases;
 event(g->slot==1?'a':g->slot==2?'t':'k');
 if(f.fail_release==f.releases) {
  f.hidden=true;
  if(f.release_shape==1)*g=(risc_runtime_capability_v1){0};
  return false;
 }
 if(f.release_shape==2){f.hidden=true;return true;}
 f.live[g->slot]=false;g->slot=g->generation=0;g->api=0;return true;
}
static risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release};
static void reset(void) {
 memset(&f,0,sizeof(f));f.phase=PORTABLE_REALTIME_GUARD_SAFE;f.after_phase=PORTABLE_REALTIME_GUARD_RETAINED;
 f.write_ok=f.read_ok=true;f.get_size=12;f.rtc_window_us=250000;
 f.snapshot=(risc_realtime_snapshot_v1){.struct_size=sizeof(f.snapshot)};f.instance=UINT64_C(0x1234ABCDEF);
 native=(risc_realtime_control_api_v1){1,sizeof(native),0,read_native,seed_native};
 rtc=(twatch_rtc_api_v1){.api_version=2,.struct_size=sizeof(rtc),.context=&f,.read=read_rtc,.write=write_rtc};
 kv=(risc_key_value_v1){1,sizeof(kv),&f,get,put};++cases;
}
static portable_set_time_request request(const char *zone,bool utc) {
 portable_set_time_request p={.local={2026,1,1,12,34,56,255},.basis={utc,946684800},.fold_choice=-1,
   .rtc_instance=f.instance,.native_readback_budget_us=1000000};
 assert(!portable_timezone_parse(zone,strlen(zone)+1,&p.rule));return p;
}
static int open_client(portable_set_time_client *c){return portable_set_time_open(c,&runtime,phase,&f);}
static void frozen(portable_set_time_client *c,int reason,const portable_set_time_request *p) {
 unsigned count=f.count;portable_set_time_result r;assert(portable_set_time_close(c)==reason);
 f.phase=PORTABLE_REALTIME_GUARD_SAFE;
 assert(portable_set_time_apply_confirmed(c,p,&r)==reason);
 assert(open_client(c)==reason && portable_set_time_close(c)==reason && f.count==count);
}
static void basics(void) {
 for(int utc=0;utc<2;++utc) {
  reset();portable_set_time_client c={0};portable_set_time_request p=request("EST5EDT,M3.2.0,M11.1.0",utc!=0);
  portable_set_time_plan plan;assert(!portable_set_time_prepare(&p,&plan));
  assert(plan.utc_epoch==INT64_C(1767288896) && plan.basis.stores_utc==(utc!=0));
  assert(plan.rtc_calendar.hour==(utc?17:12) && plan.rtc_calendar.weekday==4 && !f.count);
  assert(!open_client(&c) && !f.rtc_reads && !f.rtc_writes && !f.seeds && !f.puts);
  portable_set_time_result r;assert(!portable_set_time_apply_confirmed(&c,&p,&r));
  assert(r.stage==PORTABLE_SET_TIME_DONE && r.planned && r.reason==0 && r.cleanup==0);
  assert(r.rtc_outcome==2 && r.native.attempted && r.native.seeded && r.native.verified && r.metadata_outcome==2);
  assert(r.rtc_write_accepted && r.metadata_write_accepted);
  assert(f.written.hour==(utc?17:12) && f.written.weekday==4);
  assert(f.bytes[3]==(unsigned)utc && portable_rtc_basis_word(f.bytes+4)==1767288896u);
  assert(!strcmp(f.events,"ATRWCRtSRKPGk"));assert(!portable_set_time_close(&c));
  assert(!strcmp(f.events,"ATRWCRtSRKPGka") && !portable_set_time_close(&c));
  void *old=c.realtime.native_context;assert(!open_client(&c) && c.realtime.native_context!=old);
  assert(!portable_set_time_close(&c));
 }
 reset();portable_set_time_result r;portable_set_time_request p=request("UTC0",true);portable_set_time_client c={0};
 assert(portable_set_time_apply_confirmed(0,&p,&r)==PORTABLE_REALTIME_CONTEXT);
 assert(portable_set_time_apply_confirmed(&c,&p,&r)==PORTABLE_REALTIME_CONTEXT);
 assert(portable_set_time_apply_confirmed(&c,&p,0)==PORTABLE_REALTIME_INVALID);
 assert(portable_set_time_close(0)==PORTABLE_REALTIME_INVALID);
 assert(portable_set_time_open(0,&runtime,phase,&f)==PORTABLE_REALTIME_INVALID);
 assert(portable_set_time_open(&c,&runtime,0,&f)==PORTABLE_REALTIME_INVALID);
 assert(!portable_set_time_close(&c) && !f.count);
 assert(!open_client(&c));assert(open_client(&c)==PORTABLE_REALTIME_CONTEXT);
 portable_set_time_client copy=c;unsigned before=f.count;
 assert(portable_set_time_apply_confirmed(&copy,&p,&r)==PORTABLE_REALTIME_CONTEXT);
 assert(portable_set_time_close(&copy)==PORTABLE_REALTIME_CONTEXT && f.count==before);
 assert(!portable_set_time_close(&c));
}
static void plan_boundaries(void) {
 reset();portable_set_time_request p=request("UTC0",true);portable_set_time_plan out={.utc_epoch=7};
 assert(portable_set_time_prepare(0,&out)==PORTABLE_REALTIME_INVALID);
 assert(portable_set_time_prepare(&p,0)==PORTABLE_REALTIME_INVALID);
 for(int mode=0;mode<12;++mode) {
  p=request("UTC0",true);int want=PORTABLE_REALTIME_INVALID;
  switch(mode) {
   case 0:p.fold_choice=-2;break;case 1:p.fold_choice=2;break;case 2:p.local.month=0;break;
   case 3:p.local.day=32;break;case 4:p.local.second=60;break;case 5:p.local.hour=24;break;
   case 6:p.local=(portable_timezone_civil){2026,2,29,0,0,0,0};break;
   case 7:p.rule.has_daylight=2;break;case 8:p.basis.reference_epoch=1;break;
   case 9:p.native_readback_budget_us=5000001;want=PORTABLE_REALTIME_RANGE;break;
   case 10:p.local.year=2100;want=PORTABLE_REALTIME_RANGE;break;
   case 11:p.local.year=1999;want=PORTABLE_REALTIME_RANGE;break;
  }
  out.utc_epoch=7;assert(portable_set_time_prepare(&p,&out)==want && out.utc_epoch==7);
 }
 const portable_timezone_civil dates[]={{1999,12,31,23,59,59,0},{2000,1,1,0,0,0,0},
  {2000,2,29,12,0,0,0},{2038,1,19,3,14,7,0},{2038,1,19,3,14,8,0},{2099,12,31,23,59,59,0},{2100,1,1,0,0,0,0}};
 const int wants[]={-11,0,0,0,-11,-11,-11};
 for(unsigned i=0;i<sizeof(dates)/sizeof(dates[0]);++i) {
  p=request("UTC0",true);p.local=dates[i];assert(portable_set_time_prepare(&p,&out)==wants[i]);
 }
 p=request("EST5",false);p.local=(portable_timezone_civil){1999,12,31,19,0,0,0};
 assert(portable_set_time_prepare(&p,&out)==PORTABLE_REALTIME_RANGE); /* RTC local calendar <2000. */
 p.basis.stores_utc=true;assert(!portable_set_time_prepare(&p,&out) && out.utc_epoch==946684800);
 p=request("<+14>-14",true);p.local=(portable_timezone_civil){2000,1,1,0,0,0,0};
 assert(portable_set_time_prepare(&p,&out)==PORTABLE_REALTIME_RANGE); /* Reference/native instant <2000. */
 p.local=(portable_timezone_civil){2038,1,19,17,14,7,0};assert(!portable_set_time_prepare(&p,&out));
 p.local.second=8;assert(portable_set_time_prepare(&p,&out)==PORTABLE_REALTIME_RANGE);
 /* Invalid plan never acquires providers or writes. */
 portable_set_time_client c={0};portable_set_time_result r;assert(!open_client(&c));
 assert(portable_set_time_apply_confirmed(&c,&p,&r)==PORTABLE_REALTIME_RANGE && f.count==1);
 assert(!portable_set_time_close(&c));
}
static void failures(void) {
 for(unsigned slot=2;slot<=3;++slot)for(unsigned shape=0;shape<=4;++shape) {
  reset();portable_set_time_client c={0};portable_set_time_request p=request("UTC0",false);portable_set_time_result r;
  assert(!open_client(&c));f.fail_acquire=slot;f.acquire_shape=shape;
  assert(portable_set_time_apply_confirmed(&c,&p,&r)==PORTABLE_REALTIME_UNCERTAIN);
  assert(r.cleanup==PORTABLE_REALTIME_UNCERTAIN && f.puts==0);
  if(slot==2)assert(!f.rtc_writes && !f.seeds && !f.releases);
  else assert(r.rtc_outcome==2 && r.native.verified && f.releases==1);
  frozen(&c,PORTABLE_REALTIME_UNCERTAIN,&p);
 }
 for(int kind=0;kind<2;++kind)for(int mode=0;mode<4;++mode) {
  reset();portable_set_time_client c={0};portable_set_time_request p=request("UTC0",false);portable_set_time_result r;
  uint32_t short_table[2]={kind?1:2,8};
  if(kind){if(mode==0)f.kv_table=short_table;if(mode==1)kv.api_version=2;if(mode==2)kv.get=0;if(mode==3)kv.put=0;}
  else {if(mode==0)f.rtc_table=short_table;if(mode==1)rtc.api_version=1;if(mode==2)rtc.read=0;if(mode==3)rtc.write=0;}
  assert(!open_client(&c));assert(portable_set_time_apply_confirmed(&c,&p,&r)==PORTABLE_REALTIME_UNCERTAIN);
  assert(r.reason==PORTABLE_REALTIME_MALFORMED && !f.puts);frozen(&c,PORTABLE_REALTIME_UNCERTAIN,&p);
 }
 for(int mode=0;mode<4;++mode) {
  reset();portable_set_time_client c={0};portable_set_time_request p=request("UTC0",false);portable_set_time_result r;
  f.write_ok=mode>=2;f.read_ok=mode<2;f.commit=(mode%2)!=0;
  assert(!open_client(&c));assert(portable_set_time_apply_confirmed(&c,&p,&r)==PORTABLE_REALTIME_UNCERTAIN);
  assert(r.rtc_outcome==1 && r.reason==PORTABLE_REALTIME_IO && !f.seeds && !f.puts && !f.releases);
  assert(f.rtc_reads==(unsigned)(mode>=2));frozen(&c,PORTABLE_REALTIME_UNCERTAIN,&p);
 }
 for(unsigned release_at=1;release_at<=3;++release_at)for(unsigned mode=0;mode<2;++mode) {
  reset();portable_set_time_client c={0};portable_set_time_request p=request("UTC0",false);portable_set_time_result r;
  f.fail_release=release_at;f.release_shape=mode;assert(!open_client(&c));
  int rc=portable_set_time_apply_confirmed(&c,&p,&r);
  if(release_at==3){assert(!rc);rc=portable_set_time_close(&c);}
  assert(rc==PORTABLE_REALTIME_UNCERTAIN && f.releases==release_at);
  const risc_runtime_capability_v1 *kept=release_at==1?&c.rtc_grant:release_at==2?&c.kv_grant:&c.realtime.realtime_grant;
  assert(kept->struct_size==sizeof(*kept) && kept->api && kept->slot && kept->generation);
  if(release_at==1)assert(r.rtc_outcome==2 && !f.seeds && !f.puts);
  else assert(r.native.verified && r.metadata_outcome==2);
  frozen(&c,PORTABLE_REALTIME_UNCERTAIN,&p);
 }
 reset();portable_set_time_client lie={0};portable_set_time_request p=request("UTC0",false);portable_set_time_result r;
 f.release_shape=2;assert(!open_client(&lie));
 assert(portable_set_time_apply_confirmed(&lie,&p,&r)==PORTABLE_REALTIME_UNCERTAIN);frozen(&lie,-6,&p);
 const int statuses[]={RISC_REALTIME_INVALID,RISC_REALTIME_IO,RISC_REALTIME_CONTEXT,88};
 const int wants[]={PORTABLE_REALTIME_INVALID,PORTABLE_REALTIME_IO,PORTABLE_REALTIME_CONTEXT,PORTABLE_REALTIME_UNCERTAIN};
 for(int read=0;read<2;++read)for(unsigned i=0;i<4;++i) {
  reset();portable_set_time_client c={0};p=request("UTC0",true);
  if(read)f.post_seed_status=statuses[i];else f.seed_status=statuses[i];
  assert(!open_client(&c));assert(portable_set_time_apply_confirmed(&c,&p,&r)==wants[i]);
  assert(r.rtc_outcome==2 && r.native.attempted && r.native.seeded==(read!=0) && !r.native.verified && !f.puts);
  if(i>=2)frozen(&c,wants[i],&p);else assert(!portable_set_time_close(&c));
 }
 const int kv_statuses[]={RISC_KEY_VALUE_NOT_FOUND,RISC_KEY_VALUE_BUFFER_SMALL,RISC_KEY_VALUE_INVALID,RISC_KEY_VALUE_CONTEXT,RISC_KEY_VALUE_IO,99};
 for(int read=0;read<2;++read)for(unsigned i=0;i<sizeof(kv_statuses)/sizeof(*kv_statuses);++i)for(int commit=0;commit<2;++commit) {
  int status=kv_statuses[i];bool halted=status==RISC_KEY_VALUE_CONTEXT || status==99;
  reset();portable_set_time_client c={0};p=request("UTC0",true);f.commit=commit!=0;
  if(read)f.get_status=status;else f.put_status=status;
  assert(!open_client(&c));int want=status==RISC_KEY_VALUE_CONTEXT?PORTABLE_REALTIME_CONTEXT:
    status==99?PORTABLE_REALTIME_UNCERTAIN:status==RISC_KEY_VALUE_IO?PORTABLE_REALTIME_IO:
    status==RISC_KEY_VALUE_INVALID?PORTABLE_REALTIME_INVALID:PORTABLE_REALTIME_VERIFY;
  assert(portable_set_time_apply_confirmed(&c,&p,&r)==want);
  assert(r.rtc_outcome==2 && r.native.verified && r.metadata_outcome==1 && f.releases==(halted?1u:2u));
  assert(f.gets==(unsigned)read && r.metadata_write_accepted==(read!=0));
  if(halted){frozen(&c,want,&p);continue;}
  assert(r.reason==want && !r.cleanup && !f.live[3] && f.live[1]);
  /* No inferred rollback: IO after commit may already hold the exact bytes. */
  if(commit || read)assert(f.bytes[0]==0x52 && f.bytes[1]==0x54);
  f.put_status=f.get_status=0;
  /* A caller's next explicit action can acquire fresh RTC/KV and verify. */
  assert(!portable_set_time_apply_confirmed(&c,&p,&r));
  assert(r.rtc_outcome==2 && r.native.verified && r.metadata_outcome==2);
  assert(f.puts==2 && f.gets==(unsigned)read+1u);
  assert(!portable_set_time_close(&c));
 }
 /* A typed IO never overrides an uncertain checked release. Preserve original
  * grant evidence even when a failed callback mutates its temporary argument. */
 for(int read=0;read<2;++read)for(unsigned shape=0;shape<2;++shape) {
  reset();portable_set_time_client c={0};p=request("UTC0",true);
  if(read)f.get_status=RISC_KEY_VALUE_IO;else f.put_status=RISC_KEY_VALUE_IO;
  f.fail_release=2;f.release_shape=shape;
  assert(!open_client(&c));assert(portable_set_time_apply_confirmed(&c,&p,&r)==PORTABLE_REALTIME_UNCERTAIN);
  assert(r.reason==PORTABLE_REALTIME_IO && r.cleanup==PORTABLE_REALTIME_UNCERTAIN);
  assert(r.metadata_outcome==1 && r.native.verified && c.kv_grant.slot==3);
  frozen(&c,PORTABLE_REALTIME_UNCERTAIN,&p);
 }
}
static void retries(void) {
 for(int mode=0;mode<8;++mode) {
  reset();portable_set_time_client c={0};portable_set_time_request p=request("UTC0",true);portable_set_time_result r;
  int want=PORTABLE_REALTIME_VERIFY;
  if(mode==0)f.write_mismatch=true;
  if(mode==1)f.read_malformed=true;
  if(mode==2){f.seed_status=RISC_REALTIME_IO;want=PORTABLE_REALTIME_IO;}
  if(mode==3){f.seed_unset=true;want=PORTABLE_REALTIME_IO;}
  if(mode==4){f.native_malformed=true;want=PORTABLE_REALTIME_MALFORMED;}
  if(mode==5)f.native_delta_us=1000001;
  if(mode==6)f.get_mismatch=true;
  if(mode==7)f.get_size=11;
  assert(!open_client(&c));assert(portable_set_time_apply_confirmed(&c,&p,&r)==want);
  assert(r.rtc_outcome==(mode<2?1:2) && r.metadata_outcome==(mode>=6?1:0));
  unsigned writes=f.rtc_writes;assert(writes==1); /* No implicit retry. */
  f.write_mismatch=f.read_malformed=f.seed_unset=f.native_malformed=f.get_mismatch=false;
  f.seed_status=f.native_status=0;f.native_delta_us=0;f.get_size=12;f.snapshot.reserved=0;
  assert(!portable_set_time_apply_confirmed(&c,&p,&r) && f.rtc_writes==writes+1);
  assert(r.rtc_outcome==2 && r.native.verified && r.metadata_outcome==2);
  assert(!portable_set_time_close(&c));
 }
 reset();portable_set_time_client c={0};portable_set_time_request p=request("UTC0",true);portable_set_time_result r;
 assert(!open_client(&c));f.native_context=(void *)(uintptr_t)9999;
 assert(portable_set_time_apply_confirmed(&c,&p,&r)==PORTABLE_REALTIME_CONTEXT);
 assert(r.rtc_outcome==0 && !r.native.attempted && !f.puts);frozen(&c,PORTABLE_REALTIME_CONTEXT,&p);
}
static void tick_windows(void) {
 const uint64_t windows[]={0,250000,250001,UINT64_MAX-100};
 for(unsigned w=0;w<4;++w)for(unsigned tick=0;tick<3;++tick)for(int utc=0;utc<2;++utc) {
  reset();portable_set_time_client c={0};portable_set_time_request p=request("EST5EDT,M3.2.0,M11.1.0",utc!=0);
  portable_set_time_result r;f.rtc_window_us=windows[w];f.rtc_tick=tick;
  assert(!open_client(&c));bool succeeds=w<2 && tick<2;
  assert(portable_set_time_apply_confirmed(&c,&p,&r)==(succeeds?0:PORTABLE_REALTIME_VERIFY));
  assert(r.rtc_window_us==windows[w] && r.rtc_window_confirmed==(w<2));
  if(succeeds) {
   assert(r.rtc_outcome==2 && r.rtc_tick_accepted==(tick==1));
   assert(r.verified_utc_epoch==r.plan.utc_epoch+tick);
   assert(r.native.snapshot.epoch_seconds==r.verified_utc_epoch);
   assert(portable_rtc_basis_word(f.bytes+4)==r.verified_utc_epoch);
  } else assert(r.rtc_outcome==1 && !f.seeds && !f.puts);
  assert(!portable_set_time_close(&c));
 }
 const portable_timezone_civil dates[]={
  {2024,2,28,23,59,59,0},{2024,2,29,23,59,59,0},{2025,12,31,23,59,59,0},
  {2026,3,8,1,59,59,0},{2026,11,1,1,59,59,0},{2026,11,1,1,59,59,0}};
 for(unsigned i=0;i<6;++i)for(int utc=0;utc<2;++utc) {
  reset();portable_set_time_client c={0};portable_set_time_request p=request("EST5EDT,M3.2.0,M11.1.0",utc!=0);
  portable_set_time_result r;p.local=dates[i];p.fold_choice=i==5?1:0;f.rtc_tick=1;
  assert(!open_client(&c));bool succeeds=utc || (i!=3 && i!=4);
  assert(portable_set_time_apply_confirmed(&c,&p,&r)==(succeeds?0:PORTABLE_REALTIME_VERIFY));
  if(succeeds)assert(r.rtc_tick_accepted && r.verified_utc_epoch==r.plan.utc_epoch+1);
  else assert(!f.seeds && !f.puts && r.rtc_outcome==1);
  assert(!portable_set_time_close(&c));
 }
 for(unsigned tick=0;tick<2;++tick) {
  reset();portable_set_time_client c={0};portable_set_time_request p=request("UTC0",true);portable_set_time_result r;
  p.local=(portable_timezone_civil){2038,1,19,3,14,7,0};f.rtc_tick=tick;assert(!open_client(&c));
  assert(portable_set_time_apply_confirmed(&c,&p,&r)==(tick?PORTABLE_REALTIME_RANGE:0));
  assert(!portable_set_time_close(&c));
 }
 reset();portable_set_time_client c={0};portable_set_time_request p=request("UTC0",true);portable_set_time_result r;
 f.backward_monotonic=true;assert(!open_client(&c));
 assert(portable_set_time_apply_confirmed(&c,&p,&r)==PORTABLE_REALTIME_VERIFY && !f.seeds);
 assert(!r.rtc_window_confirmed && !portable_set_time_close(&c));
}
static void bracket_failures(void) {
 const int statuses[]={RISC_REALTIME_INVALID,RISC_REALTIME_IO,RISC_REALTIME_CONTEXT,88};
 const int wants[]={PORTABLE_REALTIME_INVALID,PORTABLE_REALTIME_IO,PORTABLE_REALTIME_CONTEXT,PORTABLE_REALTIME_UNCERTAIN};
 for(unsigned n=1;n<=2;++n)for(unsigned i=0;i<4;++i) {
  reset();portable_set_time_client c={0};portable_set_time_request p=request("UTC0",true);portable_set_time_result r;
  f.native_status=statuses[i];f.fail_read_number=n;assert(!open_client(&c));
  assert(portable_set_time_apply_confirmed(&c,&p,&r)==wants[i]);
  assert(r.rtc_outcome==(n==1?0:1) && !f.seeds && !f.puts);
  if(i>=2) {assert(f.releases==0);frozen(&c,wants[i],&p);}
  else {assert(f.releases==1);assert(!portable_set_time_close(&c));}
 }
}
static void boundaries(void) {
 for(int phase_kind=0;phase_kind<3;++phase_kind)for(unsigned stop=0;stop<=14;++stop) {
  reset();portable_set_time_client c={0};portable_set_time_request p=request("UTC0",true);portable_set_time_result r;
  f.after_phase=phase_kind==0?PORTABLE_REALTIME_GUARD_RETAINED:phase_kind==1?PORTABLE_REALTIME_GUARD_UNCONFIRMED:99;
  if(stop)f.stop_event=stop;else f.phase=f.after_phase;
  int rc=open_client(&c);if(!rc)rc=portable_set_time_apply_confirmed(&c,&p,&r);
  if(!rc)rc=portable_set_time_close(&c);
  int want=phase_kind==0?PORTABLE_REALTIME_RETAINED:PORTABLE_REALTIME_CONTEXT;
  assert(rc==want && f.count==stop);frozen(&c,want,&p);
 }
 /* Every BEFORE and AFTER guard call, including pure boundaries. */
 reset();portable_set_time_client base={0};portable_set_time_request p=request("UTC0",true);portable_set_time_result r;
 assert(!open_client(&base) && !portable_set_time_apply_confirmed(&base,&p,&r) && !portable_set_time_close(&base));
 unsigned total=f.guards;
 for(unsigned stop=1;stop<=total;++stop) {
  reset();portable_set_time_client c={0};p=request("UTC0",true);f.stop_guard=stop;
  int rc=open_client(&c);if(!rc)rc=portable_set_time_apply_confirmed(&c,&p,&r);if(!rc)rc=portable_set_time_close(&c);
  assert(rc==PORTABLE_REALTIME_RETAINED);frozen(&c,PORTABLE_REALTIME_RETAINED,&p);
 }
 for(int retained=0;retained<2;++retained) {
  reset();portable_set_time_client c={0};p=request("UTC0",true);assert(!open_client(&c));
  portable_set_time_stop(&c,retained!=0);frozen(&c,retained?PORTABLE_REALTIME_RETAINED:PORTABLE_REALTIME_CONTEXT,&p);
 }
}
static void direct_seed(void) {
 reset();portable_realtime_client original={0};portable_realtime_seed_result result;
 assert(!portable_realtime_open(&original,&runtime,1,1,phase,&f));
 portable_realtime_client copy=original;unsigned count=f.count;
 assert(portable_realtime_seed_confirmed(&copy,1767225600,1000000,&result)==PORTABLE_REALTIME_CONTEXT && f.count==count);
 assert(portable_realtime_seed_confirmed(&original,1767225600,5000001,&result)==PORTABLE_REALTIME_RANGE && f.count==count);
 assert(portable_realtime_seed_confirmed(&original,1767225600,0,0)==PORTABLE_REALTIME_INVALID && f.count==count);
 f.live[1]=false;
 assert(portable_realtime_seed_confirmed(&original,1767225600,0,&result)==PORTABLE_REALTIME_CONTEXT && result.attempted && !result.seeded);
 count=f.count;assert(portable_realtime_seed_confirmed(&original,1767225600,0,&result)==PORTABLE_REALTIME_CONTEXT);
 assert(portable_realtime_close(&original)==PORTABLE_REALTIME_CONTEXT && f.count==count);
 for(int access=0;access<2;++access)for(int startup=0;startup<2;++startup) {
  reset();portable_realtime_client c={0};portable_realtime_seed_result r;
  assert(!portable_realtime_open(&c,&runtime,access,startup,phase,&f));
  assert(portable_realtime_seed_confirmed(&c,1767225600,1000000,&r)==(access&&startup?0:PORTABLE_REALTIME_DENIED));
  assert(!portable_realtime_close(&c));
 }
 const int64_t epochs[]={-1,0,2147483647,2147483648,INT64_MAX};
 for(unsigned i=0;i<sizeof(epochs)/sizeof(epochs[0]);++i) {
  reset();portable_realtime_client c={0};portable_realtime_seed_result r;
  assert(!portable_realtime_open(&c,&runtime,1,1,phase,&f));
  assert(portable_realtime_seed_confirmed(&c,epochs[i],0,&r)==(i==1||i==2?0:PORTABLE_REALTIME_RANGE));
  assert(!portable_realtime_close(&c));
 }
 const int64_t deltas[]={-1000000,0,999999,1000000,1000001,5000000,5000001};
 for(unsigned i=0;i<sizeof(deltas)/sizeof(deltas[0]);++i) {
  reset();portable_realtime_client c={0};portable_realtime_seed_result r;f.native_delta_us=deltas[i];
  assert(!portable_realtime_open(&c,&runtime,1,1,phase,&f));
  assert(portable_realtime_seed_confirmed(&c,1767225600,1000000,&r)==(i>=1&&i<=3?0:PORTABLE_REALTIME_VERIFY));
  assert(r.seeded && r.attempted && r.verified==(i>=1&&i<=3));assert(!portable_realtime_close(&c));
 }
}
/* Generated by independent Python calendar/regex oracle, all catalog DST
 * gaps/folds and choices, both RTC bases, across every writable year. */
#include "set_time_vectors.h"
static void all_catalog_transitions(void) {
 reset();
 for(unsigned i=0;i<sizeof(set_vectors)/sizeof(set_vectors[0]);++i) {
  const struct set_vector *v=&set_vectors[i];portable_set_time_request p=request("UTC0",v->utc!=0);
  const portable_timezone_entry *entry=portable_timezone_get(v->zone);
  assert(entry && !portable_timezone_parse(entry->posix,strlen(entry->posix)+1,&p.rule));
  p.local=v->civil;p.fold_choice=v->choice;portable_set_time_plan out={.utc_epoch=7};
  int rc=portable_set_time_prepare(&p,&out);
  if(rc!=v->status)fprintf(stderr,"vector %u %s: got %d want %d\n",i,entry->id,rc,v->status);
  assert(rc==v->status);
  if(!rc) {
   assert(out.utc_epoch==v->epoch && out.basis.stores_utc==p.basis.stores_utc && out.basis.reference_epoch==v->epoch);
   portable_timezone_civil expected=v->civil;
   if(p.basis.stores_utc)assert(!portable_timezone_epoch_to_civil(v->epoch,&expected));
   assert(out.rtc_calendar.year==expected.year && out.rtc_calendar.month==expected.month &&
    out.rtc_calendar.day==expected.day && out.rtc_calendar.hour==expected.hour &&
    out.rtc_calendar.minute==expected.minute && out.rtc_calendar.second==expected.second);
  } else assert(out.utc_epoch==7);
  ++vectors;
 }
}
int main(void) {
 basics();plan_boundaries();failures();retries();tick_windows();bracket_failures();boundaries();direct_seed();all_catalog_transitions();
 printf("Portable Set Time production helpers: %u scenarios + %u independent catalog transition vectors PASS\n",cases,vectors);
 return 0;
}
