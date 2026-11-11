#pragma once
// Local profiles authorize scope. The coordinator selects an identity; it never
// supplies endpoint URLs, callback destinations or credentials.
namespace Infra {
inline bool Node::roleProfiles(JsonArray out)const{
 auto usable=[&](JsonObjectConst b){auto e=find(config["endpoints"],"endpoint_policy_id",b["endpoint_policy_id"]|"");if(e.isNull()||net.wall(now())*1000>=b["expires_at_ms"].as<uint64_t>())return false;for(const char* k:{"credential_ref","site_credential_ref"})if((e[k]|"")[0]&&!credential(e[k])[0])return false;return true;};
 auto relay=config["relay"].as<JsonObjectConst>();const bool siteReady=!(relay["site_credential_ref"]|"")[0]||credential(relay["site_credential_ref"])[0];const bool monitor=!relay.isNull()&&siteReady&&credential(relay["monitor_credential_ref"]|"")[0];
 bool shared=sharedRelay()&&credential(relay["credential_ref"]|"")[0];
 for(auto sub:relay["subscriptions"].as<JsonArrayConst>()){if(sub["deduplicates"]!=true)shared=false;for(const char* ref:{sub["auth"]["credential_ref"]|"",sub["signing_credential_ref"]|""})if(*ref&&!credential(ref)[0])shared=false;}
 if((relay["site_credential_ref"]|"")[0]&&!credential(relay["site_credential_ref"])[0])shared=false;
 // A dedicated Relay can preserve the original single-executor enrollment
 // path. Shared Relay profiles still require fenced, recoverable custody.
 const bool delivery=!relay.isNull()&&relayPolicy(relay)&&siteReady&&credential(relay["credential_ref"]|"")[0]&&(!sharedRelay()||shared);
 auto explicitProfiles=config["allocation"]["profiles"].as<JsonArrayConst>();
 for(unsigned i=0;i<(explicitProfiles.isNull()?5:explicitProfiles.size());++i){
  const char* modes[]={"relay","explorer","sentinel","reconciler","continuity"};auto source=explicitProfiles[i].as<JsonObjectConst>();const char* mode=source.isNull()?modes[i]:(source["mode"]|"");
  auto p=out.add<JsonObject>();p["profile_id"]=source.isNull()?mode:(source["profile_id"]|"");p["mode"]=mode;auto ids=p["binding_ids"].to<JsonArray>();auto services=p["services"].to<JsonArray>();auto features=p["features"].to<JsonArray>();bool eligible=true;
  auto addUnique=[](JsonArray a,auto value){for(auto x:a)if(x==value)return;a.add(value);};
  for(auto b:bindings()){
   bool selected=source.isNull()?modeAllows(p,b["capability"]|""):false;
   if(!source.isNull())for(auto id:source["binding_ids"].as<JsonArrayConst>())selected|=id==b["binding_id"];
   if(!selected)continue;if(!usable(b)){if(!source.isNull())eligible=false;continue;}
   ids.add(b["binding_id"]);addUnique(services,b["endpoint_policy_id"]);addUnique(features,b["capability"]);
  }
  if(!strcmp(mode,"sentinel")&&monitor){features.add("relay.roundtrip");char id[129];snprintf(id,sizeof(id),"relay:%.122s",relay["relay_id"]|"");services.add(id);}
  if(!strcmp(mode,"relay")&&delivery){features.add("relay.deliver");char id[129];snprintf(id,sizeof(id),"relay:%.122s",relay["relay_id"]|"");addUnique(services,id);}
  if((!strcmp(mode,"relay")||!strcmp(mode,"continuity")||!strcmp(mode,"explorer"))&&shared){features.add("relay.shared");char id[129];snprintf(id,sizeof(id),"relay:%.122s",relay["relay_id"]|"");addUnique(services,id);}
  if(!strcmp(mode,"relay")&&!delivery)eligible=false;
  if(!strcmp(mode,"continuity")&&!shared)eligible=false;
  if(!source.isNull()){
   // Features must be actually implemented and provisioned on this board.
   for(auto feature:source["features"].as<JsonArrayConst>()){bool found=false;for(auto f:features)found|=f==feature;if(!found)eligible=false;}
   p["services"]=source["services"];p["features"]=source["features"];
  }
  if(!eligible||(!ids.size()&&!features.size())||services.size()>8||features.size()>8)out.remove(out.size()-1);
 }
 return true;
}
// Materialize the union locally. No remote endpoint, credential or arbitrary
// binding can enter a role set, and shared bindings execute only once.
inline bool Node::roleConfiguration(JsonObjectConst target,JsonObject out)const{
 out.set(configuration());out.remove("active_modes");auto ids=out["active_bindings"].to<JsonArray>();auto modes=out["active_modes"].to<JsonArray>();
 JsonDocument candidates;roleProfiles(candidates.to<JsonArray>());
 auto addUnique=[](JsonArray a,JsonVariantConst v){for(auto old:a)if(old==v)return;a.add(v);};
 auto add=[&](JsonObjectConst assignment){auto p=find(candidates.as<JsonArrayConst>(),"profile_id",assignment["profile_id"]|"");if(p.isNull()||assignment["mode"]!=p["mode"]||!assignment["services"].is<JsonArrayConst>()||assignment["services"].size()>8)return false;for(auto service:assignment["services"].as<JsonArrayConst>()){bool found=false;for(auto own:p["services"].as<JsonArrayConst>())found|=own==service;if(!found)return false;}addUnique(modes,p["mode"]);for(auto id:p["binding_ids"].as<JsonArrayConst>())addUnique(ids,id);return true;};
 if(target["state"]=="assigned"){
  if(target["protocol_version"]=="infra-allocation/2"){for(auto a:target["assignments"].as<JsonArrayConst>())if(!add(a.as<JsonObjectConst>()))return false;}
  else if(!add(target))return false;
 }
 if(modes.size()>1)out["mode"]="mixed";
 else {if(modes.size())out["mode"]=modes[0];else out["mode"]="relay";out.remove("active_modes");}
 return Infra::configuration(out,config["endpoints"],config["service"]["contact_interval_ms"]);
}
inline void Node::roleOffer(JsonObject offer,const char* capability)const{
 auto target=journal.state["allocation"]["target"].as<JsonObjectConst>();if(!automaticRole()||target["protocol_version"]!="infra-allocation/2")return;
 offer["activation_generation"]=target["generation"];offer["activation_profile"]="";
 JsonDocument candidates;roleProfiles(candidates.to<JsonArray>());
 for(auto a:target["assignments"].as<JsonArrayConst>()){auto p=find(candidates.as<JsonArrayConst>(),"profile_id",a["profile_id"]|"");for(auto feature:p["features"].as<JsonArrayConst>())if(feature==capability){offer["activation_profile"]=a["profile_id"];return;}}
}
inline void Node::roleRequest(JsonObject args){
 if(config["allocation"].isNull())return;
 const bool multi=mixedAllocation();auto a=args["role_request"].to<JsonObject>();a["protocol_version"]=multi?"infra-allocation/2":"infra-allocation/1";a["automatic"]=automaticRole();a["available"]=capacity()&&!drain&&!serviceBlocked&&journal.state["protocol_rejections"]["quarantined"]!=true;
 if(multi)a["max_assignments"]=config["allocation"]["max_assignments"]|5;
 auto profiles=a["profiles"].to<JsonArray>();roleProfiles(profiles);for(auto p:profiles)p.as<JsonObject>().remove("binding_ids");
 auto state=journal.state["allocation"];auto r=a["report"].to<JsonObject>();r["generation"]=state["target"]["generation"]|0ULL;
 if(multi){auto ids=r["profile_ids"].to<JsonArray>();for(auto selected:state["target"]["assignments"].as<JsonArrayConst>()){bool found=false;for(auto id:ids)found|=id==selected["profile_id"];if(!found)ids.add(selected["profile_id"]);}}
 else r["profile_id"]=state["target"]["profile_id"]|"";
 r["state"]=roleReady()?"applied":(state["state"]=="draining"?"draining":"idle");
 if(automaticRole()&&state["state"]=="draining"&&roleBusy())r["state"]="blocked";
 if(!automaticRole())r["state"]="idle";
}
inline bool Node::roleGrant(JsonObjectConst g){
 if(!automaticRole())return true;
 if(g.isNull()){roleLeaseUntil=0;return true;}
 if(measureJson(g)>4000)return false;
 const bool multi=g["protocol_version"]=="infra-allocation/2";
 if(multi){
  if(!mixedAllocation()||!keys(g,{"protocol_version","generation","assignments","expires_at","lease_ms","state"},{"activate_after_ms"})||!g["assignments"].is<JsonArrayConst>()||g["assignments"].size()>(config["allocation"]["max_assignments"]|5))return false;
  unsigned n=0;for(auto a:g["assignments"].as<JsonArrayConst>()){if(!keys(a,{"demand_id","profile_id","mode","services"})||!text(a["demand_id"],128)||!text(a["profile_id"],128))return false;for(unsigned j=0;j<n;++j)if(g["assignments"][j]["demand_id"]==a["demand_id"])return false;++n;}
  if((g["state"]=="assigned")!=bool(g["assignments"].size())||(g["state"]!="assigned"&&g["state"]!="idle"))return false;
  if(!g["activate_after_ms"].isNull()&&!integer(g["activate_after_ms"],0,120000))return false;
 }else{
  if(!keys(g,{"protocol_version","generation","demand_id","profile_id","mode","services","expires_at","lease_ms","state"})||g["protocol_version"]!="infra-allocation/1"||!text(g["profile_id"],128,true)||!text(g["demand_id"],128,true)||!g["services"].is<JsonArrayConst>()||g["services"].size()>8)return false;
  if(g["state"]!="assigned"&&(g["state"]!="idle"||g["profile_id"]!=""||g["mode"]!=""))return false;
 }
 if(!integer(g["generation"],1,9007199254740991ULL)||!integer(g["lease_ms"],0,120000)||!integer(g["expires_at"],1,9007199254740991ULL))return false;
 JsonDocument next;if(!roleConfiguration(g,next.to<JsonObject>()))return false;
 auto a=journal.state["allocation"].as<JsonObject>();if(a.isNull())a=journal.state["allocation"].to<JsonObject>();
 if(g["generation"].as<uint64_t>()<(a["target"]["generation"]|0ULL))return false;
 // Equal generations may renew time, but cannot silently replace authority.
 bool changed=a["target"]["generation"]!=g["generation"];
 if(!changed)for(const char* key:{"protocol_version","state","assignments","profile_id","demand_id","mode","services"})if(a["target"][key]!=g[key])return false;
 const uint64_t ttl=g["lease_ms"].as<uint64_t>();roleLeaseUntil=ttl>2000?net.started+ttl-2000:0;
 roleNotBefore=now()+(g["activate_after_ms"]|0ULL);
 const uint64_t wall=net.wall(now())*1000,expiry=g["expires_at"].as<uint64_t>();if(!wall||expiry<=wall)roleLeaseUntil=0;else if(roleLeaseUntil>now()+expiry-wall-((expiry-wall)>2000?2000:0))roleLeaseUntil=now()+expiry-wall-((expiry-wall)>2000?2000:0);
 if(changed){a["target"]=g;a["state"]="draining";roleGrantDirty=true;inventoryNeeded=true;}
 return true;
}
inline int Node::roleStep(){
 if(!automaticRole())return 0;
 auto a=journal.state["allocation"].as<JsonObject>();
 if(!roleReady()&&!journal.state["job"].isNull()&&!hardwareCapability(journal.state["job"]["spec"]["capability"]["capability_id"]|"")&&journal.state["job"]["phase"]!="awaiting_acceptance")return stopJob("role_handoff_or_lease_expired");
 if(a["state"]!="draining"||roleBusy()||roleLeaseUntil<=now()||now()<roleNotBefore)return 0;
 JsonDocument next;if(!roleConfiguration(a["target"],next.to<JsonObject>())){roleLeaseUntil=0;return 0;}
 journal.state["configuration"]=next;a["state"]=a["target"]["state"]=="assigned"?"applied":"idle";
 configureCadence();inventoryNeeded=true;nextSync=0;relayFence=relayLeaseUntil=0;relayCheckpointHash[0]=0;relayReady=relayBlocked=false;relayLeaseRetryAt=0;
 return persist(Phase::Ready);
}
}
