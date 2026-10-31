#include "Node.h"
#ifdef NDEBUG
#error Relay tests require assertions
#endif
#include <cassert>
#include <fstream>
#include <string>
#include <map>
#include <memory>
using namespace Infra;
static uint64_t ticks=1000;static std::string sent,url;static std::map<std::string,std::string> headers;static unsigned posts=0;
static uint64_t clockNow(void*){return ticks;}
static int openHttp(void*,const risc_http_request_v2* r,uint64_t* token){assert(r->method==RISC_HTTP_POST);url=r->url;sent.assign(static_cast<const char*>(r->body),r->body_bytes);headers.clear();for(unsigned i=0;i<r->header_count;++i)headers[r->headers[i].name]=r->headers[i].value;*token=1;if(url=="https://hooks.example/notify")++posts;return 0;}
static void saved(Node& n){assert(n.phase==Phase::Save);assert(n.journal.valid());assert(measureJson(n.journal.state)<StateMax-32);n.phase=n.afterSave;n.journal.seq=n.saveSequence;}
static void reply(Node& n,const char* body,unsigned code=200){assert(n.phase==Phase::Send);assert(n.transmit()==1);assert(n.phase==Phase::Receive);n.net.token=0;n.net.outcome=0;n.net.info.status_code=code;n.net.size=strlen(body);memcpy(n.net.body,body,n.net.size+1);assert(n.received()>=0);}
static void init(Node& n,JsonDocument& cfg){assert(n.initialize());n.config.set(cfg);n.clock={1,sizeof(n.clock),nullptr,clockNow};n.net.utc=1793400000;n.net.utcAt=ticks;n.net.api.open=openHttp;n.phase=Phase::Ready;assert(!deserializeJson(n.journal.state,R"({"schema_version":1,"installation_id":"test","credential_generation":1,"credentials":{"relay-token":"xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx","hook-token":"header-secret","hook-sign":"signing-secret"},"participant_id":"test","cursor":0,"pending":[],"job":null,"checkpoints":{},"configuration_revision":1})"));strcpy(n.boot,"test-boot");}
static void receive(Node& n,const char* event){n.relayHealthAt=ticks+180000;n.relayRetryAt=0;for(auto& at:n.relayPollAt)at=ticks+30000;n.relayPollAt[0]=0;assert(n.relayStep()==1&&n.action==Action::RelayPoll);reply(n,event);saved(n);}
static std::unique_ptr<Node> reboot(Node& old,JsonDocument& cfg){std::string disk;serializeJson(old.journal.state,disk);auto n=std::make_unique<Node>();init(*n,cfg);assert(parse(n->journal.state,disk.data(),disk.size())&&n->journal.valid());n->relayReady=true;n->relayHealthAt=ticks+180000;old.phase=Phase::Stopped;old.dispose();return n;}
static const char* event=R"({"protocol_version":"relay/1","status":"completed","result":{"events":[{"id":"message-1","seq":4,"channel":"capabilities.events","type":"decision.accepted","version":1,"kind":"event","created":1793400000000,"correlation":"grok","fields":{"source.business_key":"grok-worker-1:test"},"payload":{"answer":"yes"}}],"event_cursor":4}})";
int main(int argc,char** argv){assert(argc==2);std::ifstream f(argv[1]);std::string bytes{std::istreambuf_iterator<char>(f),{}};JsonDocument cfg;assert(parse(cfg,bytes.data(),bytes.size()));auto r=cfg["relay"].to<JsonObject>();r["schema_version"]=1;r["relay_id"]="test-relay";r["url"]="https://bus.example/v1/relay/call";r["credential_ref"]="relay-token";r["site_credential_ref"]="";r["allowed_origins"].to<JsonArray>().add("https://hooks.example");
 r["webhook_credential_refs"].to<JsonArray>().add("hook-token");r["webhook_credential_refs"].as<JsonArray>().add("hook-sign");
 auto s=r["subscriptions"].to<JsonArray>().add<JsonObject>();s["id"]="grok";s["channel"]="capabilities.events";s["enabled"]=true;s["url"]="https://hooks.example/notify";s["auth"]["header"]="X-Webhook-Token";s["auth"]["credential_ref"]="hook-token";s["signing_credential_ref"]="hook-sign";s["match"]["types"].to<JsonArray>().add("decision.accepted");auto filter=s["match"]["filters"].to<JsonArray>().add<JsonObject>();filter["path"]="source.business_key";filter["op"]="prefix";filter["value"]="grok-worker-1:";assert(policy(cfg));
 s["url"]="http://hooks.example/notify";assert(!policy(cfg));s["url"]="https://evil.example/notify";assert(!policy(cfg));s["url"]="https://hooks.example/notify";s["auth"]["header"]="Content-Length";assert(!policy(cfg));s["auth"]["header"]="X-Webhook-Token";
 auto n=std::make_unique<Node>();init(*n,cfg);assert(n->relayStep()==1);saved(*n);assert(n->relayStep()==1&&n->action==Action::RelayConfig);reply(*n,R"({"protocol_version":"relay/1","status":"completed","result":{"relay_id":"test-relay","attachments":[{"id":"attachment","channel":"capabilities.events"}]}})");saved(*n);
 receive(*n,event);assert(posts==0&&n->journal.state["relay"]["pending"]["targets"].size()==1);
 // A reboot before dispatch must recover the durable original event.
 n=reboot(*n,cfg);assert(n->relayStep()==1&&n->action==Action::RelayPost);reply(*n,"",204);assert(posts==1&&n->phase==Phase::Save);assert(headers["Idempotency-Key"]=="message-1"&&headers["X-Webhook-Token"]=="header-secret");assert(headers["X-Relay-Signature"].size()==71);assert(sent.find("header-secret")==std::string::npos&&sent.find("signing-secret")==std::string::npos);
 // Independently verify the exact signed bytes with Python's standard HMAC.
 {std::ofstream out("/tmp/infra-relay-hmac-vector.json");JsonDocument v;v["timestamp"]=headers["X-Relay-Timestamp"];v["signature"]=headers["X-Relay-Signature"];v["body"]=sent;std::string b;serializeJson(v,b);out<<b;}
 saved(*n);n=reboot(*n,cfg);assert(n->relayStep()==1&&n->action==Action::RelayAck);assert(posts==1);reply(*n,R"({"protocol_version":"relay/1","status":"completed","result":{"acknowledged":true}})");saved(*n);assert(n->journal.state["relay"]["attachments"][0]["cursor"]==4&&n->journal.state["relay"]["pending"].isNull());
 // Ambiguous POST outcome repeats the same idempotency key; five failures lead
 // to a durable dead letter before the bus event is acknowledged.
 n->journal.state["relay"]["attachments"][0]["cursor"]=0;receive(*n,event);const uint64_t delays[]={5000,30000,120000,600000};
 for(unsigned i=0;i<5;++i){assert(n->relayStep()==1&&n->action==Action::RelayPost);reply(*n,"error",500);saved(*n);assert(n->journal.state["relay"]["pending"]["attempts"]==i+1);if(i<4){const auto before=posts;assert(n->relayStep()==0);assert(posts==before);ticks+=delays[i];n->relayHealthAt=ticks+180000;}else ticks+=5000;}
 assert(n->relayStep()==1&&n->action==Action::RelayDeadletter);assert(n->request["arguments"]["channel"]=="relay.deadletter");assert(measureJson(n->request)<4096);reply(*n,R"({"protocol_version":"relay/1","status":"completed","result":{"id":"dead"}})");saved(*n);n=reboot(*n,cfg);assert(n->relayStep()==1&&n->action==Action::RelayAck);
 // Health is still emitted during a long retry wait.
 n->phase=Phase::Ready;n->journal.state["relay"]["pending"]["index"]=0;n->journal.state["relay"]["pending"]["not_before_ms"]=n->net.wall(ticks)*1000+600000;n->relayHealthAt=0;assert(n->relayStep()==1&&n->action==Action::RelayHealth);assert(n->request["arguments"]["payload"]["queue_depth"]==1);
 // Lose power after remote 2xx but before saving local success: replay exactly
 // the same delivery identity, leaving receiver deduplication to settle effect.
 n->phase=Phase::Ready;n->journal.state["relay"]["pending"]=nullptr;n->journal.state["relay"]["attachments"][0]["cursor"]=0;receive(*n,event);std::string committed;serializeJson(n->journal.state,committed);
 assert(n->relayStep()==1&&n->action==Action::RelayPost);reply(*n,"",204);const auto originalBody=sent;const auto originalKey=headers["Idempotency-Key"];assert(n->phase==Phase::Save);
 auto power=std::make_unique<Node>();init(*power,cfg);assert(parse(power->journal.state,committed.data(),committed.size()));power->relayReady=true;power->relayHealthAt=ticks+180000;n->phase=Phase::Stopped;n->dispose();n=std::move(power);
 assert(n->relayStep()==1&&n->action==Action::RelayPost);reply(*n,"",204);assert(sent==originalBody&&headers["Idempotency-Key"]==originalKey);saved(*n);
 // An incoming subscription must not select the high-privilege bus credential
 // merely because that secret exists on the node.
 JsonDocument candidate;candidate.set(cfg["relay"]);candidate["subscriptions"][0]["auth"]["credential_ref"]="relay-token";assert(!relayPolicy(candidate.as<JsonObjectConst>()));
 // Exercise the actual control-message path: apply a durable two-target table,
 // reject a later origin escape, and preserve completed-target progress on reboot.
 n->phase=Phase::Ready;n->journal.state["relay"]["pending"]=nullptr;auto control=n->journal.state["relay"]["attachments"].as<JsonArray>().add<JsonObject>();control["id"]="control";control["channel"]="relay.subscriptions";control["cursor"]=0;
 auto update=[&](unsigned revision,bool invalid){JsonDocument doc;doc["protocol_version"]="relay/1";doc["status"]="completed";doc["result"]["event_cursor"]=revision;auto e=doc["result"]["events"].to<JsonArray>().add<JsonObject>();e["id"]="control-message";e["seq"]=revision;e["channel"]="relay.subscriptions";e["type"]="relay.subscriptions";e["kind"]="event";e["payload"]["relay_id"]="test-relay";e["payload"]["revision"]=revision;auto subs=e["payload"]["subscriptions"].to<JsonArray>();subs.add(cfg["relay"]["subscriptions"][0]);subs.add(cfg["relay"]["subscriptions"][0]);subs[1]["id"]="second";if(invalid)subs[1]["url"]="https://evil.example/notify";std::string body;serializeJson(doc,body);n->relayRetryAt=0;n->relayHealthAt=ticks+180000;for(auto& at:n->relayPollAt)at=ticks+30000;n->relayPollAt[1]=0;assert(n->relayStep()==1&&n->action==Action::RelayPoll&&n->relayIndex==1);reply(*n,body.c_str());saved(*n);assert(n->relayStep()==1&&n->action==Action::RelayAck);reply(*n,R"({"protocol_version":"relay/1","status":"completed","result":{"acknowledged":true}})");saved(*n);};
 update(1,false);assert(n->journal.state["relay"]["subscription_revision"]==1&&n->journal.state["relay"]["subscriptions"].size()==2);
 update(2,true);assert(n->journal.state["relay"]["subscription_revision"]==1&&std::string(n->relayError)=="relay_subscription_update_rejected");
 n->journal.state["relay"]["attachments"][0]["cursor"]=0;receive(*n,event);assert(n->journal.state["relay"]["pending"]["targets"].size()==2);
 assert(n->relayStep()==1&&n->action==Action::RelayPost);reply(*n,"",204);saved(*n);n=reboot(*n,cfg);const auto delivered=posts;assert(n->relayStep()==1&&n->action==Action::RelayPost);reply(*n,"",204);assert(posts==delivered+1&&sent.find("\"subscription_id\":\"second\"")!=std::string::npos);saved(*n);assert(n->relayStep()==1&&n->action==Action::RelayAck);reply(*n,R"({"protocol_version":"relay/1","status":"completed","result":{"acknowledged":true}})");saved(*n);
 n->phase=Phase::Stopped;n->dispose();puts("Relay: policy scope, durable event before dispatch, reboot before/after delivery commit, idempotency headers, five bounded retries, dead-letter before ACK, health during retry, credential isolation PASS");
}
