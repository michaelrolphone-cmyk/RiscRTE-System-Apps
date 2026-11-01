#include "relay_harness.h"
int main(int argc,char** argv){
 assert(argc==2);std::ifstream file(argv[1]);std::string source((std::istreambuf_iterator<char>(file)),{});JsonDocument cfg;assert(!deserializeJson(cfg,source));
 for(const char* mode:{"explorer","sentinel","reconciler","continuity"}){
  cfg["configuration"]["mode"]=mode;assert(policy(cfg));Node n;init(n,cfg);n.configureCadence();
  for(unsigned i=0;i<n.bindings().size();++i){bool maintenance=i<2;bool allowed=!strcmp(mode,"explorer")||(!strcmp(mode,"sentinel")?!maintenance:maintenance);assert(n.bindingDisabled[i]==!allowed);n.bindingIndex=i;
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
