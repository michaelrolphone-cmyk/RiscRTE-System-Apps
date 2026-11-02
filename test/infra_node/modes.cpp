#include "relay_harness.h"
int main(int argc,char** argv){
 assert(argc==2);std::ifstream file(argv[1]);std::string source((std::istreambuf_iterator<char>(file)),{});JsonDocument cfg;assert(!deserializeJson(cfg,source));
 for(const char* mode:{"relay","explorer","sentinel","reconciler","continuity"}){
  cfg["configuration"]["mode"]=mode;assert(policy(cfg));Node n;init(n,cfg);n.configureCadence();
  for(unsigned i=0;i<n.bindings().size();++i){bool maintenance=i<2;bool allowed=strcmp(mode,"relay")&&(!strcmp(mode,"explorer")||(!strcmp(mode,"sentinel")?!maintenance:maintenance));assert(n.bindingDisabled[i]==!allowed);n.bindingIndex=i;
   if(allowed){assert(n.bindingStep(false)==1);assert(n.phase==Phase::Send);assert(n.action==(maintenance?Action::Maintain:Action::Probe));n.phase=Phase::Ready;}
   else {assert(n.bindingStep(false)==1);assert(n.phase==Phase::Ready);}
  }
  std::string disk;serializeJson(n.journal.state,disk);n.phase=Phase::Stopped;n.dispose();
 }
 cfg["configuration"]["mode"]="reconciler";{
  Node n;init(n,cfg);n.configureCadence();n.journal.state["credentials"]["scheduler-main-credential"]="test-maintenance";n.loaded=true;n.serviceBlocked=true;n.serviceRetryAt=ticks+60000;n.serviceFailures=3;strcpy(n.serviceError,"participant_unavailable");assert(n.advance()==1&&n.action==Action::Maintain);
  reply(n,R"({"protocol_version":"maintenance/1","status":"completed","data":{"coalescible":true,"target_epoch":"test","progress":{"items_examined":1,"items_advanced":1}}})");assert(n.serviceBlocked&&n.serviceFailures==3&&n.serviceRetryAt>ticks);assert(n.cadence[n.bindingIndex].lastCompleted==ticks);n.phase=Phase::Stopped;n.dispose();
 }
 // Allocation is locally scoped, durable, lease bound and reboot safe.
 cfg["allocation"]["automatic"]=true;{
  Node a;init(a,cfg);for(auto e:cfg["endpoints"].as<JsonArrayConst>())a.journal.state["credentials"][e["credential_ref"]|""]="local-test-token";
  a.configureCadence();assert(!a.roleReady());JsonDocument profiles;a.roleProfiles(profiles.to<JsonArray>());assert(profiles.size()==3); // no shared relay provisioned
  JsonDocument grant;auto g=grant.to<JsonObject>();g["protocol_version"]="infra-allocation/1";g["generation"]=1;g["profile_id"]="sentinel";g["mode"]="sentinel";g["demand_id"]="watch";g["services"].to<JsonArray>().add("service-health-endpoint");g["expires_at"]=a.net.wall(ticks)*1000+120000;g["lease_ms"]=120000;g["state"]="assigned";a.net.started=ticks;
  assert(a.roleGrant(g)&&a.roleGrantDirty&&!a.roleReady());assert(a.journal.state["allocation"]["state"]=="draining");
  a.journal.state["relay"]["enrollments"].to<JsonArray>().add<JsonObject>()["id"]="unresolved";assert(a.roleBusy()&&a.roleStep()==0);a.journal.state.remove("relay");
  assert(a.roleStep()==1);saved(a);assert(a.roleReady());assert(!strcmp(agentMode(a.configuration()),"sentinel"));assert(a.bindingDisabled[0]&&!a.bindingDisabled[2]);
  auto b=reboot(a,cfg);assert(!b->roleReady());b->net.started=ticks;assert(b->roleGrant(g)&&b->roleReady());
  g["generation"]=2;g["profile_id"]="reconciler";g["mode"]="reconciler";g["services"].to<JsonArray>().add("scheduler-main-endpoint");assert(b->roleGrant(g)&&!b->roleReady());assert(b->roleStep()==1);saved(*b);assert(b->roleReady()&&b->bindingDisabled[2]);
  g["profile_id"]="unprovisioned";assert(!b->roleGrant(g));g["profile_id"]="reconciler";
  ticks=b->roleLeaseUntil;assert(!b->roleReady());b->bindingIndex=0;assert(b->bindingStep(false)==1&&b->phase==Phase::Ready);
  b->net.started=ticks-120000;g["expires_at"]=b->net.wall(ticks)*1000+120000;assert(b->roleGrant(g)&&!b->roleReady()); // slow response cannot extend authority
  g["generation"]=1;assert(!b->roleGrant(g));b->phase=Phase::Stopped;b->dispose();
 }cfg.remove("allocation");
 // Mode inventory is five even when only three staffing demands exist.
 // A fully provisioned board advertises all five; dedicated Relay preserves
 // the original single-executor path and drains custody before reassignment.
 {
  JsonDocument rc;rc.set(cfg);rc["allocation"]["automatic"]=true;
  auto r=rc["relay"].to<JsonObject>();r["schema_version"]=1;r["relay_id"]="group";r["url"]="https://bus.example/v1/relay/call";r["credential_ref"]="relay-token";r["site_credential_ref"]="";r["monitor_credential_ref"]="monitor-token";r["shared"]["executor_id"]="primary";r["subscriptions"].to<JsonArray>();r["allowed_origins"].to<JsonArray>();r["webhook_credential_refs"].to<JsonArray>();
  assert(policy(rc));Node n;init(n,rc);for(auto e:rc["endpoints"].as<JsonArrayConst>())n.journal.state["credentials"][e["credential_ref"]|""]="endpoint-token";n.journal.state["credentials"]["relay-token"]="relay-secret";n.journal.state["credentials"]["monitor-token"]="monitor-secret";
  JsonDocument ps;n.roleProfiles(ps.to<JsonArray>());assert(ps.size()==5);
  for(const char* mode:{"relay","explorer","sentinel","reconciler","continuity"})assert(!find(ps.as<JsonArrayConst>(),"mode",mode).isNull());
  // Every nonempty subset, including all five, runs as one bounded schedule.
  // Duplicate bindings from Explorer/Reconciler/Continuity are never multiplied.
  JsonDocument mixed;auto mg=mixed.to<JsonObject>();mg["protocol_version"]="infra-allocation/2";mg["state"]="assigned";mg["expires_at"]=n.net.wall(ticks)*1000+120000;mg["lease_ms"]=120000;
  const char* names[]={"relay","explorer","sentinel","reconciler","continuity"};
  for(unsigned mask=1;mask<32;++mask){
   mg["generation"]=mask;auto selected=mg["assignments"].to<JsonArray>();
   for(unsigned i=0;i<5;++i)if(mask&(1<<i)){auto own=find(ps.as<JsonArrayConst>(),"mode",names[i]);auto a=selected.add<JsonObject>();a["demand_id"]=names[i];a["profile_id"]=names[i];a["mode"]=names[i];a["services"]=own["services"];}
   n.net.started=ticks;assert(n.roleGrant(mg)&&!n.roleReady());assert(n.roleStep()==1);saved(n);assert(n.roleReady());
   for(unsigned i=0;i<5;++i)assert(hasMode(n.configuration(),names[i])==bool(mask&(1<<i)));
   bool maintenance=mask&((1<<1)|(1<<3)|(1<<4)),observation=mask&((1<<1)|(1<<2));
   assert(n.configuration()["active_bindings"].size()==unsigned(2*maintenance+2*observation));
   for(unsigned i=0;i<n.bindings().size();++i)assert(n.bindingDisabled[i]==!(i<2?maintenance:observation));
   // Same generation can renew but cannot change its role-set membership.
   assert(n.roleGrant(mg));auto old=selected[0]["mode"].as<const char*>();std::string savedMode=old;selected[0]["mode"]="unknown";assert(!n.roleGrant(mg));selected[0]["mode"]=savedMode;
  }
  JsonDocument offer;n.roleOffer(offer.to<JsonObject>(),"scheduler.drive");assert(offer["activation_generation"]==31&&text(offer["activation_profile"],128));
  // The mixed schedule must actually service Sentinel and Relay in the same
  // configuration, rather than returning early forever from one role.
  {Node combined;init(combined,rc);combined.journal.state["credentials"]=n.journal.state["credentials"];combined.net.started=ticks;assert(combined.roleGrant(mg)&&combined.roleStep()==1);saved(combined);assert(combined.relayStep()==1);saved(combined);assert(combined.relayStep()==1&&combined.action==Action::RelayChallenge);std::string cid=combined.journal.state["relay"]["sentinel"]["id"]|"";std::string pending="{\"protocol_version\":\"relay/1\",\"status\":\"completed\",\"result\":{\"challenge_id\":\""+cid+"\",\"state\":\"pending\"}}";reply(combined,pending.c_str());saved(combined);assert(combined.relayStep()==1&&combined.action==Action::RelayLease);combined.action=Action::RelayChallengeStatus;combined.net.info.status_code=403;combined.net.outcome=0;assert(combined.relayReceived()==1);saved(combined);assert(!combined.relayBlocked&&combined.relayRetryAt==0&&combined.sentinelRetryAt>ticks);assert(combined.relayStep()==1&&combined.action==Action::RelayLease);combined.phase=Phase::Stopped;combined.dispose();}
  JsonDocument report;n.roleRequest(report.to<JsonObject>());assert(report["role_request"]["protocol_version"]=="infra-allocation/2"&&report["role_request"]["max_assignments"]==5&&report["role_request"]["report"]["profile_ids"].size()==5);
  Node restarted;init(restarted,rc);std::string snapshot;serializeJson(n.journal.state,snapshot);assert(parseJournal(restarted.journal.state,snapshot.data(),snapshot.size())&&restarted.journal.valid());assert(!restarted.roleReady());restarted.net.started=ticks;assert(restarted.roleGrant(mg)&&restarted.roleReady());restarted.phase=Phase::Stopped;restarted.dispose();
  mg["generation"]=32;mg["activate_after_ms"]=30000;mg["assignments"].as<JsonArray>().remove(4);assert(n.roleGrant(mg)&&n.roleStep()==0&&!n.roleReady());ticks+=30000;assert(n.roleStep()==1);saved(n);assert(n.roleReady());
  mg["generation"]=33;mg["activate_after_ms"]=0;mg["assignments"].as<JsonArray>().remove(3);n.journal.state["relay"]["deliveries"]["pending"]["cursor"]=1;assert(n.roleGrant(mg)&&n.roleStep()==0);n.journal.state["relay"]["deliveries"].clear();assert(n.roleStep()==1);saved(n);
  // Reset the test installation before the independent single-role cases.
  n.journal.state.remove("allocation");n.journal.state.remove("configuration");n.journal.state.remove("relay");n.roleLeaseUntil=0;
  n.journal.state["credentials"].remove("relay-token");ps.clear();n.roleProfiles(ps.to<JsonArray>());assert(find(ps.as<JsonArrayConst>(),"mode","relay").isNull());
  n.journal.state["credentials"]["relay-token"]="relay-secret";n.config["relay"].remove("shared");ps.clear();n.roleProfiles(ps.to<JsonArray>());assert(ps.size()==4);assert(find(ps.as<JsonArrayConst>(),"mode","relay")["features"][0]=="relay.deliver");
  JsonDocument target;auto g=target.to<JsonObject>();g["protocol_version"]="infra-allocation/1";g["generation"]=1;g["profile_id"]="relay";g["mode"]="relay";g["demand_id"]="delivery";g["services"].to<JsonArray>().add("relay:group");g["expires_at"]=n.net.wall(ticks)*1000+120000;g["lease_ms"]=120000;g["state"]="assigned";n.net.started=ticks;
  assert(n.roleGrant(g));assert(n.roleStep()==1);saved(n);assert(n.roleReady()&&n.relayMode()&&!n.sharedRelay());for(unsigned i=0;i<n.bindings().size();++i)assert(n.bindingDisabled[i]);
  assert(n.relayStep()==1);saved(n);assert(n.relayStep()==1&&n.action==Action::RelayConfig);n.phase=Phase::Ready;
  g["generation"]=2;g["profile_id"]="sentinel";g["mode"]="sentinel";g["services"].clear();g["services"].to<JsonArray>().add("service-health-endpoint");n.journal.state["relay"]["deliveries"]["pending"]["cursor"]=1;
  assert(n.roleGrant(g)&&!n.roleReady());assert(n.roleStep()==0&&!strcmp(agentMode(n.configuration()),"relay"));n.journal.state["relay"]["deliveries"].clear();assert(n.roleStep()==1);saved(n);assert(n.roleReady()&&!n.relayMode()&&!strcmp(agentMode(n.configuration()),"sentinel"));n.phase=Phase::Stopped;n.dispose();
 }
 {
  Node retry;init(retry,cfg);retry.journal.state["credentials"]["participant"]="participant-test";retry.loaded=true;auto a=retry.make(Action::Open,"participant","open_session");a["participant_id"]="test";retry.send(false);reply(retry,R"({"protocol_version":"1.0","status":"rejected","code":"REVISION_CONFLICT","mutation_outcome":"not_applied"})");assert(!retry.serviceBlocked&&retry.serviceRetryAt>ticks&&retry.phase==Phase::Ready);retry.phase=Phase::Stopped;retry.dispose();
 }
 cfg["configuration"]["mode"]="unknown";assert(!policy(cfg));cfg["configuration"]["mode"]=7;assert(!policy(cfg));cfg["configuration"].remove("mode");assert(policy(cfg));
 cfg["relay"]["schema_version"]=1;cfg["relay"]["relay_id"]="group";cfg["relay"]["url"]="https://bus.example/v1/relay/call";cfg["relay"]["credential_ref"]="relay-token";cfg["relay"]["site_credential_ref"]="";cfg["relay"]["shared"]["executor_id"]="backup";cfg["relay"]["subscriptions"].to<JsonArray>();cfg["relay"]["allowed_origins"].to<JsonArray>();cfg["relay"]["webhook_credential_refs"].to<JsonArray>();cfg["configuration"]["mode"]="continuity";
 Node n;init(n,cfg);assert(n.relayStep()==1);saved(n);assert(n.relayStep()==0);ticks+=30000;assert(n.relayStep()==1&&n.action==Action::RelayLease);
 reply(n,R"({"protocol_version":"relay/1","status":"completed","result":{"acquired":false,"retry_ms":10000}})");assert(!n.relayFence&&n.phase==Phase::Ready);ticks+=10000;assert(n.relayStep()==1&&n.action==Action::RelayLease);
 reply(n,R"({"protocol_version":"relay/1","status":"completed","result":{"acquired":true,"executor_id":"backup","fence":2,"lease_ms":120000,"custody":null}})");saved(n);assert(n.relayFence==2);assert(n.relayStep()==1&&n.action==Action::RelayCheckpoint);
 reply(n,R"({"protocol_version":"relay/1","status":"completed","result":{"saved":true,"fence":2}})");assert(headers.count("Authorization"));JsonDocument body;deserializeJson(body,sent);assert(body["lease"]["fence"]==2);
 // Expired lease never dispatches even a previously prepared HTTP request.
 n.action=Action::RelayPoll;n.phase=Phase::Send;ticks=n.relayLeaseUntil;assert(n.transmit()==1&&n.phase==Phase::Ready&&n.relayFence==0);
 n.phase=Phase::Stopped;n.dispose();
 cfg["relay"].remove("shared");cfg["configuration"]["mode"]="sentinel";Node s;init(s,cfg);
 assert(s.relayStep()==1);saved(s);assert(s.relayStep()==1&&s.action==Action::RelayChallenge);const char* id=s.journal.state["relay"]["sentinel"]["id"];std::string challenge=id;
 std::string pending="{\"protocol_version\":\"relay/1\",\"status\":\"completed\",\"result\":{\"challenge_id\":\""+challenge+"\",\"state\":\"pending\"}}";
 reply(s,pending.c_str());saved(s);assert(s.journal.state["relay"]["sentinel"]["result"].isNull());ticks+=15000;assert(s.relayStep()==1&&s.action==Action::RelayChallengeStatus);
 std::string verified="{\"protocol_version\":\"relay/1\",\"status\":\"completed\",\"result\":{\"challenge_id\":\""+challenge+"\",\"state\":\"verified\"}}";reply(s,verified.c_str());saved(s);assert(s.relayStep()==1&&s.action==Action::Observe&&s.phase==Phase::Save);assert(s.journal.state["pending"][0]["request"]["arguments"]["observations"][0]["data"]["healthy"]==true);assert(s.journal.valid());s.phase=Phase::Stopped;s.dispose();
 puts("Modes: allocation scope/drain/reboot/expiry/conflict retry, admission/default/rejection, standby delay/contention/fenced checkpoint/expiry, Sentinel acceptance-vs-receipt and durable observation PASS");
}
