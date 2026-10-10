#include <cassert>
#include <string>
#define UPDATE_FIRMWARE 1
#define UPDATE_PRODUCT_X4
#include "../../Services/update/service.cpp"
#include "fixtures/paired_cohort_fixture.h"
int main(){
 WatchUpdate::Catalog data;auto good=cohortFixture(2,513,0x510000);
 auto parse=[&](const std::string& text){return WatchUpdate::parse({text.data(),text.size()},true,data,nullptr,nullptr);};
 assert(parse(good)&&data.count==1&&data.rows[0].pairedCohort);
 for(auto mutation:{std::pair<std::string,std::string>{"xteink-x4-pro","twatch-s3"},
   {"RiskRTE-XTEINK-X4-PRO","RiscRTE-T-Watch-S3"},
   {"paired-cohort","runtime-image"},{"xteink-x4-pro-cohort-","xteink-x4-pro-launcher-"},
   {"firmware-v1.2.0","firmware-v1.2.1"},{"\"store_size\":5308416","\"store_size\":16777216"}}){
   auto bad=good;cohortReplace(bad,mutation.first,mutation.second);assert(!parse(bad)&&!data.count);
 }
 auto usb=good;auto at=usb.find(",\"ota\":");usb.erase(at,usb.find("},\"apps\":",at)-at);assert(parse(usb));
 assert(data.rows[0].view.availability==SOFTWARE_UPDATE_USB_ONLY&&!data.rows[0].url[0]);
 assert(!WatchUpdate::CatalogUrl[0]);
 // Disabled feed never requests transport, native time, bank reads or a write.
 catalog=&catalog_storage;started=true;assert(refresh(nullptr,0));
 assert(view.state==SOFTWARE_UPDATE_LIST&&!catalog->count&&!http_handle&&!bank_handle);
 assert(!begin(nullptr,0,1800000000));assert(cancel(nullptr));assert(quiesce());
 puts("X4 policy: source/product/asset separation, USB rejection and offline empty feed passed");
}
