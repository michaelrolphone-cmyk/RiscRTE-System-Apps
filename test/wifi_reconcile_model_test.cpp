#include "experimental/wifi_reconcile/WifiReconcile.h"
#undef NDEBUG
#include <cassert>
#include <map>
#include <string>
#include <vector>
#include <cstdio>
using Model=RiscWifiExperimental::Reconcile;
struct Store {
 std::map<std::string,std::vector<uint8_t>> rows;unsigned writes=0,reads=0;bool busy=false;
 risc_key_value_v1 api(){return {1,sizeof(risc_key_value_v1),this,
  [](void*p,const char*k,void*out,uint32_t cap,uint32_t*n)->int32_t{auto&s=*static_cast<Store*>(p);++s.reads;*n=0;if(s.busy){s.busy=false;return RISC_KEY_VALUE_BUSY;}auto i=s.rows.find(k);if(i==s.rows.end())return RISC_KEY_VALUE_NOT_FOUND;*n=i->second.size();if(cap<*n)return RISC_KEY_VALUE_BUFFER_SMALL;std::memcpy(out,i->second.data(),*n);return 0;},
  [](void*p,const char*k,const void*v,uint32_t n)->int32_t{auto&s=*static_cast<Store*>(p);++s.writes;s.rows[k]={static_cast<const uint8_t*>(v),static_cast<const uint8_t*>(v)+n};return 0;}};}
};
static portable_wifi_credentials value(const char*ssid,const char*password="synthetic-password"){
 portable_wifi_credentials v{};assert(std::strlen(ssid)<sizeof v.ssid&&std::strlen(password)<sizeof v.password);std::strcpy(v.ssid,ssid);std::strcpy(v.password,password);return v;
}
static Model::Plan resolve(Store&s,const Model::Intent&i){
 auto kv=s.api();const unsigned before=s.writes;Model::Scan scan;assert(Model::begin(i,41,&scan)==Model::Again);
 unsigned steps=0;while(Model::step(&kv,&scan)==Model::Again){assert(++steps<10000);}assert(s.writes==before);
 const auto plan=scan.plan;assert(plan.viewRevision==41);for(auto b:scan.intent.bytes)assert(!b);Model::cancel(&scan);return plan;
}
int main(){
 {dtls_sha256_ctx h{};uint8_t out[32];dtls_sha256_init(&h);dtls_sha256_update(&h,reinterpret_cast<const uint8_t*>("abc"),3);dtls_sha256_final(out,&h);const uint8_t expected[]={0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};assert(!std::memcmp(out,expected,32));}
 Store sd;auto kv=sd.api();for(unsigned n=0;n<40;++n){char ssid[33];std::snprintf(ssid,sizeof ssid,"synthetic-network-%u",n);auto v=value(ssid);assert(portable_wifi_profile_save(&kv,n,&v)==0);}
 Model::Intent intent;auto desired=value("fresh-network");assert(Model::make(Model::Save,desired,nullptr,&intent));auto plan=resolve(sd,intent);assert(plan.result==Model::SaveAt&&plan.slot==40&&plan.count==40);
 auto prior=value("synthetic-network-17");desired=value(prior.ssid,"different-password");assert(Model::make(Model::Save,desired,&prior,&intent));plan=resolve(sd,intent);assert(plan.result==Model::SaveAt&&plan.slot==17&&plan.count==40);
 assert(Model::make(Model::Save,desired,nullptr,&intent));assert(resolve(sd,intent).result==Model::Conflict);
 assert(Model::make(Model::Save,prior,nullptr,&intent));assert(resolve(sd,intent).result==Model::Unchanged);
 auto wrong=value(prior.ssid,"changed-elsewhere");assert(Model::make(Model::Save,desired,&wrong,&intent));assert(resolve(sd,intent).result==Model::Conflict);
 assert(Model::make(Model::Forget,prior,&prior,&intent));plan=resolve(sd,intent);assert(plan.result==Model::ForgetAt&&plan.slot==17);for(unsigned i=49;i<113;++i)assert(intent.bytes[i]==0);
 assert(Model::make(Model::Forget,prior,nullptr,&intent));assert(resolve(sd,intent).result==Model::Conflict);
 auto absent=value("absent-network");assert(Model::make(Model::Forget,absent,&absent,&intent));assert(resolve(sd,intent).result==Model::Unchanged);
 assert(Model::make(Model::Save,absent,&absent,&intent));assert(resolve(sd,intent).result==Model::Conflict);
 assert(portable_wifi_profile_forget(&kv,4)==PORTABLE_WIFI_CREDENTIALS_EMPTY);assert(Model::make(Model::Save,absent,nullptr,&intent));plan=resolve(sd,intent);assert(plan.result==Model::SaveAt&&plan.slot==4&&plan.count==40);
 assert(portable_wifi_profile_save(&kv,40,&prior)==0);assert(Model::make(Model::Save,prior,nullptr,&intent));assert(resolve(sd,intent).result==Model::Conflict);
 assert(Model::make(Model::Save,absent,nullptr,&intent));sd.busy=true;assert(resolve(sd,intent).result==Model::SaveAt);
 Model::Scan scan;assert(Model::begin(intent,5,&scan)==Model::Again);assert(Model::cancel(&scan)==Model::Cancelled);for(auto b:scan.intent.bytes)assert(!b);assert(Model::step(&kv,&scan)==Model::Cancelled);
 assert(Model::begin(intent,5,&scan)==Model::Again);auto broken=intent;broken.bytes[0]^=1;assert(Model::begin(broken,5,&scan)==Model::Invalid);for(auto b:scan.intent.bytes)assert(!b);
 for(unsigned n=0;n<Model::IntentBytes;++n){auto bad=intent;bad.bytes[n]^=0x80;assert(!Model::valid(bad));}
 {auto corrupt=sd;corrupt.rows["wifi.profiles"][0]^=1;assert(resolve(corrupt,intent).result==Model::Unavailable);}
 {auto corrupt=sd;corrupt.rows["w00000003.c"][0]^=1;assert(resolve(corrupt,intent).result==Model::Unavailable);}
 portable_wifi_credentials decoded{};assert(Model::desired(intent,&decoded));assert(!std::strcmp(decoded.ssid,absent.ssid));Model::wipe(&intent,sizeof intent);portable_wifi_credentials_clear(&decoded);
 std::puts("Wi-Fi app-owned model: 40+ profiles, slot remapping, known/unknown-base conflicts, duplicate identity, idempotence, corrupt-view refusal, bounded polling and cancellation PASS");
 std::puts("UNCONNECTED MODEL: no live storage writes, network connection, SDK/production routing, or end-to-end persistence claim");
}
