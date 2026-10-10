#include "RiscProviderV2.h"
#include "RiscEntropySourceV1.h"
#include "native/RiscEntropyV1.h"
#include <cstring>
namespace {
static_assert(RISC_ENTROPY_BYTES_MAX==RISC_ENTROPY_SOURCE_BYTES_MAX,"entropy bound");
risc_entropy_v1 backend{};
uintptr_t generation;
int32_t terminal;
bool started,busy;
unsigned char scratch[RISC_ENTROPY_SOURCE_BYTES_MAX];
extern risc_entropy_source_v1 api;
void wipe(){volatile unsigned char *p=scratch;for(unsigned i=0;i<sizeof(scratch);++i)p[i]=0;}
int32_t fill(void *context,void *out,uint32_t size){
 if(!started||!context||context!=api.context)return RISC_ENTROPY_SOURCE_CONTEXT;
 if(terminal)return terminal;
 if(busy)return RISC_ENTROPY_SOURCE_BUSY;
 if(!out||!size||size>sizeof(scratch))return RISC_ENTROPY_SOURCE_INVALID;
 busy=true;
 int32_t result=backend.fill(backend.context,scratch,size);
 if(result==RISC_ENTROPY_OK)std::memcpy(out,scratch,size);
 else if(result==RISC_ENTROPY_CONTEXT)terminal=RISC_ENTROPY_SOURCE_CONTEXT;
 else if(result==RISC_ENTROPY_RETAINED||
         (result!=RISC_ENTROPY_INVALID&&result!=RISC_ENTROPY_UNAVAILABLE&&result!=RISC_ENTROPY_BUSY))
  result=terminal=RISC_ENTROPY_SOURCE_RETAINED;
 wipe();busy=false;return result;
}
bool quiesce(){
 if(busy||terminal)return false;
 wipe();backend={};api.context=nullptr;started=false;return true;
}
bool start(const risc_provider_dependency_v1 *dependencies,size_t count){
 if(started||busy||terminal||generation==UINTPTR_MAX||!dependencies||count!=1)return false;
 const auto &dependency=dependencies[0];
 if(!dependency.capability_id||std::strcmp(dependency.capability_id,RISC_ENTROPY_CAPABILITY)||
    dependency.api_version!=RISC_ENTROPY_API_V1||!dependency.api)return false;
 const auto *source=static_cast<const risc_entropy_v1*>(dependency.api);
 if(source->api_version!=RISC_ENTROPY_API_V1||source->struct_size<sizeof(*source)||!source->context||!source->fill)return false;
 backend=*source;api.context=reinterpret_cast<void*>(++generation);started=true;return true;
}
void stop(){(void)quiesce();}
risc_entropy_source_v1 api{1,sizeof(api),nullptr,fill};
const risc_driver_v2 driver{2,sizeof(driver),"crypto-entropy",RISC_ENTROPY_SOURCE_CAPABILITY,1,&api,start,stop,quiesce};
}
extern "C" __attribute__((visibility("default"))) const risc_driver_v2 *t5_driver_get(uint32_t abi){return abi==2?&driver:nullptr;}
