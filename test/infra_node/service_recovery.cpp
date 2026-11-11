#include "relay_harness.h"
static unsigned feeds=0;
static int feed(void*){++feeds;return 0;}
static void response(Node& n,unsigned code,const char* body){
 n.net.token=0;n.net.outcome=0;n.net.info.status_code=code;
 n.net.size=strlen(body);memcpy(n.net.body,body,n.net.size+1);
 n.received();
}
static void enableRelay(Node& n){
 auto policy=n.config["relay"].to<JsonObject>();policy["schema_version"]=1;policy["relay_id"]="node";
 policy["url"]="https://bus.example/v1/relay/call";policy["credential_ref"]="relay-token";policy["site_credential_ref"]="";
 auto r=n.journal.state["relay"].to<JsonObject>();r["schema_version"]=1;r["subscriptions"].to<JsonArray>();
 r["identity"]["key_id"]="test-key";r["enrollments"].to<JsonArray>();
 r["deliveries"].to<JsonObject>();r["pending"]=nullptr;
 auto a=r["attachments"].to<JsonArray>().add<JsonObject>();a["id"]="attachment";a["channel"]="capabilities.events";a["cursor"]=0;
 n.relayReady=n.relayIdentityReady=true;
}
static void heartbeat(Node& n){
 n.relayHealthAt=0;n.relayRetryAt=0;
 assert(n.advance()==1&&n.action==Action::RelayHealth);
 assert(n.request["arguments"]["payload"]["last_error"]==n.serviceError);
 reply(n,R"({"protocol_version":"relay/1","status":"completed","result":{}})");
 assert(n.phase==Phase::Ready);
}
int main(int argc,char** argv){
 assert(argc==2);std::ifstream f(argv[1]);std::string bytes{std::istreambuf_iterator<char>(f),{}};
 JsonDocument cfg;assert(parse(cfg,bytes.data(),bytes.size()));
 auto n=std::make_unique<Node>();init(*n,cfg);n->loaded=true;
 n->recovery={1,sizeof(n->recovery),nullptr,feed,nullptr,nullptr};
 // A proxy may return HTML or an empty body, including on a request whose
 // effect is unknown. These must not latch the entire node offline forever.
 for(unsigned code:{502u,503u,504u,408u,429u,200u}){
  n->make(Action::Sync,"participant","sync");n->send(false);
  response(*n,code,"<html>temporary upstream failure</html>");
  assert(n->phase!=Phase::ProtocolBlocked&&n->phase!=Phase::AuthBlocked);
 }
 enableRelay(*n);
 // A successful independent heartbeat must not mask a continuing outage.
 const auto deadline=n->serviceRetryAt;const auto attempts=n->serviceFailures;
 heartbeat(*n);assert(n->serviceRetryAt==deadline&&n->serviceFailures==attempts&&n->serviceError[0]);
 // Unknown mutation outcome preserves the complete original request and
 // checks its receipt before any replay. Relay uses a separate journal slot.
 n->make(Action::Observe,"observation","publish");n->request["arguments"]["sequence"]=7;
 n->send(true);saved(*n);std::string pending;serializeJson(n->journal.state["pending"],pending);
 response(*n,502,"bad gateway");const auto retry=n->serviceRetryAt;
 heartbeat(*n);std::string after;serializeJson(n->journal.state["pending"],after);assert(pending==after);
 n->relayHealthAt=n->relayPollAt[0]=UINT64_MAX;ticks=retry-1;
 assert(n->advance()==1&&n->phase==Phase::Ready);ticks=retry;
 assert(n->advance()==1&&n->action==Action::ReceiptLookup);
 assert(n->request["arguments"]["lookup_operation_id"]==n->journal.state["pending"][0]["request"]["operation_id"]);
 // Two simulated hours of gateway errors: bounded retry rate, unchanged
 // saved custody and independent healthy bus delivery without any reboot.
 const auto started=ticks;unsigned heartbeats=0;
 while(ticks-started<7200000){
  response(*n,503,"");assert(n->serviceRetryAt-ticks<=60000&&n->serviceRetryAt-ticks>=5000);
  const auto due=n->serviceRetryAt;heartbeat(*n);++heartbeats;
  after.clear();serializeJson(n->journal.state["pending"],after);assert(pending==after);
  ticks=due;assert(n->advance()==1&&n->action==Action::ReceiptLookup);
 }
 assert(heartbeats>=120&&heartbeats<150);
 // Successful service traffic closes only its own retry circuit.
 n->journal.state["pending"].to<JsonArray>();n->durable=false;n->action=Action::Inventory;
 response(*n,200,R"({"protocol_version":"1.0","status":"completed","data":{"profile":"esp32s3-infrastructure/1"}})");
 assert(n->phase==Phase::Ready&&!n->serviceRetryAt&&!n->serviceFailures&&!n->serviceError[0]);
 // A real authentication rejection remains fail-closed for work while the
 // separately authorized relay reports why the participant is unavailable.
 n->action=Action::Sync;response(*n,401,"unauthorized");
 assert(n->serviceBlocked&&!n->bootReady());heartbeat(*n);assert(n->serviceBlocked);
 n->relayHealthAt=n->relayPollAt[0]=UINT64_MAX;ticks+=7200000;
 assert(n->advance()==1&&n->phase==Phase::Ready&&n->serviceBlocked);
 // A semantically invalid protocol result also isolates participant work.
 n->serviceBlocked=false;n->action=Action::Sync;
 response(*n,200,R"({"protocol_version":"wrong","status":"completed"})");
 assert(n->serviceBlocked);heartbeat(*n);
 // Storage corruption and retained cleanup still retain their original fence.
 n->action=Action::Sync;assert(n->block(Phase::StorageBlocked,"storage_corrupt")==INFRA_BLOCKED);
 assert(n->phase==Phase::StorageBlocked);
 n->phase=Phase::Stopped;n->dispose();
 puts("Service recovery: HTTP/body faults, 2h independent relay, bounded retries, original custody, auth/protocol isolation and storage fences PASS (simulated time)");
}
