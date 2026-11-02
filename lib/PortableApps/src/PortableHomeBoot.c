#include "PortableHomeBoot.h"
#include <string.h>
typedef struct {bool active,available,consumed;int32_t result;uint32_t type,schema,size,cause;unsigned view;uint8_t bytes[512];} home_boot;
static home_boot boot;
static int halt(const risc_runtime_api_v1*rt){
 if(risc_runtime_get_api(1)==rt&&rt->struct_size>=RISC_RUNTIME_RETAIN_INVOCATION_V1_SIZE&&rt->retain_invocation)(void)rt->retain_invocation();
 return -1;
}
int portable_home_boot_begin(const risc_runtime_api_v1*rt){
 boot=(home_boot){.active=true,.result=RISC_RETAINED_WAKE_ABSENT,.cause=RISC_BOOT_RESET};
 if(!rt||rt->api_version!=1||rt->struct_size<RISC_RUNTIME_CAPABILITIES_V1_SIZE||!rt->acquire||!rt->release)return -1;
 risc_runtime_capability_v1 grant={.struct_size=sizeof(grant)};
 bool acquired=rt->acquire(RISC_RETAINED_WAKE_CAPABILITY,1,0,&grant);
 if(risc_runtime_get_api(1)!=rt)return -1;
 if(!acquired)return 0;
 const risc_retained_wake_api_v1*w=grant.api;
 if(!w||w->api_version!=1||w->struct_size<sizeof(*w)||!w->read||!w->stage||!w->clear){
  /* Successful acquisition still owns a grant even when its table is absent. */
  if(!rt->release(&grant))return halt(rt);
  return -1;
 }
 risc_retained_wake_record_v1 r={.struct_size=sizeof(r)};
 int rc=w->read(w->context,UINT32_C(0x57434c4b),1,&r,&boot.cause);
 if(risc_runtime_get_api(1)!=rt)return -1;
 if(rc==RISC_RETAINED_WAKE_OK){
  if(r.struct_size<sizeof(r)||r.type!=UINT32_C(0x57434c4b)||r.schema_version!=1||r.size>128)return halt(rt);
  boot.available=true;boot.type=r.type;boot.schema=r.schema_version;boot.size=r.size;
  memcpy(boot.bytes,r.payload,r.size);boot.view=RISC_HOME_VIEW_FACES;
 }else if(rc==RISC_RETAINED_WAKE_MISMATCH){
  const risc_retained_wake_api_v1_extended*x=risc_retained_wake_extended(w);
  if(x){
   uint32_t size=0;rc=x->read_bytes(w->context,UINT32_C(0x44434c4b),3,boot.bytes,sizeof(boot.bytes),&size,&boot.cause);
   if(risc_runtime_get_api(1)!=rt)return -1;
   if(rc==RISC_RETAINED_WAKE_OK){
    if(size>sizeof(boot.bytes))return halt(rt);
    boot.available=true;boot.type=UINT32_C(0x44434c4b);boot.schema=3;boot.size=size;boot.view=RISC_HOME_VIEW_OVERVIEW;
   }
  }
 }
 if(rc!=RISC_RETAINED_WAKE_OK&&rc!=RISC_RETAINED_WAKE_ABSENT&&rc!=RISC_RETAINED_WAKE_MISMATCH)return halt(rt);
 boot.result=rc;
 if(boot.cause>RISC_BOOT_DEEP_OTHER)return halt(rt);
 if(!rt->release(&grant))return halt(rt);
 if(risc_runtime_get_api(1)!=rt)return -1;
 return 0;
}
unsigned portable_home_boot_view(void){return boot.view;}
uint32_t portable_home_boot_cause(void){return boot.cause;}
int32_t portable_home_boot_read_bytes(const risc_retained_wake_api_v1_extended*x,uint32_t type,uint32_t schema,void*out,uint32_t capacity,uint32_t*size,uint32_t*cause){
 if(!boot.active)return x->read_bytes(x->base.context,type,schema,out,capacity,size,cause);
 if(!cause||!size||!out)return RISC_RETAINED_WAKE_INVALID;
 *cause=boot.cause;
 if(boot.consumed)return RISC_RETAINED_WAKE_ABSENT;
 if(!boot.available)return boot.result;
 if(type!=boot.type||schema!=boot.schema)return RISC_RETAINED_WAKE_MISMATCH;
 if(capacity<boot.size)return RISC_RETAINED_WAKE_INVALID;
 memcpy(out,boot.bytes,boot.size);*size=boot.size;boot.consumed=true;return RISC_RETAINED_WAKE_OK;
}
int32_t portable_home_boot_read(const risc_retained_wake_api_v1*w,uint32_t type,uint32_t schema,risc_retained_wake_record_v1*out,uint32_t*cause){
 if(!boot.active)return w->read(w->context,type,schema,out,cause);
 if(!out||out->struct_size<sizeof(*out)||!cause)return RISC_RETAINED_WAKE_INVALID;
 uint32_t size=0;uint8_t bytes[128];
 int rc=portable_home_boot_read_bytes(NULL,type,schema,bytes,sizeof(bytes),&size,cause);
 if(rc==RISC_RETAINED_WAKE_OK){*out=(risc_retained_wake_record_v1){.struct_size=sizeof(*out),.type=type,.schema_version=schema,.size=size};memcpy(out->payload,bytes,size);}
 return rc;
}
