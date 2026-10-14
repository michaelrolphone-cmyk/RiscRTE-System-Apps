#define INSTALLED_APPS_BOOT_PROFILES 1
#define INSTALLED_APPS_FEED_URL "https://example.invalid/native-packages-v1.json"
#include "installed_apps_fixture.h"
#include <fstream>
#include <iterator>
using namespace fixture;
static std::string feedBody;static size_t feedAt;static bool feedOpen,closeFail;static int code=200;
static const risc_http_client_v1 feedHttp={1,sizeof(feedHttp),nullptr,
 [](void*,const risc_http_request_v1*q,uint64_t*h)->int32_t{if(strcmp(q->url,INSTALLED_APPS_FEED_URL))return staged_fixture::http.open(nullptr,q,h);assert(!strcmp(q->url,INSTALLED_APPS_FEED_URL)&&q->max_bytes==65536&&q->utc_seconds==1800000000&&!feedOpen);feedOpen=true;feedAt=0;*h=99;return 0;},
 [](void*,uint64_t h,void*out,uint32_t cap,uint32_t*n)->int32_t{if(h!=99)return staged_fixture::http.read(nullptr,h,out,cap,n);assert(h==99&&feedOpen);*n=0;if(feedAt==feedBody.size())return RISC_HTTP_EOF;*n=uint32_t(std::min(size_t(cap),feedBody.size()-feedAt));memcpy(out,feedBody.data()+feedAt,*n);feedAt+=*n;return 0;},
 [](void*,uint64_t h,risc_http_response_v1*out)->int32_t{if(h!=99)return staged_fixture::http.info(nullptr,h,out);assert(h==99&&feedOpen);*out={sizeof(*out),code,uint32_t(feedAt),int64_t(feedBody.size())};return 0;},
 [](void*,uint64_t h)->int32_t{if(h!=99)return staged_fixture::http.close(nullptr,h);assert(h==99&&feedOpen);if(closeFail)return RISC_HTTP_RETAINED;feedOpen=false;return 0;}};


static risc_provision_request_status_v1 requestStatus;
static unsigned queueCalls;static int queueResult;
static const risc_provision_request_api_v1 requests={1,sizeof(requests),nullptr,
 [](void*,risc_provision_request_status_v1*out)->int32_t{assert(out->struct_size==sizeof(*out));*out=requestStatus;return 0;},
 [](void*,const risc_provision_request_v1*q,uint8_t*out)->int32_t{
  ++queueCalls;assert(q->struct_size==sizeof(*q)&&q->profile_size<16384&&q->time_size<384);
  assert(!memcmp(q->expected_active_store_sha256,requestStatus.active_store_sha256,32));
  JsonDocument d,t;assert(!deserializeJson(d,q->profile_json,q->profile_size)&&!deserializeJson(t,q->time_json,q->time_size));
  assert(d["schema"]=="riscrte.provisioning"&&d["wifi"]["ssid"]=="saved default"&&d["wifi"]["password"]=="saved-password");
  assert(t["servers"][0]=="time.example.org");memset(out,0x99,32);
  if(queueResult==0){requestStatus.state=RISC_PROVISION_REQUEST_PENDING;assert(bootDigest(d["image"]["sha256"],requestStatus.target_store_sha256));}
  return queueResult;}};
static void makeCore(bool mirror=false){
 JsonDocument d;d["schema"]="riscrte.native-packages";d["version"]=1;d["product"]=INSTALLED_APPS_FEED_PRODUCT;
 d["apps"].to<JsonArray>();d["profiles"]["core"].to<JsonArray>();
 auto row=d["boot_profiles"]["core"].to<JsonObject>();auto native=row["native"].to<JsonObject>();
 native["layout"]="watch-expanded";native["runtime_version"]="0.2.24";native["store_abi"]=3;native["firmware_size"]=123;
 native["firmware_sha256"]=std::string(64,'1');auto profile=row["profile"].to<JsonObject>();
 profile["schema"]="riscrte.provisioning";profile["schema_version"]=4;profile["image"]["url"]="https://example.invalid/core.bin";
 profile["image"]["bytes"]=5570560;profile["image"]["sha256"]=std::string(64,'3');
 auto files=profile["files"].to<JsonArray>();for(const char*path:{"board.json","boot.json","cohort.json"}){auto f=files.add<JsonArray>();f.add(path);f.add(1);f.add(std::string(64,'0'));}
 row["time"]["schema"]="riscrte.sntp";row["time"]["schema_version"]=1;row["time"]["servers"].to<JsonArray>().add("time.example.org");
 auto mirrors=row["mirrors"].to<JsonArray>();if(mirror){auto m=mirrors.add<JsonObject>();m["driver"]=false;m["size"]=5;m["sha256"]=std::string(64,'0');m["url"]="https://example.invalid/app-image.elf";m["path"]="/Apps/core/image-"+std::string(64,'0')+".elf";}
 feedBody.clear();serializeJson(d,feedBody);
}
static void tick(){executing=true;worker.workerStep();cacheWorker.workerStep();executing=false;}
static void closeCore(){for(unsigned i=0;;++i){assert(i<2000);int rc=automaticApi.cancel(nullptr);if(rc==0)break;assert(rc==INSTALLED_APPS_WORKING);tick();}}
static int driveCore(const risc_storage_volume_api_v1*v){for(unsigned i=0;i<3000;++i){unsigned before=io;int rc=automaticApi.step(nullptr,v);assert(before==io);tick();if(rc!=INSTALLED_APPS_WORKING)return rc;}assert(false);return -99;}
static void beginCore(){bootRequests=&requests;packageHttp=&feedHttp;assert(automaticApi.begin(nullptr,"core",1800000000)==0);}
static bool parseBody(){assert(feedBody.size()<sizeof(feedInput));memcpy(feedInput,feedBody.data(),feedBody.size());feedSize=unsigned(feedBody.size());feedInput[feedSize]=0;return feedParse();}
static void statusReset(){requestStatus={};requestStatus.struct_size=sizeof(requestStatus);requestStatus.store_abi=3;requestStatus.firmware_size=123;strcpy(requestStatus.layout,"watch-expanded");strcpy(requestStatus.runtime_version,"0.2.24");memset(requestStatus.firmware_sha256,0x11,32);memset(requestStatus.active_store_sha256,0x22,32);}
int main(){staged_fixture::setup();staged_fixture::begin();assert(api.refresh(nullptr)==0);statusReset();
 auto*b=installed_apps_boot_profile(&api);assert(b);makeCore();beginCore();assert(driveCore(nullptr)==INSTALLED_APPS_NEEDS_PROFILE_WIFI&&queueCalls==0);
 assert(b->supply_wifi(nullptr,"saved default","saved-password")==INSTALLED_APPS_PROFILE_QUEUED&&queueCalls==1);
 for(char c:bootProfile)assert(!c);
 assert(driveCore(nullptr)==INSTALLED_APPS_PROFILE_QUEUED&&queueCalls==1);closeCore();
 // Same confirmed pending target never retries queue; another target is busy.
 makeCore();beginCore();assert(driveCore(nullptr)==INSTALLED_APPS_PROFILE_QUEUED&&queueCalls==1);closeCore();
 requestStatus.target_store_sha256[0]^=1;beginCore();assert(driveCore(nullptr)==INSTALLED_APPS_UNAVAILABLE&&queueCalls==1);closeCore();
 // Target already active needs neither credentials nor another write.
 statusReset();memset(requestStatus.active_store_sha256,0x33,32);beginCore();assert(driveCore(nullptr)==0&&queueCalls==1);closeCore();
 statusReset();requestStatus.firmware_size++;beginCore();assert(driveCore(nullptr)==INSTALLED_APPS_INVALID&&queueCalls==1);closeCore();statusReset();
 // Public profiles cannot include credentials or replace exact image keys.
 makeCore();JsonDocument d;assert(!deserializeJson(d,feedBody));d["boot_profiles"]["core"]["profile"]["wifi"]["password"]="injected";feedBody.clear();serializeJson(d,feedBody);assert(!parseBody());
 makeCore();assert(!deserializeJson(d,feedBody));d["boot_profiles"]["core"]["profile"]["image"]["size"]=5570560;feedBody.clear();serializeJson(d,feedBody);assert(!parseBody());
 makeCore();feedBody.insert(1,"\"version\":1,");assert(!parseBody());
 makeCore(true);assert(!deserializeJson(d,feedBody));d["boot_profiles"]["core"]["mirrors"][0]["path"]="/Apps/../core.elf";feedBody.clear();serializeJson(d,feedBody);assert(!parseBody());
 // The service accepts the complete bounded cohort and rejects overflow.
 makeCore();assert(!deserializeJson(d,feedBody));auto inventory=d["boot_profiles"]["core"]["profile"]["files"].as<JsonArray>();
 for(unsigned i=3;i<160;++i){auto f=inventory.add<JsonArray>();f.add(std::string("asset-")+std::to_string(i));f.add(1);f.add(std::string(64,'0'));}
 feedBody.clear();serializeJson(d,feedBody);assert(parseBody());inventory.add<JsonArray>();feedBody.clear();serializeJson(d,feedBody);assert(!parseBody());
 // Mirrors are files only. No dynamic manifests, registration or grants.
 makeCore(true);beginCore();unsigned registrations=commits;assert(driveCore(staged_fixture::volume)==INSTALLED_APPS_NEEDS_PROFILE_WIFI);assert(commits==registrations&&api.count(nullptr)==0);
 const std::string path="/Apps/core/image-"+std::string(64,'0')+".elf";assert(staged_fixture::disk.at(path)=="IMAGE");closeCore();
 // Clean missing/exported media falls back to internal bootfs provisioning.
 for(bool unavailable:{false,true}){staged_fixture::disk.clear();present=!unavailable;staged_fixture::exported=!unavailable;makeCore(true);beginCore();assert(driveCore(staged_fixture::volume)==INSTALLED_APPS_NEEDS_PROFILE_WIFI);closeCore();}
 present=true;staged_fixture::exported=false;
 // A clean failed validation or full/write failure keeps internal fallback.
 staged_fixture::validImage=false;staged_fixture::disk.clear();makeCore(true);beginCore();assert(driveCore(staged_fixture::volume)==INSTALLED_APPS_NEEDS_PROFILE_WIFI);closeCore();staged_fixture::validImage=true;
 // Every interruption drains before release. A retry repairs only its own
 // immutable file and never publishes a partial profile or app manifest.
 for(unsigned cut=0;cut<100;++cut){staged_fixture::disk.clear();makeCore(true);beginCore();
  for(unsigned n=0;n<cut;++n){int rc=automaticApi.step(nullptr,staged_fixture::volume);tick();assert(rc==INSTALLED_APPS_WORKING||rc==INSTALLED_APPS_NEEDS_PROFILE_WIFI);if(rc==INSTALLED_APPS_NEEDS_PROFILE_WIFI)break;}
  closeCore();assert(queueCalls==1&&!feedOpen&&!staged_fixture::httpOpen&&staged_fixture::opened.empty());
  beginCore();assert(driveCore(staged_fixture::volume)==INSTALLED_APPS_NEEDS_PROFILE_WIFI);assert(staged_fixture::disk.at(path)=="IMAGE");closeCore();
 }
 // Ambiguous native persistence is not blindly retried in this invocation.
 makeCore();beginCore();assert(driveCore(nullptr)==INSTALLED_APPS_NEEDS_PROFILE_WIFI);queueResult=RISC_PROVISION_REQUEST_UNKNOWN;
 assert(b->supply_wifi(nullptr,"saved default","saved-password")==INSTALLED_APPS_UNAVAILABLE&&queueCalls==2);
 assert(b->supply_wifi(nullptr,"saved default","saved-password")==INSTALLED_APPS_INVALID&&queueCalls==2);for(char c:bootProfile)assert(!c);closeCore();
 // Once a prior profile is consumed, a changed desired profile queues
 // against that precise active-store identity, without firmware redeployment.
 queueResult=0;requestStatus.state=RISC_PROVISION_REQUEST_CONSUMED;memset(requestStatus.active_store_sha256,0x33,32);
 makeCore();assert(!deserializeJson(d,feedBody));d["boot_profiles"]["core"]["profile"]["image"]["sha256"]=std::string(64,'4');feedBody.clear();serializeJson(d,feedBody);
 beginCore();assert(driveCore(nullptr)==INSTALLED_APPS_NEEDS_PROFILE_WIFI);
 assert(b->supply_wifi(nullptr,"saved default","saved-password")==INSTALLED_APPS_PROFILE_QUEUED&&queueCalls==3);closeCore();
 // Retained ownership during SD session acquisition fences fallback/queue.
 statusReset();staged_fixture::disk.clear();makeCore(true);beginCore();
 staged_fixture::packageVolume.base.begin=[](void*,uint64_t*t)->int32_t{*t=0;return RISC_STORAGE_ASYNC_RETAINED;};
 assert(driveCore(staged_fixture::volume)==INSTALLED_APPS_RETAINED&&queueCalls==3&&mirrorDownload);
 assert(automaticApi.cancel(nullptr)==INSTALLED_APPS_RETAINED&&mirrorDownload);

 puts("Core profile service: first-start credential handoff, matching pending/active target, native binding, public-profile rejection, SD mirrors/internal fallback, 160-file bound, 100 interruption retries, changed profile, ambiguous queue and retained-media fences PASS");
}
