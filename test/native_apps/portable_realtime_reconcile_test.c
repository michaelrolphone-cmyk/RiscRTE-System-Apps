/* Real shared client with modeled RTC/native callbacks. No device timing claim. */
#define main original_client_tests
#include "portable_realtime_client_test.c"
#undef main
#include <time.h>
static void sample(int64_t rtc_epoch,int drift,uint64_t us) {
 time_t t=(time_t)rtc_epoch;struct tm *v=gmtime(&t);assert(v);
 f.calendar=(twatch_rtc_time_v1){(uint16_t)(v->tm_year+1900),(uint8_t)(v->tm_mon+1),
  (uint8_t)v->tm_mday,(uint8_t)v->tm_wday,(uint8_t)v->tm_hour,(uint8_t)v->tm_min,(uint8_t)v->tm_sec};
 f.snapshot.epoch_seconds=rtc_epoch+drift;f.snapshot.nanoseconds=123456000;
 f.snapshot.monotonic_before_us=us;f.snapshot.monotonic_after_us=us+50;
}
static int check(portable_realtime_reconcile_state *state,int startup,portable_realtime_recovery_result *out) {
 portable_realtime_client c={0};portable_realtime_recovery_policy p=policy("UTC0",true);
 assert(open_client(&c,1,startup)==0);
 int rc=portable_realtime_reconcile_rtc(&c,&p,state,out);
 assert(portable_realtime_close(&c)==0);f.event_count=0;
 return rc;
}
int main(void) {
 original_client_tests();
 const int64_t epoch=1767225600;portable_realtime_recovery_result r;
 for(int drift=-300;drift<=300;drift+=600) {
  reset();sample(epoch,drift,100);portable_realtime_reconcile_state state={0};
  assert(check(&state,1,&r)==PORTABLE_REALTIME_RECOVERED);
  assert(r.seeded && r.snapshot.epoch_seconds==epoch && f.seeds==1 && f.rtc_reads==1);
  /* Rendering/native reads at 10Hz for a full minute do not touch RTC again. */
  for(unsigned n=1;n<600;++n) {
   sample(epoch+n/10,0,100+(uint64_t)n*100000);
   assert(check(&state,1,&r)==0 && r.snapshot.nanoseconds==123456000);
  }
  assert(f.rtc_reads==1 && f.seeds==1);
  sample(epoch+60,0,60000100);assert(check(&state,1,&r)==0);
  assert(f.rtc_reads==2 && f.seeds==1);
 }
 /* Whole-second granularity never accumulates a repeated seed bias. */
 reset();portable_realtime_reconcile_state state={0};
 for(unsigned n=0;n<1440;++n) {
  sample(epoch+n*60, (int)(n%5)-2, (uint64_t)n*60000000);
  assert(check(&state,1,&r)==0 && r.snapshot.nanoseconds==123456000);
 }
 assert(f.rtc_reads==1440 && !f.seeds);
 /* Explicit RTC changes in either direction, followed by settings invalidation. */
 for(int change=-3600;change<=3600;change+=7200) {
  reset();state=(portable_realtime_reconcile_state){0};sample(epoch,0,100);
  assert(check(&state,1,&r)==0);sample(epoch+change,-change,200);
  state.checked=false;assert(check(&state,1,&r)==PORTABLE_REALTIME_RECOVERED);
  assert(r.snapshot.epoch_seconds==epoch+change);
 }
 /* Light return invalidates within the same boot; deep foreground gets fresh
  * state even if native validity survived. Timer-only wake never opens RTC. */
 reset();state=(portable_realtime_reconcile_state){0};sample(epoch,0,100);assert(check(&state,1,&r)==0);
 sample(epoch+10,300,10000100);state.checked=false;
 assert(check(&state,1,&r)==PORTABLE_REALTIME_RECOVERED && r.snapshot.epoch_seconds==epoch+10);
 state=(portable_realtime_reconcile_state){0};sample(epoch+60,300,0);
 unsigned reads=f.rtc_reads;assert(check(&state,0,&r)==PORTABLE_REALTIME_DENIED && f.rtc_reads==reads);
 assert(check(&state,1,&r)==PORTABLE_REALTIME_RECOVERED && r.snapshot.epoch_seconds==epoch+60);
 /* A safely invalid calendar is rate limited and can recover on next attempt. */
 reset();state=(portable_realtime_reconcile_state){0};sample(epoch,300,100);f.calendar.month=13;
 assert(check(&state,1,&r)==PORTABLE_REALTIME_MALFORMED && !f.seeds);
 sample(epoch,300,200);assert(check(&state,1,&r)==0 && f.rtc_reads==1);
 sample(epoch+60,300,60000200);assert(check(&state,1,&r)==PORTABLE_REALTIME_RECOVERED);
 /* Slow sample cannot seed from stale RTC; retry is bounded. */
 reset();state=(portable_realtime_reconcile_state){0};sample(epoch,300,100);f.rtc_delay_us=250001;
 assert(check(&state,1,&r)==PORTABLE_REALTIME_RANGE && !f.seeds && f.rtc_releases==1);
 f.rtc_delay_us=0;sample(epoch+60,300,60000100);assert(check(&state,1,&r)==PORTABLE_REALTIME_RECOVERED);
 /* Detect a seed that reports success but fails bounded readback. */
 reset();state=(portable_realtime_reconcile_state){0};sample(epoch,300,100);f.seed_offset_seconds=30;
 assert(check(&state,1,&r)==PORTABLE_REALTIME_VERIFY && r.seeded);
 /* False RTC I/O may hide retained custody: never release or retry it. */
 for(unsigned retained=0;retained<2;++retained) {
  reset();state=(portable_realtime_reconcile_state){0};sample(epoch,300,100);
  f.rtc_read_ok=false;f.retention_on_rtc_failure=retained;
  portable_realtime_client c={0};portable_realtime_recovery_policy p=policy("UTC0",true);
  assert(open_client(&c,1,1)==0);
  assert(portable_realtime_reconcile_rtc(&c,&p,&state,&r)==PORTABLE_REALTIME_UNCERTAIN);
  assert(!f.seeds && !f.releases);assert_frozen(&c,PORTABLE_REALTIME_UNCERTAIN);
 }
 /* UNSET recovery remains one seed/read, without an immediate duplicate read. */
 reset();state=(portable_realtime_reconcile_state){0};unset();
 assert(check(&state,1,&r)==PORTABLE_REALTIME_RECOVERED);assert(check(&state,1,&r)==0);
 assert(f.rtc_reads==1 && f.seeds==1);
 puts("RTC reconciliation PASS: +/-drift, 1200 refreshes, 1440 minute checks, explicit changes, light/deep foreground wake, timer isolation, malformed/slow/uncertain RTC and seed verification");
 return 0;
}
