#define INSTALLED_APPS_FEED_URL "https://example.invalid/native-packages-v1.json"
#include "installed_apps_fixture.h"
using namespace fixture;
static std::string feedBody;static size_t feedAt;static bool feedOpen,closeFail;static int code=200;
static const risc_http_client_v1 feedHttp={1,sizeof(feedHttp),nullptr,
 [](void*,const risc_http_request_v1*q,uint64_t*h)->int32_t{assert(!strcmp(q->url,INSTALLED_APPS_FEED_URL)&&q->max_bytes==65536&&q->utc_seconds==1800000000&&!feedOpen);feedOpen=true;feedAt=0;*h=99;return 0;},
 [](void*,uint64_t h,void*out,uint32_t cap,uint32_t*n)->int32_t{assert(h==99&&feedOpen);*n=0;if(feedAt==feedBody.size())return RISC_HTTP_EOF;*n=uint32_t(std::min(size_t(cap),feedBody.size()-feedAt));memcpy(out,feedBody.data()+feedAt,*n);feedAt+=*n;return 0;},
 [](void*,uint64_t h,risc_http_response_v1*out)->int32_t{assert(h==99&&feedOpen);*out={sizeof(*out),code,uint32_t(feedAt),int64_t(feedBody.size())};return 0;},
 [](void*,uint64_t h)->int32_t{assert(h==99&&feedOpen);if(closeFail)return RISC_HTTP_RETAINED;feedOpen=false;return 0;}};
static std::string feedCatalog(){JsonDocument d;d["schema"]="riscrte.native-packages";d["version"]=1;d["product"]="xteink-x4-pro";JsonDocument row;assert(!deserializeJson(row,staged_fixture::envelope("1.0.0")));row["url"]="https://example.invalid/image.elf";d["apps"].to<JsonArray>().add(row);std::string json;serializeJson(d,json);return json;}
static int fetch(const installed_apps_feed_v1*f){assert(f->refresh(nullptr,1800000000)==0);for(unsigned i=0;i<200;++i){int rc=f->step(nullptr);if(rc!=INSTALLED_APPS_WORKING)return rc;}assert(false);return -9;}
int main(){staged_fixture::setup();staged_fixture::begin();assert(api.refresh(nullptr)==0);packageHttp=&feedHttp;auto*f=installed_apps_feed(&api);assert(f);
 feedBody=feedCatalog();assert(fetch(f)==0&&!feedOpen&&f->count(nullptr)==1);uint32_t generation=f->generation(nullptr);installed_app_v1 row{};assert(f->get(nullptr,0,&row)&&row.available&&!strcmp(row.id,"new-after-build"));
 for(unsigned mode=0;mode<6;++mode){feedBody=feedCatalog();JsonDocument d;assert(!deserializeJson(d,feedBody));if(mode==0)d["product"]="t5s3";if(mode==1)d["schema"]="reader.legacy";if(mode==2)d["apps"][0]["medium"]=1;if(mode==3)d["apps"].as<JsonArray>().add(d["apps"][0]);if(mode==4)d["apps"][0]["url"]="http://example.invalid/unsafe";feedBody.clear();serializeJson(d,feedBody);if(mode==5)feedBody.pop_back();assert(fetch(f)==INSTALLED_APPS_INVALID&&!feedOpen&&f->count(nullptr)==1&&f->generation(nullptr)==generation);}
 feedBody=feedCatalog();code=404;assert(fetch(f)==INSTALLED_APPS_INVALID&&f->generation(nullptr)==generation);code=200;
 assert(f->refresh(nullptr,1800000000)==0);assert(f->install(nullptr,0,generation)==INSTALLED_APPS_INVALID);assert(f->cancel(nullptr)==0&&!feedOpen);
 feedBody=feedCatalog();assert(fetch(f)==0&&f->generation(nullptr)!=generation);assert(f->install(nullptr,0,generation)==INSTALLED_APPS_INVALID);assert(f->install(nullptr,0,f->generation(nullptr))==0);assert(installId[0]&&installPhase==InstallPhase::Select);assert(api.cancel(nullptr)==0);
 assert(t5_driver_get(2)->quiesce());t5_driver_get(2)->stop();staged_fixture::begin();assert(api.refresh(nullptr)==0);packageHttp=&feedHttp;
 assert(f->refresh(nullptr,1800000000)==0);closeFail=true;assert(f->cancel(nullptr)==INSTALLED_APPS_RETAINED&&feedOpen&&!t5_driver_get(2)->quiesce());
 puts("Native feed: bounded HTTPS, exact product/schema, atomic catalog replacement, stale-confirmation rejection, descriptor installation and retained close PASS");
}
