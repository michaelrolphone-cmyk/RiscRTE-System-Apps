/* Ordinary, action-separated product services. Native providers own transport
 * and atomic admission. No radio, partition path, boot grants or driver writes. */
#include "RiscProviderV2.h"
#include "RiscHttpClientV1.h"
#include "RiscBankStoreV1.h"
#include "RiscPlatformClockV1.h"
#include "SoftwareUpdateV1.h"
#include "Catalog.h"

#include <cstring>
/* Existing pinned freestanding helper, no libgcc-wide import expansion. */
extern "C" {
#include "../../lib/NativeApps/src/UnsignedDivisionCompat.c"
}
#ifndef UPDATE_FIRMWARE
#error "Compile explicitly with UPDATE_FIRMWARE=0 or 1"
#endif
namespace {
const risc_http_client_v1 *http;
const risc_bank_store_v1 *bank;
const risc_platform_clock_api_v1 *clock_api;
/* Fixed .bss belongs to this provider ELF. The required native loader places
 * its complete data image in PSRAM without an internal-RAM fallback. */
WatchUpdate::Catalog catalog_storage{};
char json_storage[WatchUpdate::CatalogMax];
char installed_manifest[RISC_BANK_MANIFEST_MAX];
WatchUpdate::Catalog *catalog;
char *json;
static_assert(sizeof(catalog_storage)+sizeof(json_storage)+sizeof(installed_manifest)<704u*1024u,"bounded PSRAM workspace");
uint32_t json_size,selected;
uint64_t http_handle,bank_handle,utc,deadline;
software_update_status_v1 view;
bool started,in_call;
uint8_t chunk[RISC_HTTP_CHUNK_MAX];
int32_t last_error;
bool cleanup() {
 if(http_handle){int s=http->close(http->context,http_handle);if(s!=RISC_HTTP_OK)return false;http_handle=0;}
 if(bank_handle){if((view.state==SOFTWARE_UPDATE_ACTIVATED||view.state==SOFTWARE_UPDATE_ACTIVATION_UNKNOWN))return false;int s=bank->abort(bank->context,bank_handle);if(s!=RISC_BANK_OK)return false;bank_handle=0;}
 return true;
}
bool fail(int32_t error) {
 last_error=error;view.error=error;
 view.state=cleanup()?SOFTWARE_UPDATE_ERROR:SOFTWARE_UPDATE_RETAINED;return false;
}
bool budget(void*) {clock_api->sleep_ms(clock_api->context,1);return clock_api->monotonic_ms(clock_api->context)<deadline;}
bool open(const char *url,uint32_t bytes) {
 risc_http_request_v1 r={sizeof(r),url,bytes,300000,utc};
 int32_t e=http->open(http->context,&r,&http_handle);
 if(e!=RISC_HTTP_OK)return fail(e);return true;
}
bool classify() {
 risc_bank_status_v1 current={};current.struct_size=sizeof(current);
 if(!bank->status(bank->context,&current))return false;
 if(UPDATE_FIRMWARE){
  for(uint32_t i=0;i<catalog->count;++i){auto& r=catalog->rows[i];
   std::snprintf(r.view.installed,sizeof(r.view.installed),"%s",current.runtime_version);
   if(r.view.availability==SOFTWARE_UPDATE_USB_ONLY)continue;
   if(std::strcmp(r.layout,current.layout)||r.storeAbi!=current.store_abi||r.view.size>current.firmware_capacity){std::strcpy(r.view.reason,"Unsupported bank layout or store ABI");continue;}
   int order=t5_package_version_compare(r.view.version,current.runtime_version);
   if(order==2){std::strcpy(r.view.reason,"Installed Runtime version unavailable");continue;}
   r.view.availability=order==1?SOFTWARE_UPDATE_AVAILABLE:SOFTWARE_UPDATE_CURRENT;
   std::strcpy(r.view.reason,order==1?"New Runtime; keeps current apps and grants":"Current or older Runtime release");
  }return true;
 }
 char *installed=installed_manifest;
 bool ok=true;
 for(uint32_t i=0;i<current.app_count&&ok;++i){uint32_t n=0;int e=bank->get_app(bank->context,i,installed,RISC_BANK_MANIFEST_MAX,&n);
  if(e!=RISC_BANK_OK||!n||n>RISC_BANK_MANIFEST_MAX){ok=false;break;}
  WatchUpdate::WorkBudget b(budget,nullptr);WatchUpdate::Slice s{installed,n};char id[65]{};
  if(!WatchUpdate::str(s,"id",id,sizeof(id),b)){ok=false;break;}
  for(uint32_t j=0;j<catalog->count;++j){auto& r=catalog->rows[j];if(std::strcmp(r.native_id,id))continue;
   auto& have=catalog->scratch[0];auto& want=catalog->scratch[1];if(!WatchUpdate::manifest(s,id,have,b)||!WatchUpdate::manifest(r.manifest,id,want,b)){ok=false;break;}
   std::strcpy(r.view.installed,have.version);
   if(!WatchUpdate::authority(have,want)){std::strcpy(r.view.reason,"Changed requirements are not authorized");continue;}
   int order=t5_package_version_compare(r.view.version,have.version);
   r.view.availability=order==1?SOFTWARE_UPDATE_AVAILABLE:SOFTWARE_UPDATE_CURRENT;
   std::strcpy(r.view.reason,order==1?"Installed app update; native admission required":"Installed version is current or newer");
  }
 }
 std::memset(installed_manifest,0,sizeof(installed_manifest));return ok;
}
bool refresh(void*,uint64_t seconds) {
 if(!started||in_call||http_handle||bank_handle||seconds<1704067200ULL||seconds>4102444799ULL)return false;
 in_call=true;utc=seconds;json_size=0;catalog->count=0;view={};view.struct_size=sizeof(view);
 view.state=SOFTWARE_UPDATE_CATALOG;last_error=0;deadline=clock_api->monotonic_ms(clock_api->context)+300000;
 bool ok=open(WatchUpdate::CatalogUrl,WatchUpdate::CatalogMax);in_call=false;return ok;
}
bool status(void*,software_update_status_v1 *out) {
 if(!started||!out||out->struct_size<sizeof(*out))return false;
 view.count=catalog->count;view.resources_open=http_handle||bank_handle;*out=view;return true;
}
bool get(void*,uint32_t i,software_update_row_v1 *out) {
 if(!started||!out||out->struct_size<sizeof(*out)||i>=catalog->count)return false;*out=catalog->rows[i].view;return true;
}
bool begin(void*,uint32_t i,uint64_t seconds) {
 if(!started||in_call||http_handle||bank_handle||seconds<1704067200ULL||seconds>4102444799ULL||i>=catalog->count||catalog->rows[i].view.availability!=SOFTWARE_UPDATE_AVAILABLE)return false;
 in_call=true;utc=seconds;selected=i;auto& r=catalog->rows[i];risc_bank_status_v1 current={};current.struct_size=sizeof(current);
 if(!bank->status(bank->context,&current)){in_call=false;return fail(RISC_BANK_UNAVAILABLE);}
 risc_bank_image_v1 image={};image.struct_size=sizeof(image);image.size=r.view.size;image.store_abi=RISC_BANK_STORE_ABI;
 std::memcpy(image.sha256,r.digest,32);std::memcpy(image.active_store_sha256,current.active_store_sha256,32);
 int32_t e=UPDATE_FIRMWARE?bank->begin_firmware(bank->context,&image,&bank_handle):
  bank->begin_app(bank->context,r.native_id,r.manifest.data,(uint32_t)r.manifest.size,&image,&bank_handle);
 view.done=0;view.total=r.view.size;view.error=0;deadline=clock_api->monotonic_ms(clock_api->context)+300000;
 view.state=SOFTWARE_UPDATE_PREPARING;bool ok=e==RISC_BANK_OK;if(!ok)fail(e);in_call=false;return ok;
}
bool tick() {
 if(clock_api->monotonic_ms(clock_api->context)>=deadline)return fail(RISC_HTTP_TIMEOUT);
 if(view.state==SOFTWARE_UPDATE_CATALOG||view.state==SOFTWARE_UPDATE_DOWNLOAD){
  uint32_t n=0;int e=http->read(http->context,http_handle,chunk,sizeof(chunk),&n);
  if(n>sizeof(chunk))return fail(RISC_HTTP_SIZE);
  if(e==RISC_HTTP_AGAIN){if(n)return fail(RISC_HTTP_INVALID);return true;}
  if(e!=RISC_HTTP_OK&&e!=RISC_HTTP_EOF)return fail(e);
  if(n){if(view.state==SOFTWARE_UPDATE_CATALOG){if(n>WatchUpdate::CatalogMax-json_size)return fail(RISC_HTTP_SIZE);std::memcpy(json+json_size,chunk,n);json_size+=n;view.done=json_size;}
   else {if(n>view.total-view.done)return fail(RISC_HTTP_SIZE);int w=bank->write(bank->context,bank_handle,chunk,n);if(w!=RISC_BANK_OK)return fail(w);view.done+=n;}}
  if(e!=RISC_HTTP_EOF)return true;
  risc_http_response_v1 response={};response.struct_size=sizeof(response);
  if(http->info(http->context,http_handle,&response)!=RISC_HTTP_OK||response.status_code!=200)return fail(RISC_HTTP_STATUS);
  int closed=http->close(http->context,http_handle);if(closed!=RISC_HTTP_OK)return fail(closed);http_handle=0;
  if(view.state==SOFTWARE_UPDATE_CATALOG){
   if(!WatchUpdate::parse({json,json_size},UPDATE_FIRMWARE,*catalog,budget,nullptr)||!classify()){catalog->count=0;return fail(RISC_HTTP_INVALID);}
   view.state=SOFTWARE_UPDATE_LIST;view.done=view.total=0;return true;
  }
  if(view.done!=view.total)return fail(RISC_HTTP_SIZE);
  int finished=bank->finish(bank->context,bank_handle);if(finished!=RISC_BANK_OK)return fail(finished);
  view.state=SOFTWARE_UPDATE_VERIFYING;return true;
 }
 if(view.state==SOFTWARE_UPDATE_PREPARING||view.state==SOFTWARE_UPDATE_VERIFYING){
  risc_bank_status_v1 s={};s.struct_size=sizeof(s);int e=bank->step(bank->context,bank_handle,&s);if(e!=RISC_BANK_OK)return fail(e);
  if(s.state==RISC_BANK_FAILED)return fail(s.error);
  if(s.state==RISC_BANK_RECEIVING&&view.state==SOFTWARE_UPDATE_PREPARING){view.state=SOFTWARE_UPDATE_DOWNLOAD;return open(catalog->rows[selected].url,view.total);}
  if(s.state==RISC_BANK_READY&&view.state==SOFTWARE_UPDATE_VERIFYING){view.state=SOFTWARE_UPDATE_READY;return true;}
  return true;
 }return true;
}
bool step(void*) {
 if(!started||in_call)return false;
 if(view.state!=SOFTWARE_UPDATE_CATALOG&&view.state!=SOFTWARE_UPDATE_PREPARING&&view.state!=SOFTWARE_UPDATE_DOWNLOAD&&view.state!=SOFTWARE_UPDATE_VERIFYING)return true;
 in_call=true;bool ok=tick();in_call=false;return ok;
}
bool cancel(void*) {
 if(!started||in_call||(view.state==SOFTWARE_UPDATE_ACTIVATED||view.state==SOFTWARE_UPDATE_ACTIVATION_UNKNOWN))return false;in_call=true;
 bool ok=cleanup();view.state=ok?(catalog->count?SOFTWARE_UPDATE_LIST:SOFTWARE_UPDATE_IDLE):SOFTWARE_UPDATE_RETAINED;in_call=false;return ok;
}
bool activate(void*) {
 if(!started||in_call||view.state!=SOFTWARE_UPDATE_READY||http_handle||!bank_handle)return false;
 int e=bank->activate(bank->context,bank_handle);if(e!=RISC_BANK_OK){
  risc_bank_status_v1 s={};s.struct_size=sizeof(s);
  if(!bank->status(bank->context,&s)||s.state==RISC_BANK_ACTIVATION_UNKNOWN||s.state==RISC_BANK_ACTIVATED){view.state=SOFTWARE_UPDATE_ACTIVATION_UNKNOWN;view.error=e;return false;}
  return fail(e);
 }
 view.state=SOFTWARE_UPDATE_ACTIVATED;return true;
}
bool restart(void*) {return started&&!in_call&&(view.state==SOFTWARE_UPDATE_ACTIVATED||view.state==SOFTWARE_UPDATE_ACTIVATION_UNKNOWN)&&!http_handle&&bank->restart(bank->context,bank_handle);}
bool quiesce() {
 if(in_call||!cleanup())return false;catalog=nullptr;json=nullptr;started=false;http=nullptr;bank=nullptr;clock_api=nullptr;return true;
}
const void *dependency(const risc_provider_dependency_v1 *deps,size_t count,const char *name,size_t size) {
 const void *out=nullptr;for(size_t i=0;i<count;++i)if(deps[i].capability_id&&!std::strcmp(deps[i].capability_id,name)){
  if(out||deps[i].api_version!=1||!deps[i].api)return nullptr;const uint32_t *h=(const uint32_t*)deps[i].api;if(h[0]!=1||h[1]<size)return nullptr;out=deps[i].api;}return out;
}
bool start(const risc_provider_dependency_v1 *deps,size_t count) {
 if(started||in_call||http_handle||bank_handle||!deps||count!=3)return false;
 http=(const risc_http_client_v1*)dependency(deps,count,RISC_HTTP_CLIENT_CAPABILITY,sizeof(*http));
 bank=(const risc_bank_store_v1*)dependency(deps,count,RISC_BANK_STORE_CAPABILITY,sizeof(*bank));
 clock_api=(const risc_platform_clock_api_v1*)dependency(deps,count,"platform.clock",sizeof(*clock_api));
 if(!http||!http->open||!http->read||!http->info||!http->close||!bank||!bank->status||!bank->get_app||!bank->begin_app||!bank->begin_firmware||!bank->step||!bank->write||!bank->finish||!bank->activate||!bank->abort||!bank->restart||!clock_api||!clock_api->monotonic_ms||!clock_api->sleep_ms)return false;
 catalog=&catalog_storage;json=json_storage;catalog->count=0;json_size=0;
 view={};view.struct_size=sizeof(view);started=true;return true;
}
void stop(){(void)quiesce();}
const software_update_v1 api={1,sizeof(api),nullptr,refresh,step,status,get,begin,cancel,activate,restart};
const risc_driver_v2 driver={2,sizeof(driver),UPDATE_FIRMWARE?"software-update-firmware":"software-update-apps",UPDATE_FIRMWARE?SOFTWARE_UPDATE_FIRMWARE_CAPABILITY:SOFTWARE_UPDATE_APPS_CAPABILITY,1,&api,start,stop,quiesce};
}
extern "C" __attribute__((visibility("default"))) const risc_driver_v2 *t5_driver_get(uint32_t abi){return abi==2?&driver:nullptr;}
