// Explicit foreground reconciliation. Every accepted package is independently
// journaled, so a restart or a changed feed cannot lose a partially completed
// installation. No profile removal deletes user apps or their private data.
#ifdef INSTALLED_APPS_FEED_URL
char automaticProfile[64];uint64_t automaticUtc;
unsigned automaticPhase,automaticIndex;bool automaticSelected[FeedMax];
int32_t automaticBegin(void*,const char*profile,uint64_t utc){
 if(!started||!loaded||retained||inCall||automaticPhase||feedReading||!profile||!profile[0]||
    std::strlen(profile)>=sizeof(automaticProfile)||utc<1704067200ULL||utc>4102444799ULL)return INSTALLED_APPS_INVALID;
 for(const char*p=profile;*p;++p)if(!((*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='-'||*p=='.'))return INSTALLED_APPS_INVALID;
 std::strcpy(automaticProfile,profile);automaticUtc=utc;automaticIndex=0;automaticPhase=1;return INSTALLED_APPS_OK;
}
int32_t automaticCancel(void*){
 int rc=cancel(nullptr);if(rc!=INSTALLED_APPS_OK)return rc;
 automaticPhase=0;automaticUtc=0;automaticProfile[0]=0;return INSTALLED_APPS_OK;
}
bool automaticSelect(){
 JsonDocument doc;if(deserializeJson(doc,feedInput,feedSize))return false;
 if(!bootSelect(doc,automaticProfile))return false;
 auto ids=doc["profiles"][automaticProfile];if(!ids.is<JsonArrayConst>()||ids.size()>FeedMax)return false;
 std::memset(automaticSelected,0,sizeof(automaticSelected));
 for(JsonVariantConst id:ids.as<JsonArrayConst>()){
  if(!id.is<const char*>())return false;
  unsigned at=0;for(;at<feedCount;++at)if(!std::strcmp(id.as<const char*>(),feedViews[at].id))break;
  if(at==feedCount||automaticSelected[at])return false;
  automaticSelected[at]=true;
 }
 return true;
}
// 0=current, 1=install/update, -1=conflicting/downgrade descriptor. Compare the
// full immutable app manifest and owned driver manifests, not just app version.
int automaticNeeded(unsigned index){
 JsonDocument plan;if(deserializeJson(plan,feedRows[index])||!selectCatalogProviders(plan))return -1;
 unsigned at=0;for(;at<currentCount;++at)if(!std::strcmp(current[at].view.id,feedViews[index].id))break;
 if(at==currentCount)return 1;
 if(newer(feedViews[index].version,current[at].view.version))return 1;
 JsonDocument app;if(deserializeJson(app,current[at].json))return -1;
 plan["manifest"].remove("description");bool legacyName=false;
 if(app.as<JsonVariantConst>()!=plan["manifest"]){
  // Discovery may own a preserved legacy filename, while a feed uses immutable
  // image-<hash>.elf names. An equal-version naming-only difference requires no
  // write. Keep its exact bytes and namespace; never overwrite a same-version
  // image, or treat any other manifest difference as current.
  const char*oldName=app["file_name"]|"";
  if(!oldName[0]||!std::strncmp(oldName,"image-",6))return -1;
  app["file_name"]=plan["manifest"]["file_name"];
  if(app.as<JsonVariantConst>()!=plan["manifest"])return -1;
  legacyName=true;
 }
 bool changed=false;
 for(JsonObjectConst row:plan["providers"].as<JsonArrayConst>()){
  Download::Record record;CachedProvider wanted;if(!provisionRecord(row,record,wanted))return -1;
  unsigned j=0;for(;j<providerCount;++j){JsonDocument old;if(deserializeJson(old,providers[j].manifest))return -1;
   if(old["id"]==row["manifest"]["id"]&&providers[j].instance==wanted.instance){
    if(std::strcmp(providers[j].manifest,wanted.manifest)){
     if(!old["version"].is<const char*>()||!row["manifest"]["version"].is<const char*>()||!newer(row["manifest"]["version"],old["version"]))return -1;
     changed=true;
    }break;
   }
  }if(j==providerCount)changed=true;
 }
 return changed?(legacyName?-1:1):0;
}
int32_t automaticStep(void*,const risc_storage_volume_api_v1*v){
 if(retained)return INSTALLED_APPS_RETAINED;
 if(!automaticPhase)return INSTALLED_APPS_OK;
 if(installId[0]||installIntent||installer){
  packageUtc=automaticUtc;
  int rc=step(nullptr,v);if(rc!=INSTALLED_APPS_OK)return rc;
  return INSTALLED_APPS_WORKING;
 }
 if(automaticPhase==1){
  // Finish/cancel discovery and recover durable work before starting HTTP.
  if(scanner||cacheOperation||savePending||commitPending||settingsCheckpoint||removeId[0]||removeIntent){int rc=step(nullptr,v);return rc==INSTALLED_APPS_OK?INSTALLED_APPS_WORKING:rc;}
  int rc=feedRefresh(nullptr,automaticUtc);if(rc!=INSTALLED_APPS_OK)return rc;
  automaticPhase=2;return INSTALLED_APPS_WORKING;
 }
 if(automaticPhase==2){
  int rc=feedStep(nullptr);if(rc!=INSTALLED_APPS_OK)return rc;
  if(!automaticSelect())return INSTALLED_APPS_INVALID;
  automaticPhase=3;return INSTALLED_APPS_WORKING;
 }
 while(automaticIndex<feedCount){
  unsigned index=automaticIndex++;
  if(!automaticSelected[index])continue;
  int needed=automaticNeeded(index);if(needed<0){--automaticIndex;return INSTALLED_APPS_INVALID;}
  if(!needed)continue;
  int rc=extendedApi.base.install_descriptor(nullptr,feedRows[index]);
  if(rc!=INSTALLED_APPS_OK){--automaticIndex;return rc;}
  return INSTALLED_APPS_WORKING;
 }
 int core=bootStep(v,automaticProfile,automaticUtc);
 if(core!=INSTALLED_APPS_OK)return core;
 automaticPhase=0;automaticUtc=0;return INSTALLED_APPS_OK;
}
#else
unsigned automaticPhase;
int32_t automaticBegin(void*,const char*,uint64_t){return INSTALLED_APPS_UNAVAILABLE;}
int32_t automaticStep(void*,const risc_storage_volume_api_v1*){return INSTALLED_APPS_UNAVAILABLE;}
int32_t automaticCancel(void*){return cancel(nullptr);}
#endif
