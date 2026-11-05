#define INSTALLED_APPS_FEED_URL "https://example.invalid/native-packages-v1.json"
#include "installed_apps_fixture.h"
static unsigned freshOpens;
static risc_http_client_time_v1 timeHttp={staged_fixture::http,RISC_HTTP_TIME_TAG_V1,RISC_HTTP_TIME_VERSION_V1,
 [](void*,const risc_http_request_v1*q,const char*server,uint64_t*h)->int32_t{
  ++freshOpens;assert(q->utc_seconds==0&&!strcmp(server,"time.cloudflare.com"));
  auto request=*q;request.utc_seconds=1800000000;return staged_fixture::http.open(nullptr,&request,h);
 }};
int main(){using namespace fixture;staged_fixture::setup();
 for(bool quiesce:{false,true}){
  staged_fixture::begin();assert(api.refresh(nullptr)==0);
  timeHttp.base.struct_size=sizeof(timeHttp);packageHttp=&timeHttp.base;
  const auto*time=installed_apps_automatic_time(&api);assert(time);
  assert(time->begin_with_time(nullptr,"core")==0&&freshTimeActive());
  if(quiesce)assert(t5_driver_get(2)->quiesce());else assert(api.cancel(nullptr)==0);
  assert(!automaticPhase&&!automaticFreshTime&&!packageUtc);
  // A later ordinary install cannot inherit a cancelled explicit transport mode.
  JsonDocument row;assert(!deserializeJson(row,staged_fixture::envelope("1.0.0")));
  row["url"]="https://example.invalid/app-image.elf";std::string json;serializeJson(row,json);
  assert(installed_apps_provision(&api)->install_descriptor(nullptr,json.c_str())==0);
  assert(staged_fixture::drive()==INSTALLED_APPS_NEEDS_NETWORK&&!freshOpens&&!staged_fixture::httpOpen);
  for(unsigned n=0;;++n){assert(n<1000);int rc=api.cancel(nullptr);if(!rc)break;
   assert(rc==INSTALLED_APPS_WORKING);executing=true;worker.workerStep();cacheWorker.workerStep();executing=false;
  }
  assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();
  saved.clear();revision=0;registered.clear();staged_fixture::disk.clear();
 }
 puts("Explicit time mode: base cancellation and quiesce restore ordinary network_ready gating PASS");
}
