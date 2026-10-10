#include "PortableBroadcastClient.h"
#include <assert.h>
#include <stdio.h>
static unsigned now,reads,writes,steps,pauses,grants,release_failures;
static bool stored,radio_stored,read_fail,write_fail,commit_io,live,health_lost;
static uint8_t record[4],radio_record[4];
static telemetry_broadcast_policy_v1 copied;
static bool health(risc_runtime_health_v1*out){out->uptime_ms=now;return !health_lost;}
static int32_t get(void*c,const char*k,void*b,uint32_t cap,uint32_t*n){(void)c;assert(cap==4);reads++;*n=0;if(read_fail)return RISC_KEY_VALUE_IO;const uint8_t*p;if(!strcmp(k,TELEMETRY_BROADCAST_KEY)){if(!stored)return RISC_KEY_VALUE_NOT_FOUND;p=record;}else{assert(!strcmp(k,PORTABLE_RADIO_KEY));if(!radio_stored)return RISC_KEY_VALUE_NOT_FOUND;p=radio_record;}memcpy(b,p,4);*n=4;return RISC_KEY_VALUE_OK;}
static int32_t put(void*c,const char*k,const void*b,uint32_t n){(void)c;assert(!strcmp(k,TELEMETRY_BROADCAST_KEY)&&n==4);writes++;if(write_fail)return RISC_KEY_VALUE_IO;memcpy(record,b,4);stored=true;return commit_io?RISC_KEY_VALUE_IO:RISC_KEY_VALUE_OK;}
static const risc_key_value_v1 kv={1,sizeof(kv),NULL,get,put};
static bool step(void*c,bool allow,const telemetry_broadcast_policy_v1*p){(void)c;steps++;copied=*p;live=allow&&p->enabled&&p->settings_valid&&p->radios_allowed;return true;}
static bool pause(void*c){(void)c;pauses++;live=false;return true;}
static bool status(void*c,telemetry_broadcast_status_v1*p){(void)c;*p=(telemetry_broadcast_status_v1){.struct_size=sizeof(*p)};return true;}
static int32_t enumerate(void*c,uint32_t i,risc_telemetry_field_v1*p){(void)c;(void)i;(void)p;return 0;}
static int32_t read_value(void*c,uint32_t i,int32_t*p){(void)c;(void)i;(void)p;return 0;}
static const telemetry_broadcast_v1 service={1,sizeof(service),NULL,step,pause,status,enumerate,read_value};
static bool acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){assert(v==1);if(!strcmp(n,TELEMETRY_BROADCAST_CAPABILITY)){assert(!id);g->api=&service;}else{assert(!strcmp(n,RISC_KEY_VALUE_CAPABILITY)&&id==1);g->api=&kv;}grants++;return true;}
static bool release(risc_runtime_capability_v1*g){assert(grants&&g->api);if(release_failures){release_failures--;return false;}grants--;g->api=NULL;return true;}
static const risc_runtime_api_v1 rt={.api_version=1,.struct_size=sizeof(rt),.health=health,.acquire=acquire,.release=release};
static void radio(unsigned flags){radio_stored=true;uint8_t b[]={0x51,1,flags,flags^0xa5};memcpy(radio_record,b,4);}
int main(void){
 portable_broadcast_client c;assert(portable_broadcast_open(&c,&rt)&&grants==1&&!reads&&!live);
 assert(portable_broadcast_step(&c,true)&&copied.enabled&&copied.settings_valid&&!copied.radios_allowed&&!live);
 unsigned n=reads;for(unsigned i=0;i<20;i++){now+=25;assert(portable_broadcast_step(&c,true));}assert(reads==n);
 radio(3);assert(portable_broadcast_pause(&c)&&portable_broadcast_step(&c,true)&&live);
 radio(1);assert(portable_broadcast_pause(&c)&&portable_broadcast_step(&c,true)&&!live);
 radio(3);now+=1000;assert(portable_broadcast_step(&c,true)&&live);
 assert(portable_broadcast_set_enabled(&c,false)&&!live&&!copied.enabled&&stored&&writes==1);
 assert(portable_broadcast_step(&c,true)&&!live&&!copied.enabled);
 assert(portable_broadcast_close(&c,&rt)&&!grants);assert(portable_broadcast_open(&c,&rt)&&portable_broadcast_step(&c,true)&&!copied.enabled);
 commit_io=true;assert(portable_broadcast_set_enabled(&c,true)&&portable_broadcast_step(&c,true)&&live);
 write_fail=true;assert(!portable_broadcast_set_enabled(&c,false)&&!live&&!copied.settings_valid);n=reads;now+=10000;assert(portable_broadcast_step(&c,true)&&!live&&reads==n);write_fail=false;assert(portable_broadcast_set_enabled(&c,false)&&!copied.enabled);
 record[3]^=1;assert(portable_broadcast_pause(&c)&&portable_broadcast_step(&c,true)&&!copied.settings_valid&&!live);assert(portable_broadcast_set_enabled(&c,true)&&portable_broadcast_step(&c,true)&&live);
 read_fail=true;now+=1000;assert(portable_broadcast_step(&c,true)&&!live&&!copied.settings_valid);read_fail=false;now+=1000;assert(portable_broadcast_step(&c,true)&&live);
 release_failures=2;now+=1000;assert(!portable_broadcast_step(&c,true)&&!live&&c.storage.api&&grants==2);n=reads;
 assert(!portable_broadcast_step(&c,true)&&reads==n&&!live);assert(portable_broadcast_step(&c,true)&&!c.storage.api&&grants==1&&live);
 health_lost=true;assert(portable_broadcast_step(&c,true)&&!live);health_lost=false;assert(portable_broadcast_step(&c,true)&&live);
 n=pauses;assert(portable_broadcast_close(&c,&rt)&&!grants&&live&&pauses==n);
 puts("Broadcast client: app-only namespace1 policy, paced reads, exact-write reconciliation, disabled restart, radio overrides, corrupt/unread records, failed-save latch, storage-release retry and healthy grant release PASS");
}
