/* Exhaustive callback-return loss injection for the opt-in checkpoint graph. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
static bool live=true;
#define PORTABLE_CONTEXTS_LIVE() live
#include "PortableContextsClient.h"
static unsigned calls,drop;
static bool fail_on_drop,save_mode,missing_files,missing_clock,invalid_files,absent_file;
static char callback_names[128][80];
static bool callback(const char *name){
 assert(live&&"provider callback after runtime loss");assert(calls<128);
 snprintf(callback_names[calls],80,"%s",name);calls++;
 if(calls==drop){live=false;return fail_on_drop;}
 return false;
}
static bool pause_svc(void*c){(void)c;return !callback("service.pause");}
static bool step(void*c,const contexts_policy_v1*p){(void)c;(void)p;return true;}
static bool status(void*c,contexts_status_v1*p){(void)c;(void)p;return true;}
static bool mask(void*c,uint32_t a){(void)c;(void)a;return true;}
static bool record(void*c,uint32_t a,uint32_t b,uint32_t d,const void*p,uint32_t n){(void)c;(void)a;(void)b;(void)d;(void)p;(void)n;return true;}
static bool finish(void*c,uint32_t a,uint32_t b){(void)c;(void)a;(void)b;return true;}
static bool result(void*c,uint32_t a,uint32_t b,uint32_t d){(void)d;return finish(c,a,b);}
static int32_t label(void*c,uint32_t a,uint32_t b,contexts_label_v1*p){(void)c;(void)a;(void)b;(void)p;return 0;}
static bool claim(void*c,uint32_t a,uint32_t b,const char*p,uint32_t d){(void)c;(void)a;(void)b;(void)p;(void)d;return true;}
static bool fingerprint(void*c,uint32_t op,void*p){
 (void)c;char name[80];snprintf(name,sizeof(name),"service.fingerprint(%u)",op);bool bad=callback(name);
 if(op==CONTEXTS_FP_STATUS)*(contexts_fingerprint_status_v1*)p=(contexts_fingerprint_status_v1){.struct_size=sizeof(contexts_fingerprint_status_v1),.sources=3,.dirty_sources=save_mode?3:0};
 if(op==CONTEXTS_FP_EXPORT){contexts_fingerprint_record_v1*q=p;q->size=4;memcpy(q->bytes,"DATA",4);}
 return !bad;
}
static const contexts_fingerprint_service_v1 service={.base={.api_version=1,.struct_size=sizeof(service),.step=step,.pause=pause_svc,.status=status,.request_export=mask,.begin_export=mask,.export_record=record,.finish_export=finish,.label=label,.claim_preset=claim,.preset_result=result,.capture_audio=pause_svc},.fingerprint_abi=0x31504643u,.fingerprint=fingerprint};
static uint8_t rule_bytes[CR_FILE_BYTES];
static int32_t file_stat(void*c,const char*name,uint32_t*size,uint64_t*rev){
 (void)c;bool bad=callback(!strcmp(name,"context-rules.ctx")?"files.stat(rules)":"files.stat(fingerprints)");*size=!strcmp(name,"context-rules.ctx")?CR_FILE_BYTES:4;*rev=1;
 return bad?RISC_APP_DATA_IO:absent_file?RISC_APP_DATA_NOT_FOUND:0;
}
static int32_t file_read(void*c,const char*name,uint64_t rev,void*out,uint32_t cap,uint32_t*size,uint64_t*actual){
 (void)c;(void)cap;bool rules=!strcmp(name,"context-rules.ctx");bool bad=callback(rules?"files.read(rules)":"files.read(fingerprints)");*size=rules?CR_FILE_BYTES:4;*actual=rev;memcpy(out,rules?(const void*)rule_bytes:(const void*)"DATA",*size);return bad?RISC_APP_DATA_IO:0;
}
static int32_t file_replace(void*c,const char*name,uint64_t rev,const void*out,uint32_t size){(void)c;(void)name;(void)rev;(void)out;(void)size;return callback("files.replace")?RISC_APP_DATA_IO:0;}
static const risc_app_data_v1 files={1,sizeof(files),NULL,file_stat,file_read,file_replace};
static const risc_app_data_v1 invalid={0};
#if defined(PORTABLE_NATIVE_TIME_TOOLBAR)
static int32_t clock_read(void*c,risc_realtime_snapshot_v1*q){(void)c;bool bad=callback("clock.read");q->validity=RISC_REALTIME_VALID;q->epoch_seconds=1700000000;return bad?-1:0;}
static const risc_realtime_api_v1 clock_api={.api_version=1,.struct_size=sizeof(clock_api),.read=clock_read};
#else
static bool clock_read(void*c,twatch_rtc_time_v1*q){(void)c;bool bad=callback("clock.read");*q=(twatch_rtc_time_v1){.year=2025,.month=1,.day=1};return !bad;}
static const twatch_rtc_api_v1 clock_api={.api_version=2,.struct_size=sizeof(clock_api),.read=clock_read};
#endif
static bool acquire(const char*name,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){
 (void)v;(void)id;bool backend=!strcmp(name,CONTEXTS_SERVICE_CAPABILITY),storage=!strcmp(name,RISC_SHARED_DATA_CAPABILITY);
 bool bad=callback(backend?"acquire(service)":storage?"acquire(files)":"acquire(clock)");
 if((storage&&missing_files)||(!backend&&!storage&&missing_clock))return false;
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g),.api=backend?(const void*)&service:storage?(invalid_files?(const void*)&invalid:(const void*)&files):(const void*)&clock_api,.slot=1,.generation=1};return !bad;
}
static bool release(risc_runtime_capability_v1*g){bool bad=callback(g->api==&service?"release(service)":g->api==&clock_api?"release(clock)":"release(files)");*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return !bad;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release};
static bool run(unsigned scenario){
 portable_contexts_client c={.runtime=&runtime,.api=&service.base,.fingerprint_sources=3,.grant={.struct_size=sizeof(c.grant),.api=&service,.slot=1,.generation=1}};
 if(scenario==0)return portable_contexts_open(&c,&runtime);
 if(scenario==1)return portable_contexts_pause(&c);
 if(scenario==2){c.storage=(risc_runtime_capability_v1){.struct_size=sizeof(c.storage),.api=&files,.slot=2,.generation=1};return portable_contexts_close(&c);}
 assert(0);return false;
}
int main(void){
 cr_store rules;cr_init(&rules);rules.generation=1;assert(cr_encode(&rules,rule_bytes,sizeof(rule_bytes)));
 unsigned probes=0;
 for(unsigned variant=0;variant<5;variant++)for(unsigned scenario=0;scenario<3;scenario++){
  missing_files=variant==1;missing_clock=variant==2;invalid_files=variant==3;absent_file=variant==4;save_mode=scenario==1;
  live=true;calls=drop=0;fail_on_drop=false;(void)run(scenario);unsigned total=calls;
  for(unsigned index=1;index<=total;index++)for(unsigned fail=0;fail<2;fail++){
   live=true;calls=0;drop=index;fail_on_drop=fail;bool ok=run(scenario);
   if(live||ok||calls!=index){fprintf(stderr,"FAIL variant=%u scenario=%u callback=%u name=%s calls=%u live=%d ok=%d\n",variant,scenario,index,callback_names[index-1],calls,live,ok);return 1;}
   probes++;
  }
  calls=0;live=false;assert(!run(scenario)&&!calls);probes++;
 }
 printf("%u callback-loss and entry-fence probes PASS\n",probes);return 0;
}
