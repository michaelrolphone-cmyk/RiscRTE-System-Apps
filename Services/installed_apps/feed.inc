#ifndef INSTALLED_APPS_FEED_PRODUCT
#define INSTALLED_APPS_FEED_PRODUCT "xteink-x4-pro"
#endif
#ifdef INSTALLED_APPS_FEED_URL
constexpr unsigned FeedMax=32,FeedBytes=65536;
char feedInput[FeedBytes+1],feedRows[FeedMax][ManifestMax+1];
installed_app_v1 feedViews[FeedMax];
uint32_t feedSize,feedCount,feedGeneration;uint64_t feedHandle;
int32_t feedCancel(void*){
 if(feedHandle){int rc=packageHttp->close(packageHttp->context,feedHandle);if(rc!=RISC_HTTP_OK){retained=true;return INSTALLED_APPS_RETAINED;}feedHandle=0;}
 feedReading=false;return INSTALLED_APPS_OK;
}
int32_t feedRefresh(void*,uint64_t utc){
 if(!started||!loaded||retained||inCall||automaticPhase>=3||feedReading||installId[0]||removeId[0]||volume||scanner||cacheOperation||savePending||!packageHttp||!INSTALLED_APPS_FEED_URL[0]||utc<1704067200ULL||utc>4102444799ULL)return INSTALLED_APPS_UNAVAILABLE;
 risc_http_request_v1 request{sizeof(request),INSTALLED_APPS_FEED_URL,FeedBytes,30000,utc};
 int rc=packageHttp->open(packageHttp->context,&request,&feedHandle);
 if(rc!=RISC_HTTP_OK){if(feedHandle||rc==RISC_HTTP_RETAINED){retained=true;return INSTALLED_APPS_RETAINED;}return INSTALLED_APPS_UNAVAILABLE;}
 feedSize=0;feedReading=true;return INSTALLED_APPS_OK;
}
bool feedParse(){
#ifdef INSTALLED_APPS_BOOT_PROFILES
 if(!RuntimePackages::PackageJsonGuard(feedInput,feedSize,160).objectOnly())return false;
#endif
 JsonDocument doc;if(deserializeJson(doc,feedInput,feedSize,DeserializationOption::NestingLimit(16))||doc["schema"]!="riscrte.native-packages"||doc["version"]!=1||doc["product"]!=INSTALLED_APPS_FEED_PRODUCT||!doc["apps"].is<JsonArrayConst>()||doc["apps"].size()>FeedMax)return false;
 if(!bootFeedValid(doc["boot_profiles"]))return false;
 auto rows=doc["apps"].as<JsonArrayConst>();
 // Complete validation before replacing a previously displayed snapshot.
 unsigned index=0;
 for(JsonObjectConst row:rows){
  if(!row["medium"].isNull()||!row["url"].is<const char*>()||std::strncmp(row["url"],"https://",8)||measureJson(row)>ManifestMax||!row["manifest"]["id"].is<const char*>())return false;
  char source[193];int n=std::snprintf(source,sizeof(source),"Apps/%s/install.json",row["manifest"]["id"].as<const char*>());if(n<=0||size_t(n)>=sizeof(source))return false;
  serializeJson(row,manifestScratch,sizeof(manifestScratch));
  if(!descriptor(source,manifestScratch,stagedScratch,nullptr))return false;
  for(unsigned j=0;j<index;++j)if(rows[j]["manifest"]["id"]==row["manifest"]["id"])return false;
  ++index;
 }
 if(feedGeneration==UINT32_MAX)return false;
 index=0;for(JsonObjectConst row:rows){
  serializeJson(row,feedRows[index],sizeof(feedRows[index]));char source[193];std::snprintf(source,sizeof(source),"Apps/%s/install.json",row["manifest"]["id"].as<const char*>());
  if(!descriptor(source,feedRows[index],stagedScratch,nullptr))return false;
  feedViews[index]=stagedScratch.view;++index;
 }
 feedCount=index;++feedGeneration;return true;
}
int32_t feedStep(void*){
 if(retained)return INSTALLED_APPS_RETAINED;
 if(!feedReading)return INSTALLED_APPS_OK;
 uint32_t size=0;uint8_t bytes[RISC_HTTP_CHUNK_MAX];int rc=packageHttp->read(packageHttp->context,feedHandle,bytes,sizeof(bytes),&size);
 if(rc==RISC_HTTP_AGAIN)return INSTALLED_APPS_WORKING;
 if(rc==RISC_HTTP_OK&&size&&size<=sizeof(bytes)&&feedSize<=FeedBytes-size){std::memcpy(feedInput+feedSize,bytes,size);feedSize+=size;return INSTALLED_APPS_WORKING;}
 bool good=rc==RISC_HTTP_EOF&&size==0;
 risc_http_response_v1 response{sizeof(response),0,0,0};
 good=good&&packageHttp->info(packageHttp->context,feedHandle,&response)==RISC_HTTP_OK&&response.status_code==200&&response.received_bytes==feedSize&&(response.content_length<0||uint64_t(response.content_length)==feedSize);
 if(feedCancel(nullptr)!=INSTALLED_APPS_OK)return INSTALLED_APPS_RETAINED;
 feedInput[feedSize]=0;return good&&feedParse()?INSTALLED_APPS_OK:INSTALLED_APPS_INVALID;
}
bool feedGet(void*,uint32_t i,installed_app_v1*out){
 if(!out||i>=feedCount)return false;
 *out=feedViews[i];out->available=true;
 for(unsigned j=0;j<currentCount;++j)if(!std::strcmp(current[j].view.id,out->id)&&!newer(out->version,current[j].view.version))out->available=false;
 return true;
}

#else
// Ordinary builds retain the historical memory footprint; selecting the API
// suffix does not allocate a catalog or activate a network feed implicitly.
uint32_t feedSize,feedCount,feedGeneration;uint64_t feedHandle;
char feedRows[1][1];
int32_t feedCancel(void*){return INSTALLED_APPS_OK;}
int32_t feedRefresh(void*,uint64_t){return INSTALLED_APPS_UNAVAILABLE;}
int32_t feedStep(void*){return INSTALLED_APPS_UNAVAILABLE;}
bool feedGet(void*,uint32_t,installed_app_v1*){return false;}
#endif
