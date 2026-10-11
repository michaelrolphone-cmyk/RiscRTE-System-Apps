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
std::string digest(64,'0');unsigned httpOffset,httpOpens,httpCloses;bool httpOpen;
const risc_http_client_v1 http={1,sizeof(http),nullptr,
 [](void*,const risc_http_request_v1*q,uint64_t*h)->int32_t{assert(!httpOpen&&q->utc_seconds==1800000000&&strstr(q->url,"late-provider"));httpOpen=true;httpOffset=0;++httpOpens;*h=1;return RISC_HTTP_OK;},
 [](void*,uint64_t,void*p,uint32_t n,uint32_t*out)->int32_t{assert(httpOpen&&n>=5);if(httpOffset){*out=0;return RISC_HTTP_EOF;}memcpy(p,"DRIVER",6);*out=httpOffset=6;return RISC_HTTP_OK;},
 [](void*,uint64_t,risc_http_response_v1*out)->int32_t{*out={sizeof(*out),200,6,6};return RISC_HTTP_OK;},
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
int main(){using namespace fixture;using namespace staged_fixture;staged_fixture::setup();begin();
 disk[descriptorPath]=envelope("1.0.0");disk["/Apps/new-after-build/image-"+digest+".elf"]="IMAGE";
 assert(api.refresh(nullptr)==INSTALLED_APPS_OK&&api.count(nullptr)==0&&io==0);assert(drive()==0&&api.staged_count(nullptr)==1&&!api.count(nullptr));
 installed_app_v1 row{};assert(api.staged_get(nullptr,0,&row)&&!strcmp(row.id,"new-after-build"));
 assert(api.install(nullptr,"new-after-build","1.0.0")==0);
 unsigned n=0;while(!cacheOperation){assert(tick(false)==INSTALLED_APPS_WORKING);assert(++n<100);}
 assert(installPhase==InstallPhase::SaveJournal&&validations==1&&!disk.count(manifestPath)&&registered.empty());
 for(unsigned i=0;i<1000;++i)assert(api.step(nullptr,staged_fixture::volume)==INSTALLED_APPS_WORKING&&!disk.count(manifestPath)&&registered.empty());
 executing=true;cacheWorker.workerStep();executing=false;const std::string journal=saved;const auto beforePublication=disk;
 assert(saved.find("activation")!=std::string::npos);assert(drive()==0&&api.count(nullptr)==1&&!api.staged_count(nullptr));
 assert(registered.size()==1&&registered[0].find("image-")!=std::string::npos&&current[0].namespaceSlot==1);
 assert(saved.find("activation")==std::string::npos&&disk.count(manifestPath));const std::string installed=saved;
 // Reset at the durable pre-publication boundary: immediate cache has no new
 // entry; recovery revalidates once before completing native publication.
 reboot(journal);disk=beforePublication;assert(!api.count(nullptr)&&registered.empty());unsigned before=validations;
 assert(drive()==0&&validations==before+1&&api.count(nullptr)==1&&current[0].namespaceSlot==1);
 // Reset after the manifest switch but before final cache commit.
 const auto published=disk;reboot(journal);disk=published;before=validations;assert(drive()==0&&validations==before+1&&api.count(nullptr)==1);
 // Offline recovery preserves the activation journal and retries on re-entry.
 reboot(journal);disk=beforePublication;present=false;
 assert(drive()==INSTALLED_APPS_UNAVAILABLE&&saved.find("activation")!=std::string::npos&&!disk.count(manifestPath));
 assert(api.cancel(nullptr)==0);present=true;assert(api.refresh(nullptr)==0&&drive()==0&&api.count(nullptr)==1);
 // An unrelated manifest never gets overwritten by journal recovery.
 reboot(journal);disk=beforePublication;disk[manifestPath]="{\"id\":\"other\",\"version\":\"1.0.0\"}";
 const auto conflict=disk[manifestPath];assert(drive()==INSTALLED_APPS_UNAVAILABLE&&disk[manifestPath]==conflict);
 assert(api.cancel(nullptr)==0);disk=beforePublication;assert(api.refresh(nullptr)==0);
 // Cancellation while the final cache write is queued must drain it before
 // dropping the installer. Re-entry publishes from that exact durable cache.
 n=0;while(!(installPhase==InstallPhase::SaveFinal&&cacheOperation)){
  assert(tick(false)==INSTALLED_APPS_WORKING);assert(++n<200);
 }
 assert(disk.count(manifestPath)&&saved.find("activation")!=std::string::npos);
 for(unsigned i=0;i<1000;++i)assert(api.cancel(nullptr)==INSTALLED_APPS_WORKING&&cacheOperation);
 executing=true;assert(cacheWorker.workerStep());executing=false;
 assert(api.cancel(nullptr)==0&&!cacheOperation&&saved.find("activation")==std::string::npos);
 assert(api.refresh(nullptr)==0&&api.count(nullptr)==1&&drive()==0);
 // Updating uses another immutable image and retains the old image/settings.
 const std::string oldImage=registered[0];digest.assign(64,'1');disk[descriptorPath]=envelope("1.1.0");disk["/Apps/new-after-build/image-"+digest+".elf"]="IMAGE";
 assert(api.refresh(nullptr)==0&&drive()==0&&api.staged_count(nullptr)==1);
 assert(api.install(nullptr,"new-after-build","1.1.0")==0&&drive()==0&&current[0].namespaceSlot==1&&disk.count("/"+oldImage));
 assert(api.get(nullptr,0,&row)&&!strcmp(row.version,"1.1.0"));
 // A descriptor replaced after display is rejected even with equal ID/version.
 digest.assign(64,'2');disk[descriptorPath]=envelope("1.2.0");disk["/Apps/new-after-build/image-"+digest+".elf"]="IMAGE";
 assert(api.refresh(nullptr)==0&&drive()==0&&api.staged_count(nullptr)==1);digest.assign(64,'3');disk[descriptorPath]=envelope("1.2.0");
 before=validations;assert(api.install(nullptr,"new-after-build","1.2.0")==0&&drive()==INSTALLED_APPS_INVALID&&validations==before);assert(api.cancel(nullptr)==0);
 assert(api.refresh(nullptr)==0&&drive()==0);disk["/Apps/new-after-build/image-"+digest+".elf"]="IMAGE";validImage=false;
 assert(api.install(nullptr,"new-after-build","1.2.0")==0&&drive()==INSTALLED_APPS_UNAVAILABLE);assert(api.cancel(nullptr)==0);validImage=true;
 assert(saved.find("activation")==std::string::npos);assert(api.refresh(nullptr)==0&&api.get(nullptr,0,&row)&&!strcmp(row.version,"1.1.0"));
 assert(api.cancel(nullptr)==0);(void)installed;
 // Install a missing dependency from a selected native descriptor. Neither
 // worker image I/O nor provider graph publication may precede durable intent.
 assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();
 installation.provider_capacity=8;installation.available=[](void*,const char*cap,uint32_t api,uint64_t){return graphAvailable&&!strcmp(cap,"test.late")&&api==1;};
 installation.providers=[](void*,const risc_install_provider_v1*rows,uint32_t count,bool check)->int32_t{
  assert(count==1&&strstr(rows[0].manifest_json,"late-provider"));
  if(!check){JsonDocument durable;assert(!deserializeJson(durable,saved));assert(durable["providers"].size()==1);graphAvailable=true;++graphCalls;}
  return RISC_INSTALL_OK;
 };
 graphBegin();assert(api.refresh(nullptr)==0);
 digest.assign(64,'4');JsonDocument selection;assert(!deserializeJson(selection,envelope("1.3.0")));
 auto dependency=selection["provider_catalog"].to<JsonArray>().add<JsonObject>();
 auto missingNeed=selection["manifest"]["requires"].add<JsonObject>();missingNeed["capability"]="test.late";missingNeed["api"]=1;
 dependency["instance"]=uint64_t(0);dependency["size"]=6;dependency["sha256"]=std::string(64,'a');dependency["url"]="https://example.invalid/late-provider.elf";
 auto provider=dependency["manifest"].to<JsonObject>();provider["type"]="driver";provider["id"]="late-provider";provider["version"]="1.0.0";provider["driver_abi"]=2;
 provider["architecture"]="xtensa-esp32s3";provider["file_name"]="image-"+std::string(64,'a')+".elf";
 provider["requires"].to<JsonArray>();auto offered=provider["provides"].to<JsonArray>().add<JsonObject>();offered["capability"]="test.late";offered["api"]=1;
 // The catalog selects only missing providers and rejects ambiguous offers.
 JsonDocument resolved;resolved.set(selection);assert(selectCatalogProviders(resolved));
 assert(resolved["providers"].size()==1&&resolved["provider_catalog"].isNull());
 graphAvailable=true;resolved.set(selection);assert(selectCatalogProviders(resolved)&&resolved["providers"].size()==0);graphAvailable=false;
 resolved.set(selection);resolved["provider_catalog"].as<JsonArray>().add(dependency);
 assert(!selectCatalogProviders(resolved));
 resolved.set(selection);resolved["provider_catalog"].to<JsonArray>();assert(!selectCatalogProviders(resolved));
 resolved.set(selection);auto unused=resolved["provider_catalog"].as<JsonArray>().add<JsonObject>();
 unused["manifest"]["provides"][0]["capability"]="test.unneeded";unused["manifest"]["provides"][0]["api"]=1;
 assert(selectCatalogProviders(resolved)&&resolved["providers"].size()==1);
 disk[descriptorPath].clear();serializeJson(selection,disk[descriptorPath]);disk["/Apps/new-after-build/image-"+digest+".elf"]="IMAGE";
 assert(drive()==0&&api.staged_count(nullptr)==1);assert(api.install(nullptr,"new-after-build","1.3.0")==0);
 n=0;while(!cacheOperation){assert(tick(false)==INSTALLED_APPS_WORKING);assert(++n<100);}
 assert(provisionPhase==ProvisionPhase::Journal&&!downloader&&!graphCalls&&!httpOpens);
 for(unsigned i=0;i<1000;++i)assert(api.step(nullptr,staged_fixture::volume)==INSTALLED_APPS_WORKING&&!downloader&&!graphCalls&&!httpOpens);
 executing=true;assert(cacheWorker.workerStep());executing=false;const auto dependencyJournal=saved;
 assert(dependencyJournal.find("provision")!=std::string::npos);
 assert(drive()==INSTALLED_APPS_NEEDS_NETWORK&&!httpOpen&&!graphCalls);
 assert(api.network_ready(nullptr,1800000000)==0);assert(drive()==0&&httpOpens==1&&httpCloses==1&&graphCalls==1&&graphAvailable);
 assert(api.get(nullptr,0,&row)&&!strcmp(row.version,"1.3.0"));assert(saved.find("provision")==std::string::npos);

 assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();graphAvailable=false;graphBegin();before=io;
 assert(api.refresh(nullptr)==0&&graphAvailable&&io==before&&api.count(nullptr)==1);
 assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();
 // A restart at pre-download intent resumes from the cache, even if the
 // public descriptor disappears. Existing admitted bytes need no connection.
 saved=dependencyJournal;++revision;disk.erase(descriptorPath);
 // Restore the prior app manifest so the fresh install can snapshot it.
 JsonDocument old;assert(!deserializeJson(old,envelope("1.1.0")));disk[manifestPath].clear();serializeJson(old["manifest"],disk[manifestPath]);
 graphAvailable=false;graphBegin();before=io;assert(api.refresh(nullptr)==0&&io==before);
 const auto beforeHttp=httpOpens;assert(drive()==0&&graphAvailable&&httpOpens==beforeHttp);
 assert(saved.find("provision")==std::string::npos);assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();
 // Every owner-tick interruption is followed by checked drain, provider
 // restart, cache replay and completion. The worker still owns all media I/O.
 const auto stableDisk=disk;const auto stableCache=saved;
 unsigned recoveryCuts=0;
 for(unsigned cut=0;cut<110;++cut){
  saved=stableCache;++revision;disk=stableDisk;graphBegin();assert(api.refresh(nullptr)==0);
  digest.assign(64,'5');assert(!deserializeJson(selection,envelope("1.4.0")));
  assert(!deserializeJson(old,disk[manifestPath]));
  JsonDocument selectedDependency;assert(!deserializeJson(selectedDependency,dependencyJournal));
  selection["providers"]=selectedDependency["provision"]["providers"];
  selection["providers"][0]["sha256"]=std::string(64,'b');selection["providers"][0]["manifest"]["file_name"]="image-"+std::string(64,'b')+".elf";selection["providers"][0]["manifest"]["version"]="1.0.1";
  disk[descriptorPath].clear();serializeJson(selection,disk[descriptorPath]);disk["/Apps/new-after-build/image-"+digest+".elf"]="IMAGE";
  assert(drive()==0&&api.install(nullptr,"new-after-build","1.4.0")==0);
  for(unsigned at=0;at<cut;++at){int rc=tick();if(rc==INSTALLED_APPS_NEEDS_NETWORK){assert(api.network_ready(nullptr,1800000000)==0);continue;}assert(rc==INSTALLED_APPS_WORKING||rc==INSTALLED_APPS_OK);if(rc==INSTALLED_APPS_OK)break;}
  const auto interruptedDisk=disk;const auto interruptedCache=saved;const auto interruptedKv=kvJournal;(void)interruptedKv;
  for(unsigned drain=0;;++drain){int rc=api.cancel(nullptr);assert(drain<100);if(rc==0)break;assert(rc==INSTALLED_APPS_WORKING);executing=true;worker.workerStep();cacheWorker.workerStep();executing=false;}
  assert(!httpOpen&&opened.empty());assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();disk=interruptedDisk;saved=interruptedCache;++revision;registered.clear();graphBegin();assert(api.refresh(nullptr)==0);
  if(!installId[0]&&(!api.get(nullptr,0,&row)||strcmp(row.version,"1.4.0"))){assert(drive()==0&&api.install(nullptr,"new-after-build","1.4.0")==0);}
  for(unsigned at=0;;++at){assert(at<500);int rc=tick();if(rc==INSTALLED_APPS_NEEDS_NETWORK){assert(api.network_ready(nullptr,1800000000)==0);continue;}if(rc==0)break;assert(rc==INSTALLED_APPS_WORKING);}
  assert(api.get(nullptr,0,&row)&&!strcmp(row.version,"1.4.0")&&!httpOpen&&opened.empty());
  assert(saved.find("provision")==std::string::npos&&saved.find("activation")==std::string::npos);
  assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();++recoveryCuts;
 }
 assert(recoveryCuts==110);
 // Missing/exported media keeps the desired dependency transaction durable.
 saved=dependencyJournal;++revision;disk=stableDisk;disk.erase(descriptorPath);graphBegin();assert(api.refresh(nullptr)==0);present=false;
 assert(drive()==INSTALLED_APPS_UNAVAILABLE&&saved.find("provision")!=std::string::npos);
 assert(api.cancel(nullptr)==0);present=true;assert(api.refresh(nullptr)==0);
 // The stable card already has the desired image and app; no network is needed.
 // Recovery accepts only the selected app's predecessor, so preserve that here.
 assert(!deserializeJson(old,envelope("1.1.0")));disk[manifestPath].clear();serializeJson(old["manifest"],disk[manifestPath]);
 assert(drive()==0&&!httpOpen);assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();
 // The catalog warms private settings via the same copied worker, before
 // launch availability. Native journal values disappear only after readback.
 saved.clear();revision=0;disk.clear();registered.clear();graphAvailable=false;kvState={};kvJournal.clear();settingsApiSetup();
 digest.assign(64,'6');JsonDocument settingsApp;assert(!deserializeJson(settingsApp,envelope("2.0.0")));
 auto need=settingsApp["manifest"]["requires"].add<JsonObject>();need["capability"]="storage.key-value";need["api"]=1;
 serializeJson(settingsApp["manifest"],disk[manifestPath]);disk["/Apps/new-after-build/image-"+digest+".elf"]="IMAGE";
 graphBegin();assert(api.refresh(nullptr)==0);assert(drive()==0&&api.get(nullptr,0,&row)&&row.available&&kvState.status==Kv::Clean);
 const char*settingsPath="/AppData/new-after-build/settings.kv";assert(disk.count(settingsPath)&&kvJournal.size()==Kv::Header);
 assert(Kv::put(kvState,kvWorkspace,kvBackend,3000,"mode","saved",5)==0);
 assert(api.refresh(nullptr)==0&&drive()==0&&kvState.status==Kv::Clean&&kvJournal.size()==Kv::Header);
 const auto settingsCache=saved;const auto settingsDisk=disk;
 assert(Kv::put(kvState,kvWorkspace,kvBackend,3000,"mode","next",4)==0);const auto pendingSettings=kvJournal;
 assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();
 for(unsigned cut=0;cut<65;++cut){
  saved=settingsCache;++revision;disk=settingsDisk;kvJournal=pendingSettings;kvState={};graphBegin();assert(api.refresh(nullptr)==0);
  assert(api.get(nullptr,0,&row)&&!row.available);
  for(unsigned at=0;at<cut;++at){int rc=tick();assert(rc==INSTALLED_APPS_WORKING||rc==INSTALLED_APPS_OK);if(rc==0)break;}
  const auto interruptedDisk=disk;const auto interruptedCache=saved;const auto interruptedKv=kvJournal;(void)interruptedKv;
  for(unsigned drain=0;;++drain){int rc=api.cancel(nullptr);assert(drain<100);if(rc==0)break;assert(rc==INSTALLED_APPS_WORKING);executing=true;worker.workerStep();cacheWorker.workerStep();executing=false;}
  assert(opened.empty());assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();disk=interruptedDisk;saved=interruptedCache;++revision;kvJournal=interruptedKv;registered.clear();kvState={};graphBegin();assert(api.refresh(nullptr)==0&&drive()==0);
  char out[64]{};uint32_t bytes=0;assert(Kv::get(kvState,kvWorkspace,kvBackend,3000,"mode",out,sizeof(out),&bytes)==0&&bytes==4&&!memcmp(out,"next",4));
  assert(api.get(nullptr,0,&row)&&row.available&&kvJournal.size()==Kv::Header);
  assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();
 }
 // Export owns the volume: no copied SD session starts and pending settings
 // remain durable. Retrying after export closes completes the checkpoint.
 saved=settingsCache;++revision;disk=settingsDisk;kvJournal=pendingSettings;kvState={};graphBegin();assert(api.refresh(nullptr)==0);exported=true;before=io;
 assert(drive()==INSTALLED_APPS_UNAVAILABLE&&io==before&&kvJournal==pendingSettings);assert(api.cancel(nullptr)==0);
 exported=false;assert(api.refresh(nullptr)==0&&drive()==0&&kvState.status==Kv::Clean);
 assert(Kv::put(kvState,kvWorkspace,kvBackend,3000,"mode","last",4)==0);const auto removedPending=kvJournal;
 present=false;assert(api.refresh(nullptr)==0&&drive()==INSTALLED_APPS_UNAVAILABLE&&kvJournal==removedPending);
 assert(api.cancel(nullptr)==0);present=true;assert(api.refresh(nullptr)==0&&drive()==0&&kvState.status==Kv::Clean);
 assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();
 // Populated delivered native namespaces remain native and cause no SD
 // checkpoint, journal writes or repeated background work after restoration.
 assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();
 saved=settingsCache;++revision;disk=settingsDisk;disk.erase(settingsPath);kvJournal.clear();kvState={};legacyNamespace=true;
 graphBegin();assert(api.refresh(nullptr)==0&&drive()==0&&kvState.status==Kv::LegacyNative);
 assert(api.get(nullptr,0,&row)&&row.available&&!settingsWanted()&&kvJournal.empty()&&!disk.count(settingsPath));
 puts("SD settings catalog/worker: warm-before-available, 65 interruption/restart points, exact checkpoint before journal cleanup, export exclusion and removed-media retry PASS");
 puts("Provider provisioning: 110 interrupted owner ticks, checked cancellation, restart/resume and unavailable-media retry PASS");

 puts("Provider provisioning: durable intent before I/O, missing image HTTPS, whole graph publication, zero-SD restart and offline journal recovery PASS");
 puts("Actual installed provider: staged discovery, cache-first restart, confirmed image admission, journal-before-publication, two recovery boundaries, update/private identity, stale descriptor and corrupt image rejection PASS");
}
