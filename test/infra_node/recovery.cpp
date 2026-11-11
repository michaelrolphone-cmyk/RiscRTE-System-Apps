#include "relay_harness.h"
static unsigned feeds=0,restarts=0;
static int feed(void*){++feeds;return 0;}
static void restart(void*){++restarts;}
static uint64_t openedAt=0;static uint32_t requestBudget=0;
static int delayedOpen(void* c,const risc_http_request_v2* r,uint64_t* token){openedAt=ticks;requestBudget=r->timeout_ms;return openHttp(c,r,token);}
static int delayedAdvance(void*,uint64_t){if(ticks-openedAt>=requestBudget)return RISC_HTTP_TIMEOUT;return ticks-openedAt>=12000?RISC_HTTP_EOF:RISC_HTTP_AGAIN;}
static int delayedRead(void*,uint64_t,void*,uint32_t,uint32_t* count){*count=0;return ticks-openedAt>=12000?RISC_HTTP_EOF:RISC_HTTP_AGAIN;}
static int delayedInfo(void*,uint64_t,risc_http_response_v2* info){info->sent_bytes=128;info->phase=RISC_HTTP_PHASE_RECEIVE;return 0;}
int main(int argc,char** argv){assert(argc==2);std::ifstream f(argv[1]);std::string bytes{std::istreambuf_iterator<char>(f),{}};JsonDocument cfg;assert(parse(cfg,bytes.data(),bytes.size()));auto n=std::make_unique<Node>();init(*n,cfg);
 n->recovery={1,sizeof(n->recovery),nullptr,feed,nullptr,restart};
 // Entropy isn't needed for non-secret retry jitter. Do not enter permanent
 // ProtocolBlocked when the platform transiently cannot supply random bytes.
 n->entropy.fill=[](void*,void*,uint32_t)->int32_t{return RISC_ENTROPY_UNAVAILABLE;};
 assert(n->backoff("wifi_reconnect_backoff")==1&&n->phase==Phase::Backoff);assert(n->waitUntil>=ticks+1000&&n->networkFailures==1);
 n->phase=Phase::Connect;n->station.connect=[](void*,const char*,const char*)->int32_t{return -1;};n->station.close=[](void*)->int32_t{return 0;};assert(n->advance()==1&&n->phase==Phase::Backoff);assert(!n->setupRecovery);
 // Nonprogressing asynchronous provider must eventually stop watchdog feeds.
 n->phase=Phase::Receive;n->supervise();unsigned before=feeds;ticks+=30001;n->supervise();assert(feeds==before);n->phase=Phase::Ready;n->supervise();assert(feeds==before+1);
 // A responsive blocked node must not become a 120-second watchdog reboot
 // loop. The real v1.6.0 trace stayed in ProtocolBlocked/NOT_FOUND until reset.
 for(auto p:{Phase::AuthBlocked,Phase::CapacityBlocked,Phase::StorageBlocked,Phase::ProtocolBlocked,Phase::Stopped}){
  n->phase=p;n->supervise();before=feeds;
  for(unsigned i=0;i<130;++i){ticks+=1000;n->advance();}
  assert(feeds==before+130&&n->phase==p&&!restarts);
 }
 // Reboot is a last resort, excluded during an outstanding atomic write.
 n->networkFailures=6;n->networkFailedSince=ticks;ticks+=120000;n->phase=Phase::Save;n->supervise();assert(!restarts);n->phase=Phase::Ready;n->supervise();assert(restarts==1);n->networkSucceeded();n->supervise();assert(restarts==1);
 // HTTP provider that returns AGAIN forever still reaches bounded close.
 auto& t=n->net;t.token=1;t.started=ticks;t.timeoutMs=5000;t.api.close=[](void*,uint64_t)->int32_t{return 0;};assert(t.advance(ticks+6000)==0&&t.outcome==RISC_HTTP_TIMEOUT&&!t.token);
 // Both service and relay transactions survive a 12-second response. The old
 // five-second policy aborted these valid slow-day requests before completion.
 n->config["service"]["credential_ref"]="relay-token";
 n->net.api.open=delayedOpen;n->net.api.advance=delayedAdvance;n->net.api.read=delayedRead;n->net.api.info=delayedInfo;
 for(bool relay:{false,true}){
  if(relay){auto rp=n->config["relay"].to<JsonObject>();rp["url"]="https://bus.example/v1/relay/call";rp["credential_ref"]="relay-token";n->action=Action::RelayHealth;n->request.clear();}
  else n->make(Action::Open,"participant","open_session");
  n->send(false);assert(n->transmit()==1&&n->phase==Phase::Receive);
  for(unsigned i=0;i<11;++i){ticks+=1000;assert(t.advance(ticks)==1&&t.token);}
  ticks+=1000;assert(t.advance(ticks)==0&&!t.token&&t.outcome==0);
 }
 // Diagnostics preserve the transport outcome and request identity without
 // including credentials, URLs or request bodies; bounded retries still count.
 n->action=Action::Open;n->net.outcome=RISC_HTTP_TIMEOUT;n->net.info.sent_bytes=128;
 const unsigned failures=n->networkFailures;assert(n->received()==1&&n->phase==Phase::Backoff);
 assert(n->networkFailures==failures+1&&strstr(n->status.reason,"http_timeout")&&strstr(n->status.reason,"action=2")&&strstr(n->status.reason,"sent=128"));
 assert(!strstr(n->status.reason,"secret")&&!strstr(n->status.reason,"https:"));
 n->net.outcome=RISC_HTTP_NETWORK;n->net.info.sent_bytes=0;assert(n->received()==1&&strstr(n->status.reason,"http_error"));
 n->net.api.open=openHttp;n->phase=Phase::Ready;n->networkSucceeded();
 // Production health carries bounded hardware diagnostics without secrets.
 auto rp=n->config["relay"].to<JsonObject>();rp["schema_version"]=1;rp["relay_id"]="node";rp["url"]="https://bus.example/v1/relay/call";rp["credential_ref"]="relay-token";rp["site_credential_ref"]="";
 auto state=n->journal.state["relay"].to<JsonObject>();state["schema_version"]=1;state["subscriptions"].to<JsonArray>();state["deliveries"].to<JsonObject>();state["pending"]=nullptr;state["attachments"].to<JsonArray>().add<JsonObject>()["id"]="attachment";state["attachments"][0]["channel"]="capabilities.events";state["attachments"][0]["cursor"]=0;
 n->relayReady=n->relayIdentityReady=true;n->recoveryHealth={sizeof(risc_recovery_snapshot_v1),6,4,100000,90000,50000,-42,1};n->relayHealthAt=0;
 // Missing published providers reject participant.inventory. Keep its pins
 // and custody, continue the independent relay, and retry with bounded cadence.
 n->config["configuration"]["bindings"].to<JsonArray>();
 n->snapshotNeeded=false;n->rebootRecovery=false;n->poolsEstablished=true;
 strcpy(n->sessionId,"test-session");n->nextSync=ticks+300000;
 n->lastCapacity=n->capacity();
 std::string custody,pins;serializeJson(n->journal.state,custody);serializeJson(n->config["service"]["providers"],pins);
 assert(n->inventoryStep()==1&&n->action==Action::Inventory);
 reply(*n,R"({"protocol_version":"1.0","status":"rejected","code":"NOT_FOUND","mutation_outcome":"not_applied","data":null})");
 assert(n->phase==Phase::Ready&&n->inventoryNeeded&&!n->bootReady());
 std::string after;serializeJson(n->journal.state,after);assert(after==custody);after.clear();serializeJson(n->config["service"]["providers"],after);assert(after==pins);
 const auto rejectedAt=ticks;
 assert(n->advance()==1&&n->action==Action::RelayHealth);
 n->phase=Phase::Ready;n->relayHealthAt=ticks+180000;n->relayPollAt[0]=ticks+180000;
 n->sync();assert(n->request["arguments"]["requested_work"]==0);n->phase=Phase::Ready;
 for(unsigned i=0;i<59;++i){ticks+=1000;assert(n->advance()==1&&n->phase==Phase::Ready);}
 ticks=rejectedAt+60000;assert(n->advance()==1&&n->action==Action::Inventory);
 reply(*n,R"({"protocol_version":"1.0","status":"completed","data":{"profile":"esp32s3-infrastructure/1"}})");
 assert(!n->inventoryNeeded&&n->phase==Phase::Ready);
 n->relayHealthAt=0;
 assert(n->relayStep()==1&&n->action==Action::RelayHealth);auto health=n->request["arguments"];assert(health["version"]==3&&health["payload"]["reset_reason"]=="task_watchdog"&&health["payload"]["boot_count"]==4&&health["payload"]["wifi_rssi"]==-42&&measureJson(health["payload"])<4096);
 n->phase=Phase::Ready;n->relayHealthAt=ticks+180000;n->config["relay"]["credential_ref"]="relay-token";
 // 18,000 unmatched retained events: bounded journal snapshots and allocator
 // usage plateau while each confirmed cursor remains reboot-recoverable.
 size_t plateau=0;for(unsigned i=1;i<=72;++i){
  n->relayPollAt[0]=0;assert(n->relayStep()==1&&n->action==Action::RelayPoll);assert(n->request["arguments"]["scan_limit"]==250);
  char body[256];snprintf(body,sizeof(body),"{\"protocol_version\":\"relay/1\",\"status\":\"completed\",\"result\":{\"events\":[],\"event_cursor\":%u}}",i*250);
  reply(*n,body);saved(*n);assert(n->relayStep()==1&&n->action==Action::RelayAck);reply(*n,R"({"protocol_version":"relay/1","status":"completed","result":{"acknowledged":true}})");saved(*n);
  assert(n->journal.state["relay"]["attachments"][0]["cursor"]==i*250&&n->journal.state["relay"]["pending"].isNull());assert(measureJson(n->journal.state)<4096);if(i==2)plateau=n->journal.allocator.peak;if(i>2)assert(n->journal.allocator.peak<=plateau+256);
 }
 n->phase=Phase::Stopped;n->dispose();puts("Recovery: entropy-unavailable retry, autonomous Wi-Fi backoff, stalled-phase watchdog, bounded HTTP, last-resort restart and storage exclusion PASS");
}
