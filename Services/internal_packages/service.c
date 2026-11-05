#include <RiscProviderV2.h>
#include <RiscInternalPackageVolumeV1.h>
#include <string.h>
/* Boot-selected projection. Native code owns the private mount and worker;
 * this ordinary provider supplies the existing loader storage.volume ABI. */
static risc_storage_volume_api_v1_download volume={.base.base.fs.state.prepared.base.sleep.terminal.power.volume.base={.api_version=1,.struct_size=sizeof(volume)}};
static const risc_internal_package_volume_v1*native;
static bool start(const risc_provider_dependency_v1*deps,size_t count){
 if(native||count!=1||strcmp(deps[0].capability_id,RISC_INTERNAL_PACKAGE_VOLUME_CAPABILITY)||deps[0].api_version!=1)return false;
 const risc_internal_package_volume_v1*p=deps[0].api;
 if(!p||p->api_version!=1||p->struct_size<sizeof(*p)||!p->exit_safe||!p->exit_safe(p->context)||!risc_storage_volume_download(p->volume))return false;
 memcpy(&volume,p->volume,sizeof(volume));volume.base.base.fs.state.prepared.base.sleep.terminal.power.volume.base.struct_size=sizeof(volume);native=p;return true;
}
static bool quiesce(void){return !native||native->exit_safe(native->context);}
static void stop(void){if(quiesce()){native=0;memset(&volume,0,sizeof(volume));volume.base.base.fs.state.prepared.base.sleep.terminal.power.volume.base.api_version=1;volume.base.base.fs.state.prepared.base.sleep.terminal.power.volume.base.struct_size=sizeof(volume);}}
static const risc_driver_v2 driver={2,sizeof(driver),"internal-packages","storage.volume",1,&volume,start,stop,quiesce};
__attribute__((visibility("default"))) const risc_driver_v2*t5_driver_get(uint32_t abi){return abi==2?&driver:0;}
