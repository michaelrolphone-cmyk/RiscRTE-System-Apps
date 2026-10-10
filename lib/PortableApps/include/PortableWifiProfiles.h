#pragma once
/* Eight bounded networks reuse the existing per-profile atomic credential
 * codec. Slot zero uses the original five keys verbatim: existing saved-network
 * consumers keep their current default, with no implicit migration or writes.
 * Other slots have independent selectors; no catalog, eviction or recovery of
 * orphan chunks is needed. Callers own namespace 6 and checked radio cleanup. */
#include "PortableWifiCredentials.h"
#define PORTABLE_WIFI_PROFILE_COUNT 8u
#define PORTABLE_WIFI_PROFILE_FULL 6

typedef struct {
    const risc_key_value_v1 *source;
    unsigned index;
} portable_wifi_profile_context;

static inline bool portable_wifi_profile_key(unsigned index,const char *key,char out[16]) {
    if(index>=PORTABLE_WIFI_PROFILE_COUNT||!key)return false;
    for(unsigned i=0;i<5;i++)if(key[i]!="wifi."[i])return false;
    size_t n=0;while(n<15&&key[n])++n;
    if(n==15)return false;
    memcpy(out,key,n+1);
    if(index){out[0]='w';out[1]='f';out[2]=(char)('0'+index);memcpy(out+3,key+4,n-3);}
    return true;
}
static inline int32_t portable_wifi_profile_get(void *context,const char *key,void *data,uint32_t capacity,uint32_t *size) {
    portable_wifi_profile_context *c=context;char mapped[16];
    if(!portable_wifi_profile_key(c->index,key,mapped)){if(size)*size=0;return RISC_KEY_VALUE_INVALID;}
    return c->source->get(c->source->context,mapped,data,capacity,size);
}
static inline int32_t portable_wifi_profile_put(void *context,const char *key,const void *data,uint32_t size) {
    portable_wifi_profile_context *c=context;char mapped[16];
    if(!portable_wifi_profile_key(c->index,key,mapped))return RISC_KEY_VALUE_INVALID;
    return c->source->put(c->source->context,mapped,data,size);
}
static inline risc_key_value_v1 portable_wifi_profile_api(portable_wifi_profile_context *c) {
    return (risc_key_value_v1){1,sizeof(risc_key_value_v1),c,portable_wifi_profile_get,portable_wifi_profile_put};
}
static inline int portable_wifi_profile_load(const risc_key_value_v1 *kv,unsigned index,portable_wifi_credentials *out) {
    if(!out)return PORTABLE_WIFI_CREDENTIALS_INVALID;
    portable_wifi_credentials_clear(out);
    if(index>=PORTABLE_WIFI_PROFILE_COUNT)return PORTABLE_WIFI_CREDENTIALS_INVALID;
    if(!portable_wifi_credentials_api_valid(kv))return PORTABLE_WIFI_CREDENTIALS_UNAVAILABLE;
    portable_wifi_profile_context c={kv,index};risc_key_value_v1 selected=portable_wifi_profile_api(&c);
    return portable_wifi_credentials_load(&selected,out);
}
static inline int portable_wifi_profile_save(const risc_key_value_v1 *kv,unsigned index,const portable_wifi_credentials *value) {
    if(index>=PORTABLE_WIFI_PROFILE_COUNT)return PORTABLE_WIFI_CREDENTIALS_INVALID;
    if(!portable_wifi_credentials_api_valid(kv))return PORTABLE_WIFI_CREDENTIALS_UNAVAILABLE;
    portable_wifi_profile_context c={kv,index};risc_key_value_v1 selected=portable_wifi_profile_api(&c);
    return portable_wifi_credentials_save(&selected,value);
}
static inline int portable_wifi_profile_forget(const risc_key_value_v1 *kv,unsigned index) {
    if(index>=PORTABLE_WIFI_PROFILE_COUNT)return PORTABLE_WIFI_CREDENTIALS_INVALID;
    if(!portable_wifi_credentials_api_valid(kv))return PORTABLE_WIFI_CREDENTIALS_UNAVAILABLE;
    portable_wifi_profile_context c={kv,index};risc_key_value_v1 selected=portable_wifi_profile_api(&c);
    return portable_wifi_credentials_forget(&selected);
}
/* A save updates the matching SSID, otherwise takes the first proven-empty
 * slot. Any unreadable/invalid slot refuses insertion: never overwrite uncertain
 * credentials or create a hidden duplicate. No automatic eviction occurs. */
static inline int portable_wifi_profile_find(const risc_key_value_v1 *kv,const char *ssid,unsigned *index) {
    if(!ssid||!index)return PORTABLE_WIFI_CREDENTIALS_INVALID;
    unsigned empty=PORTABLE_WIFI_PROFILE_COUNT;int fault=PORTABLE_WIFI_CREDENTIALS_LOADED;
    for(unsigned i=0;i<PORTABLE_WIFI_PROFILE_COUNT;i++){
        portable_wifi_credentials value={{0},{0}};
        int rc=portable_wifi_profile_load(kv,i,&value);
        bool same=rc==PORTABLE_WIFI_CREDENTIALS_LOADED&&!strcmp(value.ssid,ssid);
        portable_wifi_credentials_clear(&value);
        if(same){*index=i;return PORTABLE_WIFI_CREDENTIALS_LOADED;}
        if(rc==PORTABLE_WIFI_CREDENTIALS_EMPTY&&empty==PORTABLE_WIFI_PROFILE_COUNT)empty=i;
        else if(rc!=PORTABLE_WIFI_CREDENTIALS_EMPTY&&rc!=PORTABLE_WIFI_CREDENTIALS_LOADED)fault=rc;
    }
    if(fault!=PORTABLE_WIFI_CREDENTIALS_LOADED)return fault;
    if(empty==PORTABLE_WIFI_PROFILE_COUNT)return PORTABLE_WIFI_PROFILE_FULL;
    *index=empty;return PORTABLE_WIFI_CREDENTIALS_EMPTY;
}
