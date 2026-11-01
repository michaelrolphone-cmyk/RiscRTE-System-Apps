#pragma once
/* Last observed battery only. No address, name, pairing identity, timestamp or
 * automatic selection is persisted. Restored readings are always old.
 * The caller closes receiver/native RF custody before load/flush. */
#include "RiscRuntimeV1.h"
#include "RiscKeyValueV1.h"
#include <string.h>
#define PQB_CACHE_KEY "remote_battery"
#define PQB_CACHE_BYTES 8u
typedef enum { PQB_CACHE_UNREAD, PQB_CACHE_MISSING, PQB_CACHE_SAVED,
 PQB_CACHE_UNAVAILABLE, PQB_CACHE_INVALID, PQB_CACHE_PENDING,
 PQB_CACHE_UNCONFIRMED, PQB_CACHE_RETAINED } pqb_cache_status;
typedef struct {
 pqb_cache_status status;
 bool valid,loaded,dirty,retained;
 uint8_t percent,pending[PQB_CACHE_BYTES];
 const risc_runtime_api_v1 *runtime;
 risc_runtime_capability_v1 grant;
 bool (*alive)(void *);
 void *context;
} pqb_cache;
static inline uint32_t pqb_cache_crc(const uint8_t *p,unsigned n) {
 uint32_t c=0xffffffffu;
 for(unsigned i=0;i<n;i++){c^=p[i];for(unsigned b=0;b<8;b++)c=(c>>1)^((0u-(c&1u))&0xedb88320u);}
 return ~c;
}
static inline bool pqb_cache_encode(uint8_t out[PQB_CACHE_BYTES],unsigned percent) {
 if(!out||percent>100)return false;
 out[0]='R';out[1]='B';out[2]=1;out[3]=(uint8_t)percent;
 uint32_t c=pqb_cache_crc(out,4);for(unsigned i=0;i<4;i++)out[4+i]=(uint8_t)(c>>(8*i));
 return true;
}
static inline bool pqb_cache_decode(const void *data,uint32_t size,uint8_t *percent) {
 if(!data||!percent||size!=PQB_CACHE_BYTES)return false;
 const uint8_t *p=data;
 uint32_t c=0;for(unsigned i=0;i<4;i++)c|=(uint32_t)p[4+i]<<(8*i);
 if(p[0]!='R'||p[1]!='B'||p[2]!=1||p[3]>100||c!=pqb_cache_crc(p,4))return false;
 *percent=p[3];return true;
}
static inline bool pqb_cache_fail(pqb_cache *c) {
 c->retained=true;c->status=PQB_CACHE_RETAINED;return false;
}
static inline bool pqb_cache_live(pqb_cache *c) {
 if(c->retained)return false;
 if(!c->alive||!c->alive(c->context))return pqb_cache_fail(c);
 return true;
}
static inline bool pqb_cache_empty(const risc_runtime_capability_v1 *g) {
 return g->struct_size==sizeof(*g)&&!g->api&&!g->slot&&!g->generation;
}
static inline bool pqb_cache_open(pqb_cache *c,const risc_runtime_api_v1 *rt,uint64_t instance) {
 if(!pqb_cache_live(c))return false;
 if(c->grant.api||c->grant.slot||c->grant.generation)return pqb_cache_fail(c);
 if(!rt||rt->api_version!=1||rt->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE||
    !rt->acquire||!rt->release||!instance){c->status=PQB_CACHE_UNAVAILABLE;return false;}
 c->runtime=rt;c->grant=(risc_runtime_capability_v1){.struct_size=sizeof(c->grant)};
 bool ok=rt->acquire(RISC_KEY_VALUE_CAPABILITY,1,instance,&c->grant);
 if(!pqb_cache_live(c))return false;
 if(!ok){if(!pqb_cache_empty(&c->grant))return pqb_cache_fail(c);c->status=PQB_CACHE_UNAVAILABLE;return false;}
 const risc_key_value_v1 *api=c->grant.api;
 if(c->grant.struct_size!=sizeof(c->grant)||!c->grant.slot||!c->grant.generation||
    !api||api->api_version!=1||api->struct_size<sizeof(*api)||!api->get||!api->put)return pqb_cache_fail(c);
 return true;
}
static inline bool pqb_cache_close(pqb_cache *c) {
 if(!pqb_cache_live(c))return false;
 risc_runtime_capability_v1 g=c->grant;
 bool ok=c->runtime->release(&g);
 if(!pqb_cache_live(c))return false;
 if(!ok||!pqb_cache_empty(&g))return pqb_cache_fail(c);
 c->grant=g;c->runtime=NULL;return true;
}
static inline bool pqb_cache_result(pqb_cache *c,int32_t rc) {
 if(!pqb_cache_live(c))return false;
 if(rc!=RISC_KEY_VALUE_OK&&rc!=RISC_KEY_VALUE_NOT_FOUND&&rc!=RISC_KEY_VALUE_BUFFER_SMALL&&
    rc!=RISC_KEY_VALUE_INVALID&&rc!=RISC_KEY_VALUE_IO&&rc!=RISC_KEY_VALUE_BUSY)return pqb_cache_fail(c);
 return true;
}
static inline bool pqb_cache_load(pqb_cache *c,const risc_runtime_api_v1 *rt,uint64_t instance) {
 if(!c||c->loaded||c->retained)return c&&!c->retained;
 if(!pqb_cache_open(c,rt,instance))return !c->retained;
 const risc_key_value_v1 *api=c->grant.api;uint8_t bytes[PQB_CACHE_BYTES]={0},value=0;uint32_t n=0;
 int32_t rc=api->get(api->context,PQB_CACHE_KEY,bytes,sizeof(bytes),&n);
 if(!pqb_cache_result(c,rc))return false;
 bool valid=rc==RISC_KEY_VALUE_OK&&pqb_cache_decode(bytes,n,&value);
 if(!pqb_cache_close(c))return false;
 c->loaded=rc!=RISC_KEY_VALUE_BUSY;
 c->valid=valid;if(valid)c->percent=value;
 c->status=valid?PQB_CACHE_SAVED:rc==RISC_KEY_VALUE_NOT_FOUND?PQB_CACHE_MISSING:
   rc==RISC_KEY_VALUE_OK?PQB_CACHE_INVALID:PQB_CACHE_UNAVAILABLE;
 return true;
}
static inline void pqb_cache_observe(pqb_cache *c,uint8_t percent) {
 if(!c||c->retained||percent>100)return;
 /* A live observation supersedes an unfinished old-cache load. Never let a
  * late load replace percent while pending still encodes the new reading. */
 c->loaded=true;
 /* Identical observations do not wear storage or replay an ambiguous write. */
 if(c->valid&&c->percent==percent)return;
 c->valid=true;c->percent=percent;c->dirty=true;
 (void)pqb_cache_encode(c->pending,percent);c->status=PQB_CACHE_PENDING;
}
static inline bool pqb_cache_flush(pqb_cache *c,const risc_runtime_api_v1 *rt,uint64_t instance) {
 if(!c||c->retained)return false;
 if(!c->dirty)return true;
 if(!pqb_cache_open(c,rt,instance))return !c->retained;
 const risc_key_value_v1 *api=c->grant.api;
 int32_t rc=api->put(api->context,PQB_CACHE_KEY,c->pending,PQB_CACHE_BYTES);
 if(!pqb_cache_result(c,rc))return false;
 if(!pqb_cache_close(c))return false;
 if(rc==RISC_KEY_VALUE_OK){c->dirty=false;c->status=PQB_CACHE_SAVED;}
 else if(rc==RISC_KEY_VALUE_BUSY)c->status=PQB_CACHE_PENDING;
 else {c->dirty=false;c->status=PQB_CACHE_UNCONFIRMED;}
 return true;
}
