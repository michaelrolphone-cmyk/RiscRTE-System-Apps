#include "experimental/wifi_reconcile/WifiStage.h"
#undef NDEBUG
#include <cassert>
#include <map>
#include <string>
#include <vector>
#include <cstdio>
using S=RiscWifiExperimental::Stage;using M=RiscWifiExperimental::Reconcile;using Bytes=std::vector<uint8_t>;
struct Store {
 std::map<std::string,Bytes> rows;uint64_t revision=41,frontier=5;unsigned writes=0,reads=0,failRead=0,busyRead=0;bool busy=false;
 risc_key_value_v1 api(){return {1,sizeof(risc_key_value_v1),this,
  [](void*p,const char*k,void*out,uint32_t cap,uint32_t*n)->int32_t{auto&s=*static_cast<Store*>(p);++s.reads;*n=0;if(s.failRead&&s.reads==s.failRead){if(out&&cap)std::memset(out,0xa5,cap);return RISC_KEY_VALUE_IO;}if(s.busy||(s.busyRead&&s.reads==s.busyRead)){s.busy=false;return RISC_KEY_VALUE_BUSY;}auto i=s.rows.find(k);if(i==s.rows.end())return RISC_KEY_VALUE_NOT_FOUND;*n=i->second.size();if(cap<*n)return RISC_KEY_VALUE_BUFFER_SMALL;std::memcpy(out,i->second.data(),*n);return RISC_KEY_VALUE_OK;},
  [](void*p,const char*k,const void*v,uint32_t n)->int32_t{auto&s=*static_cast<Store*>(p);++s.writes;s.rows[k]=Bytes(static_cast<const uint8_t*>(v),static_cast<const uint8_t*>(v)+n);return RISC_KEY_VALUE_OK;}};}
};
portable_wifi_credentials value(const char*ssid,const char*password="synthetic-password"){portable_wifi_credentials v{};assert(strlen(ssid)<sizeof v.ssid&&strlen(password)<sizeof v.password);strcpy(v.ssid,ssid);strcpy(v.password,password);return v;}
Store seed(unsigned count=40){Store s;auto api=s.api();for(unsigned i=0;i<count;++i){char name[33];snprintf(name,sizeof name,"synthetic-network-%u",i);auto v=value(name);assert(portable_wifi_profile_save(&api,i,&v)==0);}s.writes=s.reads=0;return s;}
S::Result prepare(Store&s,const M::Intent&i,S&stage,uint32_t limit=S::MaxMutations){auto api=s.api();assert(stage.begin({&api,s.revision,s.frontier},3,i,limit)==S::Again);unsigned count=0;while(stage.step()==S::Again)assert(++count<100000);return stage.result();}
struct Target {
 Store&store;M::Intent original;uint64_t expectedId=3;unsigned calls=0,failMutation=0;bool busy=false,persistIo=false,dropIo=false,already=false,capacity=false;
 unsigned resolutions=0;M::Intent retainedOriginal{};
 S::Target api(){return {this,[](void*p,const S::Batch*b){return static_cast<Target*>(p)->commit(*b);}};}
 S::CommitResult commit(const S::Batch&b){++calls;
  if(b.originalIntentId!=expectedId||memcmp(b.originalIntent.bytes,original.bytes,M::IntentBytes))return S::CommitConflict;
  if(b.viewRevision!=store.revision||b.pendingFrontier!=store.frontier)return S::CommitStale;
  if(b.namespaceId!=6||b.count>S::MaxMutations||already)return S::CommitConflict;
  if(busy){busy=false;return S::CommitBusy;}
  if(capacity)return S::CommitCapacity;
  auto candidate=store.rows;
  for(unsigned i=0;i<b.count;++i){const auto&m=b.mutations[i];if(failMutation==i+1)return S::CommitIo;assert(m.size&&m.size<=64);candidate[m.key]=Bytes(m.value,m.value+m.size);}
  if(dropIo)return S::CommitIo;
  store.rows.swap(candidate);++store.revision;++resolutions;already=true;retainedOriginal=b.originalIntent;
  return persistIo?S::CommitIo:S::CommitOk;
 }
};
void expect(Store&s,unsigned slot,const portable_wifi_credentials&expected){auto api=s.api();portable_wifi_credentials actual{};assert(portable_wifi_profile_load(&api,slot,&actual)==0);assert(!strcmp(actual.ssid,expected.ssid)&&!strcmp(actual.password,expected.password));portable_wifi_credentials_clear(&actual);}
unsigned count(Store&s){auto api=s.api();uint32_t n=0;assert(portable_wifi_profile_count(&api,&n)==0);return n;}
bool contains(const S::Batch&b,const char*k){for(unsigned i=0;i<b.count;++i)if(!strcmp(b.mutations[i].key,k))return true;return false;}
int main(){
 auto sd=seed();auto originalRows=sd.rows;M::Intent intent{};auto desired=value("new-offline-network");assert(M::make(M::Save,desired,nullptr,&intent));
 {S stage;assert(prepare(sd,intent,stage)==S::Ready);assert(sd.rows==originalRows&&sd.writes==0);const auto&b=*stage.batch();assert(b.count==4&&contains(b,"wifi.profiles")&&contains(b,"w00000028.c")&&contains(b,"w00000028.a0")&&contains(b,"w00000028.a1"));assert(b.viewRevision==41&&b.pendingFrontier==5&&b.originalIntentId==3&&!memcmp(b.originalIntent.bytes,intent.bytes,160));Target target{sd,intent};assert(stage.commit(target.api())==S::Committed&&stage.sensitiveCleared());assert(target.resolutions==1&&!memcmp(target.retainedOriginal.bytes,intent.bytes,160));assert(stage.cancel()==S::Committed);assert(count(sd)==41);expect(sd,40,desired);for(unsigned i=0;i<40;++i){char n[33];snprintf(n,sizeof n,"synthetic-network-%u",i);expect(sd,i,value(n));}}
 {auto s=seed();auto api=s.api();assert(portable_wifi_profile_forget(&api,7)==PORTABLE_WIFI_CREDENTIALS_EMPTY);s.writes=0;auto before=s.rows;S stage;assert(prepare(s,intent,stage)==S::Ready);assert(stage.batch()->count==3&&!contains(*stage.batch(),"wifi.profiles")&&contains(*stage.batch(),"w00000007.c"));assert(s.rows==before&&s.writes==0);Target t{s,intent};assert(stage.commit(t.api())==S::Committed&&count(s)==40);expect(s,7,desired);}
 auto prior=value("synthetic-network-17");auto changed=value(prior.ssid,"replacement-password");assert(M::make(M::Save,changed,&prior,&intent));
 {auto s=seed();auto before=s.rows;S stage;assert(prepare(s,intent,stage)==S::Ready);assert(stage.batch()->count==3&&contains(*stage.batch(),"w00000011.c")&&!contains(*stage.batch(),"wifi.profiles"));assert(s.rows==before&&s.writes==0);Target t{s,intent};assert(stage.commit(t.api())==S::Committed&&count(s)==40);expect(s,17,changed);}
 assert(M::make(M::Forget,prior,&prior,&intent));
 {auto s=seed();S stage;assert(prepare(s,intent,stage)==S::Ready);assert(stage.batch()->count==5&&contains(*stage.batch(),"w00000011.c")&&contains(*stage.batch(),"w00000011.a0")&&contains(*stage.batch(),"w00000011.b1"));Target t{s,intent};assert(stage.commit(t.api())==S::Committed&&stage.sensitiveCleared()&&count(s)==40);auto api=s.api();portable_wifi_credentials out{};assert(portable_wifi_profile_load(&api,17,&out)==PORTABLE_WIFI_CREDENTIALS_EMPTY);}
 unsigned faultCases=0;
 for(unsigned operation=0;operation<3;++operation){
  const bool forget=operation==2,update=operation==1;
  auto request=forget?prior:update?changed:desired;assert(M::make(forget?M::Forget:M::Save,request,(forget||update)?&prior:nullptr,&intent));const unsigned changes=forget?5:update?3:4;
  for(unsigned limit=0;limit<changes;++limit){auto s=seed();auto before=s.rows;S stage;assert(prepare(s,intent,stage,limit)==S::Capacity&&stage.sensitiveCleared());assert(s.rows==before&&!s.writes);++faultCases;}
  for(unsigned failure=1;failure<=changes;++failure){auto s=seed();auto before=s.rows;S stage;assert(prepare(s,intent,stage)==S::Ready);Target t{s,intent};t.failMutation=failure;assert(stage.commit(t.api())==S::Uncertain&&stage.sensitiveCleared());assert(s.rows==before&&!t.resolutions&&!s.writes&&!memcmp(t.original.bytes,intent.bytes,M::IntentBytes));++faultCases;}
  for(bool persist:{false,true}){auto s=seed();auto before=s.rows;S stage;assert(prepare(s,intent,stage)==S::Ready);Target t{s,intent};t.persistIo=persist;t.dropIo=!persist;assert(stage.commit(t.api())==S::Uncertain&&stage.sensitiveCleared());assert((s.rows==before)==!persist);assert(stage.commit(t.api())==S::Uncertain&&t.calls==1);assert(stage.cancel()==S::Uncertain);++faultCases;}
 }
 assert(M::make(M::Save,desired,nullptr,&intent));
 for(unsigned kind=0;kind<4;++kind){auto s=seed();auto before=s.rows;S stage;assert(prepare(s,intent,stage)==S::Ready);Target t{s,intent};if(kind==0)++s.revision;else if(kind==1)++s.frontier;else if(kind==2)++t.expectedId;else t.original.bytes[49]^=1;assert(stage.commit(t.api())==(kind<2?S::Stale:S::Conflict)&&stage.sensitiveCleared());assert(s.rows==before&&!t.resolutions);}
 {auto s=seed();auto before=s.rows;S stage;assert(prepare(s,intent,stage)==S::Ready);Target t{s,intent};t.busy=true;assert(stage.commit(t.api())==S::Ready&&s.rows==before);assert(stage.cancel()==S::Cancelled&&stage.sensitiveCleared());assert(stage.commit(t.api())==S::Cancelled&&t.calls==1);}
 {auto s=seed();auto api=s.api();S stage;assert(stage.begin({&api,s.revision,s.frontier},3,intent)==S::Again);assert(stage.step()==S::Again);assert(stage.cancel()==S::Cancelled&&stage.sensitiveCleared()&&!s.writes);}
 {auto s=seed();auto api=s.api();S stage;assert(stage.begin({&api,s.revision,s.frontier},6,intent)==S::Invalid&&stage.sensitiveCleared());}
 {auto s=seed();auto before=s.rows;S stage;assert(prepare(s,intent,stage)==S::Ready);Target t{s,intent};t.capacity=true;assert(stage.commit(t.api())==S::Capacity&&stage.sensitiveCleared());assert(s.rows==before&&!t.resolutions&&!memcmp(t.original.bytes,intent.bytes,M::IntentBytes));}
 {alignas(risc_key_value_v1) const uint32_t header[]={1,8};S stage;auto*shortApi=reinterpret_cast<const risc_key_value_v1*>(header);assert(stage.begin({shortApi,1,5},3,intent)==S::Invalid&&stage.sensitiveCleared());}
 // Exhaustively fail and defer every immutable-view read for new/update/forget,
 // including BUSY after a profile-count mutation has already been staged.
 for(unsigned operation=0;operation<3;++operation){
  const bool forget=operation==2,update=operation==1;
  auto request=forget?prior:update?changed:desired;assert(M::make(forget?M::Forget:M::Save,request,(forget||update)?&prior:nullptr,&intent));
  unsigned readCount=0;{auto s=seed();S stage;assert(prepare(s,intent,stage)==S::Ready);readCount=s.reads;stage.cancel();}
  for(unsigned i=1;i<=readCount;++i){
   auto s=seed();s.failRead=i;auto before=s.rows;S stage;assert(prepare(s,intent,stage)==S::Unavailable&&stage.sensitiveCleared());assert(s.rows==before&&!s.writes);++faultCases;
   s=seed();s.busyRead=i;before=s.rows;assert(prepare(s,intent,stage)==S::Ready);assert(s.rows==before&&!s.writes);Target t{s,intent};assert(stage.commit(t.api())==S::Committed&&stage.sensitiveCleared()&&count(s)==(operation?40u:41u));++faultCases;
  }
 }
 assert(M::make(M::Save,desired,nullptr,&intent));
 {auto s=seed();s.busy=true;S stage;assert(prepare(s,intent,stage)==S::Ready);stage.cancel();assert(stage.sensitiveCleared());}
 // Duplicate logical identity or conflicting expected records are never staged.
 {auto s=seed();auto api=s.api();assert(portable_wifi_profile_save(&api,40,&prior)==0);s.writes=0;auto before=s.rows;assert(M::make(M::Save,changed,&prior,&intent));S stage;assert(prepare(s,intent,stage)==S::Conflict&&stage.sensitiveCleared());assert(s.rows==before&&!s.writes);}
 {auto s=seed();auto other=value(prior.ssid,"newer-sd-password");assert(M::make(M::Forget,prior,&other,&intent));S stage;assert(prepare(s,intent,stage)==S::Conflict&&stage.sensitiveCleared()&&!s.writes);}
 // Already-applied intent still emits one zero-mutation resolution transaction.
 {auto s=seed();assert(M::make(M::Save,prior,nullptr,&intent));S stage;assert(prepare(s,intent,stage)==S::Ready&&stage.batch()->count==0);auto before=s.rows;Target t{s,intent};assert(stage.commit(t.api())==S::Committed&&t.resolutions==1&&s.rows==before);}
 // A duplicated original operation ID cannot create a second resolution marker.
 {auto s=seed();assert(M::make(M::Save,desired,nullptr,&intent));S first,second;Target t{s,intent};assert(prepare(s,intent,first)==S::Ready);assert(first.commit(t.api())==S::Committed);auto before=s.rows;assert(prepare(s,intent,second)==S::Ready);assert(second.commit(t.api())==S::Conflict&&second.sensitiveCleared());assert(t.resolutions==1&&s.rows==before);}
 // No profile count cap is introduced by the per-operation mutation bound.
 {auto s=seed(1025);assert(M::make(M::Save,desired,nullptr,&intent));S stage;assert(prepare(s,intent,stage)==S::Ready&&stage.batch()->count==4&&contains(*stage.batch(),"w00000401.c"));assert(!s.writes);Target t{s,intent};assert(stage.commit(t.api())==S::Committed&&count(s)==1026);expect(s,1025,desired);}
 printf("Wi-Fi staging: 1025+ profiles, remapped Save/Forget batches, preserved count, original intent ID, revision/frontier CAS, conflicts, cancellation/wiping and %u staged/commit/read failures PASS\n",faultCases);
 puts("LOGICAL MODEL ONLY: 1025 profiles do not imply they fit the separate 65512-byte native replica image budget");
 puts("UNCONNECTED STAGING: atomic receiver is a model; no live ABI/UI, native/SD integration, connection or .90 changes");
}
