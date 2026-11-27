#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "PortableForegroundBroadcast.h"
static bool live=true,owned,dirty_release;
static unsigned mode,edges,pauses,steps,releases;
static telemetry_broadcast_policy_v1 received;
static bool guard(void*c){assert(c==(void*)7);return live;}
static bool pause_call(void*c){assert(c==(void*)9&&!owned);edges++;pauses++;if(mode==6)live=false;return mode!=5;}
static bool step_call(void*c,bool allow,const telemetry_broadcast_policy_v1*p){
 assert(c==(void*)9&&!owned&&allow);edges++;steps++;received=*p;
 if(mode==8)live=false;return mode!=7;
}
static bool status_call(void*c,telemetry_broadcast_status_v1*s){(void)c;(void)s;return true;}
static int32_t enum_call(void*c,uint32_t i,risc_telemetry_field_v1*f){(void)c;(void)i;(void)f;return 0;}
static int32_t read_call(void*c,uint32_t i,int32_t*v){(void)c;(void)i;(void)v;return 0;}
static telemetry_broadcast_v1 api;
static bool acquire_call(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){
 assert(!owned&&!strcmp(n,TELEMETRY_BROADCAST_CAPABILITY)&&v==1&&!i);edges++;
 if(mode==1)return false;
 if(mode==2){g->slot=1;return false;}
 *g=(risc_runtime_capability_v1){.struct_size=sizeof(*g),.slot=1,.generation=1,.api=&api};
 if(mode==3)api.struct_size=0;
 if(mode==4)live=false;
 return true;
}
static bool release_call(risc_runtime_capability_v1*g){
 assert(live&&!owned&&g->api);edges++;releases++;
 if(mode==9)return false;
 if(mode==10)live=false;
 if(!dirty_release)memset(g,0,sizeof(*g));
 return true;
}
static const risc_runtime_api_v1 rt={.api_version=1,.struct_size=sizeof(rt),.acquire=acquire_call,.release=release_call};
static portable_foreground_broadcast c;
static void reset(unsigned m){
 mode=m;live=true;owned=dirty_release=false;edges=pauses=steps=releases=0;
 api=(telemetry_broadcast_v1){1,sizeof(api),(void*)9,step_call,pause_call,status_call,enum_call,read_call};
 portable_foreground_broadcast_init(&c,&rt,guard,(void*)7);
}
int main(void){
 telemetry_broadcast_policy_v1 p={.struct_size=sizeof(p),.enabled=true,.radios_allowed=true,.settings_valid=true};
 reset(0);owned=true;assert(!portable_foreground_broadcast_pause(&c,true)&&!edges);
 owned=false;assert(portable_foreground_broadcast_pause(&c,false)&&c.paused&&!c.api&&pauses==1&&releases==1);
 unsigned before=edges;owned=true;
 assert(portable_foreground_broadcast_pause(&c,true)&&portable_foreground_broadcast_step(&c,true,true,&p)&&edges==before);
 owned=false;assert(portable_foreground_broadcast_step(&c,false,true,&p)&&steps==1&&!c.paused&&!c.api);
 assert(!memcmp(&received,&p,sizeof(p)));
 assert(portable_foreground_broadcast_pause(&c,false)&&pauses==2);
 portable_foreground_broadcast_invalidate(&c);
 assert(portable_foreground_broadcast_pause(&c,false)&&pauses==3);
 reset(1);assert(portable_foreground_broadcast_pause(&c,false)&&c.unavailable&&!c.api&&edges==1);
 owned=true;assert(portable_foreground_broadcast_pause(&c,true)&&edges==1);
 for(unsigned m=2;m<=6;m++){reset(m);assert(!portable_foreground_broadcast_pause(&c,false));assert(!releases);}
 for(unsigned m=7;m<=8;m++){reset(m);assert(!portable_foreground_broadcast_step(&c,false,true,&p));assert(!releases);}
 for(unsigned m=9;m<=10;m++){reset(m);assert(!portable_foreground_broadcast_pause(&c,false)&&releases==1);}
 reset(0);dirty_release=true;assert(!portable_foreground_broadcast_pause(&c,false));
 reset(0);live=false;assert(!portable_foreground_broadcast_pause(&c,false)&&!edges);
 puts("foreground broadcast borrowed grants, pause ownership, copied policy and custody fences PASS");return 0;
}
