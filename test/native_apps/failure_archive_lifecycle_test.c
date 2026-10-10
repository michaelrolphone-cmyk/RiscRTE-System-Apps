/* Host integration: include the product's real archive lifecycle and its real
 * formatter/AppData spool/SD exporter. Only host, UI and capability boundaries
 * are simulated. No archive function is replaced. No device I/O is performed. */
#include "RiscRuntimeV1.h"
#include "FailureEvidenceReport.h"
#include "CrashReportSpool.h"
#include "CrashReportSd.h"
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>

#define PORTABLE_CRASH_REPORT_SD 1
#define PORTABLE_RESIDENT_SHELL_HOST 1
#define PORTABLE_RESIDENT_POLICY 1
#define PORTABLE_CRASH_REPORT_NAMESPACE UINT64_C(0x43525348)
#define PORTABLE_CONTEXTS_CLIENT 1
#define PORTABLE_BLE_BROADCAST 1

static bool native_custody_retained,failed,resident_child_active,resident_dispatching;
static bool display_settled,alarm_modal,quick_modal,resident_launch_pending,resident_sleep_pending;
static bool resident_prior_failure_checked;
static struct { bool frame; } surface;
static unsigned paper_token;
static struct { bool ui; } quick;
static uint32_t ticks;
static bool contexts_ok,broadcast_ok;
static unsigned retained_calls,diagnostic_calls;
static const risc_runtime_api_v1 *rt;
static uint32_t millis_now(void) { return ticks; }
static bool pqa_visible(const bool *ui) { return *ui; }
static bool contexts_before_storage(void) { return contexts_ok; }
static bool broadcast_before_storage(void) { return broadcast_ok; }
static void portable_adapter_retain_silent(void);
typedef struct { char text[1024];size_t size; } failure_evidence_layout;
static void failure_evidence_text(failure_evidence_layout *layout,const char *line) {
 size_t n=strlen(line);assert(layout->size+n+2<sizeof(layout->text));
 memcpy(layout->text+layout->size,line,n);layout->size+=n;
 layout->text[layout->size++]='\n';layout->text[layout->size]=0;
}
#include "../../lib/PortableApps/src/failure_archive.inc"

enum { FILES=20, EVENTS=4096 };
typedef enum {
 ACQUIRE_DATA,RELEASE_DATA,DATA_STAT,DATA_READ,DATA_REPLACE,ACK,
 ACQUIRE_SD,RELEASE_SD,SD_READY,SD_STAT,SD_MKDIR,SD_OPEN,SD_WRITE,
 SD_READ,SD_INFO,SD_ERROR,SD_SYNC,SD_CLOSE,SD_RENAME,SD_REMOVE,KINDS
} operation;
typedef struct {
 bool used,synced;char path[128];unsigned char bytes[4096];size_t size;
} disk_file;
typedef struct {
 unsigned char bytes[CRASH_REPORT_SPOOL_FILE_MAX];uint32_t size;
 bool present,available;uint64_t revision;
 int32_t stat_fault,read_fault,replace_fault,post_replace_read_fault;
 bool commit_on_fault,corrupt_on_fault;
 unsigned replacements,removals;
} app_disk;
typedef struct {
 risc_app_data_v1 api;bool live;unsigned generation;
} app_lease;
typedef struct {
 app_disk data;app_lease lease[2];unsigned next_lease;bool data_active,sd_active;
 unsigned generation,events,counts[KINDS];operation trace[EVENTS];
 bool forbid,data_acquire_denied,sd_acquire_denied,release_data_failure,release_sd_failure;
 bool sd_available,sd_readonly,sd_exported,sd_directory;
 bool close_failure,remove_failure,final_read_corrupt,retain_on_sd_read;
 bool sd_poisoned,hidden_mkdir_failure,short_write;unsigned hidden_stat_at;
 unsigned sd_duration,handle,slot,flags;size_t position;
 bool eof_seen,read_bad,final_verified;
 unsigned verified_serial,deletion_serial,final_writes,renames;
 disk_file files[FILES];risc_storage_volume_api_v1_ext volume;
 risc_storage_volume_api_v1_state checked;unsigned observations;
 bool missing_state,poison_first_ready,poison_final_close;
 unsigned acks;uint32_t native_boot,native_sequence;bool native_pending;
 int32_t ack_fault;
} fixture;
static fixture f;
static unsigned tests;
static const char *test_name;

static void event(operation op) {
 assert(!f.forbid && !native_custody_retained && !failed);
 assert(f.events<EVENTS);f.trace[f.events++]=op;++f.counts[op];
 if(op>=SD_READY)ticks+=f.sd_duration;
}
static void portable_adapter_retain_silent(void) {
 ++retained_calls;native_custody_retained=true;f.forbid=true;
}
static app_disk *data_context(void *context) {
 app_lease *lease=context;assert(lease->live && f.data_active && !f.sd_active);
 assert(lease->generation==f.generation);return &f.data;
}
static int32_t data_stat(void *context,const char *name,uint32_t *size,uint64_t *revision) {
 app_disk *d=data_context(context);assert(!strcmp(name,CRASH_REPORT_SPOOL_FILE));
 event(DATA_STAT);*size=0;*revision=0;
 if(d->stat_fault) {int32_t r=d->stat_fault;d->stat_fault=0;if(r==RISC_APP_DATA_RETAINED)f.forbid=true;return r;}
 if(!d->available)return RISC_APP_DATA_UNAVAILABLE;
 if(!d->present)return RISC_APP_DATA_NOT_FOUND;
 *size=d->size;*revision=d->revision;return RISC_APP_DATA_OK;
}
static int32_t data_read(void *context,const char *name,uint64_t expected,void *bytes,
 uint32_t capacity,uint32_t *size,uint64_t *revision) {
 app_disk *d=data_context(context);assert(!strcmp(name,CRASH_REPORT_SPOOL_FILE));
 event(DATA_READ);*size=0;*revision=0;
 if(d->read_fault) {int32_t r=d->read_fault;d->read_fault=0;if(r==RISC_APP_DATA_RETAINED)f.forbid=true;return r;}
 if(!d->available)return RISC_APP_DATA_UNAVAILABLE;
 if(!d->present)return RISC_APP_DATA_NOT_FOUND;
 assert(expected==d->revision);assert(capacity>=d->size);
 memcpy(bytes,d->bytes,d->size);*size=d->size;*revision=d->revision;return RISC_APP_DATA_OK;
}
static int32_t data_replace(void *context,const char *name,uint64_t expected,const void *bytes,uint32_t size) {
 app_disk *d=data_context(context);assert(!strcmp(name,CRASH_REPORT_SPOOL_FILE));
 event(DATA_REPLACE);++d->replacements;
 assert(expected==(d->present?d->revision:0));assert(size<=sizeof(d->bytes));
 unsigned old_count=d->present && d->size>=80?crash_report_spool_u32(d->bytes+16):0;
 unsigned new_count=crash_report_spool_u32((const uint8_t *)bytes+16);
 if(new_count<old_count) {
  /* The lifecycle may delete only after the final SD file passed exact EOF,
   * error, sync and checked-close verification; grant ordering is enforced. */
  assert(f.final_verified && f.verified_serial>f.deletion_serial);
  assert(new_count+1==old_count);
  uint32_t report_size=crash_report_spool_u32(d->bytes+CRASH_REPORT_SPOOL_HEADER_SIZE+40);
  const disk_file *final=&f.files[f.slot];
  assert(strstr(final->path,".txt") && final->synced && final->size>=report_size);
  assert(!memcmp(final->bytes,d->bytes+CRASH_REPORT_SPOOL_HEADER_SIZE+CRASH_REPORT_SPOOL_ENTRY_HEADER_SIZE,report_size));
  unsigned drops=crash_report_spool_u32(d->bytes+20);
  if(drops) {
   char note[192];int length=snprintf(note,sizeof(note),
    "Report queue overflow: %lu unsaved report(s) dropped.\nLast dropped record: boot %lu / sequence %lu.\n",
    (unsigned long)drops,(unsigned long)crash_report_spool_u32(d->bytes+56),
    (unsigned long)crash_report_spool_u32(d->bytes+60));
   assert(length>0 && (size_t)length<sizeof(note) && final->size==report_size+(unsigned)length);
   assert(!memcmp(final->bytes+report_size,note,(size_t)length));
  }else assert(final->size==report_size);
  f.deletion_serial=f.events;++d->removals;
 }
 int32_t result=d->replace_fault;d->replace_fault=0;
 if(!d->available)result=RISC_APP_DATA_UNAVAILABLE;
 if(!result || d->commit_on_fault) {
  memcpy(d->bytes,bytes,size);d->size=size;d->present=true;++d->revision;
  if(d->corrupt_on_fault)d->bytes[size-1]^=0x80;
 }
 d->read_fault=d->post_replace_read_fault;d->post_replace_read_fault=0;
 d->commit_on_fault=d->corrupt_on_fault=false;
 if(result==RISC_APP_DATA_RETAINED)f.forbid=true;
 return result;
}
static unsigned file_find(const char *path) {
 for(unsigned i=0;i<FILES;++i)if(f.files[i].used && !strcmp(f.files[i].path,path))return i;
 return FILES;
}
static unsigned file_add(const char *path) {
 assert(file_find(path)==FILES);
 for(unsigned i=0;i<FILES;++i)if(!f.files[i].used) {
  memset(&f.files[i],0,sizeof(f.files[i]));f.files[i].used=true;
  assert(strlen(path)<sizeof(f.files[i].path));strcpy(f.files[i].path,path);return i;
 }
 assert(!"file capacity");return FILES;
}
static void sd_context(void *context) {assert(context==&f && f.sd_active && !f.data_active);}
static int32_t sd_observe(void *context) {
 sd_context(context);++f.observations;
 if(f.sd_poisoned)return RISC_STORAGE_STATE_RETAINED;
 return f.sd_available && !f.sd_exported?RISC_STORAGE_STATE_READY:RISC_STORAGE_STATE_UNAVAILABLE;
}
static bool sd_ready(void *context) {
 sd_context(context);event(SD_READY);
 if(f.poison_first_ready){f.sd_poisoned=true;return false;}
 if(f.sd_poisoned){f.forbid=true;return false;}
 return f.sd_available && !f.sd_exported;
}
static bool sd_stat(void *context,const char *path,uint64_t *size,bool *directory) {
 sd_context(context);event(SD_STAT);
 if(f.hidden_stat_at==f.counts[SD_STAT]){f.sd_poisoned=true;return false;}
 if(!strcmp(path,"/CrashReports")){*directory=true;*size=0;return f.sd_directory;}
 unsigned n=file_find(path);if(n==FILES)return false;
 *directory=false;*size=f.files[n].size;return true;
}
static bool sd_mkdir(void *context,const char *path) {
 sd_context(context);event(SD_MKDIR);assert(!strcmp(path,"/CrashReports"));
 if(f.hidden_mkdir_failure){f.sd_poisoned=true;return false;}
 if(f.sd_readonly)return false;
 f.sd_directory=true;return true;
}
static uint32_t sd_open(void *context,const char *path,uint32_t flags) {
 sd_context(context);event(SD_OPEN);assert(!f.handle);
 unsigned n=file_find(path);
 if(flags&RISC_STORAGE_OPEN_WRITE) {
  assert(flags==(RISC_STORAGE_OPEN_WRITE|RISC_STORAGE_OPEN_CREATE|RISC_STORAGE_OPEN_EXCLUSIVE));
  assert(strstr(path,".tmp") && !strstr(path,".txt"));
  if(f.sd_readonly || n!=FILES)return 0;
  n=file_add(path);++f.final_writes;
 }else {assert(flags==RISC_STORAGE_OPEN_READ);if(n==FILES)return 0;}
 f.handle=17;f.slot=n;f.flags=flags;f.position=0;f.eof_seen=f.read_bad=false;
 return f.handle;
}
static size_t sd_write(void *context,uint32_t handle,const void *bytes,size_t size) {
 sd_context(context);event(SD_WRITE);assert(handle==f.handle && (f.flags&RISC_STORAGE_OPEN_WRITE));
 assert(size && size<=512 && f.position+size<=4096);
 if(f.short_write)--size;
 disk_file *file=&f.files[f.slot];memcpy(file->bytes+f.position,bytes,size);
 f.position+=size;file->size=f.position;file->synced=false;return size;
}
static size_t sd_read(void *context,uint32_t handle,void *bytes,size_t size) {
 sd_context(context);event(SD_READ);assert(handle==f.handle && f.flags==RISC_STORAGE_OPEN_READ);
 assert(size && size<=512);disk_file *file=&f.files[f.slot];
 if(f.retain_on_sd_read) {native_custody_retained=true;f.forbid=true;return 0;}
 if(size>file->size-f.position)size=file->size-f.position;
 if(size)memcpy(bytes,file->bytes+f.position,size);else f.eof_seen=true;
 if(size && f.final_read_corrupt && strstr(file->path,".txt")) {
  ((unsigned char *)bytes)[0]^=0x40;f.read_bad=true;
 }
 f.position+=size;return size;
}
static bool sd_info(void *context,uint32_t handle,uint64_t *size,uint64_t *position) {
 sd_context(context);event(SD_INFO);assert(handle==f.handle);
 *size=f.files[f.slot].size;*position=f.position;return true;
}
static uint32_t sd_error(void *context,uint32_t handle,bool directory) {
 sd_context(context);event(SD_ERROR);assert(handle==f.handle && !directory);return 0;
}
static bool sd_sync(void *context,uint32_t handle) {
 sd_context(context);event(SD_SYNC);assert(handle==f.handle);f.files[f.slot].synced=true;return true;
}
static bool sd_close(void *context,uint32_t handle,bool commit) {
 sd_context(context);event(SD_CLOSE);assert(handle==f.handle && commit);
 if(f.close_failure){f.forbid=true;return false;}
 disk_file *file=&f.files[f.slot];
 if(f.flags==RISC_STORAGE_OPEN_READ && strstr(file->path,".txt") && f.eof_seen && !f.read_bad && file->synced) {
  f.final_verified=true;f.verified_serial=f.events;
  if(f.poison_final_close)f.sd_poisoned=true;
 }
 f.handle=0;return true;
}
static bool sd_remove(void *context,const char *path) {
 sd_context(context);event(SD_REMOVE);assert(!f.handle && strstr(path,".tmp") && !strstr(path,".txt"));
 if(f.remove_failure){f.sd_poisoned=true;return false;}
 unsigned n=file_find(path);assert(n<FILES);f.files[n].used=false;return true;
}
static bool sd_rename(void *context,const char *from,const char *to) {
 sd_context(context);event(SD_RENAME);assert(!f.handle && strstr(from,".tmp") && strstr(to,".txt"));
 unsigned n=file_find(from);assert(n<FILES && f.files[n].synced);
 if(file_find(to)!=FILES)return false;
 strcpy(f.files[n].path,to);++f.renames;return true;
}
static bool acquire(const char *name,uint32_t version,uint64_t instance,risc_runtime_capability_v1 *out) {
 assert(version==1 && out->struct_size==sizeof(*out));
 assert(!f.data_active && !f.sd_active);
 if(!strcmp(name,RISC_APP_DATA_CAPABILITY)) {
  event(ACQUIRE_DATA);assert(instance==PORTABLE_CRASH_REPORT_NAMESPACE);
  if(f.data_acquire_denied)return false;
  app_lease *lease=&f.lease[f.next_lease++%2];lease->live=true;
  lease->generation=++f.generation;f.data.revision+=17;
  f.data_active=true;out->slot=1;out->generation=f.generation;out->api=&lease->api;return true;
 }
 assert(!strcmp(name,"storage.volume") && instance==9);event(ACQUIRE_SD);
 if(f.sd_acquire_denied){portable_adapter_retain_silent();return false;}
 f.sd_active=true;out->slot=2;out->generation=++f.generation;out->api=f.missing_state?(const void *)&f.volume:(const void *)&f.checked;return true;
}
static bool release(risc_runtime_capability_v1 *grant) {
 if(grant->slot==1) {
  event(RELEASE_DATA);assert(f.data_active && !f.sd_active);
  if(f.release_data_failure){f.forbid=true;return false;}
  const risc_app_data_v1 *api=grant->api;((app_lease *)api->context)->live=false;f.data_active=false;
 }else {
  event(RELEASE_SD);assert(grant->slot==2 && f.sd_active && !f.data_active && !f.handle);
  if(f.release_sd_failure){f.forbid=true;return false;}
  f.sd_active=false;
 }
 memset(grant,0,sizeof(*grant));return true;
}
static bool diagnostic(const char *line) {(void)line;++diagnostic_calls;assert(!"archive must not log");return false;}
static bool durable_matches(uint32_t boot,uint32_t sequence) {
 static crash_report_spool snapshot;memset(&snapshot,0,sizeof(snapshot));
 if(!f.data.present)return false;
 snapshot.size=f.data.size;memcpy(snapshot.bytes,f.data.bytes,f.data.size);
 if(crash_report_spool_validate(&snapshot)!=CRASH_REPORT_SPOOL_OK)return false;
 for(unsigned i=0;i<snapshot.count;++i) {
  crash_report_spool_view v;assert(crash_report_spool_peek(&snapshot,i,&v)==CRASH_REPORT_SPOOL_OK);
  if(v.identity.captured_boot==boot && v.identity.captured_sequence==sequence &&
     !memcmp(v.identity.firmware_sha256,failure_archive_identity.firmware_sha256,32) &&
     v.text_size==failure_archive_text_size && !memcmp(v.text,failure_archive_text,v.text_size))return true;
 }
 return false;
}
static int32_t acknowledge(uint64_t invocation,uint32_t boot,uint32_t sequence) {
 event(ACK);assert(!f.data_active && !f.sd_active && !f.handle);
 assert(invocation==41 && failure_archive_presented && failure_archive_committed);
 assert(durable_matches(boot,sequence));++f.acks;
 if(f.ack_fault)return f.ack_fault;
 if(boot!=f.native_boot || sequence!=f.native_sequence)return RISC_FAILURE_EVIDENCE_STALE;
 f.native_pending=false;return RISC_FAILURE_EVIDENCE_OK;
}
static risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),
 .diagnostic=diagnostic,.acquire=acquire,.release=release};
static risc_failure_evidence_client_v1 client={.api_version=1,.struct_size=sizeof(client),
 .invocation=41,.acknowledge=acknowledge};
static void reset_lifecycle(void) {
 memset(&failure_archive_spool,0,sizeof(failure_archive_spool));
 memset(&failure_archive_sd,0,sizeof(failure_archive_sd));
 memset(&failure_archive_data_grant,0,sizeof(failure_archive_data_grant));
 memset(&failure_archive_sd_grant,0,sizeof(failure_archive_sd_grant));
 memset(&failure_archive_client,0,sizeof(failure_archive_client));
 memset(&failure_archive_identity,0,sizeof(failure_archive_identity));
 memset(failure_archive_text,0,sizeof(failure_archive_text));
 failure_archive_text_size=failure_archive_attempted_at=failure_archive_drops=0;
 failure_archive_initialized=failure_archive_probed=failure_archive_backlog=false;
 failure_archive_pending=failure_archive_presented=failure_archive_committed=false;
 failure_archive_attempted=failure_archive_busy=failure_archive_fault=false;
 native_custody_retained=failed=resident_child_active=resident_dispatching=false;
 alarm_modal=quick_modal=resident_launch_pending=resident_sleep_pending=false;
 surface.frame=false;paper_token=0;quick.ui=false;display_settled=true;
 resident_prior_failure_checked=contexts_ok=broadcast_ok=true;
 f.forbid=f.data_active=f.sd_active=false;f.handle=0;
 for(unsigned i=0;i<2;++i)f.lease[i].live=false;
 retained_calls=diagnostic_calls=0;rt=&runtime;ticks=1000;
}
static void reset(void) {
 memset(&f,0,sizeof(f));f.data.available=f.sd_available=true;f.data.revision=71;
 for(unsigned i=0;i<2;++i)f.lease[i].api=(risc_app_data_v1){1,sizeof(risc_app_data_v1),&f.lease[i],data_stat,data_read,data_replace};
 f.volume.base=(risc_storage_volume_api_v1){.api_version=1,.struct_size=sizeof(f.volume),.context=&f,
 .ready=sd_ready,.stat=sd_stat,.file_read=sd_read,.file_write=sd_write,.file_close=sd_close,.remove=sd_remove};
 f.volume.file_open=sd_open;f.volume.file_info=sd_info;f.volume.file_sync=sd_sync;
 f.volume.handle_error=sd_error;f.volume.mkdir=sd_mkdir;f.volume.rename=sd_rename;
 f.checked.prepared.base.sleep.terminal.power.volume=f.volume;
 f.checked.prepared.base.sleep.terminal.power.volume.base.struct_size=sizeof(f.checked);
 f.checked.state_tag=RISC_STORAGE_STATE_TAG;f.checked.state_version=1;f.checked.observe=sd_observe;
 reset_lifecycle();
}
static risc_failure_evidence_v1 record(uint32_t sequence) {
 risc_failure_evidence_v1 r={0};r.api_version=1;r.struct_size=sizeof(r);
 r.record_boot=7;r.record_sequence=sequence;r.kind=RISC_FAILURE_NATIVE_PANIC;
 r.flags=RISC_FAILURE_PENDING|RISC_FAILURE_CONTEXT|RISC_FAILURE_REGISTERS;
 r.phase=RISC_FAILURE_PHASE_MAIN;r.invocation=987654321;r.current_reset_reason=12;
 r.pc=0x42000123;r.sp=0x3fc90000;r.stack_status=RISC_FAILURE_STACK_LIMIT;
 strcpy(r.application,"apps/example.elf");strcpy(r.detail,"Copied crash\ncontrol\ttext");
 for(unsigned i=0;i<32;++i)r.firmware_sha256[i]=(uint8_t)(i*7+3);
 r.history_count=8;r.frame_count=8;
 for(unsigned i=0;i<8;++i) {
  r.history[i]=(risc_failure_step_v1){101+i,1+i,201+i};
  r.frames[i]=(risc_failure_frame_v1){0x42010000+i*32,0x3fca0000+i*64};
 }
 return r;
}
static bool capture(uint32_t sequence) {
 risc_failure_evidence_v1 r=record(sequence);f.native_boot=r.record_boot;
 f.native_sequence=r.record_sequence;f.native_pending=true;
 return failure_archive_capture(&r,&client);
}
static unsigned disk_count(void) {
 assert(f.data.present);return crash_report_spool_u32(f.data.bytes+16);
}
typedef struct {char bytes[4097];size_t used;} report_buffer;
static bool report_line(void *context,const char *line) {
 report_buffer *out=context;size_t n=strlen(line);assert(out->used+n+1<sizeof(out->bytes));
 memcpy(out->bytes+out->used,line,n);out->used+=n;out->bytes[out->used++]='\n';out->bytes[out->used]=0;return true;
}
static report_buffer format_record(const risc_failure_evidence_v1 *r) {
 report_buffer out={{0},0};
 assert(report_line(&out,"CRASH / RESTART"));assert(failure_evidence_report_format(r,report_line,&out));return out;
}
static report_buffer expected_report(uint32_t sequence) {
 risc_failure_evidence_v1 r=record(sequence);return format_record(&r);
}
static unsigned assert_final_record(const risc_failure_evidence_v1 *record) {
 report_buffer text=format_record(record);risc_failure_evidence_v1 r=*record;
 if(f.data.present && crash_report_spool_u32(f.data.bytes+20)) {
  int length=snprintf(text.bytes+text.used,sizeof(text.bytes)-text.used,
   "Report queue overflow: %lu unsaved report(s) dropped.\nLast dropped record: boot %lu / sequence %lu.\n",
   (unsigned long)crash_report_spool_u32(f.data.bytes+20),
   (unsigned long)crash_report_spool_u32(f.data.bytes+56),(unsigned long)crash_report_spool_u32(f.data.bytes+60));
  assert(length>0 && (size_t)length<sizeof(text.bytes)-text.used);text.used+=(size_t)length;
 }
 char firmware[65],path[128];static const char hex[]="0123456789abcdef";
 for(unsigned i=0;i<32;++i){firmware[i*2]=hex[r.firmware_sha256[i]>>4];firmware[i*2+1]=hex[r.firmware_sha256[i]&15];}
 firmware[64]=0;assert(crash_report_sd_path(path,sizeof(path),firmware,r.record_boot,r.record_sequence,
  crash_report_sd_crc32(text.bytes,text.used),false));
 unsigned n=file_find(path);assert(n<FILES && f.files[n].size==text.used);
 assert(!memcmp(f.files[n].bytes,text.bytes,text.used) && f.files[n].synced);return n;
}
static unsigned assert_final(uint32_t sequence) {
 risc_failure_evidence_v1 r=record(sequence);return assert_final_record(&r);
}
static void finish_presentation(void) {
 assert(!native_custody_retained && !failed && !surface.frame && !paper_token && display_settled);
 assert(failure_archive_acknowledge()==RISC_FAILURE_EVIDENCE_OK);
}
static void assert_idle_grants(void) {assert(!f.data_active && !f.sd_active && !f.handle);}
static void pass(void) {assert(!diagnostic_calls);++tests;printf("PASS %s\n",test_name);}

static void test_exact_text_and_ack_barrier(void) {
 test_name="exact formatter to durable spool to verified SD; presentation acknowledgement barrier";
 reset();f.sd_available=false;assert(capture(1));report_buffer expected=expected_report(1);
 assert(failure_archive_committed && failure_archive_pending && !failure_archive_presented);
 assert(failure_archive_text_size==expected.used && !memcmp(failure_archive_text,expected.bytes,expected.used));
 assert(durable_matches(7,1) && !f.acks && !f.counts[ACQUIRE_SD]);
 /* An explicit checkpoint before final Continue cannot acknowledge even when
  * internal storage is durable. The separate OSD suite covers physical pages. */
 assert(failure_archive_checkpoint());assert(!f.acks && f.native_pending);
 finish_presentation();assert(f.acks==1 && !f.native_pending && !failure_archive_pending);
 f.sd_available=true;ticks+=30000;assert(failure_archive_checkpoint());assert_final(1);
 assert(disk_count()==0 && f.data.removals==1 && f.deletion_serial>f.verified_serial);
 assert_idle_grants();pass();
}
static void test_deferred_media_continue(void) {
 test_name="no SD, read-only SD and USB-exported SD preserve spool and usable Continue";
 for(unsigned mode=0;mode<3;++mode) {
  reset();f.sd_available=mode!=0;f.sd_readonly=mode==1;f.sd_exported=mode==2;f.sd_acquire_denied=mode==3;
  assert(capture(2));finish_presentation();assert(f.acks==1 && !f.native_pending);
  assert(failure_archive_checkpoint());assert(disk_count()==1 && !f.data.removals && !native_custody_retained);
  assert_idle_grants();f.sd_available=true;f.sd_readonly=f.sd_exported=f.sd_acquire_denied=false;
  ticks+=30000;assert(failure_archive_checkpoint());assert_final(2);assert(disk_count()==0);assert_idle_grants();
 }
 pass();
}
static void test_no_spool_continue_and_late_backup(void) {
 test_name="unavailable internal spool leaves native evidence pending; later safe checkpoint backs up then acknowledges";
 reset();f.data.available=false;assert(capture(3));assert(!failure_archive_committed);
 failure_evidence_layout status={{0},0};failure_archive_report_status(&status);
 assert(strstr(status.text,"backup deferred"));finish_presentation();
 assert(!f.acks && f.native_pending && failure_archive_pending && failure_archive_presented);
 assert(failure_archive_checkpoint());assert(!f.acks && !f.counts[ACQUIRE_SD]);
 f.data.available=true;ticks+=30000;assert(failure_archive_checkpoint());
 assert(f.acks==1 && !f.native_pending && disk_count()==0);assert_final(3);assert_idle_grants();pass();
}
static void test_restart_and_deduplicate(void) {
 test_name="restart reacquires fresh AppData generations; duplicate append and final-file retry avoid rewrites";
 reset();assert(capture(4));unsigned writes=f.data.replacements;uint64_t old_revision=f.data.revision;
 /* A real lifecycle reset loses RAM only, retaining durable file and native pending evidence. */
 reset_lifecycle();assert(capture(4));assert(f.data.replacements==writes && f.data.revision!=old_revision);
 assert(disk_count()==1 && f.native_pending && !f.acks);finish_presentation();
 f.data.replace_fault=RISC_APP_DATA_IO;
 assert(failure_archive_checkpoint());assert_final(4);assert(disk_count()==1 && f.data.removals==1);
 unsigned sd_writes=f.final_writes,renames=f.renames;
 reset_lifecycle();assert(failure_archive_checkpoint());assert(disk_count()==0);
 assert(f.final_writes==sd_writes && f.renames==renames && f.acks==1);assert_idle_grants();pass();
}
static void test_same_identity_distinct_snapshots(void) {
 test_name="same native identity with different exact text remains two durable reports; exact duplicate does not append";
 reset();assert(capture(22));finish_presentation();
 risc_failure_evidence_v1 changed=record(22);changed.current_reset_reason=42;
 strcpy(changed.detail,"Different cold-start snapshot sharing numeric identity");
 f.native_pending=true;assert(failure_archive_capture(&changed,&client));finish_presentation();
 assert(disk_count()==2 && f.acks==2);unsigned writes=f.data.replacements;
 assert(failure_archive_capture(&changed,&client));assert(disk_count()==2 && f.data.replacements==writes);
 finish_presentation();assert(failure_archive_checkpoint());unsigned first=assert_final(22);
 assert(disk_count()==1);assert(failure_archive_checkpoint());unsigned second=assert_final_record(&changed);
 assert(first!=second && disk_count()==0);assert_idle_grants();pass();
}
static void test_final_verify_before_remove(void) {
 test_name="failed final SD readback retains queue; exact final readback precedes deletion";
 reset();assert(capture(5));finish_presentation();f.final_read_corrupt=true;
 assert(failure_archive_checkpoint());assert(disk_count()==1 && !f.data.removals && !f.final_verified);
 assert_final(5);unsigned writes=f.final_writes;
 f.final_read_corrupt=false;ticks+=30000;assert(failure_archive_checkpoint());
 assert(disk_count()==0 && f.data.removals==1 && f.final_writes==writes);
 assert(f.deletion_serial>f.verified_serial && !native_custody_retained);assert_idle_grants();pass();
}
static void test_full_queue_and_drop_history(void) {
 test_name="eight-report bound keeps oldest seven plus newest; drop identity/counter survive drain and restart";
 reset();f.sd_available=false;
 for(unsigned n=1;n<=10;++n){assert(capture(n));finish_presentation();}
 assert(disk_count()==8 && failure_archive_drops==2 && f.acks==10);
 assert(failure_archive_spool.dropped_count==2 && failure_archive_spool.last_dropped_identity.captured_sequence==9);
 for(unsigned n=0;n<8;++n) {
  crash_report_spool_view view;assert(crash_report_spool_peek(&failure_archive_spool,n,&view)==CRASH_REPORT_SPOOL_OK);
  assert(view.identity.captured_sequence==(n<7?n+1:10));
 }
 failure_evidence_layout status={{0},0};failure_archive_report_status(&status);
 assert(strstr(status.text,"2 unsaved report(s) dropped"));
 reset_lifecycle();f.sd_available=true;
 for(unsigned n=0;n<8;++n) {
  unsigned before=disk_count();assert(failure_archive_checkpoint());assert(disk_count()==before-1);
  assert_final(n<7?n+1:10); /* Exactly one transaction per checkpoint. */
 }
 assert(crash_report_spool_u32(f.data.bytes+20)==2 && failure_archive_drops==2);
 reset_lifecycle();assert(failure_archive_checkpoint());assert(failure_archive_drops==2 && !failure_archive_backlog);
 assert_idle_grants();
 /* A valid persisted near-saturated counter must never wrap on new drops. */
 reset();for(unsigned n=1;n<=8;++n){assert(capture(n));finish_presentation();}
 memcpy(f.data.bytes+24,f.data.bytes+failure_archive_spool.offsets[7],40);
 crash_report_spool_put32(f.data.bytes+20,UINT32_MAX-1);
 crash_report_spool_put32(f.data.bytes+76,crash_report_spool_crc(f.data.bytes,f.data.size));
 assert(capture(9));finish_presentation();assert(failure_archive_drops==UINT32_MAX);
 assert(capture(10));finish_presentation();assert(failure_archive_drops==UINT32_MAX);
 assert(failure_archive_spool.last_dropped_identity.captured_sequence==9);
 reset_lifecycle();f.sd_available=false;assert(failure_archive_checkpoint());
 assert(failure_archive_drops==UINT32_MAX && disk_count()==8);assert_idle_grants();pass();
}
static void test_corrupt_io_and_ambiguous_commit(void) {
 test_name="corrupt/IO/COMMIT_UNKNOWN fail closed; only exact readback resolves ambiguous commit";
 reset();assert(capture(11));finish_presentation();f.data.bytes[f.data.size-1]^=0x40;
 unsigned writes=f.data.replacements;assert(capture(12));finish_presentation();
 assert(f.acks==1 && f.native_pending && !failure_archive_committed && failure_archive_fault);
 assert(f.data.replacements==writes && !f.counts[ACQUIRE_SD]);
 assert(failure_archive_checkpoint());assert(f.data.replacements==writes && !f.counts[ACQUIRE_SD]);
 reset();assert(capture(11));finish_presentation();f.data.read_fault=RISC_APP_DATA_IO;
 writes=f.data.replacements;assert(capture(12));finish_presentation();
 assert(f.acks==1 && f.native_pending && !failure_archive_committed && f.data.replacements==writes);
 assert(failure_archive_checkpoint());assert(f.acks==2 && !f.native_pending && disk_count()==1);
 assert_final(11);assert(failure_archive_checkpoint());assert_final(12);assert(disk_count()==0);
 for(unsigned mode=0;mode<6;++mode) {
  reset();
  if(mode==0)f.data.stat_fault=RISC_APP_DATA_IO;
  if(mode==1)f.data.replace_fault=RISC_APP_DATA_IO;
  if(mode>=2){f.data.replace_fault=RISC_APP_DATA_COMMIT_UNKNOWN;f.data.commit_on_fault=mode>=3;}
  if(mode==4)f.data.post_replace_read_fault=RISC_APP_DATA_IO;
  if(mode==5)f.data.corrupt_on_fault=true;
  assert(capture(13));bool committed=mode==3;assert(failure_archive_committed==committed);
  finish_presentation();assert(f.acks==(unsigned)committed && f.native_pending!=committed);
  assert_idle_grants();
  if(mode!=5) {
   ticks+=30000;assert(failure_archive_checkpoint());assert(f.acks==1 && !f.native_pending);
   assert_final(13);assert(disk_count()==0);
  }else {assert(failure_archive_fault && f.data.removals==0);}
 }
 pass();
}
static void test_ambiguous_removal(void) {
 test_name="ambiguous internal removal reconciles disk before reporting empty; retry deduplicates SD";
 for(unsigned committed=0;committed<2;++committed) {
  reset();assert(capture(14));finish_presentation();f.data.replace_fault=RISC_APP_DATA_COMMIT_UNKNOWN;
  f.data.commit_on_fault=committed!=0;assert(failure_archive_checkpoint());assert_final(14);
  assert(disk_count()==(committed?0u:1u));unsigned writes=f.final_writes;
  if(!committed){ticks+=30000;assert(failure_archive_checkpoint());assert(disk_count()==0 && f.final_writes==writes);}
  assert_idle_grants();
 }
 pass();
}
static void test_retained_is_terminal(void) {
 test_name="AppData retention, uncertain releases and hidden SD stat/mkdir/close/read/remove retention stop every subsequent I/O";
 for(unsigned mode=0;mode<10;++mode) {
  reset();
  if(mode==0)f.data.stat_fault=RISC_APP_DATA_RETAINED;
  if(mode==1)f.release_data_failure=true;
  bool captured=capture(15);
  if(mode<2)assert(!captured);
  else {
   assert(captured);finish_presentation();
   f.close_failure=mode==2;f.release_sd_failure=mode==3;f.retain_on_sd_read=mode==4;
   if(mode>=5 && mode<=7)f.hidden_stat_at=mode-4;
   f.hidden_mkdir_failure=mode==8;f.short_write=f.remove_failure=mode==9;
   assert(!failure_archive_checkpoint());
  }
  assert(native_custody_retained && f.forbid && !f.data.removals);
  unsigned events=f.events;
  for(unsigned n=0;n<20;++n){ticks+=30000;assert(!failure_archive_checkpoint());(void)failure_archive_save_pending();}
  assert(f.events==events);assert(f.data.present || mode==0);
 }
 pass();
}
static void test_foreground_dispatch_display_modal_gates(void) {
 test_name="foreground, dispatch, active frame/token, unsettled display, modal, quick UI and handoff gates perform no I/O";
 reset();assert(capture(16));finish_presentation();
 bool *positive[]={&resident_child_active,&resident_dispatching,&surface.frame,&alarm_modal,&quick_modal,
                  &resident_launch_pending,&resident_sleep_pending,&quick.ui,&failure_archive_busy};
 for(unsigned i=0;i<sizeof(positive)/sizeof(positive[0]);++i) {
  *positive[i]=true;unsigned events=f.events;ticks+=30000;
  assert(failure_archive_checkpoint());assert(f.events==events);*positive[i]=false;
 }
 paper_token=1;unsigned events=f.events;assert(failure_archive_checkpoint());assert(f.events==events);paper_token=0;
 display_settled=false;assert(failure_archive_checkpoint());assert(f.events==events);display_settled=true;
 failed=true;assert(!failure_archive_checkpoint());assert(f.events==events);failed=false;
 /* Storage preparation can refuse cleanly without acquiring a provider. */
 contexts_ok=false;assert(failure_archive_checkpoint());assert(f.events==events);contexts_ok=true;
 ticks+=30000;broadcast_ok=false;assert(failure_archive_checkpoint());assert(f.events==events);broadcast_ok=true;
 ticks+=30000;assert(failure_archive_checkpoint());assert(disk_count()==0);assert_idle_grants();pass();
}
static void test_empty_steady_state_and_backoff(void) {
 test_name="empty queue has no steady-state I/O/logging; retry is exactly 30 seconds and handles clock wrap";
 reset();assert(failure_archive_checkpoint());assert(failure_archive_probed && !failure_archive_backlog);
 unsigned events=f.events;
 for(unsigned n=0;n<100000;++n){ticks+=10000;assert(failure_archive_checkpoint());}
 assert(f.events==events && !f.counts[ACQUIRE_SD] && !f.data.replacements);
 for(unsigned wrap=0;wrap<2;++wrap) {
  reset();assert(capture(17));finish_presentation();f.sd_available=false;
  ticks=wrap?UINT32_MAX-1000u:1000u;uint32_t start=ticks;
  assert(failure_archive_checkpoint());events=f.events;
  ticks=start+29999u;assert(failure_archive_checkpoint());assert(f.events==events);
  ticks=start+30000u;assert(failure_archive_checkpoint());assert(f.events>events);
  events=f.events;assert(failure_archive_checkpoint());assert(f.events==events);
  f.sd_available=true;ticks+=30000;assert(failure_archive_checkpoint());assert(disk_count()==0);
 }
 pass();
}
static void test_stale_ack_preserves_new_evidence(void) {
 test_name="stale delayed acknowledgement requests fresh native review and never dismisses the newer record";
 reset();f.data.available=false;assert(capture(18));finish_presentation();assert(!f.acks);
 f.native_sequence=19;f.native_pending=true;f.data.available=true;
 assert(failure_archive_checkpoint());assert(f.acks==1 && f.native_pending);
 assert(!resident_prior_failure_checked && !failure_archive_pending);assert_final(18);
 assert(capture(19));assert(f.acks==1 && f.native_pending);finish_presentation();
 assert(f.acks==2 && !f.native_pending);assert(failure_archive_checkpoint());assert_final(19);
 /* A non-stale acknowledgement failure leaves pending evidence for a retry. */
 reset();assert(capture(20));f.ack_fault=RISC_FAILURE_EVIDENCE_DENIED;
 assert(failure_archive_acknowledge()==RISC_FAILURE_EVIDENCE_DENIED);
 assert(f.native_pending && failure_archive_pending);
 assert(failure_archive_checkpoint());assert(!f.counts[ACQUIRE_SD] && disk_count()==1);
 f.ack_fault=0;ticks+=30000;assert(failure_archive_checkpoint());assert(!f.native_pending && disk_count()==0);
 assert_idle_grants();pass();
}
static void test_bounded_sd_attempt(void) {
 test_name="slow synchronous SD callbacks bound admission and retain spool for a later checkpoint";
 reset();assert(capture(21));finish_presentation();f.sd_duration=75;
 assert(failure_archive_checkpoint());assert(disk_count()==1 && !f.data.removals);
 assert(failure_archive_sd.operations<=CRASH_REPORT_SD_OPERATION_LIMIT);
 assert(!native_custody_retained);assert_idle_grants();
 f.sd_duration=0;ticks+=30000;assert(failure_archive_checkpoint());assert_final(21);assert(disk_count()==0);pass();
}
/* Fresh reconstruction of the suffix regression. The erased final fixture's
 * exact bytes are unavailable; this test is newly qualified against the
 * hash-matched recovered production implementation. */
static void test_checked_state_contract(void) {
 test_name="checked SD state: absent/malformed zero-I/O defer; first-ready/final-close retention; uncertain acquire";
 for(unsigned mode=0;mode<6;++mode) {
  reset();assert(capture(22));finish_presentation();
  if(mode==0)f.missing_state=true;
  if(mode==1)f.checked.state_tag=0;
  if(mode==2)f.checked.state_version=2;
  if(mode==3)f.checked.observe=NULL;
  if(mode==4)f.sd_available=false;
  if(mode==5)f.sd_exported=true;
  assert(failure_archive_checkpoint());assert(disk_count()==1 && !f.data.removals);
  for(unsigned op=SD_READY;op<KINDS;++op)assert(!f.counts[op]);
  assert_idle_grants();assert(!native_custody_retained);
 }
 for(unsigned mode=0;mode<4;++mode) {
  reset();assert(capture(23));finish_presentation();
  f.poison_first_ready=mode==0;f.poison_final_close=mode==1;
  f.sd_poisoned=mode==2;f.sd_acquire_denied=mode==3;
  assert(!failure_archive_checkpoint());assert(native_custody_retained && !f.data.removals && disk_count()==1);
  assert(!f.counts[RELEASE_SD]);
  if(mode==0)assert(f.counts[SD_READY]==1 && !f.counts[SD_STAT]);
  if(mode==1)assert(f.final_verified && !f.handle);
  if(mode>=2)for(unsigned op=SD_READY;op<KINDS;++op)assert(!f.counts[op]);
  unsigned events=f.events;for(unsigned i=0;i<3;++i){ticks+=30000;assert(!failure_archive_checkpoint());}assert(f.events==events);
 }
 pass();
}
int main(void) {
 setvbuf(stdout,NULL,_IONBF,0);
 test_exact_text_and_ack_barrier();test_deferred_media_continue();test_no_spool_continue_and_late_backup();
 test_restart_and_deduplicate();test_same_identity_distinct_snapshots();test_final_verify_before_remove();test_full_queue_and_drop_history();
 test_corrupt_io_and_ambiguous_commit();test_ambiguous_removal();test_retained_is_terminal();
 test_foreground_dispatch_display_modal_gates();test_empty_steady_state_and_backoff();
 test_stale_ack_preserves_new_evidence();test_bounded_sd_attempt();test_checked_state_contract();
 printf("Failure archive lifecycle: %u integrated scenarios PASS (host capability boundaries; no hardware)\n",tests);
 return 0;
}
