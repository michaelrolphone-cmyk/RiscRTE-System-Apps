#include "PortableRtcBasis.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint8_t bytes[12];static unsigned gets,puts_count;static uint32_t stored_size;
static int get_status=RISC_KEY_VALUE_NOT_FOUND,put_status=RISC_KEY_VALUE_OK;
static bool fail_readback,commit_on_error,wrong_readback;
static int32_t get(void*c,const char*k,void*out,uint32_t capacity,uint32_t*size){
 (void)c;assert(!strcmp(k,"rtc_basis")&&capacity==12);++gets;*size=stored_size;
 if(get_status!=RISC_KEY_VALUE_OK)return get_status;
 if(fail_readback&&puts_count)return RISC_KEY_VALUE_IO;
 if(stored_size<=capacity)memcpy(out,bytes,stored_size);
 if(wrong_readback&&puts_count){uint8_t*p=out;p[3]^=1;portable_rtc_basis_put_word(p+8,portable_rtc_basis_check(p));}
 return RISC_KEY_VALUE_OK;
}
static int32_t put(void*c,const char*k,const void*data,uint32_t size){
 (void)c;assert(!strcmp(k,"rtc_basis")&&size==12);++puts_count;
 if(put_status==RISC_KEY_VALUE_OK||commit_on_error){memcpy(bytes,data,size);stored_size=size;get_status=RISC_KEY_VALUE_OK;}
 return put_status;
}
static risc_key_value_v1 kv={1,sizeof(kv),NULL,get,put};
int main(void){
 portable_rtc_basis out={true,123},desired={true,1704067200u};
 assert(portable_rtc_basis_load(&kv,&out)==PORTABLE_RTC_BASIS_MISSING&&!out.stores_utc&&!out.reference_epoch&&!puts_count);
 assert(portable_rtc_basis_load(NULL,&out)==PORTABLE_RTC_BASIS_UNAVAILABLE);
 assert(portable_rtc_basis_load(&kv,NULL)==PORTABLE_RTC_BASIS_INVALID);
 for(unsigned mode=0;mode<2;++mode)for(unsigned r=0;r<3;++r){
  desired=(portable_rtc_basis){mode!=0,r==0?0:r==1?PORTABLE_RTC_BASIS_MIN_REFERENCE:PORTABLE_RTC_BASIS_MAX_REFERENCE};
  assert(portable_rtc_basis_save(&kv,&desired)==PORTABLE_RTC_BASIS_SAVED);
  unsigned count=puts_count;
  assert(portable_rtc_basis_save(&kv,&desired)==PORTABLE_RTC_BASIS_UNCHANGED&&puts_count==count);
  assert(portable_rtc_basis_load(&kv,&out)==PORTABLE_RTC_BASIS_LOADED&&out.stores_utc==desired.stores_utc&&out.reference_epoch==desired.reference_epoch);
 }
 uint8_t saved[12];memcpy(saved,bytes,12);unsigned count=puts_count;
 for(unsigned bit=0;bit<96;++bit){memcpy(bytes,saved,12);bytes[bit/8]^=(uint8_t)(1u<<(bit%8));assert(portable_rtc_basis_load(&kv,&out)==PORTABLE_RTC_BASIS_INVALID);assert(!out.stores_utc&&!out.reference_epoch&&puts_count==count);}
 memcpy(bytes,saved,12);
 for(unsigned size=0;size<=13;++size){if(size==12)continue;stored_size=size;assert(portable_rtc_basis_load(&kv,&out)==PORTABLE_RTC_BASIS_INVALID&&puts_count==count);}
 stored_size=12;get_status=RISC_KEY_VALUE_IO;assert(portable_rtc_basis_load(&kv,&out)==PORTABLE_RTC_BASIS_UNAVAILABLE);
 get_status=RISC_KEY_VALUE_BUFFER_SMALL;assert(portable_rtc_basis_load(&kv,&out)==PORTABLE_RTC_BASIS_INVALID);
 get_status=RISC_KEY_VALUE_OK;
 desired.reference_epoch=PORTABLE_RTC_BASIS_MIN_REFERENCE-1;assert(portable_rtc_basis_save(&kv,&desired)==PORTABLE_RTC_BASIS_BAD_VALUE);
 desired.reference_epoch=PORTABLE_RTC_BASIS_MAX_REFERENCE+1;assert(portable_rtc_basis_save(&kv,&desired)==PORTABLE_RTC_BASIS_BAD_VALUE);
 assert(portable_rtc_basis_save(&kv,NULL)==PORTABLE_RTC_BASIS_BAD_VALUE);
 desired=(portable_rtc_basis){false,1704067200u};assert(portable_rtc_basis_save(NULL,&desired)==PORTABLE_RTC_BASIS_NO_STORAGE);
 put_status=RISC_KEY_VALUE_IO;assert(portable_rtc_basis_save(&kv,&desired)==PORTABLE_RTC_BASIS_WRITE_FAILED);
 commit_on_error=true;assert(portable_rtc_basis_save(&kv,&desired)==PORTABLE_RTC_BASIS_WRITE_FAILED);
 assert(portable_rtc_basis_load(&kv,&out)==PORTABLE_RTC_BASIS_LOADED&&out.reference_epoch==desired.reference_epoch);
 desired.stores_utc=true;put_status=RISC_KEY_VALUE_OK;fail_readback=true;
 assert(portable_rtc_basis_save(&kv,&desired)==PORTABLE_RTC_BASIS_VERIFY_FAILED);fail_readback=false;
 desired.stores_utc=false;wrong_readback=true;get_status=RISC_KEY_VALUE_NOT_FOUND;
 assert(portable_rtc_basis_save(&kv,&desired)==PORTABLE_RTC_BASIS_VERIFY_FAILED);wrong_readback=false;
 memset(bytes,0,12);get_status=RISC_KEY_VALUE_OK;
 assert(portable_rtc_basis_save(&kv,&desired)==PORTABLE_RTC_BASIS_SAVED);
 risc_key_value_v1 bad=kv;bad.api_version=2;assert(portable_rtc_basis_load(&bad,&out)==PORTABLE_RTC_BASIS_UNAVAILABLE);
 bad=kv;bad.struct_size=8;assert(portable_rtc_basis_save(&bad,&desired)==PORTABLE_RTC_BASIS_NO_STORAGE);
 bad=kv;bad.get=NULL;assert(portable_rtc_basis_load(&bad,&out)==PORTABLE_RTC_BASIS_UNAVAILABLE);
 bad=kv;bad.put=NULL;assert(portable_rtc_basis_load(&bad,&out)==PORTABLE_RTC_BASIS_UNAVAILABLE);
 puts("RTC interpretation records: round trips, every-bit corruption, bounds, readonly fallback and uncertain persistence PASS");return 0;
}
