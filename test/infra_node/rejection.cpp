#include "relay_harness.h"
static std::unique_ptr<Node> ready(JsonDocument& cfg){
 auto n=std::make_unique<Node>();init(*n,cfg);n->journal.state["credentials"][cfg["service"]["credential_ref"]|""]="participant-secret";
 assert(n->relayStep()==1);saved(*n);n->relayReady=true;n->relayIdentityReady=true;
 auto a=n->journal.state["relay"]["attachments"].to<JsonArray>().add<JsonObject>();a["id"]="enroll-attachment";a["channel"]="relay.enroll";a["cursor"]=5000;
 return n;
}
static void reject(Node& n,const char* outcome="not_applied",bool matched=true){
 JsonDocument response;response["protocol_version"]="1.0";response["operation_id"]=matched?(n.request["operation_id"]|""):"different-operation";response["status"]="rejected";response["code"]="INVALID_REQUEST";response["mutation_outcome"]=outcome;
 // Free-form server messages may contain sensitive input; never retain them.
 response["errors"][0]["message"]="private-invalid-value";
 n.net.outcome=0;n.net.info.status_code=400;std::string body;serializeJson(response,body);memcpy(n.net.body,body.c_str(),body.size()+1);n.net.size=body.size();n.phase=Phase::Receive;
}
int main(int argc,char** argv){
 assert(argc==2);std::ifstream file(argv[1]);std::string bytes{std::istreambuf_iterator<char>(file),{}};JsonDocument cfg;assert(parse(cfg,bytes.data(),bytes.size()));auto policy=cfg["relay"].to<JsonObject>();policy["schema_version"]=1;policy["relay_id"]="test-relay";policy["url"]="https://bus.example/v1/relay/call";policy["credential_ref"]="relay-token";policy["site_credential_ref"]="";
 auto n=ready(cfg);auto a=n->make(Action::Observe,"observation","publish");a["source_id"]="test:service-health";a["source_generation"]=1;a["observations"][0]["sequence"]=1;a["observations"][0]["data"]["private"]="private-invalid-value";assert(n->send(true)==1);saved(*n);const std::string original=n->request["operation_id"]|"";
 // A v1.4.1 pending entry survives reboot unchanged, then receives a definitive
 // rejection. No checkpoint or enrollment state is acknowledged by this path.
 n=reboot(*n,cfg);n->phase=Phase::Recover;assert(n->resume()==1&&n->action==Action::ReceiptLookup);n->receiptChecked=true;assert(n->resume()==1&&n->action==Action::Observe);assert(n->request["operation_id"]==original);
 reject(*n);assert(n->received()==1&&n->phase==Phase::Save);assert(n->journal.state["pending"].size()==0);assert(n->journal.state["checkpoints"].size()==0);assert(n->journal.state["protocol_rejections"]["items"][0]["operation_id"]==original);assert(n->journal.state["protocol_rejections"]["quarantined"]==false);saved(*n);
 std::string state;serializeJson(n->journal.state,state);assert(state.find("private-invalid-value")==std::string::npos);assert(n->journal.state["relay"]["attachments"][0]["cursor"]==5000);
 n=reboot(*n,cfg);n->relayHealthAt=0;assert(n->relayStep()==1&&n->action==Action::RelayHealth);assert(n->request["arguments"]["payload"]["last_error"]=="participant_INVALID_REQUEST");reply(*n,R"({"protocol_version":"relay/1","status":"completed","result":{"id":"health"}})");
 // Fresh observations after lost local checkpoints use verified wall time,
 // while accepted checkpoints keep their existing monotonically increasing sequence.
 n->bindingIndex=2;n->externalJob=false;n->net.outcome=0;n->net.info.status_code=200;strcpy(n->net.body,"{\"status\":\"ok\"}");n->net.size=strlen(n->net.body);assert(n->probeResult()==1);assert(n->request["arguments"]["observations"][0]["sequence"].as<uint64_t>()==n->net.wall(ticks)*1000);n->journal.state["pending"].to<JsonArray>();n->phase=Phase::Ready;n->journal.state["checkpoints"]["service-health"]["sequence"]=42;n->net.outcome=1;assert(n->probeResult()==1);assert(n->request["arguments"]["observations"][0]["sequence"]==43);n->journal.state["pending"].to<JsonArray>();
 // Ambiguous or mismatched rejections retain the exact original request.
 for(bool mismatch:{false,true}){n->phase=Phase::Ready;n->make(Action::Observe,"observation","publish");n->send(true);saved(*n);reject(*n,mismatch?"not_applied":"unknown",!mismatch);assert(n->received()<0&&n->journal.state["pending"].size()==1);n->journal.state["pending"].to<JsonArray>();}
 // A rejected execution request is isolated from relay health. Its job and all
 // other pending requests remain in custody; it cannot restart external work.
 n->phase=Phase::Ready;n->journal.state["job"]["attempt_id"]="attempt-1";n->journal.state["job"]["phase"]="running";n->make(Action::Stop,"execution","stop_report");n->send(true);saved(*n);auto extra=n->journal.state["pending"].as<JsonArray>().add<JsonObject>();extra["action"]=static_cast<unsigned>(Action::Observe);extra["request"]["operation_id"]="other-request";reject(*n);assert(n->received()==1);saved(*n);assert(n->journal.state["pending"].size()==1&&n->journal.state["pending"][0]["request"]["operation_id"]=="other-request");assert(n->journal.state["job"]["attempt_id"]=="attempt-1");assert(n->journal.state["protocol_rejections"]["quarantined"]==true);assert(n->advance()==1&&n->action==Action::RelayHealth);assert(!n->bootReady());
 n->phase=Phase::Stopped;n->dispose();puts("Terminal request recovery: exact non-application, durable redacted disposition, legacy replay, health, checkpoint reset, ambiguous custody and execution isolation PASS");
}
