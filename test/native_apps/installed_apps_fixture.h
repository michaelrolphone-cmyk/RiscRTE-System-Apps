#pragma once
// Actual catalog provider/coordinator/worker adapter. Media, native ELF result
// and registry are deterministic boundary doubles; no hardware claim.
#define main existing_catalog_regressions
#include "installed_apps_service_test.cpp"
#undef main
#include <map>
#include "runtime/storage/SdFirstKeyValue.h"
namespace staged_fixture {
using namespace fixture;
std::map<std::string,std::string> disk;
std::string opened;size_t position;bool writing,validImage=true;
unsigned validations;
using Kv=RiscStorage::SdFirstKeyValue;Kv::State kvState;Kv::Workspace kvWorkspace;std::vector<uint8_t> kvJournal;
bool exported,legacyNamespace;
const Kv::Journal kvBackend={nullptr,
 [](void*,uint32_t ns,void*out,uint32_t capacity,uint32_t*n)->int32_t{assert(!executing&&ns==3000);if(kvJournal.empty())return RISC_KEY_VALUE_NOT_FOUND;assert(capacity>=kvJournal.size());*n=kvJournal.size();memcpy(out,kvJournal.data(),*n);return 0;},
 [](void*,uint32_t ns,const void*data,uint32_t n)->int32_t{assert(!executing&&ns==3000);kvJournal.assign(static_cast<const uint8_t*>(data),static_cast<const uint8_t*>(data)+n);return 0;},nullptr,[](void*,uint32_t,bool*exists)->int32_t{*exists=legacyNamespace;return 0;}};


risc_storage_worker_admission_v2 nativeWorkerV2{};auto&nativeWorker=nativeWorkerV2.base;
risc_storage_volume_api_v1_download downloadVolume{};auto&packageVolume=downloadVolume.base;
const risc_storage_volume_api_v1*volume;
void settingsApiSetup(){
 installation.settings_capacity=128;
 installation.settings_status=[](void*,uint32_t slot)->uint32_t{assert(slot==1);return kvState.status;};
 installation.settings_restore=[](void*,uint32_t slot,const char*owner,const void*data,uint32_t size,bool missing)->int32_t{assert(slot==1);return Kv::restore(kvState,kvWorkspace,kvBackend,3000,owner,data,size,missing);};
 installation.settings_snapshot=[](void*,uint32_t slot,void*out,uint32_t capacity,uint32_t*size,uint64_t*generation)->int32_t{assert(slot==1);return Kv::snapshot(kvState,3000,out,capacity,size,generation);};
 installation.settings_ack=[](void*,uint32_t slot,uint64_t generation)->int32_t{assert(slot==1&&opened.empty());return Kv::acknowledge(kvState,kvWorkspace,kvBackend,3000,generation);};
 packageVolume.base.begin=[](void*,uint64_t*t){return exported?RISC_STORAGE_ASYNC_BUSY:risc_storage_async_begin(&bridge,t);};
}
std::string digest(64,'0');unsigned httpOffset,httpOpens,httpCloses;bool httpOpen;std::string httpBody="DRIVER";
const risc_http_client_v1 http={1,sizeof(http),nullptr,
 [](void*,const risc_http_request_v1*q,uint64_t*h)->int32_t{assert(!httpOpen&&q->utc_seconds==1800000000&&(strstr(q->url,"late-provider")||strstr(q->url,"app-image")));httpBody=strstr(q->url,"app-image")?"IMAGE":"DRIVER";httpOpen=true;httpOffset=0;++httpOpens;*h=1;return RISC_HTTP_OK;},
 [](void*,uint64_t,void*p,uint32_t n,uint32_t*out)->int32_t{assert(httpOpen&&n>=5);if(httpOffset){*out=0;return RISC_HTTP_EOF;}assert(n>=httpBody.size());memcpy(p,httpBody.data(),httpBody.size());*out=httpOffset=unsigned(httpBody.size());return RISC_HTTP_OK;},
 [](void*,uint64_t,risc_http_response_v1*out)->int32_t{*out={sizeof(*out),200,uint32_t(httpBody.size()),int64_t(httpBody.size())};return RISC_HTTP_OK;},
 [](void*,uint64_t)->int32_t{assert(httpOpen);httpOpen=false;++httpCloses;return RISC_HTTP_OK;}};
void graphBegin(){const auto*d=t5_driver_get(2);risc_provider_dependency_v1 deps[]={{RISC_INSTALLATION_CAPABILITY,1,&installation},{RISC_BOUND_APP_DATA_CAPABILITY,1,&data},{RISC_HTTP_CLIENT_CAPABILITY,1,&http}};assert(d->start(deps,3));}

std::string envelope(const char*version){return std::string("{\"manifest\":{\"type\":\"application\",\"id\":\"new-after-build\",\"version\":\"")+version+"\",\"file_name\":\"image-"+digest+".elf\",\"requires\":[{\"capability\":\"storage.app-data\",\"api\":1}]},\"size\":5,\"sha256\":\""+digest+"\"}";}
const char*manifestPath="/Apps/new-after-build/manifest.json";
const char*descriptorPath="/Apps/new-after-build/install.json";
void setup(){fixture::setup();
 packageVolume.base=transport;auto&fs=packageVolume.base.fs;auto&ext=fs.state.prepared.base.sleep.terminal.power.volume;auto&v=ext.base;
 v.struct_size=sizeof(downloadVolume);volume=&v;
 v.dir_open=[](void*,const char*)->uint32_t{assert(executing);++io;cursor=0;return present?1:0;};
 v.dir_next=[](void*,uint32_t,risc_storage_dirent_v1*out){assert(executing);++io;if(cursor++)return false;strcpy(out->name,"new-after-build");out->is_directory=true;return true;};
 v.file_open_read=[](void*,const char*p,uint64_t*n)->uint32_t{assert(executing&&opened.empty());++io;auto i=disk.find(p);if(!present||i==disk.end())return 0;opened=p;position=0;writing=false;*n=i->second.size();return 2;};
 v.file_read=[](void*,uint32_t,void*p,size_t n)->size_t{assert(executing);++io;auto&s=disk.at(opened);n=std::min(n,s.size()-position);memcpy(p,s.data()+position,n);position+=n;return n;};
 v.file_open_write=[](void*,const char*p)->uint32_t{assert(executing&&opened.empty());++io;if(!present||disk.count(p))return 0;opened=p;writing=true;disk[p]="";return 2;};
 v.file_write=[](void*,uint32_t,const void*p,size_t n)->size_t{assert(executing&&writing);++io;disk[opened].append(static_cast<const char*>(p),n);return n;};
 v.file_close=[](void*,uint32_t,bool commit){assert(executing&&!opened.empty());++io;if(writing&&!commit)disk.erase(opened);opened.clear();return true;};
 v.remove=[](void*,const char*p){assert(executing);++io;return disk.erase(p)!=0;};
 ext.file_sync=[](void*,uint32_t){assert(executing);++io;return true;};
 fs.metadata=[](void*,const char*p,risc_storage_metadata_v1*m)->int32_t{assert(executing);++io;m->flags=0;return !present?RISC_STORAGE_FS_IO:disk.count(p)?RISC_STORAGE_FS_OK:RISC_STORAGE_FS_NOT_FOUND;};
 fs.replace_file=[](void*,const char*from,const char*to){assert(executing);++io;if(!disk.count(from))return false;disk[to]=disk[from];disk.erase(from);return true;};
 fs.recover_replace=[](void*,const char*p){assert(executing);++io;std::string backup=std::string(p)+".risc-replace";if(!disk.count(p)&&disk.count(backup))disk[p]=disk[backup];disk.erase(backup);return true;};
 nativeWorker.base=workerApi;nativeWorker.base.api_version=1;nativeWorker.base.struct_size=sizeof(nativeWorkerV2);nativeWorker.admission_tag=RISC_STORAGE_ADMISSION_TAG;nativeWorker.admission_version=1;
 nativeWorker.application=[](void*,const risc_storage_volume_api_v1*,const char*path,uint32_t size,const uint8_t*,uint32_t*held)->int32_t{assert(executing);++io;++validations;*held=0;return validImage&&disk.count(path)&&disk[path].size()==size?RISC_STORAGE_WORK_OK:RISC_STORAGE_WORK_INVALID;};
 nativeWorkerV2.driver_tag=RISC_STORAGE_DRIVER_ADMISSION_TAG;nativeWorkerV2.driver_version=1;nativeWorkerV2.driver=nativeWorker.application;
 downloadVolume.download_tag=RISC_STORAGE_DOWNLOAD_TAG;downloadVolume.download_version=1;
 ext.mkdir=[](void*,const char*){assert(executing);++io;return true;};
 bridge.worker=&nativeWorker.base;bridge.volume=&v;bridge.ext=&ext;bridge.fs=&fs;
 packageVolume.package_tag=RISC_STORAGE_PACKAGE_TAG;packageVolume.package_version=1;
 packageVolume.submit_package=[](void*,uint64_t s,const risc_storage_package_request_v1*q,uint64_t*t){return risc_storage_package_submit(&bridge,s,q,t);};
 installation.commit=[](void*,const risc_install_candidate_v1*items,uint32_t count,risc_install_result_v1*out,bool replace)->int32_t{
  if(denyCommit){return RISC_INSTALL_BUSY;}
  ++commits;if(replace)registered.clear();
  for(unsigned i=0;i<count;++i){JsonDocument d;assert(!deserializeJson(d,items[i].manifest_json));out[i]={};out[i].status=RISC_INSTALL_OK;std::string p=items[i].manifest_path;p.resize(p.rfind('/')+1);p+=d["file_name"].as<const char*>();strcpy(out[i].launch_path,p.c_str());registered.push_back(p);}return RISC_INSTALL_OK;
 };
}
int tick(bool cache=true){unsigned before=io;int rc=api.step(nullptr,volume);assert(io==before);executing=true;worker.workerStep();if(cache)cacheWorker.workerStep();executing=false;return rc;}
int drive(){for(unsigned i=0;i<1000;++i){int rc=tick();if(rc!=INSTALLED_APPS_WORKING)return rc;}assert(false);return -99;}
void reboot(const std::string&journal){assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();saved=journal;++revision;registered.clear();begin();unsigned before=io;assert(api.refresh(nullptr)==INSTALLED_APPS_OK&&io==before);}
}
