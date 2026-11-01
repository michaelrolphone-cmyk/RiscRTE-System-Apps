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
 puts("Modes: admission/default/rejection, standby delay/contention/fenced checkpoint/expiry, Sentinel acceptance-vs-receipt and durable observation PASS");
}
