#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <ArduinoJson.h>
#include <RiscAppDataV1.h>
#include "JsonGuard.h"
namespace Infra {
// The backend validates buffer capacity as well as the committed file length.
// Keep the complete framed journal within the native storage contract.
constexpr size_t StateMax=RISC_APP_DATA_FILE_MAX,PolicyMax=16384,RecordMax=4096,PendingMax=8,BindingsMax=8;
inline uint32_t crc32(const void* data,size_t n){uint32_t c=~0u;auto p=static_cast<const uint8_t*>(data);while(n--){c^=*p++;for(unsigned j=0;j<8;++j)c=(c>>1)^(0xedb88320u&(0u-(c&1)));}return ~c;}
inline void put32(uint8_t* p,uint32_t n){for(unsigned i=0;i<4;++i)p[i]=n>>(i*8);}
inline uint32_t get32(const uint8_t* p){uint32_t n=0;for(unsigned i=0;i<4;++i)n|=uint32_t(p[i])<<(i*8);return n;}
inline void put64(uint8_t* p,uint64_t n){put32(p,uint32_t(n));put32(p+4,uint32_t(n>>32));}
inline uint64_t get64(const uint8_t* p){return get32(p)|(uint64_t(get32(p+4))<<32);}
// Bounded ArduinoJson allocator: allocation failure is visible, never fallback.
struct Allocator:ArduinoJson::Allocator{
 size_t used=0,peak=0,limit; explicit Allocator(size_t n):limit(n){}
 void* allocate(size_t n)override{if(n>limit-used)return nullptr;auto p=static_cast<size_t*>(std::malloc(n+sizeof(size_t)));if(!p)return nullptr;*p=n;used+=n;if(used>peak)peak=used;return p+1;}
 void deallocate(void* p)override{if(p){auto q=static_cast<size_t*>(p)-1;used-=*q;std::free(q);}}
 void* reallocate(void* p,size_t n)override{if(!p)return allocate(n);auto q=static_cast<size_t*>(p)-1;const auto old=*q;if(n>old&&n-old>limit-used)return nullptr;auto v=static_cast<size_t*>(std::realloc(q,n+sizeof(size_t)));if(!v)return nullptr;used=used-old+n;if(used>peak)peak=used;*v=n;return v+1;}
};
inline bool valueBounds(JsonVariantConst v,unsigned depth=0){
 if(depth>16)return false;
 if(v.is<JsonObjectConst>()){if(v.size()>64)return false;for(JsonPairConst p:v.as<JsonObjectConst>())if(!valueBounds(p.value(),depth+1))return false;}
 else if(v.is<JsonArrayConst>()){if(v.size()>128)return false;for(auto e:v.as<JsonArrayConst>())if(!valueBounds(e,depth+1))return false;}
 else if(v.is<const char*>()){if(std::strlen(v.as<const char*>())>4096)return false;}
 else if(v.is<double>()&&!v.is<int64_t>()&&!v.is<uint64_t>())return false;
 else if(v.is<int64_t>()&&(v.as<int64_t>()>9007199254740991LL||v.as<int64_t>()< -9007199254740991LL))return false;
 return true;
}
inline bool parse(JsonDocument& doc,const char* data,size_t size){
 doc.clear();if(!size||size>StateMax||!JsonGuard(data,size).objectOnly())return false;
 return !deserializeJson(doc,data,size,DeserializationOption::NestingLimit(16))&&!doc.overflowed()&&valueBounds(doc.as<JsonVariantConst>());
}
inline bool keys(JsonObjectConst o,std::initializer_list<const char*> required,std::initializer_list<const char*> optional={}){
 if(o.isNull())return false;for(auto k:required)if(!o.containsKey(k))return false;
 for(JsonPairConst p:o){bool found=false;for(auto k:required)found|=!std::strcmp(k,p.key().c_str());for(auto k:optional)found|=!std::strcmp(k,p.key().c_str());if(!found)return false;}return true;
}
inline bool copy(char* dest,size_t cap,const char* src){if(!src||std::strlen(src)>=cap)return false;std::memcpy(dest,src,std::strlen(src)+1);return true;}
// JSON snapshot frame is endian-defined, never persists C struct padding or opaque revisions.
inline size_t frame(uint8_t* out,size_t cap,const char* payload,size_t n,uint64_t seq){
 if(!seq||n>StateMax-32||cap<n+32)return 0;std::memset(out,0,32);std::memcpy(out,"INFRAS31",8);put32(out+8,1);put32(out+12,n);put64(out+16,seq);std::memcpy(out+32,payload,n);put32(out+24,crc32(out+32,n));put32(out+28,crc32(out,28));return n+32;
}
inline bool unframe(const uint8_t* data,size_t n,const char*& payload,size_t& size,uint64_t& seq){
 if(n<32||n>StateMax||std::memcmp(data,"INFRAS31",8)||get32(data+8)!=1||get32(data+12)!=n-32||get32(data+28)!=crc32(data,28)||get32(data+24)!=crc32(data+32,n-32))return false;
 seq=get64(data+16);payload=reinterpret_cast<const char*>(data+32);size=n-32;return seq!=0;
}
struct Cadence{
 uint64_t due=0,lastStarted=0,lastCompleted=0,maxLag=0;uint32_t interval=15000,timeout=5000,coalesced=0;bool required=true;
 bool ready(uint64_t now)const{return now>=due;}
 void start(uint64_t now){if(now>due){const auto lag=now-due;if(lag>maxLag)maxLag=lag;if(interval&&lag/interval>0)coalesced+=static_cast<uint32_t>(lag/interval);}lastStarted=now;due=now+interval;}
};
inline bool schedulable(const Cadence* c,size_t n,uint32_t controlMs=1000,uint32_t contactMs=30000){
 if(n>8||!contactMs)return false;uint64_t utilization=uint64_t(controlMs)*1000000/contactMs;
 for(size_t i=0;i<n;++i){if(c[i].interval<1000||!c[i].timeout||c[i].timeout>30000||c[i].timeout>=c[i].interval)return false;if(c[i].required)utilization+=uint64_t(c[i].timeout)*1000000/c[i].interval;}
 return utilization<=900000; // retain at least 10% for scheduler/cleanup headroom
}
inline uint64_t leaseDeadline(uint64_t now,uint32_t ttl,uint64_t requestElapsed,uint32_t margin=1000){return requestElapsed+margin>=ttl?now:now+(ttl-requestElapsed-margin);}
inline uint32_t retryDelay(unsigned failures,uint32_t random){const uint32_t ceiling=failures>=9?300000:1000u<<failures;return random%(ceiling+1);}
}
