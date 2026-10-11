#pragma once
// Unconnected app-owned prototype; no live KV, SDK, network or .90 changes.
#include "PortableWifiProfiles.h"
#ifndef NDEBUG
#define RISC_WIFI_RESTORE_ASSERTS
#endif
#include "sha2.h"
#ifdef RISC_WIFI_RESTORE_ASSERTS
#undef NDEBUG
#undef RISC_WIFI_RESTORE_ASSERTS
#endif
#include <cstdint>
#include <cstring>
namespace RiscWifiExperimental {
class Reconcile {
 public:
 enum Operation:uint8_t { Save=1, Forget=2 };
 enum Result { Again=0, SaveAt=1, ForgetAt=2, Unchanged=3, Conflict=4, Unavailable=5, Invalid=6, Cancelled=7 };
 static constexpr unsigned IntentBytes=160;
 struct Intent { uint8_t bytes[IntentBytes]{}; };
 struct Plan { Result result=Again;uint32_t slot=UINT32_MAX,count=0;uint64_t viewRevision=0; };
 struct Scan {
  Intent intent{};Plan plan{};uint32_t next=0,empty=UINT32_MAX,match=UINT32_MAX,matches=0;
  bool initialized=false,expectedMatches=false,desiredMatches=false;
  Scan()=default;Scan(const Scan&)=delete;Scan&operator=(const Scan&)=delete;
  ~Scan(){portable_wifi_credentials_wipe(this,sizeof *this);}
 };
 static void wipe(void*p,size_t n){portable_wifi_credentials_wipe(p,n);}
 static bool fingerprint(const portable_wifi_credentials&value,uint8_t out[32]){
  if(!out)return false;
  wipe(out,32);if(!portable_wifi_credentials_validate(&value))return false;
  dtls_sha256_ctx h{};dtls_sha256_init(&h);static const uint8_t domain[]="RTE-WIFI-RECORD-1";
  dtls_sha256_update(&h,domain,sizeof domain);
  const uint8_t ssid=uint8_t(std::strlen(value.ssid)),password=uint8_t(std::strlen(value.password));
  dtls_sha256_update(&h,&ssid,1);dtls_sha256_update(&h,reinterpret_cast<const uint8_t*>(value.ssid),ssid);
  dtls_sha256_update(&h,&password,1);dtls_sha256_update(&h,reinterpret_cast<const uint8_t*>(value.password),password);
  dtls_sha256_final(out,&h);wipe(&h,sizeof h);return true;
 }
 // Expected is a known logical record, never a numeric local slot. Forget
 // intents contain a digest, not the old plaintext password.
 static bool make(Operation op,const portable_wifi_credentials&desired,const portable_wifi_credentials*expected,Intent*out){
  if(!out)return false;
  wipe(out,sizeof *out);
  if((op!=Save&&op!=Forget)||!portable_wifi_credentials_validate(&desired))return false;
  if(expected&&(!portable_wifi_credentials_validate(expected)||std::strcmp(desired.ssid,expected->ssid)))return false;
  wipe(out,sizeof *out);std::memcpy(out->bytes,"WFI0001",7);out->bytes[8]=op;out->bytes[9]=expected?1:0;
  std::memcpy(out->bytes+16,desired.ssid,std::strlen(desired.ssid));
  if(op==Save)std::memcpy(out->bytes+49,desired.password,std::strlen(desired.password));
  if(expected)fingerprint(*expected,out->bytes+113);
  portable_wifi_credentials_put_u32(out->bytes+156,portable_wifi_credentials_crc(out->bytes,156));return true;
 }
 static bool valid(const Intent&i){
  const auto*p=i.bytes;if(std::memcmp(p,"WFI0001",8)||(p[8]!=Save&&p[8]!=Forget)||p[9]>1)return false;
  for(unsigned n=10;n<16;++n)if(p[n])return false;
  for(unsigned n=145;n<156;++n)if(p[n])return false;
  if(!p[9])for(unsigned n=113;n<145;++n)if(p[n])return false;
  if(!std::memchr(p+16,0,33)||!std::memchr(p+49,0,64))return false;
  portable_wifi_credentials v{};std::memcpy(v.ssid,p+16,33);std::memcpy(v.password,p+49,64);
  bool ok=portable_wifi_credentials_validate(&v);
  for(size_t n=std::strlen(v.ssid)+1;n<33;++n)if(p[16+n])ok=false;
  for(size_t n=std::strlen(v.password)+1;n<64;++n)if(p[49+n])ok=false;
  if(p[8]==Forget&&v.password[0])ok=false;
  portable_wifi_credentials_clear(&v);
  return ok&&portable_wifi_credentials_u32(p+156)==portable_wifi_credentials_crc(p,156);
 }
 static bool desired(const Intent&i,portable_wifi_credentials*out){
  if(!out)return false;
  portable_wifi_credentials_clear(out);if(!valid(i))return false;
  std::memcpy(out->ssid,i.bytes+16,33);if(i.bytes[8]==Save)std::memcpy(out->password,i.bytes+49,64);return true;
 }
 static Result begin(const Intent&i,uint64_t viewRevision,Scan*s){
  if(!s)return Invalid;
  if(!viewRevision||!valid(i)){wipe(s,sizeof *s);s->plan.result=Invalid;return Invalid;}
  Intent copy=i;wipe(s,sizeof *s);s->intent=copy;wipe(&copy,sizeof copy);s->plan.viewRevision=viewRevision;s->plan.slot=s->empty=s->match=UINT32_MAX;return Again;
 }
 static Result cancel(Scan*s){if(!s)return Invalid;wipe(s,sizeof *s);s->plan.result=Cancelled;return Cancelled;}
 // At most one profile is read per call; no writes, connection or scan-radio
 // actions occur. The view must be an immutable namespace6 staged snapshot.
 static Result step(const risc_key_value_v1*view,Scan*s){
  if(!s)return Invalid;
  if(s->plan.result!=Again)return s->plan.result;
  if(!valid(s->intent))return done(s,Invalid);
  if(!s->initialized){uint32_t count=0;int rc=portable_wifi_profile_count(view,&count);
   if(rc==PORTABLE_WIFI_CREDENTIALS_AGAIN)return Again;
   if(rc)return done(s,Unavailable);
   s->plan.count=count;s->initialized=true;return Again;
  }
  if(s->next<s->plan.count){
   portable_wifi_credentials row{};const int rc=portable_wifi_profile_load(view,s->next,&row);
   if(rc==PORTABLE_WIFI_CREDENTIALS_AGAIN){portable_wifi_credentials_clear(&row);return Again;}
   if(rc!=PORTABLE_WIFI_CREDENTIALS_LOADED&&rc!=PORTABLE_WIFI_CREDENTIALS_EMPTY){portable_wifi_credentials_clear(&row);return done(s,Unavailable);}
   if(rc==PORTABLE_WIFI_CREDENTIALS_EMPTY&&s->empty==UINT32_MAX)s->empty=s->next;
   if(rc==PORTABLE_WIFI_CREDENTIALS_LOADED&&!std::strcmp(row.ssid,reinterpret_cast<const char*>(s->intent.bytes+16))){
    ++s->matches;s->match=s->next;uint8_t hash[32]{};fingerprint(row,hash);
    s->expectedMatches=s->intent.bytes[9]&&!std::memcmp(hash,s->intent.bytes+113,32);
    s->desiredMatches=!std::strcmp(row.password,reinterpret_cast<const char*>(s->intent.bytes+49));wipe(hash,sizeof hash);
   }
   portable_wifi_credentials_clear(&row);++s->next;return Again;
  }
  if(s->matches>1)return done(s,Conflict);
  const bool save=s->intent.bytes[8]==Save;
  if(s->matches){s->plan.slot=s->match;
   if(save&&s->desiredMatches)return done(s,Unchanged);
   if(!s->expectedMatches)return done(s,Conflict);
   return done(s,save?SaveAt:ForgetAt);
  }
  if(!save)return done(s,Unchanged);
  if(s->intent.bytes[9])return done(s,Conflict);
  if(s->empty==UINT32_MAX&&s->plan.count==UINT32_MAX)return done(s,Unavailable);
  s->plan.slot=s->empty==UINT32_MAX?s->plan.count:s->empty;return done(s,SaveAt);
 }
 private:
 static Result done(Scan*s,Result result){wipe(&s->intent,sizeof s->intent);s->plan.result=result;return result;}
};
}
