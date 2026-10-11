// Provider integration; mock transport only. This is not the asynchronous UI test.
#include <cassert>
#include <string>
#include <vector>
#include <cstring>
#include "runtime/storage/StorageWorker.h"
#include <RiscStorageAsyncAdapterV1.h>
#include "../../Services/installed_apps/service.cpp"
namespace fixture {
std::string saved,manifest;uint64_t revision;unsigned io,commits,renames,cursor,offset;
bool present=true,directoryError=false,denyRemoval=false,denyCommit=false,denySave=false,manifestPresent=true;
std::vector<std::string> registered;
risc_storage_volume_api_v1_async transport{};
auto&sd=*reinterpret_cast<risc_storage_volume_api_v1_ext*>(&transport);
RiscStorage::Worker worker;risc_storage_worker_v1 workerApi{};
risc_storage_async_adapter_v1 bridge{};bool executing;
risc_bound_app_data_v1 data={1,sizeof(data),nullptr,
 [](void*,const char*,uint32_t*n,uint64_t*r)->int32_t{*n=saved.size();*r=revision;return revision?RISC_APP_DATA_OK:RISC_APP_DATA_NOT_FOUND;},
 [](void*,const char*,uint64_t expected,void*out,uint32_t cap,uint32_t*n,uint64_t*r)->int32_t{assert(expected==revision&&cap>=saved.size());memcpy(out,saved.data(),saved.size());*n=saved.size();*r=revision;return RISC_APP_DATA_OK;},
 [](void*,const char*,uint64_t expected,const void*bytes,uint32_t n)->int32_t{assert(expected==revision);if(denySave)return RISC_APP_DATA_NO_SPACE;saved.assign(static_cast<const char*>(bytes),n);++revision;return RISC_APP_DATA_OK;}};
risc_installation_v1 installation={1,sizeof(installation),nullptr,nullptr,
 [](void*,const char*,bool check)->int32_t{return denyRemoval||(!check&&denyCommit)?RISC_INSTALL_DENIED:RISC_INSTALL_OK;},
 [](void*,const risc_install_candidate_v1*items,uint32_t n,risc_install_result_v1*out,bool replace)->int32_t{
  ++commits;if(replace)registered.clear();
  for(unsigned i=0;i<n;++i){assert(strstr(items[i].manifest_json,"application"));out[i]={};out[i].status=RISC_INSTALL_OK;
   std::string path=items[i].manifest_path;path.resize(path.rfind('/')+1);path+="new.elf";strcpy(out[i].launch_path,path.c_str());registered.push_back(path);
  }return RISC_INSTALL_OK;
 }};
void begin(){
 const auto*d=t5_driver_get(2);risc_provider_dependency_v1 dependencies[]={{RISC_INSTALLATION_CAPABILITY,1,&installation},{RISC_BOUND_APP_DATA_CAPABILITY,1,&data}};
 assert(d->start(dependencies,2));
}
int32_t run(){int32_t rc;unsigned steps=0;do{unsigned before=io;rc=api.step(nullptr,&sd.base);assert(io==before);assert(++steps<500);if(steps%3==0){executing=true;worker.workerStep();executing=false;}}while(rc==INSTALLED_APPS_WORKING);return rc;}
void setup(){
 auto&v=sd.base;v.api_version=1;v.struct_size=sizeof(transport);
 v.ready=[](void*){assert(executing);return present;};v.refresh=[](void*){assert(executing);return true;};
 v.dir_open=[](void*,const char*)->uint32_t{assert(executing);++io;cursor=0;return 1;};
 v.dir_next=[](void*,uint32_t,risc_storage_dirent_v1*out){assert(executing);++io;if(cursor++)return false;strcpy(out->name,"uncompiled-package");out->is_directory=true;return true;};
 v.file_open_read=[](void*,const char*,uint64_t*n)->uint32_t{assert(executing);++io;if(!manifestPresent)return 0;offset=0;*n=manifest.size();return 2;};
 v.file_read=[](void*,uint32_t,void*out,size_t n)->size_t{assert(executing);++io;n=std::min(n,manifest.size()-offset);memcpy(out,manifest.data()+offset,n);offset+=n;return n;};
 v.file_close=[](void*,uint32_t,bool){assert(executing);++io;return true;};
 sd.dir_close_checked=[](void*,uint32_t){assert(executing);++io;return true;};
 sd.handle_error=[](void*,uint32_t,bool dir)->uint32_t{return dir&&directoryError?1:0;};
 sd.rename=[](void*,const char*,const char*to){assert(executing);++io;++renames;manifestPresent=!strstr(to,".uninstalled");return true;};
 auto&fs=transport.fs;fs.state.state_tag=RISC_STORAGE_STATE_TAG;fs.state.state_version=1;
 fs.state.observe=[](void*)->int32_t{return present?RISC_STORAGE_STATE_READY:RISC_STORAGE_STATE_UNAVAILABLE;};
 fs.fs_tag=RISC_STORAGE_FS_TAG;fs.fs_version=1;
 fs.dir_tell=[](void*,uint32_t,uint64_t*){return false;};fs.dir_seek=[](void*,uint32_t,uint64_t){return false;};
 fs.metadata=[](void*,const char*,risc_storage_metadata_v1*)->int32_t{return RISC_STORAGE_FS_NOT_FOUND;};
 fs.file_truncate=[](void*,uint32_t,uint64_t){return false;};fs.replace_file=[](void*,const char*,const char*){return false;};fs.recover_replace=[](void*,const char*){return false;};
 workerApi.submit=[](void*,const void*q,uint32_t n,uint64_t*t){return worker.submit(q,n,t);};
 workerApi.collect=[](void*,uint64_t t,void*q,uint32_t n,uint32_t*s,int32_t*r){return worker.collect(t,q,n,s,r);};
 workerApi.cancel=[](void*,uint64_t t){return worker.cancel(t);};
 bridge.worker=&workerApi;bridge.volume=&v;bridge.ext=&sd;bridge.fs=&fs;
 assert(worker.bind(risc_storage_async_execute,&bridge)==0);
 transport.async_tag=RISC_STORAGE_ASYNC_TAG;transport.async_version=RISC_STORAGE_ASYNC_VERSION;
 transport.begin=[](void*,uint64_t*t){return risc_storage_async_begin(&bridge,t);};
 transport.submit=[](void*,uint64_t s,const risc_storage_async_request_v1*q,uint64_t*t){return risc_storage_async_submit(&bridge,s,q,t);};
 transport.collect=[](void*,uint64_t s,uint64_t t,risc_storage_async_result_v1*r){return risc_storage_async_collect(&bridge,s,t,r);};
 transport.end=[](void*,uint64_t s){return risc_storage_async_end(&bridge,s);};
 manifest=R"({"type":"application","id":"new-after-build","version":"1.0.0","file_name":"new.elf","requires":[],"display_name":"Discovered"})";
}
}
int main(){using namespace fixture;setup();begin();assert(api.refresh(nullptr)==INSTALLED_APPS_OK);assert(!io&&!commits&&!api.count(nullptr));
 assert(run()==INSTALLED_APPS_OK);assert(commits==1&&api.count(nullptr)==1&&!saved.empty());
 installed_app_v1 row{};assert(api.get(nullptr,0,&row)&&row.available&&!strcmp(row.id,"new-after-build"));
 assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();unsigned beforeIo=io;begin();
 assert(api.refresh(nullptr)==INSTALLED_APPS_OK);assert(io==beforeIo&&api.count(nullptr)==1); // cache frame, zero SD calls
 const uint64_t beforeRevision=revision;assert(run()==INSTALLED_APPS_OK);assert(revision==beforeRevision);
 const uint32_t beforeGeneration=api.generation(nullptr);assert(api.refresh(nullptr)==INSTALLED_APPS_OK);assert(run()==INSTALLED_APPS_OK);assert(revision==beforeRevision&&api.generation(nullptr)==beforeGeneration);
 assert(api.refresh(nullptr)==INSTALLED_APPS_OK);
 const unsigned beforeCommits=commits;directoryError=true;assert(run()==INSTALLED_APPS_UNAVAILABLE);assert(commits==beforeCommits&&api.count(nullptr)==1);
 directoryError=false;manifest.replace(manifest.find("1.0.0"),5,"1.0.1");denySave=true;assert(api.refresh(nullptr)==INSTALLED_APPS_OK);assert(run()==INSTALLED_APPS_UNAVAILABLE);assert(commits==beforeCommits&&api.count(nullptr)==1);denySave=false;
 denyRemoval=true;assert(api.remove(nullptr,"new-after-build")==INSTALLED_APPS_OK);assert(run()==INSTALLED_APPS_INVALID);assert(!renames); // protected package never renamed
 denyRemoval=false;denyCommit=true;
 assert(api.remove(nullptr,"new-after-build")==INSTALLED_APPS_OK);
 assert(run()==INSTALLED_APPS_UNAVAILABLE&&renames==2&&manifestPresent); // asynchronous rollback
 denyCommit=false;
 assert(api.refresh(nullptr)==INSTALLED_APPS_OK);
 assert(api.step(nullptr,&sd.base)==INSTALLED_APPS_WORKING); // live scan, before removal
 assert(api.remove(nullptr,"new-after-build")==INSTALLED_APPS_OK);
 assert(run()==INSTALLED_APPS_OK&&renames==3&&!manifestPresent&&api.count(nullptr)==0);
 manifestPresent=true;assert(api.refresh(nullptr)==INSTALLED_APPS_OK);assert(run()==INSTALLED_APPS_OK&&api.count(nullptr)==1);
 assert(api.remove(nullptr,"new-after-build")==INSTALLED_APPS_OK);
 assert(api.step(nullptr,&sd.base)==INSTALLED_APPS_WORKING); // copied rename queued
 for(unsigned i=0;i<10;++i)assert(api.cancel(nullptr)==INSTALLED_APPS_WORKING);
 executing=true;worker.workerStep();executing=false;
 assert(api.cancel(nullptr)==INSTALLED_APPS_OK&&!manifestPresent);
 assert(api.get(nullptr,0,&row)&&!row.available); // cancelled removal cannot remain launchable
 manifestPresent=true;assert(api.refresh(nullptr)==INSTALLED_APPS_OK);assert(run()==INSTALLED_APPS_OK);
 present=false;assert(api.refresh(nullptr)==INSTALLED_APPS_OK);assert(run()==INSTALLED_APPS_UNAVAILABLE);assert(api.get(nullptr,0,&row)&&!row.available);
 assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();
 puts("installed provider: cache-only startup, unknown manifest, transactional failure, removal preflight, absent media PASS");
}
