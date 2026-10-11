#pragma once
#include "WifiReconcile.h"
namespace RiscWifiExperimental {
// App-owned staging only. No SDK table, live Wi-Fi UI or Runtime integration.
// Source is an immutable, held namespace-6 view. Its put callback is never used.
class Stage {
 public:
 using Model=Reconcile;
 enum Result { Again=0, Ready=1, Committed=2, Conflict=3, Unavailable=4,
               Invalid=5, Cancelled=6, Stale=7, Uncertain=8, Capacity=9 };
 enum CommitResult { CommitOk=0, CommitBusy=1, CommitStale=2, CommitConflict=3,
                     CommitIo=4, CommitCapacity=5 };
 static constexpr uint32_t MaxMutations=8;
 struct Mutation {char key[16]{};uint32_t size=0;uint8_t value[64]{};};
 struct Batch {
  uint64_t viewRevision=0,pendingFrontier=0,originalIntentId=0;
  uint32_t namespaceId=PORTABLE_WIFI_CREDENTIALS_STORE_INSTANCE,count=0;
  Model::Intent originalIntent{};Mutation mutations[MaxMutations]{};
 };
 static_assert(sizeof(Batch)<=4096,"Wi-Fi operation must fit one copied native group");
 struct View {const risc_key_value_v1*kv;uint64_t revision,frontier;};
 struct Target {void*context;CommitResult(*commit)(void*,const Batch*);};
 Stage()=default;Stage(const Stage&)=delete;Stage&operator=(const Stage&)=delete;
 ~Stage(){clearSensitive();}
 // The optional limit is a per-operation admission bound, NOT a profile count.
 Result begin(const View&view,uint64_t id,const Model::Intent&intent,uint32_t limit=MaxMutations){
  Model::Intent copy=intent;clearSensitive();result_=Invalid;
  if(!view.kv||view.kv->api_version!=1||view.kv->struct_size<sizeof(risc_key_value_v1)||!view.kv->get||!view.revision||!id||id>view.frontier||limit>MaxMutations||!Model::valid(copy)){Model::wipe(&copy,sizeof copy);return result_;}
  source_={1,sizeof(source_),view.kv->context,view.kv->get,denyWrite};
  batch_.viewRevision=view.revision;batch_.pendingFrontier=view.frontier;batch_.originalIntentId=id;batch_.namespaceId=PORTABLE_WIFI_CREDENTIALS_STORE_INSTANCE;batch_.originalIntent=copy;
  limit_=limit;result_=Again;phase_=Scanning;Model::begin(copy,view.revision,&scan_);Model::wipe(&copy,sizeof copy);return result_;
 }
 // Each scan step reads at most one profile. Building performs only the codec's
 // fixed-size reads against the immutable view and writes to this bounded RAM
 // overlay. A temporary read BUSY discards the partial overlay before retry.
 Result step(){
  if(result_!=Again)return result_;
  if(phase_==Scanning){const auto rc=Model::step(&source_,&scan_);if(rc==Model::Again)return Again;
   plan_=scan_.plan;
   if(rc==Model::Conflict)return finish(Conflict);
   if(rc==Model::Unavailable)return finish(Unavailable);
   if(rc!=Model::SaveAt&&rc!=Model::ForgetAt&&rc!=Model::Unchanged)return finish(Invalid);
   phase_=Building;return Again;
  }
  if(phase_!=Building)return finish(Invalid);
  clearMutations();readError_=RISC_KEY_VALUE_OK;capacityFailed_=false;
  if(plan_.result==Model::Unchanged){result_=Ready;return result_;}
  auto staged=api();int rc=PORTABLE_WIFI_CREDENTIALS_INVALID;
  if(plan_.result==Model::SaveAt){portable_wifi_credentials desired{};
   if(!Model::desired(batch_.originalIntent,&desired))return finish(Invalid);
   rc=portable_wifi_profile_save(&staged,plan_.slot,&desired);portable_wifi_credentials_clear(&desired);
  }else rc=portable_wifi_profile_forget(&staged,plan_.slot);
  if(capacityFailed_)return finish(Capacity);
  if(readError_==RISC_KEY_VALUE_BUSY){clearMutations();return Again;}
  if(readError_!=RISC_KEY_VALUE_OK)return finish(Unavailable);
  if((plan_.result==Model::SaveAt&&rc!=PORTABLE_WIFI_CREDENTIALS_SAVED)||(plan_.result==Model::ForgetAt&&rc!=PORTABLE_WIFI_CREDENTIALS_FORGOTTEN))return finish(Unavailable);
  result_=Ready;return result_;
 }
 // Receiver must compare revision AND frontier and authenticate the original
 // pending ID/opaque bytes before atomically publishing the entire mutation
 // group plus its resolution marker. It must retain the original intent until
 // the resulting SD checkpoint is acknowledged. No callback may borrow Batch.
 Result commit(const Target&target){
  if(result_!=Ready)return result_;
  if(!target.commit)return finish(Invalid);
  switch(target.commit(target.context,&batch_)){
   case CommitOk:return finish(Committed);
   case CommitBusy:return Ready; // no external mutation began; cancellation remains available
   case CommitStale:return finish(Stale);
   case CommitConflict:return finish(Conflict);
   case CommitCapacity:return finish(Capacity);
   default:return finish(Uncertain); // reload; do not automatically replay uncertain writes
  }
 }
 Result cancel(){return result_==Again||result_==Ready?finish(Cancelled):result_;}
 Result result()const{return result_;}
 const Batch*batch()const{return result_==Ready?&batch_:nullptr;}
 bool sensitiveCleared()const{for(auto b:scan_.intent.bytes)if(b)return false;for(auto b:batch_.originalIntent.bytes)if(b)return false;for(const auto&m:batch_.mutations){for(auto b:m.value)if(b)return false;}return true;}
 private:
 enum Phase {Idle,Scanning,Building};
 Result result_=Invalid;Phase phase_=Idle;Model::Scan scan_;Model::Plan plan_{};Batch batch_{};risc_key_value_v1 source_{};uint32_t limit_=MaxMutations;int32_t readError_=0;bool capacityFailed_=false;
 static int32_t denyWrite(void*,const char*,const void*,uint32_t){return RISC_KEY_VALUE_CONTEXT;}
 static bool validKey(const char*k){if(!k||!*k)return false;for(unsigned i=0;i<16;++i){char c=k[i];if(!c)return true;if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='.'||c=='-'||c=='_'))return false;}return false;}
 void clearMutations(){Model::wipe(batch_.mutations,sizeof batch_.mutations);batch_.count=0;}
 void clearSensitive(){Model::cancel(&scan_);Model::wipe(&batch_,sizeof batch_);source_={};plan_={};phase_=Idle;readError_=0;capacityFailed_=false;}
 Result finish(Result r){clearSensitive();result_=r;return r;}
 risc_key_value_v1 api(){return {1,sizeof(risc_key_value_v1),this,
  [](void*p,const char*k,void*out,uint32_t cap,uint32_t*n){return static_cast<Stage*>(p)->read(k,out,cap,n);},
  [](void*p,const char*k,const void*value,uint32_t size){return static_cast<Stage*>(p)->write(k,value,size);}};}
 int32_t read(const char*k,void*out,uint32_t cap,uint32_t*n){
  if(n)*n=0;
  if(!n||!validKey(k)||(!out&&cap))return RISC_KEY_VALUE_INVALID;
  for(uint32_t i=0;i<batch_.count;++i)if(!std::strcmp(k,batch_.mutations[i].key)){
   const auto&m=batch_.mutations[i];*n=m.size;if(cap<m.size)return RISC_KEY_VALUE_BUFFER_SMALL;std::memcpy(out,m.value,m.size);return RISC_KEY_VALUE_OK;
  }
  uint8_t bytes[64]{};uint32_t size=0;int32_t rc=source_.get(source_.context,k,bytes,sizeof bytes,&size);
  if(rc==RISC_KEY_VALUE_OK){if(!size||size>sizeof bytes){rc=RISC_KEY_VALUE_IO;}else if(cap<size){*n=size;rc=RISC_KEY_VALUE_BUFFER_SMALL;}else{std::memcpy(out,bytes,size);*n=size;}}
  if(rc!=RISC_KEY_VALUE_OK&&rc!=RISC_KEY_VALUE_NOT_FOUND&&!readError_)readError_=rc;
  Model::wipe(bytes,sizeof bytes);return rc;
 }
 int32_t write(const char*k,const void*value,uint32_t size){
  if(!validKey(k)||!value||!size||size>64)return RISC_KEY_VALUE_INVALID;
  uint32_t i=0;for(;i<batch_.count;++i)if(!std::strcmp(k,batch_.mutations[i].key))break;
  if(i==batch_.count){if(i>=limit_){capacityFailed_=true;return RISC_KEY_VALUE_IO;}++batch_.count;}
  auto&m=batch_.mutations[i];Model::wipe(&m,sizeof m);std::strcpy(m.key,k);m.size=size;std::memcpy(m.value,value,size);return RISC_KEY_VALUE_OK;
 }
};
}
