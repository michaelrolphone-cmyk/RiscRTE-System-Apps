/* Bounded synthetic storage only. The production single-profile codec is reused. */
#include "PortableWifiProfiles.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
typedef struct {char key[16];unsigned size;unsigned char bytes[64];} cell;
static cell cells[1024];static unsigned count,reads,writes,fail_write,corrupt_read;static bool persist_failed;static unsigned busy_read,busy_write;
static cell *lookup(const char *key,bool create){for(unsigned i=0;i<count;i++)if(!strcmp(cells[i].key,key))return &cells[i];if(!create)return NULL;assert(count<1024);cell*c=&cells[count++];strcpy(c->key,key);return c;}
static int32_t get(void*c,const char*k,void*b,uint32_t cap,uint32_t*used){(void)c;++reads;*used=0;if(reads==busy_read)return RISC_KEY_VALUE_BUSY;cell*r=lookup(k,false);if(!r)return RISC_KEY_VALUE_NOT_FOUND;*used=r->size;if(cap<r->size)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(b,r->bytes,r->size);if(reads==corrupt_read)((unsigned char*)b)[0]^=1;return RISC_KEY_VALUE_OK;}
static int32_t put(void*c,const char*k,const void*b,uint32_t n){(void)c;++writes;if(writes==busy_write)return RISC_KEY_VALUE_BUSY;assert(n<=64&&strlen(k)<=15);bool fail=writes==fail_write;if(!fail||persist_failed){cell*r=lookup(k,true);r->size=n;memcpy(r->bytes,b,n);}return fail?RISC_KEY_VALUE_IO:RISC_KEY_VALUE_OK;}
static const risc_key_value_v1 kv={1,sizeof(kv),NULL,get,put};
static portable_wifi_credentials value(unsigned n){portable_wifi_credentials x={{0},{0}};snprintf(x.ssid,sizeof(x.ssid),"Fixture-%u",n);snprintf(x.password,sizeof(x.password),"synthetic-%u",n);return x;}
static void reset(void){memset(cells,0,sizeof(cells));count=reads=writes=fail_write=corrupt_read=0;persist_failed=false;busy_read=busy_write=0;}
static int find_profile(const char *ssid,unsigned *index){
 portable_wifi_profile_search search;int rc=portable_wifi_profile_search_begin(&kv,ssid,&search);unsigned steps=0;
 while(rc==PORTABLE_WIFI_PROFILE_AGAIN){unsigned before=reads;rc=portable_wifi_profile_search_step(&kv,&search);assert(reads-before<=4);assert(++steps<=search.count);}
 *index=search.index;return rc;
}
int main(void){
 portable_wifi_credentials out,first=value(0);unsigned slot=99;reset();
 assert(portable_wifi_credentials_save(&kv,&first)==0);unsigned before=writes;
 assert(portable_wifi_profile_load(&kv,0,&out)==0&&!memcmp(&out,&first,sizeof(out))&&writes==before);
 assert(find_profile(first.ssid,&slot)==0&&slot==0&&writes==before);
 for(unsigned i=1;i<50u;i++){portable_wifi_credentials x=value(i);assert(find_profile(x.ssid,&slot)==PORTABLE_WIFI_CREDENTIALS_EMPTY&&slot==i);assert(portable_wifi_profile_save(&kv,slot,&x)==0);}
 for(unsigned i=0;i<50u;i++){portable_wifi_credentials x=value(i);assert(portable_wifi_profile_load(&kv,i,&out)==0&&!memcmp(&out,&x,sizeof(out)));}
 assert(find_profile("new-network",&slot)==PORTABLE_WIFI_CREDENTIALS_EMPTY&&slot==50);
 assert(portable_wifi_credentials_load(&kv,&out)==0&&!memcmp(&out,&first,sizeof(out))); /* Legacy default remains exact. */
 before=writes;assert(portable_wifi_profile_save(&kv,52,&first)==PORTABLE_WIFI_CREDENTIALS_INVALID&&writes==before);
 assert(portable_wifi_profile_forget(&kv,3)==PORTABLE_WIFI_CREDENTIALS_EMPTY);assert(find_profile("new-network",&slot)==PORTABLE_WIFI_CREDENTIALS_EMPTY&&slot==3);
 assert(portable_wifi_profile_load(&kv,3,&out)==PORTABLE_WIFI_CREDENTIALS_EMPTY&&!out.ssid[0]);
 cell*r=lookup("w00000002.c",false);assert(r);r->bytes[0]^=1;assert(find_profile("new-network",&slot)==PORTABLE_WIFI_CREDENTIALS_INVALID);
 /* Fail each transaction write with/without persistence. Only a fully verified
  * selector may expose a profile; neighboring credentials remain unchanged. */
 for(unsigned persists=0;persists<2;persists++)for(unsigned fault=1;fault<=4;fault++){
  reset();assert(portable_wifi_profile_save(&kv,0,&first)==0);portable_wifi_credentials second=value(2);writes=0;fail_write=fault;persist_failed=persists;
  int rc=portable_wifi_profile_save(&kv,1,&second);assert(rc==(persists?0:PORTABLE_WIFI_CREDENTIALS_UNCONFIRMED));
  assert(portable_wifi_profile_load(&kv,0,&out)==0&&!memcmp(&out,&first,sizeof(out)));
  int loaded=portable_wifi_profile_load(&kv,1,&out);assert(persists?(loaded==0&&!memcmp(&out,&second,sizeof(out))):(loaded==PORTABLE_WIFI_CREDENTIALS_EMPTY&&!out.ssid[0]));
 }
 /* Every load read may defer without changing data or producing a default. */
 for(unsigned phase=1;phase<=4;phase++){
  reset();assert(portable_wifi_profile_save(&kv,0,&first)==0);reads=0;busy_read=phase;before=writes;
  memset(&out,0xa5,sizeof(out));assert(portable_wifi_profile_load(&kv,0,&out)==PORTABLE_WIFI_CREDENTIALS_AGAIN&&!out.ssid[0]&&!out.password[0]&&writes==before);
  busy_read=0;assert(portable_wifi_profile_load(&kv,0,&out)==0&&!memcmp(&out,&first,sizeof(out)));
 }
 /* Search count or current slot BUSY does not skip or corrupt its cursor. */
 reset();assert(portable_wifi_profile_save(&kv,0,&first)==0);portable_wifi_profile_search search;busy_read=reads+1;
 assert(portable_wifi_profile_search_begin(&kv,first.ssid,&search)==PORTABLE_WIFI_PROFILE_AGAIN&&!search.initialized);
 busy_read=0;assert(portable_wifi_profile_search_step(&kv,&search)==0&&search.index==0);
 assert(portable_wifi_profile_search_begin(&kv,first.ssid,&search)==PORTABLE_WIFI_PROFILE_AGAIN);busy_read=reads+1;
 assert(portable_wifi_profile_search_step(&kv,&search)==PORTABLE_WIFI_PROFILE_AGAIN&&search.next==0&&!search.fault);
 busy_read=0;assert(portable_wifi_profile_search_step(&kv,&search)==0&&search.index==0);
 /* No-I/O BUSY before the first write is retryable. After any chunk/selector
  * write, uncertainty remains explicit instead of pretending no change. */
 for(unsigned phase=1;phase<=3;phase++){
  reset();assert(portable_wifi_profile_save(&kv,0,&first)==0);portable_wifi_credentials next=value(9);writes=0;busy_write=phase;
  int rc=portable_wifi_credentials_save(&kv,&next);assert(rc==(phase==1?PORTABLE_WIFI_CREDENTIALS_AGAIN:PORTABLE_WIFI_CREDENTIALS_UNCONFIRMED));
  busy_write=0;assert(portable_wifi_profile_load(&kv,0,&out)==0&&!memcmp(&out,&first,sizeof(out)));
 }
 reset();assert(portable_wifi_profile_save(&kv,0,&first)==0);busy_read=reads+1;before=writes;
 assert(portable_wifi_profile_forget(&kv,0)==PORTABLE_WIFI_CREDENTIALS_AGAIN&&writes==before);
 busy_read=0;busy_write=writes+1;assert(portable_wifi_profile_forget(&kv,0)==PORTABLE_WIFI_CREDENTIALS_AGAIN);
 busy_write=0;assert(portable_wifi_profile_load(&kv,0,&out)==0&&!memcmp(&out,&first,sizeof(out)));
 puts("Wi-Fi growing 50-profile isolation, legacy compatibility, capacity and transaction faults PASS");return 0;
}
