/* Real Contexts client/adapter/rendezvous, with checked service and launch
 * seams. No Runtime, model owner, RF hardware or display is simulated as real. */
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#define PORTABLE_CONTEXTS_CLIENT
#define PORTABLE_CONTEXTS_CLOCK_RF_ONLY
#define PORTABLE_CONTEXTS_SOURCE_MASK CONTEXTS_RADIO
#ifdef TEST_CONTEXTS_RESIDENT
#define PORTABLE_RESIDENT_SHELL_HOST
#endif
static bool native_custody_retained,failed;
static unsigned retained,calls,acquires,releases,requests,begins,finishes,launches;
static unsigned order,closed_at,launched_at;
static bool service_live;
static const char *scenario;
static void portable_adapter_retain(void) {native_custody_retained=failed=true;retained++;}
#define PORTABLE_CONTEXTS_CUSTODY_SAFE() (!native_custody_retained)
#define PORTABLE_CONTEXTS_CLEANUP_FAILURE() portable_adapter_retain()
#include "PortableContextsClient.h"
#include "PortableBackgroundServices.h"
static contexts_status_v1 state;
static const contexts_service_v1 service;
static bool contexts_clock_recovered,contexts_clock_handoff;
static bool contexts_suspend(void);
static bool launch(const char *path);
#ifdef PORTABLE_RESIDENT_SHELL_HOST
static bool resident_queue_launch(const char *path);
#endif
static bool is(const char *name) {return !strcmp(scenario,name);}
static void call(void) {assert(!native_custody_retained&&service_live);calls++;order++;}
static bool acquire(const char *name,uint32_t api,uint64_t instance,risc_runtime_capability_v1 *out) {
 assert(!native_custody_retained&&!service_live);
 assert(!strcmp(name,CONTEXTS_SERVICE_CAPABILITY)&&api==1&&!instance);
 acquires++;order++;service_live=true;
 *out=(risc_runtime_capability_v1){sizeof(*out),acquires,acquires,&service};return true;
}
static bool release(risc_runtime_capability_v1 *grant) {
 call();assert(grant->api==&service);
 if(is("release-failed"))return false;
 releases++;service_live=false;closed_at=order;
 *grant=(risc_runtime_capability_v1){.struct_size=sizeof(*grant)};return true;
}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),
 .request_launch=launch,.acquire=acquire,.release=release};
static const risc_runtime_api_v1 *rt=&runtime;
static bool pause_service(void *c) {(void)c;call();return !is("pause-failed");}
static bool step(void *c,const contexts_policy_v1 *p) {(void)c;(void)p;call();return true;}
static bool status(void *c,contexts_status_v1 *out) {
 (void)c;call();*out=state;
 if(is("bad-size"))out->struct_size=0;
 return !is("status-failed");
}
static bool request(void *c,uint32_t sources) {
 (void)c;call();assert(sources==CONTEXTS_RADIO);requests++;
 if(is("request-failed"))return false;
 state.export_pending|=sources;state.radio.model_state=CONTEXTS_MODEL_REQUESTED;return true;
}
static bool begin(void *c,uint32_t source) {
 (void)c;call();assert(source==CONTEXTS_RADIO&&state.export_pending==source);begins++;
 if(is("begin-failed"))return false;
 state.export_active=source;state.radio.model_state=CONTEXTS_MODEL_LOADING;return true;
}
static bool record(void *c,uint32_t s,uint32_t k,uint32_t i,const void *b,uint32_t n) {
 (void)c;(void)s;(void)k;(void)i;(void)b;(void)n;assert(false);return false;
}
static bool finish(void *c,uint32_t source,uint32_t result) {
 (void)c;call();finishes++;assert(result==CONTEXTS_EXPORT_UNSUPPORTED);
 if(is("finish-failed"))return false;
 state.export_pending&=~source;state.export_active=0;
 if(source&CONTEXTS_RADIO)state.radio.model_state=CONTEXTS_MODEL_FAILED;
 return true;
}
static int32_t label(void *c,uint32_t s,uint32_t slot,contexts_label_v1 *out) {
 (void)c;(void)s;(void)slot;(void)out;assert(false);return -1;
}
static bool claim(void *c,uint32_t s,uint32_t slot,const char *name,uint32_t generation) {
 (void)c;(void)s;(void)slot;(void)name;(void)generation;assert(false);return false;
}
static bool result(void *c,uint32_t s,uint32_t generation,uint32_t value) {
 (void)c;(void)s;(void)generation;(void)value;assert(false);return false;
}
static bool capture(void *c) {(void)c;call();return true;}
static const contexts_service_v1 service={.api_version=1,.struct_size=sizeof(service),
 .step=step,.pause=pause_service,.status=status,.request_export=request,.begin_export=begin,
 .export_record=record,.finish_export=finish,.label=label,.claim_preset=claim,
 .preset_result=result,.capture_audio=capture};
#include "../../lib/PortableApps/src/contexts_adapter.inc"
#include "../../lib/PortableApps/src/contexts_clock.inc"
static bool queue(const char *path) {
 assert(!native_custody_retained&&!service_live&&!contexts_client.api);
 assert(!strcmp(path,"waterfall.elf"));launches++;launched_at=++order;
 assert(closed_at&&closed_at<launched_at);return !is("launch-refused");
}
static bool launch(const char *path) {
#ifdef PORTABLE_RESIDENT_SHELL_HOST
 (void)path;assert(false);return false;
#else
 if(!contexts_suspend())return false;
 return queue(path);
#endif
}
#ifdef PORTABLE_RESIDENT_SHELL_HOST
static bool resident_queue_launch(const char *path) {return queue(path);}
#endif
int main(int argc,char **argv) {
 assert(argc==2);scenario=argv[1];
 state=(contexts_status_v1){.struct_size=sizeof(state),.radio={.source=CONTEXTS_RADIO}};
 assert(contexts_open());
 if(is("normal")){
  assert(portable_contexts_training(true)&&contexts_client.foreground_learning);
  assert(contexts_suspend()&&!service_live&&contexts_client.foreground_learning);
  assert(contexts_open()&&contexts_client.foreground_learning);
  assert(portable_contexts_training(false)&&!contexts_client.foreground_learning);
 }
 contexts_client.settings_valid=contexts_client.policy.enabled=true;
 /* Exercise these shared boundaries too, while no operation is outstanding. */
 assert(contexts_capture_checkpoint()&&contexts_before_storage());
 if(is("disabled"))contexts_client.policy.enabled=false;
 if(is("ready"))state.radio.model_state=CONTEXTS_MODEL_READY;
 if(is("reload")) {state.export_pending=CONTEXTS_RADIO;state.radio.model_state=CONTEXTS_MODEL_REQUESTED;}
 if(is("unsupported")) {state.export_pending=CONTEXTS_AUDIO;contexts_client.policy.enabled=false;}
 if(is("unavailable")) {state.export_pending=CONTEXTS_RADIO;state.radio.model_state=CONTEXTS_MODEL_UNAVAILABLE;}
 if(is("unfinished")||is("finish-failed")) {state.export_active=CONTEXTS_RADIO;state.radio.model_state=CONTEXTS_MODEL_LOADING;}
 if(is("cleanup-pending"))state.cleanup_pending=true;
 if(is("unknown-pending"))state.export_pending=4;
 if(is("unknown-active"))state.export_active=3;
 bool ok=contexts_clock_recover();
 if(ok)ok=contexts_clock_rendezvous();
 bool terminal=is("status-failed")||is("bad-size")||is("cleanup-pending")||is("unknown-pending")||
  is("unknown-active")||is("request-failed")||is("begin-failed")||is("release-failed")||is("finish-failed");
 if(terminal) {
  assert(!ok&&native_custody_retained&&retained&&!launches);
  unsigned count=calls;assert(!portable_contexts_service()&&!portable_contexts_stop()&&!contexts_capture_checkpoint());
  assert(calls==count);
 } else {
  assert(ok&&!native_custody_retained);
  if(is("normal")||is("reload")||is("owner-unfinished")) {
   assert(contexts_clock_handoff&&launches==1&&begins==1&&!service_live&&!finishes);
#ifdef PORTABLE_RESIDENT_SHELL_HOST
   unsigned count=calls;assert(contexts_clock_rendezvous()&&calls==count);
   if(!is("owner-unfinished")) {state.export_pending=state.export_active=0;state.radio.model_state=CONTEXTS_MODEL_READY;}
   contexts_clock_owner_returned();assert(!contexts_clock_handoff&&!contexts_clock_recovered);
   assert(contexts_open()&&contexts_clock_recover());
   assert(finishes==(unsigned)is("owner-unfinished"));
   contexts_client.settings_valid=contexts_client.policy.enabled=true;
   assert(contexts_clock_rendezvous()&&launches==1);
#endif
  } else if(is("launch-refused"))assert(launches==1&&finishes==1&&!contexts_clock_handoff&&service_live);
  else assert(!launches&&!contexts_clock_handoff);
  if(service_live)assert(contexts_suspend());
  assert(!service_live&&acquires==releases);
 }
 printf("Contexts %s rendezvous %s: grant ordering, export state and terminal fence PASS\n",
#ifdef PORTABLE_RESIDENT_SHELL_HOST
  "resident",
#else
  "legacy",
#endif
  scenario);return 0;
}
