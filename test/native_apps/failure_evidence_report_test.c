#include "FailureEvidenceReport.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

enum { TEST_LINES=40, TEST_TEXT=8192 };
typedef struct {
 char lines[TEST_LINES][FAILURE_EVIDENCE_REPORT_LINE_CAPACITY];
 char text[TEST_TEXT];
 unsigned calls,count,fail_at;
 size_t used;
} report_capture;

static bool capture_line(void *context,const char *line) {
 report_capture *out=context;
 unsigned call=out->calls++;
 if(call==out->fail_at)return false;
 size_t length=strlen(line);
 assert(length && length<FAILURE_EVIDENCE_REPORT_LINE_CAPACITY);
 assert(out->count<TEST_LINES && out->used+length+2<=sizeof(out->text));
 for(size_t i=0;i<length;++i)assert((unsigned char)line[i]>=32 && (unsigned char)line[i]<=126);
 assert(line[0]!=' ');
 memcpy(out->lines[out->count++],line,length+1);
 memcpy(out->text+out->used,line,length);out->used+=length;
 out->text[out->used++]='\n';out->text[out->used]=0;
 return true;
}

static risc_failure_evidence_v1 empty_record(void) {
 risc_failure_evidence_v1 record={0};
 record.api_version=RISC_FAILURE_EVIDENCE_API_V1;record.struct_size=sizeof(record);
 record.kind=RISC_FAILURE_RESET_ONLY;
 return record;
}

static report_capture report(const risc_failure_evidence_v1 *record) {
 report_capture out={0};out.fail_at=UINT_MAX;
 risc_failure_evidence_v1 before;memcpy(&before,record,sizeof(before));
 assert(failure_evidence_report_format(record,capture_line,&out));
 assert(!memcmp(&before,record,sizeof(before)));
 assert(out.calls==out.count);
 return out;
}

static unsigned count_prefix(const report_capture *out,const char *prefix) {
 unsigned count=0;size_t length=strlen(prefix);
 for(unsigned i=0;i<out->count;++i)if(!strncmp(out->lines[i],prefix,length))++count;
 return count;
}

static void test_missing(void) {
 risc_failure_evidence_v1 record=empty_record();
 /* Unsaved fields must not become invented activity, registers or frames. */
 record.pc=record.sp=UINT32_MAX;record.phase=RISC_FAILURE_PHASE_MAIN;
 record.invocation=UINT64_MAX;record.status=INT32_MIN;
 strcpy(record.application,"hidden-app.elf");
 memset(record.frames,0x5a,sizeof(record.frames));memset(record.history,0xa5,sizeof(record.history));
 report_capture out=report(&record);
 assert(!strcmp(out.text,
  "Previous restart recorded\n"
  "Record: boot 0 / sequence 0\n"
  "This boot reset reason: 0 (raw)\n"
  "Capture reset hint: unavailable\n"
  "Application / phase / invocation: unavailable\n"
  "Failure status: unavailable\n"
  "Failure detail: unavailable\n"
  "RECENT SEQUENCE (bounded saved history)\n"
  "Sequence history: unavailable\n"
  "RAW PC / STACK (addresses, no symbols)\n"
  "Exception PC / SP: unavailable\n"
  "Stack unavailable: not captured\n"
  "Evidence may not survive power loss.\n"));
 assert(out.count==13);
 record.flags=RISC_FAILURE_CONTEXT;record.application[0]=0;
 out=report(&record);assert(!strcmp(out.lines[5],"Application: unavailable"));
 assert(!strcmp(out.lines[6],"Phase: main (3)"));
 assert(!strcmp(out.lines[7],"Invocation: 18446744073709551615"));
}

static void test_complete(void) {
 risc_failure_evidence_v1 record=empty_record();
 record.kind=RISC_FAILURE_NATIVE_PANIC;
 record.record_boot=7;record.record_sequence=81;record.current_reset_reason=6;record.captured_reset_hint=4;
 record.flags=RISC_FAILURE_PENDING|RISC_FAILURE_CONTEXT|RISC_FAILURE_REGISTERS;
 record.phase=RISC_FAILURE_PHASE_MAIN;record.invocation=42;
 record.pc=0x42012345;record.sp=0x3fca0000;record.status=-7;
 strcpy(record.application,"apps/example.elf");strcpy(record.detail,"Exception captured before restart");
 record.stack_status=RISC_FAILURE_STACK_COMPLETE;record.frame_count=2;record.history_count=2;
 record.history[0]=(risc_failure_step_v1){70,RISC_FAILURE_PHASE_LOAD,40};
 record.history[1]=(risc_failure_step_v1){71,RISC_FAILURE_PHASE_INIT,41};
 record.frames[0]=(risc_failure_frame_v1){0x42010000,0x3fca0040};
 record.frames[1]=(risc_failure_frame_v1){0x42010020,0x3fca0080};
 report_capture out=report(&record);
 assert(!strcmp(out.text,
  "Native crash recorded\n"
  "Record: boot 7 / sequence 81\n"
  "This boot reset reason: 6 (raw)\n"
  "Capture reset hint: 4 (raw)\n"
  "LAST RUNTIME ACTIVITY (not fault attribution)\n"
  "apps/example.elf\n"
  "Phase: main (3)\n"
  "Invocation: 42\n"
  "Failure status: unavailable\n"
  "Exception captured before restart\n"
  "RECENT SEQUENCE (bounded saved history)\n"
  "70: load / invocation 40\n"
  "71: initialize / invocation 41\n"
  "RAW PC / STACK (addresses, no symbols)\n"
  "Exception PC 42012345  SP 3fca0000\n"
  "Stack walk: complete\n"
  "#0 PC 42010000  SP 3fca0040\n"
  "#1 PC 42010020  SP 3fca0080\n"
  "Evidence may not survive power loss.\n"));
 assert(out.count==19);
 /* Neither pending/abort flags nor extra ABI fields change this report body. */
 record.flags=(record.flags&~RISC_FAILURE_PENDING)|RISC_FAILURE_ABORT;
 record.core=record.exception=record.a0=record.ps=record.exccause=record.excvaddr=UINT32_MAX;
 memset(record.firmware_sha256,0xff,sizeof(record.firmware_sha256));
 report_capture same=report(&record);assert(!strcmp(same.text,out.text));
 assert(!strstr(out.text,"PREVIOUS") && !strstr(out.text,"CONTINUE") && !strstr(out.text,"Tap a button"));
}

static void test_states_and_extremes(void) {
 static const struct {uint32_t value;const char *text;} states[]={
  {RISC_FAILURE_STACK_UNAVAILABLE,"Stack unavailable: not captured"},
  {RISC_FAILURE_STACK_COMPLETE,"Stack walk: complete"},
  {RISC_FAILURE_STACK_LIMIT,"Stack truncated: frame limit reached"},
  {RISC_FAILURE_STACK_INVALID,"Stack incomplete: invalid frame"},
  {RISC_FAILURE_STACK_UNSUPPORTED,"Stack unavailable: unsupported capture"},
  {UINT32_MAX,"Stack unavailable: not captured"}
 };
 risc_failure_evidence_v1 record=empty_record();
 record.kind=RISC_FAILURE_RETENTION;record.status=INT32_MIN;
 record.flags=RISC_FAILURE_CONTEXT|RISC_FAILURE_REGISTERS;
 record.record_boot=record.record_sequence=record.current_reset_reason=record.captured_reset_hint=UINT32_MAX;
 record.pc=record.sp=record.phase=UINT32_MAX;record.invocation=UINT64_MAX;
 strcpy(record.detail,"Provider cleanup unconfirmed");
 record.history_count=1;record.history[0]=(risc_failure_step_v1){UINT32_MAX,UINT32_MAX,UINT64_MAX};
 record.frame_count=1;record.frames[0]=(risc_failure_frame_v1){0,UINT32_MAX};
 for(size_t i=0;i<sizeof(states)/sizeof(states[0]);++i) {
  record.stack_status=states[i].value;
  report_capture out=report(&record);
  assert(!strcmp(out.lines[0],"Runtime stopped; restart was required"));
  assert(!strcmp(out.lines[1],"Record: boot 4294967295 / sequence 4294967295"));
  assert(!strcmp(out.lines[2],"This boot reset reason: 4294967295 (raw)"));
  assert(!strcmp(out.lines[3],"Capture reset hint: 4294967295 (raw)"));
  assert(!strcmp(out.lines[6],"Phase: unavailable (4294967295)"));
  assert(!strcmp(out.lines[7],"Invocation: 18446744073709551615"));
  assert(!strcmp(out.lines[8],"Failure status: -2147483648"));
  assert(!strcmp(out.lines[11],"4294967295: unavailable / invocation 18446744073709551615"));
  assert(!strcmp(out.lines[13],"Exception PC ffffffff  SP ffffffff"));
  assert(!strcmp(out.lines[14],states[i].text));
  assert(!strcmp(out.lines[15],"#0 PC 00000000  SP ffffffff"));
  /* Stack status does not fabricate or discard individually saved frames. */
  assert(count_prefix(&out,"#")==1);
  record.frame_count=0;out=report(&record);assert(count_prefix(&out,"#")==0);
  record.frame_count=1;
 }
 record.status=INT32_MAX;report_capture out=report(&record);
 assert(!strcmp(out.lines[8],"Failure status: 2147483647"));
 record.kind=UINT32_MAX;out=report(&record);
 assert(!strcmp(out.lines[0],"Previous restart recorded"));
 assert(!strcmp(out.lines[8],"Failure status: unavailable"));
}

static risc_failure_evidence_v1 full_record(void) {
 risc_failure_evidence_v1 record=empty_record();
 record.flags=RISC_FAILURE_CONTEXT|RISC_FAILURE_REGISTERS;
 strcpy(record.application,"apps/limit.elf");strcpy(record.detail,"Saved evidence");
 record.history_count=RISC_FAILURE_EVIDENCE_HISTORY;record.frame_count=RISC_FAILURE_EVIDENCE_FRAMES;
 record.stack_status=RISC_FAILURE_STACK_LIMIT;
 for(unsigned i=0;i<RISC_FAILURE_EVIDENCE_HISTORY;++i)
  record.history[i]=(risc_failure_step_v1){100+i,1+i,200+i};
 for(unsigned i=0;i<RISC_FAILURE_EVIDENCE_FRAMES;++i)
  record.frames[i]=(risc_failure_frame_v1){0x42010000+i*0x20,0x3fca0000+i*0x40};
 return record;
}

static void test_count_bounds_and_phases(void) {
 static const char *phases[]={"load","initialize","main","finalize","cleanup","handoff","retained","idle"};
 risc_failure_evidence_v1 record=full_record();report_capture out=report(&record);
 assert(RISC_FAILURE_EVIDENCE_HISTORY==8 && RISC_FAILURE_EVIDENCE_FRAMES==8);
 assert(count_prefix(&out,"#")==8 && count_prefix(&out,"History limited")==1);
 assert(!strstr(out.text,"Reported frames exceed capacity"));
 for(unsigned i=0;i<8;++i) {
  char expected[96];
  snprintf(expected,sizeof(expected),"%u: %s / invocation %u",100+i,phases[i],200+i);
  assert(!strcmp(out.lines[11+i],expected));
  record.phase=i+1;report_capture phase=report(&record);
  snprintf(expected,sizeof(expected),"Phase: %s (%u)",phases[i],i+1);
  assert(!strcmp(phase.lines[6],expected));
 }
 assert(!strcmp(out.lines[19],"History limited to 8 saved steps."));
 for(unsigned n=0;n<=9;++n) {
  record.frame_count=record.history_count=n;out=report(&record);
  assert(count_prefix(&out,"#")==(n<8?n:8));
  assert(count_prefix(&out,"History limited")== (unsigned)(n>=8));
  assert(count_prefix(&out,"Reported frames exceed")== (unsigned)(n>8));
  assert(count_prefix(&out,"Sequence history: unavailable")== (unsigned)(n==0));
  assert(count_prefix(&out,"Stack truncated: frame limit reached")==1);
 }
 record.frame_count=record.history_count=UINT32_MAX;out=report(&record);
 assert(out.count==33 && count_prefix(&out,"#")==8);
 assert(!strcmp(out.lines[30],"#7 PC 420100e0  SP 3fca01c0"));
 assert(!strcmp(out.lines[31],"Reported frames exceed capacity; truncated."));
 assert(!strcmp(out.lines[32],"Evidence may not survive power loss."));
}

static void test_text_bounds_and_sanitization(void) {
 risc_failure_evidence_v1 record=empty_record();record.flags=RISC_FAILURE_CONTEXT;
 memset(record.application,'A',sizeof(record.application));memset(record.detail,'D',sizeof(record.detail));
 report_capture out=report(&record);
 assert(strlen(out.lines[5])==sizeof(record.application)-1);
 assert(strlen(out.lines[9])==sizeof(record.detail)-1);
 for(size_t i=0;i<sizeof(record.application)-1;++i)assert(out.lines[5][i]=='A');
 for(size_t i=0;i<sizeof(record.detail)-1;++i)assert(out.lines[9][i]=='D');
 record.application[sizeof(record.application)-1]='X';record.detail[sizeof(record.detail)-1]='Y';
 report_capture same=report(&record);assert(!strcmp(out.text,same.text));
 strcpy(record.application," \t\r\n\x1b\x7f\x80%N\t\n\xff app.elf  ");
 strcpy(record.detail,"\x01 detail\r\nNEXT\tCONTINUE\x7f\x80\xff end");
 out=report(&record);
 assert(!strcmp(out.lines[5],"%N    app.elf  "));
 assert(!strcmp(out.lines[9],"detail  NEXT CONTINUE    end"));
 /* Every non-NUL byte value is tested, both at the start and in a field. */
 for(unsigned byte=1;byte<=255;++byte) {
  memset(record.application,0,sizeof(record.application));record.application[0]=(char)byte;
  record.application[1]='X';record.detail[0]='D';record.detail[1]=(char)byte;record.detail[2]='E';record.detail[3]=0;
  out=report(&record);
  if(byte<=32 || byte>126)assert(!strcmp(out.lines[5],"X"));
  else assert(out.lines[5][0]==(char)byte && !strcmp(out.lines[5]+1,"X"));
  assert(out.lines[9][0]=='D' && out.lines[9][1]==(char)(byte<32 || byte>126?' ':byte));
  assert(!strcmp(out.lines[9]+2,"E"));
 }
 /* Embedded NUL ends a field; all whitespace remains omitted as on the OSD. */
 memset(record.application,'A',sizeof(record.application));record.application[1]=0;
 memset(record.detail,'D',sizeof(record.detail));record.detail[1]=0;
 out=report(&record);assert(!strcmp(out.lines[5],"A") && !strcmp(out.lines[9],"D"));
 memset(record.application,' ',sizeof(record.application));memset(record.detail,'\t',sizeof(record.detail));
 out=report(&record);
 assert(out.count==14 && !strcmp(out.lines[5],"Phase: unavailable (0)"));
 assert(!strstr(out.text,"Application: unavailable") && !strstr(out.text,"Failure detail: unavailable"));
}

static unsigned null_context_calls;
static bool null_context_sink(void *context,const char *line) {
 assert(!context && line && line[0]);++null_context_calls;return true;
}

static void test_invalid_and_sink_failure(void) {
 risc_failure_evidence_v1 record=full_record();record.frame_count=record.history_count=UINT32_MAX;
 report_capture full=report(&record),out={0};out.fail_at=UINT_MAX;
 assert(!failure_evidence_report_format(NULL,capture_line,&out));
 assert(!failure_evidence_report_format(&record,NULL,&out));
 record.api_version=0;assert(!failure_evidence_report_format(&record,capture_line,&out));
 record.api_version=UINT32_MAX;assert(!failure_evidence_report_format(&record,capture_line,&out));
 record.api_version=RISC_FAILURE_EVIDENCE_API_V1;record.struct_size=sizeof(record)-1;
 assert(!failure_evidence_report_format(&record,capture_line,&out));
 record.struct_size=0;assert(!failure_evidence_report_format(&record,capture_line,&out));
 assert(!out.calls);
 record.struct_size=sizeof(record);
 for(unsigned fail=0;fail<full.count;++fail) {
  memset(&out,0,sizeof(out));out.fail_at=fail;
  risc_failure_evidence_v1 before;memcpy(&before,&record,sizeof(before));
  assert(!failure_evidence_report_format(&record,capture_line,&out));
  assert(out.calls==fail+1 && out.count==fail);
  assert(!memcmp(&before,&record,sizeof(before)));
  for(unsigned i=0;i<fail;++i)assert(!strcmp(out.lines[i],full.lines[i]));
 }
 memset(&out,0,sizeof(out));out.fail_at=full.count;
 assert(failure_evidence_report_format(&record,capture_line,&out));assert(!strcmp(out.text,full.text));
 assert(failure_evidence_report_format(&record,null_context_sink,NULL));assert(null_context_calls==full.count);
 record.struct_size=UINT32_MAX;out=report(&record);assert(!strcmp(out.text,full.text));
}

static void test_adversarial_records(void) {
 uint32_t random=0x7a37d519;
 for(unsigned trial=0;trial<2048;++trial) {
  risc_failure_evidence_v1 record;
  unsigned char *bytes=(unsigned char *)&record;
  for(size_t i=0;i<sizeof(record);++i) {
   random^=random<<13;random^=random>>17;random^=random<<5;bytes[i]=(unsigned char)random;
  }
  record.api_version=RISC_FAILURE_EVIDENCE_API_V1;record.struct_size=sizeof(record);
  report_capture out=report(&record);
  assert(out.count<=33 && out.used<TEST_TEXT);
  unsigned frames=record.frame_count<RISC_FAILURE_EVIDENCE_FRAMES?record.frame_count:RISC_FAILURE_EVIDENCE_FRAMES;
  unsigned end=out.count-1-(unsigned)(record.frame_count>RISC_FAILURE_EVIDENCE_FRAMES);
  for(unsigned i=0;i<frames;++i) {
   char expected[96];
   snprintf(expected,sizeof(expected),"#%u PC %08lx  SP %08lx",i,
    (unsigned long)record.frames[i].pc,(unsigned long)record.frames[i].sp);
   assert(!strcmp(out.lines[end-frames+i],expected));
  }
  assert(!strcmp(out.lines[out.count-1],"Evidence may not survive power loss."));
 }
}

int main(void) {
 test_missing();test_complete();test_states_and_extremes();test_count_bounds_and_phases();
 test_text_bounds_and_sanitization();test_invalid_and_sink_failure();test_adversarial_records();
 puts("Failure evidence report: exact body, missing/complete/limit/invalid/unsupported states, bounded history/frames/text, all-byte sanitization, sink failures and 2048 adversarial records PASS");
 return 0;
}
