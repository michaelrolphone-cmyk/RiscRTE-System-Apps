#include <cassert>
#define UPDATE_FIRMWARE 0
#define UPDATE_PRODUCT_X4
#include "../../Services/update/service.cpp"
namespace {
const char *installed_product=WatchUpdate::Product,*installed_repo=WatchUpdate::Repository;
bool valid_revision=true;
unsigned writes;
bool bankStatus(void*,risc_bank_status_v1 *out){out->store_abi=2;return true;}
int32_t cohortStatus(void*,risc_bank_cohort_status_v1 *out){
 strcpy(out->product,installed_product);strcpy(out->source_repo,installed_repo);strcpy(out->version,"0.1.16");
 memset(out->source_revision,valid_revision?'a':'X',40);out->source_revision[40]=0;return RISC_BANK_OK;
}
int32_t cohortBegin(void*,const risc_bank_cohort_v1*,uint64_t*){++writes;return RISC_BANK_INVALID;}
}
int main(){
 risc_bank_store_v1 mock{};mock.struct_size=sizeof(mock);mock.status=bankStatus;mock.cohort_status=cohortStatus;mock.begin_cohort=cohortBegin;
 bank=&mock;catalog=&catalog_storage;started=true;
 assert(installedProductMatches()&&classify());
 for(unsigned fault=0;fault<4;++fault){
  installed_product=WatchUpdate::Product;installed_repo=WatchUpdate::Repository;valid_revision=true;mock.struct_size=sizeof(mock);
  if(fault==0)installed_product="twatch-s3";
  if(fault==1)installed_repo="owner/watch";
  if(fault==2)valid_revision=false;
  if(fault==3)mock.struct_size=RISC_BANK_STORE_V1_PREFIX_SIZE;
  assert(!installedProductMatches()&&!classify());
  catalog->count=1;catalog->rows[0].view.availability=SOFTWARE_UPDATE_AVAILABLE;
  assert(!begin(nullptr,0,1800000000ULL)&&!writes&&!bank_handle&&!http_handle);
  catalog->count=0;
 }
 puts("X4 App Store rejects foreign/malformed installed product at classification and begin");
}
