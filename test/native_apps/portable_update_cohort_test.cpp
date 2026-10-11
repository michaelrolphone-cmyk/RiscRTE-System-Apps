#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <string>
#define UPDATE_FIRMWARE 1
#include "../../Services/update/service.cpp"
#include "fixtures/paired_cohort_fixture.h"
namespace {
uint64_t now;
uint32_t abi=1,firmware_capacity=0x300000,store_capacity=0x4f0000,bank_state=RISC_BANK_IDLE;
uint32_t payload_size=513+store_capacity,payload_firmware=513,received,offset;
unsigned began,aborts,closes,activations,restarts,cohort_reads;
uint8_t active_digest=42;
const char *layout="riscrte-paired-16m-v1",*runtime="0.1.33",*product=WatchUpdate::Product,*product_version="1.1.0",*source=WatchUpdate::Repository;
bool payload,read_failure,short_download,write_failure,finish_failure,abort_failure,close_failure,unknown_activation,restart_failure,status_failure,cohort_failure,invalid_revision;
std::string catalog_json;
int32_t hOpen(void*,const risc_http_request_v1 *r,uint64_t *handle) {
 assert(!http_handle);assert(r->utc_seconds==1800000000ULL);offset=0;
 payload=strcmp(r->url,WatchUpdate::CatalogUrl)!=0;
 if(payload){assert(std::string(r->url)==std::string("https://github.com/")+WatchUpdate::Repository+"/releases/download/firmware-v1.2.0/"+WatchUpdate::AssetPrefix+"-cohort-1.2.0.bin");assert(r->max_bytes==payload_size);}
 *handle=1;return RISC_HTTP_OK;
}
int32_t hRead(void*,uint64_t,void *out,uint32_t cap,uint32_t *size) {
 assert(cap<=RISC_HTTP_CHUNK_MAX);*size=0;
 if(payload&&read_failure)return RISC_HTTP_NETWORK;
 uint32_t total=payload?payload_size-(short_download?1:0):(uint32_t)catalog_json.size();
 if(offset==total)return RISC_HTTP_EOF;
 *size=total-offset;if(*size>cap)*size=cap;
 if(payload){auto *bytes=(uint8_t*)out;for(uint32_t i=0;i<*size;++i)bytes[i]=offset+i<payload_firmware?'F':'S';}
 else memcpy(out,catalog_json.data()+offset,*size);
 offset+=*size;return RISC_HTTP_OK;
}
int32_t hInfo(void*,uint64_t,risc_http_response_v1 *out){out->status_code=200;return 0;}
int32_t hClose(void*,uint64_t){if(close_failure)return RISC_HTTP_RETAINED;++closes;return 0;}
bool bStatus(void*,risc_bank_status_v1 *out){if(status_failure)return false;out->state=bank_state;out->firmware_capacity=firmware_capacity;out->store_capacity=store_capacity;out->store_abi=abi;strcpy(out->layout,layout);strcpy(out->runtime_version,runtime);memset(out->active_store_sha256,active_digest,32);return true;}
int32_t bCohortStatus(void*,risc_bank_cohort_status_v1 *out){++cohort_reads;if(cohort_failure)return RISC_BANK_UNAVAILABLE;assert(out->struct_size==sizeof(*out));strcpy(out->product,product);strcpy(out->version,product_version);strcpy(out->source_repo,source);memset(out->source_revision,invalid_revision?'X':'e',40);out->source_revision[40]=0;return 0;}
int32_t bCohort(void*,const risc_bank_cohort_v1 *target,uint64_t *handle){
 assert(target->struct_size==sizeof(*target)&&target->store_abi==abi&&target->firmware_size==payload_firmware&&target->store_size==store_capacity);
 assert(!strcmp(target->product,WatchUpdate::Product)&&!strcmp(target->version,"1.2.0")&&!strcmp(target->runtime_version,"0.1.33")&&!strcmp(target->source_repo,WatchUpdate::Repository));
 assert(std::string(target->source_revision)==std::string(40,'c'));
 for(unsigned i=0;i<32;++i)assert(target->sha256[i]==0xaa&&target->firmware_sha256[i]==0xbb&&target->store_sha256[i]==0xdd&&target->active_store_sha256[i]==active_digest);
 ++began;*handle=2;bank_state=RISC_BANK_COPY_STORE;return 0;
}
int32_t noFirmware(void*,const risc_bank_image_v1*,uint64_t*){assert(false);return RISC_BANK_INVALID;}
int32_t noApp(void*,const char*,const void*,uint32_t,const risc_bank_image_v1*,uint64_t*){assert(false);return RISC_BANK_INVALID;}
int32_t noGet(void*,uint32_t,void*,uint32_t,uint32_t*){assert(false);return RISC_BANK_INVALID;}
int32_t bStep(void*,uint64_t,risc_bank_status_v1 *out){if(bank_state==RISC_BANK_COPY_STORE)bank_state=RISC_BANK_RECEIVING;else if(bank_state==RISC_BANK_VERIFY_FIRMWARE)bank_state=RISC_BANK_VERIFY_STORE;else if(bank_state==RISC_BANK_VERIFY_STORE)bank_state=RISC_BANK_READY;out->state=bank_state;return 0;}
int32_t bWrite(void*,uint64_t,const void *data,uint32_t size){if(write_failure)return RISC_BANK_IO;auto *bytes=(const uint8_t*)data;for(uint32_t i=0;i<size;++i)assert(bytes[i]==(received+i<payload_firmware?'F':'S'));received+=size;return 0;}
int32_t bFinish(void*,uint64_t){assert(received==payload_size);if(finish_failure)return RISC_BANK_INTEGRITY;bank_state=RISC_BANK_VERIFY_FIRMWARE;return 0;}
int32_t bActivate(void*,uint64_t){assert(!http_handle&&bank_state==RISC_BANK_READY);if(unknown_activation){bank_state=RISC_BANK_ACTIVATION_UNKNOWN;return RISC_BANK_RETAINED;}++activations;bank_state=RISC_BANK_ACTIVATED;return 0;}
int32_t bAbort(void*,uint64_t){if(abort_failure)return RISC_BANK_RETAINED;++aborts;bank_state=RISC_BANK_IDLE;return 0;}
bool bRestart(void*,uint64_t){if(restart_failure)return false;++restarts;return true;}
uint64_t timeMs(void*){return now;}
void sleepMs(void*,uint32_t amount){now+=amount;}
const risc_http_client_v1 http_api={1,sizeof(http_api),nullptr,hOpen,hRead,hInfo,hClose};
risc_bank_store_v1 bank_api={1,sizeof(bank_api),nullptr,bStatus,noFirmware,noApp,bStep,bWrite,bFinish,bActivate,bAbort,bRestart,noGet,bCohortStatus,bCohort};
const risc_platform_clock_api_v1 time_api={1,sizeof(time_api),nullptr,timeMs,sleepMs};
void runUntil(uint32_t state){for(unsigned i=0;i<40000&&view.state!=state&&view.state!=SOFTWARE_UPDATE_ERROR&&view.state!=SOFTWARE_UPDATE_RETAINED;++i)step(nullptr);}
}
int main(int argc,char **argv){
 assert(argc==2);unsigned scenario=(unsigned)atoi(argv[1]);assert(scenario<46);
 if(scenario==1||scenario==35){abi=2;layout="riscrte-paired-appdata-v2";firmware_capacity=0x260000;store_capacity=0x510000;}
 payload_size=payload_firmware+store_capacity;catalog_json=cohortFixture(abi,payload_firmware,store_capacity);
 if(scenario==2)runtime="0.1.32"; // Same Runtime is valid; newer Runtime is also valid.
 if(scenario==3)runtime="0.1.34";
 if(scenario==4)product_version="1.2.0";
 if(scenario==5)product_version="1.3.0";
 if(scenario==6)product="other-watch";
 if(scenario==7)source="other-owner/RiscRTE-T-Watch-S3";
 if(scenario==8)invalid_revision=true;
 if(scenario==9)cohort_failure=true;
 if(scenario==10)bank_api.cohort_status=nullptr;
 if(scenario==11)bank_api.begin_cohort=nullptr;
 if(scenario==12)bank_api.struct_size=offsetof(risc_bank_store_v1,begin_cohort);
 if(scenario==13)bank_api.struct_size=RISC_BANK_STORE_V1_PREFIX_SIZE;
 if(scenario==14)++store_capacity;
 if(scenario==15)firmware_capacity=3;
 if(scenario==16)layout="different-layout";
 if(scenario==17)abi=2;
 if(scenario==36)product_version="1.01.0";
 if(scenario==42)runtime="not-a-version";
 // Allocate exactly the advertised table bytes, catching any old-prefix suffix read.
 void *table=malloc(bank_api.struct_size);assert(table);memcpy(table,&bank_api,bank_api.struct_size);
 risc_provider_dependency_v1 deps[]={{RISC_HTTP_CLIENT_CAPABILITY,1,&http_api},{RISC_BANK_STORE_CAPABILITY,1,table},{"platform.clock",1,&time_api}};
 const auto *driver=t5_driver_get(2);assert(driver->start(deps,3));assert(refresh(nullptr,1800000000ULL));runUntil(SOFTWARE_UPDATE_LIST);
 assert(view.state==SOFTWARE_UPDATE_LIST&&catalog->count==1&&catalog->rows[0].pairedCohort);
 assert(!strcmp(catalog->rows[0].view.version,"1.2.0"));
 if((scenario>=3&&scenario<=17)||scenario==36||scenario==42){assert(catalog->rows[0].view.availability!=SOFTWARE_UPDATE_AVAILABLE);assert(!begin(nullptr,0,1800000000ULL));assert(!began&&!received);}
 else {
  assert(catalog->rows[0].view.availability==SOFTWARE_UPDATE_AVAILABLE&&!strcmp(catalog->rows[0].view.installed,"1.1.0"));
  if(scenario==18)product_version="1.2.0";
  if(scenario==19)runtime="0.1.34";
  if(scenario==20)source="other-owner/RiscRTE-T-Watch-S3";
  if(scenario==21)--store_capacity;
  if(scenario==22)cohort_failure=true;
  if(scenario==23)status_failure=true;
  if(scenario==24)active_digest=57;
  if(scenario==34)abi=2;
  if(scenario==35)layout="riscrte-paired-16m-v1";
  if(scenario==37)((risc_bank_store_v1*)table)->struct_size=RISC_BANK_STORE_V1_PREFIX_SIZE;
  if(scenario==38)((risc_bank_store_v1*)table)->cohort_status=nullptr;
  if(scenario==39)((risc_bank_store_v1*)table)->begin_cohort=nullptr;
  if(scenario==40)product="other-watch";
  if(scenario==41)invalid_revision=true;
  if(scenario==43)firmware_capacity=3;
  if((scenario>=18&&scenario<=23)||scenario==34||scenario==35||(scenario>=37&&scenario<=41)||scenario==43){assert(!begin(nullptr,0,1800000000ULL));assert(view.state==SOFTWARE_UPDATE_ERROR&&!began&&!received);}
  else {
   assert(begin(nullptr,0,1800000000ULL)&&began==1&&cohort_reads==2);assert(!activate(nullptr));
   if(scenario==25){assert(cancel(nullptr));assert(aborts==1&&!received);}
   else {
    runUntil(SOFTWARE_UPDATE_DOWNLOAD);assert(view.state==SOFTWARE_UPDATE_DOWNLOAD);
    if(scenario==26)read_failure=true;
    if(scenario==27)short_download=true;
    if(scenario==28)finish_failure=true;
    if(scenario==29)write_failure=true;
    if(scenario==44){assert(step(nullptr)&&received);assert(cancel(nullptr)&&aborts==1&&!activations);}
    else if(scenario==45){now=deadline;abort_failure=true;assert(!step(nullptr)&&view.state==SOFTWARE_UPDATE_RETAINED);assert(!driver->quiesce());abort_failure=false;assert(cancel(nullptr)&&aborts==1);}
    else if(scenario==30){close_failure=true;assert(!cancel(nullptr)&&view.state==SOFTWARE_UPDATE_RETAINED&&!aborts);close_failure=false;assert(cancel(nullptr)&&aborts==1);}
    else {
     runUntil(SOFTWARE_UPDATE_READY);
     if(scenario>=26&&scenario<=29){assert(view.state==SOFTWARE_UPDATE_ERROR&&aborts==1&&!activations);}
     else {
      assert(view.state==SOFTWARE_UPDATE_READY&&received==payload_size&&!activations&&closes==2);
      if(scenario==31){abort_failure=true;assert(!cancel(nullptr)&&view.state==SOFTWARE_UPDATE_RETAINED&&!driver->quiesce());abort_failure=false;assert(cancel(nullptr)&&aborts==1);}
      else {
       if(scenario==32)unknown_activation=true;
       assert(activate(nullptr)==!unknown_activation);assert(view.state==(unknown_activation?SOFTWARE_UPDATE_ACTIVATION_UNKNOWN:SOFTWARE_UPDATE_ACTIVATED));
       assert(!cancel(nullptr)&&!driver->quiesce()&&!aborts);
       if(scenario==33){restart_failure=true;assert(!restart(nullptr));restart_failure=false;}
       assert(restart(nullptr)&&restarts==1);view.state=SOFTWARE_UPDATE_READY; // Simulated reboot teardown only.
      }
     }
    }
   }
  }
 }
 assert(driver->quiesce());assert(!http_handle&&!bank_handle);free(table);
 printf("paired cohort service scenario %u passed\n",scenario);
}
