#pragma once
#include "ContextFingerprintService.h"
#include "RiscAppDataV1.h"
#include "PortableFingerprintClock.h"
/* Ordinary app-owned checkpoints, after capture quiescence. The service keeps
 * copied models only. Source namespaces are the existing Audio@2 and RF@3. */
static uint8_t portable_fp_bytes[60000];
static inline bool portable_fp_record(portable_contexts_client*c,unsigned source,bool load){
    if(c->fingerprint_store.api)return false; /* terminal retained storage fence */
    const contexts_fingerprint_service_v1*api=contexts_fingerprint_api(c->api);
    if(!api)return true;
    contexts_fingerprint_status_v1 status={.struct_size=sizeof(status)};
    if(!api->fingerprint(api->base.context,CONTEXTS_FP_STATUS,&status))return false;
    if(load&&!(status.sources&source))return true;
    if(!load&&!(status.dirty_sources&source))return true;
    if(!load&&(c->fingerprint_unread&source)){c->fingerprint_error=RISC_APP_DATA_INVALID;return true;}
    if(load&&(status.dirty_sources&source))return true;
    c->fingerprint_store=(risc_runtime_capability_v1){.struct_size=sizeof(c->fingerprint_store)};
    if(!c->runtime->acquire(RISC_SHARED_DATA_CAPABILITY,1,source==CONTEXTS_AUDIO?2:3,&c->fingerprint_store)){
        c->fingerprint_error=RISC_APP_DATA_UNAVAILABLE;if(load)c->fingerprint_unread|=source;return true;
    }
    const risc_app_data_v1*files=c->fingerprint_store.api;
    int32_t rc=RISC_APP_DATA_INVALID;uint32_t size=0;uint64_t revision=0;
    contexts_fingerprint_record_v1 record={.struct_size=sizeof(record),.source=source,.bytes=portable_fp_bytes,.capacity=sizeof(portable_fp_bytes)};
    if(files&&files->api_version==1&&files->struct_size>=sizeof(*files)&&files->stat&&files->read&&files->replace){
        rc=files->stat(files->context,"context-fingerprints.cfp",&size,&revision);
        if(load){
            if(!rc&&size<=sizeof(portable_fp_bytes)&&revision){
                uint64_t current=0;rc=files->read(files->context,"context-fingerprints.cfp",revision,portable_fp_bytes,sizeof(portable_fp_bytes),&record.size,&current);
                if(!rc&&(current!=revision||record.size!=size||!api->fingerprint(api->base.context,CONTEXTS_FP_IMPORT,&record)))rc=RISC_APP_DATA_INVALID;
            }else if(!rc)rc=RISC_APP_DATA_INVALID;
            if(rc==RISC_APP_DATA_NOT_FOUND)rc=RISC_APP_DATA_OK;
        }else{
            if(rc==RISC_APP_DATA_NOT_FOUND){rc=0;revision=0;}
            if(!rc&&api->fingerprint(api->base.context,CONTEXTS_FP_EXPORT,&record)){
                rc=files->replace(files->context,"context-fingerprints.cfp",revision,portable_fp_bytes,record.size);
                if(!rc&&!api->fingerprint(api->base.context,CONTEXTS_FP_SAVED,&record))rc=RISC_APP_DATA_STALE;
            }else if(!rc)rc=RISC_APP_DATA_INVALID;
        }
    }
    if(load){if(rc)c->fingerprint_unread|=source;else c->fingerprint_unread&=~source;}
    if(rc)c->fingerprint_error=rc;
    if(rc==RISC_APP_DATA_RETAINED)return false;
    if(!c->runtime->release(&c->fingerprint_store))return false;
    memset(&c->fingerprint_store,0,sizeof(c->fingerprint_store));return true;
}
static inline bool portable_fp_checkpoint(portable_contexts_client*c,bool load){
    if(c->fingerprint_store.api)return false;
    if(!contexts_fingerprint_api(c->api))return true;
    c->fingerprint_error=0;
    if(!c->api->pause(c->api->context))return false;
    uint64_t utc=0;if(!portable_fingerprint_clock(c->runtime,&utc))return false;
    contexts_fingerprint_config_v1 cfg={.struct_size=sizeof(cfg),.sources=c->fingerprint_sources,.temporal_only=c->fingerprint_temporal_only,.utc_seconds=utc};
    const contexts_fingerprint_service_v1*fp=contexts_fingerprint_api(c->api);
    if(!fp->fingerprint(fp->base.context,CONTEXTS_FP_CONFIG,&cfg))return false;
    if(!portable_fp_record(c,CONTEXTS_AUDIO,load)||!portable_fp_record(c,CONTEXTS_RADIO,load))return false;
    return true;
}
