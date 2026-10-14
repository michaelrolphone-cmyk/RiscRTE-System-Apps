#include "RiscProviderV2.h"
#include "RiscInstallationV1.h"
#include "RiscInstallationMediaV1.h"
#include "RiscInternalPackageVolumeV1.h"
#include "RiscBoundAppDataV1.h"
#include "RiscBoundAppDataAsyncV1.h"
#include "InstalledAppsV1.h"
#include "InstalledAppsProvisionV1.h"
#include "InstalledAppsAutomaticV1.h"
#include "InstalledAppsBootProfileV1.h"
#ifdef INSTALLED_APPS_BOOT_PROFILES
#include "RiscProvisionRequestV1.h"
#include "runtime/packages/PackageJsonGuard.h"
#endif
#include "InstalledCatalog.h"
#include "runtime/packages/InstalledAppAsyncDiscovery.h"
#include "runtime/packages/StagedAppInstall.h"
#include "runtime/packages/SdPackageDownload.h"
#include "runtime/packages/PagedSettingsCheckpoint.h"
#include "runtime/update/Version.h"
#include <ArduinoJson.h>
#include <cstring>
#include <cstdio>
#include <new>
namespace {
constexpr unsigned Max=INSTALLED_APPS_MAX,ManifestMax=4096,CacheMax=65537;
struct Package {char path[193],json[ManifestMax+1];installed_app_v1 view;uint32_t namespaceSlot;uint32_t medium;bool settingsNeeded;};
constexpr unsigned NamespaceMax=128;
char namespaceIds[NamespaceMax][96];bool namespaceDurable[NamespaceMax];
uint32_t namespaceSlots[Max],mediaSlots[Max];
Package current[Max],pending[Max];unsigned currentCount,pendingCount;
installed_catalog catalog;
struct Staged {installed_app_v1 view;char digest[65];uint32_t size;};
Staged staged[Max],stagedPending[Max];unsigned stagedCount,stagedPendingCount;
using Install=RiscPackages::StagedAppInstall;
Install::Record installRecord;Package installCandidate,stagedScratch;
alignas(Install) unsigned char installMemory[sizeof(Install)];Install*installer;
enum class InstallPhase {Idle,FetchRefresh,FetchRead,FetchEnd,Select,Provision,Admit,Run,SaveJournal,SaveFinal,ProviderFinal,Publish,Abort};
InstallPhase installPhase;bool installIntent,installResume,installPaused;
char installId[96],installVersion[64],installDescriptor[193],installDigest[65],installExpectedDigest[65];uint32_t installExpectedSize;
uint32_t installMedium,removeMedium;bool installMediumSelected;
uint64_t installSession,installOperation;int32_t installOutcome;
risc_storage_async_result_v1 installResult;
bool offerStaged(void*,const char*,const char*);
bool loadActivation(JsonVariantConst);
void saveActivation(JsonDocument&);
int32_t installStep(const risc_storage_volume_api_v1*);
int32_t installCancel();
void preserveStaged(){stagedPendingCount=stagedCount;for(unsigned i=0;i<stagedCount;++i)stagedPending[i]=staged[i];}

risc_install_candidate_v1 candidates[Max];risc_install_result_v1 admissionResults[Max];
const risc_installation_v1*native;
const risc_storage_volume_api_v1*internalVolume;
const risc_installation_media_v1*mediaApi(){return risc_installation_media(native);}
bool readMedium(JsonVariantConst value,uint32_t&out){out=0;if(value.isNull())return true;if(!value.is<uint32_t>()||value.as<uint32_t>()>1)return false;out=value;return !out||(mediaApi()&&(mediaApi()->media_mask&2)&&internalVolume);}
const risc_storage_volume_api_v1*selectedVolume(uint32_t medium,const risc_storage_volume_api_v1*sd){return medium==1?internalVolume:sd;}
int32_t admitPackages(uint32_t count,bool replace,bool check){
 for(unsigned i=0;i<count;++i)mediaSlots[i]=pending[i].medium;
 if(mediaApi())return mediaApi()->namespaced_media(native->context,candidates,namespaceSlots,mediaSlots,count,admissionResults,replace,check);
 for(unsigned i=0;i<count;++i)if(mediaSlots[i])return RISC_INSTALL_DENIED;
 return native->namespaced(native->context,candidates,namespaceSlots,count,admissionResults,replace,check);
}
int32_t selectMedium(const risc_storage_volume_api_v1*);
constexpr unsigned ProviderMax=8;
struct CachedProvider {char manifest[ManifestMax+1],path[193];uint64_t instance;uint32_t medium;};
CachedProvider providers[ProviderMax];unsigned providerCount;
bool providerApi(){return native&&native->struct_size>=RISC_INSTALLATION_PROVIDER_PREFIX_SIZE&&native->providers&&native->available&&native->provider_capacity;}
int32_t loadProviders(JsonVariantConst);
bool saveProviders(JsonDocument&);
bool loadProvision(JsonVariantConst);
void saveProvision(JsonDocument&);
bool prepareProvision(const char*,bool);
int32_t provisionStep(const risc_storage_volume_api_v1*);
int32_t provisionCancel();
const risc_http_client_v1*packageHttp;uint64_t packageUtc;
char provisionJson[ManifestMax+1];bool provisionResume,providerSavePending,provisionOnly;
CachedProvider nextProviders[ProviderMax];unsigned nextProviderCount;
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
bool feedReading;
unsigned scanMedium;bool scanSdComplete,scanAnySuccess;
int32_t feedCancel(void*);
int bootCancel();
bool bootFeedValid(JsonVariantConst);
extern unsigned automaticPhase;
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
 for(JsonObjectConst need:doc["requires"].as<JsonArrayConst>())if(need["capability"]=="storage.key-value")out.settingsNeeded=true;
 if(!out.view.name[0])std::strcpy(out.view.name,out.view.id);
 if(!out.view.icon[0])std::strcpy(out.view.icon,"solid:f15b");
 std::strcpy(out.path,path);std::strcpy(out.json,json);return true;
}
bool offer(void*,const char*path,const char*json){
 if(pendingCount==Max){overflow=true;return false;}
 Package row{};if(!parse(row,path,json))return false;
 if(scanner)row.medium=scanMedium;
 for(unsigned i=0;i<pendingCount;++i)if(!std::strcmp(row.view.id,pending[i].view.id)){if(pending[i].medium!=row.medium)return true;overflow=true;return false;}
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
 int32_t rc=admitPackages(pendingCount,replace,false);
 if(rc==RISC_INSTALL_BUSY)return INSTALLED_APPS_WORKING;
 if(rc!=RISC_INSTALL_OK)return INSTALLED_APPS_UNAVAILABLE;
 if(!installed_catalog_begin_all(&catalog))return INSTALLED_APPS_UNAVAILABLE;
 unsigned accepted=0;
 for(unsigned i=0;i<pendingCount;++i){
  if(results[i].status!=RISC_INSTALL_OK)continue;
  // Legacy UI filename contract is 128 bytes. Never truncate a launch identity.
  if(std::strlen(results[i].launch_path)>=128)continue;
  Package&p=pending[i];std::strcpy(p.view.launch_path,results[i].launch_path);p.view.available=true;
  installed_record record{};std::strcpy(record.id,p.view.id);std::strcpy(record.name,p.view.name);
  std::strcpy(record.version,p.view.version);std::strcpy(record.icon,p.view.icon);std::strcpy(record.path,p.view.launch_path);
  record.kind=INSTALLED_APP;record.medium=p.medium?INSTALLED_INTERNAL:INSTALLED_SD;record.installation_revision=1;record.admitted=true;
  if(!installed_catalog_offer(&catalog,&record)){(void)installed_catalog_finish(&catalog,false);return INSTALLED_APPS_UNAVAILABLE;}
  acceptedIndexes[accepted++]=i;
 }
 bool stageChanged=stagedCount!=stagedPendingCount;
 for(unsigned i=0;!stageChanged&&i<stagedCount;++i){const auto&a=staged[i];const auto&b=stagedPending[i];stageChanged=std::strcmp(a.view.id,b.view.id)||std::strcmp(a.view.name,b.view.name)||std::strcmp(a.view.version,b.view.version)||std::strcmp(a.view.launch_path,b.view.launch_path)||std::strcmp(a.digest,b.digest)||a.size!=b.size;}
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
 if(unchanged&&stageChanged)++catalog.generation;
 for(unsigned i=0;i<accepted;++i)current[i]=pending[acceptedIndexes[i]];
 currentCount=accepted;stagedCount=stagedPendingCount;for(unsigned i=0;i<stagedCount;++i)staged[i]=stagedPending[i];
 commitPending=removalCommitPending=false;return INSTALLED_APPS_OK;
}
int32_t load(){
 providerSavePending=false;
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
   (doc["schema"]!=1&&doc["schema"]!=2&&doc["schema"]!=3&&doc["schema"]!=4&&doc["schema"]!=5&&doc["schema"]!=6&&doc["schema"]!=7)||!doc["apps"].is<JsonArrayConst>()||doc["apps"].size()>Max){loaded=true;return INSTALLED_APPS_INVALID;}
 if((doc["schema"]==4||doc["schema"]==5||(doc["schema"]==6||doc["schema"]==7))&&!doc["removal"].isNull()){
  auto intent=doc["removal"];
  if(!copy(removeId,sizeof(removeId),intent["id"])||!copy(removeVersion,sizeof(removeVersion),intent["version"])||
     !copy(removeSource,sizeof(removeSource),intent["path"])||!managedManifest(removeSource)){loaded=true;return INSTALLED_APPS_INVALID;}
  if(!readMedium(intent["medium"],removeMedium))return INSTALLED_APPS_INVALID;
  removeIntent=true;removePhase=RemovePhase::Refresh;
 }
 if(doc["schema"]==3||doc["schema"]==4||doc["schema"]==5||(doc["schema"]==6||doc["schema"]==7)){
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
  if(!json||!offer(nullptr,row["path"],json)||!readMedium(row["medium"],pending[pendingCount-1].medium)||!namespaceFor(pending[pendingCount-1],false)){loaded=true;pendingCount=0;return INSTALLED_APPS_INVALID;}
 }
 stagedPendingCount=0;
 if(doc["schema"]==5||(doc["schema"]==6||doc["schema"]==7)){
  if(!doc["staged"].is<JsonArrayConst>()||doc["staged"].size()>Max){loaded=true;return INSTALLED_APPS_INVALID;}
  for(JsonObjectConst row:doc["staged"].as<JsonArrayConst>()){
   auto&stage=stagedPending[stagedPendingCount++];stage={};auto&v=stage.view;
   if(!copy(v.id,sizeof(v.id),row["id"])||!copy(v.name,sizeof(v.name),row["name"])||!copy(v.version,sizeof(v.version),row["version"])||!copy(v.icon,sizeof(v.icon),row["icon"])||!copy(v.launch_path,sizeof(v.launch_path),row["path"]))return INSTALLED_APPS_INVALID;
   if(!copy(stage.digest,sizeof(stage.digest),row["sha256"])||std::strlen(stage.digest)!=64||!row["size"].is<uint32_t>()||!row["size"].as<uint32_t>()||row["size"].as<uint32_t>()>2097152u)return INSTALLED_APPS_INVALID;
   stage.size=row["size"];
  }
  if(!loadActivation(doc["activation"]))return INSTALLED_APPS_INVALID;
 }
 int32_t graph=loadProviders(doc["providers"]);if(graph!=INSTALLED_APPS_OK)return graph;
 if(!loadProvision(doc["provision"]))return INSTALLED_APPS_INVALID;
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
 JsonDocument doc;doc["schema"]=7;
 if(!saveProviders(doc))return INSTALLED_APPS_INVALID;
 saveActivation(doc);saveProvision(doc);
 auto stagedRows=doc["staged"].to<JsonArray>();
 for(unsigned i=0;i<stagedPendingCount;++i){const auto&v=stagedPending[i].view;auto row=stagedRows.add<JsonObject>();row["id"]=v.id;row["name"]=v.name;row["version"]=v.version;row["icon"]=v.icon;row["path"]=v.launch_path;row["sha256"]=stagedPending[i].digest;row["size"]=stagedPending[i].size;}
 if(removeIntent){doc["removal"]["id"]=removeId;doc["removal"]["version"]=removeVersion;doc["removal"]["path"]=removeSource;doc["removal"]["medium"]=removeMedium;}
 auto ledger=doc["namespaces"].to<JsonArray>();
 unsigned last=NamespaceMax;while(last&&!namespaceIds[last-1][0])--last;
 for(unsigned i=0;i<last;++i)ledger.add(namespaceIds[i]);
 auto dictionary=doc["requirements"].to<JsonArray>();auto apps=doc["apps"].to<JsonArray>();
 for(unsigned i=0;i<pendingCount;++i){
  JsonDocument manifest;if(deserializeJson(manifest,pending[i].json))return INSTALLED_APPS_INVALID;
  // Description is not used by native admission, launch, handlers or this UI.
  manifest.remove("description");
  auto row=apps.add<JsonObject>();row["path"]=pending[i].path;row["medium"]=pending[i].medium;row["manifest"]=manifest.as<JsonVariantConst>();
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
 int32_t checked=admitPackages(pendingCount,true,true);
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
 if(removePhase==RemovePhase::Idle)for(unsigned i=0;i<currentCount;++i)if(!std::strcmp(removeId,current[i].view.id))removeMedium=current[i].medium;
 v=selectedVolume(removeMedium,v);
 const auto*a=v?risc_storage_volume_async(v):nullptr;
 if(removePhase==RemovePhase::Idle&&(!a||a->async_version<3))return INSTALLED_APPS_UNAVAILABLE;
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
  removeMedium=current[i].medium;v=selectedVolume(removeMedium,v);a=v?risc_storage_volume_async(v):nullptr;if(!a)return INSTALLED_APPS_UNAVAILABLE;
  std::snprintf(removeSource,sizeof(removeSource),"/%s",current[i].path);
  std::strcpy(removeVersion,current[i].view.version);
  preserveStaged();
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
 if(!a||a->async_version<3)return INSTALLED_APPS_UNAVAILABLE;
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
#include "providers.inc"
#include "media.inc"
#include "install.inc"
#include "provision.inc"
#include "settings.inc"
int32_t cancel(void*){
 int bootResult=bootCancel();if(bootResult!=INSTALLED_APPS_OK)return bootResult;
 if(feedCancel(nullptr)!=INSTALLED_APPS_OK)return INSTALLED_APPS_RETAINED;
 if(retained)return INSTALLED_APPS_RETAINED;
 int settingsResult=settingsCancel();if(settingsResult!=INSTALLED_APPS_OK)return settingsResult;
 if(installer||installSession||installId[0]||installIntent){int32_t rc=installCancel();if(rc!=INSTALLED_APPS_OK)return rc;}
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
int32_t refresh(void*){if(!started||retained||inCall||feedReading)return INSTALLED_APPS_UNAVAILABLE;
 if(!settingsCheckpoint)settingsCursor=0;
 if(removePhase!=RemovePhase::Idle){requested=true;return INSTALLED_APPS_WORKING;}
 if(installPhase!=InstallPhase::Idle){requested=true;return INSTALLED_APPS_WORKING;}
 if(!scanner){scanMedium=0;scanSdComplete=scanAnySuccess=false;}
 inCall=true;int32_t rc=loaded?INSTALLED_APPS_OK:load();requested=true;inCall=false;return rc;}
int32_t step(void*,const risc_storage_volume_api_v1*v){
 if(!started||retained||inCall||feedReading)return retained?INSTALLED_APPS_RETAINED:INSTALLED_APPS_UNAVAILABLE;
 inCall=true;struct Guard{~Guard(){inCall=false;}}guard;
 if(settingsCheckpoint)return settingsStep(volume);
 if(savePending)return finishSave();
 if(commitPending){int rc=publish(commitReplace);if(rc==INSTALLED_APPS_OK&&settingsWanted()){requested=true;return INSTALLED_APPS_WORKING;}return rc;}
 if(validationPending)return validateAndSave();
 if(installId[0]||installIntent||installer)return installStep(v);
 if(!scanner&&!removeId[0]&&settingsWanted())return settingsStep(v);
 if(!requested&&!removeId[0])return INSTALLED_APPS_OK;
 if(removeId[0]||removePhase!=RemovePhase::Idle){
  int32_t rc=removeStep(v);return rc==INSTALLED_APPS_OK?INSTALLED_APPS_WORKING:rc;
 }
 const auto*sd=v;
 if(!scanner&&!scanMedium&&(!sd||!risc_storage_volume_async(sd))){
  installed_catalog_media(&catalog,false);scanMedium=1;
 }
 v=scanMedium?internalVolume:sd;
 if(!v||!risc_storage_volume_async(v)){
  requested=false;
  if(!scanAnySuccess)return INSTALLED_APPS_UNAVAILABLE;
  validationPending=true;return INSTALLED_APPS_WORKING;
 }
 if(!scanner){
  volume=v;overflow=false;
  if(!scanMedium){
   pendingCount=stagedPendingCount=0;
   for(unsigned i=0;i<currentCount;++i)if(current[i].medium==1)pending[pendingCount++]=current[i];
  }else {
   if(!scanSdComplete){pendingCount=0;preserveStaged();for(unsigned i=0;i<currentCount;++i)if(!current[i].medium)pending[pendingCount++]=current[i];}
   else {unsigned count=0;for(unsigned i=0;i<pendingCount;++i)if(!pending[i].medium)pending[count++]=pending[i];pendingCount=count;}
  }
  scanner=new(scannerMemory)Scan();
  if(!scanner->start(v,offer,nullptr,scanMedium?nullptr:offerStaged)){scanner->~Scan();scanner=nullptr;volume=nullptr;requested=false;return INSTALLED_APPS_UNAVAILABLE;}
  return INSTALLED_APPS_WORKING;
 }
 if(volume!=v)return INSTALLED_APPS_INVALID;
 scanner->step();auto state=scanner->state();
 if(!scanMedium&&scanner->ready())installed_catalog_media(&catalog,true);
 if(state==Scan::State::Retained){retained=true;return INSTALLED_APPS_RETAINED;}
 if(state!=Scan::State::Complete&&state!=Scan::State::Failed)return INSTALLED_APPS_WORKING;
 bool failed=state==Scan::State::Failed||overflow;
 if(!scanMedium&&state==Scan::State::Failed&&!scanner->ready())installed_catalog_media(&catalog,false);
 scanner->~Scan();scanner=nullptr;volume=nullptr;
 if(failed){
  // Never turn an incomplete directory enumeration into removals.
  unsigned count=0;for(unsigned i=0;i<pendingCount;++i)if(pending[i].medium!=scanMedium)pending[count++]=pending[i];pendingCount=count;
  for(unsigned i=0;i<currentCount;++i)if(current[i].medium==scanMedium&&pendingCount<Max)pending[pendingCount++]=current[i];
  if(!scanMedium)preserveStaged();
 }else scanAnySuccess=true;
 if(!scanMedium&&internalVolume){scanMedium=1;scanSdComplete=true;return INSTALLED_APPS_WORKING;}
 requested=false;
 if(!scanAnySuccess)return INSTALLED_APPS_UNAVAILABLE;
 validationPending=true;return INSTALLED_APPS_WORKING;
}
#include "feed.inc"
#include "boot_profile.inc"
extern unsigned automaticPhase;
bool start(const risc_provider_dependency_v1*d,size_t n){
 bootReset();
#ifdef INSTALLED_APPS_BOOT_PROFILES
 bootRequests=nullptr;
#endif
 automaticPhase=0;scanMedium=0;scanSdComplete=scanAnySuccess=false;
 feedCount=feedSize=feedGeneration=0;feedHandle=0;feedReading=false;
 native=nullptr;storage=nullptr;packageHttp=nullptr;internalVolume=nullptr;packageUtc=0;for(size_t i=0;i<n;++i){
#ifdef INSTALLED_APPS_BOOT_PROFILES
  if(!std::strcmp(d[i].capability_id,RISC_PROVISION_REQUEST_CAPABILITY)&&d[i].api_version==1){
   auto*p=static_cast<const risc_provision_request_api_v1*>(d[i].api);
   if(!p||p->api_version!=1||p->struct_size<sizeof(*p)||!p->status||!p->queue)return false;
   bootRequests=p;
  }
#endif
  if(!std::strcmp(d[i].capability_id,RISC_INTERNAL_PACKAGE_VOLUME_CAPABILITY)&&d[i].api_version==1){auto*p=static_cast<const risc_internal_package_volume_v1*>(d[i].api);if(!p||p->struct_size<sizeof(*p)||!p->exit_safe||!risc_storage_volume_download(p->volume))return false;internalVolume=p->volume;}
  if(!std::strcmp(d[i].capability_id,RISC_HTTP_CLIENT_CAPABILITY)&&d[i].api_version==1)packageHttp=static_cast<const risc_http_client_v1*>(d[i].api);
  if(!std::strcmp(d[i].capability_id,RISC_INSTALLATION_CAPABILITY)&&d[i].api_version==1)native=static_cast<const risc_installation_v1*>(d[i].api);
  if(!std::strcmp(d[i].capability_id,RISC_BOUND_APP_DATA_CAPABILITY)&&d[i].api_version==1)storage=static_cast<const risc_bound_app_data_v1*>(d[i].api);
 }
 if(packageHttp&&(packageHttp->api_version!=1||packageHttp->struct_size<sizeof(*packageHttp)||!packageHttp->open||!packageHttp->read||!packageHttp->info||!packageHttp->close))return false;
 if(!native||native->struct_size<RISC_INSTALLATION_APP_PREFIX_SIZE||!native->commit||!native->forget||!native->check||!native->namespaced||native->namespace_capacity>NamespaceMax||!storage||storage->struct_size<sizeof(*storage)||!storage->stat||!storage->read||!storage->replace)return false;
 settingsCheckpoint=nullptr;settingsCursor=0;
 providerCount=0;provisionJson[0]=0;provisionResume=providerSavePending=provisionOnly=false;provisionPhase=ProvisionPhase::Idle;downloader=nullptr;
 mediaProbeStarted=false;mediaProbePhase=0;
 installMedium=removeMedium=0;installMediumSelected=false;
 installRecord={};installCandidate={};installer=nullptr;installPhase=InstallPhase::Idle;installIntent=installResume=installPaused=false;installId[0]=installVersion[0]=installDescriptor[0]=0;installSession=installOperation=0;stagedCount=stagedPendingCount=0;
 removePhase=RemovePhase::Idle;removeSession=removeOperation=0;removeIntent=false;
 asyncStorage=risc_bound_app_data_async(storage);if(!asyncStorage)return false;cacheOperation=0;savePending=removalCommitPending=false;pendingPersistedSize=0;
 std::memset(namespaceIds,0,sizeof(namespaceIds));std::memset(namespaceDurable,0,sizeof(namespaceDurable));
 installed_catalog_init(&catalog);currentCount=pendingCount=0;persistedKnown=false;persistedSize=0;loaded=requested=retained=inCall=overflow=commitPending=commitReplace=validationPending=false;removeId[0]=0;scanner=nullptr;volume=nullptr;started=true;return true;
}
bool quiesce(){return feedCancel(nullptr)==INSTALLED_APPS_OK&&cancel(nullptr)==INSTALLED_APPS_OK;}
void stop(){started=false;native=nullptr;storage=nullptr;}
extern const installed_apps_feed_v1 extendedApi;
#ifdef INSTALLED_APPS_BOOT_PROFILES
constexpr unsigned AutomaticApiSize=sizeof(installed_apps_boot_profile_v1);
#else
constexpr unsigned AutomaticApiSize=sizeof(installed_apps_automatic_v1);
#endif
const installed_apps_feed_v1 extendedApi={{{1,AutomaticApiSize,nullptr,refresh,
 [](void*)->uint32_t{return catalog.generation;},[](void*)->uint32_t{return currentCount;},
 [](void*,uint32_t i,installed_app_v1*out){if(!out||i>=currentCount)return false;*out=current[i].view;out->available=out->available&&(current[i].medium==1?internalVolume!=nullptr:catalog.sd_ready)&&settingsReady(current[i]);return true;},step,cancel,
 [](void*,const char*id)->int32_t{if(!id||std::strlen(id)>=sizeof(removeId)||retained||feedReading||installId[0]||installIntent||removeId[0]||removePhase!=RemovePhase::Idle)return INSTALLED_APPS_INVALID;std::strcpy(removeId,id);return INSTALLED_APPS_OK;},
 [](void*)->uint32_t{return stagedCount;},
 [](void*,uint32_t i,installed_app_v1*out){if(!out||i>=stagedCount)return false;*out=staged[i].view;out->available=catalog.sd_ready;return true;},
 [](void*,const char*id,const char*version)->int32_t{
  if(loaded&&id&&version&&!retained&&!feedReading&&!removeId[0]&&!removeIntent&&installPhase==InstallPhase::Idle&&!volume&&!installer&&!downloader&&installId[0]&&!std::strcmp(id,installId)&&!std::strcmp(version,installVersion)){installPaused=false;return INSTALLED_APPS_OK;}
  if(!loaded||!id||!version||retained||feedReading||removeId[0]||removeIntent||installId[0]||installIntent)return INSTALLED_APPS_INVALID;
  for(unsigned i=0;i<stagedCount;++i)if(!std::strcmp(staged[i].view.id,id)&&!std::strcmp(staged[i].view.version,version)){
   std::strcpy(installId,id);std::strcpy(installVersion,version);std::strcpy(installDescriptor,staged[i].view.launch_path);std::strcpy(installExpectedDigest,staged[i].digest);installExpectedSize=staged[i].size;installPaused=false;installMedium=0;installMediumSelected=false;return INSTALLED_APPS_OK;
  }
  return INSTALLED_APPS_INVALID;
 },[](void*,uint64_t utc)->int32_t{if(!packageHttp||!installId[0]||utc<1704067200ULL||utc>4102444799ULL)return INSTALLED_APPS_INVALID;packageUtc=utc;return INSTALLED_APPS_OK;},
 [](void*){return started&&!retained&&!inCall&&settingsWanted();}},INSTALLED_APPS_PROVISION_TAG,1,
 [](void*,const char*json)->int32_t{
  if(!started||!loaded||!json||std::strlen(json)>ManifestMax||retained||inCall||feedReading||volume||scanner||settingsCheckpoint||cacheOperation||savePending||commitPending||removeId[0]||removeIntent||installId[0]||installIntent)return INSTALLED_APPS_INVALID;
  JsonDocument plan;if(deserializeJson(plan,json)||!plan["manifest"]["id"].is<const char*>()||!plan["url"].is<const char*>())return INSTALLED_APPS_INVALID;
  // External data cannot choose the medium or overwrite private selections.
  if(!plan["medium"].isNull())return INSTALLED_APPS_INVALID;
  char source[193];int n=std::snprintf(source,sizeof(source),"Apps/%s/install.json",plan["manifest"]["id"].as<const char*>());
  if(n<=0||size_t(n)>=sizeof(source)||!descriptor(source,json,installCandidate,&installRecord))return INSTALLED_APPS_INVALID;
  for(unsigned i=0;i<currentCount;++i)if(!std::strcmp(current[i].view.id,installCandidate.view.id)&&!newer(installCandidate.view.version,current[i].view.version)&&std::strcmp(installCandidate.json,current[i].json))return INSTALLED_APPS_INVALID;
  installMedium=0;installMediumSelected=false;mediaProbeStarted=false;
  if(!prepareProvision(json,false))return INSTALLED_APPS_INVALID;
  std::strcpy(installId,installCandidate.view.id);std::strcpy(installVersion,installCandidate.view.version);std::strcpy(installDescriptor,source);
  installPaused=false;installPhase=InstallPhase::Select;requested=false;return INSTALLED_APPS_OK;
 }},INSTALLED_APPS_FEED_TAG,1,feedRefresh,feedStep,feedCancel,
 [](void*){return feedGeneration;},[](void*){return feedCount;},feedGet,
 [](void*,uint32_t i,uint32_t generation)->int32_t{if(feedReading||generation!=feedGeneration||i>=feedCount)return INSTALLED_APPS_INVALID;return extendedApi.base.install_descriptor(nullptr,feedRows[i]);}};
#include "automatic.inc"
const installed_apps_automatic_v1 automaticApi={extendedApi,INSTALLED_APPS_AUTOMATIC_TAG,1,automaticBegin,automaticStep,automaticCancel};
#ifdef INSTALLED_APPS_BOOT_PROFILES
const installed_apps_boot_profile_v1 bootApi={automaticApi,INSTALLED_APPS_BOOT_PROFILE_TAG,1,bootWifi};
const installed_apps_v1&api=bootApi.base.base.base.base;
#else
const installed_apps_v1&api=automaticApi.base.base.base;
#endif
const risc_driver_v2 driver={2,sizeof(driver),"installed-apps",INSTALLED_APPS_CAPABILITY,1,&api,start,stop,quiesce};
}
extern "C" __attribute__((visibility("default"))) const risc_driver_v2*t5_driver_get(uint32_t abi){return abi==2?&driver:nullptr;}
