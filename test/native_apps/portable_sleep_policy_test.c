#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "PortableSleepPolicy.h"
static uint8_t bytes[64];static uint32_t length;static unsigned reads,writes;static int32_t read_error,write_error;static bool wrong_readback;
static int32_t get(void*c,const char*k,void*b,uint32_t cap,uint32_t*out){(void)c;assert(!strcmp(k,PORTABLE_SLEEP_KEY));reads++;*out=0;
 if(read_error)return read_error;
 if(!length)return RISC_KEY_VALUE_NOT_FOUND;
 if(cap<length){*out=length;return RISC_KEY_VALUE_BUFFER_SMALL;}
 memcpy(b,bytes,length);*out=length;return 0;
}
static int32_t put(void*c,const char*k,const void*b,uint32_t size){(void)c;assert(!strcmp(k,PORTABLE_SLEEP_KEY)&&size==4);writes++;
 if(write_error)return write_error;
 memcpy(bytes,b,size);length=size;if(wrong_readback)bytes[1]=2;return 0;
}
static risc_key_value_v1 api={1,sizeof(api),NULL,get,put};
int main(void){
 unsigned mode=99;assert(portable_sleep_load(NULL,&mode)==PORTABLE_SLEEP_UNAVAILABLE&&mode==0);
 assert(portable_sleep_load(&api,&mode)==PORTABLE_SLEEP_MISSING&&mode==0);
 api.api_version=2;assert(!portable_sleep_save(&api,1));api.api_version=1;
 api.struct_size=8;assert(!portable_sleep_save(&api,1));api.struct_size=sizeof(api);
 assert(!portable_sleep_save(&api,2)&&!writes);
 for(unsigned chosen=0;chosen<2;++chosen){
  assert(portable_sleep_save(&api,chosen));assert(portable_sleep_load(&api,&mode)==0&&mode==chosen);
  unsigned before=writes;assert(portable_sleep_save(&api,chosen)&&writes==before);
 }
 for(unsigned size=1;size<=64;++size)if(size!=4){length=size;memset(bytes,0,64);assert(portable_sleep_load(&api,&mode)==PORTABLE_SLEEP_INVALID&&mode==0);}
 length=4;
 for(unsigned field=0;field<4;++field){memcpy(bytes,(uint8_t[]){0x53,1,1,0xa4},4);bytes[field]^=0xff;assert(portable_sleep_load(&api,&mode)==PORTABLE_SLEEP_INVALID&&mode==0);}
 read_error=RISC_KEY_VALUE_IO;assert(portable_sleep_load(&api,&mode)==PORTABLE_SLEEP_UNAVAILABLE&&mode==0);read_error=0;
 write_error=RISC_KEY_VALUE_IO;assert(!portable_sleep_save(&api,1));write_error=0;
 wrong_readback=true;assert(!portable_sleep_save(&api,1));wrong_readback=false;
 assert(portable_sleep_save(&api,0));assert(portable_sleep_load(&api,&mode)==0&&mode==0);
 assert(reads&&writes);puts("Shared sleep policy: bounded record, version/corruption/defaults, no-op and failed/verified save passed");
}
