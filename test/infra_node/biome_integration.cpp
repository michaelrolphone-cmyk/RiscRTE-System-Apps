#include "relay_harness.h"
#include "../../firmware/waveshare-relay6ch/main/HttpSchedule.h"
#include "../../firmware/waveshare-relay6ch/main/Upgrade.h"
static unsigned executions=0,cancellations=0;
static bool hardwareReady(void*){return true;}
static int hardwareExecute(void*,const char*,JsonObjectConst,JsonObject){++executions;return 0;}
static bool hardwareCancel(void*){++cancellations;return true;}
static void dispose(Node& node){node.phase=Phase::Stopped;node.dispose();}
int main(int argc,char** argv){
 assert(argc==3);JsonDocument cfg,oldPins;
 std::ifstream source(argv[1]);assert(!deserializeJson(cfg,source)&&policy(cfg));
 std::ifstream previous(argv[2]);assert(!deserializeJson(oldPins,previous));

 // The two clients must never share a token, and neither may monopolize a
 // continuously ready peer. Setup suppresses new native traffic entirely.
 {BiomeRelay::HttpSchedule schedule;
  assert(schedule.grantNative(false,true,false));
  schedule.openedNative();assert(!schedule.grantNative(false,true,false));
  assert(schedule.grantNative(false,false,false));
  schedule.openedNode();assert(schedule.grantNative(false,true,false));
  for(bool waiting:{false,true})for(bool setup:{false,true})
   assert(!schedule.grantNative(true,waiting,setup));
  assert(!schedule.grantNative(false,false,true));
  for(unsigned i=0;i<100;++i){
   assert(schedule.grantNative(false,true,false));schedule.openedNative();
   assert(!schedule.grantNative(false,true,false));schedule.openedNode();
  }
 }

 // Moving Board ownership hides all five physical offers in the old catalog,
 // even when legacy bindings try to enable them. The four role pins remain.
 {Node node;init(node,cfg);node.externalHardware=true;
  node.hardware={nullptr,hardwareReady,hardwareExecute,nullptr,hardwareCancel};
  node.poolsEstablished=true;node.memory={sizeof(node.memory),262144,131072,65536,32768,8000000,4000000,16000};
  assert(node.legacyProviderCount()==4&&!node.hardwareReady());
  assert(node.inventoryStep()==1);
  auto offers=node.request["arguments"]["offers"].as<JsonArrayConst>();assert(offers.size()==4);
  for(auto item:offers)for(auto pin:cfg["service"]["providers"].as<JsonArrayConst>())
   if(pin["provider_id"]==item["offer"]["provider_id"])assert(!hardwareCapability(pin["capability_id"]|""));
  for(const char* capability:{"relay.bank","device.outputs.batch","serial.rs485","sound.buzzer","light.rgb"}){
   JsonDocument plan;plan["capability"]["capability_id"]=capability;
   assert(!node.localPlan(plan.as<JsonObjectConst>()));
   node.journal.state["pending"].to<JsonArray>();node.journal.state["job"].clear();
   auto job=node.journal.state["job"].to<JsonObject>();
   job["phase"]="hardware_armed";job["attempt_id"]="old-hardware";job["generation"]=1;
   job["spec"]["capability"]["capability_id"]=capability;
   assert(node.jobStep()==1&&node.action==Action::Stop);
   assert(node.request["arguments"]["evidence"]["reason"]=="physical_provider_moved_to_biomerte");
   assert(executions==0&&cancellations==0);
  }
  dispose(node);
 }
 // Opt-in is product-local: existing products retain all original physical
 // providers and direct hardware eligibility when externalHardware is false.
 {Node node;init(node,cfg);node.hardware={nullptr,hardwareReady,hardwareExecute};
  assert(node.legacyProviderCount()==9&&node.hardwareReady());dispose(node);
 }

 // A clean native installation can maintain its Wi-Fi without fake legacy
 // participant credentials. Any legacy identity or pending custody disables
 // this shortcut; it cannot be used to evade legacy reconciliation.
 {Node node;init(node,cfg);node.allowBiomeSetup=true;node.externalHardware=true;
  node.journal.state["participant_id"]="";
  node.journal.state["credentials"].remove(cfg["service"]["credential_ref"]|"");
  node.journal.state["biomerte"]["url"]="https://controller.example/api/provider/call";
  assert(node.networkOnly());
  node.loaded=true;node.phase=Phase::Recover;node.request.clear();node.action=Action::None;
  bool online=true;unsigned reconnects=0;
  struct Station {bool* online;unsigned* reconnects;} station{&online,&reconnects};
  node.station.context=&station;
  node.station.poll=[](void* context)->int32_t{return *static_cast<Station*>(context)->online?0:1;};
  node.station.connect=[](void* context,const char*,const char*)->int32_t{auto& s=*static_cast<Station*>(context);++*s.reconnects;*s.online=true;return 0;};
  for(unsigned i=0;i<30;++i){assert(node.advance()==1);++ticks;}
  assert(node.phase==Phase::Ready&&node.request.isNull()&&node.action==Action::None&&!node.rebootRecovery);
  online=false;assert(node.advance()==1&&node.phase==Phase::Connect);
  assert(node.advance()==1&&reconnects==1);
  assert(node.advance()==1&&node.phase==Phase::Ready&&node.request.isNull());
  node.journal.state["participant_id"]="existing";assert(!node.networkOnly());
  node.journal.state["participant_id"]="";
  node.journal.state["credentials"][cfg["service"]["credential_ref"]|""]="existing-secret";assert(!node.networkOnly());
  node.journal.state["credentials"].remove(cfg["service"]["credential_ref"]|"");
  node.journal.state["bootstrap"]="old-bootstrap";assert(!node.networkOnly());node.journal.state.remove("bootstrap");
  node.journal.state["pending"].add<JsonObject>()["action"]=1;assert(!node.networkOnly());node.journal.state["pending"].clear();
  node.journal.state["job"]["attempt_id"]="unresolved";assert(!node.networkOnly());node.journal.state["job"]=nullptr;
  node.allowBiomeSetup=false;assert(!node.networkOnly());dispose(node);
 }

 // Upgrade exactly the shipped v1.0.4 cohort, never a mixed or locally changed
 // cohort. Target endpoints, credential references and delivery custody survive.
 {assert(RelayBoard::matchesDigest(oldPins["providers"],"c4d47f4c46b4f39c309f7dbb1cc0a6460967ed8d1ceb5b4f7d019328af4ee9ba"));
  assert(RelayBoard::matchesDigest(oldPins["contracts"],"2e3fe7875d33e2ba17dfd56bec022a04f3721e8f1af24eba04148bfa676328f9"));
  JsonDocument factory;factory.set(cfg);
  for(auto pin:factory["service"]["providers"].as<JsonArray>())pin["revision"]=999;
  for(bool mixed:{false,true}){
   JsonDocument policy,relay;policy.set(cfg);policy["service"]["providers"]=oldPins["providers"];policy["service"]["contracts"]=oldPins["contracts"];
   policy["service"]["url"]="https://kept.example/channel";policy["service"]["credential_ref"]="keep-secret-ref";
   if(mixed)policy["service"]["providers"][0]["revision"]=998;
   relay["pending"]["operation_id"]="must-remain";
   RelayBoard::upgradeSetup(policy.as<JsonObject>(),factory.as<JsonObjectConst>(),relay.as<JsonObject>());
   assert(policy["service"]["url"]=="https://kept.example/channel"&&policy["service"]["credential_ref"]=="keep-secret-ref");
   assert(relay["pending"]["operation_id"]=="must-remain");
   assert(policy["service"]["providers"][0]["revision"]==(mixed?998:999));
  }
 }
 puts("Biome integration: exclusive HTTP ownership, fair turns, physical provider isolation, native-only reconnect, custody guards and exact v1.0.4 migration PASS");
}
