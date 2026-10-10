/* Product-selected healthy Home consumer. Never reached by panic capture,
 * Runtime yield, provider polling, or the diagnostic log callback. */
#ifdef PORTABLE_CRASH_REPORT_SD
#if !defined(PORTABLE_RESIDENT_SHELL_HOST) || !defined(PORTABLE_RESIDENT_POLICY)
#error Crash report persistence requires an explicitly selected resident Home
#endif
#include "CrashReportSpool.h"
#include "CrashReportSd.h"
#include "RiscStorageVolumeStateV1.h"
#ifndef PORTABLE_CRASH_REPORT_NAMESPACE
#error Select the dedicated Home crash-report namespace explicitly
#endif
static crash_report_spool failure_archive_spool;
static crash_report_sd_state failure_archive_sd;
static risc_runtime_capability_v1 failure_archive_data_grant,failure_archive_sd_grant;
static risc_failure_evidence_client_v1 failure_archive_client;
static crash_report_spool_identity failure_archive_identity;
static char failure_archive_text[CRASH_REPORT_SPOOL_TEXT_MAX];
static char failure_archive_export_text[CRASH_REPORT_SPOOL_TEXT_MAX];
static uint32_t failure_archive_text_size,failure_archive_attempted_at,failure_archive_drops;
static bool failure_archive_initialized,failure_archive_probed,failure_archive_backlog;
static bool failure_archive_pending,failure_archive_presented,failure_archive_committed;
static bool failure_archive_attempted,failure_archive_busy,failure_archive_fault;

static bool failure_archive_safe(void *context) {
 (void)context;return !native_custody_retained && !failed;
}
static uint64_t failure_archive_clock(void *context) {
 (void)context;return millis_now();
}
/* The ordinary boolean ready() can itself leave retained guard/log custody.
 * A read-only provider snapshot is required even on the first failed ready.
 * Keep this separate from general Home safety: its table is grant-scoped. */
static bool failure_archive_sd_safe(void *context) {
 const risc_storage_volume_api_v1_state *state=context;
 if(!failure_archive_safe(NULL) || !state)return false;
 int32_t status=state->observe(state->prepared.base.sleep.terminal.power.volume.base.context);
 return status==RISC_STORAGE_STATE_READY || status==RISC_STORAGE_STATE_UNAVAILABLE;
}
static bool failure_archive_append_line(void *context,const char *line) {
 (void)context;size_t n=strlen(line);
 if(n+1u>sizeof(failure_archive_text)-failure_archive_text_size)return false;
 memcpy(failure_archive_text+failure_archive_text_size,line,n);
 failure_archive_text_size+=(uint32_t)n;
 failure_archive_text[failure_archive_text_size++]='\n';return true;
}
static bool failure_archive_storage_safe(void) {
 if(!failure_archive_safe(NULL))return false;
#ifdef PORTABLE_CONTEXTS_CLIENT
 if(!contexts_before_storage())return false;
#endif
#ifdef PORTABLE_BLE_BROADCAST
 if(!broadcast_before_storage())return false;
#endif
 return failure_archive_safe(NULL);
}
static bool failure_archive_release(risc_runtime_capability_v1 *grant) {
 if(!failure_archive_safe(NULL))return false;
 if(grant->api && !rt->release(grant)) {
  portable_adapter_retain_silent();return false;
 }
 return failure_archive_safe(NULL);
}
static bool failure_archive_open_data(void) {
 if(!failure_archive_storage_safe())return false;
 failure_archive_data_grant=(risc_runtime_capability_v1){.struct_size=sizeof(failure_archive_data_grant)};
 if(!rt->acquire(RISC_APP_DATA_CAPABILITY,1,PORTABLE_CRASH_REPORT_NAMESPACE,&failure_archive_data_grant))return false;
 const risc_app_data_v1 *api=failure_archive_data_grant.api;
 if(!api || api->api_version!=1 || api->struct_size<sizeof(*api) || !api->stat || !api->read || !api->replace) {
  (void)failure_archive_release(&failure_archive_data_grant);return false;
 }
 if(!failure_archive_initialized) {
  crash_report_spool_init(&failure_archive_spool,api);failure_archive_initialized=true;
 }else failure_archive_spool.api=api;
 return true;
}
static bool failure_archive_close_data(void) {
 if(!failure_archive_release(&failure_archive_data_grant))return false;
 failure_archive_spool.api=NULL;return true;
}
static bool failure_archive_spool_result_safe(crash_report_spool_result result) {
 if(result==CRASH_REPORT_SPOOL_RETAINED || result==CRASH_REPORT_SPOOL_UNSAFE || !failure_archive_safe(NULL)) {
  if(!native_custody_retained)portable_adapter_retain_silent();
  return false;
 }
 if(result==CRASH_REPORT_SPOOL_CORRUPT || result==CRASH_REPORT_SPOOL_COLLISION || result==CRASH_REPORT_SPOOL_INVALID)
  failure_archive_fault=true;
 if(failure_archive_spool.loaded) {
  failure_archive_probed=true;failure_archive_backlog=failure_archive_spool.count!=0;
  failure_archive_drops=failure_archive_spool.dropped_count;
 }
 return true;
}
static bool failure_archive_save_pending(void) {
 if(!failure_archive_pending || failure_archive_committed)return true;
 if(!failure_archive_open_data())return failure_archive_safe(NULL);
 crash_report_spool_result result=crash_report_spool_append(&failure_archive_spool,&failure_archive_identity,
   failure_archive_text,failure_archive_text_size);
 if(!failure_archive_spool_result_safe(result))return false;
 if(result==CRASH_REPORT_SPOOL_COMMITTED)failure_archive_committed=true;
 return failure_archive_close_data();
}
static bool failure_archive_capture(const risc_failure_evidence_v1 *record,
    const risc_failure_evidence_client_v1 *client) {
 failure_archive_client=*client;failure_archive_identity.captured_boot=record->record_boot;
 failure_archive_identity.captured_sequence=record->record_sequence;
 memcpy(failure_archive_identity.firmware_sha256,record->firmware_sha256,32);
 failure_archive_text_size=0;failure_archive_presented=failure_archive_committed=false;
 failure_archive_pending=false;
 if(!failure_archive_append_line(NULL,"CRASH / RESTART") ||
    !failure_evidence_report_format(record,failure_archive_append_line,NULL))return false;
 failure_archive_pending=true;return failure_archive_save_pending();
}
static void failure_archive_report_status(failure_evidence_layout *layout) {
 failure_evidence_text(layout,failure_archive_committed?
   "Internal report backup saved; SD export pending.":"Report backup deferred; original evidence remains pending.");
 if(failure_archive_fault)failure_evidence_text(layout,"Report spool needs attention; existing data was preserved.");
 if(failure_archive_drops) {
  char line[128];snprintf(line,sizeof(line),"Report queue overflow: %lu unsaved report(s) dropped.",(unsigned long)failure_archive_drops);
  failure_evidence_text(layout,line);
  snprintf(line,sizeof(line),"Last dropped record: boot %lu / sequence %lu.",
    (unsigned long)failure_archive_spool.last_dropped_identity.captured_boot,
    (unsigned long)failure_archive_spool.last_dropped_identity.captured_sequence);
  failure_evidence_text(layout,line);
 }
}
/* Called only after every OSD page physically completed and final Continue.
 * A missing/unwritable spool does not trap the user in the report UI. */
static int32_t failure_archive_acknowledge(void) {
 failure_archive_presented=true;
 if(!failure_archive_committed)return RISC_FAILURE_EVIDENCE_OK;
 int32_t result=failure_archive_client.acknowledge(failure_archive_client.invocation,
   failure_archive_identity.captured_boot,failure_archive_identity.captured_sequence);
 if(result==RISC_FAILURE_EVIDENCE_OK || result==RISC_FAILURE_EVIDENCE_STALE)failure_archive_pending=false;
 return result;
}
/* At most one bounded SD report transaction per explicit settled Home
 * checkpoint. No media refresh/remount and no I/O at all once an empty queue
 * has been checked. Existing underlying synchronous I/O remains unpreemptible. */
static bool failure_archive_checkpoint(void) {
 if(failure_archive_busy || !failure_archive_safe(NULL) || resident_child_active || resident_dispatching ||
    surface.frame || paper_token || !display_settled || alarm_modal || quick_modal ||
    resident_launch_pending || resident_sleep_pending || pqa_visible(&quick.ui))return failure_archive_safe(NULL);
 if(failure_archive_probed && !failure_archive_backlog && !failure_archive_pending)return true;
 uint32_t now=millis_now();
 if(failure_archive_attempted && (uint32_t)(now-failure_archive_attempted_at)<30000u)return true;
 failure_archive_attempted=true;failure_archive_attempted_at=now;failure_archive_busy=true;
 if(!failure_archive_save_pending())goto retained;
 if(failure_archive_pending && failure_archive_presented && failure_archive_committed) {
  int32_t result=failure_archive_acknowledge();
  if(!failure_archive_safe(NULL))goto retained;
  if(result==RISC_FAILURE_EVIDENCE_STALE)resident_prior_failure_checked=false;
  else if(result!=RISC_FAILURE_EVIDENCE_OK)goto deferred;
 }
 if(!failure_archive_open_data())goto deferred;
 crash_report_spool_result loaded=crash_report_spool_reload(&failure_archive_spool);
 if(!failure_archive_spool_result_safe(loaded))goto retained;
 if(loaded!=CRASH_REPORT_SPOOL_OK || !failure_archive_spool.count) {
  if(!failure_archive_close_data())goto retained;
  goto deferred;
 }
 crash_report_spool_view report;
 if(crash_report_spool_peek(&failure_archive_spool,0,&report)!=CRASH_REPORT_SPOOL_OK) {
  failure_archive_fault=true;if(!failure_archive_close_data())goto retained;goto deferred;
 }
 /* Release the native AppData grant before acquiring the ordinary SD provider.
  * The report is copied in the spool's fixed buffer and has no borrowed handle. */
 if(!failure_archive_close_data())goto retained;
 failure_archive_sd_grant=(risc_runtime_capability_v1){.struct_size=sizeof(failure_archive_sd_grant)};
 if(!rt->acquire("storage.volume",1,9,&failure_archive_sd_grant))goto deferred;
 const risc_storage_volume_api_v1_ext *volume=risc_storage_volume_extension(failure_archive_sd_grant.api);
 if(!volume) {if(!failure_archive_release(&failure_archive_sd_grant))goto retained;goto deferred;}
 const risc_storage_volume_api_v1_state *state=risc_storage_volume_state(failure_archive_sd_grant.api);
 if(!state) {if(!failure_archive_release(&failure_archive_sd_grant))goto retained;goto deferred;}
 int32_t media=state->observe(volume->base.context);
 if(!failure_archive_safe(NULL) || (media!=RISC_STORAGE_STATE_READY && media!=RISC_STORAGE_STATE_UNAVAILABLE))goto retained;
 if(media!=RISC_STORAGE_STATE_READY) {if(!failure_archive_release(&failure_archive_sd_grant))goto retained;goto deferred;}
 char firmware[65];static const char digits[]="0123456789abcdef";
 for(unsigned i=0;i<32;++i){firmware[i*2]=digits[report.identity.firmware_sha256[i]>>4];firmware[i*2+1]=digits[report.identity.firmware_sha256[i]&15];}firmware[64]=0;
 const crash_report_sd_hooks hooks={.context=(void *)state,.monotonic_ms=failure_archive_clock,.safe=failure_archive_sd_safe,.budget_ms=250};
 const char *export_text=report.text;uint32_t export_size=report.text_size;
 if(failure_archive_drops) {
  /* Preserve the exact OSD report as a prefix and make bounded-queue losses
   * visible in the shareable file too. Never truncate captured evidence. */
  char note[192];int n=snprintf(note,sizeof(note),
    "Report queue overflow: %lu unsaved report(s) dropped.\nLast dropped record: boot %lu / sequence %lu.\n",
    (unsigned long)failure_archive_drops,
    (unsigned long)failure_archive_spool.last_dropped_identity.captured_boot,
    (unsigned long)failure_archive_spool.last_dropped_identity.captured_sequence);
  if(n>0 && (size_t)n<sizeof(note) && (uint32_t)n<=sizeof(failure_archive_export_text)-report.text_size) {
   memcpy(failure_archive_export_text,report.text,report.text_size);
   memcpy(failure_archive_export_text+report.text_size,note,(size_t)n);
   export_text=failure_archive_export_text;export_size+=(uint32_t)n;
  }else failure_archive_fault=true;
 }
 int saved=crash_report_sd_export(&failure_archive_sd,volume,firmware,report.identity.captured_boot,
   report.identity.captured_sequence,export_text,export_size,&hooks);
 if(saved==CRASH_REPORT_SD_RETAINED || !failure_archive_safe(NULL))goto retained;
 if(!failure_archive_release(&failure_archive_sd_grant))goto retained;
 if(saved!=CRASH_REPORT_SD_SAVED)goto deferred;
 if(!failure_archive_open_data())goto deferred;
 crash_report_spool_result removed=crash_report_spool_remove(&failure_archive_spool,&report.identity,report.text,report.text_size);
 if(!failure_archive_spool_result_safe(removed))goto retained;
 if(!failure_archive_close_data())goto retained;
 if(removed==CRASH_REPORT_SPOOL_REMOVED)failure_archive_attempted=false;
deferred:
 failure_archive_busy=false;return failure_archive_safe(NULL);
retained:
 if(!native_custody_retained)portable_adapter_retain_silent();
 return false;
}
#endif
