/* Actual provider with the existing HTTP/native-bank boundary doubles. */
#define UPDATE_SOURCE_ROUTES 1
#if !__has_include("../../Services/update/Policy.h")
namespace WatchUpdate {constexpr const char *Product="twatch-s3",*AssetPrefix="twatch-s3";}
#endif
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#define main existing_cohort_fixture_main
#include "portable_update_cohort_test.cpp"
#undef main
#pragma GCC diagnostic pop

std::string route_source(const std::string& rev=std::string(40,'e')) {
 return "{\"product\":\""+std::string(WatchUpdate::Product)+"\",\"version\":\"1.1.0\",\"source_repo\":\""+
 WatchUpdate::Repository+"\",\"source_revision\":\""+rev+"\",\"runtime_version\":\"0.1.33\",\"layout\":\"riscrte-paired-16m-v1\",\"store_abi\":1,\"active_store_sha256\":\""+std::string("2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a2a")+"\"}";
}
std::string firmware_row() {
 auto base=cohortFixture();WatchUpdate::WorkBudget b(nullptr,nullptr);WatchUpdate::Slice row;
 assert(WatchUpdate::field({base.data(),base.size()},"firmware",row,b));return {row.data,row.size};
}
std::string route(const std::string& from,const std::string& firmware) {return "{\"from\":"+from+",\"firmware\":"+firmware+"}";}
std::string feed(const std::string& routes) {auto base=cohortFixture();base.insert(base.size()-1,",\"firmware_routes\":"+routes);return base;}
int main(int argc,char **argv) {
 assert(argc==2);unsigned scenario=unsigned(atoi(argv[1]));assert(scenario<36);
 const auto row=firmware_row();auto from=route_source();auto entry=route(from,row);
 catalog_json=feed("["+route(route_source(std::string(40,'d')),row)+","+entry+"]");
 bool error=false,empty=false,refused=false;
 if(scenario==0)catalog_json=cohortFixture();
 if(scenario==2){catalog_json=feed("["+route(route_source(std::string(40,'d')),row)+"]");empty=true;}
 if(scenario==3){catalog_json=feed("["+entry+","+entry+"]");error=true;}
 if(scenario==4){cohortReplace(from,"\"source_revision\":\""+std::string(40,'e')+"\",","");error=true;}
 if(scenario==5){from=route_source("*");error=true;}
 if(scenario==6){cohortReplace(from,std::string(WatchUpdate::Product),"other-product");error=true;}
 if(scenario==7){cohortReplace(from,std::string(WatchUpdate::Repository),"other/repo");error=true;}
 if(scenario==8){auto bad=row;cohortReplace(bad,std::string(64,'a'),"bad-sha");catalog_json=feed("["+entry+","+route(route_source(std::string(40,'d')),bad)+"]");error=true;}
 if(scenario==9){cohortReplace(from,"0.1.33","0.1.34");error=true;}
 if(scenario==10){cohortReplace(from,"riscrte-paired-16m-v1","other-layout");error=true;}
 if(scenario==11){cohortReplace(from,"\"store_abi\":1","\"store_abi\":2");error=true;}
 if((scenario>=4&&scenario<=7)||(scenario>=9&&scenario<=11))catalog_json=feed("["+route(from,row)+"]");
 if(scenario==15){static const std::string long_product(64,'z');product=long_product.c_str();empty=true;}
 if(scenario==16){status_failure=true;error=true;}
 if(scenario==17){cohort_failure=true;error=true;}
 if(scenario==18){bank_api.struct_size=RISC_BANK_STORE_V1_PREFIX_SIZE;error=true;}
 if(scenario==19){bank_api.cohort_status=[](void*,risc_bank_cohort_status_v1* out)->int32_t{memset(out->product,'z',sizeof(out->product));return RISC_BANK_OK;};error=true;}
 if(scenario==20||scenario==21){std::string many="["+entry;for(unsigned i=1;i<(scenario==20?16u:17u);++i)many+=","+route(route_source(std::string(40,char('0'+i%10))),row);catalog_json=feed(many+"]");error=scenario==21;}
 if(scenario==22){catalog_json=feed("{}");error=true;}
 if(scenario==23){catalog_json=feed("[]");empty=true;}
 if(scenario==24){cohortReplace(from,"source_revision","source_revisoin");catalog_json=feed("["+route(from,row)+"]");error=true;}
 if(scenario==25){from.insert(1,"\"version\":\"1.1.0\",");catalog_json=feed("["+route(from,row)+"]");error=true;}
 if(scenario==26){auto usb=row;WatchUpdate::WorkBudget b(nullptr,nullptr);WatchUpdate::Slice ota;assert(WatchUpdate::field({usb.data(),usb.size()},"ota",ota,b));auto at=usb.find(",\"ota\":");assert(at!=std::string::npos);usb.erase(at);usb+='}';catalog_json=feed("["+route(from,usb)+"]");error=true;}
 if(scenario==27){close_failure=true;}
 if(scenario==29){runtime="0.1.32";empty=true;}
 if(scenario==31){product_version="1.01.0";error=true;}
 if(scenario==33){cohortReplace(from,"active_store_sha256","active_store_sha25x");catalog_json=feed("["+route(from,row)+"]");error=true;}
 if(scenario==34){active_digest=0;error=true;}
 if(scenario==35){active_digest=57;empty=true;}
 void* table=malloc(bank_api.struct_size);assert(table);memcpy(table,&bank_api,bank_api.struct_size);
 const risc_provider_dependency_v1 deps[]={{RISC_HTTP_CLIENT_CAPABILITY,1,&http_api},{RISC_BANK_STORE_CAPABILITY,1,table},{"platform.clock",1,&time_api}};
 const auto *driver=t5_driver_get(2);assert(driver&&driver->start(deps,3));
 if(!WatchUpdate::CatalogUrl[0]){assert(refresh(nullptr,0));assert(view.state==SOFTWARE_UPDATE_LIST&&!catalog->count&&!closes&&!cohort_reads&&!began);assert(driver->quiesce());free(table);return 0;}
 assert(refresh(nullptr,1800000000ULL));runUntil(SOFTWARE_UPDATE_LIST);
 if(scenario==27){assert(view.state==SOFTWARE_UPDATE_RETAINED&&!cohort_reads&&!began);assert(!driver->quiesce());close_failure=false;assert(cancel(nullptr));}
 else if(error){assert(view.state==SOFTWARE_UPDATE_ERROR&&!catalog->count&&!began&&!received&&!source_routed);}
 else if(empty){assert(view.state==SOFTWARE_UPDATE_LIST&&!catalog->count&&!began&&!received&&!source_routed);}
 else {
  assert(view.state==SOFTWARE_UPDATE_LIST&&catalog->count==1&&catalog->rows[0].view.availability==SOFTWARE_UPDATE_AVAILABLE);
  assert(source_routed==(scenario!=0));assert(closes==1);
  if(scenario==12||scenario==28){runtime=scenario==12?"0.1.34":"0.1.32";refused=true;}
  if(scenario==13){invalid_revision=true;refused=true;}
  if(scenario==14){product_version="1.0.99";refused=true;}
  if(scenario==32){active_digest=57;refused=true;}
  if(scenario==30){catalog_json=cohortFixture();assert(refresh(nullptr,1800000000ULL));runUntil(SOFTWARE_UPDATE_LIST);assert(!source_routed);runtime="0.1.32";}
  assert(begin(nullptr,0,1800000000ULL)==!refused);
  assert(began==unsigned(!refused)&&!received);
  if(!refused)assert(cancel(nullptr)&&aborts==1);
 }
 assert(driver->quiesce());driver->stop();free(table);
 printf("Source route scenario %u passed\n",scenario);
}
