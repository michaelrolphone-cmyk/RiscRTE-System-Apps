#pragma once
#include "Core.h"
// The vendor's target-only SHA configuration defines NDEBUG. Do not let that
// header disable assertions in callers (including the host integration tests).
#pragma push_macro("NDEBUG")
#include "../../lib/RemoteFiles/vendor/tinydtls_sha2/sha2.h"
#pragma pop_macro("NDEBUG")
namespace Infra {
class CanonicalHash {
 dtls_sha256_ctx ctx;
 public:
 CanonicalHash(){dtls_sha256_init(&ctx);}
 size_t write(uint8_t b){dtls_sha256_update(&ctx,&b,1);return 1;}
 size_t write(const uint8_t* p,size_t n){dtls_sha256_update(&ctx,p,n);return n;}
 void value(JsonVariantConst v){
  if(v.is<JsonObjectConst>()){
   const char* names[64];size_t n=0;for(JsonPairConst p:v.as<JsonObjectConst>())names[n++]=p.key().c_str();
   for(size_t i=1;i<n;++i){auto name=names[i];size_t j=i;while(j&&strcmp(names[j-1],name)>0){names[j]=names[j-1];--j;}names[j]=name;}
   write('{');for(size_t i=0;i<n;++i){if(i)write(',');write('"');write(reinterpret_cast<const uint8_t*>(names[i]),strlen(names[i]));write('"');write(':');value(v[names[i]]);}write('}');
  }else if(v.is<JsonArrayConst>()){write('[');bool comma=false;for(auto item:v.as<JsonArrayConst>()){if(comma)write(',');comma=true;value(item);}write(']');}
  else serializeJson(v,*this);
 }
 void finish(char out[65]){dtls_sha256_end(&ctx,out);}
};
inline void canonicalDigest(JsonVariantConst v,char out[65]){CanonicalHash hash;hash.value(v);hash.finish(out);}
inline void semanticDigest(JsonDocument& request,char out[65]){
 // Contract order is lexical: arguments, expected_revision, family, operation.
 CanonicalHash h;h.write('{');const char* keys[]={"arguments","expected_revision","family","operation"};bool comma=false;
 for(const char* k:keys){if(request[k].isNull())continue;if(comma)h.write(',');comma=true;h.write('"');h.write(reinterpret_cast<const uint8_t*>(k),strlen(k));h.write('"');h.write(':');h.value(request[k]);}h.write('}');h.finish(out);
}
}
