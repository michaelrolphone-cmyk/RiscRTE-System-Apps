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
 auto explicitProfiles=config["allocation"]["profiles"].as<JsonArrayConst>();
 for(unsigned i=0;i<(explicitProfiles.isNull()?4:explicitProfiles.size());++i){
  const char* modes[]={"explorer","sentinel","reconciler","continuity"};auto source=explicitProfiles[i].as<JsonObjectConst>();const char* mode=source.isNull()?modes[i]:(source["mode"]|"");
  auto p=out.add<JsonObject>();p["profile_id"]=source.isNull()?mode:(source["profile_id"]|"");p["mode"]=mode;auto ids=p["binding_ids"].to<JsonArray>();auto services=p["services"].to<JsonArray>();auto features=p["features"].to<JsonArray>();bool eligible=true;
  auto addUnique=[](JsonArray a,JsonVariantConst value){for(auto x:a)if(x==value)return;a.add(value);};
  for(auto b:bindings()){
   bool selected=source.isNull()?modeAllows(p,b["capability"]|""):false;
   if(!source.isNull())for(auto id:source["binding_ids"].as<JsonArrayConst>())selected|=id==b["binding_id"];
   if(!selected)continue;if(!usable(b)){if(!source.isNull())eligible=false;continue;}
   ids.add(b["binding_id"]);addUnique(services,b["endpoint_policy_id"]);addUnique(features,b["capability"]);
  }
  if(!strcmp(mode,"sentinel")&&monitor){features.add("relay.roundtrip");char id[129];snprintf(id,sizeof(id),"relay:%.122s",relay["relay_id"]|"");services.add(id);}
  if((!strcmp(mode,"continuity")||!strcmp(mode,"explorer"))&&shared){features.add("relay.shared");char id[129];snprintf(id,sizeof(id),"relay:%.122s",relay["relay_id"]|"");services.add(id);}
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
inline void Node::roleRequest(JsonObject args){
 if(config["allocation"].isNull())return;
 auto a=args["role_request"].to<JsonObject>();a["protocol_version"]="infra-allocation/1";a["automatic"]=automaticRole();a["available"]=capacity()&&!drain&&!serviceBlocked&&journal.state["protocol_rejections"]["quarantined"]!=true;
 auto profiles=a["profiles"].to<JsonArray>();roleProfiles(profiles);for(auto p:profiles)p.as<JsonObject>().remove("binding_ids");
 auto state=journal.state["allocation"];auto r=a["report"].to<JsonObject>();r["generation"]=state["target"]["generation"]|0ULL;r["profile_id"]=state["target"]["profile_id"]|"";r["state"]=roleReady()?"applied":(state["state"]=="draining"?"draining":"idle");
 if(automaticRole()&&state["state"]=="draining"&&roleBusy())r["state"]="blocked";
 if(!automaticRole())r["state"]="idle";
}
inline bool Node::roleGrant(JsonObjectConst g){
 if(!automaticRole())return true;
 if(g.isNull()){roleLeaseUntil=0;return true;}
 if(!keys(g,{"protocol_version","generation","demand_id","profile_id","mode","services","expires_at","lease_ms","state"})||g["protocol_version"]!="infra-allocation/1"||!integer(g["generation"],1,9007199254740991ULL)||!integer(g["lease_ms"],0,120000)||!integer(g["expires_at"],1,9007199254740991ULL)||!text(g["profile_id"],128,true)||!text(g["demand_id"],128,true)||!g["services"].is<JsonArrayConst>()||g["services"].size()>8)return false;
 JsonDocument candidates;roleProfiles(candidates.to<JsonArray>());auto p=find(candidates.as<JsonArrayConst>(),"profile_id",g["profile_id"]|"");
 if(g["state"]=="assigned"){if(p.isNull()||g["mode"]!=p["mode"])return false;for(auto service:g["services"].as<JsonArrayConst>()){bool found=false;for(auto own:p["services"].as<JsonArrayConst>())found|=own==service;if(!found)return false;}}
 else if(g["state"]!="idle"||g["profile_id"]!=""||g["mode"]!="")return false;
 auto a=journal.state["allocation"].as<JsonObject>();if(a.isNull())a=journal.state["allocation"].to<JsonObject>();
 // A reboot starts with no RAM lease. Same-target refresh can renew authority,
 // but a target change first commits a draining intent before changing config.
 if(g["generation"].as<uint64_t>()<(a["target"]["generation"]|0ULL))return false;
 bool changed=a["target"]["generation"]!=g["generation"]||a["target"]["profile_id"]!=g["profile_id"]||a["target"]["demand_id"]!=g["demand_id"];
 const uint64_t ttl=g["lease_ms"].as<uint64_t>();roleLeaseUntil=ttl>2000?net.started+ttl-2000:0;
 const uint64_t wall=net.wall(now())*1000,expiry=g["expires_at"].as<uint64_t>();if(!wall||expiry<=wall)roleLeaseUntil=0;else if(roleLeaseUntil>now()+expiry-wall-((expiry-wall)>2000?2000:0))roleLeaseUntil=now()+expiry-wall-((expiry-wall)>2000?2000:0);
 if(changed){a["target"]=g;a["state"]="draining";roleGrantDirty=true;inventoryNeeded=true;}
 return true;
}
inline int Node::roleStep(){
 if(!automaticRole())return 0;
 auto a=journal.state["allocation"].as<JsonObject>();
 if(!roleReady()&&!journal.state["job"].isNull()&&journal.state["job"]["phase"]!="awaiting_acceptance")return stopJob("role_handoff_or_lease_expired");
 if(a["state"]!="draining"||roleBusy()||roleLeaseUntil<=now())return 0;
 JsonDocument candidates;roleProfiles(candidates.to<JsonArray>());auto p=find(candidates.as<JsonArrayConst>(),"profile_id",a["target"]["profile_id"]|"");
 if(!journal.state["configuration"].is<JsonObject>())journal.state["configuration"]=config["configuration"];
 auto c=journal.state["configuration"].as<JsonObject>();c["active_bindings"].to<JsonArray>();
 if(a["target"]["state"]=="assigned"&&!p.isNull()){c["mode"]=p["mode"];c["active_bindings"]=p["binding_ids"];a["state"]="applied";}else a["state"]="idle";
 configureCadence();inventoryNeeded=true;nextSync=0;relayFence=relayLeaseUntil=0;relayCheckpointHash[0]=0;relayReady=relayBlocked=false;relayLeaseRetryAt=0;
 return persist(Phase::Ready);
}
}
