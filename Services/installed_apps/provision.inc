// Selected native package dependency closure. This is not a boot manifest and
// cannot confer native authority: the complete graph is checked by Runtime.
using Download=RiscPackages::SdPackageDownload;
alignas(Download) unsigned char downloadMemory[sizeof(Download)];Download*downloader;
Download::Record downloadRecord;
enum class ProvisionPhase {Idle,Journal,Download,SaveGraph,Publish,Done};
ProvisionPhase provisionPhase;unsigned provisionIndex,provisionCount;
bool provisionAppImage;
const risc_http_client_v1 packageProxy={1,sizeof(packageProxy),nullptr,
 [](void*,const risc_http_request_v1*q,uint64_t*h)->int32_t{
  if(!packageHttp||!packageUtc)return RISC_HTTP_NETWORK;
  auto request=*q;request.utc_seconds=packageUtc;return packageHttp->open(packageHttp->context,&request,h);
 },[](void*,uint64_t h,void*p,uint32_t n,uint32_t*out)->int32_t{return packageHttp->read(packageHttp->context,h,p,n,out);},
 [](void*,uint64_t h,risc_http_response_v1*out)->int32_t{return packageHttp->info(packageHttp->context,h,out);},
 [](void*,uint64_t h)->int32_t{return packageHttp->close(packageHttp->context,h);}};
bool provisionRecord(JsonVariantConst row,Download::Record&record,CachedProvider&provider){
 if(!row.is<JsonObjectConst>()||!row["manifest"].is<JsonObjectConst>()||!row["instance"].is<uint32_t>()||
    !row["size"].is<uint32_t>()||!row["size"].as<uint32_t>()||row["size"].as<uint32_t>()>2097152u||
    !row["sha256"].is<const char*>())return false;
 record={};provider={};const char*digest=row["sha256"];
 if(row["sha256"].as<JsonString>().size()!=64||!copy(record.url,sizeof(record.url),row["url"])||std::strncmp(record.url,"https://",8))return false;
 for(unsigned i=0;i<64;++i){char c=digest[i];int n=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;if(n<0)return false;if(i%2)record.sha256[i/2]|=uint8_t(n);else record.sha256[i/2]=uint8_t(n<<4);}
 char id[96];if(!copy(id,sizeof(id),row["manifest"]["id"]))return false;
 for(const char*p=id;*p;++p)if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||*p=='-'||*p=='_'||*p=='.'))return false;
 if(std::strstr(id,"..")||!std::strcmp(id,"."))return false;
 char file[80];std::snprintf(file,sizeof(file),"image-%s.elf",digest);
 if(row["manifest"]["file_name"]!=file||measureJson(row["manifest"])>ManifestMax)return false;
 int n=std::snprintf(record.path,sizeof(record.path),"/Drivers/%s/%s",id,file);if(n<=0||size_t(n)>=sizeof(record.path))return false;
 std::strcpy(provider.path,record.path);provider.instance=row["instance"].as<uint32_t>();
 serializeJson(row["manifest"],provider.manifest,sizeof(provider.manifest));
 record.size=row["size"];record.driver=true;return true;
}
int32_t checkProvisionGraph(bool check){
 risc_install_provider_v1 rows[ProviderMax]{};
 for(unsigned i=0;i<nextProviderCount;++i)rows[i]={nextProviders[i].manifest,nextProviders[i].path,nextProviders[i].instance};
 return native->providers(native->context,rows,nextProviderCount,check);
}
bool selectCatalogProviders(JsonDocument&doc){
 auto pool=doc["provider_catalog"];
 if(pool.isNull())return true;
 if(!providerApi()||!pool.is<JsonArrayConst>()||pool.size()>ProviderMax||!doc["providers"].isNull())return false;
 bool selected[ProviderMax]{};unsigned selectedCount=0;
 auto require=[&](JsonVariantConst needs,uint64_t instance)->bool{
  if(!needs.is<JsonArrayConst>())return false;
  for(JsonObjectConst need:needs.as<JsonArrayConst>()){
   if(!need["capability"].is<const char*>()||!need["api"].is<uint32_t>())return false;
   const char*cap=need["capability"];uint32_t api=need["api"];
   if(!std::strcmp(cap,"storage.key-value")||!std::strcmp(cap,"storage.app-data"))continue;
   if(native->available(native->context,cap,api,!std::strcmp(cap,"hardware.device")?instance:0))continue;
   unsigned found=ProviderMax;
   for(unsigned i=0;i<pool.size();++i)for(JsonObjectConst offer:pool[i]["manifest"]["provides"].as<JsonArrayConst>())
    if(offer["capability"]==cap&&offer["api"]==api){if(found!=ProviderMax&&found!=i)return false;found=i;}
   if(found==ProviderMax)return false;
   if(!selected[found]){selected[found]=true;++selectedCount;}
  }
  return true;
 };
 if(!require(doc["manifest"]["requires"],0))return false;
 unsigned before=0;
 do{before=selectedCount;for(unsigned i=0;i<pool.size();++i)if(selected[i]){
   if(!pool[i]["instance"].is<uint32_t>()||!require(pool[i]["manifest"]["requires"],pool[i]["instance"].as<uint32_t>()))return false;
  }}while(before!=selectedCount);
 auto output=doc["providers"].to<JsonArray>();for(unsigned i=0;i<pool.size();++i)if(selected[i])output.add(pool[i]);
 doc.remove("provider_catalog");return true;
}
bool prepareProvision(const char*json,bool resume){
 if(downloader)return false;
 JsonDocument doc;if(deserializeJson(doc,json)||!selectCatalogProviders(doc))return false;
 auto rows=doc["providers"];
 provisionAppImage=!doc["url"].isNull();
 if(provisionAppImage){
  if(!copy(downloadRecord.url,sizeof(downloadRecord.url),doc["url"])||
     std::strncmp(downloadRecord.url,"https://",8))return false;
 }
 if(rows.isNull())rows=doc["providers"].to<JsonArray>();
 if(!rows.is<JsonArrayConst>()||rows.size()>ProviderMax||std::strlen(json)>ManifestMax)return false;
 if(!rows.size()&&!provisionAppImage){provisionJson[0]=0;provisionPhase=ProvisionPhase::Done;provisionResume=false;return true;}
 if(rows.size()&&!providerApi())return false;
 nextProviderCount=providerCount;for(unsigned i=0;i<providerCount;++i)nextProviders[i]=providers[i];
 for(unsigned i=0;i<rows.size();++i){
  CachedProvider entry;if(!provisionRecord(rows[i],downloadRecord,entry))return false;
  for(unsigned j=0;j<i;++j)if(rows[i]["manifest"]["id"]==rows[j]["manifest"]["id"]&&rows[i]["instance"]==rows[j]["instance"])return false;
  unsigned slot=0;for(;slot<nextProviderCount;++slot){JsonDocument old;if(deserializeJson(old,nextProviders[slot].manifest))return false;if(old["id"]==rows[i]["manifest"]["id"]&&nextProviders[slot].instance==entry.instance)break;}
  if(slot==nextProviderCount){if(nextProviderCount==ProviderMax)return false;++nextProviderCount;}
  nextProviders[slot]=entry;
 }
 if(nextProviderCount&&(!providerApi()||nextProviderCount>native->provider_capacity))return false;
 // Admission is repeated at publication to account for an intervening live
 // consumer. BUSY never grants permission to replace an active provider.
 int rc=rows.size()?checkProvisionGraph(true):RISC_INSTALL_OK;if(rc!=RISC_INSTALL_OK&&rc!=RISC_INSTALL_BUSY)return false;
 if(measureJson(doc)>ManifestMax)return false;
 serializeJson(doc,provisionJson,sizeof(provisionJson));provisionCount=rows.size();provisionIndex=0;
 provisionCount+=provisionAppImage?1:0;
 provisionResume=resume;provisionPhase=resume?ProvisionPhase::Download:ProvisionPhase::Journal;return true;
}
void saveProvision(JsonDocument&doc){
 if(!provisionJson[0])return;
 JsonDocument plan;if(!deserializeJson(plan,provisionJson))doc["provision"]=plan.as<JsonVariantConst>();
}
bool loadProvision(JsonVariantConst value){
 if(value.isNull()||installIntent){provisionJson[0]=0;provisionResume=false;return true;}
 if(!value.is<JsonObjectConst>()||measureJson(value)>ManifestMax||!value["manifest"]["id"].is<const char*>())return false;
 serializeJson(value,provisionJson,sizeof(provisionJson));
 char source[193];int n=std::snprintf(source,sizeof(source),"Apps/%s/install.json",value["manifest"]["id"].as<const char*>());
 if(n<=0||size_t(n)>=sizeof(source)||!prepareProvision(provisionJson,true)||
    !descriptor(source,provisionJson,installCandidate,&installRecord)||!namespaceFor(installCandidate,false))return false;
 std::strcpy(installId,installCandidate.view.id);std::strcpy(installVersion,installCandidate.view.version);std::strcpy(installDescriptor,source);
 installPaused=installResume=false;installPhase=InstallPhase::Idle;return true;
}
int32_t provisionStep(const risc_storage_volume_api_v1*v){
 if(!provisionJson[0]||provisionPhase==ProvisionPhase::Done)return INSTALLED_APPS_OK;
 if(provisionPhase==ProvisionPhase::Journal){
  if(!namespaceFor(installCandidate,true))return INSTALLED_APPS_INVALID;
  int rc=nextProviderCount?checkProvisionGraph(true):RISC_INSTALL_OK;if(rc==RISC_INSTALL_BUSY)return INSTALLED_APPS_WORKING;if(rc!=RISC_INSTALL_OK)return INSTALLED_APPS_INVALID;
  currentPending();rc=save();if(rc!=INSTALLED_APPS_OK)return rc;
  provisionResume=true;provisionPhase=ProvisionPhase::Download;return INSTALLED_APPS_WORKING;
 }
 if(provisionPhase==ProvisionPhase::Download){
  if(!downloader){
   JsonDocument plan;if(deserializeJson(plan,provisionJson))return INSTALLED_APPS_INVALID;
   if(provisionAppImage&&provisionIndex==provisionCount-1){
    // The app uses the same immutable, copied-worker download transaction as
    // dependencies. The protected plan survives descriptor removal/restart.
    downloadRecord={};
    if(!copy(downloadRecord.url,sizeof(downloadRecord.url),plan["url"]))return INSTALLED_APPS_INVALID;
    std::strcpy(downloadRecord.path,installRecord.imagePath);
    downloadRecord.size=installRecord.imageSize;
    std::memcpy(downloadRecord.sha256,installRecord.digest,32);
   }else{
    CachedProvider entry;if(!provisionRecord(plan["providers"][provisionIndex],downloadRecord,entry))return INSTALLED_APPS_INVALID;
   }
   downloader=new(downloadMemory)Download();
   if(!downloader->start(v,&packageProxy,downloadRecord,packageUtc,true)){downloader->~Download();downloader=nullptr;return INSTALLED_APPS_UNAVAILABLE;}
  }
  if(downloader->state()==Download::State::OpenHttp&&!packageUtc)return INSTALLED_APPS_NEEDS_NETWORK;
  downloader->step();auto state=downloader->state();
  if(state==Download::State::Retained){retained=true;return INSTALLED_APPS_RETAINED;}
  if(state==Download::State::Failed||state==Download::State::Cancelled)return INSTALLED_APPS_UNAVAILABLE;
  if(state!=Download::State::Ready)return INSTALLED_APPS_WORKING;
  downloader->~Download();downloader=nullptr;
  if(++provisionIndex<provisionCount)return INSTALLED_APPS_WORKING;
  packageUtc=0;
  if(provisionCount==(provisionAppImage?1u:0u)){provisionPhase=ProvisionPhase::Done;return INSTALLED_APPS_OK;}
  providerSavePending=true;provisionPhase=ProvisionPhase::SaveGraph;
 }
 if(provisionPhase==ProvisionPhase::SaveGraph){
  int rc=save();if(rc!=INSTALLED_APPS_OK)return rc;
  provisionPhase=ProvisionPhase::Publish;
 }
 if(provisionPhase==ProvisionPhase::Publish){
  int rc=checkProvisionGraph(false);if(rc==RISC_INSTALL_BUSY)return INSTALLED_APPS_WORKING;if(rc!=RISC_INSTALL_OK)return INSTALLED_APPS_INVALID;
  providerCount=nextProviderCount;for(unsigned i=0;i<providerCount;++i)providers[i]=nextProviders[i];
  providerSavePending=false;provisionPhase=ProvisionPhase::Done;
 }
 return INSTALLED_APPS_OK;
}
int32_t provisionCancel(){
 if(downloader){
  downloader->cancel();downloader->step();auto state=downloader->state();
  if(state==Download::State::Retained){retained=true;return INSTALLED_APPS_RETAINED;}
  if(state!=Download::State::Cancelled&&state!=Download::State::Failed&&state!=Download::State::Ready)return INSTALLED_APPS_WORKING;
  downloader->~Download();downloader=nullptr;
 }
 // installCancel collects any outstanding cache write before invalidating
 // its cache snapshot. The durable plan survives interruption for retry.
 packageUtc=0;return INSTALLED_APPS_OK;
}
