// Actual catalog provider/coordinator/worker adapter. Media, native ELF result
// and registry are deterministic boundary doubles; no hardware claim.
#define main existing_catalog_regressions
#include "installed_apps_service_test.cpp"
#undef main
#include <map>
namespace staged_fixture {
using namespace fixture;
std::map<std::string,std::string> disk;
std::string opened;size_t position;bool writing,validImage=true;
unsigned validations;
risc_storage_worker_admission_v1 nativeWorker{};
risc_storage_volume_api_v1_packages packageVolume{};
const risc_storage_volume_api_v1*volume;
std::string digest(64,'0');
std::string envelope(const char*version){return std::string("{\"manifest\":{\"type\":\"application\",\"id\":\"new-after-build\",\"version\":\"")+version+"\",\"file_name\":\"image-"+digest+".elf\",\"requires\":[{\"capability\":\"storage.app-data\",\"api\":1}]},\"size\":5,\"sha256\":\""+digest+"\"}";}
const char*manifestPath="/Apps/new-after-build/manifest.json";
const char*descriptorPath="/Apps/new-after-build/install.json";
void setup(){fixture::setup();
 packageVolume.base=transport;auto&fs=packageVolume.base.fs;auto&ext=fs.state.prepared.base.sleep.terminal.power.volume;auto&v=ext.base;
 v.struct_size=sizeof(packageVolume);volume=&v;
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
 nativeWorker.base=workerApi;nativeWorker.base.api_version=1;nativeWorker.base.struct_size=sizeof(nativeWorker);nativeWorker.admission_tag=RISC_STORAGE_ADMISSION_TAG;nativeWorker.admission_version=1;
 nativeWorker.application=[](void*,const risc_storage_volume_api_v1*,const char*path,uint32_t size,const uint8_t*,uint32_t*held)->int32_t{assert(executing);++io;++validations;*held=0;return validImage&&disk.count(path)&&disk[path].size()==size?RISC_STORAGE_WORK_OK:RISC_STORAGE_WORK_INVALID;};
 bridge.worker=&nativeWorker.base;bridge.volume=&v;bridge.ext=&ext;bridge.fs=&fs;
 packageVolume.package_tag=RISC_STORAGE_PACKAGE_TAG;packageVolume.package_version=1;
 packageVolume.submit_package=[](void*,uint64_t s,const risc_storage_package_request_v1*q,uint64_t*t){return risc_storage_package_submit(&bridge,s,q,t);};
 installation.commit=[](void*,const risc_install_candidate_v1*items,uint32_t count,risc_install_result_v1*out,bool replace)->int32_t{
  if(denyCommit)return RISC_INSTALL_BUSY;++commits;if(replace)registered.clear();
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
 puts("Actual installed provider: staged discovery, cache-first restart, confirmed image admission, journal-before-publication, two recovery boundaries, update/private identity, stale descriptor and corrupt image rejection PASS");
}
