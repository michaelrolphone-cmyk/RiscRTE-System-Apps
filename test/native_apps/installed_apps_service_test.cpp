// Provider integration; mock transport only. This is not the asynchronous UI test.
#include <cassert>
#include <string>
#include <vector>
#include <cstring>
#include "runtime/storage/StorageWorker.h"
#include "runtime/storage/AsyncAppDataReplace.h"
#include <RiscStorageAsyncAdapterV1.h>
#include "../../Services/installed_apps/service.cpp"
namespace fixture {
std::string saved,manifest,activeManifest;unsigned entryCount=1;uint64_t revision;unsigned io,commits,renames,cursor,offset;
bool requireProvider=false,graphAvailable=false,graphBusy=false,graphDenied=false;unsigned graphCalls=0;
bool present=true,directoryError=false,denyRemoval=false,denyCommit=false,denySave=false,manifestPresent=true;
std::vector<std::string> registered;
risc_storage_volume_api_v1_async transport{};
auto&sd=*reinterpret_cast<risc_storage_volume_api_v1_ext*>(&transport);
RiscStorage::Worker worker;risc_storage_worker_v1 workerApi{};
risc_storage_async_adapter_v1 bridge{};bool executing;
risc_bound_app_data_async_v1 data={{1,sizeof(data),nullptr,
 [](void*,const char*,uint32_t*n,uint64_t*r)->int32_t{*n=saved.size();*r=revision;return revision?RISC_APP_DATA_OK:RISC_APP_DATA_NOT_FOUND;},
 [](void*,const char*,uint64_t expected,void*out,uint32_t cap,uint32_t*n,uint64_t*r)->int32_t{assert(expected==revision&&cap>=saved.size());memcpy(out,saved.data(),saved.size());*n=saved.size();*r=revision;return RISC_APP_DATA_OK;},
 [](void*,const char*,uint64_t expected,const void*bytes,uint32_t n)->int32_t{assert(executing&&expected==revision);if(denySave)return RISC_APP_DATA_NO_SPACE;saved.assign(static_cast<const char*>(bytes),n);++revision;return RISC_APP_DATA_OK;}},RISC_BOUND_APP_DATA_ASYNC_TAG,1,nullptr,nullptr};
unsigned char cacheBytes[RISC_APP_DATA_FILE_MAX];
RiscStorage::AsyncAppDataReplace cacheWorker(cacheBytes,sizeof(cacheBytes),
 [](void*,uint32_t,const char*name,uint64_t expected,const void*bytes,uint32_t size){return data.base.replace(nullptr,name,expected,bytes,size);},nullptr);
risc_installation_v1 installation={1,sizeof(installation),nullptr,nullptr,
 [](void*,const char*,bool check)->int32_t{return denyRemoval||(!check&&denyCommit)?RISC_INSTALL_DENIED:RISC_INSTALL_OK;},
 [](void*,const risc_install_candidate_v1*items,uint32_t n,risc_install_result_v1*out,bool replace)->int32_t{
  if(denyCommit)return RISC_INSTALL_BUSY;
  ++commits;if(replace)registered.clear();
  for(unsigned i=0;i<n;++i){assert(strstr(items[i].manifest_json,"application"));out[i]={};out[i].status=RISC_INSTALL_OK;
   std::string path=items[i].manifest_path;path.resize(path.rfind('/')+1);path+="new.elf";strcpy(out[i].launch_path,path.c_str());registered.push_back(path);
  }return RISC_INSTALL_OK;
 },[](void*,const risc_install_candidate_v1*items,uint32_t n,risc_install_result_v1*out,bool)->int32_t{
  for(unsigned i=0;i<n;++i){out[i]={};out[i].status=RISC_INSTALL_OK;assert(strstr(items[i].manifest_json,"application"));}
  return RISC_INSTALL_OK;
 },128,nullptr,nullptr,nullptr,0,0,nullptr,nullptr,nullptr,nullptr};
void begin(){
 const auto*d=t5_driver_get(2);risc_provider_dependency_v1 dependencies[]={{RISC_INSTALLATION_CAPABILITY,1,&installation},{RISC_BOUND_APP_DATA_CAPABILITY,1,&data}};
 assert(d->start(dependencies,2));
}
int32_t run(){int32_t rc;unsigned steps=0;do{unsigned before=io;rc=api.step(nullptr,&sd.base);assert(io==before);assert(++steps<500);if(steps%3==0){executing=true;worker.workerStep();cacheWorker.workerStep();executing=false;}}while(rc==INSTALLED_APPS_WORKING);return rc;}
void setup(){
 installation.namespaced=[](void*c,const risc_install_candidate_v1*items,const uint32_t*slots,uint32_t n,risc_install_result_v1*out,bool replace,bool check)->int32_t{
  if(requireProvider)assert(graphAvailable);
  int32_t rc=check?installation.check(c,items,n,out,replace):installation.commit(c,items,n,out,replace);
  for(unsigned i=0;i<n;++i)if(strstr(items[i].manifest_json,"storage.app-data")){
   if(!slots[i])out[i].status=RISC_INSTALL_DENIED;
   else if(!check){JsonDocument persistedDoc;assert(!deserializeJson(persistedDoc,saved));JsonDocument m;assert(!deserializeJson(m,items[i].manifest_json));assert(persistedDoc["namespaces"][slots[i]-1]==m["id"]);}
  }
  return rc;
 };

 data.begin_replace=[](void*,const char*name,uint64_t expected,const void*bytes,uint32_t size,uint64_t*token){return cacheWorker.begin(1,name,expected,bytes,size,token);};
 data.collect_replace=[](void*,uint64_t token,int32_t*out){return cacheWorker.collect(token,out);};
 auto&v=sd.base;v.api_version=1;v.struct_size=sizeof(transport);
 v.ready=[](void*){assert(executing);return present;};v.refresh=[](void*){assert(executing);return true;};
 v.dir_open=[](void*,const char*)->uint32_t{assert(executing);++io;cursor=0;return 1;};
 v.dir_next=[](void*,uint32_t,risc_storage_dirent_v1*out){assert(executing);++io;if(cursor==entryCount)return false;++cursor;snprintf(out->name,sizeof(out->name),"uncompiled-package%u",cursor);out->is_directory=true;return true;};
 v.file_open_read=[](void*,const char*,uint64_t*n)->uint32_t{assert(executing);++io;if(!manifestPresent)return 0;offset=0;activeManifest=manifest;if(entryCount>1){auto at=activeManifest.find("new-after-build");activeManifest.replace(at,15,"new-after-build"+std::to_string(cursor));}*n=activeManifest.size();return 2;};
 v.file_read=[](void*,uint32_t,void*out,size_t n)->size_t{assert(executing);++io;n=std::min(n,activeManifest.size()-offset);memcpy(out,activeManifest.data()+offset,n);offset+=n;return n;};
 v.remove=[](void*,const char*){assert(executing);++io;++renames;manifestPresent=false;return true;};
 v.file_close=[](void*,uint32_t,bool){assert(executing);++io;return true;};
 sd.dir_close_checked=[](void*,uint32_t){assert(executing);++io;return true;};
 sd.handle_error=[](void*,uint32_t,bool dir)->uint32_t{return dir&&directoryError?1:0;};
 sd.rename=[](void*,const char*,const char*to){assert(executing);++io;++renames;manifestPresent=!strstr(to,".uninstalled");return true;};
 auto&fs=transport.fs;fs.state.state_tag=RISC_STORAGE_STATE_TAG;fs.state.state_version=1;
 fs.state.observe=[](void*)->int32_t{return present?RISC_STORAGE_STATE_READY:RISC_STORAGE_STATE_UNAVAILABLE;};
 fs.fs_tag=RISC_STORAGE_FS_TAG;fs.fs_version=1;
 fs.dir_tell=[](void*,uint32_t,uint64_t*){return false;};fs.dir_seek=[](void*,uint32_t,uint64_t){return false;};
 fs.metadata=[](void*,const char*,risc_storage_metadata_v1*m)->int32_t{assert(executing);if(!present)return RISC_STORAGE_FS_IO;m->flags=0;return manifestPresent?RISC_STORAGE_FS_OK:RISC_STORAGE_FS_NOT_FOUND;};
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
 denyRemoval=false;
 assert(api.refresh(nullptr)==INSTALLED_APPS_OK);assert(run()==INSTALLED_APPS_OK);
 assert(api.remove(nullptr,"new-after-build")==INSTALLED_APPS_OK);
 assert(api.step(nullptr,&sd.base)==INSTALLED_APPS_WORKING&&cacheOperation);
 for(unsigned i=0;i<1000;++i)assert(api.step(nullptr,&sd.base)==INSTALLED_APPS_WORKING&&renames==0&&api.count(nullptr)==1);
 executing=true;assert(cacheWorker.workerStep());executing=false;
 denyCommit=true;assert(api.step(nullptr,&sd.base)==INSTALLED_APPS_WORKING&&renames==0);
 const std::string interrupted=saved;
 assert(interrupted.find("removal")!=std::string::npos);
 denyCommit=false;assert(run()==INSTALLED_APPS_OK&&renames==1&&!manifestPresent&&api.count(nullptr)==0);
 assert(saved.find("removal")==std::string::npos);
 // Restore exactly the durable bytes from a power cut before SD mutation.
 assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();
 saved=interrupted;++revision;manifestPresent=true;registered.clear();begin();beforeIo=io;
 assert(api.refresh(nullptr)==INSTALLED_APPS_OK&&api.count(nullptr)==0&&registered.empty()&&io==beforeIo);
 present=false;assert(run()==INSTALLED_APPS_UNAVAILABLE&&manifestPresent);
 assert(api.cancel(nullptr)==INSTALLED_APPS_OK); // offline intent does not trap Home
 assert(saved.find("removal")!=std::string::npos);
 present=true;assert(api.refresh(nullptr)==INSTALLED_APPS_WORKING);assert(run()==INSTALLED_APPS_OK&&!manifestPresent&&api.count(nullptr)==0);
 // Cancellation at journal-clear must collect its native cache write even
 // though the SD session has already ended and its borrowed volume is null.
 manifestPresent=true;assert(api.refresh(nullptr)==INSTALLED_APPS_OK);assert(run()==INSTALLED_APPS_OK);
 assert(api.remove(nullptr,"new-after-build")==INSTALLED_APPS_OK);
 unsigned clearSteps=0;
 while(!(removePhase==RemovePhase::Clear&&cacheOperation)){
  assert(api.step(nullptr,&sd.base)==INSTALLED_APPS_WORKING);assert(++clearSteps<100);
  executing=true;worker.workerStep();if(removePhase!=RemovePhase::Clear)cacheWorker.workerStep();executing=false;
 }
 assert(!volume);
 for(unsigned i=0;i<1000;++i)assert(api.cancel(nullptr)==INSTALLED_APPS_WORKING&&cacheOperation);
 executing=true;assert(cacheWorker.workerStep());executing=false;
 assert(api.cancel(nullptr)==INSTALLED_APPS_OK&&!cacheOperation&&!removeIntent);
 // Repeated restart after deletion but before journal-clear is idempotent.
 assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();saved=interrupted;++revision;begin();
 assert(api.refresh(nullptr)==INSTALLED_APPS_OK&&api.count(nullptr)==0);const unsigned deleted=renames;
 assert(run()==INSTALLED_APPS_OK&&renames==deleted);
 // A replacement version/card at the same path is never deleted by old intent.
 assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();saved=interrupted;++revision;manifestPresent=true;
 manifest.replace(manifest.find("1.0.1"),5,"1.0.2");begin();assert(api.refresh(nullptr)==INSTALLED_APPS_OK);
 assert(run()==INSTALLED_APPS_OK&&manifestPresent&&renames==deleted&&api.count(nullptr)==1);
 manifest.replace(manifest.find("1.0.2"),5,"1.0.1");
 manifestPresent=true;assert(api.refresh(nullptr)==INSTALLED_APPS_OK);assert(run()==INSTALLED_APPS_OK);
 present=false;assert(api.refresh(nullptr)==INSTALLED_APPS_OK);assert(run()==INSTALLED_APPS_UNAVAILABLE);assert(api.get(nullptr,0,&row)&&!row.available);
 present=true;entryCount=40;manifest.pop_back();manifest+=",\"description\":\""+std::string(3850,'x')+"\"}";
 assert(manifest.size()<=4096);assert(api.refresh(nullptr)==INSTALLED_APPS_OK);assert(run()==INSTALLED_APPS_OK&&api.count(nullptr)==40);
 assert(saved.size()<65536&&saved.find("description")==std::string::npos);
 assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();beforeIo=io;begin();
 assert(api.refresh(nullptr)==INSTALLED_APPS_OK&&api.count(nullptr)==40&&io==beforeIo);
 manifest.replace(manifest.find("1.0.1"),5,"1.0.2");assert(api.refresh(nullptr)==INSTALLED_APPS_OK);
 unsigned attempts=0;while(!cacheOperation){assert(api.step(nullptr,&sd.base)==INSTALLED_APPS_WORKING);executing=true;worker.workerStep();executing=false;assert(++attempts<500);}
 const unsigned beforePendingCommits=commits;
 for(unsigned i=0;i<1000;++i){assert(api.step(nullptr,&sd.base)==INSTALLED_APPS_WORKING);assert(api.cancel(nullptr)==INSTALLED_APPS_WORKING);}
 assert(commits==beforePendingCommits);
 executing=true;assert(cacheWorker.workerStep());executing=false;
 assert(api.cancel(nullptr)==INSTALLED_APPS_OK&&!cacheOperation);
 beforeIo=io;assert(api.refresh(nullptr)==INSTALLED_APPS_OK&&api.count(nullptr)==40&&io==beforeIo);
 assert(api.get(nullptr,0,&row)&&!strcmp(row.version,"1.0.2"));
 assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();
 // Removed private identities remain in the protected ledger. A new app
 // gets a new slot; reinstalling the former app recovers its original slot.
 begin();entryCount=1;manifest=R"({"type":"application","id":"private-a","version":"1.0.0","file_name":"new.elf","requires":[{"capability":"storage.app-data","api":1}]})";
 assert(api.refresh(nullptr)==INSTALLED_APPS_OK);assert(run()==INSTALLED_APPS_OK&&api.count(nullptr)==1);
 assert(current[0].namespaceSlot==1&&namespaceDurable[0]);
 assert(api.remove(nullptr,"private-a")==INSTALLED_APPS_OK);assert(run()==INSTALLED_APPS_OK);
 assert(saved.find("private-a")!=std::string::npos&&api.count(nullptr)==0);
 manifestPresent=true;manifest.replace(manifest.find("private-a"),9,"private-b");assert(api.refresh(nullptr)==INSTALLED_APPS_OK);assert(run()==INSTALLED_APPS_OK&&current[0].namespaceSlot==2);
 assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();begin();beforeIo=io;assert(api.refresh(nullptr)==INSTALLED_APPS_OK&&current[0].namespaceSlot==2&&io==beforeIo);
 manifest.replace(manifest.find("private-b"),9,"private-a");assert(run()==INSTALLED_APPS_OK&&current[0].namespaceSlot==1);
 JsonDocument noPrivate;assert(!deserializeJson(noPrivate,manifest));noPrivate["requires"].to<JsonArray>();manifest.clear();serializeJson(noPrivate,manifest);
 assert(api.refresh(nullptr)==INSTALLED_APPS_OK);assert(run()==INSTALLED_APPS_OK&&current[0].namespaceSlot==1);
 assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();begin();assert(api.refresh(nullptr)==INSTALLED_APPS_OK&&current[0].namespaceSlot==1);
 assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();
 // Schema 6 restores the durable graph before app publication, without SD.
 JsonDocument graphCache;assert(!deserializeJson(graphCache,saved));graphCache["schema"]=6;
 auto dependency=graphCache["providers"].to<JsonArray>().add<JsonObject>();
 dependency["manifest"]["type"]="driver";dependency["manifest"]["id"]="late-provider";
 dependency["manifest"]["version"]="1.0.0";dependency["instance"]=uint64_t(0);
 dependency["path"]="/Drivers/late-provider/image-0000000000000000000000000000000000000000000000000000000000000000.elf";
 saved.clear();serializeJson(graphCache,saved);++revision;const auto graphSaved=saved;
 installation.providers=[](void*,const risc_install_provider_v1*items,uint32_t n,bool check)->int32_t{
  ++graphCalls;assert(n==1&&!check&&items[0].instance==0&&strstr(items[0].manifest_json,"late-provider"));
  JsonDocument durable;assert(!deserializeJson(durable,saved));assert(durable["providers"].size()==1);
  if(graphBusy){return RISC_INSTALL_BUSY;}
  if(graphDenied){return RISC_INSTALL_DENIED;}
  graphAvailable=true;return RISC_INSTALL_OK;
 };
 installation.available=[](void*,const char*,uint32_t,uint64_t){return graphAvailable;};installation.provider_capacity=8;
 requireProvider=true;graphBusy=true;beforeIo=io;begin();const auto beforeGraphCommit=commits;
 assert(api.refresh(nullptr)==INSTALLED_APPS_WORKING&&commits==beforeGraphCommit&&!api.count(nullptr));
 graphBusy=false;assert(api.refresh(nullptr)==INSTALLED_APPS_OK&&graphAvailable&&api.count(nullptr)==1&&io==beforeIo);
 assert(run()==INSTALLED_APPS_OK);JsonDocument savedGraph;assert(!deserializeJson(savedGraph,saved)&&savedGraph["providers"].size()==1);
 assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();graphAvailable=false;begin();beforeIo=io;
 assert(api.refresh(nullptr)==INSTALLED_APPS_OK&&graphAvailable&&io==beforeIo);
 assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();
 graphDenied=true;graphAvailable=false;begin();const auto deniedCommits=commits;
 assert(api.refresh(nullptr)==INSTALLED_APPS_INVALID&&commits==deniedCommits&&!api.count(nullptr));
 graphDenied=false;requireProvider=false;assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();
 // An app-only older native prefix remains usable; graphs need the suffix.
 installation.struct_size=RISC_INSTALLATION_APP_PREFIX_SIZE;begin();
 assert(api.refresh(nullptr)==INSTALLED_APPS_UNAVAILABLE&&!api.count(nullptr));
 assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();
 assert(!deserializeJson(graphCache,graphSaved));graphCache.remove("providers");graphCache["schema"]=5;
 saved.clear();serializeJson(graphCache,saved);++revision;begin();assert(api.refresh(nullptr)==INSTALLED_APPS_OK);
 assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();installation.struct_size=sizeof(installation);
 puts("Provider cache: graph before apps, zero-SD restart, busy retry, denial and old-prefix compatibility PASS");
 puts("Durable namespaces: persist before admission, removed identity tombstones, restart and reinstall stable ownership PASS");
 puts("Delayed cache write: 1000 owner/cancel polls, no early registry commit, completion and reentry PASS");
 puts("40 near-4KiB manifests: compact admitted cache and zero-SD cached restart PASS");
 puts("installed provider: cache-only startup, unknown manifest, transactional failure, removal preflight, absent media PASS");
 return 0;
}
