#include "PortableAutomaticProvision.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned acquired,released,refreshes,local_steps,network_steps,automatic_steps,prepared;
static bool allowed=true,owner=true,utc_valid=true,media=true,cleanup_pending,network_pending,failed_release;
static int scan_result=0,auto_result=1;
static bool boot_extension,kv_busy,lose_read,lose_supply;
static unsigned supplied,kv_reads,kv_writes;
static unsigned char blobs[5][64];static uint32_t lengths[5];
static const char*keys[]={"wifi.commit","wifi.a0","wifi.a1","wifi.b0","wifi.b1"};
static int32_t kv_get(void*p,const char*k,void*out,uint32_t cap,uint32_t*n){(void)p;++kv_reads;*n=0;if(kv_busy)return RISC_KEY_VALUE_BUSY;if(lose_read)owner=false;
 for(unsigned i=0;i<5;++i)if(!strcmp(keys[i],k)){if(!lengths[i])return RISC_KEY_VALUE_NOT_FOUND;*n=lengths[i];if(cap<*n)return RISC_KEY_VALUE_BUFFER_SMALL;memcpy(out,blobs[i],*n);return 0;}
 return RISC_KEY_VALUE_NOT_FOUND;}
static int32_t kv_put(void*p,const char*k,const void*data,uint32_t n){(void)p;++kv_writes;for(unsigned i=0;i<5;++i)if(!strcmp(keys[i],k)){assert(n<=64);memcpy(blobs[i],data,n);lengths[i]=n;return 0;}assert(0);return -1;}
static const risc_key_value_v1 kv={1,sizeof(kv),NULL,kv_get,kv_put};
static int32_t supply(void*p,const char*s,const char*w){(void)p;++supplied;assert(!strcmp(s,"saved default")&&!strcmp(w,"saved-password"));if(lose_supply)owner=false;return INSTALLED_APPS_PROFILE_QUEUED;}

static bool network_allowed(void*p){(void)p;return allowed;}
static bool safe(void*p){(void)p;return owner;}
static bool prepare(void*p){(void)p;++prepared;return true;}
static bool utc(void*p,uint64_t*t){(void)p;*t=1800000000;return utc_valid;}
static void retained(void*p){(void)p;owner=false;}
static int refresh(void*p){(void)p;++refreshes;return 0;}
static int scan(void*p,const risc_storage_volume_api_v1*v){(void)p;assert(media?!!v:!v);++local_steps;return scan_result;}
static int cancel(void*p){(void)p;return cleanup_pending?1:0;}
static int begin(void*p,const char*profile,uint64_t now){(void)p;assert(!strcmp(profile,"core")&&now==1800000000);return 0;}
static int progress(void*p,const risc_storage_volume_api_v1*v){(void)p;assert(media?!!v:!v);++automatic_steps;return auto_result;}
static bool get(void*p,uint32_t i,installed_app_v1*out){(void)p;(void)i;(void)out;return false;}
static uint32_t count(void*p){(void)p;return 0;}
static int feed_start(void*p,uint64_t u){(void)p;(void)u;return 0;}
static int feed_step(void*p){(void)p;return 0;}
static int install(void*p,uint32_t i,uint32_t g){(void)p;(void)i;(void)g;return 0;}
static int descriptor(void*p,const char*s){(void)p;(void)s;return 0;}
static const installed_apps_automatic_v1 api={
 .base={.base={.base={.api_version=1,.struct_size=sizeof(api),.refresh=refresh,.step=scan,.cancel=cancel},.provision_tag=INSTALLED_APPS_PROVISION_TAG,.provision_version=1,.install_descriptor=descriptor},
 .feed_tag=INSTALLED_APPS_FEED_TAG,.feed_version=1,.refresh=feed_start,.step=feed_step,.cancel=cancel,.generation=count,.count=count,.get=get,.install=install},
 .automatic_tag=INSTALLED_APPS_AUTOMATIC_TAG,.automatic_version=1,.begin=begin,.step=progress,.cancel=cancel};
static installed_apps_boot_profile_v1 boot_api;
static bool acquire(const char*id,uint32_t v,uint64_t instance,risc_runtime_capability_v1*g){assert(v==1);++acquired;
 if(!strcmp(id,INSTALLED_APPS_CAPABILITY)){assert(!instance);g->api=boot_extension?(const void*)&boot_api:(const void*)&api;}
 else if(!strcmp(id,"storage.volume")){assert(instance==12);if(!media)return false;g->api=(void*)2;}
 else if(!strcmp(id,"net.wifi")){assert(instance==15);g->api=(void*)3;}
 else {assert(!strcmp(id,"storage.key-value")&&instance==6);g->api=boot_extension?(const void*)&kv:(void*)4;}return true;}
static bool release(risc_runtime_capability_v1*g){assert(!cleanup_pending&&!network_pending);if(failed_release)return false;++released;g->api=NULL;return true;}
void portable_network_session_init(portable_network_session*s,const wifi_api_v1*w,const risc_key_value_v1*k,bool(*ok)(void*),void*c){memset(s,0,sizeof(*s));assert(w==(void*)3&&(boot_extension?k==&kv:k==(void*)4)&&ok(c));}
portable_network_result portable_network_session_start(portable_network_session*s,uint32_t n){(void)s;(void)n;return PORTABLE_NETWORK_PENDING;}
portable_network_result portable_network_session_step(portable_network_session*s,uint32_t n){(void)s;(void)n;++network_steps;return PORTABLE_NETWORK_READY;}
portable_network_close_result portable_network_session_close(portable_network_session*s){(void)s;return network_pending?PORTABLE_NETWORK_CLOSE_PENDING:PORTABLE_NETWORK_CLOSED;}
portable_network_service_result portable_network_session_service_begin(portable_network_session*s){(void)s;return PORTABLE_NETWORK_SERVICE_OK;}
portable_network_service_result portable_network_session_service_end(portable_network_session*s){(void)s;return PORTABLE_NETWORK_SERVICE_OK;}
int main(void){risc_runtime_api_v1 rt={.acquire=acquire,.release=release};portable_automatic_hooks hooks={.runtime=&rt,.network_allowed=network_allowed,.safe=safe,.prepare=prepare,.utc=utc,.retain=retained,.profile="core",.wifi_instance=15,.settings_namespace=6,.volume_instance=12,.sd_enabled=true};portable_automatic_provision c;
 portable_automatic_init(&c,&hooks);assert(!acquired&&!refreshes&&!network_steps);
 assert(portable_automatic_step(&c,0)==1&&refreshes==1&&!network_steps);
 assert(portable_automatic_step(&c,1)==1&&local_steps==1&&!network_steps);
 for(unsigned i=2;i<6;++i)assert(portable_automatic_step(&c,i)==1);
 assert(automatic_steps&&prepared==1);
 cleanup_pending=true;unsigned old=released;
 for(unsigned i=0;i<1000;++i)assert(portable_automatic_close(&c)==1&&released==old);
 cleanup_pending=false;network_pending=true;assert(portable_automatic_close(&c)==1&&released==old);
 network_pending=false;assert(portable_automatic_close(&c)==0&&released==old+4);
 // No SD grant still runs the internal scanner and configured automatic API.
 media=false;portable_automatic_init(&c,&hooks);for(unsigned i=0;i<6;++i)assert(portable_automatic_step(&c,i)==1);
 auto_result=0;assert(portable_automatic_step(&c,UINT32_MAX-10u)==0&&!c.service);
 old=acquired;assert(portable_automatic_step(&c,100)==0&&acquired==old);assert(portable_automatic_step(&c,c.retry_at)==1&&acquired>old);
 assert(portable_automatic_close(&c)==0);
 allowed=false;portable_automatic_init(&c,&hooks);old=network_steps;assert(portable_automatic_step(&c,0)==1);assert(portable_automatic_step(&c,1)==1);assert(portable_automatic_step(&c,2)==INSTALLED_APPS_UNAVAILABLE&&network_steps==old&&!c.service);allowed=true;
 utc_valid=false;portable_automatic_init(&c,&hooks);old=network_steps;assert(portable_automatic_step(&c,0)==1);assert(portable_automatic_step(&c,1)==1);assert(portable_automatic_step(&c,2)==INSTALLED_APPS_UNAVAILABLE&&network_steps==old&&!c.service);
 // Additive profile handoff reads the authorized default slot only, with no
 // writes, yields on storage busy and closes every lease after queueing.
 utc_valid=true;boot_extension=true;boot_api=(installed_apps_boot_profile_v1){.base=api,.boot_profile_tag=INSTALLED_APPS_BOOT_PROFILE_TAG,.boot_profile_version=1,.supply_wifi=supply};
 boot_api.base.base.base.base.struct_size=sizeof(boot_api);
 portable_wifi_credentials saved={"saved default","saved-password"};assert(portable_wifi_profile_save(&kv,0,&saved)==PORTABLE_WIFI_CREDENTIALS_SAVED);portable_wifi_credentials_clear(&saved);
 unsigned writes=kv_writes;auto_result=INSTALLED_APPS_NEEDS_PROFILE_WIFI;
 portable_automatic_init(&c,&hooks);for(unsigned i=0;i<4;++i)assert(portable_automatic_step(&c,i)==1);
 kv_busy=true;assert(portable_automatic_step(&c,4)==1&&!supplied&&c.phase==4);kv_busy=false;
 assert(portable_automatic_step(&c,5)==INSTALLED_APPS_PROFILE_QUEUED&&supplied==1&&c.pending_boot&&!c.service&&kv_writes==writes);
 // Losing custody during any credential read fences every later callback.
 portable_automatic_init(&c,&hooks);for(unsigned i=0;i<4;++i)assert(portable_automatic_step(&c,i)==1);
 lose_read=true;unsigned reads=kv_reads;old=released;
 assert(portable_automatic_step(&c,4)==INSTALLED_APPS_RETAINED&&kv_reads==reads+1&&supplied==1&&released==old);
 assert(portable_automatic_close(&c)==INSTALLED_APPS_RETAINED&&released==old);lose_read=false;owner=true;
 // Lost owner after native queue call does not release uncertain live grants.
 portable_automatic_init(&c,&hooks);for(unsigned i=0;i<4;++i)assert(portable_automatic_step(&c,i)==1);
 lose_supply=true;old=released;assert(portable_automatic_step(&c,4)==INSTALLED_APPS_RETAINED&&supplied==2&&released==old);lose_supply=false;owner=true;
 // An old automatic API cannot request the new credential handoff.
 boot_extension=false;portable_automatic_init(&c,&hooks);for(unsigned i=0;i<4;++i)assert(portable_automatic_step(&c,i)==1);
 reads=kv_reads;assert(portable_automatic_step(&c,4)==INSTALLED_APPS_UNAVAILABLE&&kv_reads==reads&&!c.service);
 auto_result=1;
 portable_automatic_init(&c,&hooks);assert(portable_automatic_step(&c,0)==1);failed_release=true;assert(portable_automatic_close(&c)==INSTALLED_APPS_RETAINED);old=released;assert(portable_automatic_close(&c)==INSTALLED_APPS_RETAINED&&released==old);
 puts("Automatic foreground: cache/discovery before networking, no-SD internal path, 1000 pending cleanup polls, network drain before grant release, UTC fence, wrap-safe retry, sticky retention; next-boot profile credential handoff, read-only default slot, busy retry and per-read ownership fences PASS");
}
