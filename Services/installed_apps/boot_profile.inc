/* Public core-store descriptions remain data. Native owns queue persistence,
 * source identity, unchanged policy admission, inactive writes and activation.
 * SD mirrors publish only immutable image files, never manifests or grants. */
#ifdef INSTALLED_APPS_BOOT_PROFILES
const risc_provision_request_api_v1*bootRequests;
char bootProfile[RISC_PROVISION_REQUEST_PROFILE_MAX+1],bootTime[RISC_PROVISION_REQUEST_TIME_MAX+1];
uint32_t bootGeneration,bootMirrorIndex;bool bootSelected,bootWaitingWifi,bootMirrorsDone,bootQueued;
risc_provision_request_status_v1 bootStatus;
alignas(Download) unsigned char mirrorMemory[sizeof(Download)];Download*mirrorDownload;
Download::Record mirrorRecord;
const risc_storage_volume_api_v1*mirrorVolume;
void bootWipe(void*p,size_t n){volatile unsigned char*b=static_cast<volatile unsigned char*>(p);while(n--)*b++=0;}
size_t bootLength(const char*s,size_t bound){size_t n=0;while(n<bound&&s[n])++n;return n;}
bool bootDigest(JsonVariantConst value,uint8_t*out){
 if(!value.is<const char*>()||value.as<JsonString>().size()!=64)return false;
 const char*s=value;for(unsigned i=0;i<32;++i){
  auto hex=[](char c){return c>='0'&&c<='9'?c-'0':c>='a'&&c<='f'?c-'a'+10:-1;};
  int a=hex(s[i*2]),b=hex(s[i*2+1]);if(a<0||b<0)return false;out[i]=uint8_t(a*16+b);
 }return true;
}
bool bootPair(JsonVariantConst row,const risc_provision_request_status_v1&s){
 uint8_t digest[32];return row.is<JsonObjectConst>()&&row.size()==5&&row["layout"].is<const char*>()&&row["runtime_version"].is<const char*>()&&
  row["store_abi"].is<uint32_t>()&&row["firmware_size"].is<uint32_t>()&&
  row["layout"]==s.layout&&row["runtime_version"]==s.runtime_version&&row["store_abi"]==s.store_abi&&row["firmware_size"]==s.firmware_size&&
  bootDigest(row["firmware_sha256"],digest)&&!std::memcmp(digest,s.firmware_sha256,32);
}
bool bootMirror(JsonVariantConst row,Download::Record&out){
 out={};if(!row.is<JsonObjectConst>()||row.size()!=5||!row["driver"].is<bool>()||!row["size"].is<uint32_t>()||
  !row["size"].as<uint32_t>()||row["size"].as<uint32_t>()>2097152u||!bootDigest(row["sha256"],out.sha256)||
  !copy(out.url,sizeof(out.url),row["url"])||std::strncmp(out.url,"https://",8)||!copy(out.path,sizeof(out.path),row["path"]))return false;
 out.driver=row["driver"];out.size=row["size"];
 const char*prefix=out.driver?"/Drivers/":"/Apps/";size_t start=std::strlen(prefix);
 if(std::strncmp(out.path,prefix,start))return false;
 const char*slash=std::strchr(out.path+start,'/');if(!slash||slash==out.path+start)return false;
 for(const char*p=out.path+start;p<slash;++p)if(!((*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='-'||*p=='_'))return false;
 if(std::strlen(slash)!=75||std::strncmp(slash,"/image-",7)||std::strcmp(slash+71,".elf"))return false;
 static const char hex[]="0123456789abcdef";for(unsigned i=0;i<32;++i)if(slash[7+i*2]!=hex[out.sha256[i]>>4]||slash[8+i*2]!=hex[out.sha256[i]&15])return false;
 return true;
}
bool bootFeedValid(JsonVariantConst profiles){
 if(profiles.isNull())return true;
 if(!profiles.is<JsonObjectConst>()||profiles.size()>8)return false;
 for(JsonPairConst named:profiles.as<JsonObjectConst>()){
  const char*name=named.key().c_str();size_t n=std::strlen(name);if(!n||n>=64)return false;
  for(const char*p=name;*p;++p)if(!((*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||*p=='-'||*p=='.'))return false;
  JsonObjectConst row=named.value();if(row.isNull()||row.size()!=4||!row["native"].is<JsonObjectConst>())return false;
  auto profile=row["profile"];if(!profile.is<JsonObjectConst>()||profile.size()!=4||profile["schema"]!="riscrte.provisioning"||
   (profile["schema_version"]!=3&&profile["schema_version"]!=4)||!profile["image"].is<JsonObjectConst>()||!profile["files"].is<JsonArrayConst>()||
   profile["files"].size()<3||profile["files"].size()>160||measureJson(profile)>RISC_PROVISION_REQUEST_PROFILE_MAX-512)return false;
  const auto image=profile["image"];uint8_t digest[32];
  if(image.size()!=3||!image["bytes"].is<uint32_t>()||!image["bytes"].as<uint32_t>()||
   !image["url"].is<const char*>()||image["url"].as<JsonString>().size()>384||std::strncmp(image["url"].as<const char*>(),"https://",8)||!bootDigest(image["sha256"],digest))return false;
  if(profile["schema_version"]==4){for(JsonVariantConst file:profile["files"].as<JsonArrayConst>()){
   if(!file.is<JsonArrayConst>()||file.size()!=3||!file[0].is<const char*>()||!file[0].as<JsonString>().size()||file[0].as<JsonString>().size()>191||
    !file[1].is<uint32_t>()||!file[1].as<uint32_t>()||!bootDigest(file[2],digest))return false;
  }}
  auto time=row["time"];if(!time.is<JsonObjectConst>()||time.size()!=3||time["schema"]!="riscrte.sntp"||time["schema_version"]!=1||
   !time["servers"].is<JsonArrayConst>()||!time["servers"].size()||time["servers"].size()>3||measureJson(time)>RISC_PROVISION_REQUEST_TIME_MAX)return false;
  for(JsonVariantConst server:time["servers"].as<JsonArrayConst>())if(!server.is<const char*>()||!server.as<JsonString>().size()||server.as<JsonString>().size()>95)return false;
  auto mirrors=row["mirrors"];if(!mirrors.is<JsonArrayConst>()||mirrors.size()>64)return false;
  for(JsonVariantConst mirror:mirrors.as<JsonArrayConst>()){Download::Record record;if(!bootMirror(mirror,record))return false;}
 }
 return true;
}
void bootReset(){bootSelected=bootWaitingWifi=bootMirrorsDone=bootQueued=false;bootGeneration=bootMirrorIndex=0;mirrorDownload=nullptr;mirrorVolume=nullptr;bootWipe(bootProfile,sizeof(bootProfile));bootTime[0]=0;bootStatus={};}
int bootCancel(){
 if(mirrorDownload){mirrorDownload->cancel();mirrorDownload->step();auto state=mirrorDownload->state();
  if(state==Download::State::Retained){retained=true;return INSTALLED_APPS_RETAINED;}
  if(state!=Download::State::Ready&&state!=Download::State::Failed&&state!=Download::State::Cancelled)return INSTALLED_APPS_WORKING;
  mirrorDownload->~Download();mirrorDownload=nullptr;mirrorVolume=nullptr;
 }
 bootWaitingWifi=false;bootWipe(bootProfile,sizeof(bootProfile));return INSTALLED_APPS_OK;
}
bool bootSelect(JsonDocument&doc,const char*name){
 bootReset();auto row=doc["boot_profiles"][name];if(row.isNull())return true;
 if(!bootRequests||!bootFeedValid(doc["boot_profiles"]))return false;
 bootStatus={};bootStatus.struct_size=sizeof(bootStatus);
 if(bootRequests->status(bootRequests->context,&bootStatus)!=RISC_PROVISION_REQUEST_OK||
  bootStatus.struct_size!=sizeof(bootStatus)||!bootPair(row["native"],bootStatus))return false;
 serializeJson(row["profile"],bootProfile,sizeof(bootProfile));serializeJson(row["time"],bootTime,sizeof(bootTime));
 bootGeneration=feedGeneration;bootSelected=true;return true;
}
int bootStep(const risc_storage_volume_api_v1*sd,const char*profile,uint64_t utc){
 if(!bootSelected)return INSTALLED_APPS_OK;
 if(bootGeneration!=feedGeneration)return INSTALLED_APPS_INVALID;
 if(bootQueued)return INSTALLED_APPS_PROFILE_QUEUED;
 JsonDocument doc;if(deserializeJson(doc,feedInput,feedSize))return INSTALLED_APPS_INVALID;
 auto row=doc["boot_profiles"][profile];
 if(!bootMirrorsDone){
  auto mirrors=row["mirrors"].as<JsonArrayConst>();
  // No source capability means internal bootfs. Never fabricate an SD device
  // or route core images into the small private app-data package volume.
  if(!sd||!risc_storage_volume_download(sd)||bootMirrorIndex>=mirrors.size())bootMirrorsDone=true;
  else {
   if(!mirrorDownload){
    if(!bootMirror(mirrors[bootMirrorIndex],mirrorRecord))return INSTALLED_APPS_INVALID;
    mirrorDownload=new(mirrorMemory)Download();mirrorVolume=sd;
    // No application policy is published by a mirror. Interrupted .part files
    // cannot be selected by boot; retry validates immutable final bytes anew.
    if(!mirrorDownload->start(sd,packageHttp,mirrorRecord,utc,true)){mirrorDownload->~Download();mirrorDownload=nullptr;mirrorVolume=nullptr;bootMirrorsDone=true;}
   }
   if(mirrorDownload){
    if(mirrorVolume!=sd)return INSTALLED_APPS_INVALID;
    mirrorDownload->step();auto state=mirrorDownload->state();
    if(state==Download::State::Retained){retained=true;return INSTALLED_APPS_RETAINED;}
    if(state!=Download::State::Ready&&state!=Download::State::Failed&&state!=Download::State::Cancelled)return INSTALLED_APPS_WORKING;
    mirrorDownload->~Download();mirrorDownload=nullptr;mirrorVolume=nullptr;
    if(state==Download::State::Ready)++bootMirrorIndex;else bootMirrorsDone=true;
    return INSTALLED_APPS_WORKING;
   }
  }
 }
 bootStatus={};bootStatus.struct_size=sizeof(bootStatus);
 if(bootRequests->status(bootRequests->context,&bootStatus)!=RISC_PROVISION_REQUEST_OK||!bootPair(row["native"],bootStatus))return INSTALLED_APPS_UNAVAILABLE;
 uint8_t target[32];if(!bootDigest(row["profile"]["image"]["sha256"],target))return INSTALLED_APPS_INVALID;
 if(!std::memcmp(target,bootStatus.active_store_sha256,32))return INSTALLED_APPS_OK;
 if(bootStatus.state==RISC_PROVISION_REQUEST_PENDING){if(std::memcmp(target,bootStatus.target_store_sha256,32))return INSTALLED_APPS_UNAVAILABLE;bootQueued=true;return INSTALLED_APPS_PROFILE_QUEUED;}
 if(bootStatus.state==RISC_PROVISION_REQUEST_BLOCKED)return INSTALLED_APPS_UNAVAILABLE;
 bootWaitingWifi=true;return INSTALLED_APPS_NEEDS_PROFILE_WIFI;
}
// Append credentials directly into the already validated bounded public JSON.
// Escape every control byte; ArduinoJson only escapes named controls. This
// avoids credential heap copies and preserves valid control-bearing SSIDs.
bool bootAppend(size_t&used,const char*text){size_t n=std::strlen(text);if(n>RISC_PROVISION_REQUEST_PROFILE_MAX-used)return false;std::memcpy(bootProfile+used,text,n+1);used+=n;return true;}
bool bootQuote(size_t&used,const char*value){
 if(!bootAppend(used,"\""))return false;
 static const char hex[]="0123456789abcdef";
 for(const unsigned char*p=reinterpret_cast<const unsigned char*>(value);*p;++p){
  char escaped[7]={char(*p),0,0,0,0,0,0};
  if(*p<32){escaped[0]='\\';escaped[1]='u';escaped[2]='0';escaped[3]='0';escaped[4]=hex[*p>>4];escaped[5]=hex[*p&15];}
  else if(*p=='"'||*p=='\\'){escaped[0]='\\';escaped[1]=char(*p);}
  bool good=bootAppend(used,escaped);bootWipe(escaped,sizeof(escaped));if(!good)return false;
 }
 return bootAppend(used,"\"");
}
int32_t bootWifi(void*,const char*ssid,const char*password){
 if(!started||retained||!bootSelected||!bootWaitingWifi||!bootRequests||!ssid||!password||
  bootLength(ssid,33)>32||!ssid[0]||bootLength(password,64)>63)return INSTALLED_APPS_INVALID;
 const size_t passwordSize=std::strlen(password);if(passwordSize&&passwordSize<8)return INSTALLED_APPS_INVALID;
 for(size_t i=0;i<passwordSize;++i)if(static_cast<unsigned char>(password[i])<32||static_cast<unsigned char>(password[i])>126)return INSTALLED_APPS_INVALID;
 size_t size=std::strlen(bootProfile);
 if(!size||bootProfile[size-1]!='}')return INSTALLED_APPS_INVALID;
 bootProfile[--size]=0;
 if(!bootAppend(size,",\"wifi\":{\"ssid\":")||!bootQuote(size,ssid)||!bootAppend(size,",\"password\":")||!bootQuote(size,password)||!bootAppend(size,"}}")){
  bootWipe(bootProfile,sizeof(bootProfile));bootWaitingWifi=false;return INSTALLED_APPS_INVALID;
 }
 risc_provision_request_v1 request{};request.struct_size=sizeof(request);request.profile_json=bootProfile;request.profile_size=uint32_t(size);
 request.time_json=bootTime;request.time_size=uint32_t(std::strlen(bootTime));std::memcpy(request.expected_active_store_sha256,bootStatus.active_store_sha256,32);
 uint8_t digest[32]{};int rc=bootRequests->queue(bootRequests->context,&request,digest);
 bootWipe(bootProfile,sizeof(bootProfile));bootWaitingWifi=false;
 if(rc==RISC_PROVISION_REQUEST_OK){bootQueued=true;return INSTALLED_APPS_PROFILE_QUEUED;}
 // UNKNOWN is reconciled only through status on a subsequent refresh. No
// retry of this write, cancellation of the queued intent, or reboot occurs.
 return rc==RISC_PROVISION_REQUEST_INVALID?INSTALLED_APPS_INVALID:INSTALLED_APPS_UNAVAILABLE;
}
#else
[[maybe_unused]] bool bootFeedValid(JsonVariantConst value){return value.isNull();}
void bootReset(){}
int bootCancel(){return INSTALLED_APPS_OK;}
[[maybe_unused]] bool bootSelect(JsonDocument&,const char*){return true;}
[[maybe_unused]] int bootStep(const risc_storage_volume_api_v1*,const char*,uint64_t){return INSTALLED_APPS_OK;}
#endif
