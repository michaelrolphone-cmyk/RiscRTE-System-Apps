#include "installed_apps_fixture.h"
#include "runtime/storage/PackageFiles.h"
#include <filesystem>
#include <stdlib.h>
#include <unistd.h>
#include <fstream>
namespace fs=std::filesystem;
namespace media_fixture {
using namespace fixture;using namespace staged_fixture;
RiscStorage::Worker internalWorker;RiscStorage::PackageFiles*files;
risc_storage_async_adapter_v1 internalBridge{};
risc_storage_worker_admission_v2 internalWork{};
risc_storage_volume_api_v1_download internalTable{};
risc_installation_media_v1 mediaInstallation{};
risc_internal_package_volume_v1 internalApi{};
uint64_t sdFree=0,internalFree=524288;uint32_t publishedMedium=99,providerMedium=99;
bool held=false;std::string directory;
void begin(){risc_provider_dependency_v1 d[]={{RISC_INSTALLATION_CAPABILITY,1,&mediaInstallation},{RISC_BOUND_APP_DATA_CAPABILITY,1,&data},{RISC_HTTP_CLIENT_CAPABILITY,1,&http},{RISC_INTERNAL_PACKAGE_VOLUME_CAPABILITY,1,&internalApi}};assert(t5_driver_get(2)->start(d,4));}
int tick(){unsigned before=io;int rc=api.step(nullptr,staged_fixture::volume);assert(io==before);executing=true;worker.workerStep();internalWorker.workerStep();cacheWorker.workerStep();executing=false;return rc;}
int finish(){for(unsigned i=0;i<1000;++i){int rc=tick();if(rc==INSTALLED_APPS_NEEDS_NETWORK){assert(saved.find("provision")!=std::string::npos);assert(api.network_ready(nullptr,1800000000)==0);continue;}if(rc!=INSTALLED_APPS_WORKING)return rc;}assert(false);return -9;}
void stop(){for(unsigned i=0;;++i){assert(i<100);int rc=api.cancel(nullptr);if(!rc)break;assert(rc==INSTALLED_APPS_WORKING);executing=true;worker.workerStep();internalWorker.workerStep();cacheWorker.workerStep();executing=false;}assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();assert(files->exitSafe()&&!held&&!httpOpen);}
std::string plan(const char*version,bool dependencies){JsonDocument d;assert(!deserializeJson(d,envelope(version)));d["url"]="https://example.invalid/app-image.elf";
 if(dependencies){auto row=d["providers"].to<JsonArray>().add<JsonObject>();row["instance"]=0;row["size"]=6;row["sha256"]=std::string(64,'a');row["url"]="https://example.invalid/late-provider.elf";
 auto m=row["manifest"].to<JsonObject>();m["type"]="driver";m["id"]="late-provider";m["version"]=version;m["driver_abi"]=2;m["architecture"]="xtensa-esp32s3";m["file_name"]="image-"+std::string(64,'a')+".elf";m["requires"].to<JsonArray>();auto offer=m["provides"].to<JsonArray>().add<JsonObject>();offer["capability"]="test.late";offer["api"]=1;}
 std::string result;serializeJson(d,result);return result;}
void setup(){
 staged_fixture::setup();downloadVolume.download_version=2;bridge.package_free_bytes=[](void*,uint64_t*out){assert(executing);*out=sdFree;return true;};
 internalWork.base.base=workerApi;auto&w=internalWork.base.base;
 w.submit=[](void*,const void*q,uint32_t n,uint64_t*t){return internalWorker.submit(q,n,t);};
 w.collect=[](void*,uint64_t t,void*out,uint32_t cap,uint32_t*n,int32_t*s){return internalWorker.collect(t,out,cap,n,s);};w.cancel=[](void*,uint64_t t){return internalWorker.cancel(t);};
 w.api_version=1;w.struct_size=sizeof(internalWork);internalWork.base.admission_tag=RISC_STORAGE_ADMISSION_TAG;internalWork.base.admission_version=1;
 internalWork.base.application=[](void*,const risc_storage_volume_api_v1*v,const char*p,uint32_t n,const uint8_t*,uint32_t*t)->int32_t{assert(executing);*t=0;uint64_t size=0;auto h=v->file_open_read(v->context,p,&size);if(!h)return RISC_STORAGE_WORK_INVALID;char b[7]{};bool ok=size==n&&n<=6&&v->file_read(v->context,h,b,n)==n&&(!strcmp(b,"IMAGE")||!strcmp(b,"DRIVER"));if(!v->file_close(v->context,h,false)){*t=h;return RISC_STORAGE_WORK_RETAINED;}return ok?RISC_STORAGE_WORK_OK:RISC_STORAGE_WORK_INVALID;};
 internalWork.driver_tag=RISC_STORAGE_DRIVER_ADMISSION_TAG;internalWork.driver_version=1;internalWork.driver=internalWork.base.application;
 assert(internalWorker.bind(risc_storage_async_execute,&internalBridge)==0);
 installation.provider_capacity=8;installation.available=[](void*,const char*,uint32_t,uint64_t){return false;};installation.providers=[](void*,const risc_install_provider_v1*,uint32_t,bool)->int32_t{return 0;};
 mediaInstallation.base=installation;mediaInstallation.base.struct_size=sizeof(mediaInstallation);mediaInstallation.media_tag=RISC_INSTALLATION_MEDIA_TAG;mediaInstallation.media_version=1;mediaInstallation.media_mask=3;
 mediaInstallation.namespaced_media=[](void*c,const risc_install_candidate_v1*rows,const uint32_t*slots,const uint32_t*media,uint32_t count,risc_install_result_v1*out,bool replace,bool check)->int32_t{
  for(unsigned i=0;i<count;++i){assert(media[i]<=1);if(!check){JsonDocument d;assert(!deserializeJson(d,saved));assert(d["apps"][i]["medium"]==media[i]);publishedMedium=media[i];}}
  return installation.namespaced(c,rows,slots,count,out,replace,check);
 };
 mediaInstallation.providers_media=[](void*,const risc_install_provider_v1*,const uint32_t*media,uint32_t count,bool check)->int32_t{for(unsigned i=0;i<count;++i){assert(media[i]<=1);if(!check){JsonDocument d;assert(!deserializeJson(d,saved));assert(d["providers"][i]["medium"]==media[i]);providerMedium=media[i];}}return 0;};
}
void bindFiles(){
 auto&a=internalTable.base.base;a.fs=*risc_storage_volume_fs(files->api());auto&v=a.fs.state.prepared.base.sleep.terminal.power.volume.base;v.struct_size=sizeof(internalTable);
 a.async_tag=RISC_STORAGE_ASYNC_TAG;a.async_version=3;a.begin=[](void*,uint64_t*t){return risc_storage_async_begin(&internalBridge,t);};a.submit=[](void*,uint64_t s,const risc_storage_async_request_v1*q,uint64_t*t){return risc_storage_async_submit(&internalBridge,s,q,t);};a.collect=[](void*,uint64_t s,uint64_t t,risc_storage_async_result_v1*out){return risc_storage_async_collect(&internalBridge,s,t,out);};a.end=[](void*,uint64_t s){return risc_storage_async_end(&internalBridge,s);};internalTable.base.package_tag=RISC_STORAGE_PACKAGE_TAG;internalTable.base.package_version=1;internalTable.base.submit_package=[](void*,uint64_t s,const risc_storage_package_request_v1*q,uint64_t*t){return risc_storage_package_submit(&internalBridge,s,q,t);};internalTable.download_tag=RISC_STORAGE_DOWNLOAD_TAG;internalTable.download_version=2;
 internalBridge.worker=&internalWork.base.base;internalBridge.volume=&v;internalBridge.ext=risc_storage_volume_extension(&v);internalBridge.fs=&a.fs;internalBridge.package_free_bytes=[](void*,uint64_t*out){return files->freeBytes(out);};internalApi={1,sizeof(internalApi),nullptr,&v,[](void*){return files->exitSafe()&&!internalBridge.session;}};
}
}
int main(){using namespace media_fixture;media_fixture::setup();
 for(unsigned cut=0;cut<180;++cut){
  directory=fs::canonical(fs::temp_directory_path()).string()+"/risc-live-"+std::to_string(getpid())+"-"+std::to_string(cut);assert(fs::create_directory(directory));
  RiscStorage::PackageFiles store({nullptr,[](void*){assert(executing);held=true;return true;},[](void*){assert(executing&&held);held=false;},[](void*){return false;},[](void*,uint64_t*t,uint64_t*u){*t=internalFree;*u=0;return true;}});files=&store;assert(store.configure(directory.c_str()));bindFiles();
  fixture::saved.clear();fixture::revision=0;staged_fixture::disk.clear();fixture::registered.clear();fixture::present=false;sdFree=0;internalFree=524288;publishedMedium=providerMedium=99;
  media_fixture::begin();assert(api.refresh(nullptr)==0);const auto descriptor=plan("1.0.0",true);assert(installed_apps_provision(&api)->install_descriptor(nullptr,descriptor.c_str())==0);
  for(unsigned i=0;i<cut;++i){int rc=media_fixture::tick();if(rc==INSTALLED_APPS_NEEDS_NETWORK){assert(api.network_ready(nullptr,1800000000)==0);continue;}assert(rc==0||rc==INSTALLED_APPS_WORKING);if(!rc)break;}
  auto cache=fixture::saved;std::map<std::string,std::string> image;for(const auto&e:fs::recursive_directory_iterator(directory))if(e.is_regular_file()){std::ifstream f(e.path(),std::ios::binary);image[e.path().lexically_relative(directory).string()]=std::string(std::istreambuf_iterator<char>(f),{});}
  media_fixture::stop();fs::remove_all(directory);fs::create_directory(directory);for(const auto&e:image){auto p=fs::path(directory)/e.first;fs::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary);f<<e.second;}
  fixture::saved=cache;fixture::revision=cache.empty()?0:fixture::revision+1;media_fixture::begin();assert(api.refresh(nullptr)==0);
  // Once journaled, a newly available SD must not redirect either image.
  fixture::present=true;sdFree=8*1024*1024;
  if(!installId[0]&&!api.count(nullptr)){fixture::present=false;assert(installed_apps_provision(&api)->install_descriptor(nullptr,descriptor.c_str())==0);}
  assert(media_fixture::finish()==0);installed_app_v1 row{};assert(api.get(nullptr,0,&row)&&row.available&&publishedMedium==1&&providerMedium==1);
  assert(fs::exists(directory+"/Apps/new-after-build/manifest.json")&&fs::exists(directory+"/Drivers/late-provider/image-"+std::string(64,'a')+".elf"));assert(staged_fixture::disk.empty());
  media_fixture::stop();media_fixture::begin();assert(api.refresh(nullptr)==0);fixture::present=false;assert(api.get(nullptr,0,&row)&&row.available);assert(media_fixture::finish()==INSTALLED_APPS_OK);assert(api.get(nullptr,0,&row)&&row.available);
  // Internal update retains private namespace and destination while SD is ready.
  assert(api.cancel(nullptr)==0);fixture::present=true;auto update=plan("1.0.1",true);assert(installed_apps_provision(&api)->install_descriptor(nullptr,update.c_str())==0);assert(media_fixture::finish()==0);assert(api.get(nullptr,0,&row)&&!strcmp(row.version,"1.0.1")&&current[0].namespaceSlot==1&&current[0].medium==1);
  // A changed profile may update a dependency without rebuilding the app.
  const std::string appBefore=current[0].json;
  JsonDocument driverUpdate;assert(!deserializeJson(driverUpdate,update));
  driverUpdate["providers"][0]["manifest"]["version"]="1.0.2";
  std::string driverPlan;serializeJson(driverUpdate,driverPlan);
  assert(installed_apps_provision(&api)->install_descriptor(nullptr,driverPlan.c_str())==0);
  for(unsigned i=0;i<cut;++i){int rc=media_fixture::tick();if(rc==INSTALLED_APPS_NEEDS_NETWORK){assert(api.network_ready(nullptr,1800000000)==0);continue;}assert(rc==0||rc==INSTALLED_APPS_WORKING);if(!rc)break;}
  auto driverCache=fixture::saved;std::map<std::string,std::string> driverFiles;
  for(const auto&e:fs::recursive_directory_iterator(directory))if(e.is_regular_file()){std::ifstream f(e.path(),std::ios::binary);driverFiles[e.path().lexically_relative(directory).string()]=std::string(std::istreambuf_iterator<char>(f),{});}
  media_fixture::stop();fs::remove_all(directory);fs::create_directory(directory);
  for(const auto&e:driverFiles){auto p=fs::path(directory)/e.first;fs::create_directories(p.parent_path());std::ofstream f(p,std::ios::binary);f<<e.second;}
  fixture::saved=driverCache;++fixture::revision;media_fixture::begin();assert(api.refresh(nullptr)==0);
  if(!installId[0]){JsonDocument graph;assert(!deserializeJson(graph,providers[0].manifest));if(graph["version"]!="1.0.2")assert(installed_apps_provision(&api)->install_descriptor(nullptr,driverPlan.c_str())==0);}
  if(installId[0])assert(media_fixture::finish()==0);
  assert(current[0].json==appBefore&&current[0].namespaceSlot==1&&current[0].medium==1);
  JsonDocument graph;assert(!deserializeJson(graph,providers[0].manifest));assert(graph["version"]=="1.0.2");
  assert(installed_apps_provision(&api)->install_descriptor(nullptr,update.c_str())==INSTALLED_APPS_INVALID);
  assert(installed_apps_provision(&api)->install_descriptor(nullptr,driverPlan.c_str())==INSTALLED_APPS_INVALID);
  if(cut==0){
   // Simulate a new package copied into internal Apps while the UI is open.
   JsonDocument added;assert(!deserializeJson(added,appBefore));added["id"]="appeared-later";
   fs::create_directories(directory+"/Apps/appeared-later");
   std::string text;serializeJson(added,text);{std::ofstream f(directory+"/Apps/appeared-later/manifest.json");f<<text;}
   fs::copy_file(directory+"/Apps/new-after-build/"+std::string(added["file_name"].as<const char*>()),directory+"/Apps/appeared-later/"+std::string(added["file_name"].as<const char*>()));
   present=false;unsigned oldIo=io;assert(api.refresh(nullptr)==0&&api.count(nullptr)==1&&io==oldIo);
   assert(media_fixture::finish()==0&&api.count(nullptr)==2);
   bool found=false;for(unsigned j=0;j<currentCount;++j)if(!strcmp(current[j].view.id,"appeared-later")){found=true;assert(current[j].medium==1&&current[j].namespaceSlot==2);}
   assert(found);assert(api.remove(nullptr,"appeared-later")==0);assert(media_fixture::finish()==0&&api.count(nullptr)==1);
  }
  assert(api.remove(nullptr,row.id)==0);assert(media_fixture::finish()==0);assert(!api.count(nullptr)&&!fs::exists(directory+"/Apps/new-after-build/manifest.json"));media_fixture::stop();fs::remove_all(directory);
 }

 // Capacity policy before the first durable journal or image mutation.
 for(unsigned scenario=0;scenario<4;++scenario){
  directory=fs::canonical(fs::temp_directory_path()).string()+"/risc-capacity-"+std::to_string(getpid())+"-"+std::to_string(scenario);assert(fs::create_directory(directory));
  RiscStorage::PackageFiles store({nullptr,[](void*){assert(executing);held=true;return true;},[](void*){assert(executing&&held);held=false;},[](void*){return false;},[](void*,uint64_t*t,uint64_t*u){*t=internalFree;*u=0;return true;}});files=&store;assert(store.configure(directory.c_str()));bindFiles();
  fixture::saved.clear();fixture::revision=0;staged_fixture::disk.clear();fixture::registered.clear();fixture::present=true;
  sdFree=scenario==0?8*1024*1024:0;internalFree=scenario==2?139264:524288;
  downloadVolume.download_version=scenario==3?1:2;
  media_fixture::begin();assert(api.refresh(nullptr)==0);auto descriptor=plan("1.0.0",true);assert(installed_apps_provision(&api)->install_descriptor(nullptr,descriptor.c_str())==0);
  int rc=INSTALLED_APPS_WORKING;
  for(unsigned n=0;n<1000&&!installMediumSelected&&rc==INSTALLED_APPS_WORKING;++n)rc=media_fixture::tick();
  if(scenario==2){assert(!installMediumSelected&&rc==INSTALLED_APPS_UNAVAILABLE&&fixture::saved.empty());}
  else assert(installMediumSelected&&installMedium==(scenario==0?0u:1u));
  assert(staged_fixture::disk.empty());media_fixture::stop();fs::remove_all(directory);
 }
 downloadVolume.download_version=2;
 puts("Dependency-only updates: 180 restart cuts, unchanged app bytes and namespace, no provider downgrade or same-version replacement PASS");
 puts("SD-first capacity: sufficient SD, full SD fallback, both full fail closed, legacy SD fallback PASS");
 puts("Live internal package storage: 180 app+driver restart cuts, absent SD, durable media despite inserted SD, cache replay, real immutable files/activation, internal update/private namespace and removal PASS");
}
