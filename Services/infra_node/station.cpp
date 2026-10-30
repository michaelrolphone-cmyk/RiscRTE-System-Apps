#include <cstring>
#include <RiscProviderV2.h>
#include <GardenRadioAsyncV1.h>
#include <RiscHardwareConfigV1.h>
#include "Station.h"
namespace {
garden_radio_async_v1 radio{};uint64_t token=0;uint32_t operation=0;bool closing=false,retained=false;uintptr_t generation=0;infra_station_v1 api{};
int32_t poll(void* context){
 if(context!=api.context||!context)return -3;if(retained)return -2;if(!token||!operation)return -1;
 risc_radio_progress_v1 progress{};progress.struct_size=sizeof(progress);auto r=radio.poll(radio.base.context,token,operation,&progress);
 if(r==RISC_RADIO_RETAINED||progress.phase==RISC_RADIO_CLEANUP_FAILED){retained=true;return -2;}
 if(r==RISC_RADIO_QUIESCENT||progress.quiescent){if(!radio.base.release(radio.base.context,token)){retained=true;return -2;}token=0;operation=0;closing=false;return -1;}
 if(r<0||progress.failure){return -1;}return progress.phase==RISC_RADIO_CONNECTED?0:1;
}
int32_t begin(void* context,const char* ssid,const char* pass,uint32_t kind){
 if(context!=api.context||!context||!ssid||!pass||strlen(ssid)>32||strlen(pass)>63||!ssid[0])return -3;
 if(retained)return -2;if(token)return poll(context);
 if(!radio.base.claim(radio.base.context,&token))return 1;
 risc_radio_request_v1 request{};request.struct_size=sizeof(request);request.kind=kind;strcpy(request.ssid,ssid);strcpy(request.password,pass);
 const auto r=radio.begin(radio.base.context,token,&request,&operation);volatile char* p=request.password;for(unsigned i=0;i<sizeof(request.password);++i)p[i]=0;
 if(r==RISC_RADIO_RETAINED){retained=true;return -2;}if(r<0){if(!radio.base.release(radio.base.context,token)){retained=true;return -2;}token=0;operation=0;return r==RISC_RADIO_BUSY?1:-1;}return 1;
}
int32_t connect(void* c,const char* ssid,const char* pass){return begin(c,ssid,pass,RISC_RADIO_REQUEST_JOIN);}
int32_t ap(void* c,const char* ssid,const char* pass){return begin(c,ssid,pass,RISC_RADIO_REQUEST_AP);}
int32_t close(void* context){
 if(context!=api.context||!context)return -3;if(retained)return -2;if(!token)return 0;
 if(!closing){const auto r=radio.cancel(radio.base.context,token,operation);if(r==RISC_RADIO_RETAINED){retained=true;return -2;}closing=true;}
 const auto r=poll(context);return token?(r==-2?-2:1):0;
}
bool quiesce(){if(retained||token)return false;api.context=nullptr;return true;}
void stop(){if(!token&&!retained){radio={};api.context=nullptr;}}
bool start(const risc_provider_dependency_v1* deps,size_t count){
 if(token||retained||api.context)return false;bool hardware=false;
 for(size_t i=0;i<count;++i){auto& d=deps[i];if(!strcmp(d.capability_id,"platform.radio")&&d.api_version==1){auto* b=static_cast<const garden_radio_async_v1*>(d.api);if(!b||b->base.struct_size<sizeof(*b)||b->async_tag!=RISC_RADIO_ASYNC_TAG||b->async_version!=1||!b->begin||!b->poll||!b->cancel)return false;radio=*b;}
 if(!strcmp(d.capability_id,"hardware.device")&&d.api_version==1){auto* h=static_cast<const risc_hardware_device_v1*>(d.api);hardware=h&&h->struct_size>=sizeof(*h)&&h->instance_id&&h->compatible&&!strcmp(h->compatible,"espressif,esp32s3-wifi")&&h->config_type&&!strcmp(h->config_type,"radio.integrated")&&h->config_version==1&&h->config_size==sizeof(risc_hw_radio_v1)&&h->config; if(hardware){auto* c=static_cast<const risc_hw_radio_v1*>(h->config);hardware=c->struct_size==sizeof(*c)&&c->unit==0&&c->features==1;}}}
 if(!hardware||!radio.begin)return false;api={1,sizeof(api),reinterpret_cast<void*>(++generation),connect,poll,close,ap};return true;
}
const risc_driver_v2 driver{2,sizeof(driver),"infra-station","infra.station",1,&api,start,stop,quiesce};
}
extern "C" __attribute__((visibility("default"))) const risc_driver_v2* t5_driver_get(uint32_t abi){return abi==2?&driver:nullptr;}
