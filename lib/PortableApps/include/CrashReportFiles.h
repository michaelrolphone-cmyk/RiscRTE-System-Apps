#pragma once
#include "RiscStorageFilesV1.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

/* Foreground serialized crash export through a provisioned file service.
 * No backend discovery or storage mechanism belongs here. The native/provider
 * implementation owns availability, operation completion and resource custody.
 * Keep the exact internal spool until SAVED. RETAINED is terminal until restart.
 * Hooks check invocation safety and elapsed time without I/O or reentrancy.
 * Bounds: <=4096 text bytes, <=512 bytes per transfer, <=96 file-service calls;
 * cleanup has four reserved calls after the ordinary admission budget expires.
 * A synchronous running call cannot be preempted by this admission deadline.
 * No allocation, scans, background work or deletion of completed reports.
 */
#define CRASH_REPORT_FILES_TEXT_MAX 4096u
#define CRASH_REPORT_FILES_CHUNK_MAX 512u
#define CRASH_REPORT_FILES_PATH_CAPACITY 128u
#define CRASH_REPORT_FILES_OPERATION_LIMIT 96u
#define CRASH_REPORT_FILES_CLEANUP_RESERVE 4u
#define CRASH_REPORT_FILES_DEFAULT_BUDGET_MS 250u
#define CRASH_REPORT_FILES_MAX_BUDGET_MS 1000u
typedef enum {CRASH_REPORT_FILES_DEFERRED=0,CRASH_REPORT_FILES_SAVED=1,CRASH_REPORT_FILES_RETAINED=2} crash_report_files_result;
typedef struct {bool retained;unsigned operations;} crash_report_files_state;
typedef struct {
 void *context;uint64_t (*monotonic_ms)(void*);bool (*safe)(void*);uint32_t budget_ms;
} crash_report_files_hooks;
static inline uint32_t crash_report_files_crc32(const void *text,size_t length) {
 const unsigned char *bytes=(const unsigned char*)text;uint32_t crc=UINT32_MAX;
 for(size_t i=0;i<length;++i){crc^=bytes[i];for(unsigned bit=0;bit<8;++bit)crc=(crc>>1)^((0u-(crc&1u))&UINT32_C(0xedb88320));}
 return ~crc;
}
static inline char *crash_report_files_hex32(char *out,uint32_t value) {
 static const char hex[]="0123456789abcdef";for(unsigned i=0;i<8;++i)*out++=hex[(value>>(28u-i*4u))&15u];return out;
}
/* Stable legacy filename identity: full firmware hash, boot, sequence, text CRC.
 * Existing final files are compared exactly and never replaced or removed. */
static inline bool crash_report_files_path(char *out,size_t capacity,const char sha256[65],uint32_t boot,uint32_t sequence,uint32_t text_crc,bool temporary) {
 if(!out || capacity<CRASH_REPORT_FILES_PATH_CAPACITY || !sha256)return false;
 for(unsigned i=0;i<64;++i){char c=sha256[i];if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))return false;}
 if(sha256[64])return false;
 char *p=out;memcpy(p,"/CrashReports/",14);p+=14;
 for(unsigned i=0;i<64;++i){char c=sha256[i];*p++=(char)(c>='A'&&c<='F'?c+('a'-'A'):c);}
 *p++='-';p=crash_report_files_hex32(p,boot);*p++='-';p=crash_report_files_hex32(p,sequence);
 *p++='-';p=crash_report_files_hex32(p,text_crc);memcpy(p,temporary?".tmp":".txt",5);return true;
}
typedef struct {
 crash_report_files_state *state;const risc_storage_files_v1 *files;const crash_report_files_hooks *hooks;
 uint64_t started;uint32_t budget;risc_storage_files_handle_v1 handle;
 bool temporary_owned,temporary_cleanup_failed;const char *temporary;
} crash_report_files_attempt;
static inline bool crash_report_files_safe(crash_report_files_attempt *a) {
 if(a->state->retained)return false;
 if(!a->hooks->safe(a->hooks->context))a->state->retained=true;
 return !a->state->retained;
}
static inline bool crash_report_files_result_ok(crash_report_files_attempt *a,int32_t rc) {
 if(rc==RISC_FILES_RETAINED)a->state->retained=true;
 return crash_report_files_safe(a) && rc==RISC_FILES_OK;
}
static inline bool crash_report_files_admit(crash_report_files_attempt *a,bool cleanup) {
 if(!crash_report_files_safe(a))return false;
 unsigned limit=cleanup?CRASH_REPORT_FILES_OPERATION_LIMIT:CRASH_REPORT_FILES_OPERATION_LIMIT-CRASH_REPORT_FILES_CLEANUP_RESERVE;
 if(a->state->operations>=limit){if(cleanup)a->state->retained=true;return false;}
 if(!cleanup){uint64_t now=a->hooks->monotonic_ms(a->hooks->context);if(!crash_report_files_safe(a)||now-a->started>=a->budget)return false;}
 ++a->state->operations;return true;
}
static inline bool crash_report_files_close(crash_report_files_attempt *a) {
 if(!a->handle)return !a->state->retained;
 if(!crash_report_files_admit(a,true))return false;
 int32_t rc=a->files->close(a->files->context,a->handle);
 if(!crash_report_files_result_ok(a,rc)){a->state->retained=true;return false;}
 a->handle=RISC_STORAGE_FILES_HANDLE_INVALID;return true;
}
static inline bool crash_report_files_remove_temporary(crash_report_files_attempt *a) {
 if(a->temporary_cleanup_failed)return false;
 if(!a->temporary_owned)return !a->state->retained;
 if(!crash_report_files_admit(a,true))return false;
 int32_t rc=a->files->remove(a->files->context,a->temporary);
 if(!crash_report_files_result_ok(a,rc)){a->temporary_cleanup_failed=true;return false;}
 a->temporary_owned=false;return true;
}
static inline bool crash_report_files_open(crash_report_files_attempt *a,const char *path,uint32_t flags) {
 if(!crash_report_files_admit(a,false))return false;
 int32_t rc=a->files->open(a->files->context,path,flags,&a->handle);
 if(!crash_report_files_result_ok(a,rc))return false;
 return a->handle!=RISC_STORAGE_FILES_HANDLE_INVALID;
}
static inline bool crash_report_files_sync(crash_report_files_attempt *a) {
 if(!crash_report_files_admit(a,false))return false;
 return crash_report_files_result_ok(a,a->files->sync(a->files->context,a->handle));
}
/* Exact byte comparison includes length, initial offset, EOF, sync and checked
 * close. This same proof is required for a final-file deduplication. */
static inline bool crash_report_files_verify(crash_report_files_attempt *a,const void *text,size_t length) {
 uint64_t size=0,position=UINT64_MAX;
 if(!crash_report_files_admit(a,false))return false;
 int32_t rc=a->files->info(a->files->context,a->handle,&size,&position);
 if(!crash_report_files_result_ok(a,rc)||size!=length||position)return false;
 unsigned char chunk[CRASH_REPORT_FILES_CHUNK_MAX];
 for(size_t offset=0;offset<length;){
  uint32_t request=(uint32_t)(length-offset),read=0;if(request>sizeof(chunk))request=sizeof(chunk);
  if(!crash_report_files_admit(a,false))return false;
  rc=a->files->read(a->files->context,a->handle,chunk,request,&read);
  if(!crash_report_files_result_ok(a,rc)||read!=request||memcmp(chunk,(const unsigned char*)text+offset,request))return false;
  offset+=request;
 }
 if(!crash_report_files_admit(a,false))return false;
 uint32_t extra=0;
 rc=a->files->read(a->files->context,a->handle,chunk,1,&extra);
 if(!crash_report_files_result_ok(a,rc)||extra||!crash_report_files_sync(a))return false;
 return crash_report_files_close(a);
}
static inline crash_report_files_result crash_report_files_export(crash_report_files_state *state,
 const risc_storage_files_v1 *files,const char firmware_sha256[65],uint32_t boot,uint32_t sequence,
 const void *text,size_t length,const crash_report_files_hooks *hooks) {
 if(!state)return CRASH_REPORT_FILES_DEFERRED;
 if(state->retained)return CRASH_REPORT_FILES_RETAINED;
 state->operations=0;
 if(!files||files->api_version!=RISC_STORAGE_FILES_API_V1||files->struct_size<sizeof(*files)||
    !files->stat||!files->read||!files->write||!files->close||!files->remove||!files->open||!files->info||
    !files->sync||!files->mkdir||!files->rename||!hooks||!hooks->monotonic_ms||!hooks->safe||!text||!length||length>CRASH_REPORT_FILES_TEXT_MAX)
  return CRASH_REPORT_FILES_DEFERRED;
 char final[CRASH_REPORT_FILES_PATH_CAPACITY],temporary[CRASH_REPORT_FILES_PATH_CAPACITY];uint32_t crc=crash_report_files_crc32(text,length);
 if(!crash_report_files_path(final,sizeof(final),firmware_sha256,boot,sequence,crc,false)||
    !crash_report_files_path(temporary,sizeof(temporary),firmware_sha256,boot,sequence,crc,true))return CRASH_REPORT_FILES_DEFERRED;
 crash_report_files_attempt a={state,files,hooks,0,0,RISC_STORAGE_FILES_HANDLE_INVALID,false,false,temporary};
 a.budget=hooks->budget_ms?hooks->budget_ms:CRASH_REPORT_FILES_DEFAULT_BUDGET_MS;
 if(a.budget>CRASH_REPORT_FILES_MAX_BUDGET_MS)a.budget=CRASH_REPORT_FILES_MAX_BUDGET_MS;
 if(!crash_report_files_safe(&a))return CRASH_REPORT_FILES_RETAINED;
 a.started=hooks->monotonic_ms(hooks->context);
 if(!crash_report_files_safe(&a))return CRASH_REPORT_FILES_RETAINED;
 bool saved=false;risc_storage_files_stat_v1 st={.struct_size=sizeof(st)};int32_t rc;
 if(!crash_report_files_admit(&a,false))goto finish;
 rc=files->stat(files->context,"/CrashReports",&st);
 if(!crash_report_files_result_ok(&a,rc)){
  if(rc!=RISC_FILES_NOT_FOUND||state->retained)goto finish;
  if(!crash_report_files_admit(&a,false))goto finish;
  if(!crash_report_files_result_ok(&a,files->mkdir(files->context,"/CrashReports")))goto finish;
 }else if(!(st.flags&RISC_FILES_DIRECTORY))goto finish;
 if(!crash_report_files_admit(&a,false))goto finish;
 rc=files->stat(files->context,final,&st);
 if(crash_report_files_result_ok(&a,rc)){
  if((st.flags&RISC_FILES_DIRECTORY)||st.size!=length||!crash_report_files_open(&a,final,RISC_FILES_READ))goto finish;
  saved=crash_report_files_verify(&a,text,length);goto finish;
 }
 if(rc!=RISC_FILES_NOT_FOUND||state->retained)goto finish;
 /* Only this report's deterministic temporary file is eligible for cleanup. */
 if(!crash_report_files_admit(&a,false))goto finish;
 rc=files->stat(files->context,temporary,&st);
 if(crash_report_files_result_ok(&a,rc)){
  if(st.flags&RISC_FILES_DIRECTORY)goto finish;
  a.temporary_owned=true;
  if(!crash_report_files_remove_temporary(&a))goto finish;
 }else if(rc!=RISC_FILES_NOT_FOUND||state->retained)goto finish;
 if(!crash_report_files_open(&a,temporary,RISC_FILES_WRITE|RISC_FILES_CREATE|RISC_FILES_EXCLUSIVE))goto finish;
 a.temporary_owned=true;
 for(size_t offset=0;offset<length;){
  uint32_t request=(uint32_t)(length-offset),written=0;if(request>CRASH_REPORT_FILES_CHUNK_MAX)request=CRASH_REPORT_FILES_CHUNK_MAX;
  if(!crash_report_files_admit(&a,false))goto finish;
  rc=files->write(files->context,a.handle,(const unsigned char*)text+offset,request,&written);
  if(!crash_report_files_result_ok(&a,rc)||written!=request)goto finish;
  offset+=request;
 }
 if(!crash_report_files_sync(&a)||!crash_report_files_close(&a))goto finish;
 if(!crash_report_files_open(&a,temporary,RISC_FILES_READ)||!crash_report_files_verify(&a,text,length))goto finish;
 if(!crash_report_files_admit(&a,false))goto finish;
 if(!crash_report_files_result_ok(&a,files->rename(files->context,temporary,final)))goto finish;
 a.temporary_owned=false;
 if(!crash_report_files_open(&a,final,RISC_FILES_READ))goto finish;
 saved=crash_report_files_verify(&a,text,length);
finish:
 if(!state->retained&&crash_report_files_close(&a))crash_report_files_remove_temporary(&a);
 return state->retained?CRASH_REPORT_FILES_RETAINED:saved&&!a.temporary_cleanup_failed?CRASH_REPORT_FILES_SAVED:CRASH_REPORT_FILES_DEFERRED;
}
