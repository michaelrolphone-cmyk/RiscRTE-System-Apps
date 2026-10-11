// Included inside the catalog provider namespace. Metadata and durable cache
// stay on the owner; every SD byte and image check stays on the scoped worker.
bool newer(const char*a,const char*b){uint32_t x[3],y[3];return RiscUpdate::parseVersion(a,x)&&RiscUpdate::parseVersion(b,y)&&RiscUpdate::compareVersion(x,y)>0;}
bool descriptor(const char*source,const char*json,Package&candidate,Install::Record*record,char*digestOut=nullptr,uint32_t*sizeOut=nullptr){
 JsonDocument doc;if(deserializeJson(doc,json)||!doc["manifest"].is<JsonObjectConst>()||!doc["size"].is<uint32_t>()||!doc["sha256"].is<const char*>())return false;
 uint32_t size=doc["size"];const char*digest=doc["sha256"];if(!size||size>2097152u||std::strlen(digest)!=64)return false;
 if(doc["sha256"].as<JsonString>().size()!=64)return false;
 uint8_t bytes[32];for(unsigned i=0;i<64;++i){char c=digest[i];int n=c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;if(n<0)return false;if(i%2)bytes[i/2]|=uint8_t(n);else bytes[i/2]=uint8_t(n<<4);}
 char filename[80];std::snprintf(filename,sizeof(filename),"image-%s.elf",digest);
 if(doc["manifest"]["file_name"]!=filename)return false;
 doc["manifest"].remove("description");
 if(measureJson(doc["manifest"])>ManifestMax)return false;
 serializeJson(doc["manifest"],manifestScratch,sizeof(manifestScratch));
 if(!parse(candidate,source,manifestScratch))return false;
 uint32_t version[3];if(!RiscUpdate::parseVersion(candidate.view.version,version))return false;
 char expected[193];int n=std::snprintf(expected,sizeof(expected),"Apps/%s/install.json",candidate.view.id);
 if(n<=0||size_t(n)>=sizeof(expected)||std::strcmp(source,expected))return false;
 std::snprintf(candidate.path,sizeof(candidate.path),"Apps/%s/manifest.json",candidate.view.id);
 if(record){
  *record={};std::snprintf(record->manifestPath,sizeof(record->manifestPath),"/%s",candidate.path);
  std::snprintf(record->stagePath,sizeof(record->stagePath),"/Apps/%s/manifest.install",candidate.view.id);
  n=std::snprintf(record->imagePath,sizeof(record->imagePath),"/Apps/%s/%s",candidate.view.id,filename);
  if(n<=0||size_t(n)>=128)return false; // existing native launch identity limit
  std::strcpy(record->manifest,candidate.json);record->imageSize=size;std::memcpy(record->digest,bytes,32);std::strcpy(installDigest,digest);
 }
 if(digestOut)std::strcpy(digestOut,digest);
 if(sizeOut)*sizeOut=size;
 return true;
}
bool offerStaged(void*,const char*path,const char*json){
 char digest[65];uint32_t size=0;
 if(!descriptor(path,json,stagedScratch,nullptr,digest,&size))return false;
 for(unsigned i=0;i<pendingCount;++i)if(!std::strcmp(pending[i].view.id,stagedScratch.view.id)&&!newer(stagedScratch.view.version,pending[i].view.version))return true;
 for(unsigned i=0;i<stagedPendingCount;++i)if(!std::strcmp(stagedPending[i].view.id,stagedScratch.view.id)){overflow=true;return false;}
 if(stagedPendingCount==Max){overflow=true;return false;}
 auto&stage=stagedPending[stagedPendingCount++];stage={};stage.view=stagedScratch.view;std::strcpy(stage.view.launch_path,path);std::strcpy(stage.digest,digest);stage.size=size;return true;
}
void saveActivation(JsonDocument&doc){
 if(!installIntent)return;
 JsonDocument m;if(deserializeJson(m,installRecord.manifest))return;
 auto a=doc["activation"].to<JsonObject>();a["manifest"]=m.as<JsonObjectConst>();a["size"]=installRecord.imageSize;a["sha256"]=installDigest;
 if(installRecord.previousPresent)a["previous"]=installRecord.previous;else a["previous"]=nullptr;
}
bool loadActivation(JsonVariantConst value){
 installIntent=installResume=false;installId[0]=0;installPhase=InstallPhase::Idle;
 if(value.isNull())return true;
 if(removeIntent||!value.is<JsonObjectConst>()||!value["manifest"]["id"].is<const char*>())return false;
 char source[193];int n=std::snprintf(source,sizeof(source),"Apps/%s/install.json",value["manifest"]["id"].as<const char*>());
 if(n<=0||size_t(n)>=sizeof(source))return false;
 JsonDocument d;d["manifest"]=value["manifest"];d["size"]=value["size"];d["sha256"]=value["sha256"];
 if(measureJson(d)>ManifestMax)return false;
 serializeJson(d,manifestScratch,sizeof(manifestScratch));
 if(!descriptor(source,manifestScratch,installCandidate,&installRecord)||!namespaceFor(installCandidate,false))return false;
 if(!value["previous"].isNull()){
  if(!copy(installRecord.previous,sizeof(installRecord.previous),value["previous"]))return false;
  installRecord.previousPresent=true;
 }
 std::strcpy(installId,installCandidate.view.id);std::strcpy(installVersion,installCandidate.view.version);std::strcpy(installDescriptor,source);
 installIntent=installResume=true;installPaused=false;return true;
}
void currentPending(){pendingCount=currentCount;for(unsigned i=0;i<currentCount;++i)pending[i]=current[i];preserveStaged();}
bool candidatePending(){
 pendingCount=0;for(unsigned i=0;i<currentCount;++i)if(std::strcmp(current[i].view.id,installCandidate.view.id))pending[pendingCount++]=current[i];
 if(pendingCount==Max)return false;
 pending[pendingCount++]=installCandidate;preserveStaged();
 unsigned n=0;for(unsigned i=0;i<stagedPendingCount;++i)if(std::strcmp(stagedPending[i].view.id,installId))stagedPending[n++]=stagedPending[i];stagedPendingCount=n;return true;
}
int32_t installStep(const risc_storage_volume_api_v1*v){
 if(installPaused)return INSTALLED_APPS_UNAVAILABLE;
 const auto*pkg=v?risc_storage_volume_packages(v):nullptr;
 if(!pkg)return INSTALLED_APPS_UNAVAILABLE;
 const auto*a=&pkg->base;
 if(installPhase==InstallPhase::Idle){
  if(scanner){scanner->cancel();scanner->step();if(scanner->state()==Scan::State::Retained){retained=true;return INSTALLED_APPS_RETAINED;}
   if(scanner->state()!=Scan::State::Complete&&scanner->state()!=Scan::State::Failed)return INSTALLED_APPS_WORKING;
   scanner->~Scan();scanner=nullptr;
  }
  validationPending=requested=false;currentPending();volume=v;
  if(installResume)installPhase=InstallPhase::Admit;
  else {
   int rc=a->begin(v->context,&installSession);if(rc==RISC_STORAGE_ASYNC_BUSY)return INSTALLED_APPS_WORKING;
   if(rc!=RISC_STORAGE_ASYNC_OK)return INSTALLED_APPS_UNAVAILABLE;
   installOutcome=INSTALLED_APPS_WORKING;installPhase=InstallPhase::FetchRefresh;
  }
 }
 if(volume!=v)return INSTALLED_APPS_INVALID;
 if(installPhase==InstallPhase::FetchRefresh||installPhase==InstallPhase::FetchRead){
  if(!installOperation){
   risc_storage_async_request_v1 q{};q.operation=installPhase==InstallPhase::FetchRefresh?RISC_STORAGE_ASYNC_REFRESH:RISC_STORAGE_ASYNC_READ_SMALL_FILE;
   std::snprintf(q.path,sizeof(q.path),"/%s",installDescriptor);
   int rc=a->submit(v->context,installSession,&q,&installOperation);if(rc==RISC_STORAGE_ASYNC_BUSY)return INSTALLED_APPS_WORKING;
   if(rc!=RISC_STORAGE_ASYNC_OK){retained=true;return INSTALLED_APPS_RETAINED;}return INSTALLED_APPS_WORKING;
  }
  int rc=a->collect(v->context,installSession,installOperation,&installResult);
  if(rc==RISC_STORAGE_ASYNC_PENDING||rc==RISC_STORAGE_ASYNC_BUSY)return INSTALLED_APPS_WORKING;
  installOperation=0;if(rc==RISC_STORAGE_ASYNC_RETAINED){retained=true;return INSTALLED_APPS_RETAINED;}
  if(installPhase==InstallPhase::FetchRefresh&&rc==RISC_STORAGE_ASYNC_OK){installPhase=InstallPhase::FetchRead;return INSTALLED_APPS_WORKING;}
  installOutcome=INSTALLED_APPS_INVALID;
  if(installPhase==InstallPhase::FetchRead&&rc==RISC_STORAGE_ASYNC_OK&&installResult.size&&installResult.size<=4096&&!std::memchr(installResult.data,0,installResult.size)){
   std::memcpy(manifestScratch,installResult.data,installResult.size);manifestScratch[installResult.size]=0;
   if(descriptor(installDescriptor,manifestScratch,installCandidate,&installRecord)&&!std::strcmp(installId,installCandidate.view.id)&&!std::strcmp(installVersion,installCandidate.view.version)&&!std::strcmp(installExpectedDigest,installDigest)&&installExpectedSize==installRecord.imageSize)installOutcome=INSTALLED_APPS_OK;
  }
  installPhase=InstallPhase::FetchEnd;
 }
 if(installPhase==InstallPhase::FetchEnd){
  int rc=a->end(v->context,installSession);if(rc==RISC_STORAGE_ASYNC_PENDING||rc==RISC_STORAGE_ASYNC_BUSY)return INSTALLED_APPS_WORKING;
  if(rc!=RISC_STORAGE_ASYNC_OK){retained=true;return INSTALLED_APPS_RETAINED;}installSession=0;
  if(installOutcome!=INSTALLED_APPS_OK){installPaused=true;return installOutcome;}
  installPhase=InstallPhase::Admit;
 }
 if(installPhase==InstallPhase::Admit){
  if(!namespaceFor(installCandidate,!installResume)||!candidatePending()){installPaused=true;return INSTALLED_APPS_INVALID;}
  for(unsigned i=0;i<pendingCount;++i){candidates[i]={pending[i].path,pending[i].json};namespaceSlots[i]=pending[i].namespaceSlot;}
  int rc=native->namespaced(native->context,candidates,namespaceSlots,pendingCount,admissionResults,true,true);
  if(rc==RISC_INSTALL_BUSY)return INSTALLED_APPS_WORKING;
  bool accepted=rc==RISC_INSTALL_OK;for(unsigned i=0;i<pendingCount;++i)accepted&=admissionResults[i].status==RISC_INSTALL_OK;
  if(!accepted){installPaused=true;return INSTALLED_APPS_INVALID;}
  currentPending();installer=new(installMemory)Install();
  if(!installer->start(v,installRecord,installResume)){installer->~Install();installer=nullptr;return INSTALLED_APPS_UNAVAILABLE;}
  installPhase=InstallPhase::Run;
 }
 if(installPhase==InstallPhase::Run){
  installer->step();auto state=installer->state();
  if(state==Install::State::Retained){retained=true;return INSTALLED_APPS_RETAINED;}
  if(state==Install::State::Journal){
   bool previousOkay=true;
   if(installRecord.previousPresent){JsonDocument old;previousOkay=!deserializeJson(old,installRecord.previous)&&old["id"]==installId&&old["version"].is<const char*>()&&newer(installVersion,old["version"].as<const char*>());}
   if(!previousOkay){installOutcome=INSTALLED_APPS_INVALID;installPhase=InstallPhase::Abort;installer->cancel();return INSTALLED_APPS_WORKING;}
   installIntent=true;installPhase=InstallPhase::SaveJournal;
  }else if(state==Install::State::Ready){
   if(!candidatePending()){installPaused=true;return INSTALLED_APPS_INVALID;}
   installPhase=InstallPhase::SaveFinal;
  }else if(state==Install::State::Failed||state==Install::State::Cancelled){installPaused=true;return INSTALLED_APPS_UNAVAILABLE;}
  else return INSTALLED_APPS_WORKING;
 }
 if(installPhase==InstallPhase::SaveJournal){
  int rc=save();if(rc!=INSTALLED_APPS_OK)return rc;
  if(!installer->journalCommitted()){retained=true;return INSTALLED_APPS_RETAINED;}
  installResume=true;installPhase=InstallPhase::Run;return INSTALLED_APPS_WORKING;
 }
 if(installPhase==InstallPhase::SaveFinal){
  installIntent=false;int rc=save();if(rc!=INSTALLED_APPS_OK){installIntent=true;return rc;}
  installPhase=InstallPhase::Publish;
 }
 if(installPhase==InstallPhase::Publish){
  int rc=publish(true);if(rc!=INSTALLED_APPS_OK)return rc;
  installer->~Install();installer=nullptr;installId[0]=0;installIntent=installResume=false;installPhase=InstallPhase::Idle;volume=nullptr;requested=true;return INSTALLED_APPS_OK;
 }
 if(installPhase==InstallPhase::Abort){installer->step();if(installer->state()==Install::State::Retained){retained=true;return INSTALLED_APPS_RETAINED;}
  if(installer->state()!=Install::State::Cancelled)return INSTALLED_APPS_WORKING;
  installPaused=true;return installOutcome;
 }
 return INSTALLED_APPS_WORKING;
}
int32_t installCancel(){
 if(installSession){
  const auto*a=volume?risc_storage_volume_async(volume):nullptr;if(!a){retained=true;return INSTALLED_APPS_RETAINED;}
  int rc=a->end(volume->context,installSession);if(rc==RISC_STORAGE_ASYNC_PENDING||rc==RISC_STORAGE_ASYNC_BUSY)return INSTALLED_APPS_WORKING;
  if(rc!=RISC_STORAGE_ASYNC_OK){retained=true;return INSTALLED_APPS_RETAINED;}installSession=installOperation=0;
 }
 if(installer){
  installer->cancel();installer->step();auto s=installer->state();
  if(s==Install::State::Retained){retained=true;return INSTALLED_APPS_RETAINED;}
  if(s!=Install::State::Cancelled&&s!=Install::State::Ready&&s!=Install::State::Failed)return INSTALLED_APPS_WORKING;
 }
 if(cacheOperation){int rc=save();if(rc==INSTALLED_APPS_WORKING||rc==INSTALLED_APPS_RETAINED)return rc;}
 if(installer){installer->~Install();installer=nullptr;}
 loaded=false;installIntent=installResume=installPaused=false;installPhase=InstallPhase::Idle;installId[0]=0;volume=nullptr;requested=false;return INSTALLED_APPS_OK;
}
