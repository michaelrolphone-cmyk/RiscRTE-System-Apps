#include "CrashReportSd.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

#define FILES 8u
#define EVENTS 128u
static const char sha[]="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
static unsigned char report_text[CRASH_REPORT_SD_TEXT_MAX];

typedef enum { READY, STAT, MKDIR, OPEN, WRITE, ERROR, SYNC, CLOSE, READ, INFO, RENAME, REMOVE, KINDS } kind;
typedef struct {bool used;char path[CRASH_REPORT_SD_PATH_CAPACITY];unsigned char data[4097];size_t size;bool synced;} fake_file;
typedef struct {
 risc_storage_volume_api_v1_ext api;
 fake_file files[FILES];
 bool directory,available,readonly,exported,full,poisoned,forbid,unsafe;
 unsigned calls,counts[KINDS],fail_call,short_call,corrupt_call,hidden_call,unsafe_at,safe_calls;
 unsigned failures,removes,opens,closes,writes,renames,hidden_ready;
 unsigned duration,budget_expired_calls;
 uint64_t now;
 kind events[EVENTS];
 char paths[EVENTS][CRASH_REPORT_SD_PATH_CAPACITY];
 unsigned handle,slot;
 size_t position;
 uint32_t flags,error;
 unsigned close_failure_at,remove_failure_at,rename_collision_at;
 bool rename_commit_failure,clean_remove_refusal;
} fake;

static unsigned find_file(fake *f,const char *path) {
 for(unsigned i=0;i<FILES;++i)if(f->files[i].used && !strcmp(f->files[i].path,path))return i;
 return FILES;
}
static unsigned add_file(fake *f,const char *path,const void *data,size_t length) {
 assert(find_file(f,path)==FILES && length<=sizeof(f->files[0].data));
 for(unsigned i=0;i<FILES;++i)if(!f->files[i].used) {
  fake_file *file=&f->files[i];memset(file,0,sizeof(*file));file->used=true;
  strcpy(file->path,path);file->size=length;if(length)memcpy(file->data,data,length);
  return i;
 }
 assert(!"fake file capacity exceeded");return FILES;
}
static bool event(fake *f,kind operation,const char *path) {
 assert(!f->forbid && !f->unsafe);
 assert(f->calls<EVENTS);
 f->events[f->calls]=operation;if(path)strcpy(f->paths[f->calls],path);
 ++f->calls;++f->counts[operation];f->now+=f->duration;
 bool fail=f->calls==f->fail_call;
 if(fail)++f->failures;
 return fail;
}
static bool ready(void *context) {
 fake *f=context;bool fail=event(f,READY,NULL);
 if(f->poisoned) {++f->hidden_ready;f->forbid=true;return false;}
 return !fail && f->available && !f->exported;
}
static bool stat_path(void *context,const char *path,uint64_t *size,bool *directory) {
 fake *f=context;if(event(f,STAT,path))return false;
 if(f->calls==f->hidden_call) {f->poisoned=true;return false;}
 if(!strcmp(path,"/CrashReports")) {*directory=true;*size=0;return f->directory;}
 unsigned slot=find_file(f,path);if(slot==FILES)return false;
 *size=f->files[slot].size;*directory=false;return true;
}
static bool mkdir_path(void *context,const char *path) {
 fake *f=context;assert(!strcmp(path,"/CrashReports"));
 if(event(f,MKDIR,path) || f->readonly || f->directory)return false;
 f->directory=true;
 if(f->calls==f->hidden_call) {f->poisoned=true;return false;}
 return true;
}
static uint32_t open_file(void *context,const char *path,uint32_t flags) {
 fake *f=context;bool fail=event(f,OPEN,path);assert(!f->handle);
 ++f->opens;
 if(fail)return 0;
 unsigned slot=find_file(f,path);
 if(flags & RISC_STORAGE_OPEN_WRITE) {
  assert(flags==(RISC_STORAGE_OPEN_WRITE|RISC_STORAGE_OPEN_CREATE|RISC_STORAGE_OPEN_EXCLUSIVE));
  assert(strstr(path,".tmp") && !strstr(path,".txt"));
  if(f->readonly || slot!=FILES)return 0;
  slot=add_file(f,path,NULL,0);
 }else {
  assert(flags==RISC_STORAGE_OPEN_READ);
  if(slot==FILES)return 0;
 }
 f->handle=17;f->slot=slot;f->position=0;f->flags=flags;f->error=0;
 if(f->calls==f->hidden_call) {f->poisoned=true;return 0;}
 return f->handle;
}
static size_t write_file(void *context,uint32_t handle,const void *data,size_t length) {
 fake *f=context;bool fail=event(f,WRITE,NULL);
 assert(handle==f->handle && length && length<=512 && (f->flags&RISC_STORAGE_OPEN_WRITE));
 ++f->writes;assert(f->position+length<=4096);
 if(fail)return 0;
 if(f->calls==f->short_call || f->full) {--length;f->error=1;}
 fake_file *file=&f->files[f->slot];memcpy(file->data+f->position,data,length);
 f->position+=length;file->size=f->position;file->synced=false;return length;
}
static size_t read_file(void *context,uint32_t handle,void *data,size_t length) {
 fake *f=context;bool fail=event(f,READ,NULL);
 assert(handle==f->handle && length && length<=512 && (f->flags&RISC_STORAGE_OPEN_READ));
 if(fail)return 0;
 fake_file *file=&f->files[f->slot];
 if(length>file->size-f->position)length=file->size-f->position;
 if(f->calls==f->short_call && length)--length;
 if(length)memcpy(data,file->data+f->position,length);
 if(f->calls==f->corrupt_call && length)((unsigned char *)data)[0]^=0x80;
 f->position+=length;return length;
}
static bool info_file(void *context,uint32_t handle,uint64_t *size,uint64_t *position) {
 fake *f=context;assert(handle==f->handle);if(event(f,INFO,NULL))return false;
 *size=f->files[f->slot].size;*position=f->position;return true;
}
static uint32_t error_file(void *context,uint32_t handle,bool directory) {
 fake *f=context;assert(handle==f->handle && !directory);
 return event(f,ERROR,NULL)?1:f->error;
}
static bool sync_file(void *context,uint32_t handle) {
 fake *f=context;assert(handle==f->handle);if(event(f,SYNC,NULL))return false;
 if(f->error)return false;
 f->files[f->slot].synced=true;return true;
}
static bool close_file(void *context,uint32_t handle,bool commit) {
 fake *f=context;assert(handle==f->handle && commit);bool fail=event(f,CLOSE,NULL);++f->closes;
 if(fail || f->closes==f->close_failure_at) {f->forbid=true;return false;}
 f->handle=0;return true;
}
static bool remove_path(void *context,const char *path) {
 fake *f=context;assert(!f->handle && strstr(path,".tmp") && !strstr(path,".txt"));
 bool fail=event(f,REMOVE,path);++f->removes;
 unsigned slot=find_file(f,path);
 if(fail || f->removes==f->remove_failure_at) {f->poisoned=true;return false;}
 if(f->readonly || f->clean_remove_refusal || slot==FILES)return false;
 f->files[slot].used=false;return true;
}
static bool rename_path(void *context,const char *source,const char *destination) {
 fake *f=context;assert(!f->handle);bool fail=event(f,RENAME,source);++f->renames;
 assert(strstr(source,".tmp") && strstr(destination,".txt"));
 if(f->renames==f->rename_collision_at)add_file(f,destination,"different",9);
 if(fail || find_file(f,destination)!=FILES)return false;
 unsigned slot=find_file(f,source);assert(slot<FILES && f->files[slot].synced);
 strcpy(f->files[slot].path,destination);
 if(f->calls==f->hidden_call) {f->poisoned=true;return false;}
 return !f->rename_commit_failure;
}
static uint64_t now(void *context) {return ((fake *)context)->now;}
static bool safe(void *context) {
 fake *f=context;++f->safe_calls;
 if(f->safe_calls==f->unsafe_at)f->unsafe=true;
 return !f->unsafe;
}
static void init(fake *f) {
 memset(f,0,sizeof(*f));f->available=true;
 f->api.base.api_version=RISC_STORAGE_VOLUME_API_V1;f->api.base.struct_size=sizeof(f->api);
 f->api.base.context=f;f->api.base.ready=ready;f->api.base.stat=stat_path;
 f->api.base.file_read=read_file;f->api.base.file_write=write_file;f->api.base.file_close=close_file;
 f->api.base.remove=remove_path;f->api.file_open=open_file;f->api.file_info=info_file;
 f->api.file_sync=sync_file;f->api.handle_error=error_file;f->api.mkdir=mkdir_path;f->api.rename=rename_path;
}
static crash_report_sd_result run(fake *f,crash_report_sd_state *state,size_t length,uint32_t budget) {
 crash_report_sd_hooks hooks={f,now,safe,budget};
 return crash_report_sd_export(state,&f->api,sha,7,81,report_text,length,&hooks);
}
static void paths(char *final,char *temporary,size_t length) {
 uint32_t crc=crash_report_sd_crc32(report_text,length);
 assert(crash_report_sd_path(final,CRASH_REPORT_SD_PATH_CAPACITY,sha,7,81,crc,false));
 assert(crash_report_sd_path(temporary,CRASH_REPORT_SD_PATH_CAPACITY,sha,7,81,crc,true));
}
static void assert_saved(fake *f,size_t length) {
 char final[128],temporary[128];paths(final,temporary,length);
 unsigned slot=find_file(f,final);assert(slot<FILES && f->files[slot].size==length);
 assert(!memcmp(f->files[slot].data,report_text,length) && f->files[slot].synced);
 assert(!f->handle);
}
static void assert_retained(fake *f,crash_report_sd_state *state) {
 assert(state->retained);unsigned calls=f->calls,safe_calls=f->safe_calls;
 assert(run(f,state,4096,0)==CRASH_REPORT_SD_RETAINED);
 assert(f->calls==calls && f->safe_calls==safe_calls);
}
static void test_normal_and_dedupe(void) {
 for(size_t length=1;length<=4096;length=length==1?511:length==511?512:length==512?513:length==513?4095:4096) {
  fake f;init(&f);crash_report_sd_state state={0};
  assert(run(&f,&state,length,0)==CRASH_REPORT_SD_SAVED);assert_saved(&f,length);
  assert(f.calls==state.operations && state.operations<=CRASH_REPORT_SD_OPERATION_LIMIT);
  unsigned writes=f.writes,renames=f.renames;
  /* Dedupe never opens a writer or changes an existing final. */
  assert(run(&f,&state,length,0)==CRASH_REPORT_SD_SAVED);assert_saved(&f,length);
  assert(f.writes==writes && f.renames==renames);
  if(length==4096)break;
 }
}
static void test_unavailable_and_limits(void) {
 fake f;crash_report_sd_state state={0};
 init(&f);f.available=false;assert(run(&f,&state,4096,0)==CRASH_REPORT_SD_DEFERRED && f.calls==1);
 init(&f);f.exported=true;assert(run(&f,&state,4096,0)==CRASH_REPORT_SD_DEFERRED && f.calls==1);
 init(&f);f.readonly=true;assert(run(&f,&state,4096,0)==CRASH_REPORT_SD_DEFERRED && !f.handle);
 init(&f);f.directory=true;f.readonly=true;assert(run(&f,&state,4096,0)==CRASH_REPORT_SD_DEFERRED && !f.handle);
 assert(f.counts[READY]==4); /* Missing final/temp and clean denied open check readiness. */
 init(&f);f.full=true;assert(run(&f,&state,4096,0)==CRASH_REPORT_SD_DEFERRED && !f.handle && f.removes==1);
 init(&f);assert(run(&f,&state,4097,0)==CRASH_REPORT_SD_DEFERRED && !f.calls);
 assert(run(&f,&state,0,0)==CRASH_REPORT_SD_DEFERRED && !f.calls);
 crash_report_sd_hooks hooks={&f,now,safe,0};
 char bad_sha[65]="bad";
 assert(crash_report_sd_export(&state,&f.api,bad_sha,7,81,report_text,1,&hooks)==CRASH_REPORT_SD_DEFERRED && !f.calls);
 f.api.base.struct_size=sizeof(f.api.base);
 assert(run(&f,&state,1,0)==CRASH_REPORT_SD_DEFERRED && !f.calls);
 f.api.base.struct_size=sizeof(f.api);f.api.rename=NULL;
 assert(run(&f,&state,1,0)==CRASH_REPORT_SD_DEFERRED && !f.calls);
}
static void test_path_identity_and_collisions(void) {
 char final[128],temporary[128],other[128],upper[65];paths(final,temporary,4096);
 assert(!strncmp(final,"/CrashReports/",14) && strstr(final,sha)==final+14);
 assert(strlen(final)<RISC_STORAGE_VOLUME_NAME_MAX && strlen(temporary)==strlen(final));
 assert(strstr(final,"-00000007-00000051-"));
 assert(crash_report_sd_crc32("123456789",9)==UINT32_C(0xcbf43926));
 for(unsigned i=0;i<65;++i)upper[i]=sha[i]>='a' && sha[i]<='f'?(char)(sha[i]-32):sha[i];
 assert(crash_report_sd_path(other,128,upper,7,81,crash_report_sd_crc32(report_text,4096),false));
 assert(!strcmp(other,final));upper[63]='g';assert(!crash_report_sd_path(other,128,upper,7,81,0,false));
 assert(!crash_report_sd_path(other,100,sha,7,81,0,false));
 fake f;init(&f);f.directory=true;crash_report_sd_state state={0};
 unsigned slot=add_file(&f,final,report_text,4096);f.files[slot].data[0]^=0x40;
 assert(run(&f,&state,4096,0)==CRASH_REPORT_SD_DEFERRED && !f.handle && !f.writes && !f.removes);
 assert(f.files[slot].data[0]!=(unsigned char)report_text[0]);
 init(&f);f.directory=true;add_file(&f,final,"short",5);
 assert(run(&f,&state,4096,0)==CRASH_REPORT_SD_DEFERRED && !f.opens && !f.removes);
 init(&f);f.rename_collision_at=1;
 assert(run(&f,&state,4096,0)==CRASH_REPORT_SD_DEFERRED && !f.handle && f.removes==1);
 slot=find_file(&f,final);assert(slot<FILES && f.files[slot].size==9 && !memcmp(f.files[slot].data,"different",9));
}
static void test_interrupted_retry(void) {
 char final[128],temporary[128];paths(final,temporary,4096);
 fake f;init(&f);f.directory=true;crash_report_sd_state state={0};
 add_file(&f,temporary,"partial",7);
 unsigned unrelated=add_file(&f,"/CrashReports/unrelated.tmp","keep",4);
 unsigned old=add_file(&f,"/CrashReports/older.txt","keep",4);
 assert(run(&f,&state,4096,0)==CRASH_REPORT_SD_SAVED);assert_saved(&f,4096);
 assert(f.removes==1 && f.files[unrelated].used && f.files[old].used);
 assert(find_file(&f,temporary)==FILES);
 /* A reset after rename but before final verification is safely deduplicated. */
 init(&f);f.directory=true;unsigned slot=add_file(&f,final,report_text,4096);f.files[slot].synced=true;
 assert(run(&f,&state,4096,0)==CRASH_REPORT_SD_SAVED);assert_saved(&f,4096);assert(!f.writes && !f.renames);
}
static void test_every_clean_failure(const fake *baseline) {
 for(unsigned at=1;at<=baseline->calls;++at) {
  fake f;init(&f);f.fail_call=at;crash_report_sd_state state={0};
  crash_report_sd_result result=run(&f,&state,4096,0);
  assert(f.failures==1 && f.calls<=CRASH_REPORT_SD_OPERATION_LIMIT);
  if(result==CRASH_REPORT_SD_RETAINED) {
   assert(baseline->events[at-1]==CLOSE || baseline->events[at-1]==READY);assert_retained(&f,&state);
  }else {
   assert(!f.handle);
   if(result==CRASH_REPORT_SD_SAVED)assert_saved(&f,4096);
  }
 }
}
static void test_all_short_and_corrupt_transfers(const fake *baseline) {
 for(unsigned at=1;at<=baseline->calls;++at) {
  kind k=baseline->events[at-1];if(k!=WRITE && k!=READ)continue;
  for(unsigned corrupt=0;corrupt<2;++corrupt) {
   if(corrupt && k!=READ)continue;
   fake f;init(&f);if(corrupt)f.corrupt_call=at;else f.short_call=at;
   crash_report_sd_state state={0};crash_report_sd_result result=run(&f,&state,4096,0);
   assert(result!=CRASH_REPORT_SD_RETAINED && !f.handle);
   if(result==CRASH_REPORT_SD_SAVED) {
    /* The deliberate EOF probe has no byte to shorten/corrupt. */
    assert(baseline->events[at-2]==ERROR);assert_saved(&f,4096);
   }else assert(result==CRASH_REPORT_SD_DEFERRED);
  }
 }
}
static void test_all_safety_boundaries(const fake *baseline) {
 for(unsigned at=1;at<=baseline->safe_calls;++at) {
  fake f;init(&f);f.unsafe_at=at;crash_report_sd_state state={0};
  assert(run(&f,&state,4096,0)==CRASH_REPORT_SD_RETAINED);
  assert(f.unsafe && f.safe_calls==at);assert_retained(&f,&state);
 }
}
static void test_hidden_handles_and_cleanup(const fake *baseline) {
 /* Guard-leave failures during stat/mkdir can poison provider custody without
  * yielding a handle. No ordinary operation may follow the checked ready=false. */
 for(unsigned at=1;at<=baseline->calls;++at) {
  kind k=baseline->events[at-1];if(k!=STAT && k!=MKDIR && k!=RENAME)continue;
  fake f;init(&f);f.hidden_call=at;crash_report_sd_state state={0};
  assert(run(&f,&state,4096,0)==CRASH_REPORT_SD_RETAINED);
  assert(!f.handle && f.poisoned && f.hidden_ready==1 && f.calls==at+1);
  assert(f.events[at]==READY);assert_retained(&f,&state);
 }
 for(unsigned at=1;at<=baseline->calls;++at)if(baseline->events[at-1]==OPEN) {
  fake f;init(&f);f.hidden_call=at;crash_report_sd_state state={0};
  assert(run(&f,&state,4096,0)==CRASH_REPORT_SD_RETAINED);
  assert(f.handle && f.poisoned && f.hidden_ready==1);assert_retained(&f,&state);
 }
 /* A failed checked close and failed exact-temp removal both latch forever. */
 for(unsigned close_at=1;close_at<=3;++close_at) {
  fake f;init(&f);f.close_failure_at=close_at;crash_report_sd_state state={0};
  assert(run(&f,&state,4096,0)==CRASH_REPORT_SD_RETAINED && f.handle);assert_retained(&f,&state);
 }
 fake f;init(&f);f.full=true;f.remove_failure_at=1;crash_report_sd_state state={0};
 assert(run(&f,&state,4096,0)==CRASH_REPORT_SD_RETAINED && !f.handle);assert_retained(&f,&state);
 init(&f);state=(crash_report_sd_state){0};f.directory=true;f.remove_failure_at=1;
 char final[128],temporary[128];paths(final,temporary,4096);add_file(&f,temporary,"partial",7);
 assert(run(&f,&state,4096,0)==CRASH_REPORT_SD_RETAINED && !f.handle);assert_retained(&f,&state);
 /* Rename returning false after a mutation cannot justify deleting a final or
  * claiming success. Confirmed ready custody can defer after a refused
  * derived-temp unlink; the report remains in the internal spool. */
 unsigned char spool_before[sizeof(report_text)];memcpy(spool_before,report_text,sizeof(report_text));
 init(&f);state=(crash_report_sd_state){0};f.rename_commit_failure=true;
 assert(run(&f,&state,4096,0)==CRASH_REPORT_SD_DEFERRED && !f.handle && !state.retained);
 assert(f.removes==1 && find_file(&f,final)<FILES && find_file(&f,temporary)==FILES);
 assert(!memcmp(spool_before,report_text,sizeof(report_text)));
}
static void test_clean_cleanup_refusal(void) {
 char final[128],temporary[128];paths(final,temporary,4096);
 unsigned char spool_before[sizeof(report_text)];memcpy(spool_before,report_text,sizeof(report_text));
 /* Existing staging on read-only media does not open a handle or freeze Home. */
 fake f;init(&f);f.directory=true;f.readonly=true;
 unsigned slot=add_file(&f,temporary,"partial",7);crash_report_sd_state state={0};
 assert(run(&f,&state,4096,0)==CRASH_REPORT_SD_DEFERRED);
 assert(!state.retained && !f.handle && !f.opens && f.removes==1);
 assert(f.files[slot].used && f.files[slot].size==7 && !memcmp(f.files[slot].data,"partial",7));
 assert(f.events[f.calls-1]==READY && !memcmp(spool_before,report_text,sizeof(report_text)));
 f.readonly=false;assert(run(&f,&state,4096,0)==CRASH_REPORT_SD_SAVED);assert_saved(&f,4096);
 assert(f.removes==2); /* A subsequent attempt can retry after the clean defer. */
 /* A clean refusal after a full-media short write also checks/closes once,
  * leaves only its own staging file, and never retries the refused cleanup. */
 init(&f);f.full=true;f.clean_remove_refusal=true;state=(crash_report_sd_state){0};
 assert(run(&f,&state,4096,0)==CRASH_REPORT_SD_DEFERRED);
 assert(!state.retained && !f.handle && f.removes==1 && f.closes==1);
 assert(find_file(&f,temporary)<FILES && find_file(&f,final)==FILES);
 assert(f.events[f.calls-1]==READY && !memcmp(spool_before,report_text,sizeof(report_text)));
 /* Expired admission still has room for close + refused unlink + readiness. */
 init(&f);f.duration=1;f.clean_remove_refusal=true;state=(crash_report_sd_state){0};
 assert(run(&f,&state,4096,12)==CRASH_REPORT_SD_DEFERRED);
 assert(!state.retained && !f.handle && f.removes==1 && f.closes==1);
 assert(f.calls<=12+CRASH_REPORT_SD_CLEANUP_RESERVE && f.events[f.calls-1]==READY);
 /* Safety still guards both sides of the new cleanup-readiness callback. */
 fake baseline;init(&baseline);baseline.full=true;baseline.clean_remove_refusal=true;
 state=(crash_report_sd_state){0};assert(run(&baseline,&state,4096,0)==CRASH_REPORT_SD_DEFERRED);
 for(unsigned at=1;at<=baseline.safe_calls;++at) {
  init(&f);f.full=true;f.clean_remove_refusal=true;f.unsafe_at=at;state=(crash_report_sd_state){0};
  assert(run(&f,&state,4096,0)==CRASH_REPORT_SD_RETAINED);assert_retained(&f,&state);
 }
}
static void test_cleanup_safety_and_operation_reserve(void) {
 fake full;init(&full);full.full=true;crash_report_sd_state state={0};
 assert(run(&full,&state,4096,0)==CRASH_REPORT_SD_DEFERRED && full.removes==1);
 /* Both sides of cleanup callbacks must stop immediately on unsafe custody. */
 for(unsigned at=1;at<=full.safe_calls;++at) {
  fake f;init(&f);f.full=true;f.unsafe_at=at;state=(crash_report_sd_state){0};
  assert(run(&f,&state,4096,0)==CRASH_REPORT_SD_RETAINED);assert_retained(&f,&state);
 }
 /* Force the count limit directly: ordinary work cannot consume the reserve;
  * an already owned file can still be closed and its derived .tmp removed. */
 fake f;init(&f);char final[128],temporary[128];paths(final,temporary,4096);
 state=(crash_report_sd_state){false,CRASH_REPORT_SD_OPERATION_LIMIT-CRASH_REPORT_SD_CLEANUP_RESERVE};
 crash_report_sd_hooks hooks={&f,now,safe,0};
 crash_report_sd_attempt a={&state,&f.api,&hooks,0,250,17,true,false,temporary};
 f.slot=add_file(&f,temporary,"partial",7);f.handle=17;
 assert(!crash_report_sd_admit(&a,false) && !state.retained && !f.calls);
 assert(crash_report_sd_close(&a) && crash_report_sd_remove_temporary(&a));
 assert(f.calls==2 && !f.handle && !a.temporary_owned && !state.retained);
 state.operations=CRASH_REPORT_SD_OPERATION_LIMIT;
 assert(!crash_report_sd_admit(&a,true) && state.retained && f.calls==2);
 assert_retained(&f,&state);
}
static void test_admission_budgets(const fake *baseline) {
 for(unsigned budget=1;budget<=baseline->calls+1;++budget) {
  fake f;init(&f);f.duration=1;crash_report_sd_state state={0};
  crash_report_sd_result result=run(&f,&state,4096,budget);
  assert(result!=CRASH_REPORT_SD_RETAINED && !f.handle);
  assert(f.calls==state.operations && f.calls<=CRASH_REPORT_SD_OPERATION_LIMIT);
  assert(f.calls<=budget+CRASH_REPORT_SD_CLEANUP_RESERVE);
  for(unsigned i=budget;i<f.calls;++i)assert(f.events[i]==CLOSE || f.events[i]==REMOVE || f.events[i]==READY);
  if(result==CRASH_REPORT_SD_SAVED)assert_saved(&f,4096);
 }
 /* A single synchronous callback may exceed the budget, then only bounded
  * cleanup/readiness is admitted. There is deliberately no preemption claim. */
 fake f;init(&f);f.duration=2000;crash_report_sd_state state={0};
 assert(run(&f,&state,4096,0)==CRASH_REPORT_SD_DEFERRED && f.calls==1 && f.now==2000);
 init(&f);f.duration=1000;assert(run(&f,&state,4096,UINT32_MAX)==CRASH_REPORT_SD_DEFERRED && f.calls==1);
 init(&f);f.duration=1;f.now=UINT64_MAX-20;
 assert(run(&f,&state,4096,250)==CRASH_REPORT_SD_SAVED);assert_saved(&f,4096);
}
int main(void) {
 for(unsigned i=0;i<sizeof(report_text);++i)report_text[i]=(unsigned char)(' '+i%95);
 fake baseline;init(&baseline);crash_report_sd_state state={0};
 assert(run(&baseline,&state,4096,0)==CRASH_REPORT_SD_SAVED);assert_saved(&baseline,4096);
 test_normal_and_dedupe();test_unavailable_and_limits();test_path_identity_and_collisions();
 test_interrupted_retry();test_every_clean_failure(&baseline);test_all_short_and_corrupt_transfers(&baseline);
 test_all_safety_boundaries(&baseline);test_hidden_handles_and_cleanup(&baseline);
 test_clean_cleanup_refusal();test_cleanup_safety_and_operation_reserve();test_admission_budgets(&baseline);
 printf("crash_report_sd: PASS (max report %u bytes; happy path %u calls; %u safety boundaries)\n",
        CRASH_REPORT_SD_TEXT_MAX,baseline.calls,baseline.safe_calls);
 return 0;
}
