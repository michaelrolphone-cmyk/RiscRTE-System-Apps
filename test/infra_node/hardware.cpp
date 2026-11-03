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
 // Both halves refresh the same eight pins, and physical offers ignore role
 // staffing while the infrastructure offers remain lease gated.
 {Node inv;init(inv,cfg);inv.hardware={nullptr,hwReady,execute};inv.poolsEstablished=true;inv.memory.internal_free=200000;inv.memory.internal_largest=100000;inv.memory.dma_free=100000;inv.memory.dma_largest=100000;inv.memory.stack_headroom=16000;
 for(unsigned batch=0;batch<2;++batch){inv.inventoryBatch=batch;assert(inv.inventoryStep()==1);auto offers=inv.request["arguments"]["offers"];assert(offers.size()==4);for(auto offer:offers.as<JsonArrayConst>())assert(offer["offer"]["observed_health"]==(batch?"healthy":"capacity_blocked"));}inv.phase=Phase::Stopped;inv.dispose();}
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
