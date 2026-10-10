#include "PortableContextsClient.h"
#include <assert.h>
#include <stdio.h>
static uint32_t dirty=3,source,imports,exports,saved,releases,pauses,writes;
static int32_t write_error;
static bool paused,release_ok=true;
static uint8_t disk[2][4]={{1,2,3,4},{5,6,7,8}};
static bool pause_service(void*c){(void)c;paused=true;++pauses;return true;}
static bool step_service(void*c,const contexts_policy_v1*p){(void)c;(void)p;paused=false;return true;}
static bool status_service(void*c,contexts_status_v1*s){(void)c;(void)s;return true;}
static bool request(void*c,uint32_t s){(void)c;(void)s;return true;}
static bool finish(void*c,uint32_t s,uint32_t n){(void)c;(void)s;(void)n;return true;}
static bool record(void*c,uint32_t s,uint32_t k,uint32_t i,const void*b,uint32_t n){(void)c;(void)s;(void)k;(void)i;(void)b;(void)n;return true;}
static int32_t label(void*c,uint32_t s,uint32_t i,contexts_label_v1*l){(void)c;(void)s;(void)i;(void)l;return 0;}
static bool claim(void*c,uint32_t s,uint32_t i,const char*n,uint32_t g){(void)c;(void)s;(void)i;(void)n;(void)g;return true;}
static bool result(void*c,uint32_t s,uint32_t g,uint32_t r){(void)c;(void)s;(void)g;(void)r;return true;}
static bool capture(void*c){(void)c;return true;}
static bool fingerprint(void*c,uint32_t op,void*data){
    (void)c;
    if(op==CONTEXTS_FP_CONFIG)return true;
    if(op==CONTEXTS_FP_STATUS){contexts_fingerprint_status_v1*s=data;s->sources=3;s->dirty_sources=dirty;return true;}
    contexts_fingerprint_record_v1*r=data;
    assert(paused);
    if(op==CONTEXTS_FP_IMPORT){assert(r->size==4);++imports;return true;}
    if(op==CONTEXTS_FP_EXPORT){assert(r->capacity>=4);memcpy(r->bytes,"new!",4);r->size=4;r->generation=7;++exports;return true;}
    if(op==CONTEXTS_FP_SAVED){assert(r->generation==7);dirty&=~r->source;++saved;return true;}
    return false;
}
static const contexts_fingerprint_service_v1 service={
    .base={.api_version=1,.struct_size=sizeof(service),.step=step_service,.pause=pause_service,.status=status_service,
        .request_export=request,.begin_export=request,.export_record=record,.finish_export=finish,.label=label,
        .claim_preset=claim,.preset_result=result,.capture_audio=capture},
    .fingerprint_abi=0x31504643u,.fingerprint=fingerprint};
static int32_t stat_file(void*c,const char*n,uint32_t*size,uint64_t*revision){(void)c;assert(paused&&!strcmp(n,"context-fingerprints.cfp"));*size=4;*revision=1;return 0;}
static int32_t read_file(void*c,const char*n,uint64_t revision,void*out,uint32_t cap,uint32_t*size,uint64_t*current){(void)c;(void)n;assert(paused&&revision==1&&cap>=4);memcpy(out,disk[source],4);*size=4;*current=1;return 0;}
static int32_t replace_file(void*c,const char*n,uint64_t revision,const void*data,uint32_t size){(void)c;(void)n;assert(paused&&revision==1&&size==4);++writes;if(write_error)return write_error;memcpy(disk[source],data,4);return 0;}
static const risc_app_data_v1 files={.api_version=1,.struct_size=sizeof(files),.stat=stat_file,.read=read_file,.replace=replace_file};
static bool acquire(const char*name,uint32_t version,uint64_t instance,risc_runtime_capability_v1*g){
    if(!strcmp(name,"rtc.clock"))return false;
    assert(version==1);
    if(!strcmp(name,CONTEXTS_SERVICE_CAPABILITY)){g->api=&service;return true;}
    if(!strcmp(name,RISC_APP_DATA_CAPABILITY)){assert(paused&&(instance==2||instance==3));source=instance-2;g->api=&files;return true;}
    return false;
}
static bool release(risc_runtime_capability_v1*g){++releases;if(!release_ok)return false;g->api=NULL;return true;}
static const risc_runtime_api_v1 runtime={.api_version=1,.struct_size=sizeof(runtime),.acquire=acquire,.release=release};
int main(void){
    portable_contexts_client c;dirty=0;
    assert(portable_contexts_open(&c,&runtime)&&imports==2&&!writes);
    dirty=3;paused=false;assert(portable_contexts_pause(&c));
    assert(!dirty&&saved==2&&writes==2&&exports==2&&!memcmp(disk[0],"new!",4));
    dirty=1;write_error=RISC_APP_DATA_NO_SPACE;assert(portable_contexts_pause(&c));
    assert(dirty==1&&saved==2&&c.fingerprint_error==RISC_APP_DATA_NO_SPACE);
    write_error=0;assert(portable_contexts_pause(&c)&&!dirty&&saved==3);
    dirty=2;write_error=RISC_APP_DATA_RETAINED;
    assert(!portable_contexts_pause(&c)&&c.fingerprint_store.api&&dirty==2);
    uint32_t previous_releases=releases,previous_writes=writes;
    assert(!portable_contexts_pause(&c)&&releases==previous_releases&&writes==previous_writes);
    puts("Fingerprint client: quiescent load/save, per-source grants, no-space retry, generation acknowledgment and retained storage fence PASS");
}
