#include "RiscProviderV2.h"
#include "RiscInstallationV1.h"
#include "RiscBoundAppDataV1.h"
#include "InstalledAppsV1.h"
#include "InstalledCatalog.h"
#include "runtime/packages/InstalledAppAsyncDiscovery.h"
#include <ArduinoJson.h>
#include <cstring>
#include <cstdio>
#include <new>
namespace {
constexpr unsigned Max=INSTALLED_APPS_MAX,ManifestMax=4096,CacheMax=49152;
struct Package {char path[193],json[ManifestMax+1];installed_app_v1 view;};
Package current[Max],pending[Max];unsigned currentCount,pendingCount;
installed_catalog catalog;
risc_install_candidate_v1 candidates[Max];risc_install_result_v1 admissionResults[Max];
const risc_installation_v1*native;
const risc_bound_app_data_v1*storage;
const risc_storage_volume_api_v1*volume;
using Scan=RiscPackages::InstalledAppAsyncDiscovery;
alignas(Scan) unsigned char scannerMemory[sizeof(Scan)];Scan*scanner;
char scratch[CacheMax],persisted[CacheMax],removeId[96];size_t persistedSize;bool persistedKnown;
bool started,loaded,requested,retained,inCall,overflow,commitPending,commitReplace;
const char cacheName[]="installed.apps";
bool copy(char*out,size_t bound,JsonVariantConst value,bool optional=false){
 if(value.isNull()&&optional){out[0]=0;return true;}
 if(!value.is<const char*>())return false;
 const char*s=value.as<const char*>();size_t n=std::strlen(s);
 if(n>=bound||(!optional&&!n))return false;
 for(size_t i=0;i<n;++i)if(static_cast<unsigned char>(s[i])<32||s[i]==127)return false;
 std::memcpy(out,s,n+1);return true;
}
bool parse(Package&out,const char*path,const char*json){
 if(!path||!json||std::strlen(path)>=sizeof(out.path)||std::strlen(json)>ManifestMax)return false;
 JsonDocument doc;if(deserializeJson(doc,json)||doc["type"]!="application")return false;
 out={};if(!copy(out.view.id,sizeof(out.view.id),doc["id"])||!copy(out.view.version,sizeof(out.view.version),doc["version"]))return false;
 if(!copy(out.view.name,sizeof(out.view.name),doc["display_name"],true)||!copy(out.view.icon,sizeof(out.view.icon),doc["icon"],true))return false;
 for(const char*p=out.view.id;*p;++p)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='-'||*p=='_'||*p=='.'))return false;
 if(!out.view.name[0])std::strcpy(out.view.name,out.view.id);
 if(!out.view.icon[0])std::strcpy(out.view.icon,"solid:f15b");
 std::strcpy(out.path,path);std::strcpy(out.json,json);return true;
}
bool offer(void*,const char*path,const char*json){
 if(pendingCount==Max){overflow=true;return false;}
 Package row{};if(!parse(row,path,json))return false;
 for(unsigned i=0;i<pendingCount;++i)if(!std::strcmp(row.view.id,pending[i].view.id)){overflow=true;return false;}
 pending[pendingCount++]=row;return true;
}
int32_t result(int32_t rc){if(rc==RISC_APP_DATA_RETAINED){retained=true;return INSTALLED_APPS_RETAINED;}return INSTALLED_APPS_UNAVAILABLE;}
int32_t publish(bool replace){
 if(catalog.generation==UINT32_MAX||catalog.scanning)return INSTALLED_APPS_UNAVAILABLE;
 auto*items=candidates;auto*results=admissionResults;
 std::memset(results,0,sizeof(admissionResults));
 unsigned acceptedIndexes[Max]{};
 for(unsigned i=0;i<pendingCount;++i)items[i]={pending[i].path,pending[i].json};
 int32_t rc=native->commit(native->context,items,pendingCount,results,replace);
 if(rc==RISC_INSTALL_BUSY)return INSTALLED_APPS_WORKING;
 if(rc!=RISC_INSTALL_OK)return INSTALLED_APPS_UNAVAILABLE;
 if(!installed_catalog_begin(&catalog,INSTALLED_SD))return INSTALLED_APPS_UNAVAILABLE;
 unsigned accepted=0;
 for(unsigned i=0;i<pendingCount;++i){
  if(results[i].status!=RISC_INSTALL_OK)continue;
  // Legacy UI filename contract is 128 bytes. Never truncate a launch identity.
  if(std::strlen(results[i].launch_path)>=128)continue;
  Package&p=pending[i];std::strcpy(p.view.launch_path,results[i].launch_path);p.view.available=true;
  installed_record record{};std::strcpy(record.id,p.view.id);std::strcpy(record.name,p.view.name);
  std::strcpy(record.version,p.view.version);std::strcpy(record.icon,p.view.icon);std::strcpy(record.path,p.view.launch_path);
  record.kind=INSTALLED_APP;record.medium=INSTALLED_SD;record.installation_revision=1;record.admitted=true;
  if(!installed_catalog_offer(&catalog,&record)){(void)installed_catalog_finish(&catalog,false);return INSTALLED_APPS_UNAVAILABLE;}
  acceptedIndexes[accepted++]=i;
 }
 bool unchanged=catalog.pending_count==catalog.count;
 for(unsigned i=0;unchanged&&i<catalog.count;++i){
  const installed_record&a=catalog.pending[i],&b=catalog.current[i];
  unchanged=!std::strcmp(a.id,b.id)&&!std::strcmp(a.version,b.version)&&
   !std::strcmp(a.path,b.path)&&!std::strcmp(a.name,b.name)&&!std::strcmp(a.icon,b.icon)&&
   a.installation_revision==b.installation_revision&&a.kind==b.kind&&
   a.medium==b.medium&&a.admitted==b.admitted;
 }
 if(unchanged)(void)installed_catalog_finish(&catalog,false);
 else if(!installed_catalog_finish(&catalog,true))return INSTALLED_APPS_UNAVAILABLE;
 for(unsigned i=0;i<accepted;++i)current[i]=pending[acceptedIndexes[i]];
 currentCount=accepted;commitPending=false;return INSTALLED_APPS_OK;
}
int32_t load(){
 uint32_t size=0,actual=0;uint64_t token=0;int32_t rc=storage->stat(storage->context,cacheName,&size,&token);

 if(rc==RISC_APP_DATA_NOT_FOUND){loaded=true;return INSTALLED_APPS_OK;}
 if(rc!=RISC_APP_DATA_OK)return result(rc);
 if(!size||size>=CacheMax){loaded=true;return INSTALLED_APPS_INVALID;}
 rc=storage->read(storage->context,cacheName,token,scratch,sizeof(scratch)-1,&actual,&token);
 if(rc!=RISC_APP_DATA_OK)return result(rc);
 if(actual>=sizeof(scratch))return INSTALLED_APPS_INVALID;
 scratch[actual]=0;
 JsonDocument doc;if(actual!=size||deserializeJson(doc,scratch)||doc["schema"]!=1||!doc["apps"].is<JsonArrayConst>()||doc["apps"].size()>Max){loaded=true;return INSTALLED_APPS_INVALID;}
 pendingCount=0;overflow=false;
 for(JsonObjectConst row:doc["apps"].as<JsonArrayConst>())if(!row["path"].is<const char*>()||!row["manifest"].is<const char*>()||!offer(nullptr,row["path"],row["manifest"])){loaded=true;pendingCount=0;return INSTALLED_APPS_INVALID;}
 std::memcpy(persisted,scratch,actual);persistedSize=actual;persistedKnown=true;
 loaded=true;int32_t outcome=publish(false);
 if(outcome==INSTALLED_APPS_WORKING){commitPending=true;commitReplace=false;}
 return outcome;
}
int32_t save(){
 JsonDocument doc;doc["schema"]=1;auto apps=doc["apps"].to<JsonArray>();
 for(unsigned i=0;i<pendingCount;++i){auto row=apps.add<JsonObject>();row["path"]=pending[i].path;row["manifest"]=pending[i].json;}
 size_t size=measureJson(doc);if(!size||size>=sizeof(scratch))return INSTALLED_APPS_INVALID;
 serializeJson(doc,scratch,sizeof(scratch));
 if(persistedKnown&&persistedSize==size&&!std::memcmp(persisted,scratch,size))return INSTALLED_APPS_OK;
 // Re-stat before replacement: other bound writes may invalidate session tokens.
 uint32_t oldSize=0;uint64_t token=0;int32_t rc=storage->stat(storage->context,cacheName,&oldSize,&token);
 if(rc!=RISC_APP_DATA_OK&&rc!=RISC_APP_DATA_NOT_FOUND)return result(rc);
 rc=storage->replace(storage->context,cacheName,token,scratch,uint32_t(size));
 if(rc==RISC_APP_DATA_COMMIT_UNKNOWN){loaded=false;persistedKnown=false;return INSTALLED_APPS_UNAVAILABLE;}
 if(rc!=RISC_APP_DATA_OK)return result(rc);
 std::memcpy(persisted,scratch,size);persistedSize=size;persistedKnown=true;
 return INSTALLED_APPS_OK;
}
int32_t cancel(void*){
 if(retained)return INSTALLED_APPS_RETAINED;
 if(scanner){scanner->cancel();scanner->step();
  if(scanner->state()==Scan::State::Retained){retained=true;return INSTALLED_APPS_RETAINED;}
  if(scanner->state()!=Scan::State::Complete&&scanner->state()!=Scan::State::Failed)return INSTALLED_APPS_WORKING;
  scanner->~Scan();scanner=nullptr;
 }
 volume=nullptr;requested=false;return INSTALLED_APPS_OK;
}
int32_t refresh(void*){if(!started||retained||inCall)return INSTALLED_APPS_UNAVAILABLE;
 inCall=true;int32_t rc=loaded?INSTALLED_APPS_OK:load();requested=true;inCall=false;return rc;}
int32_t step(void*,const risc_storage_volume_api_v1*v){
 if(!started||retained||inCall)return retained?INSTALLED_APPS_RETAINED:INSTALLED_APPS_UNAVAILABLE;
 inCall=true;struct Guard{~Guard(){inCall=false;}}guard;
 if(commitPending)return publish(commitReplace);
 if(!requested&&!removeId[0])return INSTALLED_APPS_OK;
 if(!v||!risc_storage_volume_async(v)){installed_catalog_media(&catalog,false);return INSTALLED_APPS_UNAVAILABLE;}
 if(removeId[0]){
  if(scanner)return INSTALLED_APPS_WORKING;
  unsigned i=0;for(;i<currentCount;++i)if(!std::strcmp(removeId,current[i].view.id))break;
  if(i==currentCount){removeId[0]=0;return INSTALLED_APPS_INVALID;}
  const auto*ext=reinterpret_cast<const risc_storage_volume_api_v1_ext*>(v);
  if(v->struct_size<sizeof(*ext)||!ext->rename)return INSTALLED_APPS_INVALID;
  char source[194],destination[205];std::snprintf(source,sizeof(source),"/%s",current[i].path);std::snprintf(destination,sizeof(destination),"%s.uninstalled",source);
  int32_t rc=native->forget(native->context,current[i].view.launch_path,true);
  if(rc!=RISC_INSTALL_OK){removeId[0]=0;return rc==RISC_INSTALL_BUSY?INSTALLED_APPS_WORKING:INSTALLED_APPS_INVALID;}
  if(!ext->rename(v->context,source,destination))return INSTALLED_APPS_UNAVAILABLE;
  rc=native->forget(native->context,current[i].view.launch_path,false);
  if(rc!=RISC_INSTALL_OK){
   if(!ext->rename(v->context,destination,source)){retained=true;return INSTALLED_APPS_RETAINED;}
   return INSTALLED_APPS_UNAVAILABLE;
  }
  removeId[0]=0;requested=true;return INSTALLED_APPS_WORKING;
 }
 if(!scanner){volume=v;pendingCount=0;overflow=false;scanner=new(scannerMemory)Scan();
  if(!scanner->start(v,offer,nullptr)){scanner->~Scan();scanner=nullptr;volume=nullptr;requested=false;return INSTALLED_APPS_UNAVAILABLE;}
  return INSTALLED_APPS_WORKING;
 }
 if(volume!=v)return INSTALLED_APPS_INVALID;
 scanner->step();auto state=scanner->state();
 if(scanner->ready())installed_catalog_media(&catalog,true);
 if(state==Scan::State::Retained){retained=true;return INSTALLED_APPS_RETAINED;}
 if(state!=Scan::State::Complete&&state!=Scan::State::Failed)return INSTALLED_APPS_WORKING;
 if(state==Scan::State::Failed&&!scanner->ready())installed_catalog_media(&catalog,false);
 scanner->~Scan();scanner=nullptr;volume=nullptr;requested=false;
 if(state==Scan::State::Failed||overflow)return INSTALLED_APPS_UNAVAILABLE;
 // No native registry mutation has occurred before complete checked enumeration.
 int32_t rc=save();if(rc!=INSTALLED_APPS_OK)return rc;
 commitPending=commitReplace=true;return INSTALLED_APPS_WORKING;
}
bool start(const risc_provider_dependency_v1*d,size_t n){
 native=nullptr;storage=nullptr;for(size_t i=0;i<n;++i){
  if(!std::strcmp(d[i].capability_id,RISC_INSTALLATION_CAPABILITY)&&d[i].api_version==1)native=static_cast<const risc_installation_v1*>(d[i].api);
  if(!std::strcmp(d[i].capability_id,RISC_BOUND_APP_DATA_CAPABILITY)&&d[i].api_version==1)storage=static_cast<const risc_bound_app_data_v1*>(d[i].api);
 }
 if(!native||native->struct_size<sizeof(*native)||!native->commit||!native->forget||!storage||storage->struct_size<sizeof(*storage)||!storage->stat||!storage->read||!storage->replace)return false;
 installed_catalog_init(&catalog);currentCount=pendingCount=0;persistedKnown=false;persistedSize=0;loaded=requested=retained=inCall=overflow=commitPending=commitReplace=false;removeId[0]=0;scanner=nullptr;volume=nullptr;started=true;return true;
}
bool quiesce(){return cancel(nullptr)==INSTALLED_APPS_OK;}
void stop(){started=false;native=nullptr;storage=nullptr;}
const installed_apps_v1 api={1,sizeof(api),nullptr,refresh,
 [](void*)->uint32_t{return catalog.generation;},[](void*)->uint32_t{return currentCount;},
 [](void*,uint32_t i,installed_app_v1*out){if(!out||i>=currentCount)return false;*out=current[i].view;out->available=out->available&&catalog.sd_ready;return true;},step,cancel,
 [](void*,const char*id)->int32_t{if(!id||std::strlen(id)>=sizeof(removeId)||retained)return INSTALLED_APPS_INVALID;std::strcpy(removeId,id);return INSTALLED_APPS_OK;}};
const risc_driver_v2 driver={2,sizeof(driver),"installed-apps",INSTALLED_APPS_CAPABILITY,1,&api,start,stop,quiesce};
}
extern "C" __attribute__((visibility("default"))) const risc_driver_v2*t5_driver_get(uint32_t abi){return abi==2?&driver:nullptr;}
