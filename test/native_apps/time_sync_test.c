#include "PortableTimeSync.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t now;static unsigned calls,opens,steps,closes,releases;static bool live,deny,bad_release;
static int open_rc,step_rc,close_rc;static unsigned fault_after;
static risc_time_sample_v1 sampled;
static void call(void){assert(live);calls++;if(fault_after==calls)live=false;}
static bool guard(void *p){(void)p;return live;}
static bool clock_read(uint32_t *out){call();*out=now;return true;}
static int32_t op(void*p,const risc_time_sample_request_v1*r,uint64_t*t){(void)p;call();opens++;assert(r->timeout_ms==5000&&!strcmp(r->server,"pool.ntp.org"));if(!open_rc)*t=7;return open_rc;}
static int32_t step(void*p,uint64_t t,risc_time_sample_v1*out){(void)p;call();steps++;assert(t==7);if(!step_rc)*out=sampled;return step_rc;}
static int32_t close_sample(void*p,uint64_t t){(void)p;call();closes++;assert(t==7);return close_rc;}
static risc_time_sample_api_v1 api={1,sizeof(api),NULL,op,step,close_sample};
static bool acquire(const char*n,uint32_t v,uint64_t i,risc_runtime_capability_v1*g){call();assert(!strcmp(n,"net.time-sample")&&v==1&&!i);if(deny)return false;g->slot=1;g->generation=2;g->api=&api;return true;}
static bool release(risc_runtime_capability_v1*g){call();releases++;if(bad_release)return false;*g=(risc_runtime_capability_v1){.struct_size=sizeof(*g)};return true;}
static risc_runtime_api_v1 rt;
static portable_time_sync client;
static void reset(void){client=(portable_time_sync){0};now=100;calls=opens=steps=closes=releases=0;fault_after=0;live=true;deny=bad_release=false;open_rc=close_rc=0;step_rc=1;sampled=(risc_time_sample_v1){sizeof(sampled),1,1800000000,100,1000000000,0};rt=(risc_runtime_api_v1){.api_version=1,.struct_size=sizeof(rt),.acquire=acquire,.release=release,.monotonic_ms=clock_read};}
static int start(void){return portable_time_sync_start(&client,&rt,guard,NULL,"net.time-sample",0,"pool.ntp.org",5000);}
int main(void){unsigned cases=0;
 reset();assert(start()==1);assert(portable_time_sync_step(&client)==1);assert(!closes&&!releases);step_rc=0;assert(portable_time_sync_step(&client)==2);assert(closes==1&&releases==1);int64_t e=0;assert(portable_time_sync_epoch(&client,&e)&&e==1800000000);cases++;
 reset();assert(start()==1);assert(portable_time_sync_cancel(&client)==0);assert(closes==1&&releases==1);assert(portable_time_sync_cancel(&client)==0);assert(closes==1);cases++;
 for(int rc=-1;rc>=-9;rc--){reset();assert(start()==1);step_rc=rc;int state=portable_time_sync_step(&client);if(rc==-3||rc==-8){assert(state==4&&!closes&&!releases);}else assert(state==3&&closes==1&&releases==1);cases++;}
 for(int field=0;field<7;field++){reset();assert(start()==1);step_rc=0;switch(field){case 0:sampled.struct_size--;break;case 1:sampled.flags=0;break;case 2:sampled.reserved=1;break;case 3:sampled.resolution_ns=0;break;case 4:sampled.utc_seconds=2147483648ULL;break;case 5:sampled.utc_seconds=1;break;case 6:sampled.observed_monotonic_ms=101;break;}assert(portable_time_sync_step(&client)==3);assert(closes==1&&releases==1);cases++;}
 reset();assert(start()==1);now=5101;assert(portable_time_sync_step(&client)==3&&!steps&&closes==1);cases++;
 reset();now=UINT32_MAX-50;assert(start()==1);now=50;sampled.observed_monotonic_ms=(UINT64_C(1)<<32)+49;step_rc=0;assert(portable_time_sync_step(&client)==2);cases++;
 reset();assert(start()==1);step_rc=0;close_rc=1;assert(portable_time_sync_step(&client)==5&&!releases);assert(portable_time_sync_step(&client)==5&&!releases);close_rc=0;assert(portable_time_sync_step(&client)==2&&releases==1);cases++;
 reset();assert(start()==1);close_rc=1;assert(portable_time_sync_cancel(&client)==5&&!releases);close_rc=0;assert(portable_time_sync_cancel(&client)==0&&releases==1);cases++;
 reset();deny=true;assert(start()==3&&!opens&&!releases);cases++;
 reset();assert(start()==1);close_rc=-7;assert(portable_time_sync_cancel(&client)==4&&!releases);unsigned n=calls;assert(portable_time_sync_cancel(&client)==4&&calls==n);cases++;
 reset();assert(start()==1);bad_release=true;assert(portable_time_sync_cancel(&client)==4);cases++;
 /* Every external callback can revoke authority. No subsequent callback. */
 for(unsigned phase=1;phase<=9;phase++){reset();fault_after=phase;int state=start();if(state==1){step_rc=0;state=portable_time_sync_step(&client);}if(!live){assert(state==4);unsigned n=calls;assert(portable_time_sync_cancel(&client)==4&&calls==n);}cases++;}
 printf("PASS time sync %u lifecycle/sample/custody cases\n",cases);return 0;
}
