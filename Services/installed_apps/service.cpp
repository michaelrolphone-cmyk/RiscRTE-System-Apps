#include "RiscProviderV2.h"
#include "RiscInstallationV1.h"
#include "RiscBoundAppDataV1.h"
#include "RiscBoundAppDataAsyncV1.h"
#include "InstalledAppsV1.h"
#include "InstalledCatalog.h"
#include "runtime/packages/InstalledAppAsyncDiscovery.h"
#include <ArduinoJson.h>
#include <cstring>
#include <cstdio>
#include <new>
namespace {
constexpr unsigned Max=INSTALLED_APPS_MAX,ManifestMax=4096,CacheMax=65537;
struct Package {char path[193],json[ManifestMax+1];installed_app_v1 view;uint32_t namespaceSlot;};
constexpr unsigned NamespaceMax=128;
char namespaceIds[NamespaceMax][96];bool namespaceDurable[NamespaceMax];
uint32_t namespaceSlots[Max];
Package current[Max],pending[Max];unsigned currentCount,pendingCount;
installed_catalog catalog;
risc_install_candidate_v1 candidates[Max];risc_install_result_v1 admissionResults[Max];
const risc_installation_v1*native;
const risc_bound_app_data_v1*storage;const risc_bound_app_data_async_v1*asyncStorage;
uint64_t cacheOperation;size_t pendingPersistedSize;bool savePending,removalCommitPending;
const risc_storage_volume_api_v1*volume;
using Scan=RiscPackages::InstalledAppAsyncDiscovery;
alignas(Scan) unsigned char scannerMemory[sizeof(Scan)];Scan*scanner;
char manifestScratch[ManifestMax+1];
char scratch[CacheMax],persisted[CacheMax],removeId[96];size_t persistedSize;bool persistedKnown;
enum class RemovePhase {Idle,Intent,Commit,Refresh,Read,Delete,End,Clear};
RemovePhase removePhase;uint64_t removeSession,removeOperation;
char removeSource[205],removeVersion[64];int32_t removeOutcome;bool removeIntent;
risc_storage_async_result_v1 removeResult;
bool started,loaded,requested,retained,inCall,overflow,commitPending,commitReplace,validationPending;
const char cacheName[]="installed.apps";
bool copy(char*out,size_t bound,JsonVariantConst value,bool optional=false){
 if(value.isNull()&&optional){out[0]=0;return true;}
 if(!value.is<const char*>())return false;
 const char*s=value.as<const char*>();size_t n=std::strlen(s);
 if(n>=bound||(!optional&&!n))return false;
 for(size_t i=0;i<n;++i)if(static_cast<unsigned char>(s[i])<32||s[i]==127)return false;
 std::memcpy(out,s,n+1);return true;
}
bool managedManifest(const char*p){
 if(std::strncmp(p,"/Apps/",6))return false;
 const char*name=p+6;const char*end=std::strchr(name,'/');
 if(!end||end==name||std::strcmp(end,"/manifest.json"))return false;
 if((end-name==1&&name[0]=='.')||(end-name==2&&name[0]=='.'&&name[1]=='.'))return false;
 for(const char*c=name;c<end;++c)if(!((*c>='a'&&*c<='z')||(*c>='A'&&*c<='Z')||(*c>='0'&&*c<='9')||*c=='-'||*c=='_'||*c=='.'))return false;
 return true;
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
bool needsNamespace(const Package&p){
 JsonDocument doc;if(deserializeJson(doc,p.json))return false;
 for(JsonObjectConst need:doc["requires"].as<JsonArrayConst>())
  if(need["capability"]=="storage.app-data"||need["capability"]=="storage.key-value")return true;
 return false;
}
bool namespaceFor(Package&p,bool allocate){
 p.namespaceSlot=0;
 unsigned capacity=native->namespace_capacity;if(capacity>NamespaceMax)return false;
 for(unsigned i=0;i<capacity;++i)if(!std::strcmp(namespaceIds[i],p.view.id)){p.namespaceSlot=i+1;return true;}
 if(!needsNamespace(p))return true;
 if(allocate)for(unsigned i=0;i<capacity;++i)if(!namespaceIds[i][0]){
  std::strcpy(namespaceIds[i],p.view.id);p.namespaceSlot=i+1;return true;
 }
 return false;
}
void discardUnadmittedNamespaces(){
 for(unsigned i=0;i<NamespaceMax;++i)if(namespaceIds[i][0]&&!namespaceDurable[i]){
  bool found=false;for(unsigned j=0;j<pendingCount;++j)found|=pending[j].namespaceSlot==i+1;
  if(!found)namespaceIds[i][0]=0;
 }
}
int32_t result(int32_t rc){if(rc==RISC_APP_DATA_RETAINED){retained=true;return INSTALLED_APPS_RETAINED;}return INSTALLED_APPS_UNAVAILABLE;}
int32_t publish(bool replace){
 if(catalog.generation==UINT32_MAX||catalog.scanning)return INSTALLED_APPS_UNAVAILABLE;
 auto*items=candidates;auto*results=admissionResults;
 std::memset(results,0,sizeof(admissionResults));
 unsigned acceptedIndexes[Max]{};
 for(unsigned i=0;i<pendingCount;++i){items[i]={pending[i].path,pending[i].json};namespaceSlots[i]=pending[i].namespaceSlot;}
 int32_t rc=native->namespaced(native->context,items,namespaceSlots,pendingCount,results,replace,false);
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
 currentCount=accepted;commitPending=removalCommitPending=false;return INSTALLED_APPS_OK;
}
int32_t load(){
 std::memset(namespaceIds,0,sizeof(namespaceIds));std::memset(namespaceDurable,0,sizeof(namespaceDurable));
 uint32_t size=0,actual=0;uint64_t token=0;int32_t rc=storage->stat(storage->context,cacheName,&size,&token);

 if(rc==RISC_APP_DATA_NOT_FOUND){loaded=true;return INSTALLED_APPS_OK;}
 if(rc!=RISC_APP_DATA_OK)return result(rc);
 if(!size||size>=CacheMax){loaded=true;return INSTALLED_APPS_INVALID;}
 rc=storage->read(storage->context,cacheName,token,scratch,sizeof(scratch)-1,&actual,&token);
 if(rc!=RISC_APP_DATA_OK)return result(rc);
 if(actual>=sizeof(scratch))return INSTALLED_APPS_INVALID;
 scratch[actual]=0;
 JsonDocument doc;if(actual!=size||deserializeJson(doc,static_cast<const char*>(scratch))||
   (doc["schema"]!=1&&doc["schema"]!=2&&doc["schema"]!=3&&doc["schema"]!=4)||!doc["apps"].is<JsonArrayConst>()||doc["apps"].size()>Max){loaded=true;return INSTALLED_APPS_INVALID;}
 if(doc["schema"]==4&&!doc["removal"].isNull()){
  auto intent=doc["removal"];
  if(!copy(removeId,sizeof(removeId),intent["id"])||!copy(removeVersion,sizeof(removeVersion),intent["version"])||
     !copy(removeSource,sizeof(removeSource),intent["path"])||!managedManifest(removeSource)){loaded=true;return INSTALLED_APPS_INVALID;}
  removeIntent=true;removePhase=RemovePhase::Refresh;
 }
 if(doc["schema"]==3||doc["schema"]==4){
  if(!doc["namespaces"].is<JsonArrayConst>()||doc["namespaces"].size()>native->namespace_capacity||doc["namespaces"].size()>NamespaceMax){loaded=true;return INSTALLED_APPS_INVALID;}
  unsigned i=0;for(JsonVariantConst id:doc["namespaces"].as<JsonArrayConst>()){
   if(!copy(namespaceIds[i],sizeof(namespaceIds[i]),id,true)){loaded=true;return INSTALLED_APPS_INVALID;}
   if(namespaceIds[i][0])for(unsigned j=0;j<i;++j)if(!std::strcmp(namespaceIds[j],namespaceIds[i])){loaded=true;return INSTALLED_APPS_INVALID;}
   namespaceDurable[i]=namespaceIds[i][0]!=0;++i;
  }
 }
 pendingCount=0;overflow=false;
 for(JsonObjectConst row:doc["apps"].as<JsonArrayConst>()){
  if(!row["path"].is<const char*>()){loaded=true;return INSTALLED_APPS_INVALID;}
  const char*json=nullptr;
  if(doc["schema"]==1){if(row["manifest"].is<const char*>())json=row["manifest"];}
  else if(row["manifest"].is<JsonObjectConst>()&&doc["requirements"].is<JsonArrayConst>()){
   JsonDocument model;model.set(row["manifest"]);auto needs=model["requires"].to<JsonArray>();
   if(!row["manifest"]["requires"].is<JsonArrayConst>()){loaded=true;return INSTALLED_APPS_INVALID;}
   for(JsonVariantConst index:row["manifest"]["requires"].as<JsonArrayConst>()){
    if(!index.is<unsigned>()||index.as<unsigned>()>=doc["requirements"].size()){loaded=true;return INSTALLED_APPS_INVALID;}
    needs.add(doc["requirements"][index.as<unsigned>()]);
   }
   if(measureJson(model)>ManifestMax){loaded=true;return INSTALLED_APPS_INVALID;}
   serializeJson(model,manifestScratch,sizeof(manifestScratch));json=manifestScratch;
  }
  if(!json||!offer(nullptr,row["path"],json)||!namespaceFor(pending[pendingCount-1],false)){loaded=true;pendingCount=0;return INSTALLED_APPS_INVALID;}
 }
 std::memcpy(persisted,scratch,actual);persistedSize=actual;persistedKnown=true;
 loaded=true;int32_t outcome=publish(false);
 if(outcome==INSTALLED_APPS_WORKING){commitPending=true;commitReplace=false;}
 return outcome;
}
int32_t save(){
 if(cacheOperation){
  int32_t outcome=RISC_APP_DATA_UNAVAILABLE;
  int32_t rc=asyncStorage->collect_replace(storage->context,cacheOperation,&outcome);
  if(rc==RISC_APP_DATA_BUSY)return INSTALLED_APPS_WORKING;
  if(rc!=RISC_APP_DATA_OK)return result(rc);
  cacheOperation=0;
  if(outcome==RISC_APP_DATA_COMMIT_UNKNOWN){loaded=false;persistedKnown=false;return INSTALLED_APPS_UNAVAILABLE;}
  if(outcome!=RISC_APP_DATA_OK)return result(outcome);
  for(unsigned i=0;i<NamespaceMax;++i)namespaceDurable[i]=namespaceIds[i][0]!=0;
  std::memcpy(persisted,scratch,pendingPersistedSize);persistedSize=pendingPersistedSize;persistedKnown=true;
  return INSTALLED_APPS_OK;
 }
 JsonDocument doc;doc["schema"]=4;
 if(removeIntent){doc["removal"]["id"]=removeId;doc["removal"]["version"]=removeVersion;doc["removal"]["path"]=removeSource;}
 auto ledger=doc["namespaces"].to<JsonArray>();
 unsigned last=NamespaceMax;while(last&&!namespaceIds[last-1][0])--last;
 for(unsigned i=0;i<last;++i)ledger.add(namespaceIds[i]);
 auto dictionary=doc["requirements"].to<JsonArray>();auto apps=doc["apps"].to<JsonArray>();
 for(unsigned i=0;i<pendingCount;++i){
  JsonDocument manifest;if(deserializeJson(manifest,pending[i].json))return INSTALLED_APPS_INVALID;
  // Description is not used by native admission, launch, handlers or this UI.
  manifest.remove("description");
  auto row=apps.add<JsonObject>();row["path"]=pending[i].path;row["manifest"]=manifest.as<JsonVariantConst>();
  auto indexes=row["manifest"]["requires"].to<JsonArray>();
  for(JsonVariantConst need:manifest["requires"].as<JsonArrayConst>()){
   unsigned index=0;for(;index<dictionary.size();++index)
    if(dictionary[index]["capability"]==need["capability"]&&dictionary[index]["api"]==need["api"])break;
   if(index==dictionary.size())dictionary.add(need);
   indexes.add(index);
  }
 }
 size_t size=measureJson(doc);if(!size||size>=sizeof(scratch))return INSTALLED_APPS_INVALID;
 serializeJson(doc,scratch,sizeof(scratch));
 if(persistedKnown&&persistedSize==size&&!std::memcmp(persisted,scratch,size))return INSTALLED_APPS_OK;
 // Re-stat before replacement: other bound writes may invalidate session tokens.
 uint32_t oldSize=0;uint64_t token=0;int32_t rc=storage->stat(storage->context,cacheName,&oldSize,&token);
 if(rc!=RISC_APP_DATA_OK&&rc!=RISC_APP_DATA_NOT_FOUND)return result(rc);
 rc=asyncStorage->begin_replace(storage->context,cacheName,token,scratch,uint32_t(size),&cacheOperation);
 if(rc==RISC_APP_DATA_BUSY)return INSTALLED_APPS_WORKING;
 if(rc!=RISC_APP_DATA_OK)return result(rc);
 pendingPersistedSize=size;return INSTALLED_APPS_WORKING;
}
int32_t finishSave(){
 int32_t rc=save();if(rc==INSTALLED_APPS_WORKING)return rc;
 savePending=false;if(rc!=INSTALLED_APPS_OK)return rc;
 commitPending=commitReplace=true;return INSTALLED_APPS_WORKING;
}
int32_t validateAndSave(){
 // Native validation is transactional and restores every slot on return.
 for(unsigned i=0;i<pendingCount;++i){
  // Exhausted or unconfigured private authority denies this package, without
  // lending an existing identity's namespace or blocking unrelated packages.
  (void)namespaceFor(pending[i],true);
  candidates[i]={pending[i].path,pending[i].json};namespaceSlots[i]=pending[i].namespaceSlot;
 }
 int32_t checked=native->namespaced(native->context,candidates,namespaceSlots,pendingCount,admissionResults,true,true);
 if(checked==RISC_INSTALL_BUSY)return INSTALLED_APPS_WORKING;
 validationPending=false;
 if(checked!=RISC_INSTALL_OK)return INSTALLED_APPS_UNAVAILABLE;
 unsigned accepted=0;for(unsigned i=0;i<pendingCount;++i)if(admissionResults[i].status==RISC_INSTALL_OK){
  if(accepted!=i)pending[accepted]=pending[i];
  ++accepted;
 }
 pendingCount=accepted;discardUnadmittedNamespaces();
 savePending=true;return finishSave();
}
// The protected cache records desired removal before touching SD. Restart
// rehydrates the reduced inventory and resumes cleanup; it cannot resurrect the
// old image while the card is missing or exported. Images/settings stay intact.
int32_t removeStep(const risc_storage_volume_api_v1*v){
 const auto*a=v?risc_storage_volume_async(v):nullptr;
 if(!a||a->async_version<3)return INSTALLED_APPS_UNAVAILABLE;
 if(removePhase==RemovePhase::Idle){
  if(scanner){
   scanner->cancel();scanner->step();
   if(scanner->state()==Scan::State::Retained){retained=true;return INSTALLED_APPS_RETAINED;}
   if(scanner->state()!=Scan::State::Complete&&scanner->state()!=Scan::State::Failed)return INSTALLED_APPS_WORKING;
   scanner->~Scan();scanner=nullptr;volume=nullptr;
  }
  unsigned i=0;for(;i<currentCount;++i)if(!std::strcmp(removeId,current[i].view.id))break;
  if(i==currentCount){removeId[0]=0;return INSTALLED_APPS_INVALID;}
  int32_t rc=native->forget(native->context,current[i].view.launch_path,true);
  if(rc==RISC_INSTALL_BUSY)return INSTALLED_APPS_WORKING;
  if(rc!=RISC_INSTALL_OK){removeId[0]=0;return INSTALLED_APPS_INVALID;}
  std::snprintf(removeSource,sizeof(removeSource),"/%s",current[i].path);
  std::strcpy(removeVersion,current[i].view.version);
  pendingCount=0;for(unsigned j=0;j<currentCount;++j)if(j!=i)pending[pendingCount++]=current[j];
  removeIntent=true;removePhase=RemovePhase::Intent;volume=v;
 }
 if(removePhase==RemovePhase::Intent){
  int32_t rc=save();if(rc!=INSTALLED_APPS_OK)return rc;
  removePhase=RemovePhase::Commit;
 }
 if(removePhase==RemovePhase::Commit){
  int32_t rc=publish(true);if(rc!=INSTALLED_APPS_OK)return rc;
  removePhase=RemovePhase::Refresh;
 }
 if(removePhase==RemovePhase::Clear){
  removeIntent=false;
  int32_t rc=save();if(rc!=INSTALLED_APPS_OK){removeIntent=true;return rc;}
  removeId[0]=0;removePhase=RemovePhase::Idle;requested=true;return INSTALLED_APPS_OK;
 }
 if(!removeSession){
  int32_t rc=a->begin(v->context,&removeSession);
  if(rc==RISC_STORAGE_ASYNC_BUSY)return INSTALLED_APPS_WORKING;
  if(rc!=RISC_STORAGE_ASYNC_OK)return INSTALLED_APPS_UNAVAILABLE;
  volume=v;removeOutcome=INSTALLED_APPS_UNAVAILABLE;
 }
 if(volume!=v)return INSTALLED_APPS_INVALID;
 if(removePhase!=RemovePhase::End){
  if(!removeOperation){
   risc_storage_async_request_v1 q{};
   q.operation=removePhase==RemovePhase::Refresh?RISC_STORAGE_ASYNC_REFRESH:
     removePhase==RemovePhase::Read?RISC_STORAGE_ASYNC_READ_SMALL_FILE:RISC_STORAGE_ASYNC_REMOVE_FILE;
   std::strcpy(q.path,removeSource);
   int32_t rc=a->submit(v->context,removeSession,&q,&removeOperation);
   if(rc==RISC_STORAGE_ASYNC_BUSY)return INSTALLED_APPS_WORKING;
   if(rc!=RISC_STORAGE_ASYNC_OK){retained=true;return INSTALLED_APPS_RETAINED;}
   return INSTALLED_APPS_WORKING;
  }
  int32_t rc=a->collect(v->context,removeSession,removeOperation,&removeResult);
  if(rc==RISC_STORAGE_ASYNC_PENDING||rc==RISC_STORAGE_ASYNC_BUSY)return INSTALLED_APPS_WORKING;
  removeOperation=0;
  if(rc==RISC_STORAGE_ASYNC_RETAINED){retained=true;return INSTALLED_APPS_RETAINED;}
  if(removePhase==RemovePhase::Refresh&&rc==RISC_STORAGE_ASYNC_OK){removePhase=RemovePhase::Read;return INSTALLED_APPS_WORKING;}
  if(removePhase==RemovePhase::Read){
   if(rc==RISC_STORAGE_ASYNC_NOT_FOUND)removeOutcome=INSTALLED_APPS_OK;
   else if(rc==RISC_STORAGE_ASYNC_OK){
    JsonDocument manifest;
    if(!deserializeJson(manifest,removeResult.data,removeResult.size)&&manifest["id"].is<const char*>()&&manifest["version"].is<const char*>()){
     if(manifest["id"]==removeId&&manifest["version"]==removeVersion){removePhase=RemovePhase::Delete;return INSTALLED_APPS_WORKING;}
     // A replacement card/version at the same path is not this transaction.
     removeOutcome=INSTALLED_APPS_OK;
    }
   }
  }else if(removePhase==RemovePhase::Delete&&rc==RISC_STORAGE_ASYNC_OK)removeOutcome=INSTALLED_APPS_OK;
  removePhase=RemovePhase::End;
 }
 int32_t rc=a->end(v->context,removeSession);
 if(rc==RISC_STORAGE_ASYNC_PENDING||rc==RISC_STORAGE_ASYNC_BUSY)return INSTALLED_APPS_WORKING;
 if(rc!=RISC_STORAGE_ASYNC_OK){retained=true;return INSTALLED_APPS_RETAINED;}
 removeSession=removeOperation=0;volume=nullptr;
 if(removeOutcome!=INSTALLED_APPS_OK){removePhase=RemovePhase::Refresh;return INSTALLED_APPS_UNAVAILABLE;}
 pendingCount=currentCount;for(unsigned i=0;i<currentCount;++i)pending[i]=current[i];
 removePhase=RemovePhase::Clear;return INSTALLED_APPS_WORKING;
}
int32_t cancel(void*){
 if(retained)return INSTALLED_APPS_RETAINED;
 if(removePhase!=RemovePhase::Idle){
  // Drain already-owned work. An offline cleanup keeps its durable intent but
  // releases the volume, so Home and USB export need not wait for card return.
  if(!volume&&removePhase==RemovePhase::Refresh)return INSTALLED_APPS_OK;
  int32_t rc=removeStep(volume);if(rc==INSTALLED_APPS_WORKING||rc==INSTALLED_APPS_RETAINED)return rc;
  if(removeIntent)return INSTALLED_APPS_OK;
 }
 if(removalCommitPending){
  if(savePending){int32_t rc=finishSave();
   if(rc==INSTALLED_APPS_RETAINED)return rc;
   if(savePending)return INSTALLED_APPS_WORKING;
   if(rc<0){removalCommitPending=false;loaded=false;}
  }
  if(commitPending){int32_t rc=publish(true);if(rc==INSTALLED_APPS_WORKING||rc==INSTALLED_APPS_RETAINED)return rc;}
 }else if(cacheOperation){int32_t rc=save();if(rc==INSTALLED_APPS_WORKING||rc==INSTALLED_APPS_RETAINED)return rc;loaded=false;}
 if(commitPending)loaded=false;
 savePending=commitPending=removalCommitPending=false;
 removeId[0]=0;
 if(scanner){scanner->cancel();scanner->step();
  if(scanner->state()==Scan::State::Retained){retained=true;return INSTALLED_APPS_RETAINED;}
  if(scanner->state()!=Scan::State::Complete&&scanner->state()!=Scan::State::Failed)return INSTALLED_APPS_WORKING;
  scanner->~Scan();scanner=nullptr;
 }
 volume=nullptr;requested=validationPending=false;return INSTALLED_APPS_OK;
}
int32_t refresh(void*){if(!started||retained||inCall)return INSTALLED_APPS_UNAVAILABLE;
 if(removePhase!=RemovePhase::Idle){requested=true;return INSTALLED_APPS_WORKING;}
 inCall=true;int32_t rc=loaded?INSTALLED_APPS_OK:load();requested=true;inCall=false;return rc;}
int32_t step(void*,const risc_storage_volume_api_v1*v){
 if(!started||retained||inCall)return retained?INSTALLED_APPS_RETAINED:INSTALLED_APPS_UNAVAILABLE;
 inCall=true;struct Guard{~Guard(){inCall=false;}}guard;
 if(savePending)return finishSave();
 if(commitPending)return publish(commitReplace);
 if(validationPending)return validateAndSave();
 if(!requested&&!removeId[0])return INSTALLED_APPS_OK;
 if(!v||!risc_storage_volume_async(v)){installed_catalog_media(&catalog,false);return INSTALLED_APPS_UNAVAILABLE;}
 if(removeId[0]||removePhase!=RemovePhase::Idle){
  int32_t rc=removeStep(v);return rc==INSTALLED_APPS_OK?INSTALLED_APPS_WORKING:rc;
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
 validationPending=true;return INSTALLED_APPS_WORKING;
}
bool start(const risc_provider_dependency_v1*d,size_t n){
 native=nullptr;storage=nullptr;for(size_t i=0;i<n;++i){
  if(!std::strcmp(d[i].capability_id,RISC_INSTALLATION_CAPABILITY)&&d[i].api_version==1)native=static_cast<const risc_installation_v1*>(d[i].api);
  if(!std::strcmp(d[i].capability_id,RISC_BOUND_APP_DATA_CAPABILITY)&&d[i].api_version==1)storage=static_cast<const risc_bound_app_data_v1*>(d[i].api);
 }
 if(!native||native->struct_size<sizeof(*native)||!native->commit||!native->forget||!native->check||!native->namespaced||native->namespace_capacity>NamespaceMax||!storage||storage->struct_size<sizeof(*storage)||!storage->stat||!storage->read||!storage->replace)return false;
 removePhase=RemovePhase::Idle;removeSession=removeOperation=0;removeIntent=false;
 asyncStorage=risc_bound_app_data_async(storage);if(!asyncStorage)return false;cacheOperation=0;savePending=removalCommitPending=false;pendingPersistedSize=0;
 std::memset(namespaceIds,0,sizeof(namespaceIds));std::memset(namespaceDurable,0,sizeof(namespaceDurable));
 installed_catalog_init(&catalog);currentCount=pendingCount=0;persistedKnown=false;persistedSize=0;loaded=requested=retained=inCall=overflow=commitPending=commitReplace=validationPending=false;removeId[0]=0;scanner=nullptr;volume=nullptr;started=true;return true;
}
bool quiesce(){return cancel(nullptr)==INSTALLED_APPS_OK;}
void stop(){started=false;native=nullptr;storage=nullptr;}
const installed_apps_v1 api={1,sizeof(api),nullptr,refresh,
 [](void*)->uint32_t{return catalog.generation;},[](void*)->uint32_t{return currentCount;},
 [](void*,uint32_t i,installed_app_v1*out){if(!out||i>=currentCount)return false;*out=current[i].view;out->available=out->available&&catalog.sd_ready;return true;},step,cancel,
 [](void*,const char*id)->int32_t{if(!id||std::strlen(id)>=sizeof(removeId)||retained||removeId[0]||removePhase!=RemovePhase::Idle)return INSTALLED_APPS_INVALID;std::strcpy(removeId,id);return INSTALLED_APPS_OK;}};
const risc_driver_v2 driver={2,sizeof(driver),"installed-apps",INSTALLED_APPS_CAPABILITY,1,&api,start,stop,quiesce};
}
extern "C" __attribute__((visibility("default"))) const risc_driver_v2*t5_driver_get(uint32_t abi){return abi==2?&driver:nullptr;}
