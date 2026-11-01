#include "PortableRemoteBatteryCache.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static bool live=true,acquire_ok=true,release_ok=true,dirty_acquire=false,dirty_release=false,kill=false;
static int32_t read_rc=RISC_KEY_VALUE_NOT_FOUND,write_rc=0;
static uint8_t persisted[8];static uint32_t persisted_size;
static unsigned acquired,released,reads,writes;
static risc_key_value_v1 api;
static bool alive(void *p){(void)p;return live;}
static bool acquire(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){
 assert(live);assert(!strcmp(n,RISC_KEY_VALUE_CAPABILITY)&&v==1&&i==1);
 acquired++;
 if(acquire_ok||dirty_acquire){g->api=&api;g->slot=1;g->generation=2;}
 return acquire_ok;
}
static bool release(risc_runtime_capability_v1*g){
 assert(live);released++;
 if(release_ok&&!dirty_release)*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};
 return release_ok;
}
static int32_t get(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){
 (void)c;assert(live);assert(!strcmp(k,PQB_CACHE_KEY)&&cap==8);reads++;
 if(kill){live=false;return RISC_KEY_VALUE_CONTEXT;}
 if(read_rc==0){memcpy(b,persisted,8);*n=persisted_size;}else *n=0;
 return read_rc;
}
static int32_t put(void*c,const char*k,const void*b,uint32_t n){
 (void)c;assert(live);assert(!strcmp(k,PQB_CACHE_KEY)&&n==8);writes++;
 if(kill){live=false;return RISC_KEY_VALUE_CONTEXT;}
 if(write_rc==0){memcpy(persisted,b,8);persisted_size=8;read_rc=0;}
 return write_rc;
}
static risc_runtime_api_v1 rt;
static pqb_cache fresh(void){
 live=acquire_ok=release_ok=true;dirty_acquire=dirty_release=kill=false;
 read_rc=RISC_KEY_VALUE_NOT_FOUND;write_rc=0;persisted_size=0;
 acquired=released=reads=writes=0;memset(persisted,0,8);
 api=(risc_key_value_v1){1,sizeof(api),NULL,get,put};
 rt=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(rt),.acquire=acquire,.release=release};
 return (pqb_cache){.alive=alive};
}
int main(void){
 unsigned cases=0;uint8_t b[8],v=255;
 for(unsigned p=0;p<=100;p++){assert(pqb_cache_encode(b,p));assert(pqb_cache_decode(b,8,&v)&&v==p);}
 assert(!pqb_cache_encode(b,101));cases++;
 for(unsigned i=0;i<8;i++){pqb_cache_encode(b,50);b[i]^=1;v=255;assert(!pqb_cache_decode(b,8,&v)&&v==255);}cases++;
 pqb_cache c=fresh();assert(pqb_cache_load(&c,&rt,1)&&c.loaded&&!c.valid&&!writes&&released==1);cases++;
 c=fresh();pqb_cache_observe(&c,73);assert(c.dirty);assert(pqb_cache_flush(&c,&rt,1)&&c.status==PQB_CACHE_SAVED&&writes==1);cases++;
 pqb_cache restarted={.alive=alive};assert(pqb_cache_load(&restarted,&rt,1)&&restarted.valid&&restarted.percent==73&&!restarted.dirty);assert(writes==1);cases++;
 pqb_cache_observe(&restarted,73);assert(pqb_cache_flush(&restarted,&rt,1)&&writes==1);cases++;
 c=fresh();pqb_cache_encode(persisted,20);persisted[5]^=1;persisted_size=8;read_rc=0;assert(pqb_cache_load(&c,&rt,1)&&!c.valid&&c.status==PQB_CACHE_INVALID&&!writes);cases++;
 c=fresh();acquire_ok=false;assert(pqb_cache_load(&c,&rt,1)&&!c.retained&&c.status==PQB_CACHE_UNAVAILABLE&&!reads&&!released);cases++;
 c=fresh();acquire_ok=false;dirty_acquire=true;assert(!pqb_cache_load(&c,&rt,1)&&c.retained&&!reads&&!released);cases++;
 c=fresh();release_ok=false;assert(!pqb_cache_load(&c,&rt,1)&&c.retained&&c.grant.api);unsigned calls=acquired+reads+released+writes;assert(!pqb_cache_flush(&c,&rt,1));assert(calls==acquired+reads+released+writes);cases++;
 c=fresh();dirty_release=true;assert(!pqb_cache_load(&c,&rt,1)&&c.retained);cases++;
 c=fresh();read_rc=RISC_KEY_VALUE_CONTEXT;assert(!pqb_cache_load(&c,&rt,1)&&c.retained&&!released);cases++;
 c=fresh();read_rc=123;assert(!pqb_cache_load(&c,&rt,1)&&c.retained&&!released);cases++;
 c=fresh();kill=true;assert(!pqb_cache_load(&c,&rt,1)&&c.retained&&!released);cases++;
 c=fresh();pqb_cache_observe(&c,40);write_rc=RISC_KEY_VALUE_BUSY;assert(pqb_cache_flush(&c,&rt,1)&&c.dirty);write_rc=0;assert(pqb_cache_flush(&c,&rt,1)&&!c.dirty&&writes==2);cases++;
 c=fresh();pqb_cache_observe(&c,40);write_rc=RISC_KEY_VALUE_IO;assert(pqb_cache_flush(&c,&rt,1)&&!c.dirty&&c.status==PQB_CACHE_UNCONFIRMED);pqb_cache_observe(&c,40);assert(pqb_cache_flush(&c,&rt,1)&&writes==1);cases++;
 c=fresh();pqb_cache_observe(&c,40);write_rc=RISC_KEY_VALUE_CONTEXT;assert(!pqb_cache_flush(&c,&rt,1)&&c.retained&&!released);cases++;
 c=fresh();read_rc=RISC_KEY_VALUE_BUSY;assert(pqb_cache_load(&c,&rt,1)&&!c.loaded);read_rc=RISC_KEY_VALUE_NOT_FOUND;assert(pqb_cache_load(&c,&rt,1)&&c.loaded&&reads==2);cases++;
 c=fresh();live=false;assert(!pqb_cache_load(&c,&rt,1)&&c.retained&&!acquired);cases++;
 c=fresh();pqb_cache_observe(&c,101);assert(!c.valid&&!c.dirty&&!writes);cases++;
 c=fresh();read_rc=RISC_KEY_VALUE_BUSY;assert(pqb_cache_load(&c,&rt,1)&&!c.loaded);
 pqb_cache_observe(&c,20);pqb_cache_encode(persisted,10);persisted_size=8;read_rc=0;
 assert(pqb_cache_load(&c,&rt,1)&&c.percent==20&&c.dirty&&reads==1);
 assert(pqb_cache_flush(&c,&rt,1)&&persisted[3]==20);
 pqb_cache_observe(&c,10);assert(c.dirty&&pqb_cache_flush(&c,&rt,1)&&persisted[3]==10&&writes==2);cases++;
 printf("remote battery cache: %u cases passed; no radio or physical storage endpoints\n",cases);return 0;
}
