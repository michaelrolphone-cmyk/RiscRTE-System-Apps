#include "relay_harness.h"
#include "Digest.h"
static unsigned executed=0;
static bool hwReady(void*){return true;}
static int execute(void*,const char*,JsonObjectConst,JsonObject out){++executed;out["tx_completed"]=true;return 0;}
int main(int argc,char** argv){
 assert(argc==2);JsonDocument cfg;std::ifstream file(argv[1]);assert(!deserializeJson(cfg,file));assert(policy(cfg));
 JsonDocument in;in["channel"]=1;in["on"]=true;assert(hardwareInput("relay.bank","set",in.as<JsonObjectConst>()));in["channel"]=7;assert(!hardwareInput("relay.bank","set",in.as<JsonObjectConst>()));in["channel"]=1;in["pin"]=45;assert(!hardwareInput("relay.bank","set",in.as<JsonObjectConst>()));
 in.clear();in["baud"]=9600;in["parity"]="none";in["stop_bits"]=1;in["tx_hex"]="0103";in["rx_max"]=32;in["timeout_ms"]=100;assert(hardwareInput("serial.rs485","exchange",in.as<JsonObjectConst>()));in["tx_hex"]="01z3";assert(!hardwareInput("serial.rs485","exchange",in.as<JsonObjectConst>()));in["tx_hex"]="0103";
 // The deployed embedded protocol admits at most four offers per request.
 // All batches refresh the nine pins, and physical offers ignore role
 // staffing while the infrastructure offers remain lease gated.
 {Node inv;init(inv,cfg);inv.hardware={nullptr,hwReady,execute};inv.poolsEstablished=true;inv.memory.internal_free=200000;inv.memory.internal_largest=100000;inv.memory.dma_free=100000;inv.memory.dma_largest=100000;inv.memory.stack_headroom=16000;
 for(unsigned batch=0;batch<3;++batch){inv.inventoryBatch=batch;assert(inv.inventoryStep()==1);auto offers=inv.request["arguments"]["offers"];assert(offers.size()==(batch==2?1:4));for(auto offer:offers.as<JsonArrayConst>())assert(offer["offer"]["observed_health"]==(batch?"healthy":"capacity_blocked"));}inv.phase=Phase::Stopped;inv.dispose();}
 // A real sync can return a work offer while TLS temporarily owns the heap.
 // The adapter refreshes memory between advance() calls, after the socket closes.
 for (bool recovered : {false, true}) {
  Node worker; init(worker,cfg); worker.hardware={nullptr,hwReady,execute};
  worker.phase=Phase::Receive; worker.action=Action::Sync; worker.sessionGeneration=3;
  worker.net.started=ticks; worker.net.timeoutMs=30000; worker.net.token=1;
  const char* body=R"({"protocol_version":"1.0","status":"completed","code":"ok","data":{"generation":3,"controls":[],"work_offers":[{"assignment_id":"bench-led"}],"coverage":{"complete":true}}})";
  strcpy(worker.net.body,body); worker.net.size=strlen(body); worker.net.limit=32768;
  worker.net.api.advance=[](void*,uint64_t)->int32_t{return RISC_HTTP_EOF;};
  worker.net.api.read=[](void*,uint64_t,void*,uint32_t,uint32_t* n)->int32_t{*n=0;return RISC_HTTP_EOF;};
  worker.net.api.info=[](void*,uint64_t,risc_http_response_v2* info)->int32_t{info->status_code=200;return 0;};
  worker.net.api.close=[](void*,uint64_t)->int32_t{return 0;};
  worker.memory.internal_free=42924;worker.memory.internal_largest=26624;
  worker.memory.dma_free=16384;worker.memory.dma_largest=8192;worker.memory.stack_headroom=11304;
  assert(worker.advance()==1&&worker.phase==Phase::Receive&&worker.net.token==0);
  if(recovered)worker.memory.internal_free=83632;
  assert(worker.advance()==1);
  assert(recovered?(worker.phase==Phase::Send&&worker.action==Action::Assignment&&worker.request["arguments"]["assignment_id"]=="bench-led"):worker.phase==Phase::Ready);
  worker.phase=Phase::Stopped;worker.dispose();
 }
 // Frame validation supports arbitrary channel sets, partial updates, tone-off
 // and waits. A conflicting later step rejects the entire command.
 JsonDocument program;assert(!deserializeJson(program,R"({"steps":[{"on":[1,4],"off":[6],"rgb":[2,19,185],"tone_hz":734,"wait_ms":137},{"off":[1],"tone_hz":0},{"wait_ms":1234}]})"));
 OutputFrame decoded[OutputFrameLimit];unsigned duration=0;
 assert(decodeOutputBatch(program.as<JsonObjectConst>(),decoded,duration)&&duration==1371);
 assert(decoded[0].on==9&&decoded[0].off==32&&decoded[0].rgb[2]==185&&decoded[0].tone==734);
 assert(decoded[1].off==1&&!decoded[1].hasRgb&&decoded[1].tone==0);
 assert(!decoded[2].on&&decoded[2].tone<0);
 for (const char* invalid:{R"({"steps":[]})",R"({"steps":[{}]})",R"({"steps":[{"on":[7]}]})",
      R"({"steps":[{"on":[1,1]}]})",R"({"steps":[{"off":[3],"on":[3]}]})",
      R"({"steps":[{"tone_hz":1}]})",R"({"steps":[{"rgb":[1,2]}]})",
      R"({"steps":[{"wait_ms":60000},{"wait_ms":1}]})"}) {
  JsonDocument bad;assert(!deserializeJson(bad,invalid));assert(!hardwareInput("device.outputs.batch","apply",bad.as<JsonObjectConst>()));
 }
 {Node worker;init(worker,cfg);unsigned starts=0;
  worker.hardware={&starts,hwReady,[](void* c,const char*,JsonObjectConst,JsonObject)->int{++*static_cast<unsigned*>(c);return 1;},
   [](void*,JsonObject out)->int{out["steps_completed"]=3;return 0;},nullptr};
  worker.rebootRecovery=false;worker.leaseUntil=ticks+8500;
  auto j=worker.journal.state["job"].to<JsonObject>();j["phase"]="hardware_armed";j["boot_id"]=worker.boot;j["spec"]["capability"]["capability_id"]="device.outputs.batch";
  j["spec"]["operation"]="apply";j["spec"]["inputs"]=program;j["grant"]["allowed_effects"]="nonrepeatable_or_unobservable";
  assert(worker.hardwareStep()==1&&worker.action==Action::Renew&&starts==0);
  worker.leaseUntil=ticks+120000;assert(worker.hardwareStep()==1&&starts==1&&j["phase"]=="hardware_wait");saved(worker);
  assert(worker.hardwareStep()==1&&starts==1&&j["phase"]=="hardware_done"&&j["hardware_output"]["steps_completed"]==3);
  worker.phase=Phase::Stopped;worker.dispose();
 }
 // A failed hardware result waits for a matching rejection, even across a reboot.
 for(bool matches:{false,true}){
  Node worker;init(worker,cfg);worker.journal.state["credentials"][cfg["service"]["credential_ref"]|"participant"]="test-participant";auto j=worker.journal.state["job"].to<JsonObject>();
  j["phase"]="awaiting_acceptance";j["assignment_id"]="bench";j["attempt_id"]="attempt-b";j["candidate_id"]="result-b";j["hardware_output"]["status"]="unknown";
  worker.journal.state["pending"].add<JsonObject>()["action"]=unsigned(Action::Result);
  worker.make(Action::Review,"assignment","get");worker.phase=Phase::Send;
  JsonDocument answer;answer["status"]="completed";answer["protocol_version"]="1.0";auto a=answer["data"].to<JsonObject>();
  a["state"]="waiting";a["reason_code"]="review_rejected";a["id"]="bench";a["attempt_id"]="attempt-b";a["result_candidate_id"]=matches?"result-b":"different-result";
  std::string payload;serializeJson(answer,payload);reply(worker,payload.c_str());assert(worker.journal.state["job"].isNull()==matches);
  assert(worker.journal.state["pending"].size()==(matches?0:1));worker.phase=Phase::Stopped;worker.dispose();
 }
 Node n;init(n,cfg);n.hardware={nullptr,hwReady,execute};n.journal.state["credentials"]["participant"]="test-hardware-token";n.rebootRecovery=false;n.leaseUntil=ticks+120000;assert(n.workReady()&&!n.roleReady());
 auto j=n.journal.state["job"].to<JsonObject>();j["phase"]="running";j["boot_id"]=n.boot;j["attempt_id"]="hardware-test";j["generation"]=1;j["spec"]["capability"]["capability_id"]="serial.rs485";j["spec"]["operation"]="exchange";j["spec"]["inputs"]=in;j["grant"]["allowed_effects"]="nonrepeatable_or_unobservable";j["grant"]["input_digest"]=std::string(64,'a');
 assert(n.jobStep()==1&&executed==0&&j["phase"]=="hardware_intent");saved(n);assert(n.jobStep()==1&&executed==0&&n.action==Action::HardwareIntent);saved(n);
 assert(n.request["arguments"]["effect"]["outcome"]=="intent");
 // Only the exact accepted remote effect receipt arms the native operation.
 JsonDocument receipt;receipt["protocol_version"]="1.0";receipt["status"]="completed";receipt["receipt"]["operation_id"]=n.request["operation_id"];char hash[65];semanticDigest(n.request,hash);receipt["receipt"]["request_hash"]=hash;receipt["data"]["id"]="effect-test";receipt["data"]["attempt_id"]="hardware-test";receipt["data"]["request_hash"]=std::string(64,'a');std::string body;serializeJson(receipt,body);reply(n,body.c_str());saved(n);assert(j["phase"]=="hardware_armed");
 assert(n.jobStep()==1&&executed==1&&j["phase"]=="hardware_done");saved(n);assert(n.jobStep()==1&&executed==1&&n.action==Action::HardwareOutcome);saved(n);
 // An armed intent recovered on another boot must never reexecute UART I/O.
 auto old=reboot(n,cfg);old->hardware={nullptr,hwReady,execute};old->rebootRecovery=true;old->journal.state["job"]["phase"]="hardware_armed";old->leaseUntil=ticks+120000;assert(old->jobStep()==1&&executed==1&&old->action==Action::Stop);
 old->phase=Phase::Stopped;old->dispose();
 cfg.remove("memory_profile");assert(!policy(cfg));
 puts("Hardware: strict inputs, independent role eligibility, durable intent, acknowledged arm, single execution, reboot fence and memory policy PASS");
}
