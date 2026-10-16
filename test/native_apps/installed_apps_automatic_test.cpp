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

#ifdef TEST_AUTOMATIC_FRESH_TIME
static unsigned freshHttpOpens;
static risc_http_client_time_v1 timeHttp={feedHttp,RISC_HTTP_TIME_TAG_V1,RISC_HTTP_TIME_VERSION_V1,
 [](void*,const risc_http_request_v1*q,const char*server,uint64_t*h)->int32_t{
  assert(q->utc_seconds==0&&!strcmp(server,"time.cloudflare.com")&&freshTimeActive());++freshHttpOpens;
  // Transport double only: production owns its private sample and never
  // returns it to the service. Existing payload doubles expect this UTC.
  auto request=*q;request.utc_seconds=1800000000;return feedHttp.open(nullptr,&request,h);
 }};
static void selectHttp(){timeHttp.base.struct_size=sizeof(timeHttp);packageHttp=&timeHttp.base;}
static int selectedBegin(const char*profile){
 assert(automaticApi.begin(nullptr,profile,0)==INSTALLED_APPS_INVALID);
 assert(feedRefresh(nullptr,0)==INSTALLED_APPS_UNAVAILABLE);
 assert(api.network_ready(nullptr,0)==INSTALLED_APPS_INVALID);
 const auto*time=installed_apps_automatic_time(&api);assert(time);return time->begin_with_time(nullptr,profile);
}
#else
static void selectHttp(){packageHttp=&feedHttp;}
static int selectedBegin(const char*profile){return automaticApi.begin(nullptr,profile,1800000000);}
#endif

static void makeCatalog(const char*version,bool second=false){
 JsonDocument d;d["schema"]="riscrte.native-packages";d["version"]=1;d["product"]=INSTALLED_APPS_FEED_PRODUCT;
 JsonDocument row;assert(!deserializeJson(row,staged_fixture::envelope(version)));row["url"]="https://example.invalid/app-image.elf";
 auto apps=d["apps"].to<JsonArray>();apps.add(row);auto ids=d["profiles"]["core"].to<JsonArray>();ids.add("new-after-build");
 if(second){row["manifest"]["id"]="second-app";apps.add(row);ids.add("second-app");}
 feedBody.clear();serializeJson(d,feedBody);
}
static int driveAutomatic(){auto*a=installed_apps_automatic(&api);assert(a);
 for(unsigned i=0;i<2000;++i){unsigned before=io;int rc=a->step(nullptr,staged_fixture::volume);assert(before==io);
  executing=true;worker.workerStep();cacheWorker.workerStep();executing=false;
  if(rc!=INSTALLED_APPS_WORKING)return rc;
 }assert(false);return -99;
}
int main(){staged_fixture::setup();staged_fixture::begin();assert(api.refresh(nullptr)==0);selectHttp();
 auto*a=installed_apps_automatic(&api);assert(a);makeCatalog("1.0.0");
 assert(selectedBegin("core")==0);assert(driveAutomatic()==0&&api.count(nullptr)==1);
 unsigned downloads=staged_fixture::httpOpens;assert(selectedBegin("core")==0);assert(driveAutomatic()==0&&staged_fixture::httpOpens==downloads);
 // A preserved legacy package with the same immutable metadata must not be
 // redownloaded just because the feed names its identical version by hash.
 std::string original=current[0].json;JsonDocument legacy;assert(!deserializeJson(legacy,original));legacy["file_name"]="legacy.elf";serializeJson(legacy,current[0].json,sizeof(current[0].json));
 assert(selectedBegin("core")==0);assert(driveAutomatic()==0&&staged_fixture::httpOpens==downloads);
 legacy["name"]="conflicting metadata";serializeJson(legacy,current[0].json,sizeof(current[0].json));
 assert(selectedBegin("core")==0);assert(driveAutomatic()==INSTALLED_APPS_INVALID);assert(a->cancel(nullptr)==0);
 std::strcpy(current[0].json,original.c_str());
 makeCatalog("1.0.1",true);assert(selectedBegin("core")==0);assert(driveAutomatic()==0&&api.count(nullptr)==2);
 assert(!strcmp(current[0].view.version,"1.0.1"));
 makeCatalog("1.0.1");assert(selectedBegin("core")==0);assert(driveAutomatic()==0&&api.count(nullptr)==2); // preserve deselected/user apps
 // Firmware-managed products may have no additional automatic packages yet.
 // An explicit empty profile must preserve all existing/user-managed apps.
 {JsonDocument empty;empty["schema"]="riscrte.native-packages";empty["version"]=1;empty["product"]=INSTALLED_APPS_FEED_PRODUCT;
  empty["apps"].to<JsonArray>();empty["profiles"]["core"].to<JsonArray>();feedBody.clear();serializeJson(empty,feedBody);
  downloads=staged_fixture::httpOpens;assert(selectedBegin("core")==0);
  assert(driveAutomatic()==0&&api.count(nullptr)==2&&staged_fixture::httpOpens==downloads);
 }
 assert(selectedBegin("unknown")==0);assert(driveAutomatic()==INSTALLED_APPS_INVALID);assert(a->cancel(nullptr)==0);
 assert(api.refresh(nullptr)==0);makeCatalog("1.0.0");assert(selectedBegin("core")==0);assert(driveAutomatic()==INSTALLED_APPS_INVALID);assert(a->cancel(nullptr)==0);
 assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();
 for(unsigned cut=0;cut<160;++cut){
  saved.clear();revision=0;staged_fixture::disk.clear();registered.clear();present=true;
  staged_fixture::begin();assert(api.refresh(nullptr)==0);selectHttp();makeCatalog("1.0.0");
  assert(selectedBegin("core")==0);
  for(unsigned n=0;n<cut;++n){int rc=a->step(nullptr,staged_fixture::volume);executing=true;worker.workerStep();cacheWorker.workerStep();executing=false;assert(rc==0||rc==INSTALLED_APPS_WORKING);if(!rc)break;}
  auto journal=saved;auto disk=staged_fixture::disk;
  const bool cancellingFresh=automaticFreshTime;const unsigned cancellingPhase=automaticPhase;
  for(unsigned n=0;;++n){assert(n<1000);int rc=cut%2?api.cancel(nullptr):a->cancel(nullptr);if(!rc)break;assert(rc==INSTALLED_APPS_WORKING&&automaticFreshTime==cancellingFresh&&automaticPhase==cancellingPhase);executing=true;worker.workerStep();cacheWorker.workerStep();executing=false;}
  assert(!automaticFreshTime);assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();saved=journal;revision=journal.empty()?0:revision+1;staged_fixture::disk=disk;
  staged_fixture::begin();assert(api.refresh(nullptr)==0);selectHttp();
  // A profile change after an interrupted download first finishes the owned
  // durable transaction, then reconciles the new desired inventory.
  makeCatalog("1.0.1",true);assert(selectedBegin("core")==0);assert(driveAutomatic()==0&&api.count(nullptr)==2);
  assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();
 }
 puts("Automatic profiles: 160 interruption boundaries with changed-profile restart and durable old-plan recovery PASS");
 puts("Automatic profiles: first-start download, idempotent restart, changed versions/new core app, preserve deselected apps, unknown-profile and downgrade rejection PASS");
}
