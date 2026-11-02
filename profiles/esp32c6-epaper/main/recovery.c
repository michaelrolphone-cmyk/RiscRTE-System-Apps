/* Alternating, independently verifiable snapshots. Never infer a successful
 * external mutation from an interrupted local commit. NVS owns atomic keys;
 * this envelope independently detects incomplete/corrupt snapshot content. */
#include "recovery.h"
#include <string.h>
static void put32(uint8_t *p,uint32_t n){for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(n>>(8*i));}
static uint32_t get32(const uint8_t *p){uint32_t n=0;for(unsigned i=0;i<4;i++)n|=(uint32_t)p[i]<<(8*i);return n;}
static uint32_t crc(uint32_t value,const uint8_t *p,size_t n){
 while(n--){value^=*p++;for(unsigned bit=0;bit<8;bit++)value=(value>>1)^(0xedb88320u&(0u-(value&1u)));}return value;
}
static uint32_t checksum(const uint8_t *p,size_t n){return crc(crc(UINT32_MAX,p,16),p+RECOVERY_HEADER_BYTES,n)^UINT32_MAX;}
void recovery_pack(uint8_t *out,const char *payload,size_t length,uint32_t sequence){
 memset(out,0,RECOVERY_HEADER_BYTES);put32(out,0x42494f4au);put32(out+4,1);put32(out+8,sequence);put32(out+12,(uint32_t)length);
 memcpy(out+RECOVERY_HEADER_BYTES,payload,length);put32(out+16,checksum(out,length));
}
bool recovery_valid(const uint8_t *data,size_t size,uint32_t *sequence,size_t *length){
 if(!data||size<RECOVERY_HEADER_BYTES||get32(data)!=0x42494f4au||get32(data+4)!=1||get32(data+20)!=0)return false;
 size_t n=get32(data+12);
 if(!n||n>RECOVERY_MAX_BYTES||n!=size-RECOVERY_HEADER_BYTES||data[size-1]!=0||get32(data+16)!=checksum(data,n))return false;
 *sequence=get32(data+8);*length=n;return true;
}
bool recovery_newer(uint32_t a,uint32_t b){return a!=b&&(uint32_t)(a-b)<0x80000000u;}
