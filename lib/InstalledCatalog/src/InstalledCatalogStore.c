#include "InstalledCatalogStore.h"
static const char name[]="installed.idx";
static bool valid(const risc_app_data_v1*a){return a&&a->api_version==1&&a->struct_size>=sizeof(*a)&&a->stat&&a->read&&a->replace;}
int32_t installed_catalog_load(installed_catalog*c,const char*key,const risc_app_data_v1*a,
 void*scratch,uint32_t capacity,uint64_t*revision){
 if(!c||!key||!valid(a)||!scratch||!capacity||!revision)return RISC_APP_DATA_INVALID;
 *revision=0;uint32_t size=0;uint64_t token=0;
 int32_t status=a->stat(a->context,name,&size,&token);
 if(status!=RISC_APP_DATA_OK)return status;
 *revision=token; /* Preserve existing-file authority even if cache bytes are unusable. */
 if(!size||size>INSTALLED_CATALOG_CACHE_MAX||!token)return RISC_APP_DATA_INVALID;
 if(size>capacity)return RISC_APP_DATA_BUFFER_SMALL;
 uint32_t actual=0;uint64_t read_token=0;
 status=a->read(a->context,name,token,scratch,capacity,&actual,&read_token);
 if(status!=RISC_APP_DATA_OK)return status;
 if(!read_token)return RISC_APP_DATA_INVALID;
 *revision=read_token;
 if(actual!=size||!installed_catalog_decode(c,key,scratch,actual))return RISC_APP_DATA_INVALID;
 return RISC_APP_DATA_OK;
}
int32_t installed_catalog_save(const installed_catalog*c,const char*key,const risc_app_data_v1*a,
 void*scratch,uint32_t capacity,uint64_t expected_revision){
 if(!c||!key||!valid(a)||!scratch||!capacity)return RISC_APP_DATA_INVALID;
 size_t size=installed_catalog_encode(c,key,scratch,capacity);
 if(!size||size>INSTALLED_CATALOG_CACHE_MAX)return RISC_APP_DATA_INVALID;
 return a->replace(a->context,name,expected_revision,scratch,(uint32_t)size);
}
