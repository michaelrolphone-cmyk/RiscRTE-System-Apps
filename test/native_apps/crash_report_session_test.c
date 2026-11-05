#define main archive_original_main
#include "failure_archive_lifecycle_test.c"
#undef main
#include "CrashReportSession.h"
static crash_report_session session;
static bool safe_hook(void *p){(void)p;return !native_custody_retained;}
static bool prepare_hook(void *p){(void)p;return contexts_ok && broadcast_ok;}
static uint64_t now_hook(void *p){(void)p;return ticks;}
static void retain_hook(void *p){(void)p;portable_adapter_retain_silent();}
static int32_t read_hook(uint64_t invocation,risc_failure_evidence_v1 *out){assert(invocation==41);*out=record(f.native_sequence);return f.native_pending?RISC_FAILURE_EVIDENCE_OK:RISC_FAILURE_EVIDENCE_NONE;}
static int32_t ack_hook(uint64_t invocation,uint32_t boot,uint32_t seq){
 assert(session.presented && session.committed && !f.data_active && !f.sd_active);
 failure_archive_presented=failure_archive_committed=true;failure_archive_identity=session.identity;
 memcpy(failure_archive_text,session.text,session.text_size);failure_archive_text_size=session.text_size;
 return acknowledge(invocation,boot,seq);
}
static bool client_hook(risc_failure_evidence_client_v1 *out){*out=client;out->read=read_hook;out->acknowledge=ack_hook;return true;}
static void session_start(bool sd){
 runtime.failure_evidence=client_hook;
 crash_report_session_hooks h={.runtime=&runtime,.safe=safe_hook,.prepare_storage=prepare_hook,.now=now_hook,.retain=retain_hook,
 .namespace_id=PORTABLE_CRASH_REPORT_NAMESPACE,.volume_instance=9,.sd_enabled=sd};
 crash_session_init(&session,&h);
}
int main(void){
 for(unsigned mode=0;mode<4;++mode){
  reset();session_start(mode!=3);f.native_pending=true;f.native_boot=7;f.native_sequence=11;
  f.sd_available=mode!=0;f.sd_exported=mode==1;f.data.available=mode!=2;
  assert(crash_session_read(&session));report_buffer expected=expected_report(11);
  assert(session.text_size==expected.used && !memcmp(session.text,expected.bytes,expected.used));
  assert(!f.acks);assert(crash_session_checkpoint(&session));assert(!f.acks);
  session.presented=true;assert(crash_session_ack(&session));assert(f.acks==(mode!=2));
  if(mode==2){f.data.available=true;ticks+=30000;assert(crash_session_checkpoint(&session));assert(f.acks==1);}
  if(mode==0 || mode==1 || mode==3){assert(disk_count()==1);assert(!f.data.removals);}
  /* Fresh Home lifecycle keeps the exact bytes, even after native ack. */
  reset_lifecycle();session_start(mode!=3);assert(crash_session_read(&session));assert(!session.pending);
  f.sd_available=true;f.sd_exported=false;ticks+=30000;assert(crash_session_checkpoint(&session));
  if(mode!=3){assert_final(11);assert(disk_count()==0);}else assert(disk_count()==1);
  assert_idle_grants();
 }
 /* Full backlog leaves the new native report pending; drain, retry, then ack. */
 reset();for(unsigned i=1;i<=8;++i){assert(capture(i));finish_presentation();}
 session_start(true);f.native_pending=true;f.native_boot=7;f.native_sequence=9;
 assert(crash_session_read(&session));assert(!session.committed);session.presented=true;
 assert(crash_session_ack(&session));assert(f.native_pending);
 assert(crash_session_checkpoint(&session));assert(disk_count()==7 && f.native_pending);
 ticks+=30000;assert(crash_session_checkpoint(&session));assert(!f.native_pending);
 puts("Crash report session PASS: exact text, acknowledgement barriers, next-boot internal persistence, absent/exported SD retries, full-queue recovery");return 0;
}
