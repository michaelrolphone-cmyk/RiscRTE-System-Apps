#pragma once
#include "Core.h"
namespace Infra {
inline bool text(JsonVariantConst v,size_t n,bool empty=false){return v.is<const char*>()&&strlen(v.as<const char*>())<=n&&(empty||v.as<const char*>()[0]);}
inline bool integer(JsonVariantConst v,uint64_t lo,uint64_t hi){return v.is<uint64_t>()&&v.as<uint64_t>()>=lo&&v.as<uint64_t>()<=hi;}
inline bool digest(JsonVariantConst v){if(!text(v,64)||strlen(v.as<const char*>())!=64)return false;for(const char* p=v.as<const char*>();*p;++p)if(!(*p>='0'&&*p<='9')&&!(*p>='a'&&*p<='f'))return false;return true;}
inline bool https(JsonVariantConst v){if(!text(v,2048))return false;const char* p=v.as<const char*>();if(strncmp(p,"https://",8)||!p[8])return false;for(p+=8;*p;++p)if(*p<=32||*p>=127||*p=='@'||*p=='#'||*p=='\\')return false;return true;}
inline JsonObjectConst find(JsonArrayConst list,const char* key,const char* value){for(auto x:list)if(x[key].is<const char*>()&&!strcmp(x[key],value))return x.as<JsonObjectConst>();return {};}
inline bool binding(JsonObjectConst b){
 if(!keys(b,{"binding_id","revision","capability","api_version","operation","endpoint_policy_id","credential_ref","cadence_ms","timeout_ms","request_max_bytes","response_max_bytes","effect_class","max_items","max_elapsed_ms","continuation","expires_at_ms","authorization_scope","required"},{"wake_semantics","format","collection_bound","projection","expected_status_min","expected_status_max","semantic_field","semantic_value","latest_state"}))return false;
 if(!text(b["binding_id"],128)||!integer(b["revision"],1,9007199254740991ULL)||b["api_version"]!=1||!text(b["endpoint_policy_id"],128)||!text(b["credential_ref"],128)||!integer(b["cadence_ms"],1000,86400000)||!integer(b["timeout_ms"],100,30000)||!integer(b["request_max_bytes"],1,16384)||!integer(b["response_max_bytes"],1,32768)||!integer(b["max_items"],1,32)||!integer(b["max_elapsed_ms"],100,b["capability"]=="scheduler.drive"?10000:2000)||!integer(b["expires_at_ms"],1704067200000ULL,4102444800000ULL)||!b["required"].is<bool>()||!b["authorization_scope"].is<JsonArrayConst>()||b["authorization_scope"].size()>8)return false;
 if(b["capability"]=="scheduler.drive"&&b["max_elapsed_ms"].as<unsigned>()+2000>b["timeout_ms"].as<unsigned>())return false;
 for(auto scope:b["authorization_scope"].as<JsonArrayConst>())if(!text(scope,128))return false;
 const char* c=b["capability"]|"";
 if(!strcmp(c,"scheduler.drive")||!strcmp(c,"service.reconcile"))return b["operation"]=="maintain"&&b["effect_class"]=="target_deduplicated"&&b["wake_semantics"]=="coalescible-durable-work"&&b["continuation"]=="target-cursor"&&b["response_max_bytes"].as<unsigned>()<=4096;
 if(strcmp(c,"service.health.observe")&&strcmp(c,"source.poll"))return false;
 if(b["effect_class"]!="repeatable_compute"||!b["latest_state"].is<bool>()||!integer(b["collection_bound"],1,128)||b["projection"].size()>16)return false;
 // Latest-state only. Lossless event feeds require a durable server cursor and
 // are explicitly outside this shipped observation contract.
 if(b["latest_state"]!=true||b["format"]!="json"||!b["projection"].is<JsonArrayConst>())return false;
 for(auto field:b["projection"].as<JsonArrayConst>())if(!text(field,64))return false;
 return (b["operation"]=="observe"||b["operation"]=="poll")&&integer(b["expected_status_min"],100,599)&&integer(b["expected_status_max"],b["expected_status_min"].as<unsigned>(),599);
}
inline bool configuration(JsonObjectConst c,JsonArrayConst endpoints,uint32_t contact){
 if(!keys(c,{"schema_version","revision","bindings","enabled_providers","retry","telemetry","update_policy"})||c["schema_version"]!=1||!integer(c["revision"],1,9007199254740991ULL)||measureJson(c)>8192||!c["bindings"].is<JsonArrayConst>()||c["bindings"].size()>8||!c["enabled_providers"].is<JsonArrayConst>()||c["enabled_providers"].size()>4)return false;
 if(!keys(c["retry"],{"initial_ms","maximum_ms","full_jitter"})||c["retry"]["initial_ms"]!=1000||c["retry"]["maximum_ms"]!=300000||c["retry"]["full_jitter"]!=true||!keys(c["telemetry"],{"interval_ms","diagnostic_bytes"})||!integer(c["telemetry"]["interval_ms"],15000,86400000)||c["telemetry"]["diagnostic_bytes"]!=16384||!keys(c["update_policy"],{"mode"})||c["update_policy"]["mode"]!="serial-owner")return false;
 Cadence cadences[8]{};size_t n=0;
 for(auto v:c["bindings"].as<JsonArrayConst>()){
  auto b=v.as<JsonObjectConst>();if(!binding(b))return false;
  for(size_t i=0;i<n;++i)if(c["bindings"][i]["binding_id"]==b["binding_id"])return false;
  const auto e=find(endpoints,"endpoint_policy_id",b["endpoint_policy_id"]);if(e.isNull()||e["capability"]!=b["capability"]||e["credential_ref"]!=b["credential_ref"])return false;
  for(auto scope:b["authorization_scope"].as<JsonArrayConst>()){bool allowed=false;for(auto permitted:e["authorization_scope"].as<JsonArrayConst>())allowed|=scope==permitted;if(!allowed)return false;}
  bool enabled=false;for(auto p:c["enabled_providers"].as<JsonArrayConst>())enabled|=p==b["capability"];if(!enabled)return false;
  cadences[n].interval=b["cadence_ms"];cadences[n].timeout=b["timeout_ms"];cadences[n++].required=b["required"];
 }
 // Reserve one5s control contact and one5s inventory refresh every90s.
 return schedulable(cadences,n,5000+(contact*5000+89999)/90000,contact);
}
inline bool relayPolicy(JsonObjectConst);
inline bool policy(JsonDocument& p){
 if(!keys(p.as<JsonObjectConst>(),{"schema_version","wifi","service","endpoints","configuration","low_water","provenance"},{"relay"})||p["schema_version"]!=1)return false;
 if(!keys(p["wifi"],{"ssid","credential_ref"})||!text(p["wifi"]["ssid"],32)||!text(p["wifi"]["credential_ref"],128))return false;
 auto s=p["service"].as<JsonObjectConst>();if(!keys(s,{"url","credential_ref","site_credential_ref","time_server","contact_interval_ms","contracts","providers"})||!https(s["url"])||!text(s["credential_ref"],128)||!text(s["site_credential_ref"],128,true)||!text(s["time_server"],253)||!integer(s["contact_interval_ms"],15000,30000)||s["contracts"].size()!=4||s["providers"].size()!=4)return false;
 for(auto pin:s["contracts"].as<JsonArrayConst>())if(!keys(pin,{"capability_id","api_version","contract_digest"})||pin["api_version"]!=1||!digest(pin["contract_digest"]))return false;
 for(auto pin:s["providers"].as<JsonArrayConst>())if(!keys(pin,{"capability_id","provider_id","revision","definition_digest","package_digest"})||!digest(pin["definition_digest"])||!digest(pin["package_digest"])||!integer(pin["revision"],1,9007199254740991ULL))return false;
 if(!p["endpoints"].is<JsonArray>()||p["endpoints"].size()>8)return false;
 size_t n=0;for(auto e:p["endpoints"].as<JsonArrayConst>()){
  if(!keys(e,{"endpoint_policy_id","url","method","capability","credential_ref","site_credential_ref","authorization_scope","maintenance_qualified"})||!text(e["endpoint_policy_id"],128)||!https(e["url"])||!text(e["credential_ref"],128,true)||!text(e["site_credential_ref"],128,true))return false;
  for(size_t i=0;i<n;++i)if(p["endpoints"][i]["endpoint_policy_id"]==e["endpoint_policy_id"])return false;++n;
  if(!e["authorization_scope"].is<JsonArrayConst>()||e["authorization_scope"].size()>8||!e["maintenance_qualified"].is<bool>())return false;for(auto scope:e["authorization_scope"].as<JsonArrayConst>())if(!text(scope,128))return false;
  bool wake=e["capability"]=="scheduler.drive"||e["capability"]=="service.reconcile";
  if(wake?(e["method"]!="POST"||e["maintenance_qualified"]!=true):e["method"]!="GET")return false;
 }
 auto low=p["low_water"].as<JsonObjectConst>();if(!keys(low,{"internal_free","internal_largest","dma_free","dma_largest","psram_free","psram_largest","stack_headroom"}))return false;for(auto kv:low)if(!integer(kv.value(),1,16777216))return false;
 return relayPolicy(p["relay"])&&configuration(p["configuration"],p["endpoints"],s["contact_interval_ms"]);
}
}

#include "RelayPolicy.h"
