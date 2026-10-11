#include "RiscProviderV2.h"
#include "UserFilesVolume.h"
#include <cstdlib>
#include <cstring>
#include <new>
namespace {
alignas(RiscUserFiles::Volume) unsigned char storage[sizeof(RiscUserFiles::Volume)];
RiscUserFiles::Volume* volume;bool started;
risc_storage_volume_api_v1_fs api{};
bool owner(void*){return started;}
bool quiesce(){if(volume&&!volume->end())return false;if(volume){volume->~Volume();volume=nullptr;}started=false;return true;}
void stop(){(void)quiesce();}
bool start(const risc_provider_dependency_v1*d,size_t n){
 if(started||volume||!d||n!=1||!d[0].capability_id||strcmp(d[0].capability_id,"storage.app-data.bound")||d[0].api_version!=1||!d[0].api)return false;
 started=true;volume=new(storage)RiscUserFiles::Volume();
 if(!volume->begin(static_cast<const risc_app_data_v1*>(d[0].api),{nullptr,owner,malloc,free}))return false;
 memcpy(&api,volume->api(),sizeof(api));return true;
}
const risc_driver_v2 driver{2,sizeof(driver),"user-files","storage.user-files",1,&api,start,stop,quiesce};
}
extern "C" __attribute__((visibility("default"))) const risc_driver_v2*t5_driver_get(uint32_t version){return version==2?&driver:nullptr;}
