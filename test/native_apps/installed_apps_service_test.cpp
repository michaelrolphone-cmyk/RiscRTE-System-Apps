// Provider integration; mock transport only. This is not the asynchronous UI test.
#include <cassert>
#include <string>
#include <vector>
#include <cstring>
#include "../../Services/installed_apps/service.cpp"
namespace fixture {
std::string saved,manifest;uint64_t revision;unsigned io,commits,renames,cursor,offset;
bool present=true,directoryError=false,denyRemoval=false,denySave=false;
std::vector<std::string> registered;
risc_storage_volume_api_v1_ext sd{};
risc_bound_app_data_v1 data={1,sizeof(data),nullptr,
 [](void*,const char*,uint32_t*n,uint64_t*r)->int32_t{*n=saved.size();*r=revision;return revision?RISC_APP_DATA_OK:RISC_APP_DATA_NOT_FOUND;},
 [](void*,const char*,uint64_t expected,void*out,uint32_t cap,uint32_t*n,uint64_t*r)->int32_t{assert(expected==revision&&cap>=saved.size());memcpy(out,saved.data(),saved.size());*n=saved.size();*r=revision;return RISC_APP_DATA_OK;},
 [](void*,const char*,uint64_t expected,const void*bytes,uint32_t n)->int32_t{assert(expected==revision);if(denySave)return RISC_APP_DATA_NO_SPACE;saved.assign(static_cast<const char*>(bytes),n);++revision;return RISC_APP_DATA_OK;}};
risc_installation_v1 installation={1,sizeof(installation),nullptr,nullptr,
 [](void*,const char*,bool)->int32_t{return denyRemoval?RISC_INSTALL_DENIED:RISC_INSTALL_OK;},
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
int32_t run(){int32_t rc;unsigned steps=0;do{rc=api.step(nullptr,&sd.base);assert(++steps<100);}while(rc==INSTALLED_APPS_WORKING);return rc;}
void setup(){
 auto&v=sd.base;v.api_version=1;v.struct_size=sizeof(sd);
 v.ready=[](void*){return present;};
 v.dir_open=[](void*,const char*)->uint32_t{++io;cursor=0;return 1;};
 v.dir_next=[](void*,uint32_t,risc_storage_dirent_v1*out){++io;if(cursor++)return false;strcpy(out->name,"uncompiled-package");out->is_directory=true;return true;};
 v.file_open_read=[](void*,const char*,uint64_t*n)->uint32_t{++io;offset=0;*n=manifest.size();return 2;};
 v.file_read=[](void*,uint32_t,void*out,size_t n)->size_t{++io;n=std::min(n,manifest.size()-offset);memcpy(out,manifest.data()+offset,n);offset+=n;return n;};
 v.file_close=[](void*,uint32_t,bool){++io;return true;};
 sd.dir_close_checked=[](void*,uint32_t){++io;return true;};
 sd.handle_error=[](void*,uint32_t,bool dir)->uint32_t{return dir&&directoryError?1:0;};
 sd.rename=[](void*,const char*,const char*){++renames;return true;};
 manifest=R"({"type":"application","id":"new-after-build","version":"1.0.0","file_name":"new.elf","requires":[],"display_name":"Discovered"})";
}
}
int main(){using namespace fixture;setup();begin();assert(api.refresh(nullptr)==INSTALLED_APPS_OK);assert(!io&&!commits&&!api.count(nullptr));
 assert(run()==INSTALLED_APPS_OK);assert(commits==1&&api.count(nullptr)==1&&!saved.empty());
 installed_app_v1 row{};assert(api.get(nullptr,0,&row)&&row.available&&!strcmp(row.id,"new-after-build"));
 assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();unsigned beforeIo=io;begin();
 assert(api.refresh(nullptr)==INSTALLED_APPS_OK);assert(io==beforeIo&&api.count(nullptr)==1); // cache frame, zero SD calls
 const unsigned beforeCommits=commits;directoryError=true;assert(run()==INSTALLED_APPS_UNAVAILABLE);assert(commits==beforeCommits&&api.count(nullptr)==1);
 directoryError=false;denySave=true;assert(api.refresh(nullptr)==INSTALLED_APPS_OK);assert(run()==INSTALLED_APPS_UNAVAILABLE);assert(commits==beforeCommits&&api.count(nullptr)==1);denySave=false;
 denyRemoval=true;assert(api.remove(nullptr,"new-after-build")==INSTALLED_APPS_OK);assert(run()==INSTALLED_APPS_INVALID);assert(!renames); // protected package never renamed
 present=false;assert(api.refresh(nullptr)==INSTALLED_APPS_OK);assert(run()==INSTALLED_APPS_UNAVAILABLE);assert(api.get(nullptr,0,&row)&&!row.available);
 assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();
 puts("installed provider: cache-only startup, unknown manifest, transactional failure, removal preflight, absent media PASS");
}
