#pragma once
#include <RiscRuntimeV1.h>
#include "FailureEvidenceReport.h"
#include "CrashReportSpool.h"
#include "CrashReportSd.h"
#include "RiscStorageVolumeStateV1.h"

/* Healthy default-app consumer, called only at settled foreground checkpoints.
 * Report text is copied once and is the sole input to both display and storage.
 * No panic-path I/O, hidden retries, media remounts or native acknowledgements
 * before durable readback AND explicit completed presentation. */
typedef struct {
 const risc_runtime_api_v1 *runtime;
 bool (*safe)(void *); bool (*prepare_storage)(void *);
 uint64_t (*now)(void *); void (*retain)(void *); void *context;
 uint32_t namespace_id,volume_instance; bool sd_enabled;
} crash_report_session_hooks;
typedef struct {
 crash_report_session_hooks hooks;
 crash_report_spool spool;
 crash_report_sd_state sd;
 risc_failure_evidence_client_v1 client;
 crash_report_spool_identity identity;
 char text[CRASH_REPORT_SPOOL_TEXT_MAX];uint32_t text_size;
 bool checked,pending,presented,committed,retained,attempted,empty;
 uint64_t attempted_at;
} crash_report_session;
static inline bool crash_session_safe(crash_report_session *s) {
 return !s->retained && s->hooks.safe(s->hooks.context);
}
static inline bool crash_session_retain(crash_report_session *s) {
 s->retained=true;s->hooks.retain(s->hooks.context);return false;
}
static inline bool crash_session_result(crash_report_session *s,crash_report_spool_result r) {
 if(r==CRASH_REPORT_SPOOL_RETAINED || r==CRASH_REPORT_SPOOL_UNSAFE || !crash_session_safe(s))
  return crash_session_retain(s);
 return true;
}
static inline bool crash_session_release(crash_report_session *s,risc_runtime_capability_v1 *g) {
 if(!crash_session_safe(s))return false;
 if(g->api && !s->hooks.runtime->release(g))return crash_session_retain(s);
 return crash_session_safe(s);
}
static inline bool crash_session_open(crash_report_session *s,risc_runtime_capability_v1 *g) {
 if(!crash_session_safe(s) || !s->hooks.prepare_storage(s->hooks.context))return false;
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};
 if(!s->hooks.runtime->acquire(RISC_APP_DATA_CAPABILITY,1,s->hooks.namespace_id,g))return false;
 s->spool.api=g->api;return true;
}
static inline bool crash_session_line(void *context,const char *line) {
 crash_report_session *s=context;size_t n=strlen(line);
 if(n+1>sizeof(s->text)-s->text_size)return false;
 memcpy(s->text+s->text_size,line,n);s->text_size+=(uint32_t)n;s->text[s->text_size++]='\n';return true;
}
static inline void crash_session_init(crash_report_session *s,const crash_report_session_hooks *hooks) {
 memset(s,0,sizeof(*s));s->hooks=*hooks;
}
static inline bool crash_session_save(crash_report_session *s) {
 if(!s->pending || s->committed)return crash_session_safe(s);
 risc_runtime_capability_v1 g={0};
 if(!crash_session_open(s,&g))return crash_session_safe(s);
 crash_report_spool_result r=crash_report_spool_append(&s->spool,&s->identity,s->text,s->text_size);
 if(!crash_session_result(s,r))return false;
 s->committed=r==CRASH_REPORT_SPOOL_COMMITTED;
 if(!crash_session_release(s,&g))return false;
 s->spool.api=NULL;return true;
}
static inline bool crash_session_read(crash_report_session *s) {
 if(s->checked)return crash_session_safe(s);
 const risc_runtime_api_v1 *rt=s->hooks.runtime;
 if(!crash_session_safe(s))return false;
 if(rt->struct_size<RISC_RUNTIME_FAILURE_EVIDENCE_V1_SIZE || !rt->failure_evidence){s->checked=true;return true;}
 s->client=(risc_failure_evidence_client_v1){.struct_size=sizeof(s->client)};
 if(!rt->failure_evidence(&s->client))return crash_session_safe(s); /* Not promoted yet: retry. */
 if(s->client.api_version!=1 || s->client.struct_size<sizeof(s->client) || !s->client.invocation || !s->client.read || !s->client.acknowledge)return false;
 risc_failure_evidence_v1 r={.struct_size=sizeof(r)};
 int result=s->client.read(s->client.invocation,&r);
 if(!crash_session_safe(s))return false;
 if(result==RISC_FAILURE_EVIDENCE_NONE){s->checked=true;return true;}
 if(result!=RISC_FAILURE_EVIDENCE_OK || r.struct_size<sizeof(r) || r.api_version!=1)return false;
 s->checked=true;if(!(r.flags&RISC_FAILURE_PENDING))return true;
 s->identity.captured_boot=r.record_boot;s->identity.captured_sequence=r.record_sequence;
 memcpy(s->identity.firmware_sha256,r.firmware_sha256,32);s->text_size=0;
 if(!crash_session_line(s,"CRASH / RESTART") || !failure_evidence_report_format(&r,crash_session_line,s))return false;
 s->pending=true;s->empty=false;return crash_session_save(s);
}
static inline bool crash_session_ack(crash_report_session *s) {
 if(!s->pending || !s->presented || !s->committed)return crash_session_safe(s);
 if(!crash_session_safe(s))return false;
 int r=s->client.acknowledge(s->client.invocation,s->identity.captured_boot,s->identity.captured_sequence);
 if(!crash_session_safe(s))return false;
 if(r==RISC_FAILURE_EVIDENCE_STALE){s->checked=s->pending=s->presented=s->committed=false;return true;}
 if(r==RISC_FAILURE_EVIDENCE_OK)s->pending=false;
 return r==RISC_FAILURE_EVIDENCE_OK;
}
typedef struct {crash_report_session *session;const risc_storage_volume_api_v1_state *state;} crash_session_sd_context;
static inline bool crash_session_sd_safe(void *context) {
 crash_session_sd_context *c=context;
 if(!crash_session_safe(c->session))return false;
 int r=c->state->observe(c->state->prepared.base.sleep.terminal.power.volume.base.context);
 return r==RISC_STORAGE_STATE_READY || r==RISC_STORAGE_STATE_UNAVAILABLE;
}
static inline uint64_t crash_session_sd_now(void *context) {
 crash_report_session *s=((crash_session_sd_context*)context)->session;return s->hooks.now(s->hooks.context);
}
static inline bool crash_session_checkpoint(crash_report_session *s) {
 if(!crash_session_safe(s))return false;
 if(s->empty && !s->pending)return true;
 uint64_t now=s->hooks.now(s->hooks.context);
 if(s->attempted && now-s->attempted_at<30000)return true;
 s->attempted=true;s->attempted_at=now;
 if(!crash_session_save(s) || !crash_session_ack(s))return false;
 if(!s->hooks.sd_enabled)return true;
 risc_runtime_capability_v1 data={0},sd={.struct_size=sizeof(sd)};
 if(!crash_session_open(s,&data))return crash_session_safe(s);
 crash_report_spool_result r=crash_report_spool_reload(&s->spool);
 if(!crash_session_result(s,r))return false;
 crash_report_spool_view report={0};
 bool available=r==CRASH_REPORT_SPOOL_OK && s->spool.count && crash_report_spool_peek(&s->spool,0,&report)==CRASH_REPORT_SPOOL_OK;
 s->empty=r==CRASH_REPORT_SPOOL_OK && !s->spool.count;
 if(!crash_session_release(s,&data))return false;
 s->spool.api=NULL;
 if(!available)return true;
 if(!s->hooks.runtime->acquire("storage.volume",1,s->hooks.volume_instance,&sd))return crash_session_safe(s);
 const risc_storage_volume_api_v1_ext *volume=risc_storage_volume_extension(sd.api);
 const risc_storage_volume_api_v1_state *state=risc_storage_volume_state(sd.api);
 if(!volume || !state)return crash_session_release(s,&sd);
 crash_session_sd_context ctx={s,state};
 if(!crash_session_sd_safe(&ctx))return crash_session_retain(s);
 int media=state->observe(volume->base.context);
 if(media!=RISC_STORAGE_STATE_READY)return crash_session_release(s,&sd);
 char hash[65];static const char hex[]="0123456789abcdef";
 for(unsigned i=0;i<32;++i){hash[2*i]=hex[report.identity.firmware_sha256[i]>>4];hash[2*i+1]=hex[report.identity.firmware_sha256[i]&15];}hash[64]=0;
 crash_report_sd_hooks hooks={.context=&ctx,.monotonic_ms=crash_session_sd_now,.safe=crash_session_sd_safe,.budget_ms=250};
 int saved=crash_report_sd_export(&s->sd,volume,hash,report.identity.captured_boot,report.identity.captured_sequence,report.text,report.text_size,&hooks);
 if(saved==CRASH_REPORT_SD_RETAINED || !crash_session_safe(s))return crash_session_retain(s);
 if(!crash_session_release(s,&sd))return false;
 if(saved!=CRASH_REPORT_SD_SAVED)return true;
 if(!crash_session_open(s,&data))return crash_session_safe(s);
 r=crash_report_spool_remove(&s->spool,&report.identity,report.text,report.text_size);
 if(!crash_session_result(s,r))return false;
 if(!crash_session_release(s,&data))return false;
 s->spool.api=NULL;
 return true;
}
