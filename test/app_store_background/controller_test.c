#include "app_store_background.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static risc_runtime_api_v1 rt,other;static const risc_runtime_api_v1*current;
static telemetry_broadcast_policy_api_v1 service;static risc_key_value_v1 kv;static app_store_background state;
static int retained,opens,closes,steps,pauses,reads,writes,defaults_called,session_polls;
static bool absent,dirty,default_on,record_present,record_enabled,radio_on,malformed,fail_pause,replace_step,fail_release;
static telemetry_broadcast_policy_v1 policy;static uint32_t uptime;
const risc_runtime_api_v1*risc_runtime_get_api(uint32_t version){assert(version==1);return current;}
static bool alive(void*c){(void)c;return !retained;}
static void keep(void*c){(void)c;retained++;}
static bool acquire(const char*name,uint32_t version,uint64_t id,risc_runtime_capability_v1*g){assert(!retained&&version==1);if(!strcmp(name,TELEMETRY_BROADCAST_CAPABILITY)){assert(id==0);if(absent){if(dirty)g->slot=1;return false;}opens++;session_polls=0;memset(&policy,0,sizeof(policy));g->api=&service;}else{assert(!strcmp(name,RISC_KEY_VALUE_CAPABILITY)&&id==1);g->api=&kv;}g->slot=1;g->generation=1;return true;}
static bool release(risc_runtime_capability_v1*g){assert(!retained);if(fail_release)return false;if(g->api==&service){closes++;memset(&policy,0,sizeof(policy));session_polls=0;}*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static bool health(risc_runtime_health_v1*h){assert(!retained);h->uptime_ms=uptime;return true;}
static int get(void*c,const char*key,void*out,uint32_t cap,uint32_t*n){(void)c;assert(!retained&&cap>=4);reads++;uint8_t*b=out;
 if(!strcmp(key,TELEMETRY_BROADCAST_KEY)){if(!record_present){*n=0;return RISC_KEY_VALUE_NOT_FOUND;}b[0]=0x62;b[1]=1;b[2]=record_enabled;b[3]=b[2]^0xa5;if(malformed)b[3]++;*n=4;return 0;}
 assert(!strcmp(key,"quick_radio"));uint8_t flags=radio_on?PORTABLE_RADIO_BLUETOOTH:0;b[0]=0x51;b[1]=1;b[2]=flags;b[3]=flags^0xa5;*n=4;return 0;
}
static int put(void*c,const char*k,const void*v,uint32_t n){(void)c;(void)k;(void)v;(void)n;writes++;assert(0);return -1;}
static bool step(void*c,bool allow,const telemetry_broadcast_policy_v1*p){(void)c;assert(!retained);steps++;session_polls++;policy=*p;if(!allow)policy.enabled=false;if(replace_step)current=&other;return true;}
static bool pause_service(void*c){(void)c;assert(!retained);pauses++;policy.enabled=false;return !fail_pause;}
static bool status(void*c,telemetry_broadcast_status_v1*out){(void)c;assert(!retained);*out=(telemetry_broadcast_status_v1){.struct_size=sizeof(*out),.state=policy.settings_valid?(policy.enabled&&policy.radios_allowed?TELEMETRY_BROADCAST_LIVE:TELEMETRY_BROADCAST_OFF):TELEMETRY_BROADCAST_SETTINGS_ERROR,.enabled=policy.enabled,.settings_valid=policy.settings_valid};return true;}
static int enumerate(void*c,uint32_t i,risc_telemetry_field_v1*out){(void)c;(void)i;(void)out;return -1;}
static int read_value(void*c,uint32_t i,int32_t*out){(void)c;(void)i;(void)out;return -1;}
static bool defaults(void*c,telemetry_broadcast_policy_v1*out){(void)c;defaults_called++;*out=(telemetry_broadcast_policy_v1){.struct_size=sizeof(*out),.enabled=default_on,.settings_valid=true};return true;}
static void reset(void){retained=opens=closes=steps=pauses=reads=writes=defaults_called=session_polls=0;absent=dirty=record_present=record_enabled=malformed=fail_pause=replace_step=fail_release=false;default_on=radio_on=true;uptime=0;rt=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(rt),.acquire=acquire,.release=release,.health=health};current=&rt;kv=(risc_key_value_v1){1,sizeof(kv),NULL,get,put};service=(telemetry_broadcast_policy_api_v1){.base={1,sizeof(service),NULL,step,pause_service,status,enumerate,read_value},.tag=TELEMETRY_BROADCAST_POLICY_TAG,.version=1,.defaults=defaults};}
static int init(void){return app_store_background_init(&state,&rt,alive,keep,NULL);}
int main(void){unsigned n=0;
#define TEST(code) do{reset();code;assert(!writes);n++;}while(0)
 TEST(assert(!init());assert(!app_store_background_step(&state,true));assert(policy.enabled&&policy.settings_valid&&defaults_called==1));
 TEST(default_on=false;assert(!init());assert(!app_store_background_step(&state,true));assert(!policy.enabled&&policy.settings_valid));
 TEST(record_present=true;record_enabled=false;assert(!init());assert(!app_store_background_step(&state,true));assert(!policy.enabled&&!defaults_called));
 TEST(default_on=false;record_present=record_enabled=true;assert(!init());assert(!app_store_background_step(&state,true));assert(policy.enabled&&!defaults_called));
 TEST(record_present=malformed=true;assert(!init());assert(!app_store_background_step(&state,true));assert(!policy.settings_valid&&!defaults_called));
 TEST(radio_on=false;assert(!init());assert(!app_store_background_step(&state,true));assert(!policy.radios_allowed));
 TEST(assert(!init());for(int i=0;i<8;i++){uptime+=20;assert(!app_store_background_step(&state,true));}assert(opens==1&&!closes&&session_polls==8));
 TEST(assert(!init());assert(!app_store_background_step(&state,true));telemetry_broadcast_status_v1 s;int before=steps;assert(app_store_background_status(&state,&s)&&s.enabled&&steps==before&&opens==1));
 TEST(assert(!init());assert(!app_store_background_step(&state,true));assert(!app_store_background_pause(&state)&&closes==1);assert(!app_store_background_step(&state,true)&&opens==2&&policy.enabled));
 TEST(assert(!init());assert(!app_store_background_step(&state,true));assert(!app_store_background_close(&state)&&closes==1);assert(app_store_background_step(&state,true)==BATTERY_BROADCAST_UNAVAILABLE&&opens==1));
 TEST(absent=true;assert(init()==BATTERY_BROADCAST_UNAVAILABLE);assert(!app_store_background_close(&state)&&!closes));
 TEST(absent=dirty=true;assert(init()==BATTERY_BROADCAST_RETAINED&&retained));
 TEST(assert(!init());fail_pause=true;assert(app_store_background_pause(&state)==BATTERY_BROADCAST_RETAINED&&!closes);int old=pauses;assert(app_store_background_close(&state)==BATTERY_BROADCAST_RETAINED&&pauses==old));
 TEST(assert(!init());replace_step=true;assert(app_store_background_step(&state,true)==BATTERY_BROADCAST_RETAINED&&!closes));
 TEST(assert(!init());fail_release=true;assert(app_store_background_pause(&state)==BATTERY_BROADCAST_RETAINED&&retained));
 TEST(service.base.struct_size=sizeof(telemetry_broadcast_v1);assert(!init());assert(!app_store_background_step(&state,true));assert(!policy.settings_valid&&!defaults_called));
 TEST(service.tag=0;assert(!init());assert(app_store_background_step(&state,true)==BATTERY_BROADCAST_RETAINED));
 TEST(assert(!init());assert(!app_store_background_step(&state,true));record_present=true;record_enabled=false;uptime=1001;assert(!app_store_background_step(&state,true)&&!policy.enabled&&opens==1));
 printf("%u read-only background cases PASS\n",n);return 0;
}
