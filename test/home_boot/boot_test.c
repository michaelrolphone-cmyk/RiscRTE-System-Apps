#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "PortableHomeBoot.h"
static unsigned reads,releases,retains,kind,cause,available=1,release_ok=1;
static int32_t read_record(void*c,uint32_t type,uint32_t schema,risc_retained_wake_record_v1*out,uint32_t*why){
 (void)c;reads++;*why=cause;if(!available)return RISC_RETAINED_WAKE_ABSENT;
 if(kind!=1||type!=0x57434c4b||schema!=1)return RISC_RETAINED_WAKE_MISMATCH;
 *out=(risc_retained_wake_record_v1){.struct_size=sizeof(*out),.type=type,.schema_version=schema,.size=1,.payload={1}};return 0;
}
static int32_t read_bytes(void*c,uint32_t type,uint32_t schema,void*out,uint32_t capacity,uint32_t*size,uint32_t*why){
 (void)c;reads++;*why=cause;if(!available)return RISC_RETAINED_WAKE_ABSENT;
 if(kind!=2||type!=0x44434c4b||schema!=3)return RISC_RETAINED_WAKE_MISMATCH;
 if(capacity<408)return RISC_RETAINED_WAKE_INVALID;
 memset(out,0x5a,408);*size=408;return 0;
}
static int32_t stage(void*c,const risc_retained_wake_record_v1*r){(void)c;(void)r;assert(0);return -1;}
static int32_t clear(void*c){(void)c;assert(0);return -1;}
static int32_t stage_bytes(void*c,uint32_t t,uint32_t s,const void*p,uint32_t n){(void)c;(void)t;(void)s;(void)p;(void)n;assert(0);return -1;}
static risc_retained_wake_api_v1_extended wake={.base={1,sizeof(wake),NULL,read_record,stage,clear},.extension_tag=RISC_RETAINED_WAKE_EXTENDED_TAG,.extension_version=1,.max_payload_bytes=512,.read_bytes=read_bytes,.stage_bytes=stage_bytes};
static bool acquire(const char*name,uint32_t api,uint64_t instance,risc_runtime_capability_v1*out){
 assert(!strcmp(name,RISC_RETAINED_WAKE_CAPABILITY)&&api==1&&!instance);out->api=&wake;return true;
}
static bool release(risc_runtime_capability_v1*g){assert(g->api==&wake);releases++;return release_ok;}
static bool retain(void){retains++;return true;}
int main(void){
 risc_runtime_api_v1 rt={.api_version=1,.struct_size=sizeof(rt),.acquire=acquire,.release=release,.retain_invocation=retain};
 for(cause=0;cause<=4;cause++)for(kind=1;kind<=2;kind++){
 reads=releases=retains=0;assert(!portable_home_boot_begin(&rt));assert(reads==kind&&releases==1&&!retains);
 assert(portable_home_boot_view()==kind&&portable_home_boot_cause()==cause);
 unsigned before=reads;uint32_t why=99,n=999;unsigned char payload[512];memset(payload,0xcc,sizeof(payload));
 assert(portable_home_boot_read_bytes(&wake,7,7,payload,sizeof(payload),&n,&why)==RISC_RETAINED_WAKE_MISMATCH&&n==999&&payload[0]==0xcc&&why==cause);
 if(kind==1){risc_retained_wake_record_v1 record={.struct_size=sizeof(record)};assert(!portable_home_boot_read(&wake.base,0x57434c4b,1,&record,&why));assert(record.size==1&&record.payload[0]==1);}
 else {assert(portable_home_boot_read_bytes(&wake,0x44434c4b,3,payload,128,&n,&why)==RISC_RETAINED_WAKE_INVALID&&n==999);assert(!portable_home_boot_read_bytes(&wake,0x44434c4b,3,payload,sizeof(payload),&n,&why));assert(n==408);for(unsigned i=0;i<n;i++)assert(payload[i]==0x5a);}
 assert(portable_home_boot_read_bytes(&wake,7,7,payload,sizeof(payload),&n,&why)==RISC_RETAINED_WAKE_ABSENT&&reads==before);
 }
 cause=RISC_BOOT_RESET;available=0;assert(!portable_home_boot_begin(&rt)&&!portable_home_boot_view());
 available=1;release_ok=0;assert(portable_home_boot_begin(&rt)<0&&retains==1);
 puts("Home bootstrap:10 native boot/schema cases, exact408/1byte records, nonconsuming mismatch/capacity refusal, exactly-once delivery and retention fence PASS");
}
