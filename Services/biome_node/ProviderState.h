#pragma once
#include "../infra_node/Hardware.h"
#include "NativeEncoding.h"

namespace BiomeRelay {
// One physical resource, one durable in-flight command. The controller retains
// acknowledged command IDs; a fresh claim must still match this local intent.
struct Storage {
 void* context=nullptr;
 bool (*save)(void*,JsonObjectConst)=nullptr;
};
class ProviderState {
 Storage storage{};
 Infra::Hardware hardware{};
 bool freshClaim=false, running=false, cancelling=false;
 char expectedProvider[65]{},expectedResource[257]{};
 bool persist(){
  if(journal.overflowed()||!storage.save||!storage.save(storage.context,journal.as<JsonObjectConst>())){
   healthy=false;Infra::copy(error,sizeof(error),"durable_state_unavailable");return false;
  }
  return true;
 }
 bool reject(const char* why){Infra::copy(error,sizeof(error),why);return false;}
 bool validBinding(JsonObjectConst f)const{
  return completeStrings(f["resource"])&&completeStrings(f["guard"])&&completeStrings(f["lease"])&&
   Infra::text(f["resource"],256)&&Infra::text(f["guard"],512)&&
   Infra::text(f["lease"],128)&&Infra::integer(f["generation"],1,9007199254740991ULL);
 }
 bool sameBinding(JsonObjectConst a,JsonObjectConst b)const{
  return a["resource"]==b["resource"]&&a["guard"]==b["guard"]&&
   a["lease"]==b["lease"]&&a["generation"]==b["generation"];
 }
 static const char* capability(const char* contract){
  if(!strcmp(contract,"relay.bank:1"))return "relay.bank";
  if(!strcmp(contract,"device.outputs.batch:1"))return "device.outputs.batch";
  if(!strcmp(contract,"serial.rs485:1"))return "serial.rs485";
  if(!strcmp(contract,"sound.buzzer:1"))return "sound.buzzer";
  if(!strcmp(contract,"light.rgb:1"))return "light.rgb";
  return nullptr;
 }
 static const char* operationName(const char* cap){
  return !strcmp(cap,"device.outputs.batch")?"apply":!strcmp(cap,"serial.rs485")?"exchange":
   !strcmp(cap,"sound.buzzer")?"tone":"set";
 }
 bool validAction(JsonObjectConst a)const{
  const char* cap=capability(a["contract"]|"");
  return cap&&completeStrings(a["id"])&&completeStrings(a["provider"])&&completeStrings(a["contract"])&&
   Infra::text(a["id"],128)&&Infra::text(a["provider"],64)&&
   !strcmp(a["provider"]|"",expectedProvider)&&!strcmp(a["resource"]|"",expectedResource)&&
   validBinding(a)&&a["inputs"].is<JsonObjectConst>();
 }
 static bool actionDigest(JsonObjectConst a,char out[65]){
  // Local receipt binding also hashes invalid physical inputs, including
  // finite fractional values, so a rejected command can settle without I/O.
  NativeCanonical hash(nullptr,0,true);hash.write('[');
  const char* fields[]={"id","provider","contract","resource","guard","lease","generation","inputs"};
  for(unsigned i=0;i<8;++i){if(i)hash.write(',');hash.value(a[fields[i]]);}
  hash.write(']');uint8_t bytes[32];if(!hash.finish(bytes))return false;
  const char* digits="0123456789abcdef";for(unsigned i=0;i<32;++i){out[i*2]=digits[bytes[i]>>4];out[i*2+1]=digits[bytes[i]&15];}out[64]=0;return true;
 }
 bool matchesAction(JsonObjectConst a)const{
  if(!validAction(a))return false;char digest[65];if(!actionDigest(a,digest))return false;
  return journal["command"]["id"]==a["id"]&&journal["command"]["digest"]==digest;
 }
 void terminal(const char* state,const char* reason=nullptr){
  auto c=journal["command"].as<JsonObject>();c["state"]=state;c["reported"]=false;
  // Uncertain outcomes are explicit provider state, never a successful replay.
  if(reason)c["result"]["reason"]=reason;
  running=cancelling=freshClaim=false;persist();
 }
 public:
 JsonDocument journal;bool healthy=false;char error[96]{};
 bool begin(Storage s,Infra::Hardware h,JsonObjectConst saved){
  storage=s;hardware=h;healthy=true;freshClaim=running=cancelling=false;error[0]=0;
  if(saved.isNull()){
   journal.clear();journal["schema_version"]=1;journal["boot_count"]=0;
   journal["fence"]=nullptr;journal["command"]=nullptr;
  }else{
   journal.set(saved);
   auto f=journal["fence"].as<JsonObjectConst>();auto c=journal["command"].as<JsonObjectConst>();
   if(journal["schema_version"]!=1||!Infra::integer(journal["boot_count"],0,9007199254740990ULL)||
      (!journal["fence"].isNull()&&!validBinding(f))||
      (!journal["command"].isNull()&&(!Infra::text(c["id"],128)||!Infra::digest(c["digest"])||
       !Infra::text(c["contract"],64)||!Infra::text(c["provider"],64)||!c["inputs"].is<JsonObjectConst>()||!validBinding(c)||!c["result"].is<JsonObjectConst>()||
       !c["reported"].is<bool>()||!c["claimed"].is<bool>()||!c["state"].is<const char*>()||
       (c["state"]!="claiming"&&c["state"]!="armed"&&c["state"]!="running"&&
        c["state"]!="succeeded"&&c["state"]!="failed"&&c["state"]!="uncertain")))){
    healthy=false;return reject("durable_state_invalid");
   }
  }
  journal["boot_count"]=journal["boot_count"].as<uint64_t>()+1;
  auto c=journal["command"].as<JsonObject>();
  if(!c.isNull()&&(c["state"]=="claiming"||c["state"]=="armed"||c["state"]=="running")){
   c["state"]="uncertain";c["reported"]=false;
   c["result"]["reason"]="reboot_before_durable_completion";
  }
  return persist();
 }
 bool identity(const char* provider,const char* resource){
  if(!provider||strlen(provider)!=64||!resource||!resource[0]||strlen(resource)>256)
   return reject("invalid_provider_identity");
  if(!journal["command"].isNull()&&!journal["command"]["reported"].as<bool>()&&
     expectedProvider[0]&&(strcmp(provider,expectedProvider)||strcmp(resource,expectedResource)))
   return reject("pending_command_identity");
  return Infra::copy(expectedProvider,sizeof(expectedProvider),provider)&&
   Infra::copy(expectedResource,sizeof(expectedResource),resource);
 }
 bool matches(JsonObjectConst action)const{return matchesAction(action);}
 bool pending()const{return !journal["command"].isNull()&&journal["command"]["reported"]!=true;}
 bool outcome()const{
  const auto c=journal["command"];return pending()&&
   (c["state"]=="succeeded"||c["state"]=="failed"||c["state"]=="uncertain");
 }
 bool active()const{return running;}
 bool needsClaim()const{return pending()&&journal["command"]["state"]=="claiming";}
 bool bound(JsonObjectConst a)const{
  return !journal["fence"].isNull()&&sameBinding(a,journal["fence"].as<JsonObjectConst>());
 }
 // 0 installed; 1 waiting for the actual board runner to quiesce; -1 rejected.
 int fence(JsonObjectConst f){
  error[0]=0;
  if(!healthy||!validBinding(f)||strcmp(f["resource"]|"",expectedResource)){
   reject("invalid_fence");return -1;
  }
  auto old=journal["fence"].as<JsonObjectConst>();
  if(!old.isNull()){
   if(old["resource"]!=f["resource"]||f["generation"].as<uint64_t>()<old["generation"].as<uint64_t>()||
      (f["generation"]==old["generation"]&&!sameBinding(old,f))){
    reject("stale_fence");return -1;
   }
   if(sameBinding(old,f))return 0;
  }
  if(running){
   cancelling=true;
   if(!hardware.cancel||!hardware.cancel(hardware.context))return 1;
   terminal("uncertain","fenced_during_physical_execution");
   if(!healthy)return -1;
  }
  if(needsClaim()){terminal("uncertain","fenced_before_claim_confirmed");if(!healthy)return -1;}
  auto target=journal["fence"].to<JsonObject>();
  for(const char* field:{"resource","guard","lease","generation"})target[field]=f[field];
  return persist()?0:-1;
 }
 bool stage(JsonObjectConst a){
  error[0]=0;
  if(!healthy||!expectedProvider[0]||!validAction(a)||!bound(a))return reject("action_not_admitted");
  if(journal["command"]["id"]==a["id"]){
   if(!matchesAction(a))return reject("operation_identity_conflict");
   // This is a receipt lookup/retry, never permission to repeat an effect.
   return true;
  }
  if(pending())return reject("previous_command_unsettled");
  if(a["state"]!="queued"&&a["state"]!="claimed")return reject("action_not_outstanding");
  char digest[65];if(!actionDigest(a,digest))return reject("action_identity_unencodable");auto c=journal["command"].to<JsonObject>();
  for(const char* field:{"id","provider","contract","resource","guard","lease","generation","inputs"})c[field]=a[field];
  c["digest"]=digest;c["boot_count"]=journal["boot_count"];c["reported"]=false;c["claimed"]=a["state"]=="claimed";c["result"].to<JsonObject>();
  c["state"]=a["state"]=="queued"?"claiming":"uncertain";
  if(a["state"]=="claimed")c["result"]["reason"]="claimed_without_local_effect_receipt";
  freshClaim=a["state"]=="queued";return persist();
 }
 bool claimed(JsonObjectConst a){
  error[0]=0;
  if(!healthy||!matchesAction(a)||(!outcome()&&!bound(a)))return reject("claim_identity_conflict");
  if(a["state"]=="succeeded"||a["state"]=="failed"||a["state"]=="uncertain"){
   auto c=journal["command"].as<JsonObject>();c["state"]=a["state"];c["result"]=a["result"];
   if(!c["result"].is<JsonObject>())c["result"].to<JsonObject>();
   c["reported"]=true;freshClaim=false;return persist();
  }
  if(a["state"]!="claimed")return reject("claim_not_granted");
  if(!freshClaim||journal["command"]["boot_count"]!=journal["boot_count"]||
     journal["command"]["state"]!="claiming"){
   if(outcome()){journal["command"]["claimed"]=true;return persist();}
   terminal("uncertain","claim_has_no_live_execution_intent");return healthy;
  }
  const char* cap=capability(a["contract"]|"");
  if(!completeStrings(a["inputs"])||!Infra::hardwareInput(cap,operationName(cap),a["inputs"].as<JsonObjectConst>())){
   journal["command"]["claimed"]=true;
   terminal("failed","invalid_hardware_inputs");return healthy;
  }
  // The journal is uncertain BEFORE the first physical effect. A reset can
  // report uncertainty, but cannot cause this command to be dispatched twice.
  journal["command"]["state"]="armed";journal["command"]["claimed"]=true;freshClaim=false;
  if(!persist())return false;
  JsonDocument result;
  int rc=hardware.ready&&hardware.execute&&hardware.ready(hardware.context)?
   hardware.execute(hardware.context,cap,a["inputs"].as<JsonObjectConst>(),result.to<JsonObject>()):-1;
  journal["command"]["result"]=result.as<JsonObjectConst>();
  if(rc>0){running=true;journal["command"]["state"]="running";return true;}
  terminal(rc==0?"succeeded":"uncertain",rc==0?nullptr:"hardware_execution_failed_after_intent");
  return healthy;
 }
 void tick(){
  if(!running)return;
  if(!healthy){if(hardware.cancel&&hardware.cancel(hardware.context))running=false;return;}
  if(cancelling){
   if(hardware.cancel&&hardware.cancel(hardware.context))
    terminal("uncertain","physical_execution_cancelled");
   return;
  }
  JsonDocument result;
  int rc=hardware.poll?hardware.poll(hardware.context,result.to<JsonObject>()):-1;
  if(rc>0)return;
  journal["command"]["result"]=result.as<JsonObjectConst>();
  terminal(rc==0?"succeeded":"uncertain",rc==0?nullptr:"hardware_completion_unknown");
 }
 bool reportAccepted(JsonObjectConst a){
  if(!healthy||!outcome()||!matchesAction(a)||a["result"]!=journal["command"]["result"]||
     a["state"]!=journal["command"]["state"])return reject("result_receipt_mismatch");
  journal["command"]["reported"]=true;return persist();
 }
 bool superseded(){
  // Controller authority ended; retain the historical effect receipt, cancel
  // active execution locally, and stop trying to report an obsolete generation.
  if(!healthy)return false;
  if(running){cancel();if(running)return false;}
  if(needsClaim())terminal("uncertain","controller_superseded_before_claim");
  if(!journal["command"].isNull()){
   journal["command"]["reported"]=true;
   journal["command"]["controller_abandoned"]=true;
   freshClaim=false;
   return persist();
  }
  return true;
 }
 void cancel(){
  if(!running)return;cancelling=true;tick();
 }
};
}
