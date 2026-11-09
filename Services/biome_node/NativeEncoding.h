#pragma once
#include "../infra_node/Digest.h"
#include <cmath>

namespace BiomeRelay {
// Native BiomeRTE store.encode is JSON with sorted keys, no whitespace,
// ensure_ascii=True, and integral protocol values. This writer hashes the
// exact signed representation; its optional output is useful for test vectors.
class NativeCanonical {
 dtls_sha256_ctx hash;
 char* buffer;size_t capacity,count=0;bool good=true,allowFloating=false;
 void byte(uint8_t c){dtls_sha256_update(&hash,&c,1);if(buffer){if(count+1<capacity)buffer[count]=char(c);else good=false;}++count;}
 void raw(const char* s){while(*s)byte(uint8_t(*s++));}
 void escaped(uint16_t c){const char* hex="0123456789abcdef";raw("\\u");for(int shift=12;shift>=0;shift-=4)byte(hex[(c>>shift)&15]);}
 void string(const char* text){
  byte('"');auto p=reinterpret_cast<const uint8_t*>(text);
  while(*p&&good){
   uint32_t c=*p++;
   if(c=='"'||c=='\\'){byte('\\');byte(c);}
   else if(c=='\b')raw("\\b");else if(c=='\f')raw("\\f");else if(c=='\n')raw("\\n");
   else if(c=='\r')raw("\\r");else if(c=='\t')raw("\\t");
   else if(c<32||c==127)escaped(c);
   else if(c<128)byte(c);
   else{
    unsigned continuation=0;uint32_t minimum=0;
    if(c>=0xc2&&c<=0xdf){continuation=1;minimum=0x80;c&=31;}
    else if(c>=0xe0&&c<=0xef){continuation=2;minimum=0x800;c&=15;}
    else if(c>=0xf0&&c<=0xf4){continuation=3;minimum=0x10000;c&=7;}
    else {good=false;break;}
    for(unsigned i=0;i<continuation;++i){if((*p&0xc0)!=0x80){good=false;break;}c=(c<<6)|(*p++&63);}
    if(!good||c<minimum||c>0x10ffff||(c>=0xd800&&c<=0xdfff)){good=false;break;}
    if(c<=0xffff)escaped(c);else{c-=0x10000;escaped(0xd800+(c>>10));escaped(0xdc00+(c&1023));}
   }
  }
  byte('"');
 }
 public:
 explicit NativeCanonical(char* out=nullptr,size_t size=0,bool floating=false):buffer(out),capacity(size),allowFloating(floating){dtls_sha256_init(&hash);}
 size_t write(uint8_t c){byte(c);return 1;}
 size_t write(const uint8_t* p,size_t n){for(size_t i=0;i<n;++i)byte(p[i]);return n;}
 void value(JsonVariantConst v,unsigned depth=0){
  if(!good||depth>(allowFloating?24u:16u)){good=false;return;}
  if(v.is<JsonObjectConst>()){
   const size_t count=v.size();if(count>256){good=false;return;}
   auto names=static_cast<const char**>(malloc(count*sizeof(const char*)));if(count&&!names){good=false;return;}
   size_t n=0;for(JsonPairConst p:v.as<JsonObjectConst>())names[n++]=p.key().c_str();
   for(size_t i=1;i<n;++i){auto name=names[i];size_t j=i;while(j&&strcmp(names[j-1],name)>0){names[j]=names[j-1];--j;}names[j]=name;}
   byte('{');for(size_t i=0;i<n;++i){if(i)byte(',');string(names[i]);byte(':');value(v[names[i]],depth+1);}byte('}');free(names);
  }else if(v.is<JsonArrayConst>()){
   if(v.size()>(allowFloating?4096u:128u)){good=false;return;}byte('[');bool comma=false;
   for(auto entry:v.as<JsonArrayConst>()){if(comma)byte(',');comma=true;value(entry,depth+1);}byte(']');
  }else if(v.is<const char*>())string(v.as<const char*>());
  else if(v.isNull())raw("null");
  else if(v.is<bool>())raw(v.as<bool>()?"true":"false");
  else if(v.is<int64_t>()){char number[32];snprintf(number,sizeof(number),"%lld",static_cast<long long>(v.as<int64_t>()));raw(number);}
  else if(v.is<uint64_t>()){char number[32];snprintf(number,sizeof(number),"%llu",static_cast<unsigned long long>(v.as<uint64_t>()));raw(number);}
  else if(allowFloating&&v.is<double>()){
   const double number=v.as<double>();if(!std::isfinite(number)){good=false;return;}
   char encoded[64];snprintf(encoded,sizeof(encoded),"%.17g",number);raw(encoded);
   // Receipt identity distinguishes a fractional JSON type from an integer,
   // even when both carry the mathematical value one. Native signatures
   // still reject floating values; this branch is local receipt hashing only.
   if(!strchr(encoded,'.')&&!strchr(encoded,'e')&&!strchr(encoded,'E'))raw(".0");
  }
  else good=false;
 }
 bool finish(uint8_t out[32]){dtls_sha256_final(out,&hash);if(buffer&&capacity)buffer[count<capacity?count:capacity-1]=0;return good;}
};
inline bool nativeDigest(JsonVariantConst value,uint8_t out[32]){
 NativeCanonical canonical;canonical.value(value);return canonical.finish(out);
}
inline void bytesDigest(const uint8_t* data,size_t size,char out[65]){
 dtls_sha256_ctx hash;dtls_sha256_init(&hash);dtls_sha256_update(&hash,data,size);dtls_sha256_end(&hash,out);
}
inline size_t base64(const uint8_t* data,size_t size,char* out,size_t capacity){
 const size_t needed=4*((size+2)/3);if(capacity<=needed)return 0;
 const char* alphabet="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
 size_t written=0;for(size_t i=0;i<size;i+=3){
  uint32_t n=uint32_t(data[i])<<16;if(i+1<size)n|=uint32_t(data[i+1])<<8;if(i+2<size)n|=data[i+2];
  out[written++]=alphabet[(n>>18)&63];out[written++]=alphabet[(n>>12)&63];
  out[written++]=i+1<size?alphabet[(n>>6)&63]:'=';out[written++]=i+2<size?alphabet[n&63]:'=';
 }
 out[written]=0;return written;
}
inline size_t derLength(uint8_t* out,size_t value){
 if(value<128){out[0]=value;return 1;}if(value<256){out[0]=0x81;out[1]=value;return 2;}
 if(value<=65535){out[0]=0x82;out[1]=value>>8;out[2]=value;return 3;}return 0;
}
// PSA exports RSAPublicKey (PKCS#1). Native peer fingerprints and PUBLIC KEY
// PEM use SubjectPublicKeyInfo, including rsaEncryption's explicit NULL.
inline size_t rsaSpki(const uint8_t* publicKey,size_t size,uint8_t* out,size_t capacity){
 static constexpr uint8_t algorithm[]={0x30,0x0d,0x06,0x09,0x2a,0x86,0x48,0x86,0xf7,0x0d,0x01,0x01,0x01,0x05,0x00};
 if(!size||size>4096)return 0;uint8_t bitLength[3],sequenceLength[3];
 size_t b=derLength(bitLength,size+1),payload=sizeof(algorithm)+1+b+1+size;
 size_t s=derLength(sequenceLength,payload),total=1+s+payload;
 if(!b||!s||capacity<total)return 0;
 size_t n=0;out[n++]=0x30;memcpy(out+n,sequenceLength,s);n+=s;memcpy(out+n,algorithm,sizeof(algorithm));n+=sizeof(algorithm);
 out[n++]=0x03;memcpy(out+n,bitLength,b);n+=b;out[n++]=0;memcpy(out+n,publicKey,size);return total;
}
inline bool publicPem(const uint8_t* spki,size_t size,char* out,size_t capacity){
 char encoded[1024];const size_t n=base64(spki,size,encoded,sizeof(encoded));if(!n)return false;
 const char* begin="-----BEGIN PUBLIC KEY-----\n";const char* end="-----END PUBLIC KEY-----\n";
 const size_t needed=strlen(begin)+n+(n+63)/64+strlen(end)+1;if(needed>capacity)return false;
 size_t used=strlen(begin);memcpy(out,begin,used);
 for(size_t offset=0;offset<n;offset+=64){size_t count=n-offset;if(count>64)count=64;memcpy(out+used,encoded+offset,count);used+=count;out[used++]='\n';}
 strcpy(out+used,end);return true;
}
}
